// ============================================================================
// TerrainStreamingPolicy.h
// GPU / Windows SDK와 분리된 순수 거리 판정. 단독 Unit Test가 가능하다.
// ============================================================================
#pragma once
namespace TerrainStreamingPolicy
{
    inline bool ShouldKeep(bool enabled, bool retained, float distance,
                           float loadRadius, float unloadRadius)
    {
        // Streaming OFF: Culling/LOD와 관계없이 모두 GPU에 유지한다.
        if (!enabled) return true;
        // 이미 로드됐거나 요청된 Chunk는 더 먼 거리까지 유지한다.
        return distance <= (retained ? unloadRadius : loadRadius);
    }
}
