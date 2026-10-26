// ============================================================================
// Texture2D.cpp
// ============================================================================

#include "Graphics/Texture2D.h"

#include <Windows.h>
#include <wincodec.h>

#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
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

bool Texture2D::Load(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& imagePath,
    TextureColorSpace colorSpace)
{
    texture_.Reset();
    shaderResourceView_.Reset();

    loaded_ = false;
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"Texture2D::Load에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    if (!std::filesystem::exists(imagePath))
    {
        lastErrorMessage_ =
            L"Texture 파일을 찾을 수 없습니다.\nPath: " +
            imagePath.wstring();

        return false;
    }

    // ------------------------------------------------------------------------
    // WIC가 사용하는 COM 환경을 현재 Thread에 준비한다.
    //
    // 이미 다른 Apartment 모델로 COM이 초기화된 경우
    // RPC_E_CHANGED_MODE가 반환될 수 있지만 COM 자체는 사용 가능하므로
    // 해당 경우에는 계속 진행한다.
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
            L"COM 초기화에 실패했습니다.\nHRESULT: " +
            HResultToString(initializeResult);

        return false;
    }

    bool success = false;

    do
    {
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
        // 2. 이미지 Decoder / Frame 생성
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
                L"Texture Decoder 생성에 실패했습니다.\nPath: " +
                imagePath.wstring() +
                L"\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        ComPtr<IWICBitmapFrameDecode> frame;

        hr =
            decoder->GetFrame(
                0,
                frame.GetAddressOf());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"Texture Frame을 가져오는데 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        UINT width = 0u;
        UINT height = 0u;

        hr =
            frame->GetSize(
                &width,
                &height);

        if (FAILED(hr) ||
            width == 0u ||
            height == 0u)
        {
            lastErrorMessage_ =
                L"Texture 크기를 읽는데 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        // --------------------------------------------------------------------
        // 3. 입력 이미지 Format을 32bit RGBA로 통일한다.
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
                L"Texture를 32bpp RGBA로 변환하는데 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        constexpr UINT bytesPerPixel = 4u;

        const UINT rowPitch =
            width *
            bytesPerPixel;

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(rowPitch) *
            static_cast<std::size_t>(height));

        hr =
            formatConverter->CopyPixels(
                nullptr,
                rowPitch,
                static_cast<UINT>(pixels.size()),
                pixels.data());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"Texture Pixel 복사에 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        // --------------------------------------------------------------------
        // 4. Texture의 용도에 맞는 Color Space를 선택한다.
        //
        // Color Texture:
        // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        //
        // Data Texture(Splat Map):
        // DXGI_FORMAT_R8G8B8A8_UNORM
        // --------------------------------------------------------------------
        const DXGI_FORMAT textureFormat =
            colorSpace == TextureColorSpace::SRGB
            ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            : DXGI_FORMAT_R8G8B8A8_UNORM;

        // --------------------------------------------------------------------
        // 5. GPU Texture 생성
        //
        // MipLevels = 0:
        // 전체 Mip Chain을 생성한다.
        //
        // GENERATE_MIPS:
        // 원본 이미지를 0번 Mip에 업로드한 뒤 GPU가 하위 Mip을 생성한다.
        // --------------------------------------------------------------------
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 0u;
        textureDesc.ArraySize = 1u;
        textureDesc.Format = textureFormat;
        textureDesc.SampleDesc.Count = 1u;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;

        textureDesc.BindFlags =
            D3D11_BIND_SHADER_RESOURCE |
            D3D11_BIND_RENDER_TARGET;

        textureDesc.MiscFlags =
            D3D11_RESOURCE_MISC_GENERATE_MIPS;

        hr =
            device->CreateTexture2D(
                &textureDesc,
                nullptr,
                texture_.GetAddressOf());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"ID3D11Texture2D 생성에 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        // --------------------------------------------------------------------
        // 6. Shader Resource View 생성
        // --------------------------------------------------------------------
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

        srvDesc.Format = textureFormat;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0u;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(-1);

        hr =
            device->CreateShaderResourceView(
                texture_.Get(),
                &srvDesc,
                shaderResourceView_.GetAddressOf());

        if (FAILED(hr))
        {
            lastErrorMessage_ =
                L"Texture ShaderResourceView 생성에 실패했습니다.\nHRESULT: " +
                HResultToString(hr);

            break;
        }

        // 원본 Pixel을 가장 높은 해상도의 Mip Level에 업로드한다.
        context->UpdateSubresource(
            texture_.Get(),
            0u,
            nullptr,
            pixels.data(),
            rowPitch,
            0u);

        // 하위 Mip Level을 자동 생성한다.
        context->GenerateMips(
            shaderResourceView_.Get());

        success = true;
    }
    while (false);

    if (shouldCoUninitialize)
    {
        CoUninitialize();
    }

    loaded_ = success;
    return success;
}

void Texture2D::BindPS(
    ID3D11DeviceContext* context,
    UINT slot) const
{
    ID3D11ShaderResourceView* view =
        shaderResourceView_.Get();

    context->PSSetShaderResources(
        slot,
        1u,
        &view);
}

void Texture2D::UnbindPS(
    ID3D11DeviceContext* context,
    UINT slot)
{
    ID3D11ShaderResourceView* nullView = nullptr;

    context->PSSetShaderResources(
        slot,
        1u,
        &nullView);
}

bool Texture2D::IsLoaded() const
{
    return loaded_;
}

const std::wstring& Texture2D::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}
