// ============================================================================
// QuadTreeLODGeomorphWorker.cpp
// ----------------------------------------------------------------------------
// v0.10에서 Main Thread의 DiskTerrainManager::Install()이 수행하던
// Geomorph Mesh 전체 생성 작업을 별도 CPU Thread로 이동한다.
//
// 이 Worker가 하는 일:
// 1) 기존 TilePrepared의 QuadTree / MeshData를 읽는다.
// 2) 원본 Grid Vertex 구간만 추출한다.
// 3) Parent 표면 목표 높이/Normal 및 Geomorph용 Node Mesh를 만든다.
// 4) 완성 결과를 Main Thread로 전달한다.
//
// GPU Buffer 생성은 기존 원칙대로 Main Thread에서만 수행한다.
// ============================================================================
#include "Features/QuadTreeLODGeomorph/QuadTreeLODGeomorphWorker.h"

#include <algorithm>
#include <utility>

QuadTreeLODGeomorphWorker::~QuadTreeLODGeomorphWorker()
{
    Stop();
}

bool QuadTreeLODGeomorphWorker::Start(
    TileWorldInfo world,
    QuadTreeLODSettings lodSettings)
{
    Stop();

    if (world.Tiles.empty() ||
        world.TilesX == 0u ||
        world.TilesZ == 0u ||
        world.CellsPerTile == 0u)
    {
        return false;
    }

    world_ = std::move(world);
    lodSettings_ = lodSettings;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        latestGeneration_.assign(world_.Tiles.size(), 0u);
        stopping_ = false;
    }

    try
    {
        thread_ = std::thread(&QuadTreeLODGeomorphWorker::Main, this);
    }
    catch (...)
    {
        Stop();
        return false;
    }

    return true;
}

bool QuadTreeLODGeomorphWorker::Submit(TilePrepared& tile)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_ || tile.Index >= latestGeneration_.size())
        {
            return false;
        }

        latestGeneration_[tile.Index] = tile.Generation;
        jobs_.push_back({ std::move(tile) });
    }

    wake_.notify_one();
    return true;
}

void QuadTreeLODGeomorphWorker::Cancel(
    std::uint32_t index,
    std::uint64_t nextGeneration)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (index >= latestGeneration_.size())
    {
        return;
    }

    latestGeneration_[index] = nextGeneration;

    // 아직 시작하지 않은 같은 Tile 작업은 즉시 제거한다.
    // 실행 중인 작업은 Build 도중 메모리를 건드리지 않기 위해 강제 중단하지 않고,
    // 완료 직전 Generation 재검사에서 결과만 버린다.
    jobs_.erase(
        std::remove_if(
            jobs_.begin(),
            jobs_.end(),
            [index](const Job& job)
            {
                return job.Tile.Index == index;
            }),
        jobs_.end());

    // 이미 완료됐지만 Main Thread가 아직 가져가지 않은 오래된 결과도 제거한다.
    ready_.erase(
        std::remove_if(
            ready_.begin(),
            ready_.end(),
            [index](const QuadTreeLODGeomorphPreparedTile& item)
            {
                return item.Tile.Index == index;
            }),
        ready_.end());
}

bool QuadTreeLODGeomorphWorker::TryPop(
    QuadTreeLODGeomorphPreparedTile& result)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (ready_.empty())
    {
        return false;
    }

    result = std::move(ready_.front());
    ready_.pop_front();
    return true;
}

void QuadTreeLODGeomorphWorker::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        jobs_.clear();
    }

    wake_.notify_all();

    if (thread_.joinable())
    {
        thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.clear();
        ready_.clear();
        latestGeneration_.clear();
        stopping_ = false;
    }

    world_ = {};
}

void QuadTreeLODGeomorphWorker::Main()
{
    for (;;)
    {
        Job job;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(
                lock,
                [this]
                {
                    return stopping_ || !jobs_.empty();
                });

            if (stopping_)
            {
                return;
            }

            job = std::move(jobs_.front());
            jobs_.pop_front();

            if (job.Tile.Index >= latestGeneration_.size() ||
                latestGeneration_[job.Tile.Index] != job.Tile.Generation)
            {
                continue;
            }
        }

        QuadTreeLODGeomorphPreparedTile result;
        result.Tile = std::move(job.Tile);

        // 기본 Tile 생성 자체가 실패한 경우에는 Geomorph 작업을 하지 않는다.
        // 정상 경로에서는 Manager가 Error 결과를 이 Worker에 보내지 않지만,
        // 독립 Feature의 방어 로직으로 한 번 더 확인한다.
        if (result.Tile.Error.empty() && result.Tile.Tree.GetRoot())
        {
            const std::uint32_t tileX = result.Tile.Index % world_.TilesX;
            const std::uint32_t tileZ = result.Tile.Index / world_.TilesX;
            const std::uint32_t startX = tileX * world_.CellsPerTile;
            const std::uint32_t startZ = tileZ * world_.CellsPerTile;

            const std::uint32_t cellsX =
                std::min(world_.CellsPerTile, world_.TotalCellsX - startX);
            const std::uint32_t cellsZ =
                std::min(world_.CellsPerTile, world_.TotalCellsZ - startZ);

            const std::uint32_t vertexWidth = cellsX + 1u;
            const std::uint32_t vertexHeight = cellsZ + 1u;
            const std::size_t sourceVertexCount =
                static_cast<std::size_t>(vertexWidth) * vertexHeight;

            if (sourceVertexCount <= result.Tile.MeshData.Vertices.size())
            {
                // QuadTreeLOD::Build는 원본 Grid Vertex를 MeshData 앞쪽에 그대로
                // 보관하고 뒤에 Skirt Vertex만 추가한다. 따라서 기존 Builder를
                // 수정하지 않고 원본 Grid 구간만 복사해 Geomorph 입력으로 사용한다.
                // 이 복사와 무거운 Geomorph Build 모두 현재 Background Thread에서 실행된다.
                std::vector<Vertex> sourceVertices(
                    result.Tile.MeshData.Vertices.begin(),
                    result.Tile.MeshData.Vertices.begin() + sourceVertexCount);

                result.GeomorphAvailable =
                    QuadTreeLODGeomorphBuilder::Build(
                        sourceVertices,
                        vertexWidth,
                        vertexHeight,
                        result.Tile.Tree,
                        result.Tile.MeshData,
                        lodSettings_,
                        result.GeomorphMesh,
                        result.GeomorphWarning);
            }
            else
            {
                result.GeomorphWarning =
                    L"Geomorph 원본 Grid Vertex 범위가 올바르지 않습니다.";
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_)
            {
                return;
            }

            // CPU Build가 진행되는 동안 Camera가 이동해 Generation이 바뀌었다면
            // 오래된 결과를 Main Thread로 보내지 않는다.
            if (result.Tile.Index >= latestGeneration_.size() ||
                latestGeneration_[result.Tile.Index] != result.Tile.Generation)
            {
                continue;
            }

            ready_.push_back(std::move(result));
        }
    }
}
