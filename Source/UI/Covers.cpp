#include "UI/Covers.h"

#include "UI/CoverPlatform.h"

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
        constexpr unsigned kThumbWidth = 148;
        constexpr unsigned kThumbHeight = 69;
        constexpr unsigned kFullWidth = 460;
        constexpr unsigned kFullHeight = 215;
        constexpr std::size_t kMaxThumbs = 300;
        constexpr std::size_t kMaxFull = 6;
        constexpr std::size_t kMaxFiles = 200;
        constexpr unsigned kAvatarSize = 64;
        constexpr std::uint64_t kFileKey = 1ull << 63;

        struct Entry
        {
            ImTextureID View{};
            int LastUsedFrame = 0;
            std::filesystem::path File;
            unsigned Width = 0;
            unsigned Height = 0;
            bool Pending = false;
            CoverPlatform::Pixels Pixels;
        };

        enum class Kind
        {
            Thumb,
            Full,
            File
        };

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

        void Work()
        {
            CoverPlatform::BeginWorker();

            while (true)
            {
                std::uint64_t key = 0;
                std::filesystem::path root;
                std::filesystem::path picture;
                unsigned pictureWidth = 0;
                unsigned pictureHeight = 0;
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

                CoverPlatform::Pixels pixels;
                bool decoded = false;
                if (KindOf(key) == Kind::File)
                {
                    std::error_code error;
                    if (!picture.empty() && std::filesystem::is_regular_file(picture, error))
                        decoded = CoverPlatform::Decode(picture, pictureWidth, pictureHeight, static_cast<float>(pictureWidth) / static_cast<float>(pictureHeight), pixels);
                }
                else
                {
                    const bool full = KindOf(key) == Kind::Full;
                    const auto appId = static_cast<std::uint32_t>(key >> 1);
                    if (const auto file = FindPicture(root, appId); !file.empty())
                        decoded = CoverPlatform::Decode(file, static_cast<unsigned>(static_cast<float>(full ? kFullWidth : kThumbWidth) * scale), static_cast<unsigned>(static_cast<float>(full ? kFullHeight : kThumbHeight) * scale), kAspect, pixels);
                }

                ImTextureID view{};
                if (decoded && CoverPlatform::UploadsOnWorker())
                    view = CoverPlatform::Upload(pixels);

                std::lock_guard lock(g_lock);
                const auto entry = g_entries.find(key);
                if (entry != g_entries.end() && entry->second.View == ImTextureID{} && !entry->second.Pending)
                {
                    if (view != ImTextureID{})
                        entry->second.View = view;
                    else if (decoded && !CoverPlatform::UploadsOnWorker())
                    {
                        entry->second.Pixels = std::move(pixels);
                        entry->second.Pending = true;
                    }
                }
                else if (view != ImTextureID{})
                    CoverPlatform::Release(view);
            }

            CoverPlatform::EndWorker();
        }

        void Forget(Entry& entry)
        {
            if (entry.View != ImTextureID{})
                CoverPlatform::Release(entry.View);
            entry.View = ImTextureID{};
        }
    }

    void Initialise(void* device)
    {
        CoverPlatform::SetDevice(device);
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
            Forget(entry);
        g_entries.clear();
        g_requests.clear();
        CoverPlatform::SetDevice(nullptr);
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
            Forget(entry->second);
            std::erase(g_requests, entry->first);
            entry = g_entries.erase(entry);
        }
    }

    void NewFrame()
    {
        ++g_frame;
        if (!CoverPlatform::UploadsOnWorker())
        {
            std::lock_guard lock(g_lock);
            for (auto& [key, entry] : g_entries)
            {
                if (!entry.Pending)
                    continue;
                entry.View = CoverPlatform::Upload(entry.Pixels);
                entry.Pixels = {};
                entry.Pending = false;
            }
        }
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
                Forget(entry->second);
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
        return entry->second.View;
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
            created.Width = static_cast<unsigned>(width);
            created.Height = static_cast<unsigned>(height);
            entry = g_entries.emplace(key, std::move(created)).first;
            g_requests.push_back(key);
            g_wake.notify_one();
        }
        entry->second.LastUsedFrame = g_frame;
        return entry->second.View;
    }

    void RetryFiles()
    {
        std::lock_guard lock(g_lock);
        for (auto entry = g_entries.begin(); entry != g_entries.end();)
        {
            const bool waiting = std::find(g_requests.begin(), g_requests.end(), entry->first) != g_requests.end();
            if (KindOf(entry->first) == Kind::File && entry->second.View == ImTextureID{} && !entry->second.Pending && !waiting)
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
