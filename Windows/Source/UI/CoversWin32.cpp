#include "UI/CoverPlatform.h"

#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>

namespace Idler::UI::CoverPlatform
{
    namespace
    {
        ID3D11Device* g_device = nullptr;
        IWICImagingFactory* g_factory = nullptr;

        template <typename T>
        void Drop(T*& object)
        {
            if (object)
            {
                object->Release();
                object = nullptr;
            }
        }
    }

    void SetDevice(void* device)
    {
        g_device = static_cast<ID3D11Device*>(device);
    }

    bool UploadsOnWorker()
    {
        return true;
    }

    void BeginWorker()
    {
        ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_factory));
    }

    void EndWorker()
    {
        Drop(g_factory);
        ::CoUninitialize();
    }

    bool Decode(const std::filesystem::path& file, unsigned width, unsigned height, float shape, Pixels& pixels)
    {
        if (!g_factory)
            return false;

        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICBitmapClipper* clipper = nullptr;
        IWICBitmapScaler* scaler = nullptr;
        IWICFormatConverter* converter = nullptr;

        HRESULT hr = g_factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        UINT sourceWidth = 0;
        UINT sourceHeight = 0;
        if (SUCCEEDED(hr)) hr = frame->GetSize(&sourceWidth, &sourceHeight);
        if (SUCCEEDED(hr) && (sourceWidth == 0 || sourceHeight == 0))
            hr = E_FAIL;

        WICRect cut{ 0, 0, static_cast<INT>(sourceWidth), static_cast<INT>(sourceHeight) };
        if (SUCCEEDED(hr))
        {
            const float aspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
            if (aspect > shape + 0.02f)
            {
                cut.Width = static_cast<INT>(static_cast<float>(sourceHeight) * shape);
                cut.X = (static_cast<INT>(sourceWidth) - cut.Width) / 2;
            }
            else if (aspect < shape - 0.02f)
            {
                cut.Height = static_cast<INT>(static_cast<float>(sourceWidth) / shape);
                cut.Y = (static_cast<INT>(sourceHeight) - cut.Height) / 2;
            }
            hr = g_factory->CreateBitmapClipper(&clipper);
            if (static_cast<UINT>(cut.Width) < width)
            {
                width = static_cast<UINT>(cut.Width);
                height = static_cast<UINT>(cut.Height);
            }
        }
        if (SUCCEEDED(hr)) hr = clipper->Initialize(frame, &cut);
        if (SUCCEEDED(hr)) hr = g_factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) hr = scaler->Initialize(clipper, width, height, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr)) hr = g_factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr)) hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

        if (SUCCEEDED(hr))
        {
            pixels.Bgra.resize(static_cast<std::size_t>(width) * height * 4);
            pixels.Width = width;
            pixels.Height = height;
            hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.Bgra.size()), pixels.Bgra.data());
        }

        Drop(converter);
        Drop(scaler);
        Drop(clipper);
        Drop(frame);
        Drop(decoder);
        return SUCCEEDED(hr);
    }

    ImTextureID Upload(const Pixels& pixels)
    {
        if (!g_device || pixels.Bgra.empty())
            return ImTextureID{};

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = pixels.Width;
        desc.Height = pixels.Height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA data{ pixels.Bgra.data(), pixels.Width * 4, 0 };

        ID3D11ShaderResourceView* view = nullptr;
        ID3D11Texture2D* texture = nullptr;
        if (SUCCEEDED(g_device->CreateTexture2D(&desc, &data, &texture)))
        {
            g_device->CreateShaderResourceView(texture, nullptr, &view);
            texture->Release();
        }
        return view ? static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(view)) : ImTextureID{};
    }

    void Release(ImTextureID texture)
    {
        if (texture != ImTextureID{})
            reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<std::uintptr_t>(texture))->Release();
    }
}
