#pragma once

#include <filesystem>

namespace Idler
{
    class FileLock
    {
    public:
        FileLock() = default;
        ~FileLock();

        FileLock(const FileLock&) = delete;
        FileLock& operator=(const FileLock&) = delete;

        bool Take(const std::filesystem::path& file);
        void Release();
        bool Held() const;

    private:
#ifdef _WIN32
        void* m_handle = nullptr;
#else
        int m_descriptor = -1;
#endif
    };

    std::filesystem::path SettingsLockFile(const std::filesystem::path& settings);
    bool SettingsInUse(const std::filesystem::path& settings);
}
