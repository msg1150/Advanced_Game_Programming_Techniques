// ============================================================================
// PerlinTerrainGenerator.h
// ----------------------------------------------------------------------------
// Perlin Noise를 사용하여 Terrain용 CPU Mesh 데이터를 생성한다.
//
// 이 클래스는 GPU Buffer를 생성하거나 Shader를 다루지 않는다.
// 오직 다음 작업만 담당한다.
//
// 1. Grid Vertex 생성
// 2. Perlin Noise를 이용한 Height 계산
// 3. Triangle Index 생성
// 4. Vertex Normal 계산
// 5. UV 생성
//
// Terrain 생성과 렌더링을 분리해 두었기 때문에
// 이후 FBM, HeightMap 등 다른 생성 방식으로 교체하기 쉽다.
// ============================================================================

#pragma once

#include "Graphics/Mesh.h"

#include <cstdint>
#include <string>
#include <vector>

// Perlin Terrain을 생성하기 위한 최소 설정값.
//
// 현재 단계에서는 단일 Perlin Noise만 사용한다.
// Octave / Persistence / Lacunarity 등 FBM 관련 값은
// 기본 Perlin 동작을 검증한 뒤 별도 단계에서 추가한다.
struct PerlinTerrainSettings
{
    // X축 방향 Cell 개수.
    std::uint32_t CellsX = 24u;

    // Z축 방향 Cell 개수.
    std::uint32_t CellsZ = 24u;

    // 인접 Vertex 사이의 World 간격.
    float CellSize = 0.35f;

    // World 좌표를 Noise 좌표로 변환할 때 곱하는 값.
    //
    // 값이 작을수록 넓고 완만한 지형,
    // 값이 클수록 짧은 간격으로 굴곡이 생긴다.
    float NoiseFrequency = 0.18f;

    // Perlin Noise 결과를 최종 Y 높이로 변환하는 배율.
    float HeightScale = 1.8f;

    // 동일 Seed는 항상 동일한 Terrain을 만든다.
    std::uint32_t Seed = 1337u;
};

// GPU에 올리기 전의 CPU Mesh Data.
//
// 기존 Framework의 Mesh::Initialize()에 바로 전달할 수 있도록
// 공용 Vertex 구조를 그대로 사용한다.
struct PerlinTerrainMeshData
{
    std::vector<Vertex> Vertices;
    std::vector<std::uint32_t> Indices;
};

class PerlinTerrainGenerator
{
public:
    // Settings를 기반으로 Terrain Mesh Data를 생성한다.
    //
    // 성공:
    // - true 반환
    // - outMeshData에 Vertex / Index 저장
    //
    // 실패:
    // - false 반환
    // - outErrorMessage에 이유 저장
    static bool Generate(
        const PerlinTerrainSettings& settings,
        PerlinTerrainMeshData& outMeshData,
        std::wstring& outErrorMessage);

private:
    // 모든 Triangle의 Face Normal을 각 Vertex에 누적한 뒤
    // 마지막에 Normalize하여 Smooth Vertex Normal을 만든다.
    static void CalculateNormals(
        std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices);
};
