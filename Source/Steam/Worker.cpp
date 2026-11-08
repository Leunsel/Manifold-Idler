#include "Steam/Worker.h"

#include "Core/Library.h"
#include "Core/Text.h"
#include "Steam/SteamClient.h"
#include "Steam/SteamLocal.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace Idler::Worker
{
    namespace
    {
        constexpr wchar_t kIdleArgument[] = L"--idle=";
        constexpr wchar_t kScanArgument[] = L"--scan";
        constexpr wchar_t kAchievementsArgument[] = L"--achievements=";
        constexpr wchar_t kChangeAchievementsArgument[] = L"--change-achievements=";

        constexpr std::uint32_t kFreePackageApp = 480;

        void Say(const std::string& line)
        {
            const std::string text = line + "\n";
#ifdef _WIN32
            DWORD written = 0;
            ::WriteFile(::GetStdHandle(STD_OUTPUT_HANDLE), text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
#else
            std::size_t done = 0;
            while (done < text.size())
            {
                const auto written = ::write(STDOUT_FILENO, text.data() + done, text.size() - done);
                if (written <= 0)
                    return;
                done += static_cast<std::size_t>(written);
            }
#endif
        }

        std::size_t ReadInput(char* buffer, std::size_t size)
        {
#ifdef _WIN32
            DWORD count = 0;
            return ::ReadFile(::GetStdHandle(STD_INPUT_HANDLE), buffer, static_cast<DWORD>(size), &count, nullptr) ? count : 0;
#else
            const auto count = ::read(STDIN_FILENO, buffer, size);
            return count > 0 ? static_cast<std::size_t>(count) : 0;
#endif
        }

        void CountAsGame(std::uint32_t appId)
        {
#ifdef _WIN32
            const std::wstring id = std::to_wstring(appId);
            ::SetEnvironmentVariableW(L"SteamAppId", id.c_str());
            ::SetEnvironmentVariableW(L"SteamGameId", id.c_str());
#else
            const std::string id = std::to_string(appId);
            ::setenv("SteamAppId", id.c_str(), 1);
            ::setenv("SteamGameId", id.c_str(), 1);
#endif
        }

        struct InputWatch
        {
            std::mutex Lock;
            std::condition_variable Wake;
            bool Closed = false;
        };

        int Idle(std::uint32_t appId)
        {
            const Steam::Status status = Steam::ReadStatus();
            if (!status.LoggedOn())
            {
                Say("error steam Steam is not running.");
                return 3;
            }

            CountAsGame(appId);

            Steam::Connection steam;
            std::string error;
            if (!steam.Connect(Steam::ClientLibrary(status), error))
            {
                Say("error steam " + error);
                return 3;
            }
            if (!steam.User()->BLoggedOn())
            {
                Say("error steam Steam is offline.");
                return 3;
            }
            if (!steam.Apps()->BIsSubscribedApp(appId))
            {
                Say("error app This account does not own the game.");
                return 4;
            }
            if (steam.Utils()->GetAppID() != appId)
            {
                Say("error app Steam did not accept the game.");
                return 4;
            }

            const auto watch = std::make_shared<InputWatch>();
            std::thread([watch]()
                {
                    char buffer[64];
                    while (ReadInput(buffer, sizeof(buffer)) > 0)
                    {
                    }
                    const std::lock_guard<std::mutex> guard(watch->Lock);
                    watch->Closed = true;
                    watch->Wake.notify_all();
                }).detach();

            Say("ready");

#ifdef _WIN32
            ::SetProcessWorkingSetSize(::GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
            std::unique_lock<std::mutex> lock(watch->Lock);
            while (true)
            {
                if (watch->Wake.wait_for(lock, std::chrono::seconds(1), [&]() { return watch->Closed; }))
                    return 0;
                bool closing = !Steam::ProcessLives(status.ProcessId);
                steam.DrainCallbacks([&](int callback, const void*) { closing |= callback == Steam::kCallbackSteamShutdown; });
                if (closing)
                {
                    Say("gone");
                    return 0;
                }
            }
        }

        std::string ReadAllInput()
        {
            std::string input;
            char buffer[4096];
            while (const std::size_t count = ReadInput(buffer, sizeof(buffer)))
                input.append(buffer, count);
            return input;
        }

        std::string OneLine(const char* text)
        {
            std::string line = text ? text : "";
            for (char& c : line)
            {
                if (c == '\t' || c == '\r' || c == '\n')
                    c = ' ';
            }
            return line;
        }

        int WaitForStats(Steam::Connection& steam, std::uint32_t appId, int kind)
        {
            int result = -1;
            for (int tries = 0; tries < 300 && result < 0; ++tries)
            {
                steam.DrainCallbacks([&](int callback, const void* data)
                    {
                        if (callback != kind || !data)
                            return;
                        const auto* stats = static_cast<const Steam::StatsCallback*>(data);
                        if (static_cast<std::uint32_t>(stats->GameId) == appId)
                            result = stats->Result;
                    });
                if (result < 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return result;
        }

        int OpenStats(Steam::Connection& steam, const Steam::Status& status, std::uint32_t appId)
        {
            if (!status.LoggedOn())
            {
                Say("error steam Steam is not running.");
                return 3;
            }

            CountAsGame(appId);

            std::string error;
            if (!steam.Connect(Steam::ClientLibrary(status), error))
            {
                Say("error steam " + error);
                return 3;
            }
            if (!steam.User()->BLoggedOn())
            {
                Say("error steam Steam is offline.");
                return 3;
            }
            if (!steam.Apps()->BIsSubscribedApp(appId))
            {
                Say("error app This account does not own the game.");
                return 4;
            }

            steam.UserStats()->RequestCurrentStats();
            if (const int result = WaitForStats(steam, appId, Steam::kCallbackStatsReceived); result != 1)
            {
                Say(result < 0 ? "error steam Steam did not send the achievements." : "error app Steam has no achievements for this game.");
                return result < 0 ? 3 : 4;
            }
            return 0;
        }

        void ListAchievements(Steam::IUserStats* stats)
        {
            const std::uint32_t count = stats->GetNumAchievements();
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const char* name = stats->GetAchievementName(index);
                if (!name)
                    continue;
                bool achieved = false;
                std::uint32_t unlocked = 0;
                stats->GetAchievementAndUnlockTime(name, &achieved, &unlocked);
                const char* hidden = stats->GetAchievementDisplayAttribute(name, "hidden");
                Say(std::string("ach ") + (achieved ? "1 " : "0 ") + std::to_string(unlocked) + " " + (hidden && hidden[0] == '1' ? "1 " : "0 ") + OneLine(name) + "\t" +
                    OneLine(stats->GetAchievementDisplayAttribute(name, "name")) + "\t" + OneLine(stats->GetAchievementDisplayAttribute(name, "desc")));
            }
        }

        std::optional<double> StatValue(Steam::IUserStats* stats, const StatSchema& stat)
        {
            if (stat.Kind == StatKind::Int)
            {
                std::int32_t value = 0;
                if (stats->GetStatInt(stat.Name.c_str(), &value))
                    return static_cast<double>(value);
                return std::nullopt;
            }
            float value = 0.0f;
            if (stats->GetStatFloat(stat.Name.c_str(), &value))
                return static_cast<double>(value);
            return std::nullopt;
        }

        void ListStats(Steam::IUserStats* stats, const std::vector<StatSchema>& schema)
        {
            for (const StatSchema& stat : schema)
            {
                if (const std::optional<double> value = StatValue(stats, stat))
                    Say("stat " + NumberText(*value, stat.Kind) + " " + OneLine(stat.Name.c_str()));
            }
        }

        int Achievements(std::uint32_t appId)
        {
            const Steam::Status status = Steam::ReadStatus();
            Steam::Connection steam;
            if (const int failed = OpenStats(steam, status, appId))
                return failed;
            ListAchievements(steam.UserStats());
            ListStats(steam.UserStats(), Steam::ReadStatSchema(status, appId));
            Say("done");
            return 0;
        }

        int ChangeAchievements(std::uint32_t appId)
        {
            const std::vector<AchievementChange> changes = ParseAchievementChanges(ReadAllInput());
            const Steam::Status status = Steam::ReadStatus();
            Steam::Connection steam;
            if (const int failed = OpenStats(steam, status, appId))
                return failed;

            Steam::IUserStats* stats = steam.UserStats();
            const std::vector<AchievementIcons> schema = Steam::ReadAchievementIcons(status, appId);
            const std::vector<StatSchema> statSchema = Steam::ReadStatSchema(status, appId);
            int asked = 0;
            for (const AchievementChange& change : changes)
            {
                bool taken = false;
                if (change.SetsStat)
                {
                    const StatSchema* stat = FindStat(statSchema, change.Id);
                    const std::optional<double> current = stat ? StatValue(stats, *stat) : std::nullopt;
                    if (stat && current && StatChangeProblem(*stat, *current, change.Value).empty())
                    {
                        taken = stat->Kind == StatKind::Int ? stats->SetStatInt(stat->Name.c_str(), static_cast<std::int32_t>(std::llround(change.Value)))
                                                            : stats->SetStatFloat(stat->Name.c_str(), static_cast<float>(change.Value));
                    }
                }
                else
                {
                    taken = MaySetAchievement(change.Id, schema) &&
                        (change.Unlock ? stats->SetAchievement(change.Id.c_str()) : stats->ClearAchievement(change.Id.c_str()));
                }
                if (taken)
                    ++asked;
                else
                    Say("refused " + OneLine(change.Id.c_str()));
            }
            if (asked > 0)
            {
                if (!stats->StoreStats())
                {
                    Say("error steam Steam did not take the change.");
                    return 3;
                }
                const int stored = WaitForStats(steam, appId, Steam::kCallbackStatsStored);
                if (stored < 0)
                {
                    Say("error steam Steam did not confirm the change.");
                    return 3;
                }
                Say("stored " + std::to_string(stored));
            }
            ListAchievements(stats);
            ListStats(stats, statSchema);
            Say("done");
            return 0;
        }

        int Scan()
        {
            const Steam::Status status = Steam::ReadStatus();
            if (!status.LoggedOn())
            {
                Say("error steam Steam is not running.");
                return 3;
            }

            const std::string input = ReadAllInput();

            Steam::Connection steam;
            std::string error;
            if (!steam.Connect(Steam::ClientLibrary(status), error))
            {
                Say("error steam " + error);
                return 3;
            }

#ifdef _WIN32
            std::uint64_t steamId = 0;
            steam.User()->GetSteamID(&steamId);
#else
            const std::uint64_t steamId = steam.User()->GetSteamID();
#endif
            Say("client " + std::to_string(static_cast<std::uint32_t>(steamId)) + " " + std::to_string(steam.Apps()->GetEarliestPurchaseUnixTime(kFreePackageApp)));
            if (const char* persona = steam.Friends()->GetPersonaName())
                Say(std::string("persona ") + persona);

            std::size_t start = 0;
            while (start < input.size())
            {
                auto end = input.find('\n', start);
                if (end == std::string::npos)
                    end = input.size();
                if (const auto appId = ParseUnsigned(Trim(std::string_view(input).substr(start, end - start))))
                {
                    if (steam.Apps()->BIsSubscribedApp(*appId))
                    {
                        Say("app " + std::to_string(*appId) + " " + std::to_string(steam.Apps()->GetEarliestPurchaseUnixTime(*appId)) + " " +
                            (steam.Apps()->BIsAppInstalled(*appId) ? "1" : "0"));
                    }
                }
                start = end + 1;
            }
            Say("done");
            return 0;
        }
    }

    bool IsWorkerCommand(const wchar_t* commandLine)
    {
        return commandLine && (std::wcsstr(commandLine, kIdleArgument) || std::wcsstr(commandLine, kScanArgument) || std::wcsstr(commandLine, kAchievementsArgument) ||
            std::wcsstr(commandLine, kChangeAchievementsArgument));
    }

    int Run(const wchar_t* commandLine)
    {
#ifdef _WIN32
        ::SetPriorityClass(::GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
        const int gentler = ::nice(5);
        static_cast<void>(gentler);
#endif

        if (const wchar_t* argument = std::wcsstr(commandLine, kIdleArgument))
        {
            const auto appId = static_cast<std::uint32_t>(std::wcstoul(argument + std::wcslen(kIdleArgument), nullptr, 10));
            if (appId == 0)
                return 2;
            return Idle(appId);
        }
        if (const wchar_t* argument = std::wcsstr(commandLine, kChangeAchievementsArgument))
        {
            const auto appId = static_cast<std::uint32_t>(std::wcstoul(argument + std::wcslen(kChangeAchievementsArgument), nullptr, 10));
            if (appId == 0)
                return 2;
            return ChangeAchievements(appId);
        }
        if (const wchar_t* argument = std::wcsstr(commandLine, kAchievementsArgument))
        {
            const auto appId = static_cast<std::uint32_t>(std::wcstoul(argument + std::wcslen(kAchievementsArgument), nullptr, 10));
            if (appId == 0)
                return 2;
            return Achievements(appId);
        }
        return Scan();
    }
}
