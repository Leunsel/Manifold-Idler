#include "UI/Covers.h"

#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Idler::UI::Covers
{
    namespace
    {
        constexpr UINT kThumbWidth = 148;
        constexpr UINT kThumbHeight = 69;
        constexpr UINT kFullWidth = 460;
        constexpr UINT kFullHeight = 215;
        constexpr std::size_t kMaxThumbs = 300;
        constexpr std::size_t kMaxFull = 6;
        constexpr std::size_t kMaxFiles = 200;
        constexpr UINT kAvatarSize = 64;
        constexpr std::uint64_t kFileKey = 1ull << 63;

        struct Entry
        {
            ID3D11ShaderResourceView* View = nullptr;
            int LastUsedFrame = 0;
            std::filesystem::path File;
            UINT Width = 0;
            UINT Height = 0;
        };

        enum class Kind
        {
            Thumb,
            Full,
            File
        };

        ID3D11Device* g_device = nullptr;
        std::filesystem::path g_root;
        int g_frame = 0;
        float g_pictureScale = 1.0f;

        std::mutex g_lock;
        std::condition_variable g_wake;
        std::thread g_thread;
        bool g_stop = false;
        bool g_working = false;
        std::unordered_map<std::uint64_t, Entry> g_entries;
        std::vector<std::uint64_t> g_requests;

        std::uint64_t KeyOf(std::uint32_t appId, Size size)
        {
            return (static_cast<std::uint64_t>(appId) << 1) | (size == Size::Full ? 1u : 0u);
        }

        std::uint64_t KeyOfFile(const std::filesystem::path& file, int width, int height)
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const wchar_t c : file.native())
            {
                hash ^= static_cast<std::uint64_t>(c);
                hash *= 1099511628211ull;
            }
            hash ^= (static_cast<std::uint64_t>(width) << 16) | static_cast<std::uint64_t>(height);
            hash *= 1099511628211ull;
            return hash | kFileKey;
        }

        Kind KindOf(std::uint64_t key)
        {
            return (key & kFileKey) != 0 ? Kind::File : (key & 1) != 0 ? Kind::Full : Kind::Thumb;
        }

        template <typename T>
        void Release(T*& object)
        {
            if (object)
            {
                object->Release();
                object = nullptr;
            }
        }

        std::filesystem::path FindPicture(const std::filesystem::path& root, std::uint32_t appId)
        {
            const std::filesystem::path folder = root / L"appcache" / L"librarycache" / std::to_wstring(appId);
            std::error_code error;
            if (!std::filesystem::is_directory(folder, error))
                return {};

            std::vector<std::filesystem::path> folders{ folder };
            for (const auto& item : std::filesystem::directory_iterator(folder, error))
            {
                if (item.is_directory(error))
                    folders.push_back(item.path());
            }
            for (const wchar_t* name : { L"header.jpg", L"library_header.jpg", L"library_hero.jpg", L"library_600x900.jpg" })
            {
                for (const auto& place : folders)
                {
                    const auto file = place / name;
                    if (std::filesystem::is_regular_file(file, error))
                        return file;
                }
            }
            for (const auto& place : folders)
            {
                for (const auto& item : std::filesystem::directory_iterator(place, error))
                {
                    if (item.path().filename().wstring().starts_with(L"header") && item.path().extension() == L".jpg")
                        return item.path();
                }
            }
            return {};
        }

        ID3D11ShaderResourceView* Read(IWICImagingFactory* factory, const std::filesystem::path& file, UINT width, UINT height, float shape)
        {
            IWICBitmapDecoder* decoder = nullptr;
            IWICBitmapFrameDecode* frame = nullptr;
            IWICBitmapClipper* clipper = nullptr;
            IWICBitmapScaler* scaler = nullptr;
            IWICFormatConverter* converter = nullptr;
            ID3D11ShaderResourceView* view = nullptr;

            HRESULT hr = factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
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
                hr = factory->CreateBitmapClipper(&clipper);
                if (static_cast<UINT>(cut.Width) < width)
                {
                    width = static_cast<UINT>(cut.Width);
                    height = static_cast<UINT>(cut.Height);
                }
            }
            if (SUCCEEDED(hr)) hr = clipper->Initialize(frame, &cut);
            if (SUCCEEDED(hr)) hr = factory->CreateBitmapScaler(&scaler);
            if (SUCCEEDED(hr)) hr = scaler->Initialize(clipper, width, height, WICBitmapInterpolationModeFant);
            if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
            if (SUCCEEDED(hr)) hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

            std::vector<BYTE> pixels;
            if (SUCCEEDED(hr))
            {
                pixels.resize(static_cast<std::size_t>(width) * height * 4);
                hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
            }
            if (SUCCEEDED(hr))
            {
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = width;
                desc.Height = height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                const D3D11_SUBRESOURCE_DATA data{ pixels.data(), width * 4, 0 };

                ID3D11Texture2D* texture = nullptr;
                if (SUCCEEDED(g_device->CreateTexture2D(&desc, &data, &texture)))
                {
                    g_device->CreateShaderResourceView(texture, nullptr, &view);
                    texture->Release();
                }
            }

            Release(converter);
            Release(scaler);
            Release(clipper);
            Release(frame);
            Release(decoder);
            return view;
        }

        void Work()
        {
            ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            IWICImagingFactory* factory = nullptr;
            ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

            while (true)
            {
                std::uint64_t key = 0;
                std::filesystem::path root;
                std::filesystem::path picture;
                UINT pictureWidth = 0;
                UINT pictureHeight = 0;
                float scale = 1.0f;
                {
                    std::unique_lock lock(g_lock);
                    g_working = false;
                    g_wake.wait(lock, [] { return g_stop || !g_requests.empty(); });
                    if (g_stop)
                        break;
                    key = g_requests.back();
                    g_requests.pop_back();
                    root = g_root;
                    scale = g_pictureScale;
                    g_working = true;
                    if (const auto entry = g_entries.find(key); entry != g_entries.end())
                    {
                        picture = entry->second.File;
                        pictureWidth = entry->second.Width;
                        pictureHeight = entry->second.Height;
                    }
                }

                ID3D11ShaderResourceView* view = nullptr;
                if (factory && KindOf(key) == Kind::File)
                {
                    std::error_code error;
                    if (!picture.empty() && std::filesystem::is_regular_file(picture, error))
                        view = Read(factory, picture, pictureWidth, pictureHeight, static_cast<float>(pictureWidth) / static_cast<float>(pictureHeight));
                }
                else if (factory)
                {
                    const bool full = KindOf(key) == Kind::Full;
                    const auto appId = static_cast<std::uint32_t>(key >> 1);
                    if (const auto file = FindPicture(root, appId); !file.empty())
                        view = Read(factory, file, static_cast<UINT>(static_cast<float>(full ? kFullWidth : kThumbWidth) * scale), static_cast<UINT>(static_cast<float>(full ? kFullHeight : kThumbHeight) * scale), kAspect);
                }

                std::lock_guard lock(g_lock);
                const auto entry = g_entries.find(key);
                if (entry != g_entries.end() && !entry->second.View)
                    entry->second.View = view;
                else if (view)
                    view->Release();
            }

            Release(factory);
            ::CoUninitialize();
        }
    }

    void Initialise(ID3D11Device* device)
    {
        g_device = device;
        g_stop = false;
        g_thread = std::thread(Work);
    }

    void Shutdown()
    {
        {
            std::lock_guard lock(g_lock);
            g_stop = true;
        }
        g_wake.notify_all();
        if (g_thread.joinable())
            g_thread.join();

        for (auto& [key, entry] : g_entries)
        {
            if (entry.View)
                entry.View->Release();
        }
        g_entries.clear();
        g_requests.clear();
        g_device = nullptr;
    }

    void SetSteamRoot(const std::filesystem::path& root)
    {
        if (root == g_root)
            return;
        std::lock_guard lock(g_lock);
        g_root = root;
    }

    void SetPictureScale(float scale)
    {
        if (scale == g_pictureScale)
            return;
        std::lock_guard lock(g_lock);
        g_pictureScale = scale;
        for (auto entry = g_entries.begin(); entry != g_entries.end();)
        {
            if (KindOf(entry->first) == Kind::File)
            {
                ++entry;
                continue;
            }
            if (entry->second.View)
                entry->second.View->Release();
            std::erase(g_requests, entry->first);
            entry = g_entries.erase(entry);
        }
    }

    void NewFrame()
    {
        ++g_frame;
        if (g_entries.size() <= kMaxThumbs + kMaxFull + kMaxFiles)
            return;

        std::lock_guard lock(g_lock);
        for (const Kind kind : { Kind::Thumb, Kind::Full, Kind::File })
        {
            std::vector<std::pair<int, std::uint64_t>> known;
            for (const auto& [key, entry] : g_entries)
            {
                if (KindOf(key) == kind && g_frame - entry.LastUsedFrame > 2)
                    known.emplace_back(entry.LastUsedFrame, key);
            }
            const std::size_t limit = kind == Kind::File ? kMaxFiles : kind == Kind::Full ? kMaxFull : kMaxThumbs;
            if (known.size() <= limit)
                continue;
            std::sort(known.begin(), known.end());
            for (std::size_t index = 0; index + limit < known.size(); ++index)
            {
                const auto entry = g_entries.find(known[index].second);
                if (entry->second.View)
                    entry->second.View->Release();
                g_entries.erase(entry);
                std::erase(g_requests, known[index].second);
            }
        }
    }

    ImTextureID Get(std::uint32_t appId, Size size)
    {
        const std::uint64_t key = KeyOf(appId, size);
        std::lock_guard lock(g_lock);
        auto entry = g_entries.find(key);
        if (entry == g_entries.end())
        {
            entry = g_entries.emplace(key, Entry{}).first;
            g_requests.push_back(key);
            g_wake.notify_one();
        }
        entry->second.LastUsedFrame = g_frame;
        return entry->second.View ? static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(entry->second.View)) : ImTextureID{};
    }

    ImTextureID File(const std::filesystem::path& file, int width, int height)
    {
        if (file.empty())
            return {};
        const std::uint64_t key = KeyOfFile(file, width, height);
        std::lock_guard lock(g_lock);
        auto entry = g_entries.find(key);
        if (entry == g_entries.end())
        {
            Entry created;
            created.File = file;
            created.Width = static_cast<UINT>(width);
            created.Height = static_cast<UINT>(height);
            entry = g_entries.emplace(key, std::move(created)).first;
            g_requests.push_back(key);
            g_wake.notify_one();
        }
        entry->second.LastUsedFrame = g_frame;
        return entry->second.View ? static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(entry->second.View)) : ImTextureID{};
    }

    void RetryFiles()
    {
        std::lock_guard lock(g_lock);
        for (auto entry = g_entries.begin(); entry != g_entries.end();)
        {
            const bool waiting = std::find(g_requests.begin(), g_requests.end(), entry->first) != g_requests.end();
            if (KindOf(entry->first) == Kind::File && !entry->second.View && !waiting)
                entry = g_entries.erase(entry);
            else
                ++entry;
        }
    }

    ImTextureID Avatar(std::uint64_t steamId)
    {
        if (steamId == 0 || g_root.empty())
            return {};
        return File(g_root / L"config" / L"avatarcache" / (std::to_wstring(steamId) + L".png"), kAvatarSize, kAvatarSize);
    }

    bool Loading()
    {
        std::lock_guard lock(g_lock);
        return g_working || !g_requests.empty();
    }
}
