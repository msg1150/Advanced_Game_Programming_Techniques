// ============================================================================
// HeightMapImage.cpp
// ============================================================================

#include "Features/HeightMapTerrain/HeightMapImage.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <sstream>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
    // HRESULT를 확인하기 쉬운 문자열로 변환한다.
    std::wstring HResultToString(HRESULT hr)
    {
        wchar_t buffer[32] = {};

        swprintf_s(
            buffer,
            L"0x%08X",
            static_cast<unsigned int>(hr));

        return buffer;
    }
}

bool HeightMapImage::Load(const std::filesystem::path& imagePath)
{
    width_ = 0u;
    height_ = 0u;
    heightValues_.clear();
    lastErrorMessage_.clear();

    // ------------------------------------------------------------------------
    // WIC는 COM 기반 API이므로 현재 Thread에서 COM을 사용할 수 있게 만든다.
    //
    // - S_OK / S_FALSE:
    //   현재 호출이 COM 초기화 참조 카운트를 하나 획득한 상태이므로
    //   함수 종료 전에 CoUninitialize()를 호출해야 한다.
    //
    // - RPC_E_CHANGED_MODE:
    //   이미 다른 Apartment 모델로 초기화된 상태다.
    //   이 경우에도 현재 Thread는 COM을 사용할 수 있으므로 계속 진행한다.
    //   다만 이번 호출이 참조 카운트를 획득한 것은 아니므로
    //   CoUninitialize()를 호출하면 안 된다.
    // ------------------------------------------------------------------------
    const HRESULT initializeResult =
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

    const bool shouldCoUninitialize =
        initializeResult == S_OK ||
        initializeResult == S_FALSE;

    if (FAILED(initializeResult) &&
        initializeResult != RPC_E_CHANGED_MODE)
    {
        lastErrorMessage_ =
            L"COM 초기화(CoInitializeEx)에 실패했습니다.\nHRESULT: " +
            HResultToString(initializeResult);

        return false;
    }

    bool success = false;

    do
    {
        if (!std::filesystem::exists(imagePath))
        {
            lastErrorMessage_ =
                L"HeightMap 이미지 파일을 찾을 수 없습니다.\nPath: " +
                imagePath.wstring();

            break;
        }

        // --------------------------------------------------------------------
        // 1. WIC Factory 생성
        // --------------------------------------------------------------------
        ComPtr<IWICImagingFactory> imagingFactory;

        HRESULT hr =
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(imagingFactory.GetAddressOf()));

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"IWICImagingFactory 생성에 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        // --------------------------------------------------------------------
        // 2. 이미지 Decoder 생성
        // --------------------------------------------------------------------
        ComPtr<IWICBitmapDecoder> decoder;

        hr =
            imagingFactory->CreateDecoderFromFilename(
                imagePath.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                decoder.GetAddressOf());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"HeightMap 이미지 Decoder 생성에 실패했습니다.\nPath: " +
                imagePath.wstring() +
                L"\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        // --------------------------------------------------------------------
        // 3. 첫 번째 Frame을 사용한다.
        //    일반적인 HeightMap은 단일 이미지이므로 0번 Frame이면 충분하다.
        // --------------------------------------------------------------------
        ComPtr<IWICBitmapFrameDecode> frame;

        hr =
            decoder->GetFrame(
                0,
                frame.GetAddressOf());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"HeightMap 이미지 Frame을 가져오는데 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        hr =
            frame->GetSize(
                &width_,
                &height_);

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"HeightMap 이미지 크기를 읽는데 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        if (width_ < 2u ||
            height_ < 2u)
        {
            lastErrorMessage_ =
                L"HeightMap 이미지는 최소 2x2 이상이어야 합니다.";

            break;
        }

        // --------------------------------------------------------------------
        // 4. 모든 이미지를 32bit RGBA로 통일해 이후 Pixel 처리 로직을 단순화한다.
        // --------------------------------------------------------------------
        ComPtr<IWICFormatConverter> formatConverter;

        hr =
            imagingFactory->CreateFormatConverter(
                formatConverter.GetAddressOf());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"WIC Format Converter 생성에 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        hr =
            formatConverter->Initialize(
                frame.Get(),
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom);

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"HeightMap 이미지를 32bpp RGBA로 변환하는데 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        const std::uint32_t bytesPerPixel = 4u;
        const std::uint32_t rowPitch =
            width_ * bytesPerPixel;

        std::vector<std::uint8_t> pixelBytes(
            static_cast<std::size_t>(rowPitch) *
            static_cast<std::size_t>(height_));

        hr =
            formatConverter->CopyPixels(
                nullptr,
                rowPitch,
                static_cast<UINT>(pixelBytes.size()),
                pixelBytes.data());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"HeightMap 픽셀 복사에 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        heightValues_.resize(
            static_cast<std::size_t>(width_) *
            static_cast<std::size_t>(height_));

        // --------------------------------------------------------------------
        // 5. RGBA 픽셀을 정규화된 Height 값으로 변환한다.
        //
        // 일반적인 HeightMap은 흑백 이미지지만,
        // 혹시 컬러 이미지가 들어와도 사람이 보는 밝기에 가깝게 처리하기 위해
        // Luminance 공식을 사용한다.
        // --------------------------------------------------------------------
        for (std::uint32_t y = 0u;
             y < height_;
             ++y)
        {
            for (std::uint32_t x = 0u;
                 x < width_;
                 ++x)
            {
                const std::size_t pixelIndex =
                    static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(rowPitch) +
                    static_cast<std::size_t>(x) *
                    bytesPerPixel;

                const float red =
                    static_cast<float>(pixelBytes[pixelIndex + 0u]);

                const float green =
                    static_cast<float>(pixelBytes[pixelIndex + 1u]);

                const float blue =
                    static_cast<float>(pixelBytes[pixelIndex + 2u]);

                const float luminance =
                    0.2126f * red +
                    0.7152f * green +
                    0.0722f * blue;

                const float normalizedHeight =
                    std::clamp(
                        luminance / 255.0f,
                        0.0f,
                        1.0f);

                heightValues_[
                    static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(width_) +
                    static_cast<std::size_t>(x)] =
                    normalizedHeight;
            }
        }

        success = true;
    }
    while (false);

    if (shouldCoUninitialize)
    {
        CoUninitialize();
    }

    return success;
}

std::uint32_t HeightMapImage::GetWidth() const
{
    return width_;
}

std::uint32_t HeightMapImage::GetHeight() const
{
    return height_;
}

const std::vector<float>& HeightMapImage::GetHeightValues() const
{
    return heightValues_;
}

float HeightMapImage::GetHeightValue(
    std::uint32_t x,
    std::uint32_t y) const
{
    return heightValues_[
        static_cast<std::size_t>(y) *
        static_cast<std::size_t>(width_) +
        static_cast<std::size_t>(x)];
}

const std::wstring& HeightMapImage::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}
