#include "Steam/SteamLocal.h"

#include "Core/Text.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <system_error>
#endif

#include <fstream>
#include <iterator>

namespace Idler::Steam
{
#ifdef _WIN32
    namespace
    {
        constexpr wchar_t kSteamKey[] = L"Software\\Valve\\Steam";
        constexpr wchar_t kActiveKey[] = L"Software\\Valve\\Steam\\ActiveProcess";
        constexpr wchar_t kAppsKey[] = L"Software\\Valve\\Steam\\Apps";

        std::wstring ReadString(const wchar_t* key, const wchar_t* value)
        {
            DWORD size = 0;
            if (::RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || size == 0)
                return {};
            std::wstring text(size / sizeof(wchar_t), L'\0');
            if (::RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, text.data(), &size) != ERROR_SUCCESS)
                return {};
            text.resize(::wcslen(text.c_str()));
            return text;
        }

        DWORD ReadNumber(const wchar_t* key, const wchar_t* value)
        {
            DWORD number = 0;
            DWORD size = sizeof(number);
            if (::RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD, nullptr, &number, &size) != ERROR_SUCCESS)
                return 0;
            return number;
        }
    }

    bool ProcessLives(std::uint32_t processId)
    {
        HANDLE process = ::OpenProcess(SYNCHRONIZE, FALSE, processId);
        if (!process)
            return false;
        const bool alive = ::WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
        ::CloseHandle(process);
        return alive;
    }

    Status ReadStatus()
    {
        Status status;
        const std::wstring root = ReadString(kSteamKey, L"SteamPath");
        if (!root.empty())
            status.Root = std::filesystem::path(root).lexically_normal();

        const DWORD processId = ReadNumber(kActiveKey, L"pid");
        if (processId != 0 && ProcessLives(processId))
        {
            status.ProcessId = processId;
            status.Account = ReadNumber(kActiveKey, L"ActiveUser");
        }
        return status;
    }

    std::filesystem::path ClientLibrary(const Status& status)
    {
        const std::wstring library = ReadString(kActiveKey, L"SteamClientDll64");
        if (!library.empty())
            return library;
        return status.Root / L"steamclient64.dll";
    }

    std::vector<std::uint32_t> RunningApps()
    {
        std::vector<std::uint32_t> running;
        HKEY apps = nullptr;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, kAppsKey, 0, KEY_READ, &apps) != ERROR_SUCCESS)
            return running;

        wchar_t name[32];
        for (DWORD index = 0;; ++index)
        {
            DWORD length = ARRAYSIZE(name);
            const LONG result = ::RegEnumKeyExW(apps, index, name, &length, nullptr, nullptr, nullptr, nullptr);
            if (result == ERROR_NO_MORE_ITEMS)
                break;
            if (result != ERROR_SUCCESS)
                continue;

            DWORD value = 0;
            DWORD size = sizeof(value);
            if (::RegGetValueW(apps, name, L"Running", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS && value != 0)
            {
                if (const auto appId = ParseUnsigned(WideToUtf8(name)))
                    running.push_back(*appId);
            }
        }
        ::RegCloseKey(apps);
        return running;
    }
#else
    namespace
    {
        std::filesystem::path HomeFolder()
        {
            const char* home = std::getenv("HOME");
            return home ? std::filesystem::path(home) : std::filesystem::path();
        }

        std::string ReadTextFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            return file ? std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()) : std::string();
        }

        SteamRegistry ReadRegistry()
        {
            return ParseSteamRegistry(ReadTextFile(HomeFolder() / ".steam" / "registry.vdf"));
        }

        std::filesystem::path FindRoot()
        {
            const std::filesystem::path home = HomeFolder();
            for (const std::filesystem::path& candidate : { home / ".steam" / "root", home / ".steam" / "steam", home / ".local" / "share" / "Steam", home / ".steam" / "debian-installation" })
            {
                std::error_code ignored;
                if (std::filesystem::exists(candidate / "appcache", ignored))
                {
                    const std::filesystem::path real = std::filesystem::canonical(candidate, ignored);
                    return ignored ? candidate : real;
                }
            }
            return {};
        }
    }

    bool ProcessLives(std::uint32_t processId)
    {
        return processId != 0 && (::kill(static_cast<pid_t>(processId), 0) == 0 || errno == EPERM);
    }

    Status ReadStatus()
    {
        Status status;
        status.Root = FindRoot();
        const SteamRegistry registry = ReadRegistry();
        std::uint32_t processId = registry.ProcessId;
        if (processId == 0)
            processId = ParseUnsigned(Trim(ReadTextFile(HomeFolder() / ".steam" / "steam.pid"))).value_or(0);
        if (ProcessLives(processId))
        {
            status.ProcessId = processId;
            status.Account = registry.Account;
            if (status.Account == 0 && !status.Root.empty())
                status.Account = ParseMostRecentAccount(ReadTextFile(status.Root / "config" / "loginusers.vdf"));
        }
        return status;
    }

    std::filesystem::path ClientLibrary(const Status& status)
    {
        const SteamRegistry registry = ReadRegistry();
        if (!registry.ClientLibrary.empty())
            return registry.ClientLibrary;
        return status.Root / "linux64" / "steamclient.so";
    }

    std::vector<std::uint32_t> RunningApps()
    {
        return ReadRegistry().RunningApps;
    }
#endif

    std::vector<std::uint8_t> ReadAppInfoFile(const Status& status)
    {
        std::ifstream file(status.Root / L"appcache" / L"appinfo.vdf", std::ios::binary);
        if (!file)
            return {};
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    namespace
    {
        std::string ReadSchemaFile(const Status& status, std::uint32_t appId)
        {
            std::ifstream file(status.Root / L"appcache" / L"stats" / (L"UserGameStatsSchema_" + std::to_wstring(appId) + L".bin"), std::ios::binary);
            if (!file)
                return {};
            return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        }
    }

    std::vector<AchievementIcons> ReadAchievementIcons(const Status& status, std::uint32_t appId)
    {
        return ParseAchievementIcons(ReadSchemaFile(status, appId));
    }

    std::vector<StatSchema> ReadStatSchema(const Status& status, std::uint32_t appId)
    {
        return ParseStatSchema(ReadSchemaFile(status, appId));
    }

    void ReadUnlockRules(const Status& status, std::vector<Game>& games)
    {
        for (Game& game : games)
            game.Unlock = UnlockRuleOf(ReadAchievementIcons(status, game.AppId));
    }

    std::vector<SteamCollection> ReadCollections(const Status& status)
    {
        std::ifstream file(status.Root / L"userdata" / std::to_wstring(status.Account) / L"config" / L"cloudstorage" / L"cloud-storage-namespace-1.json", std::ios::binary);
        if (!file)
            return {};
        const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        return ParseSteamCollections(text);
    }

    AchievementMap ReadAchievementCounts(const Status& status, const std::vector<Ownership>& games)
    {
        AchievementMap counts;
        const std::filesystem::path folder = status.Root / L"userdata" / std::to_wstring(status.Account) / L"config" / L"librarycache";
        std::string text;
        for (const Ownership& game : games)
        {
            std::ifstream file(folder / (std::to_wstring(game.AppId) + L".json"), std::ios::binary);
            if (!file)
                continue;
            text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            const AchievementCount count = ParseAchievementCounts(text);
            if (count.Total > 0)
                counts[game.AppId] = count;
        }
        return counts;
    }

    std::string ReadLocalConfigFile(const Status& status)
    {
        std::ifstream file(status.Root / L"userdata" / std::to_wstring(status.Account) / L"config" / L"localconfig.vdf", std::ios::binary);
        if (!file)
            return {};
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
}
