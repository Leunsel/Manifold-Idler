#include "Paths.h"

#include "Core/FileLock.h"
#include "Core/Text.h"
#include "Ipc.h"
#include "Request.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <system_error>

namespace Idler::Cli
{
    std::filesystem::path PathFromUtf8(std::string_view text)
    {
#ifdef _WIN32
        return std::filesystem::path(Utf8ToWide(text));
#else
        return std::filesystem::path(std::string(text));
#endif
    }

    std::string PathToUtf8(const std::filesystem::path& path)
    {
#ifdef _WIN32
        return WideToUtf8(path.wstring());
#else
        return path.string();
#endif
    }

    std::filesystem::path DefaultSettingsFolder()
    {
#ifdef _WIN32
        std::wstring path(MAX_PATH, L'\0');
        DWORD length = 0;
        while ((length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))) == path.size())
            path.resize(path.size() * 2);
        path.resize(length);
        return std::filesystem::path(path).parent_path();
#else
        std::filesystem::path folder;
        if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && config[0])
            folder = config;
        else if (const char* home = std::getenv("HOME"); home && home[0])
            folder = std::filesystem::path(home) / ".config";
        else
            folder = ".";
        return folder / "manifold-idler";
#endif
    }

    std::filesystem::path OldSettingsFile()
    {
        return DefaultSettingsFolder() / "idler.ini";
    }

    std::filesystem::path DefaultSettingsFile()
    {
        const std::filesystem::path shared = DefaultSettingsFolder() / "Manifold-Idler.ini";
        std::error_code error;
        if (!std::filesystem::exists(shared, error) && std::filesystem::exists(OldSettingsFile(), error))
            return OldSettingsFile();
        return shared;
    }

    std::filesystem::path TakeOverOldSettings(const std::filesystem::path& settings)
    {
        const std::filesystem::path old = OldSettingsFile();
        const std::filesystem::path shared = DefaultSettingsFolder() / "Manifold-Idler.ini";
        std::error_code error;
        if (settings != old || std::filesystem::exists(shared, error))
            return settings;
        std::string reply;
        if (Ask(SocketFile(old), JoinRequest({ "ping" }), reply))
            return settings;
        FileLock oldLock;
        if (!oldLock.Take(SettingsLockFile(old)))
            return settings;

        std::string moved;
        for (const char* extension : { ".ini", ".stats", ".web-token", ".achievement-icons" })
        {
            std::filesystem::path from = old;
            from.replace_extension(extension);
            std::filesystem::path to = shared;
            to.replace_extension(extension);
            if (!std::filesystem::exists(from, error) || std::filesystem::exists(to, error))
                continue;
            std::filesystem::rename(from, to, error);
            if (error)
                return extension == std::string(".ini") ? settings : shared;
            moved += (moved.empty() ? "" : ", ") + PathToUtf8(from.filename());
        }
        std::printf("Moved %s to Manifold-Idler.*, the window program's names: both share them now.\n", moved.c_str());
        return shared;
    }

    std::string SocketName(const std::filesystem::path& settings)
    {
        std::error_code ignored;
        const std::filesystem::path real = std::filesystem::weakly_canonical(settings, ignored);
        const std::string text = PathToUtf8(ignored ? settings : real);

        std::uint64_t hash = 14695981039346656037ull;
        for (const char character : text)
        {
            hash ^= static_cast<unsigned char>(character);
            hash *= 1099511628211ull;
        }
        char name[64];
        std::snprintf(name, sizeof(name), "manifold-idler-%016llx.sock", static_cast<unsigned long long>(hash));
        return name;
    }

    std::filesystem::path SocketFile(const std::filesystem::path& settings)
    {
        const std::string name = SocketName(settings);
#ifdef _WIN32
        std::error_code ignored;
        return std::filesystem::temp_directory_path(ignored) / name;
#else
        if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime && runtime[0])
            return std::filesystem::path(runtime) / name;
        std::error_code ignored;
        return std::filesystem::temp_directory_path(ignored) / (std::to_string(::getuid()) + "-" + name);
#endif
    }

    std::filesystem::path WebTokenFile(const std::filesystem::path& settings)
    {
        return std::filesystem::path(settings).replace_extension(".web-token");
    }

    std::filesystem::path DefaultThemesFolder(const std::filesystem::path& settings)
    {
        return std::filesystem::path(settings).replace_extension(".themes");
    }
}
