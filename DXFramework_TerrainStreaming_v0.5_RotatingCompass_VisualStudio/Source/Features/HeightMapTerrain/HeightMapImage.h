// ============================================================================
// HeightMapImage.h
// ----------------------------------------------------------------------------
// HeightMap Terrain Feature에서 사용할 HeightMap 이미지를 로드하는 클래스.
//
// 이 클래스의 역할은 오직 "이미지 파일 -> 정규화된 높이 샘플 배열" 변환이다.
// Terrain Vertex 생성, DirectX Buffer 생성, 렌더링은 담당하지 않는다.
//
// 구현에는 Windows WIC(Windows Imaging Component)를 사용한다.
// 따라서 외부 이미지 라이브러리를 추가하지 않고도
// PNG / BMP / JPG 같은 일반적인 이미지를 읽을 수 있다.
//
// 현재 Showcase에서는 HeightMap으로 PNG 사용을 권장한다.
// JPEG는 압축 특성상 픽셀 값이 살짝 변할 수 있어 HeightMap 용도로는 덜 적합하다.
// ============================================================================

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class HeightMapImage
{
public:
    // HeightMap 이미지를 로드한다.
    //
    // 성공 시:
    // - true 반환
    // - Width / Height / HeightValues가 채워짐
    //
    // 실패 시:
    // - false 반환
    // - GetLastErrorMessage()로 실패 이유 확인 가능
    bool Load(const std::filesystem::path& imagePath);

    std::uint32_t GetWidth() const;
    std::uint32_t GetHeight() const;

    // 0.0 ~ 1.0 범위로 정규화된 높이값 배열을 반환한다.
    const std::vector<float>& GetHeightValues() const;

    // 특정 픽셀 위치의 정규화된 높이값을 반환한다.
    // x, y는 반드시 이미지 범위 안이라고 가정한다.
    float GetHeightValue(
        std::uint32_t x,
        std::uint32_t y) const;

    const std::wstring& GetLastErrorMessage() const;

private:
    std::uint32_t width_ = 0u;
    std::uint32_t height_ = 0u;

    // 각 픽셀의 밝기를 0.0 ~ 1.0 범위로 저장한다.
    std::vector<float> heightValues_;

    std::wstring lastErrorMessage_;
};
