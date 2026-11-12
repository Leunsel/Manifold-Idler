#include "Core/FileLock.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Idler
{
    FileLock::~FileLock()
    {
        Release();
    }

    bool FileLock::Take(const std::filesystem::path& file)
    {
        Release();
        std::error_code ignored;
        if (file.has_parent_path())
            std::filesystem::create_directories(file.parent_path(), ignored);
#ifdef _WIN32
        const HANDLE handle = ::CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        m_handle = handle;
        return true;
#else
        const int descriptor = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (descriptor < 0)
            return false;
        if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0)
        {
            ::close(descriptor);
            return false;
        }
        m_descriptor = descriptor;
        return true;
#endif
    }

    void FileLock::Release()
    {
#ifdef _WIN32
        if (m_handle)
            ::CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
#else
        if (m_descriptor >= 0)
            ::close(m_descriptor);
        m_descriptor = -1;
#endif
    }

    bool FileLock::Held() const
    {
#ifdef _WIN32
        return m_handle != nullptr;
#else
        return m_descriptor >= 0;
#endif
    }

    std::filesystem::path SettingsLockFile(const std::filesystem::path& settings)
    {
        return std::filesystem::path(settings).replace_extension(".lock");
    }

    bool SettingsInUse(const std::filesystem::path& settings)
    {
        FileLock probe;
        return !probe.Take(SettingsLockFile(settings));
    }
}
