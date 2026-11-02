// ============================================================================
// QuadTreeLODGeomorphWorker.h
// ----------------------------------------------------------------------------
// Geomorph 전용 CPU 준비 작업을 별도 Thread에서 처리한다.
//
// 기존 DiskTileWorker는 파일 I/O + 기본 QuadTree LOD 생성 역할만 유지하고,
// Geomorph용 대형 Vertex/Index 재구성은 이 Worker가 이어서 처리한다.
// Direct3D Device / Context / GPU Resource에는 절대 접근하지 않는다.
//
// Feature 제거 시 이 파일과 Manager의 연결부만 제거하면 기존 DiskTileWorker와
// TileGeometryBuilder는 원래 구조 그대로 사용할 수 있다.
// ============================================================================
#pragma once

#include "Features/DiskTerrainStreaming/TileGeometryBuilder.h"
#include "Features/QuadTreeLODGeomorph/QuadTreeLODGeomorph.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct QuadTreeLODGeomorphPreparedTile
{
    // 기존 DiskTileWorker가 만든 결과를 그대로 보관한다.
    // Tree / 기본 Mesh / Border Mesh의 생성 규칙은 수정하지 않는다.
    TilePrepared Tile;

    // Geomorph Feature 전용 추가 데이터.
    QuadTreeLODGeomorphMeshData GeomorphMesh;
    bool GeomorphAvailable = false;

    // Geomorph 생성 실패는 Terrain 전체 실패로 취급하지 않는다.
    // Manager는 기존 Tile.MeshData로 자동 Fallback할 수 있다.
    std::wstring GeomorphWarning;
};

class QuadTreeLODGeomorphWorker
{
public:
    QuadTreeLODGeomorphWorker() = default;
    ~QuadTreeLODGeomorphWorker();

    QuadTreeLODGeomorphWorker(const QuadTreeLODGeomorphWorker&) = delete;
    QuadTreeLODGeomorphWorker& operator=(const QuadTreeLODGeomorphWorker&) = delete;

    // Metadata와 LOD 설정만 복사한다. GPU 객체는 전달하지 않는다.
    bool Start(TileWorldInfo world, QuadTreeLODSettings lodSettings);

    // DiskTileWorker가 완료한 기본 Tile을 이동시켜 비동기 Geomorph 작업을 요청한다.
    // 성공 시 tile은 Move된 상태가 된다.
    bool Submit(TilePrepared& tile);

    // Streaming 범위를 벗어난 Tile의 오래된 Generation을 무효화한다.
    // 이미 실행 중인 CPU 계산을 강제로 중단하지는 않지만 결과는 폐기한다.
    void Cancel(std::uint32_t index, std::uint64_t nextGeneration);

    // 메인 Thread에서는 완료된 CPU 데이터만 가져와 GPU Buffer를 생성한다.
    bool TryPop(QuadTreeLODGeomorphPreparedTile& result);

    void Stop();

private:
    struct Job
    {
        TilePrepared Tile;
    };

    void Main();

    TileWorldInfo world_;
    QuadTreeLODSettings lodSettings_ = {};

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    std::deque<QuadTreeLODGeomorphPreparedTile> ready_;
    std::vector<std::uint64_t> latestGeneration_;
    bool stopping_ = false;
};
