// ============================================================================
// Texture2D.h
// ----------------------------------------------------------------------------
// Direct3D 11에서 사용할 2D Texture를 로드하고 Shader Resource View를
// 관리하는 공용 Graphics 클래스.
//
// 이 클래스는 Terrain 전용이 아니다.
// Character, Weapon, UI, Material 등 다른 기능에서도 그대로 사용할 수 있다.
//
// 이미지 로딩:
// - Windows WIC(Windows Imaging Component) 사용
// - 외부 이미지 라이브러리 의존성 없음
//
// Color Space:
// - SRGB  : Grass / Rock / Snow 같은 색상 Texture
// - Linear: Splat Map, Mask 등 수치 데이터 Texture
//
// Splat Map을 SRGB로 읽으면 RGB Weight가 Gamma 보정되어 잘못된 혼합 비율이
// 나오므로 Linear 모드를 명시적으로 지원한다.
// ============================================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

enum class TextureColorSpace
{
    // 색상 표현용 Texture.
    // Shader Sampling 시 SRGB -> Linear 변환이 적용된다.
    SRGB,

    // Mask / Splat Map처럼 픽셀 값 자체가 데이터인 Texture.
    Linear
};

class Texture2D
{
public:
    // 이미지 파일을 GPU Texture로 로드한다.
    //
    // context가 필요한 이유:
    // - 0번 Mip에 원본 이미지를 업로드
    // - GenerateMips()로 하위 Mip 생성
    bool Load(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& imagePath,
        TextureColorSpace colorSpace = TextureColorSpace::SRGB);

    // Pixel Shader의 지정된 Texture Slot(t#)에 SRV를 바인딩한다.
    void BindPS(
        ID3D11DeviceContext* context,
        UINT slot) const;

    // 지정된 Pixel Shader Texture Slot을 비운다.
    static void UnbindPS(
        ID3D11DeviceContext* context,
        UINT slot);

    bool IsLoaded() const;

    const std::wstring& GetLastErrorMessage() const;

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView_;

    bool loaded_ = false;
    std::wstring lastErrorMessage_;
};
