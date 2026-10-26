// ============================================================================
// TileArchive.cpp — 파일을 열기 전 Tile의 유효 범위를 검증한다.
// 작은 Binary 헤더는 필드별로 읽어 구조체 Padding / ABI 문제를 방지한다.
// ============================================================================
#include "Features/DiskTerrainStreaming/TileArchive.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
namespace
{
    template<class T> bool Read(std::ifstream& stream, T& value)
    {
        return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(T)));
    }
    bool Magic(std::ifstream& stream, const char expected[4])
    {
        char value[4] = {};
        return static_cast<bool>(stream.read(value, 4)) &&
            value[0]==expected[0] && value[1]==expected[1] &&
            value[2]==expected[2] && value[3]==expected[3];
    }
}

bool TileArchive::ReadMetadata(const std::filesystem::path& path,
                               TileWorldInfo& output, std::wstring& error)
{
    error.clear(); output = {};
    std::ifstream in(path, std::ios::binary);
    if(!in) { error = L"Tile metadata 열기 실패: " + path.wstring(); return false; }
    std::uint32_t version = 0;
    if(!Magic(in,"DTM1") || !Read(in, version) || version!=1 ||
       !Read(in,output.TotalCellsX) || !Read(in,output.TotalCellsZ) ||
       !Read(in,output.CellsPerTile) || !Read(in,output.TilesX) ||
       !Read(in,output.TilesZ) || !Read(in,output.CellSize) ||
       !Read(in,output.OriginX) || !Read(in,output.OriginZ))
    { error=L"Tile metadata 헤더 오류 또는 지원하지 않는 버전"; return false; }
    const std::uint64_t count=static_cast<std::uint64_t>(output.TilesX)*output.TilesZ;
    if(!output.CellsPerTile || output.CellsPerTile>256u ||
       !output.TilesX || !output.TilesZ || count>4096u ||
       !output.TotalCellsX || !output.TotalCellsZ ||
       output.TilesX!=(output.TotalCellsX+output.CellsPerTile-1u)/output.CellsPerTile ||
       output.TilesZ!=(output.TotalCellsZ+output.CellsPerTile-1u)/output.CellsPerTile ||
       !(output.CellSize>0.0f) || !std::isfinite(output.CellSize) ||
       !std::isfinite(output.OriginX) || !std::isfinite(output.OriginZ))
    { error=L"Tile metadata 범위/크기 검사 실패";return false; }
    output.Tiles.resize(static_cast<std::size_t>(count));
    for(auto& tile:output.Tiles)
    {
        if(!Read(in,tile.MinimumHeight) || !Read(in,tile.MaximumHeight) ||
           !std::isfinite(tile.MinimumHeight) || !std::isfinite(tile.MaximumHeight) ||
           tile.MinimumHeight>tile.MaximumHeight)
        { error=L"Tile metadata 높이 Bounds 손상";output={};return false; }
    }
    if(in.peek()!=std::char_traits<char>::eof())
    { error=L"Tile metadata 크기 불일치";output={};return false; }
    return true;
}

std::filesystem::path TileArchive::TileFile(const std::filesystem::path& directory,
                                             std::uint32_t x, std::uint32_t z)
{
    std::wostringstream name;
    name << L"Tile_" << std::setfill(L'0') << std::setw(2) << x
         << L"_" << std::setw(2) << z << L".tile";
    return directory / name.str();
}

bool TileArchive::ReadTile(const std::filesystem::path& root,
                           const TileWorldInfo& meta, std::uint32_t x, std::uint32_t z,
                           TileSamples& output, std::wstring& error)
{
    output={};error.clear();
    if(x>=meta.TilesX || z>=meta.TilesZ || meta.CellsPerTile==0u)
    { error=L"Tile Index 범위 초과";return false; }
    const auto path=TileFile(root,x,z);
    std::ifstream in(path,std::ios::binary);
    if(!in) { error=L"Tile 파일 열기 실패: "+path.wstring();return false; }
    std::uint32_t version=0;
    if(!Magic(in,"DTL1") || !Read(in,version) || version!=1 ||
       !Read(in,output.TileX) || !Read(in,output.TileZ) ||
       !Read(in,output.CellsX) || !Read(in,output.CellsZ))
    { error=L"Tile 파일 헤더 오류: "+path.wstring();return false; }
    const std::uint32_t startX=x*meta.CellsPerTile;
    const std::uint32_t startZ=z*meta.CellsPerTile;
    if(output.TileX!=x || output.TileZ!=z ||
       output.CellsX!=std::min(meta.CellsPerTile,meta.TotalCellsX-startX) ||
       output.CellsZ!=std::min(meta.CellsPerTile,meta.TotalCellsZ-startZ))
    { error=L"Tile 헤더 좌표/셀 개수 불일치: "+path.wstring();return false; }
    const auto width=static_cast<std::size_t>(output.CellsX)+3u;
    const auto height=static_cast<std::size_t>(output.CellsZ)+3u;
    output.Heights.resize(width*height);
    if(!in.read(reinterpret_cast<char*>(output.Heights.data()),
                static_cast<std::streamsize>(output.Heights.size()*sizeof(float))) ||
       in.peek()!=std::char_traits<char>::eof())
    { error=L"Tile Sample 데이터 길이 오류: "+path.wstring();output={};return false; }
    for(const float heightSample:output.Heights)
    {
        if(!std::isfinite(heightSample))
        { error=L"Tile 높이에 NaN/Inf가 포함됨: "+path.wstring();output={};return false; }
    }
    return true;
}
