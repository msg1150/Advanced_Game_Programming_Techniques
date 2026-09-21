// ============================================================================
// HeightMapTerrainGenerator.h
// ----------------------------------------------------------------------------
// HeightMap 이미지를 기반으로 Terrain용 CPU Mesh 데이터를 생성한다.
//
// 이 클래스는 DirectX Device / Shader / Buffer를 직접 다루지 않는다.
// 오직 다음 역할만 담당한다.
//
// 1. HeightMapImage의 픽셀 밝기 읽기
// 2. Pixel -> Height 변환
// 3. Grid Vertex / Index 생성
// 4. UV 생성
// 5. Smooth Vertex Normal 계산
// ============================================================================

#pragma once

#include "Features/HeightMapTerrain/HeightMapImage.h"
#include "Graphics/Mesh.h"

#include <string>
#include <vector>

struct HeightMapTerrainSettings
{
    // 인접 Vertex 사이의 World 간격.
    float CellSize = 0.05f;

    // HeightMap에서 가장 어두운 값(0.0)이 대응할 World 높이.
    float MinHeight = -1.0f;

    // HeightMap에서 가장 밝은 값(1.0)이 대응할 World 높이.
    float MaxHeight = 3.0f;
};

struct HeightMapTerrainMeshData
{
    std::vector<Vertex> Vertices;
    std::vector<std::uint32_t> Indices;
};

class HeightMapTerrainGenerator
{
public:
    static bool Generate(
        const HeightMapImage& heightMapImage,
        const HeightMapTerrainSettings& settings,
        HeightMapTerrainMeshData& outMeshData,
        std::wstring& outErrorMessage);

private:
    static void CalculateNormals(
        std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices);
};
