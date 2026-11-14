#pragma once

#include "Core/Queue.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Idler;

inline std::filesystem::path TestFile(const std::string& ending)
{
    static const auto run = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("Manifold-Idler-Test-" + std::to_string(run) + ending);
}

inline FILE* OpenForWriting(const std::filesystem::path& path)
{
#ifdef _WIN32
    FILE* file = nullptr;
    _wfopen_s(&file, path.c_str(), L"wb");
    return file;
#else
    return std::fopen(path.c_str(), "wb");
#endif
}

inline int g_checks = 0;
inline int g_failures = 0;

inline void Check(bool passed, const char* expression, int line)
{
    ++g_checks;
    if (!passed)
    {
        ++g_failures;
        std::printf("  FAILED line %d: %s\n", line, expression);
    }
}

#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

inline QueueEntry Entry(std::uint32_t appId, GoalKind kind = GoalKind::None, float hours = 1.0f, double idled = 0.0, std::uint32_t baseline = 0)
{
    QueueEntry entry;
    entry.AppId = appId;
    entry.Goal = { kind, hours };
    entry.IdledSeconds = idled;
    entry.BaselineMinutes = baseline;
    return entry;
}

inline std::filesystem::path FindErrorReference()
{
    std::error_code ignored;
    for (std::filesystem::path folder = std::filesystem::current_path(ignored); !folder.empty(); folder = folder.parent_path())
    {
        if (std::filesystem::exists(folder / "Source" / "Core" / "Errors.h", ignored))
            return folder / "Docs" / "ERRORS.md";
        if (folder == folder.parent_path())
            break;
    }
    return {};
}

namespace Tests
{
    void TestVdf();
    void TestPlaytimes();
    void TestAppInfo();
    void TestLibrary();
    void TestQueue();
    void TestSmoothWheel();
    void TestRefundGuard();
    void TestForecast();
    void TestHourWindow();
    void TestStats();
    void TestCardQueue();
    void TestCreatures();
    void TestTerrarium();
    void TestWebPolicy();
    void TestRuntimeLog();
    void TestBondsAndSets();
    void TestEncounters();
    void TestEvolutions();
    void TestCharacters();
    void TestSteamRegistry();
    void TestLocalTime();
    void TestVariedTimes();
    void TestShownGame();
    void TestCardPages();
    void TestMarkGamesWithCards();
    void TestCardSet();
    void TestCardLog();
    void TestSettings();
    void TestErrors();
    void TestJson();
    void TestSpriteFile();
    void TestSpriteEdit();
    void TestSpriteTimeline();
    void TestCreatureFiles();
    void TestFileLock();
    void TestText();
    void TestMiniGames();
    void TestFetch();
    void TestSnailShell();
    void TestClock();
    void TestParkaKidBurst();
    void TestCovers();
    void TestTug();
    void TestZip();
    void TestBackups();
    void TestCrashReport();
    void TestReleases();
    void TestVersion();
}
