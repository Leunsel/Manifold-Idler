#include "TestKit.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>

int main()
{
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
#endif
    const struct
    {
        const char* Name;
        void (*Run)();
    } tests[] = {
        { "Vdf", Tests::TestVdf },
        { "Playtimes", Tests::TestPlaytimes },
        { "AppInfo", Tests::TestAppInfo },
        { "Library", Tests::TestLibrary },
        { "Queue", Tests::TestQueue },
        { "CardQueue", Tests::TestCardQueue },
        { "Creatures", Tests::TestCreatures },
        { "Terrarium", Tests::TestTerrarium },
        { "Web policy", Tests::TestWebPolicy },
        { "RuntimeLog", Tests::TestRuntimeLog },
        { "VariedTimes", Tests::TestVariedTimes },
        { "SteamRegistry", Tests::TestSteamRegistry },
        { "BondsAndSets", Tests::TestBondsAndSets },
        { "Encounters", Tests::TestEncounters },
        { "Characters", Tests::TestCharacters },
        { "Evolutions", Tests::TestEvolutions },
        { "MiniGames", Tests::TestMiniGames },
        { "Fetch", Tests::TestFetch },
        { "SnailShell", Tests::TestSnailShell },
        { "ParkaKidBurst", Tests::TestParkaKidBurst },
        { "Clock", Tests::TestClock },
        { "Covers", Tests::TestCovers },
        { "Tug", Tests::TestTug },
        { "LocalTime", Tests::TestLocalTime },
        { "ShownGame", Tests::TestShownGame },
        { "CardPages", Tests::TestCardPages },
        { "MarkGamesWithCards", Tests::TestMarkGamesWithCards },
        { "CardSet", Tests::TestCardSet },
        { "CardLog", Tests::TestCardLog },
        { "SmoothWheel", Tests::TestSmoothWheel },
        { "RefundGuard", Tests::TestRefundGuard },
        { "Forecast", Tests::TestForecast },
        { "HourWindow", Tests::TestHourWindow },
        { "Stats", Tests::TestStats },
        { "Settings", Tests::TestSettings },
        { "Text", Tests::TestText },
        { "Errors", Tests::TestErrors },
        { "Json", Tests::TestJson },
        { "SpriteFile", Tests::TestSpriteFile },
        { "SpriteEdit", Tests::TestSpriteEdit },
        { "SpriteTimeline", Tests::TestSpriteTimeline },
        { "CreatureFiles", Tests::TestCreatureFiles },
        { "FileLock", Tests::TestFileLock },
        { "Zip", Tests::TestZip },
        { "Backups", Tests::TestBackups },
        { "CrashReport", Tests::TestCrashReport },
        { "Version", Tests::TestVersion },
        { "Releases", Tests::TestReleases },
    };
    for (const auto& test : tests)
    {
        std::printf("%s\n", test.Name);
        test.Run();
    }
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
