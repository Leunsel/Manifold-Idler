#include "Core/AppInfo.h"
#include "Core/Cards.h"
#include "Core/Clock.h"
#include "Core/Creatures.h"
#include "Core/Errors.h"
#include "Core/FileLock.h"
#include "Core/Json.h"
#include "Core/Library.h"
#include "Core/Queue.h"
#include "Core/RuntimeLog.h"
#include "Core/Settings.h"
#include "Core/Stats.h"
#include "Core/SmoothWheel.h"
#include "Core/SpriteEdit.h"
#include "Core/SpriteFile.h"
#include "Core/Terrarium.h"
#include "Core/WebPolicy.h"
#include "Core/Text.h"
#include "Core/Vdf.h"
#include "UI/CreatureSprites.h"
#include "UI/SpriteView.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    std::filesystem::path TestFile(const std::string& ending)
    {
        static const auto run = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("Manifold-Idler-Test-" + std::to_string(run) + ending);
    }

    FILE* OpenForWriting(const std::filesystem::path& path)
    {
#ifdef _WIN32
        FILE* file = nullptr;
        _wfopen_s(&file, path.c_str(), L"wb");
        return file;
#else
        return std::fopen(path.c_str(), "wb");
#endif
    }

    int g_checks = 0;
    int g_failures = 0;

    void Check(bool passed, const char* expression, int line)
    {
        ++g_checks;
        if (!passed)
        {
            ++g_failures;
            std::printf("  FAILED line %d: %s\n", line, expression);
        }
    }

#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

    using namespace Idler;

    void TestVdf()
    {
        Vdf::Node root;
        std::string error;
        CHECK(Vdf::Parse("// comment\n\"Root\"\n{\n\t\"key\"\t\t\"value\"\n\t\"Quoted\"\t\t\"a \\\"b\\\" \\\\ c\"\n\tBare 12\n\t\"Empty\" {}\n}\n", root, &error));
        const Vdf::Node* node = root.Find("root");
        CHECK(node != nullptr);
        if (node)
        {
            CHECK(node->Find("KEY") && node->Find("KEY")->Value == "value");
            CHECK(node->Find("quoted") && node->Find("quoted")->Value == "a \"b\" \\ c");
            CHECK(node->Find("bare") && node->Find("bare")->Value == "12");
            CHECK(node->Find("empty") && node->Find("empty")->Children.empty());
            CHECK(node->Find("missing") == nullptr);
        }

        for (const char* damaged : { "\"a\" {", "\"a\" \"b", "}", "\"a\"", "\"a\" { \"b\" } }" })
        {
            Vdf::Node ignored;
            CHECK(!Vdf::Parse(damaged, ignored));
        }

        std::string deep;
        for (int level = 0; level < 100000; ++level)
            deep += "a {";
        Vdf::Node ignored;
        CHECK(!Vdf::Parse(deep, ignored));
    }

    void TestPlaytimes()
    {
        const std::string text =
            "\"UserLocalConfigStore\"\n{\n\t\"Software\"\n\t{\n\t\t\"Valve\"\n\t\t{\n\t\t\t\"Steam\"\n\t\t\t{\n\t\t\t\t\"apps\"\n\t\t\t\t{\n"
            "\t\t\t\t\t\"10\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"Playtime\"\t\t\"125\"\n\t\t\t\t\t\t\"LastPlayed\"\t\t\"1700000000\"\n\t\t\t\t\t}\n"
            "\t\t\t\t\t\"20\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"LastPlayed\"\t\t\"5\"\n\t\t\t\t\t}\n"
            "\t\t\t\t\t\"30\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"cloud\"\t\t\"x\"\n\t\t\t\t\t}\n"
            "\t\t\t\t\t\"name\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"Playtime\"\t\t\"9\"\n\t\t\t\t\t}\n"
            "\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n";
        const PlaytimeMap playtimes = ParsePlaytimes(text);
        CHECK(playtimes.size() == 2);
        CHECK(playtimes.count(10) && playtimes.at(10).Minutes == 125 && playtimes.at(10).LastPlayed == 1700000000);
        CHECK(playtimes.count(20) && playtimes.at(20).Minutes == 0);
        CHECK(!playtimes.count(30));
        CHECK(ParsePlaytimes("\"damaged\" {").empty());
        CHECK(ParsePlaytimes("").empty());
    }

    class AppInfoBuilder
    {
    public:
        explicit AppInfoBuilder(std::uint32_t magic) : m_magic(magic) {}

        void Add(std::uint32_t appId, std::string_view name, std::string_view type, bool cards)
        {
            std::vector<std::uint8_t> kv;
            Object(kv, "appinfo");
            UInt32Field(kv, "appid", appId);
            Object(kv, "common");
            String(kv, "name", name);
            String(kv, "type", type);
            UInt32Field(kv, "review_percentage", 87);
            UInt32Field(kv, "metacritic_score", 91);
            UInt32Field(kv, "steam_release_date", 1500000000);
            Object(kv, "icons");
            String(kv, "name", "not the name");
            UInt64Field(kv, "big", 7);
            kv.push_back(8);
            if (cards)
            {
                Object(kv, "category");
                UInt32Field(kv, "category_2", 1);
                UInt32Field(kv, "category_29", 1);
                UInt32Field(kv, "category_22", 1);
                kv.push_back(8);
            }
            kv.push_back(8);
            Object(kv, "extended");
            String(kv, "type", "not the type");
            kv.push_back(8);
            kv.push_back(8);
            kv.push_back(8);

            std::vector<std::uint8_t> entry(HeaderSize(), 0);
            entry.insert(entry.end(), kv.begin(), kv.end());
            Put32(m_body, appId);
            Put32(m_body, static_cast<std::uint32_t>(entry.size()));
            m_body.insert(m_body.end(), entry.begin(), entry.end());
        }

        void AddDamaged(std::uint32_t appId)
        {
            std::vector<std::uint8_t> entry(HeaderSize(), 0);
            entry.push_back(99);
            entry.push_back(1);
            Put32(m_body, appId);
            Put32(m_body, static_cast<std::uint32_t>(entry.size()));
            m_body.insert(m_body.end(), entry.begin(), entry.end());
        }

        std::vector<std::uint8_t> Build() const
        {
            std::vector<std::uint8_t> file;
            Put32(file, m_magic);
            Put32(file, 1);
            const std::size_t bodyAt = file.size() + (m_magic == 0x07564429 ? 8u : 0u);
            const std::uint64_t tableAt = bodyAt + m_body.size() + 4;
            if (m_magic == 0x07564429)
            {
                Put32(file, static_cast<std::uint32_t>(tableAt));
                Put32(file, static_cast<std::uint32_t>(tableAt >> 32));
            }
            file.insert(file.end(), m_body.begin(), m_body.end());
            Put32(file, 0);
            if (m_magic == 0x07564429)
            {
                Put32(file, static_cast<std::uint32_t>(m_strings.size()));
                for (const std::string& text : m_strings)
                {
                    file.insert(file.end(), text.begin(), text.end());
                    file.push_back(0);
                }
            }
            return file;
        }

    private:
        std::size_t HeaderSize() const { return 4 + 4 + 8 + 20 + 4 + (m_magic >= 0x07564428 ? 20u : 0u); }

        static void Put32(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
                out.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        void Key(std::vector<std::uint8_t>& out, std::string_view key)
        {
            if (m_magic != 0x07564429)
            {
                out.insert(out.end(), key.begin(), key.end());
                out.push_back(0);
                return;
            }
            std::size_t index = 0;
            while (index < m_strings.size() && m_strings[index] != key)
                ++index;
            if (index == m_strings.size())
                m_strings.emplace_back(key);
            Put32(out, static_cast<std::uint32_t>(index));
        }

        void Object(std::vector<std::uint8_t>& out, std::string_view key)
        {
            out.push_back(0);
            Key(out, key);
        }

        void String(std::vector<std::uint8_t>& out, std::string_view key, std::string_view value)
        {
            out.push_back(1);
            Key(out, key);
            out.insert(out.end(), value.begin(), value.end());
            out.push_back(0);
        }

        void UInt32Field(std::vector<std::uint8_t>& out, std::string_view key, std::uint32_t value)
        {
            out.push_back(2);
            Key(out, key);
            Put32(out, value);
        }

        void UInt64Field(std::vector<std::uint8_t>& out, std::string_view key, std::uint32_t value)
        {
            out.push_back(7);
            Key(out, key);
            Put32(out, value);
            Put32(out, 0);
        }

        std::uint32_t m_magic;
        std::vector<std::uint8_t> m_body;
        std::vector<std::string> m_strings;
    };

    void TestAppInfo()
    {
        for (const std::uint32_t magic : { 0x07564427u, 0x07564428u, 0x07564429u })
        {
            AppInfoBuilder builder(magic);
            builder.Add(10, "Counter-Strike", "Game", true);
            builder.AddDamaged(15);
            builder.Add(20, "T\xC3\xA4st", "dlc", false);
            const auto file = builder.Build();

            const AppInfoResult result = ParseAppInfo(file);
            CHECK(result.Error.empty());
            CHECK(result.Damaged == 1);
            CHECK(result.Apps.size() == 2);
            if (result.Apps.size() == 2)
            {
                CHECK(result.Apps[0].AppId == 10);
                CHECK(result.Apps[0].Name == "Counter-Strike");
                CHECK(result.Apps[0].Type == "game");
                CHECK(result.Apps[0].HasCards);
                CHECK(result.Apps[0].HasAchievements);
                CHECK(!result.Apps[1].HasAchievements);
                CHECK(result.Apps[0].ReviewPercent == 87);
                CHECK(result.Apps[0].Metacritic == 91);
                CHECK(result.Apps[0].Released == 1500000000);
                CHECK(result.Apps[1].AppId == 20);
                CHECK(result.Apps[1].Name == "T\xC3\xA4st");
                CHECK(result.Apps[1].Type == "dlc");
                CHECK(!result.Apps[1].HasCards);
            }

            for (std::size_t size = 0; size < file.size(); ++size)
            {
                const AppInfoResult cut = ParseAppInfo(std::span(file).first(size));
                CHECK(!cut.Error.empty() || cut.Apps.size() <= 2);
            }
        }

        const std::uint8_t future[] = { 0x2A, 0x44, 0x56, 0x07, 1, 0, 0, 0, 0, 0, 0, 0 };
        const AppInfoResult result = ParseAppInfo(future);
        CHECK(result.Error.find("0756442A") != std::string::npos);
        CHECK(result.Apps.empty());

        AppInfoBuilder builder(0x07564429);
        builder.Add(10, "Name", "game", false);
        auto file = builder.Build();
        file[8] = 0xFF;
        file[9] = 0xFF;
        file[10] = 0xFF;
        file[11] = 0x7F;
        CHECK(!ParseAppInfo(file).Error.empty());
    }

    std::vector<Game> SampleLibrary()
    {
        const std::vector<AppInfoEntry> apps = {
            { 1, "game", "Game 10", true },
            { 2, "game", "Game 2", false },
            { 3, "application", "A Tool", false },
            { 4, "dlc", "Some DLC", true },
            { 5, "game", "Free Unplayed", false },
            { 6, "game", "Free Played", false },
            { 7, "game", "Free Installed", false },
            { 8, "game", "Not Owned", false },
            { 9, "game", "", false },
        };
        const std::vector<Ownership> owned = {
            { 1, 1000, false }, { 2, 2000, true }, { 3, 3000, false }, { 4, 1000, false }, { 5, 500, false }, { 6, 500, false }, { 7, 500, true }, { 9, 1000, false },
            { 99, 1000, false },
        };
        PlaytimeMap playtimes;
        playtimes[1] = { 600, 1700000000 };
        playtimes[6] = { 5, 1600000000 };
        return BuildLibrary(apps, owned, playtimes, 500);
    }

    void TestLibrary()
    {
        const std::vector<Game> games = SampleLibrary();
        std::vector<std::uint32_t> ids;
        for (const Game& game : games)
            ids.push_back(game.AppId);
        CHECK((ids == std::vector<std::uint32_t>{ 3, 7, 6, 2, 1 }));

        const auto game = [&](std::uint32_t appId) -> const Game&
            { return *std::find_if(games.begin(), games.end(), [&](const Game& g) { return g.AppId == appId; }); };
        CHECK(game(1).HasCards && game(1).PlaytimeMinutes == 600 && game(1).Purchased == 1000);
        CHECK(game(3).Software && !game(1).Software);
        CHECK(game(6).Purchased == 0);

        const auto view = [&](std::string_view search, LibraryFilter filter, LibrarySort sort)
            {
                std::vector<std::uint32_t> result;
                for (const int index : LibraryView(games, search, filter, sort))
                    result.push_back(games[static_cast<std::size_t>(index)].AppId);
                return result;
            };
        CHECK(view("", LibraryFilter::All, LibrarySort::Name).size() == 5);
        CHECK((view("gAmE", LibraryFilter::All, LibrarySort::Name) == std::vector<std::uint32_t>{ 2, 1 }));
        CHECK((view("  game  ", LibraryFilter::Cards, LibrarySort::Name) == std::vector<std::uint32_t>{ 1 }));
        CHECK((view("3", LibraryFilter::All, LibrarySort::Name) == std::vector<std::uint32_t>{ 3 }));
        CHECK((view("", LibraryFilter::Installed, LibrarySort::Name) == std::vector<std::uint32_t>{ 7, 2 }));
        CHECK((view("", LibraryFilter::Played, LibrarySort::Playtime) == std::vector<std::uint32_t>{ 1, 6 }));
        CHECK(view("", LibraryFilter::NotPlayed, LibrarySort::Name).size() == 3);
        CHECK(view("", LibraryFilter::All, LibrarySort::LastPlayed).front() == 1);
        CHECK(view("", LibraryFilter::All, LibrarySort::Purchased).front() == 3);
        CHECK(view("nothing like it", LibraryFilter::All, LibrarySort::Name).empty());

        std::vector<AppInfoEntry> rated = {
            { 1, "game", "One", false, false, 95, 90, 1000 },
            { 2, "game", "Two", false, false, 72, 0, 3000 },
            { 3, "game", "Three", false, false, 0, 60, 2000 },
            { 4, "game", "Four", false, false, 85, 75, 0 },
        };
        AchievementMap achievements;
        achievements[1] = { 5, 10 };
        achievements[2] = { 30, 30 };
        achievements[3] = { 1, 100 };
        const std::vector<Game> ratedGames = BuildLibrary(rated, { { 1, 1000, false }, { 2, 1000, false }, { 3, 1000, false }, { 4, 1000, false } }, {}, 0, achievements);
        const auto ratedView = [&](LibrarySort sort, std::uint32_t minReview)
            {
                std::vector<std::uint32_t> result;
                for (const int index : LibraryView(ratedGames, "", LibraryFilter::All, sort, minReview))
                    result.push_back(ratedGames[static_cast<std::size_t>(index)].AppId);
                return result;
            };
        CHECK((ratedView(LibrarySort::Reviews, 0) == std::vector<std::uint32_t>{ 1, 4, 2, 3 }));
        CHECK((ratedView(LibrarySort::Metacritic, 0) == std::vector<std::uint32_t>{ 1, 4, 3, 2 }));
        CHECK((ratedView(LibrarySort::Released, 0) == std::vector<std::uint32_t>{ 2, 3, 1, 4 }));
        CHECK((ratedView(LibrarySort::Achievements, 0) == std::vector<std::uint32_t>{ 2, 1, 3, 4 }));
        CHECK((ratedView(LibrarySort::Name, 80) == std::vector<std::uint32_t>{ 4, 1 }));
        CHECK(ratedView(LibrarySort::Name, 96).empty());

        const AchievementCount counts = ParseAchievementCounts(
            R"([["achievements",{"version":2,"data":{"vecHighlight":[],"nTotal":117,"nAchieved":111}}],["descriptions",{"data":"say \"nTotal\":5"}]])");
        CHECK(counts.Total == 117 && counts.Done == 111);
        const AchievementCount quoted = ParseAchievementCounts(R"([["descriptions",{"data":"say \"nTotal\":5, \"nAchieved\":3"}]])");
        CHECK(quoted.Total == 0 && quoted.Done == 0);
        CHECK(ParseAchievementCounts("").Total == 0);
        CHECK(ParseAchievementCounts(R"({"nTotal":)").Total == 0);

        const std::vector<AppInfoEntry> apps = { { 1, "game", "\xC3\x84RGER", false } };
        const std::vector<Game> german = BuildLibrary(apps, { { 1, 1000, false } }, {}, 0);
        CHECK(LibraryView(german, "\xC3\xA4rger", LibraryFilter::All, LibrarySort::Name).size() == 1);
    }

    QueueEntry Entry(std::uint32_t appId, GoalKind kind = GoalKind::None, float hours = 1.0f, double idled = 0.0, std::uint32_t baseline = 0)
    {
        QueueEntry entry;
        entry.AppId = appId;
        entry.Goal = { kind, hours };
        entry.IdledSeconds = idled;
        entry.BaselineMinutes = baseline;
        return entry;
    }

    void TestQueue()
    {
        CHECK(!IsFinished(Entry(1, GoalKind::None, 1.0f, 1e9)));
        CHECK(Progress(Entry(1)) < 0.0f);
        CHECK(SecondsLeft(Entry(1)) == 0.0);

        CHECK(!IsFinished(Entry(1, GoalKind::ForHours, 1.0f, 3599.0)));
        CHECK(IsFinished(Entry(1, GoalKind::ForHours, 1.0f, 3600.0)));
        CHECK(!IsFinished(Entry(1, GoalKind::ForHours, 1.0f, 0.0, 600)));
        CHECK(SecondsLeft(Entry(1, GoalKind::ForHours, 2.0f, 3600.0)) == 3600.0);
        CHECK(Progress(Entry(1, GoalKind::ForHours, 2.0f, 3600.0)) == 0.5f);

        CHECK(IsFinished(Entry(1, GoalKind::UntilTotal, 3.0f, 0.0, 180)));
        CHECK(!IsFinished(Entry(1, GoalKind::UntilTotal, 3.0f, 0.0, 179)));
        CHECK(IsFinished(Entry(1, GoalKind::UntilTotal, 3.0f, 60.0, 179)));
        CHECK(Progress(Entry(1, GoalKind::UntilTotal, 2.0f, 1800.0, 30)) == 0.5f);
        CHECK(Progress(Entry(1, GoalKind::ForHours, 0.0f)) == 1.0f);

        std::vector<QueueEntry> queue = { Entry(1, GoalKind::ForHours, 1.0f, 3600.0), Entry(2), Entry(3), Entry(4), Entry(5) };
        queue[2].Failed = true;
        CHECK((WantedRunning(queue, 1) == std::vector<std::uint32_t>{ 2 }));
        CHECK((WantedRunning(queue, 2) == std::vector<std::uint32_t>{ 2, 4 }));
        CHECK((WantedRunning(queue, 32) == std::vector<std::uint32_t>{ 2, 4, 5 }));
        CHECK((WantedRunning(queue, 0) == std::vector<std::uint32_t>{ 2 }));
        CHECK(WantedRunning({}, 4).empty());

        std::vector<QueueEntry> many;
        for (std::uint32_t appId = 1; appId <= 40; ++appId)
            many.push_back(Entry(appId));
        CHECK(WantedRunning(many, 1000).size() == kMaxSlots);

        const auto order = [](const std::vector<QueueEntry>& entries)
            {
                std::vector<std::uint32_t> ids;
                for (const QueueEntry& entry : entries)
                    ids.push_back(entry.AppId);
                return ids;
            };
        std::vector<QueueEntry> moved = { Entry(1), Entry(2), Entry(3), Entry(4) };
        MoveEntry(moved, 0, 2);
        CHECK((order(moved) == std::vector<std::uint32_t>{ 2, 3, 1, 4 }));
        MoveEntry(moved, 3, 0);
        CHECK((order(moved) == std::vector<std::uint32_t>{ 4, 2, 3, 1 }));
        MoveEntry(moved, 1, 1);
        MoveEntry(moved, -1, 2);
        MoveEntry(moved, 1, 4);
        CHECK((order(moved) == std::vector<std::uint32_t>{ 4, 2, 3, 1 }));

        CHECK(FindEntry(moved, 3) == &moved[2]);
        CHECK(FindEntry(moved, 9) == nullptr);

        const auto takeApp = [](std::vector<QueueEntry>& entries, std::uint32_t appId)
            {
                return TakeEntries(entries, [&](const QueueEntry& entry) { return entry.AppId == appId; });
            };
        const auto add = [](std::vector<RemovedEntry>& all, std::vector<RemovedEntry> more)
            {
                all.insert(all.end(), more.begin(), more.end());
            };
        const std::vector<std::uint32_t> whole = { 1, 2, 3, 4, 5 };
        std::vector<QueueEntry> taking = { Entry(1), Entry(2), Entry(3), Entry(4), Entry(5) };
        taking[1].IdledSeconds = 120.0;
        taking[1].GoalFactor = 1.25f;
        taking[1].Resting = true;

        std::vector<RemovedEntry> removed = TakeEntries(taking, [](const QueueEntry& entry) { return entry.AppId % 2 == 0; });
        CHECK((order(taking) == std::vector<std::uint32_t>{ 1, 3, 5 }));
        CHECK(removed.size() == 2);
        PutBackEntries(taking, removed);
        CHECK(order(taking) == whole);
        CHECK(taking[1].IdledSeconds == 120.0);
        CHECK(taking[1].GoalFactor == 1.25f);
        CHECK(taking[1].Resting);

        removed = takeApp(taking, 2);
        add(removed, takeApp(taking, 5));
        add(removed, takeApp(taking, 1));
        CHECK((order(taking) == std::vector<std::uint32_t>{ 3, 4 }));
        add(removed, TakeEntries(taking, [](const QueueEntry&) { return true; }));
        CHECK(taking.empty());
        PutBackEntries(taking, removed);
        CHECK(order(taking) == whole);

        CHECK(takeApp(taking, 99).empty());
        CHECK(order(taking) == whole);

        removed = takeApp(taking, 2);
        taking.push_back(Entry(2));
        PutBackEntries(taking, removed);
        CHECK(order(taking) == whole);
        CHECK(taking[1].IdledSeconds == 120.0);

        removed = takeApp(taking, 5);
        taking.resize(1);
        PutBackEntries(taking, removed);
        CHECK((order(taking) == std::vector<std::uint32_t>{ 1, 5 }));

        CHECK(PhaseOfQueue(false, false, false, 0) == QueuePhase::Stopped);
        CHECK(PhaseOfQueue(false, false, false, 3) == QueuePhase::Stopping);
        CHECK(PhaseOfQueue(true, false, true, 0) == QueuePhase::Starting);
        CHECK(PhaseOfQueue(true, false, false, 3) == QueuePhase::Running);
        CHECK(PhaseOfQueue(true, true, true, 3) == QueuePhase::Waiting);

        const auto foldedName = [](std::uint32_t appId) -> std::string
            {
                switch (appId)
                {
                case 1: return "zeta";
                case 2: return "alpha";
                case 4: return "beta";
                case 5: return "alpha";
                default: return {};
                }
            };
        const std::vector<QueueEntry> unsorted = { Entry(1, GoalKind::ForHours, 5.0f), Entry(2), Entry(3, GoalKind::ForHours, 1.0f, 3600.0), Entry(4, GoalKind::ForHours, 2.0f), Entry(5, GoalKind::None, 1.0f, 0.0, 600) };
        std::vector<QueueEntry> sorting = unsorted;
        SortQueue(sorting, QueueOrder::TimeLeft, foldedName);
        CHECK((order(sorting) == std::vector<std::uint32_t>{ 4, 1, 2, 5, 3 }));
        sorting = unsorted;
        SortQueue(sorting, QueueOrder::Name, foldedName);
        CHECK((order(sorting) == std::vector<std::uint32_t>{ 2, 5, 4, 1, 3 }));
        sorting = unsorted;
        SortQueue(sorting, QueueOrder::LeastPlayed, foldedName);
        CHECK((order(sorting) == std::vector<std::uint32_t>{ 1, 2, 4, 3, 5 }));
        sorting = unsorted;
        SortQueue(sorting, QueueOrder::MostPlayed, foldedName);
        CHECK((order(sorting) == std::vector<std::uint32_t>{ 5, 3, 1, 2, 4 }));

        const std::vector<std::uint32_t> before = OrderOf(unsorted);
        CHECK((before == std::vector<std::uint32_t>{ 1, 2, 3, 4, 5 }));
        RestoreOrder(sorting, before);
        CHECK(order(sorting) == before);
        CHECK(FindEntry(sorting, 3)->IdledSeconds == 3600.0);
        SortQueue(sorting, QueueOrder::MostPlayed, foldedName);
        sorting.push_back(Entry(6));
        std::erase_if(sorting, [](const QueueEntry& entry) { return entry.AppId == 2; });
        RestoreOrder(sorting, before);
        CHECK((order(sorting) == std::vector<std::uint32_t>{ 1, 3, 4, 5, 6 }));

        const std::vector<std::uint32_t> shownGames = { 10, 20, 30, 40, 50 };
        std::unordered_set<std::uint32_t> picked;
        std::uint32_t currentGame = 0;
        std::uint32_t anchorGame = 0;
        const auto click = [&](std::uint32_t appId, bool ctrl, bool shift, const std::vector<std::uint32_t>& shown)
            {
                ClickSelection(picked, currentGame, anchorGame, shown, appId, ctrl, shift);
            };
        using Picked = std::unordered_set<std::uint32_t>;
        click(20, false, false, shownGames);
        CHECK(picked == Picked({ 20 }) && currentGame == 20 && anchorGame == 20);
        click(40, true, false, shownGames);
        CHECK(picked == Picked({ 20, 40 }) && currentGame == 40 && anchorGame == 40);
        click(40, true, false, shownGames);
        CHECK(picked == Picked({ 20 }) && currentGame == 20);
        click(20, true, false, shownGames);
        CHECK(picked.empty() && currentGame == 0);
        click(20, false, false, shownGames);
        click(40, false, true, shownGames);
        CHECK(picked == Picked({ 20, 30, 40 }) && currentGame == 40 && anchorGame == 20);
        click(10, false, true, shownGames);
        CHECK(picked == Picked({ 10, 20 }) && currentGame == 10 && anchorGame == 20);
        click(40, false, true, { 30, 40 });
        CHECK(picked == Picked({ 40 }) && currentGame == 40 && anchorGame == 40);
        click(30, true, false, shownGames);
        click(30, true, false, shownGames);
        click(50, false, true, shownGames);
        CHECK(picked == Picked({ 50 }) && currentGame == 50);
        click(20, false, false, shownGames);
        click(50, true, false, shownGames);
        click(40, false, true, shownGames);
        CHECK(picked == Picked({ 40, 50 }) && anchorGame == 50);

        std::string schema;
        const auto kvText = [&](std::string_view key, std::string_view value)
            {
                schema += '\x01';
                schema += key;
                schema += '\0';
                schema += value;
                schema += '\0';
            };
        const auto kvOpen = [&](std::string_view key)
            {
                schema += '\0';
                schema += key;
                schema += '\0';
            };
        const auto kvNumber = [&](std::string_view key)
            {
                schema += '\x02';
                schema += key;
                schema += '\0';
                schema.append(4, '\x07');
            };
        kvOpen("620");
        kvText("gamename", "Portal 2");
        kvOpen("stats");
        kvOpen("1");
        kvNumber("type");
        kvOpen("bits");
        for (const char* id : { "ACH_ONE", "ACH_TWO" })
        {
            kvOpen("0");
            kvText("name", id);
            kvOpen("display");
            kvText("hidden", "0");
            kvText("icon", std::string(id) + ".jpg");
            kvText("icon_gray", std::string(id) + "_BW.jpg");
            schema += '\x08';
            schema += '\x08';
        }
        schema += "\x08\x08\x08\x08";
        std::vector<AchievementIcons> achievementIcons = ParseAchievementIcons(schema);
        CHECK(achievementIcons.size() == 2);
        CHECK(achievementIcons[0].Id == "ACH_ONE" && achievementIcons[0].Icon == "ACH_ONE.jpg" && achievementIcons[0].IconLocked == "ACH_ONE_BW.jpg");
        CHECK(achievementIcons[1].Id == "ACH_TWO");
        for (std::size_t length = 0; length < schema.size(); ++length)
            ParseAchievementIcons(std::string_view(schema).substr(0, length));
        CHECK(ParseAchievementIcons("\x63x").empty());
        CHECK(ParseAchievementIcons("").empty());
        CHECK(achievementIcons[0].Permission == 0 && MaySetAchievement("ACH_ONE", achievementIcons) && MaySetAchievement("ACH_TWO", achievementIcons));
        CHECK(!MaySetAchievement("ACH_THREE", achievementIcons));
        CHECK(!MaySetAchievement("", achievementIcons));

        schema.clear();
        const auto kvInt = [&](std::string_view key, std::uint32_t value)
            {
                schema += '\x02';
                schema += key;
                schema += '\0';
                for (int shift = 0; shift < 32; shift += 8)
                    schema += static_cast<char>((value >> shift) & 0xFF);
            };
        kvOpen("326460");
        kvOpen("stats");
        kvOpen("1");
        kvText("type", "ACHIEVEMENTS");
        kvOpen("bits");
        const std::pair<const char*, int> kPermissions[] = { { "FREE", 0 }, { "GAME_SERVER", 1 }, { "OFFICIAL_SERVER", 2 }, { "BOTH_BITS", 3 }, { "HIGH_BIT", 4 }, { "WORD", -1 } };
        for (const auto& [id, permission] : kPermissions)
        {
            kvOpen("0");
            kvText("name", id);
            if (permission >= 0)
                kvInt("permission", static_cast<std::uint32_t>(permission));
            else
                kvText("permission", "client");
            kvOpen("display");
            kvText("icon", std::string(id) + ".jpg");
            schema += '\x08';
            schema += '\x08';
        }
        schema += "\x08\x08\x08\x08";
        const std::vector<AchievementIcons> permissions = ParseAchievementIcons(schema);
        CHECK(permissions.size() == 6);
        CHECK(permissions.size() == 6 && permissions[1].Permission == 1 && permissions[2].Permission == 2);
        CHECK(MaySetAchievement("FREE", permissions));
        CHECK(!MaySetAchievement("GAME_SERVER", permissions));
        CHECK(!MaySetAchievement("OFFICIAL_SERVER", permissions));
        CHECK(!MaySetAchievement("BOTH_BITS", permissions));
        CHECK(MaySetAchievement("HIGH_BIT", permissions));
        CHECK(!MaySetAchievement("WORD", permissions));

        CHECK(UnlockRuleOf(permissions) == UnlockRule::Mixed);
        CHECK(UnlockRuleOf(achievementIcons) == UnlockRule::Here);
        CHECK(UnlockRuleOf({}) == UnlockRule::Unknown);
        CHECK(UnlockRuleOf({ permissions[1], permissions[2], permissions[5] }) == UnlockRule::Server);
        CHECK(UnlockRuleOf({ permissions[0], permissions[4] }) == UnlockRule::Here);

        const std::string changeText = WriteAchievementChanges({ { "ACH_A", true }, { "ACH B", false }, { "", true }, { "BAD\nID", true } });
        CHECK(changeText == "unlock ACH_A\nlock ACH B\n");
        const std::vector<AchievementChange> changes = ParseAchievementChanges(changeText + "unlock \nopen X\r\nlock ACH_C\r\nunlock ACH_D");
        CHECK(changes.size() == 4);
        CHECK(changes.size() == 4 && changes[0].Id == "ACH_A" && changes[0].Unlock && changes[1].Id == "ACH B" && !changes[1].Unlock &&
            changes[2].Id == "ACH_C" && !changes[2].Unlock && changes[3].Id == "ACH_D" && changes[3].Unlock);
        CHECK(ParseAchievementChanges("").empty());
        CHECK(ParseAchievementChanges("lock").empty());

        CHECK(AchievementChangeText(0, 0, 0).empty());
        CHECK(AchievementChangeText(3, 0, 0) == "Unlocked 3.");
        CHECK(AchievementChangeText(0, 1, 0) == "Locked 1.");
        CHECK(AchievementChangeText(2, 0, 1) == "Unlocked 2, Steam refused 1.");
        CHECK(AchievementChangeText(0, 0, 4) == "Steam refused 4.");
        CHECK(AchievementChangeText(1, 2, 3) == "Unlocked 1, locked 2, Steam refused 3.");

        schema.clear();
        const auto kvFloat = [&](std::string_view key, float value)
            {
                schema += '\x03';
                schema += key;
                schema += '\0';
                std::uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                for (int shift = 0; shift < 32; shift += 8)
                    schema += static_cast<char>((bits >> shift) & 0xFF);
            };
        kvOpen("480");
        kvText("gamename", "Spacewar");
        kvOpen("stats");
        kvOpen("1");
        kvInt("type", 1);
        kvText("name", "NumGames");
        kvOpen("display");
        kvText("name", "Games played");
        schema += '\x08';
        kvInt("min", 0);
        kvInt("max", 1000);
        kvText("incrementonly", "1");
        schema += '\x08';
        kvOpen("2");
        kvText("type", "FLOAT");
        kvText("name", "FeetTraveled");
        kvOpen("display");
        kvOpen("name");
        kvText("german", "Gelaufen");
        kvText("english", "Feet traveled");
        schema += "\x08\x08";
        kvFloat("maxchange", 0.1f);
        kvInt("min", static_cast<std::uint32_t>(-50));
        schema += '\x08';
        kvOpen("3");
        kvInt("type_int", 3);
        kvText("name", "AverageSpeed");
        schema += '\x08';
        kvOpen("4");
        kvInt("type", 1);
        kvText("name", "ServerKills");
        kvInt("permission", 2);
        kvInt("min", 10);
        kvInt("max", 10);
        kvInt("maxchange", 0);
        schema += '\x08';
        kvOpen("5");
        kvInt("type", 1);
        kvText("name", "OddPermission");
        kvText("permission", "client");
        schema += '\x08';
        kvOpen("6");
        kvInt("type", 4);
        kvOpen("bits");
        kvOpen("0");
        kvText("name", "ACH_WIN");
        kvOpen("display");
        kvText("icon", "win.jpg");
        schema += '\x08';
        kvOpen("progress");
        kvOpen("value");
        kvText("operation", "statvalue");
        kvText("operand1", "NumGames");
        schema += '\x08';
        kvInt("min_val", 0);
        kvInt("max_val", 100);
        schema += "\x08\x08\x08\x08";
        kvOpen("7");
        kvInt("type", 1);
        schema += '\x08';
        schema += "\x08\x08\x08\x08";
        const std::vector<StatSchema> statSchema = ParseStatSchema(schema);
        CHECK(statSchema.size() == 5);
        if (statSchema.size() == 5)
        {
            const StatSchema& games = statSchema[0];
            CHECK(games.Name == "NumGames" && games.Label == "Games played" && games.Kind == StatKind::Int && games.Permission == 0 && games.IncrementOnly);
            CHECK(games.Min == 0.0 && games.Max == 1000.0 && !games.MaxChange);
            const StatSchema& feet = statSchema[1];
            CHECK(feet.Label == "Feet traveled" && feet.Kind == StatKind::Float && !feet.IncrementOnly);
            CHECK(feet.Min == -50.0 && !feet.Max && feet.MaxChange && std::fabs(*feet.MaxChange - 0.1) < 1e-6);
            CHECK(statSchema[2].Kind == StatKind::AverageRate && statSchema[2].Label.empty());
            CHECK(statSchema[3].Permission == 2 && !statSchema[3].Min && !statSchema[3].Max && !statSchema[3].MaxChange);
            CHECK(statSchema[4].Permission == 3);
            CHECK(MaySetStat(games) && MaySetStat(feet) && !MaySetStat(statSchema[2]) && !MaySetStat(statSchema[3]) && !MaySetStat(statSchema[4]));
            CHECK(FindStat(statSchema, "FeetTraveled") == &statSchema[1] && !FindStat(statSchema, "feettraveled") && !FindStat(statSchema, ""));

            CHECK(StatChangeProblem(games, 5, 6).empty());
            CHECK(StatChangeProblem(games, 5, 1000).empty());
            CHECK(StatChangeProblem(games, 5, 5).empty());
            CHECK(StatChangeProblem(games, 5, 4) == "It only counts up, so not below 5.");
            CHECK(StatChangeProblem(games, 5, 1001) == "Not above 1000.");
            CHECK(StatChangeProblem(games, 5, 6.5) == "Only whole numbers.");
            CHECK(StatChangeProblem(games, 5, std::numeric_limits<double>::infinity()) == "That is no number.");
            CHECK(StatChangeProblem(statSchema[4], 0, 3e9) == "Only the game's own server can set this one.");
            StatSchema wide = games;
            wide.Max.reset();
            CHECK(StatChangeProblem(wide, 5, 3e9) == "That is too large for it.");
            CHECK(StatChangeProblem(feet, 1.0, 1.05).empty());
            CHECK(StatChangeProblem(feet, 1.0, 0.95).empty());
            CHECK(StatChangeProblem(feet, 1.0, 1.5) == "At most 0.1 away from 1 at a time.");
            CHECK(StatChangeProblem(feet, 1.0, -60) == "Not below -50.");
            CHECK(StatChangeProblem(statSchema[2], 1.0, 2.0) == "The game works this one out by itself.");
        }
        for (std::size_t length = 0; length < schema.size(); ++length)
            ParseStatSchema(std::string_view(schema).substr(0, length));
        CHECK(ParseStatSchema("").empty());

        const std::vector<AchievementIcons> progressIcons = ParseAchievementIcons(schema);
        CHECK(progressIcons.size() == 1 && progressIcons[0].ProgressStat == "NumGames" && progressIcons[0].ProgressMin == 0.0 && progressIcons[0].ProgressMax == 100.0);
        CHECK(achievementIcons[0].ProgressStat.empty());

        CHECK(ParseNumber("12") == 12.0 && ParseNumber(" -3.5 ") == -3.5 && ParseNumber("1e3") == 1000.0);
        CHECK(!ParseNumber("") && !ParseNumber("12a") && !ParseNumber("1,5") && !ParseNumber("abc") && !ParseNumber("inf") && !ParseNumber("1 2"));
        CHECK(NumberText(12.0, StatKind::Int) == "12" && NumberText(-7.4, StatKind::Int) == "-7" && NumberText(2147483647.0, StatKind::Int) == "2147483647");
        CHECK(NumberText(0.1f, StatKind::Float) == "0.1" && NumberText(1.5, StatKind::AverageRate) == "1.5" && NumberText(16777216.0, StatKind::Float) == "16777216");
        CHECK(NumberText(0.0, StatKind::Float) == "0");

        AchievementChange setGames;
        setGames.Id = "NumGames";
        setGames.SetsStat = true;
        setGames.Value = 2147483647.0;
        AchievementChange setFeet;
        setFeet.Id = "FeetTraveled";
        setFeet.SetsStat = true;
        setFeet.Value = -0.25;
        const std::string statChangeText = WriteAchievementChanges({ setGames, { "ACH_WIN", true }, setFeet });
        CHECK(statChangeText == "set 2147483647 NumGames\nunlock ACH_WIN\nset -0.25 FeetTraveled\n");
        const std::vector<AchievementChange> statChanges = ParseAchievementChanges(statChangeText + "set x Bad\nset 5\nset 1e2 Hundred\n");
        CHECK(statChanges.size() == 4);
        CHECK(statChanges.size() == 4 && statChanges[0].SetsStat && statChanges[0].Id == "NumGames" && statChanges[0].Value == 2147483647.0 &&
            !statChanges[1].SetsStat && statChanges[1].Unlock && statChanges[2].SetsStat && statChanges[2].Value == -0.25 &&
            statChanges[3].Id == "Hundred" && statChanges[3].Value == 100.0);

        constexpr std::string_view kIconAddress = "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/620/ACH.jpg";
        CHECK(AchievementIconAddress(620, "ACH.jpg") == kIconAddress);
        CHECK(IsAchievementIconAddress(kIconAddress));
        for (const char* wrong : { "", "..", "a.exe", "..\\x.jpg", "../x.jpg", "a/b.jpg", "a b.jpg", "x" })
            CHECK(AchievementIconAddress(620, wrong).empty());
        CHECK(AchievementIconAddress(0, "ACH.jpg").empty());
        for (const char* wrong : { "", "https://evil.example/ACH.jpg", "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/x/ACH.jpg",
            "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/620/../ACH.jpg", "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/620/a.exe" })
            CHECK(!IsAchievementIconAddress(wrong));

        constexpr std::string_view kCollections = R"([["GameReleased",{"key":"GameReleased","timestamp":1,"value":"{\"name\":\"Not one\",\"added\":[1]}"}],
["user-collections.uc-a",{"key":"user-collections.uc-a","timestamp":2,"is_deleted":true,"version":"3"}],
["ea-thing",{"key":"ea-thing","timestamp":3,"value":"{\"name\":\"Not one either\",\"added\":[2]}"}],
["user-collections.uc-b",{"key":"user-collections.uc-b","timestamp":4,"value":"{\"id\":\"uc-b\",\"name\":\"Zombies \\u00e4 \\\"x\\\"\",\"added\":[730,620],\"removed\":[]}","version":"5"}],
["user-collections.favorite",{"key":"user-collections.favorite","timestamp":6,"value":"{\"id\":\"favorite\",\"name\":\"Favoriten\",\"added\":[10,20,30],\"removed\":[20]}","version":"7","strMethodId":"x{y"}],
["user-collections.partner-ea-access",{"key":"user-collections.partner-ea-access","timestamp":8,"value":"{\"id\":\"partner-ea-access\",\"name\":\"EA\"}"}],
["user-collections.uc-c",{"key":"user-collections.uc-c","timestamp":9,"value":"{\"id\":\"uc-c\",\"name\":\"By a filter\",\"added\":[],\"removed\":[],\"filterSpec\":{\"nFormatVersion\":2}}"}],
["whatsnew",{"key":"whatsnew","timestamp":10,"value":"{}"}]])";
        const std::vector<SteamCollection> collections = ParseSteamCollections(kCollections);
        CHECK(collections.size() == 2);
        CHECK(collections[0].Name == "Favoriten");
        CHECK((collections[0].Apps == std::vector<std::uint32_t>{ 10, 30 }));
        constexpr std::string_view kZombies = "Zombies \xC3\xA4 \"x\"";
        CHECK(collections[1].Name == kZombies);
        CHECK((collections[1].Apps == std::vector<std::uint32_t>{ 730, 620 }));
        constexpr std::string_view kCutOff = R"([["user-collections.x",{"key":"user-collections.x","value":"{\"name)";
        CHECK(ParseSteamCollections(kCutOff).empty());
        CHECK(ParseSteamCollections("").empty());
        CHECK(ParseSteamCollections("[]").empty());

        CHECK(MatchesSearch("counter-strike 2", 730, "strike"));
        CHECK(MatchesSearch("counter-strike 2", 730, "730"));
        CHECK(!MatchesSearch("counter-strike 2", 730, "73"));
        CHECK(!MatchesSearch("portal", 620, "strike"));
    }

    void TestSmoothWheel()
    {
        const auto drain = [](SmoothWheel& wheel, float frame, int& frames)
            {
                float total = 0.0f;
                frames = 0;
                while (wheel.Busy() && frames < 1000)
                {
                    total += wheel.Take(frame, true);
                    ++frames;
                }
                return total;
            };
        const auto same = [](float lhs, float rhs) { return std::fabs(lhs - rhs) < 0.0001f; };

        for (const float frame : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f, 1.0f / 240.0f })
        {
            SmoothWheel wheel;
            wheel.Add(1.0f);
            const float first = wheel.Take(frame, true);
            CHECK(first > 0.0f && first < 1.0f);
            int frames = 0;
            CHECK(same(first + drain(wheel, frame, frames), 1.0f));
            CHECK(frames >= 1);
            CHECK(static_cast<float>(frames + 1) * frame < 0.3f);
        }

        SmoothWheel steps;
        steps.Add(-3.0f);
        float previous = 100.0f;
        float moved = 0.0f;
        while (steps.Busy())
        {
            const float step = steps.Take(1.0f / 60.0f, true);
            CHECK(step < 0.0f);
            CHECK(std::fabs(step) <= previous + 0.0001f);
            previous = std::fabs(step);
            moved += step;
        }
        CHECK(same(moved, -3.0f));

        SmoothWheel adding;
        adding.Add(1.0f);
        float sum = adding.Take(1.0f / 60.0f, true);
        adding.Add(1.0f);
        int frames = 0;
        CHECK(same(sum + drain(adding, 1.0f / 60.0f, frames), 2.0f));

        SmoothWheel turning;
        turning.Add(3.0f);
        turning.Take(1.0f / 60.0f, true);
        turning.Add(-1.0f);
        CHECK(same(drain(turning, 1.0f / 60.0f, frames), -1.0f));

        SmoothWheel touchpad;
        touchpad.Add(0.25f);
        CHECK(same(touchpad.Take(1.0f / 60.0f, true), 0.25f));
        CHECK(!touchpad.Busy());

        SmoothWheel plain;
        plain.Add(2.0f);
        CHECK(same(plain.Take(1.0f / 60.0f, false), 2.0f));
        CHECK(!plain.Busy());

        SmoothWheel paused;
        paused.Add(1.0f);
        CHECK(paused.Take(5.0f, true) < 1.0f);

        SmoothWheel idle;
        CHECK(!idle.Busy() && idle.Take(1.0f / 60.0f, true) == 0.0f);
    }

    void TestRefundGuard()
    {
        const std::uint32_t bought = 1000000;
        const std::uint32_t day = 24 * 3600;

        CHECK(IsRefundable(bought, 0.0, bought + day));
        CHECK(RefundGuardLeft(bought, 0.0, bought + day) == kRefundPlaySeconds - kRefundMarginSeconds);
        CHECK(RefundGuardLeft(bought, 3600.0, bought + day) == 3000.0);
        CHECK(IsRefundable(bought, 7000.0, bought + day));
        CHECK(RefundGuardLeft(bought, 7000.0, bought + day) == 0.0);
        CHECK(!IsRefundable(bought, kRefundPlaySeconds, bought + day));
        CHECK(RefundGuardLeft(bought, kRefundPlaySeconds, bought + day) < 0.0);
        CHECK(!IsRefundable(bought, 0.0, bought + 14 * day));
        CHECK(RefundGuardLeft(bought, 0.0, bought + 14 * day) < 0.0);
        CHECK(IsRefundable(bought, 0.0, bought + 14 * day - 1));
        CHECK(!IsRefundable(0, 0.0, bought));
        CHECK(!IsRefundable(bought, 0.0, bought - 1));

        std::vector<QueueEntry> queue = { Entry(1), Entry(2) };
        queue[0].GuardSecondsLeft = 0.0;
        CHECK(IsHeld(queue[0]) && !IsHeld(queue[1]));
        CHECK((WantedRunning(queue, 2) == std::vector<std::uint32_t>{ 2 }));
        queue[0].GuardSecondsLeft = -1.0;
        CHECK((WantedRunning(queue, 2) == std::vector<std::uint32_t>{ 1, 2 }));
        queue[0].GuardSecondsLeft = 60.0;
        CHECK((WantedRunning(queue, 2) == std::vector<std::uint32_t>{ 1, 2 }));
    }

    void TestForecast()
    {
        CHECK(ForecastQueue({}, 4).Seconds == 0.0 && !ForecastQueue({}, 4).Endless);

        const std::vector<QueueEntry> three = { Entry(1, GoalKind::ForHours, 1.0f), Entry(2, GoalKind::ForHours, 2.0f), Entry(3, GoalKind::ForHours, 1.0f) };
        CHECK(ForecastQueue(three, 1).Seconds == 4.0 * 3600.0);
        CHECK(ForecastQueue(three, 2).Seconds == 2.0 * 3600.0);
        CHECK(ForecastQueue(three, 3).Seconds == 2.0 * 3600.0);
        CHECK(!ForecastQueue(three, 1).Endless);

        std::vector<QueueEntry> mixed = { Entry(1, GoalKind::ForHours, 1.0f, 3600.0), Entry(2, GoalKind::ForHours, 1.0f, 1800.0), Entry(3, GoalKind::ForHours, 5.0f), Entry(4, GoalKind::ForHours, 5.0f) };
        mixed[2].Failed = true;
        mixed[3].GuardSecondsLeft = 0.0;
        CHECK(ForecastQueue(mixed, 1).Seconds == 1800.0);

        std::vector<QueueEntry> endless = { Entry(1), Entry(2, GoalKind::ForHours, 1.0f) };
        CHECK(ForecastQueue(endless, 2).Endless);
        CHECK(ForecastQueue(endless, 2).Seconds == 3600.0);
        CHECK(ForecastQueue(endless, 1).Endless);
        endless[0].GuardSecondsLeft = 600.0;
        CHECK(!ForecastQueue(endless, 1).Endless);
        CHECK(ForecastQueue(endless, 1).Seconds == 4200.0);

        std::vector<QueueEntry> guarded = { Entry(1, GoalKind::ForHours, 5.0f) };
        guarded[0].GuardSecondsLeft = 60.0;
        CHECK(ForecastQueue(guarded, 1).Seconds == 60.0);
    }

    void TestHourWindow()
    {
        CHECK(InHourWindow(8, 17, 8));
        CHECK(InHourWindow(8, 17, 16));
        CHECK(!InHourWindow(8, 17, 17));
        CHECK(!InHourWindow(8, 17, 7));
        CHECK(InHourWindow(22, 8, 22));
        CHECK(InHourWindow(22, 8, 0));
        CHECK(InHourWindow(22, 8, 7));
        CHECK(!InHourWindow(22, 8, 8));
        CHECK(!InHourWindow(22, 8, 21));
        for (int hour = 0; hour < 24; ++hour)
            CHECK(InHourWindow(5, 5, hour));
    }

    void TestStats()
    {
        CHECK(DayNumber(1970, 1, 1) == 0);
        CHECK(DayNumber(2000, 3, 1) - DayNumber(2000, 2, 28) == 2);
        CHECK(DayNumber(1900, 3, 1) - DayNumber(1900, 2, 28) == 1);
        CHECK(DayText(0) == "1970-01-01");
        for (const char* date : { "2026-11-02", "2024-02-29", "2000-12-31", "1999-01-01" })
        {
            const auto day = ParseDay(date);
            CHECK(day.has_value() && DayText(*day) == date);
        }
        for (const char* wrong : { "", "2026-11", "2026-13-01", "2026-00-10", "2026-11-32", "20261102xx", "abcd-ef-gh" })
            CHECK(!ParseDay(wrong).has_value());
        CHECK(TodayNumber() > DayNumber(2020, 1, 1));

        const std::int32_t today = *ParseDay("2026-11-02");
        IdleStats stats;
        stats.Add(7, today, 10, 3600.0);
        stats.Add(7, today, 10, 1800.0);
        stats.Add(7, today, 20, 600.0);
        stats.Add(7, today - 1, 20, 7200.0);
        stats.Add(7, today - 40, 30, 60.0);
        stats.Add(8, today, 99, 5000.0);
        stats.Add(7, today, 50, 0.0);

        const std::vector<double> days = stats.PerDay(7, today, 3);
        CHECK((days == std::vector<double>{ 0.0, 7200.0, 6000.0 }));
        CHECK(stats.PerDay(7, today, 0).empty());
        CHECK(stats.PerDay(9, today, 2) == (std::vector<double>{ 0.0, 0.0 }));
        CHECK(stats.Total(7) == 13260.0);
        CHECK(stats.Total(7, today - 1, today) == 13200.0);
        CHECK(stats.Total(7, today, today) == 6000.0);
        CHECK(stats.Total(9) == 0.0);

        const auto games = stats.ByGame(7);
        CHECK(games.size() == 3);
        CHECK(games[0].AppId == 20 && games[0].Seconds == 7800.0);
        CHECK(games[1].AppId == 10 && games[1].Seconds == 5400.0);
        CHECK(games[2].AppId == 30);
        const auto ofToday = stats.ByGame(7, today);
        CHECK(ofToday.size() == 2 && ofToday[0].AppId == 10);

        const auto history = stats.History(7);
        CHECK(history.size() == 4);
        CHECK(history[0].Day == today && history[0].AppId == 10);
        CHECK(history[1].Day == today && history[1].AppId == 20);
        CHECK(history[2].Day == today - 1);
        CHECK(history[3].Day == today - 40);
        CHECK(stats.History(7, today - 1).size() == 1);
        CHECK(stats.History(7, today - 2).empty());

        const std::filesystem::path path = TestFile(".stats");
        CHECK(stats.Save(path));
        IdleStats loaded;
        loaded.Load(path);
        CHECK(loaded.Total(7) == 13260.0 && loaded.Total(8) == 5000.0);
        CHECK(loaded.PerDay(7, today, 3) == days);

        {
            FILE* file = OpenForWriting(path);
            CHECK(file != nullptr);
            if (file)
            {
                std::fputs("; comment\r\nnonsense\r\n2026-11-02 | 7 | 10\r\nyesterday | 7 | 10 | 60\r\n2026-11-02 | 7 | x | 60\r\n2026-11-02 | 7 | 10 | 60\r\n", file);
                std::fclose(file);
            }
        }
        IdleStats damaged;
        damaged.Load(path);
        CHECK(damaged.Total(7) == 60.0);
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        IdleStats missing;
        missing.Load(path);
        CHECK(missing.Total(7) == 0.0);

        CHECK(WeekdayOf(today) == 0);
        CHECK(WeekdayOf(today + 6) == 6);
        CHECK(WeekdayOf(0) == 3);
        CHECK(WeekdayOf(-1) == 2);

        IdleStats played;
        played.AddSession(7, today - 4, 10);
        played.Add(7, today - 4, 10, 1000.0);
        played.NoteSessionLength(7, today - 4, 10, 1000.0);
        played.AddSession(7, today - 3, 10);
        played.AddSession(7, today - 3, 10);
        played.Add(7, today - 3, 10, 2000.0);
        played.NoteSessionLength(7, today - 3, 10, 500.0);
        played.NoteSessionLength(7, today - 3, 10, 1500.0);
        played.NoteSessionLength(7, today - 3, 10, 700.0);
        played.Add(7, today - 3, 20, 9000.0);
        played.NoteSessionLength(7, today - 3, 20, 9000.0);
        played.AddSession(7, today - 3, 20);
        played.Add(7, today - 1, 30, 60.0);
        played.Add(7, today, 30, 60.0);
        played.Add(8, today, 99, 99999.0);
        played.AddHour(7, 13, 100.0);
        played.AddHour(7, 13, 50.0);
        played.AddHour(7, 24, 50.0);
        played.AddHour(7, -1, 50.0);

        const std::vector<GameFigures> figures = played.Figures(7);
        CHECK(figures.size() == 3);
        CHECK(figures[0].AppId == 20 && figures[0].Seconds == 9000.0);
        CHECK(figures[1].AppId == 10 && figures[1].Sessions == 3 && figures[1].LongestSession == 1500.0 && figures[1].Days == 2);
        CHECK(figures[1].FirstDay == today - 4 && figures[1].LastDay == today - 3);
        CHECK(figures[2].AppId == 30 && figures[2].Sessions == 0 && figures[2].Days == 2);

        std::vector<GameFigures> ranked = figures;
        const auto firstBy = [&](GameOrder order)
            {
                SortFigures(ranked, order);
                return ranked.front().AppId;
            };
        CHECK(firstBy(GameOrder::Sessions) == 10);
        CHECK(firstBy(GameOrder::LongestSession) == 20);
        CHECK(firstBy(GameOrder::Recent) == 30);
        CHECK(firstBy(GameOrder::Days) == 10);
        CHECK(firstBy(GameOrder::Time) == 20);
        for (int order = 0; order < static_cast<int>(GameOrder::Count); ++order)
            CHECK(std::string_view(GameOrderName(static_cast<GameOrder>(order))).size() > 0);

        const StatsSummary summary = played.Summary(7, today);
        CHECK(summary.Games == 3);
        CHECK(summary.Seconds == 12120.0);
        CHECK(summary.Sessions == 4);
        CHECK(summary.DaysIdled == 4);
        CHECK(summary.FirstDay == today - 4 && summary.FirstSessionDay == today - 4);
        CHECK(summary.BestDay == today - 3 && summary.BestDaySeconds == 11000.0);
        CHECK(summary.MostGamesInADay == 2 && summary.MostGamesDay == today - 3);
        CHECK(summary.LongestSession == 9000.0 && summary.LongestSessionApp == 20);
        CHECK(summary.MostSessions == 3 && summary.MostSessionsApp == 10);
        CHECK(summary.LongestStreak == 2 && summary.CurrentStreak == 2);
        CHECK(played.Summary(7, today + 1).CurrentStreak == 2);
        CHECK(played.Summary(7, today + 2).CurrentStreak == 0);
        CHECK(summary.Weekdays[0] == 60.0);
        CHECK(summary.Weekdays[6] == 60.0);
        CHECK(played.Summary(9, today).Games == 0 && played.Summary(9, today).CurrentStreak == 0);

        {
            IdleStats streak;
            for (std::int32_t day = today - 5; day <= today; ++day)
                streak.Add(3, day, 440, 3600.0);
            streak.Add(3, today + 6, 440, 10.0);
            const StatsSummary withFuture = streak.Summary(3, today);
            CHECK(withFuture.CurrentStreak == 6);
            CHECK(withFuture.LongestStreak == 6);
            CHECK(streak.Summary(3, today + 1).CurrentStreak == 6);
        }

        CHECK(played.PerHour(7)[13] == 150.0);
        CHECK(played.PerHour(9)[13] == 0.0);
        const std::vector<double> ofOne = played.PerDay(7, 10, today - 3, 2);
        CHECK(ofOne.size() == 2 && ofOne[0] == 1000.0 && ofOne[1] == 2000.0);

        IdleStats starting;
        starting.AddSession(7, today, 10);
        CHECK(starting.ByGame(7).empty() && starting.History(7).empty() && starting.Figures(7).empty());
        CHECK(starting.Summary(7, today).Sessions == 0 && starting.Summary(7, today).FirstSessionDay == today);

        played.Save(path);
        IdleStats reloaded;
        reloaded.Load(path);
        CHECK(reloaded.Total(7) == 12120.0);
        CHECK(reloaded.Figures(7)[1].Sessions == 3 && reloaded.Figures(7)[1].LongestSession == 1500.0);
        CHECK(reloaded.PerHour(7)[13] == 150.0);
        {
            std::ofstream older(path, std::ios::binary | std::ios::trunc);
            older << "; Seconds idled per day, account and game. Written by Steam-Idler.\r\n2026-11-02 | 7 | 10 | 3600\r\n";
        }
        IdleStats fromBefore;
        fromBefore.Load(path);
        CHECK(fromBefore.Total(7) == 3600.0 && fromBefore.Figures(7)[0].Sessions == 0 && fromBefore.Figures(7)[0].LongestSession == 0.0);
        std::filesystem::remove(path, ignored);
    }

    void TestCardQueue()
    {
        QueueEntry waiting = Entry(1, GoalKind::CardDrops);
        CHECK(!IsFinished(waiting));
        waiting.DropsLeft = 2;
        CHECK(!IsFinished(waiting));
        CHECK(Progress(waiting) < 0.0f && SecondsLeft(waiting) == 0.0);
        waiting.DropsLeft = 0;
        CHECK(IsFinished(waiting));

        const double warmup = 3.0 * 3600.0;
        std::vector<QueueEntry> queue = { Entry(1, GoalKind::CardDrops, 0.0f, 0.0, 200), Entry(2, GoalKind::CardDrops, 0.0f, 0.0, 10), Entry(3, GoalKind::CardDrops, 0.0f, 3600.0, 0), Entry(4, GoalKind::CardDrops) };
        CHECK(PlayedSeconds(queue[0]) == 200.0 * 60.0);
        CHECK(PlayedSeconds(queue[2]) == 3600.0);
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 1 }));
        std::vector<QueueEntry> warmedLast = { Entry(2, GoalKind::CardDrops, 0.0f, 0.0, 10), Entry(3, GoalKind::CardDrops), Entry(1, GoalKind::CardDrops, 0.0f, 0.0, 200), Entry(5, GoalKind::CardDrops, 0.0f, 0.0, 300) };
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 1 }));
        warmedLast[2].DropsLeft = 0;
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 5 }));
        warmedLast[3].Failed = true;
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 2, 3 }));
        warmedLast[3].Failed = false;
        warmedLast[3].GuardSecondsLeft = 0.0;
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 2, 3 }));

        queue[0].DropsLeft = 0;
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 2, 3, 4 }));
        queue[2].IdledSeconds = warmup;
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 3 }));
        queue[2].DropsLeft = 0;
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 2, 4 }));
        queue[2].DropsLeft = -1;
        queue[2].IdledSeconds = 3600.0;

        queue[0].DropsLeft = -1;
        CHECK((WantedForCards(queue, 0.0) == std::vector<std::uint32_t>{ 1 }));
        queue[0].DropsLeft = 0;
        CHECK((WantedForCards(queue, 0.0) == std::vector<std::uint32_t>{ 2 }));
        queue[1].Failed = true;
        queue[2].GuardSecondsLeft = 0.0;
        CHECK((WantedForCards(queue, 0.0) == std::vector<std::uint32_t>{ 4 }));
        queue[3].DropsLeft = 0;
        CHECK(WantedForCards(queue, 0.0).empty());
        CHECK(!HasWorkLeft(queue));

        std::vector<QueueEntry> many;
        for (std::uint32_t appId = 1; appId <= 40; ++appId)
            many.push_back(Entry(appId, GoalKind::CardDrops));
        CHECK(WantedForCards(many, warmup).size() == kMaxSlots);

        CHECK(ForecastQueue({ Entry(1, GoalKind::CardDrops) }, 1).Endless);
    }

    void TestCreatures()
    {
        CHECK(CreatureOfGame(2169200) == CreatureId::Ghillie);
        CHECK(CreatureOfGame(238090) == CreatureId::Ghillie);
        CHECK(CreatureOfGame(782330) == CreatureId::Demon);
        CreatureFacts lateAtNight;
        lateAtNight.RunningApps = { 620 };
        lateAtNight.Hour = 22;
        CHECK(IsEarned(InfoOf(CreatureId::Firefly), lateAtNight));
        CHECK(!IsEarned(InfoOf(CreatureId::Owl), lateAtNight));
        CreatureFacts valentine;
        valentine.RunningApps = { 620 };
        valentine.Month = 2;
        valentine.Day = 14;
        CHECK(IsEarned(InfoOf(CreatureId::LoveLetter), valentine));
        CHECK(!IsEarned(InfoOf(CreatureId::Parrot), valentine));
        CreatureFacts petted;
        petted.Pets = 999;
        CHECK(!IsEarned(InfoOf(CreatureId::Pufferfish), petted));
        petted.Pets = 1000;
        CHECK(IsEarned(InfoOf(CreatureId::Pufferfish), petted));
        CHECK(CreatureWithKey("coffee") == CreatureId::CoffeeMug);

        const auto all = AllCreatures();
        CHECK(all.size() == kCreatureCount);
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            CHECK(static_cast<std::size_t>(all[index].Id) == index);
            CHECK(CreatureWithKey(all[index].Key) == all[index].Id);
            CHECK(std::string_view(all[index].Name).size() > 0 && std::string_view(all[index].How).size() > 0 && std::string_view(all[index].Flavour).size() > 0);
            for (std::size_t other = index + 1; other < all.size(); ++other)
                CHECK(std::string_view(all[index].Key) != all[other].Key);
            for (const std::uint32_t appId : all[index].Games)
            {
                if (appId != 0)
                    CHECK(CreatureOfGame(appId) == all[index].Id);
            }
        }
        CHECK(!CreatureWithKey("nothing"));

        CreatureFacts facts;
        CHECK(IsEarned(InfoOf(CreatureId::Slime), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Frog), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Bug), facts));

        facts.TotalHours = 98.4;
        facts.GamesIdled = 73;
        CHECK(IsEarned(InfoOf(CreatureId::Frog), facts) && IsEarned(InfoOf(CreatureId::Whale), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Bunny), facts));
        CHECK(IsEarned(InfoOf(CreatureId::Turtle), facts) && !IsEarned(InfoOf(CreatureId::Octopus), facts));
        CHECK(ProgressText(InfoOf(CreatureId::Bunny), facts) == "98 of 100 hours");
        CHECK(ProgressShare(InfoOf(CreatureId::Octopus), facts) == 0.73f);
        CHECK(ProgressText(InfoOf(CreatureId::Frog), facts) == "10 of 10 hours");
        CreatureSet someFound;
        someFound.set(static_cast<std::size_t>(CreatureId::Frog));
        someFound.set(static_cast<std::size_t>(CreatureId::Whale));
        const std::vector<CreatureId> next = NextFinds(someFound, facts, 3);
        CHECK(next.size() == 3);
        CHECK(next[0] == CreatureId::Snail);
        CHECK(std::find(next.begin(), next.end(), CreatureId::Frog) == next.end());
        CHECK(std::find(next.begin(), next.end(), CreatureId::Pumpkin) == next.end());
        CHECK(NextFinds(someFound, facts, 0).empty());
        CHECK(NextFinds(CreatureSet().set(), facts, 3).empty());

        CreatureFacts almost;
        almost.TotalHours = 99.9;
        CHECK(!IsEarned(InfoOf(CreatureId::Bunny), almost) && ProgressText(InfoOf(CreatureId::Bunny), almost) == "99 of 100 hours");
        CHECK(ProgressShare(InfoOf(CreatureId::Frog), facts) == 1.0f);
        CHECK(ProgressShare(InfoOf(CreatureId::Pumpkin), facts) < 0.0f && ProgressText(InfoOf(CreatureId::Pumpkin), facts).empty());

        facts.Month = 10;
        facts.Day = 31;
        CHECK(!IsEarned(InfoOf(CreatureId::Pumpkin), facts));
        facts.RunningApps = { 10 };
        CHECK(IsEarned(InfoOf(CreatureId::Pumpkin), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Snowman), facts));
        facts.Month = 12;
        for (const int day : { 23, 24, 26, 27 })
        {
            facts.Day = day;
            CHECK(IsEarned(InfoOf(CreatureId::Snowman), facts) == (day == 24 || day == 26));
        }
        facts.Day = 13;
        facts.Weekday = 4;
        CHECK(!IsEarned(InfoOf(CreatureId::BlackCat), facts));
        facts.Weekday = 5;
        CHECK(IsEarned(InfoOf(CreatureId::BlackCat), facts));
        facts.Hour = 2;
        CHECK(!IsEarned(InfoOf(CreatureId::Owl), facts));
        facts.Hour = 3;
        CHECK(IsEarned(InfoOf(CreatureId::Owl), facts));
        facts.RunningApps.clear();
        CHECK(!IsEarned(InfoOf(CreatureId::Owl), facts));

        CHECK(!IsEarned(InfoOf(CreatureId::Crab), facts));
        const int gamesBefore = facts.GamesIdled;
        NoteGameTotal(facts, 774801, kGameCountsAfterSeconds - 1.0);
        CHECK(!IsEarned(InfoOf(CreatureId::Crab), facts));
        CHECK(facts.GamesIdled == gamesBefore);
        NoteGameTotal(facts, 774801, kGameCountsAfterSeconds);
        CHECK(IsEarned(InfoOf(CreatureId::Crab), facts));
        CHECK(facts.GamesIdled == gamesBefore + 1);
        facts.RunningApps = { 414700 };
        CHECK(!IsEarned(InfoOf(CreatureId::NightCam), facts));
        NoteGameTotal(facts, 414700, kGameCountsAfterSeconds + 60.0);
        CHECK(IsEarned(InfoOf(CreatureId::NightCam), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Centipede), facts));
        facts.RunningApps.assign(32, 1u);
        CHECK(IsEarned(InfoOf(CreatureId::Centipede), facts));

        CHECK(CountDaysInARow({}) == 0);
        CHECK(CountDaysInARow({ 5.0, 0.0, 5.0, 5.0, 5.0 }) == 3);
        CHECK(CountDaysInARow({ 5.0, 5.0, 0.0 }) == 2);
        CHECK(CountDaysInARow({ 5.0, 0.0, 0.0 }) == 0);

        CreatureSet found;
        for (int index = 0; index < 4; ++index)
            found.set(static_cast<std::size_t>(index));
        std::vector<CreatureId> before;
        for (std::uint32_t appId = 1000; appId < 3000; ++appId)
            before.push_back(PickCreature(appId, found));
        CHECK(std::count(before.begin(), before.end(), CreatureId::Slime) > 300);
        CHECK(std::count(before.begin(), before.end(), CreatureId::Robot) > 300);
        found.set(static_cast<std::size_t>(CreatureId::Frog));
        int moved = 0;
        for (std::uint32_t appId = 1000; appId < 3000; ++appId)
        {
            const CreatureId now = PickCreature(appId, found);
            if (now != before[appId - 1000])
            {
                CHECK(now == CreatureId::Frog);
                ++moved;
            }
        }
        CHECK(moved > 200 && moved < 600);

        std::array<int, static_cast<std::size_t>(Rarity::Count)> perRarity{};
        for (const CreatureInfo& info : AllCreatures())
        {
            ++perRarity[static_cast<std::size_t>(RarityOf(info.Id))];
            if (info.Kind == UnlockKind::Game)
                CHECK(RarityOf(info.Id) == Rarity::Uncommon);
            if (info.Kind == UnlockKind::Start)
                CHECK(RarityOf(info.Id) == Rarity::Common);
            if (info.Kind == UnlockKind::Date)
                CHECK(RarityOf(info.Id) == Rarity::Rare);
        }
        CHECK(perRarity[0] == 10 && perRarity[1] == 28 && perRarity[2] == 13 && perRarity[3] == 9 && perRarity[4] == 8);
        for (int rarity = 0; rarity < static_cast<int>(Rarity::Count); ++rarity)
        {
            CHECK(std::string_view(RarityName(static_cast<Rarity>(rarity))).size() > 0);
            CHECK(RarityWeight(static_cast<Rarity>(rarity)) > 0);
            if (rarity > 0)
                CHECK(RarityWeight(static_cast<Rarity>(rarity)) < RarityWeight(static_cast<Rarity>(rarity - 1)));
        }

        CreatureSet everyone;
        everyone.set();
        CreatureSet withoutOne = everyone;
        withoutOne.reset(static_cast<std::size_t>(CreatureId::Sloth));
        std::array<int, kCreatureCount> shownOn{};
        int movedToSloth = 0;
        constexpr int kDrawn = 60000;
        for (std::uint32_t appId = 5000000; appId < 5000000 + kDrawn; ++appId)
        {
            const CreatureId withAll = PickCreature(appId, everyone);
            ++shownOn[static_cast<std::size_t>(withAll)];
            const CreatureId without = PickCreature(appId, withoutOne);
            if (without != withAll)
            {
                CHECK(withAll == CreatureId::Sloth);
                ++movedToSloth;
            }
        }
        int weights = 0;
        for (const CreatureInfo& info : AllCreatures())
            weights += info.Kind == UnlockKind::Game ? 0 : RarityWeight(RarityOf(info.Id));
        for (const CreatureInfo& info : AllCreatures())
        {
            const int shown = shownOn[static_cast<std::size_t>(info.Id)];
            if (info.Kind == UnlockKind::Game)
            {
                CHECK(shown == 0);
                continue;
            }
            const double expected = static_cast<double>(kDrawn) * RarityWeight(RarityOf(info.Id)) / weights;
            CHECK(shown > expected * 0.75 && shown < expected * 1.25);
        }
        CHECK(shownOn[static_cast<std::size_t>(CreatureId::Slime)] > shownOn[static_cast<std::size_t>(CreatureId::Sloth)] * 5);
        CHECK(movedToSloth == shownOn[static_cast<std::size_t>(CreatureId::Sloth)]);

        CHECK(PickCreature(774801, found) != CreatureId::Crab);
        found.set(static_cast<std::size_t>(CreatureId::Crab));
        CHECK(PickCreature(774801, found) == CreatureId::Crab);
        for (std::uint32_t appId = 1000; appId < 3000; ++appId)
            CHECK(PickCreature(appId, found) != CreatureId::Crab);
        CHECK(PickCreature(5, CreatureSet{}) == CreatureId::Slime);

        int shiny = 0;
        for (std::uint32_t appId = 0; appId < 128000; ++appId)
            shiny += IsShiny(appId) ? 1 : 0;
        CHECK(shiny > 800 && shiny < 1200);
        std::uint32_t shinyApp = 0;
        while (!IsShiny(shinyApp))
            ++shinyApp;
        CHECK(!IsEarned(InfoOf(CreatureId::Star), {}));
        CreatureFacts shinyFacts;
        shinyFacts.RunningApps = { shinyApp };
        CHECK(IsEarned(InfoOf(CreatureId::Star), shinyFacts));
    }

    void TestTerrarium()
    {
        const TerrariumSize size{ 78.0f, 130.0f, 16.0f, 14.0f };
        const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds, bool animated = true)
            {
                for (double passed = 0.0; passed < seconds; passed += 0.05)
                    box.Update(guests, size, 0.05, animated);
            };
        const auto inside = [&](const TerrariumResident& resident)
            {
                return resident.X >= 0.0f && resident.X <= size.Width - size.SpriteWidth && resident.Y >= 0.0f && resident.Y <= size.Height - size.SpriteHeight;
            };

        Terrarium box;
        const std::vector<TerrariumGuest> three = { { 1, Habit::Walks, false }, { 2, Habit::Floats, false }, { 3, Habit::Stays, false } };
        box.Update(three, size, 0.05, true);
        CHECK(box.Residents().size() == 1);
        run(box, three, 10.0);
        CHECK(box.Residents().size() == 3);
        float stayX = -1.0f;
        for (const TerrariumResident& resident : box.Residents())
        {
            if (resident.Habit == Habit::Stays)
                stayX = resident.X;
        }
        bool allInside = true;
        for (int second = 0; second < 300; ++second)
        {
            run(box, three, 1.0);
            for (const TerrariumResident& resident : box.Residents())
            {
                allInside = allInside && (inside(resident) || resident.State == ResidentState::Entering);
                if (resident.Habit == Habit::Stays)
                    CHECK(resident.X == stayX);
                if (resident.Habit == Habit::Walks)
                    CHECK(resident.Y >= size.Height - size.SpriteHeight - kTerrariumGroundDepth - 0.01f);
                if (resident.Habit == Habit::Floats && resident.Encounter == TerrariumEncounter::None && resident.State != ResidentState::Moving)
                    CHECK(resident.Y <= size.Height - size.SpriteHeight - kTerrariumGroundDepth);
            }
        }
        CHECK(allInside);

        {
            const TerrariumSize deep{ 120.0f, 400.0f, 16.0f, 14.0f, 140.0f, 120.0f };
            Terrarium sinking;
            const std::vector<TerrariumGuest> trophy = { TerrariumGuest{ 7, Habit::Stays, false, CreatureId::Trophy } };
            for (double passed = 0.0; passed < 10.0; passed += 0.05)
                sinking.Update(trophy, deep, 0.05, true);
            CHECK(sinking.Residents().size() == 1 && sinking.Residents()[0].State == ResidentState::Resting);
            const float stood = sinking.Residents().empty() ? 0.0f : sinking.Residents()[0].Y;
            const float lakeTop = deep.Height - deep.LakeDepth;
            bool keptOutOfLake = true;
            bool hiddenBelowWhereItStood = true;
            double gone = -1.0;
            for (double passed = 0.05; passed < 5.0; passed += 0.05)
            {
                sinking.Update({}, deep, 0.05, true);
                for (const TerrariumResident& resident : sinking.Residents())
                {
                    keptOutOfLake = keptOutOfLake && resident.Y <= lakeTop + 0.01f;
                    hiddenBelowWhereItStood = hiddenBelowWhereItStood && std::fabs(resident.GroundLine - (stood + deep.SpriteHeight)) < 0.01f;
                }
                if (sinking.Residents().empty() && gone < 0.0)
                    gone = passed;
            }
            CHECK(keptOutOfLake);
            CHECK(hiddenBelowWhereItStood);
            CHECK(gone > 0.5 && gone < 2.0);

            Terrarium dropping;
            dropping.Update(trophy, deep, 0.05, true);
            dropping.Update(trophy, deep, 0.05, true);
            const float landsAt = dropping.Residents().empty() ? 0.0f : dropping.Residents()[0].TargetY;
            CHECK(!dropping.Residents().empty() && dropping.Residents()[0].State == ResidentState::Entering && dropping.Residents()[0].Y < landsAt);
            dropping.Update({}, deep, 0.05, true);
            CHECK(!dropping.Residents().empty() && std::fabs(dropping.Residents()[0].GroundLine - (landsAt + deep.SpriteHeight)) < 0.01f);
        }

        std::vector<TerrariumGuest> sleepers = three;
        sleepers[0].Asleep = true;
        run(box, sleepers, 0.1);
        const auto walker = [&]() { return *std::find_if(box.Residents().begin(), box.Residents().end(), [](const TerrariumResident& r) { return r.Seed == 1; }); };
        run(box, sleepers, 1.0);
        const float sleptAt = walker().X;
        CHECK(walker().Asleep && walker().State == ResidentState::Resting);
        run(box, sleepers, 20.0);
        CHECK(walker().X == sleptAt);
        run(box, three, 20.0);
        CHECK(!walker().Asleep);

        run(box, { three[1], three[2] }, 20.0);
        CHECK(box.Residents().size() == 2);
        run(box, {}, 20.0);
        CHECK(box.Residents().empty());

        std::vector<TerrariumGuest> crowd;
        for (std::uint32_t seed = 1; seed <= 32; ++seed)
            crowd.push_back({ seed, Habit::Walks, seed % 2 == 0 });
        Terrarium busy;
        run(busy, crowd, 8.0);
        CHECK(static_cast<int>(busy.Residents().size()) == kMaxTerrariumResidents);
        CHECK(std::none_of(busy.Residents().begin(), busy.Residents().end(), [](const TerrariumResident& r) { return r.Asleep; }));
        std::vector<std::uint32_t> seen;
        for (int second = 0; second < 1200; ++second)
        {
            run(busy, crowd, 1.0);
            int staying = 0;
            for (const TerrariumResident& resident : busy.Residents())
            {
                staying += resident.State != ResidentState::Leaving ? 1 : 0;
                if (std::find(seen.begin(), seen.end(), resident.Seed) == seen.end())
                    seen.push_back(resident.Seed);
            }
            CHECK(staying <= kMaxTerrariumResidents);
        }
        CHECK(seen.size() > kMaxTerrariumResidents);

        Terrarium again;
        run(again, three, 10.0);
        run(again, { three[1], three[2] }, 0.1);
        run(again, three, 20.0);
        CHECK(again.Residents().size() == 3);

        Terrarium still;
        still.Update(three, size, 0.05, false);
        CHECK(still.Residents().size() == 3);
        for (const TerrariumResident& resident : still.Residents())
            CHECK(inside(resident) && resident.State == ResidentState::Resting);
        const float stillX = still.Residents()[0].X;
        run(still, three, 10.0, false);
        CHECK(still.Residents()[0].X == stillX);
        still.Update({}, size, 0.05, false);
        CHECK(still.Residents().empty());

        Terrarium lake;
        const std::vector<TerrariumGuest> shore = { { 1, Habit::Walks, false }, { 2, Habit::Swims, false } };
        for (int second = 0; second < 200; ++second)
        {
            run(lake, shore, 1.0);
            for (const TerrariumResident& resident : lake.Residents())
            {
                if (resident.Habit == Habit::Swims)
                    CHECK(resident.Y >= size.Height - kTerrariumLakeDepth);
                else
                    CHECK(resident.Y + size.SpriteHeight <= size.Height - kTerrariumLakeDepth);
            }
        }

        const auto onLand = [](const TerrariumResident& resident, const TerrariumSize& box)
            {
                return resident.Y + box.SpriteHeight >= box.Height - box.GroundDepth - 0.01f && resident.Y + box.SpriteHeight <= box.Height - box.LakeDepth;
            };
        const TerrariumSize tall{ 300.0f, 700.0f, 16.0f, 14.0f };
        Terrarium falling;
        const std::vector<TerrariumGuest> cube = { { 3, Habit::Stays, false } };
        for (int step = 0; step < 60; ++step)
            falling.Update(cube, tall, 0.05, true);
        CHECK(falling.Residents().size() == 1 && falling.Residents()[0].State == ResidentState::Resting && onLand(falling.Residents()[0], tall));

        const std::vector<TerrariumGuest> all = { { 1, Habit::Walks, false }, { 2, Habit::Floats, false }, { 3, Habit::Stays, false }, { 4, Habit::Swims, false } };
        for (const bool animated : { false, true })
        {
            Terrarium growing;
            growing.Update(all, size, 0.05, false);
            growing.Update(all, tall, 0.05, animated);
            float stays = -1.0f;
            for (const TerrariumResident& resident : growing.Residents())
            {
                if (resident.Habit == Habit::Walks || resident.Habit == Habit::Stays)
                    CHECK(onLand(resident, tall));
                if (resident.Habit == Habit::Swims)
                    CHECK(resident.Y >= tall.Height - tall.LakeDepth);
                if (resident.Habit == Habit::Floats)
                    CHECK(resident.Y + tall.SpriteHeight <= tall.Height - tall.GroundDepth);
                if (resident.Habit == Habit::Stays)
                    stays = resident.Y;
            }
            growing.Update(all, size, 0.05, animated);
            for (const TerrariumResident& resident : growing.Residents())
            {
                CHECK(resident.X <= size.Width - size.SpriteWidth);
                if (resident.Habit == Habit::Stays)
                    CHECK(onLand(resident, size) && stays != resident.Y);
            }
        }

        const TerrariumSize roomy{ 400.0f, 300.0f, 16.0f, 14.0f, 100.0f, 40.0f };
        const TerrariumSize narrow{ 200.0f, 120.0f, 16.0f, 14.0f, 50.0f, 20.0f };
        std::vector<TerrariumGuest> crowded;
        for (std::uint32_t seed = 1; seed <= 24; ++seed)
            crowded.push_back({ seed, seed % 3 == 0 ? Habit::Floats : seed % 3 == 1 ? Habit::Walks : Habit::Swims, false });
        Terrarium keeping(5u, 24);
        keeping.Update(crowded, roomy, 0.0, false);
        const std::vector<TerrariumResident> atFirst = keeping.Residents();
        const auto landShare = [](const TerrariumResident& resident, const TerrariumSize& box)
            {
                const float low = box.Height - box.SpriteHeight - box.GroundDepth;
                const float high = box.Height - box.SpriteHeight - box.LakeDepth - 1.0f;
                return (resident.Y - low) / (high - low);
            };
        keeping.Update(crowded, narrow, 0.0, false);
        CHECK(keeping.Residents().size() == atFirst.size());
        float lowestShare = 1.0f;
        float highestShare = 0.0f;
        for (std::size_t index = 0; index < atFirst.size(); ++index)
        {
            const TerrariumResident& resident = keeping.Residents()[index];
            CHECK(std::fabs(resident.X / (narrow.Width - narrow.SpriteWidth) - atFirst[index].X / (roomy.Width - roomy.SpriteWidth)) < 0.001f);
            if (resident.Habit != Habit::Walks)
                continue;
            CHECK(std::fabs(landShare(resident, narrow) - landShare(atFirst[index], roomy)) < 0.001f);
            lowestShare = std::min(lowestShare, landShare(resident, narrow));
            highestShare = std::max(highestShare, landShare(resident, narrow));
        }
        CHECK(highestShare - lowestShare > 0.4f);
        keeping.Update(crowded, roomy, 0.0, false);
        for (std::size_t index = 0; index < atFirst.size(); ++index)
        {
            CHECK(std::fabs(keeping.Residents()[index].X - atFirst[index].X) < 0.01f);
            CHECK(std::fabs(keeping.Residents()[index].Y - atFirst[index].Y) < 0.01f);
        }

        Terrarium passing;
        const std::vector<TerrariumGuest> oneWalker = { { 1, Habit::Walks, false } };
        passing.Update(oneWalker, roomy, 0.05, true);
        CHECK(passing.Residents().size() == 1 && passing.Residents()[0].State == ResidentState::Entering);
        const float outside = passing.Residents()[0].X < 0.0f ? passing.Residents()[0].X : passing.Residents()[0].X - roomy.Width;
        passing.Update(oneWalker, narrow, 0.0, true);
        CHECK(std::fabs((passing.Residents()[0].X < 0.0f ? passing.Residents()[0].X : passing.Residents()[0].X - narrow.Width) - outside) < 0.01f);
        for (int step = 0; step < 200; ++step)
            passing.Update(oneWalker, narrow, 0.05, true);
        passing.Update({}, narrow, 0.05, true);
        CHECK(passing.Residents().size() == 1 && passing.Residents()[0].State == ResidentState::Leaving);
        const bool leftWall = passing.Residents()[0].TargetX < 0.0f;
        passing.Update({}, roomy, 0.0, true);
        CHECK(leftWall ? passing.Residents()[0].TargetX < 0.0f : passing.Residents()[0].TargetX > roomy.Width);

        const auto residentOf = [](const Terrarium& of, std::uint32_t seed) { return *std::find_if(of.Residents().begin(), of.Residents().end(), [&](const TerrariumResident& r) { return r.Seed == seed; }); };
        const auto carry = [&](Terrarium& hand, std::uint32_t seed, float x, float y)
            {
                hand.Grab(seed);
                hand.Carry(seed, x, y);
                for (int step = 0; step < 40; ++step)
                    hand.Update(all, tall, 0.05, true);
                CHECK(residentOf(hand, seed).X == x && residentOf(hand, seed).Y == y && residentOf(hand, seed).State == ResidentState::Held);
                hand.Drop(seed, tall);
            };
        const auto settle = [&](Terrarium& hand) { for (int step = 0; step < 160; ++step) hand.Update(all, tall, 0.05, true); };
        Terrarium hand;
        hand.Update(all, tall, 0.05, false);
        carry(hand, 1, 50.0f, 10.0f);
        CHECK(residentOf(hand, 1).State == ResidentState::Settling);
        settle(hand);
        CHECK(residentOf(hand, 1).State != ResidentState::Settling && onLand(residentOf(hand, 1), tall));
        carry(hand, 1, 50.0f, tall.Height - tall.SpriteHeight);
        settle(hand);
        CHECK(onLand(residentOf(hand, 1), tall));
        carry(hand, 2, 60.0f, tall.Height - tall.SpriteHeight);
        settle(hand);
        CHECK(residentOf(hand, 2).Y + tall.SpriteHeight <= tall.Height - tall.GroundDepth);
        carry(hand, 4, 70.0f, 10.0f);
        settle(hand);
        CHECK(residentOf(hand, 4).Y >= tall.Height - tall.LakeDepth);
        carry(hand, 3, 80.0f, 10.0f);
        settle(hand);
        CHECK(residentOf(hand, 3).X == 80.0f && residentOf(hand, 3).State == ResidentState::Resting && onLand(residentOf(hand, 3), tall));
        for (int step = 0; step < 400; ++step)
            hand.Update(all, tall, 0.05, true);
        CHECK(residentOf(hand, 3).X == 80.0f);
        Terrarium night;
        std::vector<TerrariumGuest> flyer = { { 2, Habit::Floats, false } };
        run(night, flyer, 10.0);
        CHECK(residentOf(night, 2).Y + size.SpriteHeight <= size.Height - size.GroundDepth);
        flyer[0].Asleep = true;
        run(night, flyer, 30.0);
        CHECK(residentOf(night, 2).State == ResidentState::Resting);
        CHECK(residentOf(night, 2).Y + size.SpriteHeight >= size.Height - size.GroundDepth);
        const float sleptAtX = residentOf(night, 2).X;
        run(night, flyer, 30.0);
        CHECK(residentOf(night, 2).X == sleptAtX);
        flyer[0].Asleep = false;
        run(night, flyer, 30.0);
        CHECK(residentOf(night, 2).Y + size.SpriteHeight <= size.Height - size.GroundDepth);
        Terrarium dark;
        const std::vector<TerrariumGuest> sleepingFlyer = { { 2, Habit::Floats, true } };
        dark.Update(sleepingFlyer, size, 0.05, false);
        CHECK(residentOf(dark, 2).Y + size.SpriteHeight >= size.Height - size.GroundDepth);

        const auto thrown = [&](Terrarium& box, std::uint32_t seed, float fromX, float fromY, float speedX, float speedY)
            {
                box.Grab(seed);
                float x = fromX;
                float y = fromY;
                box.Carry(seed, x, y);
                for (int step = 0; step < 10; ++step)
                {
                    x += speedX * 0.016f;
                    y += speedY * 0.016f;
                    box.Carry(seed, x, y, 0.016);
                }
                box.Drop(seed, tall);
                const auto at = residentOf(box, seed);
                CHECK(at.State == ResidentState::Settling);
                bool stayedInside = true;
                float farthest = at.X;
                for (int step = 0; step < 400; ++step)
                {
                    box.Update(all, tall, 0.05, true);
                    const auto now = residentOf(box, seed);
                    stayedInside = stayedInside && now.X >= 0.0f && now.X <= tall.Width - tall.SpriteWidth && now.Y >= 0.0f && now.Y <= tall.Height - tall.SpriteHeight;
                    if (now.State == ResidentState::Settling)
                        farthest = speedX > 0.0f ? std::max(farthest, now.X) : std::min(farthest, now.X);
                }
                CHECK(stayedInside);
                CHECK(residentOf(box, seed).State != ResidentState::Settling);
                return farthest - at.X;
            };
        Terrarium throwing;
        throwing.Update(all, tall, 0.05, false);
        CHECK(thrown(throwing, 1, 40.0f, 300.0f, 120.0f, 0.0f) > 10.0f);
        CHECK(onLand(residentOf(throwing, 1), tall) || residentOf(throwing, 1).State == ResidentState::Moving);
        CHECK(thrown(throwing, 2, 200.0f, 300.0f, -120.0f, 60.0f) < -10.0f);
        CHECK(thrown(throwing, 3, 100.0f, 400.0f, 90.0f, -200.0f) > 10.0f);
        CHECK(onLand(residentOf(throwing, 3), tall));
        CHECK(thrown(throwing, 4, 100.0f, 500.0f, 60.0f, 100.0f) > 1.0f);
        CHECK(residentOf(throwing, 4).Y >= tall.Height - tall.LakeDepth);
        Terrarium wall;
        wall.Update(all, tall, 0.05, false);
        thrown(wall, 1, 260.0f, 620.0f, 300.0f, 0.0f);
        CHECK(residentOf(wall, 1).X < tall.Width - tall.SpriteWidth);
        Terrarium steady;
        steady.Update(all, tall, 0.05, false);
        const auto before = residentOf(steady, 3);
        steady.Grab(3);
        for (int step = 0; step < 30; ++step)
            steady.Carry(3, before.X, before.Y, 0.016);
        steady.Drop(3, tall);
        CHECK(residentOf(steady, 3).State == ResidentState::Resting && residentOf(steady, 3).X == before.X);

        Terrarium plain;
        plain.Update(all, tall, 0.05, false);
        plain.Grab(1);
        plain.Carry(1, 5.0f, 5.0f);
        plain.Drop(1, tall);
        plain.Update(all, tall, 0.05, false);
        CHECK(residentOf(plain, 1).State == ResidentState::Resting && onLand(residentOf(plain, 1), tall));
        plain.Grab(99);
        plain.Carry(99, 1.0f, 1.0f);
        plain.Drop(99, tall);

        Terrarium sinking;
        sinking.Update(cube, size, 0.05, false);
        const float restedAt = sinking.Residents()[0].Y;
        sinking.Update({}, size, 0.05, true);
        sinking.Update({}, size, 0.05, true);
        CHECK(sinking.Residents()[0].State == ResidentState::Leaving && sinking.Residents()[0].Y > restedAt);

        Terrarium home(2u, 52);
        std::vector<TerrariumGuest> everyone;
        for (std::uint32_t seed = 1; seed <= 52; ++seed)
            everyone.push_back({ seed, Habit::Walks, false });
        home.Update(everyone, tall, 0.05, false);
        CHECK(home.Residents().size() == 52);
        for (int step = 0; step < 2000; ++step)
            home.Update(everyone, tall, 0.05, true);
        CHECK(home.Residents().size() == 52);
        CHECK(std::none_of(home.Residents().begin(), home.Residents().end(), [](const TerrariumResident& r) { return r.State == ResidentState::Leaving; }));

        Terrarium tiny;
        const TerrariumSize small{ 8.0f, 6.0f, 16.0f, 14.0f };
        for (int step = 0; step < 200; ++step)
            tiny.Update(three, small, 0.05, true);
        CHECK(tiny.Residents().size() == 3);
    }

    void TestWebPolicy()
    {
        CHECK(IsSteamPage("https://steamcommunity.com/login/home/"));
        CHECK(IsSteamPage("https://store.steampowered.com/login/"));
        CHECK(IsSteamPage("https://login.steampowered.com/jwt/refresh"));
        CHECK(IsSteamPage("https://steamcommunity.com:443/my"));
        CHECK(IsSteamPage("HTTPS://SteamCommunity.COM/"));
        CHECK(IsSteamPage("https://steamcommunity.com?x=https://evil.example/"));

        CHECK(!IsSteamPage("http://steamcommunity.com/"));
        CHECK(!IsSteamPage("https://steamcommunity.com.evil.example/"));
        CHECK(!IsSteamPage("https://evil.example/steamcommunity.com"));
        CHECK(!IsSteamPage("https://notsteamcommunity.com/"));
        CHECK(!IsSteamPage("https://steamcommunity.com@evil.example/"));
        CHECK(!IsSteamPage("https://evil.example@steamcommunity.com/"));
        CHECK(!IsSteamPage("https://evil.example\\steamcommunity.com/"));
        CHECK(!IsSteamPage("https://evil.example#steamcommunity.com"));
        CHECK(!IsSteamPage("https://evil.example?steamcommunity.com"));
        CHECK(!IsSteamPage("javascript:alert('https://steamcommunity.com')"));
        CHECK(!IsSteamPage("about:blank"));
        CHECK(!IsSteamPage("file:///C:/steamcommunity.com"));
        CHECK(!IsSteamPage(""));
        CHECK(!IsSteamPage("https://"));

        CHECK(HostOf("https://Store.SteamPowered.com:443/app/1") == "store.steampowered.com");
        CHECK(HostOf("https://a@b.example/") == "b.example");
        CHECK(HostOf("not an address").empty());
        CHECK(IsHttps("https://example.com") && !IsHttps("http://example.com") && !IsHttps("https://"));

        CHECK(WithoutQuery("https://login.steampowered.com/jwt/refresh?redir=x") == "https://login.steampowered.com/jwt/refresh");
        CHECK(WithoutQuery("https://steamcommunity.com/my#top") == "https://steamcommunity.com/my");
        CHECK(WithoutQuery("https://steamcommunity.com/my") == "https://steamcommunity.com/my");

        CHECK(ParsePageUser("76561198000000000|Some Name").SteamId == 76561198000000000ull);
        CHECK(ParsePageUser("76561198000000000|Some Name").Name == "Some Name");
        CHECK(ParsePageUser("76561198000000000|a|b").Name == "a|b");
        CHECK(ParsePageUser("76561198000000000|").Name.empty());
        CHECK(ParsePageUser("|").SteamId == 0);
        CHECK(ParsePageUser("").SteamId == 0);
        CHECK(ParsePageUser("false|").SteamId == 0);
        CHECK(ParsePageUser("12345|Short").SteamId == 0);
        CHECK(ParsePageUser("7656119800000000x|Bad").SteamId == 0);
        CHECK(ParsePageUser("00000000000000000|Zero").SteamId == 0);
        CHECK(ParsePageUser("|Only a name").Name.empty());
    }

    void TestRuntimeLog()
    {
        CHECK(RuntimeLog::TraceText("C:\\src\\Core\\File.cpp", "Idler::Model::Run", 12) == "File.cpp:Run:12");
        CHECK(RuntimeLog::TraceText("a/b.cpp", "Main", 3) == "b.cpp:Main:3");
        CHECK(RuntimeLog::TraceText(nullptr, nullptr, 0) == "unknown:unknown:0");
        CHECK(std::string_view(RuntimeLog::LevelName(LogLevel::Warning)) == "WARN");

        RuntimeLog::Clear();
        const std::uint64_t generation = RuntimeLog::Generation();
        LOG_WARN("one\r\nline");
        CHECK(RuntimeLog::Generation() > generation);
        std::vector<LogEntry> entries = RuntimeLog::Entries();
        CHECK(entries.size() == 1);
        CHECK(entries[0].Level == LogLevel::Warning && entries[0].Message == "one  line");
        CHECK(entries[0].Time.size() == 8);
        CHECK(entries[0].Trace.starts_with("Tests.cpp:TestRuntimeLog:"));

        LogEntry sample;
        sample.Level = LogLevel::Warning;
        sample.Trace = "IdlerModel.cpp:Schedule:12";
        sample.Message = "Paused: Another Game runs";
        CHECK(RuntimeLog::EntryMatches(sample, LogLevel::Info, ""));
        CHECK(RuntimeLog::EntryMatches(sample, LogLevel::Warning, ""));
        CHECK(!RuntimeLog::EntryMatches(sample, LogLevel::Error, ""));
        CHECK(RuntimeLog::EntryMatches(sample, LogLevel::Info, "another game"));
        CHECK(RuntimeLog::EntryMatches(sample, LogLevel::Info, "SCHEDULE"));
        CHECK(!RuntimeLog::EntryMatches(sample, LogLevel::Info, "crab"));
        sample.Level = LogLevel::Success;
        CHECK(RuntimeLog::EntryMatches(sample, LogLevel::Info, ""));
        CHECK(!RuntimeLog::EntryMatches(sample, LogLevel::Warning, ""));

        RuntimeLog::Clear();
        for (std::size_t index = 0; index < RuntimeLog::kMaxEntries + 5; ++index)
            LOG_INFO(std::to_string(index));
        entries = RuntimeLog::Entries();
        CHECK(entries.size() == RuntimeLog::kMaxEntries);
        CHECK(entries.front().Message == "5");
        CHECK(entries.back().Message == std::to_string(RuntimeLog::kMaxEntries + 4));

        const std::filesystem::path folder = std::filesystem::temp_directory_path() / "steam-idler-log-test";
        const std::filesystem::path file = folder / "a.log";
        const std::filesystem::path previous = folder / "a.previous.log";
        std::error_code ignored;
        std::filesystem::create_directories(folder, ignored);
        std::filesystem::remove(file, ignored);
        std::filesystem::remove(previous, ignored);
        const auto read = [](const std::filesystem::path& path)
            {
                std::ifstream stream(path, std::ios::binary);
                return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            };

        RuntimeLog::OpenFile(file);
        LOG_INFO("first run");
        RuntimeLog::Clear();
        RuntimeLog::OpenFile(file);
        LOG_ERROR("second run");
        RuntimeLog::OpenFile({});
        LOG_INFO("no file now");

        CHECK(read(previous).find("[INFO] [Tests.cpp:TestRuntimeLog:") != std::string::npos);
        CHECK(read(previous).find("first run") != std::string::npos);
        CHECK(read(file).find("[ERROR]") != std::string::npos);
        CHECK(read(file).find("second run") != std::string::npos);
        CHECK(read(file).find("first run") == std::string::npos);
        CHECK(read(file).find("no file now") == std::string::npos);

        std::filesystem::remove(file, ignored);
        std::filesystem::remove(previous, ignored);
        std::filesystem::remove(folder, ignored);
        RuntimeLog::Clear();
    }

    void TestBondsAndSets()
    {
        CHECK(BondLevel(0.0) == 0);
        CHECK(BondLevel(0.99) == 0);
        CHECK(BondLevel(1.0) == 1);
        CHECK(BondLevel(9.99) == 1);
        CHECK(BondLevel(10.0) == 2);
        CHECK(BondLevel(1000.0) == kMaxBondLevel);
        CHECK(BondLevel(100000.0) == kMaxBondLevel);
        CHECK(BondHours(3600.0, 0) == 1.0);
        CHECK(BondHours(0.0, 10) == 1.0);
        CHECK(BondHoursOfLevel(0) == 0.0);
        CHECK(BondHoursOfLevel(kMaxBondLevel) == 1000.0);
        CHECK(BondHoursOfLevel(kMaxBondLevel + 3) == 1000.0);
        CHECK(BondShare(0.0) == 0.0f);
        CHECK(BondShare(0.5) == 0.5f);
        CHECK(BondShare(1000.0) == 1.0f);
        for (int level = 0; level <= kMaxBondLevel; ++level)
        {
            CHECK(BondName(level)[0] != '\0');
            CHECK(BondLevel(BondHoursOfLevel(level)) == level);
        }

        std::array<int, kCreatureCount> sets{};
        for (const CreatureFamily& family : AllFamilies())
        {
            CHECK(family.Members.size() >= 2);
            for (std::size_t first = 0; first < family.Members.size(); ++first)
            {
                ++sets[static_cast<std::size_t>(family.Members[first])];
                for (std::size_t second = first + 1; second < family.Members.size(); ++second)
                    CHECK(family.Members[first] != family.Members[second]);
            }
        }
        for (const int count : sets)
            CHECK(count <= 2);

        CreatureSet found;
        const CreatureFamily& cats = AllFamilies()[1];
        CHECK(std::string_view(cats.Name) == "Cats");
        CHECK(FoundInFamily(cats, found) == 0);
        found.set(static_cast<std::size_t>(CreatureId::Cat));
        found.set(static_cast<std::size_t>(CreatureId::Slime));
        CHECK(FoundInFamily(cats, found) == 1);
        CHECK(IsInFamily(cats, CreatureId::BlackCat));
        CHECK(!IsInFamily(cats, CreatureId::Slime));

        CHECK(RelationOf(CreatureId::Cat, CreatureId::Cat) == Relation::None);
        CHECK(RelationOf(CreatureId::Cat, CreatureId::Hamster) == Relation::Chases);
        CHECK(RelationOf(CreatureId::Hamster, CreatureId::Cat) == Relation::None);
        CHECK(RelationOf(CreatureId::Cat, CreatureId::LeetCat) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Penguin, CreatureId::Snowman) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Snowman, CreatureId::Penguin) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Capybara, CreatureId::Shark) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Slime, CreatureId::Trophy) == Relation::None);
        CHECK(RelationOf(CreatureId::Shark, CreatureId::Fish) == Relation::Chases);
        CHECK(RelationOf(CreatureId::Fish, CreatureId::Shark) == Relation::None);
        for (const CreatureInfo& one : AllCreatures())
        {
            for (const CreatureInfo& other : AllCreatures())
                CHECK(!(RelationOf(one.Id, other.Id) == Relation::Chases && RelationOf(other.Id, one.Id) == Relation::Chases));
        }
    }

    void TestEncounters()
    {
        const TerrariumSize size{ 78.0f, 130.0f, 16.0f, 14.0f };
        const auto inside = [&](const TerrariumResident& resident)
            {
                return resident.X >= 0.0f && resident.X <= size.Width - size.SpriteWidth && resident.Y >= 0.0f && resident.Y <= size.Height - size.SpriteHeight;
            };
        const auto walker = [](std::uint32_t seed, CreatureId id, bool asleep = false) { return TerrariumGuest{ seed, Habit::Walks, asleep, id }; };
        constexpr double kStep = 0.05;

        {
            Terrarium box(11u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::LeetCat) };
            bool sought = false;
            bool greeted = false;
            bool hopped = false;
            bool hearts = false;
            bool over = false;
            for (double passed = 0.0; passed < 120.0 && !over; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    sought |= resident.Encounter == TerrariumEncounter::Seeking;
                    if (resident.Encounter == TerrariumEncounter::Greeting)
                    {
                        greeted = true;
                        hopped |= resident.Lift > 0.5f;
                        hearts |= resident.Emote == TerrariumEmote::Heart;
                        const TerrariumResident& first = box.Residents()[0];
                        const TerrariumResident& second = box.Residents()[1];
                        CHECK(std::fabs(first.X - second.X) <= size.SpriteWidth + 1.0f);
                    }
                    else if (greeted && box.Residents()[0].Encounter == TerrariumEncounter::None && box.Residents()[1].Encounter == TerrariumEncounter::None)
                    {
                        over = true;
                    }
                }
            }
            CHECK(sought && greeted && hopped && hearts && over);
            for (const TerrariumResident& resident : box.Residents())
                CHECK(resident.Lift == 0.0f && resident.Emote == TerrariumEmote::None && resident.Partner == 0);
        }

        {
            Terrarium box(5u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::Hamster) };
            bool chased = false;
            bool alarm = false;
            for (double passed = 0.0; passed < 120.0 && !chased; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    if (resident.Encounter == TerrariumEncounter::Chasing)
                    {
                        chased = true;
                        CHECK(resident.Creature == CreatureId::Cat);
                    }
                }
            }
            CHECK(chased);
            for (double passed = 0.0; passed < 20.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    alarm |= resident.Emote == TerrariumEmote::Alarm;
                    if (resident.State != ResidentState::Entering)
                        CHECK(inside(resident));
                }
            }
            CHECK(alarm);
        }

        {
            Terrarium box(3u);
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Swims, false, CreatureId::Fish } };
            bool leapt = false;
            bool splash = false;
            float highest = 0.0f;
            for (double passed = 0.0; passed < 120.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    if (resident.Encounter != TerrariumEncounter::Leaping)
                        continue;
                    leapt = true;
                    splash |= resident.Emote == TerrariumEmote::Splash;
                    highest = std::max(highest, resident.Y + size.SpriteHeight - resident.Lift);
                    if (resident.Lift > 1.0f)
                        CHECK(resident.TargetY == resident.Y);
                }
            }
            CHECK(leapt && splash);
            CHECK(!box.Residents().empty() && box.Residents()[0].Lift == 0.0f);
        }

        {
            Terrarium box(9u);
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Floats, false, CreatureId::Firefly }, TerrariumGuest{ 2, Habit::Stays, false, CreatureId::Bonfire } };
            bool visited = false;
            for (double passed = 0.0; passed < 300.0 && !visited; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                    visited |= resident.Habit == Habit::Floats && resident.Encounter == TerrariumEncounter::Greeting;
            }
            CHECK(visited);
            for (double passed = 0.0; passed < 30.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Residents().size() == 2);
        }

        int huddled = 0;
        for (std::uint32_t seed = 1; seed <= 12; ++seed)
        {
            Terrarium box(seed);
            std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Slime, true), walker(2, CreatureId::Robot, true) };
            box.Update(guests, size, kStep, false);
            for (double passed = 0.0; passed < 30.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                    CHECK(resident.Encounter == TerrariumEncounter::None);
            }
            const float dx = box.Residents()[0].X - box.Residents()[1].X;
            const float dy = box.Residents()[0].Y - box.Residents()[1].Y;
            guests[1].Asleep = false;
            box.Update(guests, size, kStep, true);
            guests[1].Asleep = true;
            for (double passed = 0.0; passed < 15.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            const TerrariumResident& first = box.Residents()[0];
            const TerrariumResident& second = box.Residents()[1];
            CHECK(!first.Huddling && !second.Huddling);
            CHECK(first.State == ResidentState::Resting && second.State == ResidentState::Resting);
            if (std::sqrt(dx * dx + dy * dy) < 75.0f)
            {
                CHECK(std::fabs(first.X - second.X) <= size.SpriteWidth);
                ++huddled;
            }
        }
        CHECK(huddled > 0);

        {
            Terrarium box(11u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::LeetCat) };
            bool met = false;
            for (double passed = 0.0; passed < 120.0 && !met; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                met = box.Residents().size() == 2 && box.Residents()[0].Encounter != TerrariumEncounter::None;
            }
            CHECK(met);
            const std::uint32_t taken = box.Residents()[0].Seed;
            box.Grab(taken);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            for (const TerrariumResident& resident : box.Residents())
                CHECK(resident.Encounter == TerrariumEncounter::None && resident.Partner == 0);
            box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            box.Drop(taken, size);
        }

        {
            Terrarium box(11u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::LeetCat) };
            for (double passed = 0.0; passed < 30.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            box.Update(guests, size, kStep, false);
            for (const TerrariumResident& resident : box.Residents())
                CHECK(resident.Encounter == TerrariumEncounter::None && resident.Lift == 0.0f && resident.Emote == TerrariumEmote::None);
        }

        {
            Terrarium box(21u);
            std::vector<TerrariumGuest> guests;
            std::uint32_t seed = 1;
            for (const CreatureId id : { CreatureId::Cat, CreatureId::Hamster, CreatureId::Slime, CreatureId::KingSlime, CreatureId::Zombie, CreatureId::Bunny, CreatureId::Dwarf, CreatureId::Bug })
                guests.push_back(walker(seed++, id));
            guests.push_back(TerrariumGuest{ seed++, Habit::Swims, false, CreatureId::Shark });
            guests.push_back(TerrariumGuest{ seed++, Habit::Swims, false, CreatureId::Fish });
            guests.push_back(TerrariumGuest{ seed++, Habit::Floats, false, CreatureId::Bat });
            guests.push_back(TerrariumGuest{ seed++, Habit::Floats, false, CreatureId::Firefly });
            guests.push_back(TerrariumGuest{ seed++, Habit::Stays, false, CreatureId::Bonfire });
            int encounters = 0;
            for (double passed = 0.0; passed < 600.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    encounters += resident.Encounter == TerrariumEncounter::Greeting || resident.Encounter == TerrariumEncounter::Chasing ? 1 : 0;
                    if (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving)
                    {
                        CHECK(resident.X >= -1.0f && resident.X <= size.Width - size.SpriteWidth + 1.0f);
                        CHECK(resident.Y >= -1.0f && resident.Y <= size.Height - size.SpriteHeight + 1.0f);
                    }
                    if (resident.Partner != 0)
                    {
                        const auto partner = std::find_if(box.Residents().begin(), box.Residents().end(), [&](const TerrariumResident& one) { return one.Seed == resident.Partner; });
                        CHECK(partner != box.Residents().end());
                    }
                }
            }
            CHECK(encounters > 0);
            guests.clear();
            for (double passed = 0.0; passed < 60.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Residents().empty());
        }
    }

    void TestEvolutions()
    {
        CHECK(EvolutionOf(CreatureId::Slime) != nullptr);
        CHECK(std::size(kEvolutions) == kCreatureCount);
        CHECK(!EvolvesAt(CreatureId::Slime, kEvolveAtBond - 1));
        CHECK(EvolvesAt(CreatureId::Slime, kEvolveAtBond));
        CHECK(EvolvesAt(CreatureId::Robot, kMaxBondLevel));
        CHECK(!EvolvesAt(CreatureId::Crab, kEvolveAtBond - 1));
        for (std::size_t index = 0; index < std::size(kEvolutions); ++index)
        {
            const CreatureEvolution& evolution = kEvolutions[index];
            CHECK(index == 0 || kEvolutions[index - 1].Id < evolution.Id);
            CHECK(std::string_view(evolution.Name).find(InfoOf(evolution.Id).Name) != std::string_view::npos);
            CHECK(std::string_view(evolution.Flavour).size() > 0);
        }

        {
            const auto file = TestFile("-evolutions.ini");
            Settings saved;
            saved.Creatures.push_back({ "slime", 0u, 30u, true, false });
            saved.Creatures.push_back({ "cat", 1730000000u, 2u, true, true });
            saved.Creatures.push_back({ "frog", 1730000000u, 5u });
            CHECK(saved.Save(file));
            Settings loaded;
            loaded.Load(file);
            CHECK(loaded.Creatures.size() == 3);
            CHECK(loaded.Creatures.size() == 3 && loaded.Creatures[0].Evolved && !loaded.Creatures[0].FirstForm && loaded.Creatures[0].Pets == 30);
            CHECK(loaded.Creatures.size() == 3 && loaded.Creatures[1].Evolved && loaded.Creatures[1].FirstForm);
            CHECK(loaded.Creatures.size() == 3 && !loaded.Creatures[2].Evolved && !loaded.Creatures[2].FirstForm && loaded.Creatures[2].Pets == 5);
            std::filesystem::remove(file);
        }

        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto guest = [](std::uint32_t evolutions) { return TerrariumGuest{ 1, Habit::Walks, false, CreatureId::Slime, kEvolveAtBond, evolutions }; };

        {
            Terrarium box(5u);
            box.Update({ guest(1) }, size, kStep, true);
            box.Update({ guest(1) }, size, kStep, true);
            CHECK(box.Residents().size() == 1 && box.Residents()[0].Reaction == TerrariumReaction::None);
        }

        {
            Terrarium box(6u);
            box.Update({ guest(0) }, size, kStep, false);
            const TerrariumResident& slime = box.Residents()[0];
            box.Update({ guest(1) }, size, kStep, true);
            CHECK(slime.Reaction == TerrariumReaction::Evolving);
            int first = 0;
            int shown = 0;
            int turns = 0;
            bool last = true;
            bool petted = false;
            for (double passed = 0.0; passed < 1.65; passed += kStep)
            {
                box.Update({ guest(1) }, size, kStep, true);
                (slime.ShowsFirstForm ? first : shown) += 1;
                turns += slime.ShowsFirstForm != last ? 1 : 0;
                last = slime.ShowsFirstForm;
                if (!petted && passed > 0.5)
                {
                    box.Pet(1);
                    petted = true;
                }
            }
            CHECK(first > 0 && shown > 0);
            CHECK(turns >= 6);
            CHECK(slime.Reaction == TerrariumReaction::Evolving);
            bool hopped = false;
            bool heart = false;
            for (double passed = 0.0; passed < 0.6; passed += kStep)
            {
                box.Update({ guest(1) }, size, kStep, true);
                hopped = hopped || slime.Lift > 0.5f;
                heart = heart || slime.Emote == TerrariumEmote::Heart;
                CHECK(!slime.ShowsFirstForm);
            }
            CHECK(hopped && heart);
            for (double passed = 0.0; passed < 1.0; passed += kStep)
                box.Update({ guest(1) }, size, kStep, true);
            CHECK(slime.Reaction == TerrariumReaction::None && !slime.ShowsFirstForm && slime.Lift == 0.0f);
        }

        {
            Terrarium box(7u);
            box.Update({ guest(0) }, size, kStep, false);
            box.Update({ guest(1) }, size, kStep, false);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None && !box.Residents()[0].ShowsFirstForm);

            Terrarium held(8u);
            held.Update({ guest(0) }, size, kStep, false);
            held.Update({ guest(1) }, size, kStep, true);
            CHECK(held.Residents()[0].Reaction == TerrariumReaction::Evolving);
            held.Grab(1);
            held.Update({ guest(1) }, size, kStep, true);
            CHECK(!held.Residents()[0].ShowsFirstForm);
        }
    }

    void TestCharacters()
    {
        CHECK(CharacterOf(CreatureId::Crab).Throw == ThrowManner::Loves);
        CHECK(CharacterOf(CreatureId::Ghost).Throw == ThrowManner::Slips);
        CHECK(CharacterOf(CreatureId::Slime).Throw == ThrowManner::Splats);
        CHECK(CharacterOf(CreatureId::Dragon).Throw == ThrowManner::Furious);
        CHECK(CharacterOf(CreatureId::Mimic).Pet == PetManner::Snaps);
        CHECK(CharacterOf(CreatureId::Cat).Pet == PetManner::Moody);
        CHECK(CharacterOf(CreatureId::Pufferfish).Pet == PetManner::Touchy);
        CHECK(CharacterOf(CreatureId::Capybara).Pet == PetManner::Unmoved && CharacterOf(CreatureId::Capybara).Throw == ThrowManner::Unmoved);
        CHECK(CharacterOf(CreatureId::Sloth).Pet == PetManner::Unmoved);

        CHECK(OutcomeOfPet(CreatureId::Slime, 0, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Bunny, 0, 1) == PetOutcome::Dodge);
        CHECK(OutcomeOfPet(CreatureId::Bunny, 1, 1) == PetOutcome::Dodge);
        CHECK(OutcomeOfPet(CreatureId::Bunny, 2, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Mimic, 0, 1) == PetOutcome::Bite);
        CHECK(OutcomeOfPet(CreatureId::Mimic, 3, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Pufferfish, 0, 1) == PetOutcome::Puff);
        CHECK(OutcomeOfPet(CreatureId::Pufferfish, 4, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Capybara, 5, 1) == PetOutcome::Shrug);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 3) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 4) == PetOutcome::Bite);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 5) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Cat, 4, 4) == PetOutcome::Joy);
        CHECK(ThrowMannerOf(CreatureId::Bunny, 0) == ThrowManner::Flees);
        CHECK(ThrowMannerOf(CreatureId::Bunny, 3) == ThrowManner::Unmoved);
        CHECK(ThrowMannerOf(CreatureId::Dragon, 0) == ThrowManner::Furious);
        CHECK(ThrowMannerOf(CreatureId::Dragon, 4) == ThrowManner::Hates);
        CHECK(ThrowMannerOf(CreatureId::Crab, 5) == ThrowManner::Loves);

        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
            {
                for (double passed = 0.0; passed < seconds; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };
        const auto walker = [](CreatureId id, int bond = 0, bool asleep = false) { return TerrariumGuest{ 1, Habit::Walks, asleep, id, bond }; };
        const auto throwIt = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests)
            {
                box.Grab(1);
                box.Carry(1, 100.0f, 10.0f);
                box.Drop(1, size);
                for (double passed = 0.0; passed < 3.0 && box.Residents()[0].State == ResidentState::Settling; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Crab) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 20.0f, 70.0f);
            throwIt(box, guests);
            const TerrariumResident& crab = box.Residents()[0];
            CHECK(crab.Reaction == TerrariumReaction::Eager);
            CHECK(crab.Squash > 0.0f);
            float highest = 0.0f;
            for (double passed = 0.0; passed < 7.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                highest = std::max(highest, crab.Lift);
            }
            CHECK(std::fabs(crab.X - 20.0f) <= 5.0f);
            CHECK(highest > 1.0f);
            CHECK(crab.Emote == TerrariumEmote::Note);
            box.SetPointer(true, 150.0f, 70.0f);
            run(box, guests, 0.5);
            CHECK(crab.Reaction == TerrariumReaction::Eager);
            run(box, guests, 3.0);
            CHECK(crab.Reaction == TerrariumReaction::None);
            CHECK(crab.Lift == 0.0f && crab.Emote == TerrariumEmote::None && crab.Squash == 0.0f);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Bunny) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 100.0f, 70.0f);
            throwIt(box, guests);
            const TerrariumResident& bunny = box.Residents()[0];
            CHECK(bunny.Reaction == TerrariumReaction::Scared);
            run(box, guests, 4.0);
            CHECK(std::fabs(bunny.X - 100.0f) > 40.0f);
            CHECK(bunny.Emote == TerrariumEmote::Alarm);

            const std::vector<TerrariumGuest> trusting{ walker(CreatureId::Bunny, 3) };
            Terrarium other(4u);
            other.Update(trusting, size, kStep, false);
            other.SetPointer(true, 100.0f, 70.0f);
            throwIt(other, trusting);
            CHECK(other.Residents()[0].Reaction == TerrariumReaction::Sulking);
        }

        {
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Floats, false, CreatureId::Ghost, 0 } };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.Grab(1);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            run(box, guests, 0.3);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            CHECK(box.Residents()[0].Emote == TerrariumEmote::Question);
            run(box, guests, 0.5);
            CHECK(box.Residents()[0].State != ResidentState::Held);
            box.Carry(1, 5.0f, 5.0f);
            box.Drop(1, size);
            run(box, guests, 6.0);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Slime) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            throwIt(box, guests);
            const TerrariumResident& slime = box.Residents()[0];
            CHECK(slime.Reaction == TerrariumReaction::Sulking);
            CHECK(slime.Squash > 0.9f);
            const float x = slime.X;
            run(box, guests, 3.0);
            CHECK(slime.X == x);
            CHECK(slime.Squash > 0.0f && slime.Squash < 0.9f);
            run(box, guests, 8.0);
            CHECK(slime.Reaction == TerrariumReaction::None && slime.Squash == 0.0f);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Hamster) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            throwIt(box, guests);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Dizzy);
            bool wobbled = false;
            for (double passed = 0.0; passed < 2.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                wobbled |= box.Residents()[0].ShiftX != 0.0f;
                CHECK(box.Residents()[0].Emote == TerrariumEmote::Stars);
            }
            CHECK(wobbled);

            const std::vector<TerrariumGuest> angry{ walker(CreatureId::Dragon) };
            Terrarium other(4u);
            other.Update(angry, size, kStep, false);
            other.SetPointer(true, 30.0f, 70.0f);
            throwIt(other, angry);
            CHECK(other.Residents()[0].Reaction == TerrariumReaction::Furious);
            run(other, angry, 4.0);
            CHECK(std::fabs(other.Residents()[0].X - 30.0f) <= 5.0f);
            CHECK(other.Residents()[0].Emote == TerrariumEmote::Anger);
        }

        {
            const std::vector<TerrariumGuest> shy{ walker(CreatureId::Bunny) };
            Terrarium box(4u);
            box.Update(shy, size, kStep, false);
            box.SetPointer(true, box.Residents()[0].X, box.Residents()[0].Y);
            const float x = box.Residents()[0].X;
            CHECK(box.Pet(1) == PetOutcome::Dodge);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Dodging);
            run(box, shy, 1.0);
            CHECK(std::fabs(box.Residents()[0].X - x) > 5.0f);
            CHECK(box.Residents()[0].Emote == TerrariumEmote::Dots);
            run(box, shy, 1.0);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None);

            const std::vector<TerrariumGuest> moody{ walker(CreatureId::Cat) };
            Terrarium cat(4u);
            cat.Update(moody, size, kStep, false);
            CHECK(cat.Pet(1) == PetOutcome::Joy);
            CHECK(cat.Pet(1) == PetOutcome::Joy);
            CHECK(cat.Pet(1) == PetOutcome::Joy);
            CHECK(cat.Pet(1) == PetOutcome::Bite);
            CHECK(cat.Residents()[0].Reaction == TerrariumReaction::Snapping);
            run(cat, moody, 13.0);
            CHECK(cat.Pet(1) == PetOutcome::Joy);

            const std::vector<TerrariumGuest> happy{ walker(CreatureId::Slime) };
            Terrarium slime(4u);
            slime.Update(happy, size, kStep, false);
            CHECK(slime.Pet(1) == PetOutcome::Joy);
            run(slime, happy, 0.15);
            CHECK(slime.Residents()[0].Lift > 0.5f && slime.Residents()[0].Emote == TerrariumEmote::Heart);
            run(slime, happy, 1.0);
            CHECK(slime.Residents()[0].Reaction == TerrariumReaction::None && slime.Residents()[0].Lift == 0.0f);

            CHECK(slime.Pet(99) == PetOutcome::Joy);
            slime.Grab(1);
            CHECK(slime.Pet(1) == PetOutcome::Joy);
            CHECK(slime.Residents()[0].State == ResidentState::Held);
            slime.Drop(1, size);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Crab, 0, true) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 20.0f, 70.0f);
            throwIt(box, guests);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Eager);
            run(box, guests, 4.0);
            CHECK(std::fabs(box.Residents()[0].X - 20.0f) <= 5.0f);
            run(box, guests, 8.0);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None);
            CHECK(box.Residents()[0].State == ResidentState::Resting);
        }

        {
            const std::vector<TerrariumGuest> friendly{ walker(CreatureId::Bunny, 4) };
            Terrarium box(4u);
            box.Update(friendly, size, kStep, false);
            box.SetPointer(true, 60.0f, 70.0f);
            bool visited = false;
            for (double passed = 0.0; passed < 120.0 && !visited; passed += kStep)
            {
                box.Update(friendly, size, kStep, true);
                visited = box.Residents()[0].Reaction == TerrariumReaction::Visiting;
            }
            CHECK(visited);
            run(box, friendly, 5.0);
            CHECK(std::fabs(box.Residents()[0].X - 60.0f) <= 5.0f);

            const std::vector<TerrariumGuest> strange{ walker(CreatureId::Bunny, 3) };
            Terrarium other(4u);
            other.Update(strange, size, kStep, false);
            other.SetPointer(true, 60.0f, 70.0f);
            for (double passed = 0.0; passed < 120.0; passed += kStep)
            {
                other.Update(strange, size, kStep, true);
                CHECK(other.Residents()[0].Reaction != TerrariumReaction::Visiting);
            }

            Terrarium away(4u);
            away.Update(friendly, size, kStep, false);
            for (double passed = 0.0; passed < 120.0; passed += kStep)
            {
                away.Update(friendly, size, kStep, true);
                CHECK(away.Residents()[0].Reaction != TerrariumReaction::Visiting);
            }
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Cat) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, box.Residents()[0].X + 20.0f, 70.0f);
            box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].State != ResidentState::Resting || box.Residents()[0].Direction == 1);
            box.SetPointer(true, box.Residents()[0].X - 20.0f, 70.0f);
            box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].State != ResidentState::Resting || box.Residents()[0].Direction == -1);
        }

        for (const CreatureInfo& info : AllCreatures())
        {
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, info.Id, 0 } };
            Terrarium box(info.Key[0]);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 60.0f, 60.0f);
            box.Pet(1);
            run(box, guests, 2.0);
            throwIt(box, guests);
            run(box, guests, 12.0);
            const TerrariumResident& resident = box.Residents()[0];
            CHECK(resident.Reaction == TerrariumReaction::None || resident.Reaction == TerrariumReaction::Visiting);
            CHECK(resident.X >= 0.0f && resident.X <= size.Width - size.SpriteWidth);
            CHECK(resident.Y >= 0.0f && resident.Y <= size.Height - size.SpriteHeight);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Slime) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.Pet(1);
            box.Update(guests, size, kStep, false);
            CHECK(box.Residents()[0].Emote == TerrariumEmote::Heart && box.Residents()[0].Lift == 0.0f);
            for (double passed = 0.0; passed < 1.0; passed += kStep)
                box.Update(guests, size, kStep, false);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None && box.Residents()[0].Emote == TerrariumEmote::None);
        }
    }

    void TestSteamRegistry()
    {
        const std::string text = R"("Registry"
{
    "HKCU"
    {
        "Software"
        {
            "Valve"
            {
                "Steam"
                {
                    "language"      "english"
                    "RunningAppID"  "0"
                    "ActiveProcess"
                    {
                        "pid"               "4242"
                        "SteamClientDll"    "/home/x/.steam/ubuntu12_32/steamclient.so"
                        "SteamClientDll64"  "/home/x/.steam/linux64/steamclient.so"
                        "ActiveUser"        "12345"
                    }
                    "apps"
                    {
                        "620"   { "installed" "1" "Running" "1" }
                        "730"   { "installed" "1" "Running" "0" }
                        "480"   { "installed" "0" }
                        "name"  { "Running" "1" }
                    }
                }
            }
        }
    }
})";
        const SteamRegistry registry = ParseSteamRegistry(text);
        CHECK(registry.ProcessId == 4242);
        CHECK(registry.Account == 12345);
        CHECK(registry.ClientLibrary == "/home/x/.steam/linux64/steamclient.so");
        CHECK(registry.RunningApps.size() == 1);
        CHECK(!registry.RunningApps.empty() && registry.RunningApps[0] == 620);

        const SteamRegistry nothing = ParseSteamRegistry("");
        CHECK(nothing.ProcessId == 0 && nothing.Account == 0 && nothing.ClientLibrary.empty() && nothing.RunningApps.empty());
        const SteamRegistry damaged = ParseSteamRegistry("\"Registry\" { \"ActiveProcess\" { \"pid\" \"not a number\" ");
        CHECK(damaged.ProcessId == 0);

        const SteamRegistry twice = ParseSteamRegistry(R"("Registry"
{
    "HKCU" { "Software" { "Valve" { "Steam" { "ActiveProcess" { "pid" "4242" "ActiveUser" "12345" "SteamClientDll64" "/x/steamclient.so" } } } } }
    "HKLM" { "Software" { "Valve" { "Steam" { "ActiveProcess" { "SteamClientDll64" "" } } } } }
})");
        CHECK(twice.ProcessId == 4242 && twice.Account == 12345 && twice.ClientLibrary == "/x/steamclient.so");

        using namespace std::string_view_literals;
        CHECK(SteamAppIdOfEnvironment("HOME=/home/x\0SteamAppId=620\0SteamGameId=620\0"sv) == 620u);
        CHECK(SteamAppIdOfEnvironment("SteamAppId=480"sv) == 480u);
        CHECK(SteamAppIdOfEnvironment("PATH=/bin\0NotSteamAppId=620\0"sv) == 0u);
        CHECK(SteamAppIdOfEnvironment("SteamAppId=\0"sv) == 0u);
        CHECK(SteamAppIdOfEnvironment("SteamAppId=12x\0"sv) == 0u);
        CHECK(SteamAppIdOfEnvironment(""sv) == 0u);
    }

    void TestLocalTime()
    {
        std::tm local{};
        CHECK(LocalTime(std::time_t{ 1700000000 }, local));
        CHECK(local.tm_year == 123);
        CHECK(local.tm_mon == 10);
    }

    void TestVariedTimes()
    {
        CHECK(DrawGoalFactor(0.25f, 0.0) == 0.75f);
        CHECK(DrawGoalFactor(0.25f, 0.5) == 1.0f);
        CHECK(DrawGoalFactor(0.25f, 1.0) == 1.25f);
        CHECK(DrawRunSeconds(0.0) == 40.0 * 60.0 && DrawRunSeconds(1.0) == 120.0 * 60.0);
        CHECK(DrawRestSeconds(0.0) == 5.0 * 60.0 && DrawRestSeconds(1.0) == 25.0 * 60.0);

        QueueEntry entry = Entry(1, GoalKind::ForHours, 2.0f, 3600.0);
        entry.GoalFactor = 1.5f;
        CHECK(SecondsLeft(entry) == 2.0 * 3600.0);
        CHECK(!IsFinished(entry));
        entry.IdledSeconds = 3.0 * 3600.0;
        CHECK(IsFinished(entry));
        QueueEntry total = Entry(1, GoalKind::UntilTotal, 2.0f, 0.0, 90);
        total.GoalFactor = 0.5f;
        CHECK(!IsFinished(total));
        CHECK(SecondsLeft(total) == 15.0 * 60.0);
        CHECK(GoalSeconds(total) == 105.0 * 60.0);
        QueueEntry nearly = Entry(1, GoalKind::UntilTotal, 100.0f, 0.0, 99 * 60);
        nearly.GoalFactor = 0.75f;
        CHECK(!IsFinished(nearly));
        CHECK(SecondsLeft(nearly) == 0.75 * 3600.0);
        nearly.GoalFactor = 1.25f;
        CHECK(SecondsLeft(nearly) == 1.25 * 3600.0);
        nearly.IdledSeconds = 1.25 * 3600.0;
        CHECK(IsFinished(nearly));
        QueueEntry past = Entry(1, GoalKind::UntilTotal, 100.0f, 0.0, 120 * 60);
        past.GoalFactor = 1.25f;
        CHECK(IsFinished(past));
        CHECK(SecondsLeft(past) == 0.0);
        CHECK(Progress(past) == 1.0f);

        QueueEntry rhythm = Entry(1);
        AdvanceRhythm(rhythm, 100.0, true, 0.0);
        CHECK(rhythm.RunUntil == 100.0 + 40.0 * 60.0 && !rhythm.Resting);
        AdvanceRhythm(rhythm, 200.0, true, 1.0);
        CHECK(rhythm.RunUntil == 100.0 + 40.0 * 60.0 && !rhythm.Resting);
        AdvanceRhythm(rhythm, 100.0 + 40.0 * 60.0, true, 0.0);
        CHECK(rhythm.Resting && rhythm.RunUntil == 0.0);
        const double back = rhythm.RestUntil;
        CHECK(back == 100.0 + 45.0 * 60.0);
        AdvanceRhythm(rhythm, back - 1.0, false, 0.5);
        CHECK(rhythm.Resting);
        AdvanceRhythm(rhythm, back, false, 0.5);
        CHECK(!rhythm.Resting);
        AdvanceRhythm(rhythm, back + 1.0, true, 1.0);
        CHECK(rhythm.RunUntil == back + 1.0 + 120.0 * 60.0);
        QueueEntry waiting = Entry(2);
        AdvanceRhythm(waiting, 500.0, false, 0.5);
        CHECK(waiting.RunUntil == 0.0 && !waiting.Resting);
        QueueEntry pushedOut = Entry(4);
        AdvanceRhythm(pushedOut, 100.0, true, 0.0);
        CHECK(pushedOut.RunUntil == 100.0 + 40.0 * 60.0);
        AdvanceRhythm(pushedOut, 200.0, false, 0.5);
        CHECK(pushedOut.RunUntil == 0.0 && !pushedOut.Resting);
        AdvanceRhythm(pushedOut, 100.0 + 90.0 * 60.0, true, 0.0);
        CHECK(!pushedOut.Resting && pushedOut.RunUntil == 100.0 + 130.0 * 60.0);
        pushedOut.RunUntil = 0.0;
        pushedOut.RestUntil = 9000.0;
        AdvanceRhythm(pushedOut, 8000.0, false, 0.5);
        CHECK(pushedOut.Resting && pushedOut.RestUntil == 9000.0);

        CHECK(!AllOnBreak({}));
        QueueEntry resting = Entry(5);
        resting.Resting = true;
        QueueEntry runnable = Entry(6);
        QueueEntry done = Entry(7, GoalKind::ForHours, 1.0f, 7200.0);
        QueueEntry failed = Entry(8);
        failed.Failed = true;
        QueueEntry held = Entry(9);
        held.GuardSecondsLeft = 0.0;
        CHECK(AllOnBreak({ resting }));
        CHECK(AllOnBreak({ done, resting, failed, held }));
        CHECK(!AllOnBreak({ resting, runnable }));
        CHECK(!AllOnBreak({ done, failed, held }));
        QueueEntry farming = Entry(3, GoalKind::CardDrops);
        farming.RunUntil = 1.0;
        farming.RestUntil = 900.0;
        AdvanceRhythm(farming, 500.0, true, 0.5);
        CHECK(!farming.Resting && farming.RunUntil == 0.0 && farming.RestUntil == 0.0);

        std::vector<QueueEntry> queue = { Entry(1), Entry(2), Entry(3) };
        queue[0].Resting = true;
        CHECK((WantedRunning(queue, 2) == std::vector<std::uint32_t>{ 2, 3 }));
        queue[1].Resting = true;
        queue[2].Resting = true;
        CHECK(WantedRunning(queue, 2).empty());
        CHECK(HasWorkLeft(queue));
    }

    void TestShownGame()
    {
        std::vector<QueueEntry> queue = { Entry(1), Entry(2), Entry(3), Entry(4) };
        CHECK((WantedWithShown(queue, 2, 0) == std::vector<std::uint32_t>{ 1, 2 }));
        CHECK((WantedWithShown(queue, 2, 1) == std::vector<std::uint32_t>{ 1, 2 }));
        CHECK((WantedWithShown(queue, 2, 4) == std::vector<std::uint32_t>{ 1, 4 }));
        CHECK((WantedWithShown(queue, 1, 4) == std::vector<std::uint32_t>{ 4 }));
        CHECK((WantedWithShown(queue, 8, 4) == std::vector<std::uint32_t>{ 1, 2, 3, 4 }));
        CHECK((WantedWithShown(queue, 2, 99) == std::vector<std::uint32_t>{ 1, 2 }));
        queue[3].Failed = true;
        CHECK((WantedWithShown(queue, 2, 4) == std::vector<std::uint32_t>{ 1, 2 }));
    }

    void TestCardPages()
    {
        const std::string loggedIn = R"(<script>g_steamID = "76561198000000000";</script>)";
        const auto row = [](const std::string& href, const std::string& drops)
            {
                return R"(<div class="badge_row is_link"><a class="badge_row_overlay" href="https://steamcommunity.com/id/x)" + href + R"("></a>)" +
                    R"(<div class="badge_title_stats_drops">)" + drops + "</div></div>";
            };

        const std::string page = loggedIn +
            row("/gamecards/730/", R"(<span class="progress_info_bold">3 card drops remaining</span>)") +
            row("/gamecards/440/", R"(<span class="progress_info_bold">1 card drop remaining</span>)") +
            row("/gamecards/570/", R"(<span class="progress_info_bold">No card drops remaining</span>)") +
            row("/gamecards/730/?border=1", R"(<span class="progress_info_bold">9 card drops remaining</span>)") +
            row("/gamecards/620/", R"(<span class="progress_info_bold">Something else</span>)") +
            row("/gamecards/630/", "") +
            R"(<a class="badge_row_overlay" href="https://steamcommunity.com/id/x/badges/1"></a>)" +
            R"(<a class="pagelink" href="?p=2">2</a><a class="pagelink" href="?p=12">12</a>)";
        const BadgePage parsed = ParseBadgePage(page);
        CHECK(parsed.LoggedIn);
        CHECK(parsed.LastPage == 12);
        CHECK(parsed.Games.size() == 3);
        if (parsed.Games.size() == 3)
        {
            CHECK(parsed.Games[0].AppId == 730 && parsed.Games[0].Drops == 3);
            CHECK(parsed.Games[1].AppId == 440 && parsed.Games[1].Drops == 1);
            CHECK(parsed.Games[2].AppId == 570 && parsed.Games[2].Drops == 0);
        }

        CHECK(ParseBadgePage(R"(<a class="pagelink" href="?l=english&p=7">7</a>)").LastPage == 7);
        CHECK(ParseBadgePage(R"(<a class="pagelink" href="?l=english&amp;p=9">9</a>)").LastPage == 9);
        CHECK(ParseBadgePage(R"(<a href="/search?q=3">no page</a>)").LastPage == 1);

        const BadgePage visitor = ParseBadgePage(R"(<script>g_steamID = false;</script>)" + row("/gamecards/730/", ""));
        CHECK(!visitor.LoggedIn && visitor.Games.empty() && visitor.LastPage == 1);
        CHECK(ParseBadgePage("").Games.empty());

        CHECK(ParseCardPage(loggedIn + R"(<span class="progress_info_bold">2 card drops remaining</span>)") == 2u);
        CHECK(ParseCardPage(loggedIn + R"(<span class="progress_info_bold">No card drops remaining</span>)") == 0u);
        CHECK(!ParseCardPage(loggedIn + "<span>nothing</span>").has_value());
        CHECK(!ParseCardPage(R"(g_steamID = false; <span class="progress_info_bold">2 card drops remaining</span>)").has_value());

    }

    void TestMarkGamesWithCards()
    {
        std::vector<Game> games(3);
        games[0].AppId = 1;
        games[0].HasCards = true;
        games[1].AppId = 2;
        games[2].AppId = 3;

        CHECK(MarkGamesWithCards(games, { { 2, 0 }, { 99, 5 } }));
        CHECK(games[0].HasCards && games[1].HasCards && !games[2].HasCards);
        CHECK(!MarkGamesWithCards(games, { { 2, 0 } }));
        CHECK(!MarkGamesWithCards(games, {}));
        CHECK(games[0].HasCards && games[1].HasCards && !games[2].HasCards);
    }

    void TestCardSet()
    {
        const std::string_view page = R"(<div class="badge_card_set_cards">

	<div class="badge_card_set_card owned">
		<div data-panel="{&quot;focusable&quot;:true,&quot;clickOnActivate&quot;:true}" role="button" onclick="" class="game_card_ctn ">
			<img class="gamecard" src="https://community.fastly.steamstatic.com/economy/image/IzMF03bk9-Wp_Sb/96fx96f">
		</div>
		<div class="badge_card_set_text badge_card_set_title ellipsis">
			<div class="badge_card_set_text_qty">(4)</div>
			Anarchist										<div style="clear: right"></div>
		</div>
		<div class="badge_card_set_text ellipsis">
			1 of 5, Series 1																			</div>
	</div>
	<div class="badge_card_set_card unowned">
		<div class="game_card_ctn "><img class="gamecard" src="https://community.fastly.steamstatic.com/economy/image/Second"></div>
		<div class="badge_card_set_text badge_card_set_title ellipsis">
			Heavy &amp; Medic&#39;s Card										<div style="clear: right"></div>
		</div>
		<div class="badge_card_set_text ellipsis">2 of 5, Series 1</div>
	</div>
	<div class="badge_card_set_card owned">
		<div class="game_card_ctn "><img class="gamecard" src="https://example.com/economy/image/Third"></div>
		<div class="badge_card_set_text badge_card_set_title ellipsis">
			No number										<div style="clear: right"></div>
		</div>
	</div>
</div><div class="badge_card_set_text_qty">(9)</div>)";

        const std::vector<GameCard> cards = ParseCardSet(page);
        CHECK(cards.size() == 3);
        if (cards.size() == 3)
        {
            CHECK(cards[0].Name == "Anarchist" && cards[0].Owned == 4);
            CHECK(cards[0].Picture == "https://community.fastly.steamstatic.com/economy/image/IzMF03bk9-Wp_Sb/96fx96f");
            CHECK(cards[1].Name == "Heavy & Medic's Card" && cards[1].Owned == 0);
            CHECK(!cards[1].Picture.empty());
            CHECK(cards[2].Name == "No number" && cards[2].Owned == 1 && cards[2].Picture.empty());
        }
        CHECK(ParseCardSet("<div class=\"badge_card_set_cards\"></div>").empty());

        CHECK(IsCardPicture("https://community.fastly.steamstatic.com/economy/image/Ab-c_1/96fx96f"));
        CHECK(IsCardPicture("https://community.akamai.steamstatic.com/economy/image/x"));
        CHECK(!IsCardPicture("http://community.fastly.steamstatic.com/economy/image/x"));
        CHECK(!IsCardPicture("https://steamstatic.com.example.org/economy/image/x"));
        CHECK(!IsCardPicture("https://.steamstatic.com/economy/image/x"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/other/x"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/economy/image/"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/economy/image/../../x"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/economy/image/x?y=1"));
        CHECK(!IsCardPicture(""));
    }

    void TestCardLog()
    {
        constexpr std::uint64_t kOne = 76561198000000001ull;
        constexpr std::uint64_t kTwo = 76561198000000002ull;
        const auto set = [](std::uint32_t a, std::uint32_t b)
            {
                return std::vector<GameCard>{ { "A|1", "https://x.steamstatic.com/economy/image/a", a }, { "B", "https://x.steamstatic.com/economy/image/b", b } };
            };

        CardLog log;
        CHECK(!log.Knows(kOne, 10));

        CHECK(log.NotePage(kOne, 10, 3, set(1, 0), 100));
        CHECK(log.Knows(kOne, 10) && !log.Knows(kTwo, 10) && !log.Knows(kOne, 11));
        CHECK(log.Received().empty());

        CHECK(!log.NotePage(kOne, 10, 3, set(1, 0), 200));
        CHECK(log.Received().empty());

        CHECK(log.NotePage(kOne, 10, 2, set(1, 1), 300));
        CHECK(log.Received().size() == 1);
        CHECK(log.Received().back().Name == "B" && log.Received().back().Time == 300 && log.Received().back().AppId == 10 && log.Received().back().Account == kOne);
        CHECK(log.Received().back().Picture == "https://x.steamstatic.com/economy/image/b");

        CHECK(log.NotePage(kOne, 10, 2, set(2, 1), 400));
        CHECK(log.Received().size() == 1);

        CHECK(log.NotePage(kOne, 10, 0, set(5, 1), 500));
        CHECK(log.Received().size() == 3);
        CHECK(log.Received()[1].Name == "A|1" && log.Received()[2].Name == "A|1");

        CardLog foil;
        foil.NotePage(kOne, 20, 1, set(1, 1), 100);
        CHECK(foil.NotePage(kOne, 20, 0, set(1, 1), 200));
        CHECK(foil.Received().size() == 1 && foil.Received()[0].Name.empty() && foil.Received()[0].Picture.empty());

        CardLog crafted;
        crafted.NotePage(kOne, 30, 2, set(3, 3), 100);
        crafted.NotePage(kOne, 30, 2, set(2, 2), 200);
        CHECK(crafted.Received().empty());

        CardLog other;
        other.NotePage(kOne, 1, 5, set(0, 0), 100);
        other.NotePage(kOne, 2, 3, set(0, 0), 100);
        other.NotePage(kOne, 3, 2, set(0, 0), 100);
        other.NotePage(kTwo, 2, 9, set(0, 0), 100);
        std::vector<std::uint32_t> fallen = other.Fallen(kOne, { { 1, 5 }, { 2, 2 } });
        std::sort(fallen.begin(), fallen.end());
        CHECK((fallen == std::vector<std::uint32_t>{ 2, 3 }));
        CHECK(other.KnownDrops(kTwo).size() == 1 && other.KnownDrops(kTwo).at(2) == 9);

        CardLog restored = CardLog::FromText(log.ToText());
        CHECK(restored.ToText() == log.ToText());
        CHECK(restored.Received().size() == 3 && restored.Received()[1].Name == "A|1");
        CHECK(restored.Received()[1].Picture == "https://x.steamstatic.com/economy/image/a");
        CHECK(restored.Knows(kOne, 10));
        CHECK(!restored.NotePage(kOne, 10, 0, set(5, 1), 600));

        const CardLog damaged = CardLog::FromText("Card|1\nnonsense\n\nSet|\nCard|1|2|3|https://example.com/economy/image/x|Name\r\n");
        CHECK(damaged.Received().size() == 1 && damaged.Received()[0].Name == "Name" && damaged.Received()[0].Picture.empty());
    }

    void TestSettings()
    {
        const std::filesystem::path path = TestFile(".ini");

        Settings saved;
        saved.X = -40;
        saved.Y = 120;
        saved.Placed = true;
        saved.Slots = 7;
        saved.DefaultGoal = { GoalKind::UntilTotal, 2.5f };
        saved.PauseWhilePlaying = false;
        saved.KeepInTray = false;
        saved.ResumeOnStart = true;
        saved.WasActive = true;
        StoredEntry first;
        first.Account = 42;
        first.Entry = Entry(730, GoalKind::ForHours, 1.5f, 1234.0, 60);
        StoredEntry second;
        second.Account = 43;
        second.List = "Night shift";
        second.Entry = Entry(440);
        second.Entry.IgnoreRefund = true;
        second.Entry.GoalFactor = 1.125f;
        first.Entry.Goal = { GoalKind::CardDrops, 1.5f };
        saved.Queue = { first, second };
        saved.Lists = { { 42, "Queue" }, { 43, "Night shift" }, { 43, "Empty one" } };
        saved.ActiveLists = { { 43, "Night shift" } };
        saved.KeepAwake = false;
        saved.RefundGuard = false;
        saved.OnlyWhenAway = true;
        saved.AwayMinutes = 12.5f;
        saved.OnlyInHours = true;
        saved.FromHour = 23;
        saved.ToHour = 6;
        saved.InvisibleWhileIdling = true;
        saved.SetInvisible = true;
        saved.VaryTimes = true;
        saved.VarySpread = 30.0f;
        saved.CardWarmupHours = 2.5f;
        saved.ShowTerrarium = false;
        saved.ShowLog = true;
        saved.Season = SceneSeason::Autumn;
        saved.LibraryCovers = true;
        saved.QueueCovers = true;
        saved.CreatureHome = true;
        saved.HomeBall = true;
        saved.CreaturesTugMouse = false;
        saved.Width = 1400;
        saved.Height = 900;
        saved.Maximised = true;
        saved.Creatures.push_back({ "frog", 1730000000u, 12u });
        saved.Creatures.push_back({ "crab", 0u });
        saved.CreatureBuddy = "crab";
        saved.CreatureGoals = 3;
        saved.CreatureBreaks = 7;
        saved.CreaturePets = 42;
        saved.CreaturePlays.push_back({ "frog", { 75, 20400, 12, 9, 4, { 31, 0, 0, 5 } } });
        saved.ShownGames = { { 42, 730 } };
        CHECK(saved.Save(path));

        Settings loaded;
        loaded.Load(path);
        CHECK(loaded.X == -40 && loaded.Y == 120 && loaded.Placed);
        CHECK(loaded.Slots == 7);
        CHECK(loaded.DefaultGoal.Kind == GoalKind::UntilTotal && loaded.DefaultGoal.Hours == 2.5f);
        CHECK(!loaded.PauseWhilePlaying && !loaded.KeepInTray && loaded.ResumeOnStart && loaded.WasActive);
        CHECK(loaded.Queue.size() == 2);
        if (loaded.Queue.size() == 2)
        {
            CHECK(loaded.Queue[0].Account == 42);
            CHECK(loaded.Queue[0].Entry.AppId == 730);
            CHECK(loaded.Queue[0].Entry.Goal.Kind == GoalKind::CardDrops && loaded.Queue[0].Entry.Goal.Hours == 1.5f);
            CHECK(loaded.Queue[0].Entry.GoalFactor == 1.0f && loaded.Queue[1].Entry.GoalFactor == 1.125f);
            CHECK(loaded.Queue[0].Entry.IdledSeconds == 1234.0 && loaded.Queue[0].Entry.BaselineMinutes == 60);
            CHECK(loaded.Queue[1].Account == 43 && loaded.Queue[1].Entry.Goal.Kind == GoalKind::None);
            CHECK(loaded.Queue[0].List == "Queue" && !loaded.Queue[0].Entry.IgnoreRefund);
            CHECK(loaded.Queue[1].List == "Night shift" && loaded.Queue[1].Entry.IgnoreRefund);
        }
        CHECK(loaded.Lists.size() == 3 && loaded.Lists[2].Account == 43 && loaded.Lists[2].Name == "Empty one");
        CHECK(loaded.ActiveLists.size() == 1 && loaded.ActiveLists[0].Name == "Night shift");
        CHECK(!loaded.KeepAwake && !loaded.RefundGuard);
        CHECK(loaded.OnlyWhenAway && loaded.AwayMinutes == 12.5f);
        CHECK(loaded.OnlyInHours && loaded.FromHour == 23 && loaded.ToHour == 6);
        CHECK(loaded.InvisibleWhileIdling && loaded.SetInvisible);
        CHECK(loaded.VaryTimes && loaded.VarySpread == 30.0f && loaded.CardWarmupHours == 2.5f);
        CHECK(loaded.Creatures.size() == 2 && loaded.Creatures[0].Pets == 12 && loaded.Creatures[1].Pets == 0);
        CHECK(loaded.Creatures.size() == 2 && loaded.Creatures[0].Key == "frog" && loaded.Creatures[0].Found == 1730000000u && loaded.Creatures[1].Key == "crab");
        CHECK(loaded.ShowLog && loaded.Season == SceneSeason::Autumn);
        CHECK(!Settings{}.ShowLog && Settings{}.Season == SceneSeason::ByDate);
        CHECK(SeasonIn(SceneSeason::ByDate, 3) == SceneSeason::Spring && SeasonIn(SceneSeason::ByDate, 5) == SceneSeason::Spring);
        CHECK(SeasonIn(SceneSeason::ByDate, 6) == SceneSeason::Summer && SeasonIn(SceneSeason::ByDate, 8) == SceneSeason::Summer);
        CHECK(SeasonIn(SceneSeason::ByDate, 9) == SceneSeason::Autumn && SeasonIn(SceneSeason::ByDate, 11) == SceneSeason::Autumn);
        CHECK(SeasonIn(SceneSeason::ByDate, 12) == SceneSeason::Winter && SeasonIn(SceneSeason::ByDate, 1) == SceneSeason::Winter);
        CHECK(SeasonIn(SceneSeason::Winter, 7) == SceneSeason::Winter);
        for (int season = 0; season < static_cast<int>(SceneSeason::Count); ++season)
            CHECK(std::string_view(SceneSeasonName(static_cast<SceneSeason>(season))).size() > 0);
        CHECK(!loaded.ShowTerrarium && loaded.LibraryCovers && loaded.QueueCovers && loaded.CreatureHome && loaded.HomeBall && !Settings{}.HomeBall && !loaded.CreaturesTugMouse && Settings{}.CreaturesTugMouse);
        CHECK(loaded.Width == 1400 && loaded.Height == 900 && loaded.Maximised);
        CHECK(loaded.CreatureBuddy == "crab" && loaded.CreatureGoals == 3 && loaded.CreatureBreaks == 7 && loaded.CreaturePets == 42);
        CHECK(loaded.CreaturePlays.size() == 1);
        if (loaded.CreaturePlays.size() == 1)
        {
            const PlayRecord& play = loaded.CreaturePlays[0].Record;
            CHECK(loaded.CreaturePlays[0].Key == "frog" && play.Minutes == 75 && play.Day == 20400 && play.MinutesThatDay == 12 && play.Games == 9 && play.Fetches == 4);
            CHECK(play.Best[0] == 31 && play.Best[1] == 0 && play.Best[3] == 5);
        }
        CHECK(loaded.ShownGames.size() == 1 && loaded.ShownGames[0].Account == 42 && loaded.ShownGames[0].AppId == 730);

        const Settings fresh;
        CHECK(fresh.KeepAwake && fresh.RefundGuard && !fresh.OnlyWhenAway && !fresh.OnlyInHours && !fresh.InvisibleWhileIdling && !fresh.SetInvisible && !fresh.VaryTimes);

        {
            FILE* file = OpenForWriting(path);
            CHECK(file != nullptr);
            if (file)
            {
                std::fputs("; comment\r\nSlots=9000\r\nGoalHours=-3\r\nPauseWhilePlaying=perhaps\r\nnonsense\r\nQueue=1 | 2\r\nQueue=x | 2 | none | 1 | 0 | 0\r\nQueue=1 | 0 | none | 1 | 0 | 0\r\nQueue=1 | 5 | none | 1 | 0 | 0\r\nFromHour=99\r\nList=x | name\r\nList=1 | \r\n", file);
                std::fclose(file);
            }
        }
        Settings edited;
        edited.Load(path);
        CHECK(edited.Slots == kMaxSlots);
        CHECK(edited.DefaultGoal.Hours == 3.0f);
        CHECK(edited.PauseWhilePlaying);
        CHECK(edited.Queue.size() == 1 && edited.Queue[0].Entry.AppId == 5 && edited.Queue[0].List == kDefaultListName && !edited.Queue[0].Entry.IgnoreRefund);
        CHECK(edited.FromHour == 23);
        CHECK(edited.Lists.empty());

        {
            FILE* file = OpenForWriting(path);
            CHECK(file != nullptr);
            if (file)
            {
                std::fputs("\xEF\xBB\xBFX=-1579\r\nY=100\r\nSeason=winter\r\n", file);
                std::fclose(file);
            }
        }
        Settings marked;
        marked.Load(path);
        CHECK(marked.Placed && marked.X == -1579 && marked.Y == 100);

        std::error_code ignored;
        std::filesystem::remove(path, ignored);

        Settings missing;
        missing.Load(path);
        CHECK(!missing.Placed && missing.Queue.empty());
    }

    std::filesystem::path FindErrorReference()
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

    void TestErrors()
    {
        const auto table = ErrorTable();
        CHECK(table.size() == std::size(kErrorCodes));
        for (const ErrorCode code : kErrorCodes)
            CHECK(FindError(code) && FindError(code)->Code == code);

        bool ordered = true;
        bool described = true;
        bool plain = true;
        bool areas = true;
        bool linked = true;
        for (std::size_t index = 0; index < table.size(); ++index)
        {
            const ErrorInfo& info = table[index];
            const int number = static_cast<int>(info.Code);
            ordered &= index == 0 || static_cast<int>(table[index - 1].Code) < number;
            areas &= number >= 1001 && number <= 9999 && number % 1000 != 0 && !ErrorArea(info.Code).empty();
            described &= !info.Message.empty() && !info.Cause.empty() && !info.Help.empty() && info.Cause.back() == '.' && info.Help.back() == '.';
            const std::size_t place = info.Message.find("{}");
            plain &= (place == std::string_view::npos || info.Message.find("{}", place + 2) == std::string_view::npos) &&
                info.Message.find_first_of("\"\\<>\n") == std::string_view::npos && info.Cause.find('\n') == std::string_view::npos;
            for (const std::string_view text : { info.Cause, info.Help })
            {
                for (std::size_t at = text.find("MI-"); at != std::string_view::npos; at = text.find("MI-", at + 3))
                    linked &= FindError(text.substr(at, 7)) != nullptr;
            }
        }
        CHECK(ordered);
        CHECK(described);
        CHECK(plain);
        CHECK(areas);
        CHECK(linked);

        CHECK(ErrorName(ErrorCode::SteamNotRunning) == "MI-1001");
        CHECK(ErrorArea(ErrorCode::SteamNotRunning) == "Steam");
        CHECK(ErrorArea(ErrorCode::OtherAccount) == "Card farm");
        CHECK(ErrorArea(ErrorCode::SpriteNotSaved) == "Sprite Creator");
        CHECK(ErrorMessage(ErrorCode::SteamNotRunning) == "Steam is not running. (MI-1001)");
        CHECK(ErrorMessage(ErrorCode::AppInfoNewerFormat, "0756442A") == "Steam uses a newer format for its list of apps (0756442A). (MI-3002)");
        CHECK(ErrorMessage(ErrorCode::PageBadAnswer, "{}") == "The idler gave an answer the page cannot read (HTTP {}). (MI-7016)");

        CHECK(FindError("MI-1001") == FindError(ErrorCode::SteamNotRunning));
        CHECK(FindError(" mi-1001\n") == FindError(ErrorCode::SteamNotRunning));
        CHECK(FindError("1001") == FindError(ErrorCode::SteamNotRunning));
        CHECK(!FindError("MI-9999") && !FindError("MI-100") && !FindError("MI-10011") && !FindError("") && !FindError("XY-1001") && !FindError("MI-1a01"));

        CHECK(ErrorOf(ErrorMessage(ErrorCode::GameNotOwned)) == FindError(ErrorCode::GameNotOwned));
        CHECK(ErrorOf("Portal 2 (620): " + ErrorMessage(ErrorCode::SteamWentAway) + "\n") == FindError(ErrorCode::SteamWentAway));
        CHECK(ErrorOf("Steam did not take ACH_WIN, Portal 2 (620) (MI-4010)") == FindError(ErrorCode::ChangesRefused));
        CHECK(!ErrorOf("Steam is not running.") && !ErrorOf("(MI-9999)") && !ErrorOf("MI-1001") && !ErrorOf("") && !ErrorOf("Portal 2 (620)"));

        const std::string reference = ErrorReference(*FindError(ErrorCode::NoServer));
        CHECK(reference.rfind("MI-6001 (Command line and server)\n", 0) == 0);
        CHECK(reference.find("What happened: ") != std::string::npos && reference.find("What to do: ") != std::string::npos);
        CHECK(reference.find("ERRORS.md#mi-6001") != std::string::npos);
        const std::string list = ErrorList();
        CHECK(list.rfind("Steam\n  MI-1001  Steam is not running.\n", 0) == 0);
        CHECK(list.find("  MI-6005  No socket can be made at ....\n") != std::string::npos);

        const std::string markdown = ErrorsMarkdown();
        for (const ErrorInfo& info : table)
            CHECK(markdown.find("\n### " + ErrorName(info.Code) + "\n") != std::string::npos);
        CHECK(markdown.find("| [Card farm](#card-farm) | MI-8001 to MI-8009 |") != std::string::npos);
        CHECK(markdown.find("| [Sprite Creator](#sprite-creator) | MI-9001 to MI-9004 |") != std::string::npos);

        const std::filesystem::path file = FindErrorReference();
        std::ifstream in(file, std::ios::binary);
        std::string written((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        written.erase(std::remove(written.begin(), written.end(), '\r'), written.end());
        const bool current = !file.empty() && written == markdown;
        if (!current)
            std::printf("  Docs/ERRORS.md is not the table's: write it again with idler explain --markdown\n");
        CHECK(current);
    }

    void TestJson()
    {
        std::string problem;
        const auto parsed = Json::Parse("{\"a\": [1, -2, 3.5, true, null, \"x\\\"\\u00e9\\ud83d\\ude00\"], \"b\": {}}", problem);
        CHECK(parsed && problem.empty());
        if (parsed)
        {
            const Json::Value* list = parsed->Find("a");
            CHECK(list && list->Is(Json::Kind::Array) && list->Items.size() == 6);
            if (list && list->Items.size() == 6)
            {
                CHECK(list->Items[0].Integer() == 1);
                CHECK(list->Items[1].Integer() == -2);
                CHECK(list->Items[2].Is(Json::Kind::Number) && !list->Items[2].Integer());
                CHECK(list->Items[3].Is(Json::Kind::Boolean) && list->Items[3].Flag);
                CHECK(list->Items[4].Is(Json::Kind::Null));
                CHECK(list->Items[5].Text == "x\"\xC3\xA9\xF0\x9F\x98\x80");
                CHECK(Json::Quoted(list->Items[5].Text) == "\"x\\\"\\u00e9\\ud83d\\ude00\"");
            }
            CHECK(parsed->Find("b") && parsed->Find("b")->Is(Json::Kind::Object));
            CHECK(!parsed->Find("c"));
        }
        CHECK(Json::Parse("  [ ]\r\n", problem) && problem.empty());
        CHECK(Json::Quoted("a\nb\t\x01\x7f/") == "\"a\\nb\\t\\u0001\\u007f/\"");

        for (const char* wrong : { "", "{", "[1,]", "{\"a\" 1}", "{\"a\": 1, \"a\": 2}", "\"open", "01", "1.", "-", "tru", "[1] 2", "\"\\x\"", "\"\\ud83d\"",
                 "\"tab\there\"" })
        {
            problem.clear();
            CHECK(!Json::Parse(wrong, problem) && !problem.empty());
        }
        CHECK(!Json::Parse(std::string(100, '['), problem));
        CHECK(!Json::Parse("{\n  \"a\": x}", problem) && problem == "line 2, column 8: a value is missing");
    }

    SpriteFile::Sprite SmallSprite()
    {
        SpriteFile::Sprite sprite;
        sprite.Key = "test";
        sprite.Id = "Test";
        sprite.Body = { SpriteFile::ColorKind::Rgb, 0x5CC756 };
        sprite.Second = { SpriteFile::ColorKind::Rgb, 0xD1E58F };
        const SpriteFile::Frame first = {
            "................",
            "................",
            "................",
            "................",
            "....########....",
            "...##########...",
            "..############..",
            "..###*####*###..",
            "..############..",
            "..############..",
            "..##aaaaaaaa##..",
            "..############..",
            "...##########...",
            "...##......##...",
        };
        SpriteFile::Frame second = first;
        second[6] = "..#####++#####..";
        sprite.Frames = { first, second };
        sprite.Steps = { { 0, 0, 0, false, 16 }, { 0, 0, 0, true, 1 }, { 1, 0, 0, false, 8 } };
        return sprite;
    }

    bool Has(const std::vector<std::string>& lines, std::string_view text)
    {
        return std::find(lines.begin(), lines.end(), text) != lines.end();
    }

    bool HasStart(const std::vector<std::string>& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [&](const std::string& line) { return line.starts_with(text); });
    }

    void TestSpriteFile()
    {
        using SpriteFile::ColorKind;
        const SpriteFile::Sprite small = SmallSprite();
        const SpriteFile::Findings clean = SpriteFile::Lint(small);
        CHECK(clean.Errors.empty() && clean.Warnings.empty());

        std::string problem;
        const auto again = SpriteFile::Read(SpriteFile::Write(small), problem);
        CHECK(again && *again == small && problem.empty());

        SpriteFile::Sprite tiny;
        tiny.Key = "k";
        tiny.Id = "K";
        tiny.Body = { ColorKind::Accent, 0 };
        tiny.Third = { ColorKind::Rgb, 0x0A0B0C };
        tiny.Floats = true;
        tiny.Facing = -1;
        tiny.Frames = { { "ab", "cd" }, { "ef" } };
        tiny.Steps = { { 0, 1, -1, true, 3 } };
        CHECK(SpriteFile::Write(tiny) ==
            "{\n  \"key\": \"k\",\n  \"id\": \"K\",\n  \"body\": \"accent\",\n  \"second\": null,\n  \"third\": \"#0A0B0C\",\n  \"floats\": true,\n"
            "  \"objectLike\": false,\n  \"sleep\": 0,\n  \"facing\": -1,\n  \"stays\": false,\n  \"frames\": [\n    [\n      \"ab\",\n      \"cd\"\n    ],\n"
            "    [\n      \"ef\"\n    ]\n  ],\n  \"steps\": [\n    [0, 1, -1, true, 3]\n  ]\n}\n");
        tiny.Swims = false;
        const std::string swimming = SpriteFile::Write(tiny);
        CHECK(swimming.find("  \"floats\": true,\n  \"swims\": false,\n  \"objectLike\"") != std::string::npos);
        const auto swims = SpriteFile::Read(swimming, problem);
        CHECK(swims && swims->Swims == false && *swims == tiny);

        CHECK(!SpriteFile::Read("[]", problem) && problem == "the file holds no object");
        CHECK(!SpriteFile::Read("{", problem) && problem.starts_with("line 1"));
        CHECK(!SpriteFile::Read("{\"key\": \"a\"}", problem) && problem == "the field \"id\" is missing");
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"colour\": 1}", problem) && problem == "the field \"colour\" is unknown");
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"body\": \"#12345\"}", problem) && problem.starts_with("\"body\" must be"));
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"steps\": [[0, 0, 0, false]]}", problem) && problem.starts_with("\"steps\" must be"));
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"floats\": 1}", problem) && problem == "\"floats\" must be true or false");
        const auto lower = SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"body\": \"#5cc756\", \"steps\": [[0, 0, 0, 1, 2]]}", problem);
        const SpriteFile::Color green{ ColorKind::Rgb, 0x5CC756 };
        CHECK(lower && lower->Body == green && lower->Steps.size() == 1 && lower->Steps[0].Blink);

        const auto lint = [](SpriteFile::Sprite sprite, auto change)
            {
                change(sprite);
                return SpriteFile::Lint(sprite);
            };
        CHECK(lint(small, [](SpriteFile::Sprite& s) { s.Frames.clear(); }).Errors == std::vector<std::string>{ "no frames" });
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][4] = "....#######...."; }).Errors, "frame 0 row 4 has 15 cells, not 16: '....#######....'"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][5][0] = 'x'; }).Errors, "frame 0 row 5 has unknown cells ['x']"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0].pop_back(); }).Errors, "frame 0 has 13 rows, not 14"));
        const auto noSecond = lint(small, [](SpriteFile::Sprite& s) { s.Second = {}; });
        CHECK(Has(noSecond.Errors, "frame 0 row 10 uses second-color cells but \"second\" is empty") && Has(noSecond.Errors, "frame 1 row 10 uses second-color cells but \"second\" is empty"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Body = {}; }).Errors, "no body color"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[0].Lift = 5; }).Errors, "step 0: lift must be 0 to 4"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[2].Shift = -3; }).Errors, "step 2: shift must be -2 to 2"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[1].Frame = 2; }).Errors, "step 1 names frame 2"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Sleep = 7; }).Errors, "sleep names frame 7"));

        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps = { { 0, 0, 0, true, 1 }, { 1, 0, 0, false, 1 } }; }).Warnings, "the loop is only 0.2 s long: it will look hectic"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps = { { 0, 0, 0, true, 3 }, { 1, 0, 0, false, 3 } }; }).Warnings, "the loop is only 0.8 s long: it will look hectic"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps = { { 0, 0, 0, true, 60 }, { 1, 0, 0, false, 60 } }; }).Warnings, "the loop is 15.0 s long: its action will rarely be seen"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[1].Blink = false; }).Warnings, "it has eyes but never blinks"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps.pop_back(); }).Warnings, "frame 1 is never used"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[1] = s.Frames[0]; }).Warnings, "frames 0 and 1 are identical"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Third = { SpriteFile::ColorKind::Rgb, 0xFF0000 }; }).Warnings, "third color is set but never used"));
        CHECK(HasStart(lint(small, [](SpriteFile::Sprite& s) { s.Body = { SpriteFile::ColorKind::Rgb, 0x101010 }; }).Warnings, "body color has low contrast on the card ("));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][2][0] = '#'; }).Warnings, "frame 0: lone cell '#' at row 2 column 0"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][7][5] = 'e'; }).Warnings,
            "frame 0 row 7 column 5: eye 'e' sits on the body: use '*' so it shuts in the right color"));
        const auto lifted = [](SpriteFile::Sprite& s)
            {
                for (SpriteFile::Frame& frame : s.Frames)
                {
                    frame.erase(frame.begin());
                    frame.push_back("................");
                }
            };
        CHECK(Has(lint(small, lifted).Warnings, "it stands on row 12, not on the ground row 13: or set \"floats\": true"));
        CHECK(lint(small, [&](SpriteFile::Sprite& s) { lifted(s); s.Floats = true; }).Warnings.empty());
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[1][13] = "................"; s.Frames[1][12] = "...##......##..."; }).Warnings,
            "the lowest cell moves between frames [13, 12]: feet should stay on the ground, or set \"floats\": true"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.ObjectLike = false; s.Frames[0][7] = s.Frames[0][6]; s.Frames[1][7] = s.Frames[1][6]; }).Warnings,
            "no eye cells (* or e) in any frame: set \"objectLike\": true if that is on purpose"));
    }

    void TestSpriteEdit()
    {
        using namespace SpriteEdit;
        SpriteFile::Frame frame = EmptyFrame();
        CHECK(frame.size() == 14 && frame[0] == std::string(16, '.'));
        Paint(frame, 2, 3, '#', true);
        CHECK(CellAt(frame, 2, 3) == '#' && CellAt(frame, 2, 12) == '#');
        CHECK(!SetCell(frame, 20, 3, '#') && CellAt(frame, -1, 0) == '\0' && CellAt(frame, 0, 16) == '\0');
        CHECK(!SetCell(frame, 2, 3, '#'));

        PaintLine(frame, 0, 0, 0, 15, 'o', false);
        CHECK(frame[0] == std::string(16, 'o'));
        PaintLine(frame, 5, 0, 9, 4, 'a', false);
        CHECK(CellAt(frame, 5, 0) == 'a' && CellAt(frame, 6, 1) == 'a' && CellAt(frame, 7, 2) == 'a' && CellAt(frame, 8, 3) == 'a' && CellAt(frame, 9, 4) == 'a');
        CHECK(CellAt(frame, 5, 1) == '.' && CellAt(frame, 9, 3) == '.');
        PaintLine(frame, 12, 15, 12, 13, 'k', true);
        CHECK(CellAt(frame, 12, 13) == 'k' && CellAt(frame, 12, 15) == 'k' && CellAt(frame, 12, 0) == 'k' && CellAt(frame, 12, 2) == 'k');

        frame = EmptyFrame();
        PaintRectangle(frame, 8, 9, 4, 4, '#', false, false);
        CHECK(CellAt(frame, 4, 4) == '#' && CellAt(frame, 8, 9) == '#' && CellAt(frame, 6, 4) == '#' && CellAt(frame, 4, 7) == '#');
        CHECK(CellAt(frame, 6, 6) == '.' && CellAt(frame, 3, 4) == '.' && CellAt(frame, 4, 10) == '.');
        Fill(frame, 6, 6, 'a', false);
        CHECK(CellAt(frame, 6, 6) == 'a' && CellAt(frame, 5, 8) == 'a' && CellAt(frame, 7, 5) == 'a');
        CHECK(CellAt(frame, 0, 0) == '.' && CellAt(frame, 4, 4) == '#');
        Fill(frame, 0, 0, 'k', false);
        CHECK(CellAt(frame, 0, 0) == 'k' && CellAt(frame, 13, 15) == 'k' && CellAt(frame, 6, 6) == 'a' && CellAt(frame, 4, 4) == '#');
        Fill(frame, 6, 6, 'a', false);
        CHECK(CellAt(frame, 6, 6) == 'a');

        SpriteFile::Frame walled = EmptyFrame();
        PaintLine(walled, 0, 7, 13, 7, '#', true);
        Fill(walled, 0, 0, 'c', true);
        CHECK(CellAt(walled, 5, 3) == 'c' && CellAt(walled, 5, 12) == 'c' && CellAt(walled, 5, 7) == '#' && CellAt(walled, 5, 8) == '#');

        SpriteFile::Frame dot = EmptyFrame();
        SetCell(dot, 0, 0, '#');
        const SpriteFile::Frame moved = Shifted(dot, 2, 3);
        CHECK(CellAt(moved, 2, 3) == '#' && CellAt(moved, 0, 0) == '.' && moved.size() == 14 && moved[13].size() == 16);
        const SpriteFile::Frame gone = Shifted(dot, -1, 0);
        CHECK(std::all_of(gone.begin(), gone.end(), [](const std::string& line) { return line == std::string(16, '.'); }));
        FlipHorizontal(dot);
        CHECK(CellAt(dot, 0, 15) == '#' && CellAt(dot, 0, 0) == '.');
        CHECK(MirroredColumn(0) == 15 && MirroredColumn(7) == 8);

        CHECK(CellName('o') == "Body shade" && CellName('.') == "Empty" && CellName('x') == "Unknown");
        CHECK(std::size(kCellKinds) == SpriteFile::kCells.size());
        for (const CellKind& kind : kCellKinds)
            CHECK(SpriteFile::IsCell(kind.Cell));
    }

    void TestSpriteTimeline()
    {
        using namespace SpriteEdit;
        const auto marked = [](int column)
            {
                SpriteFile::Frame frame = EmptyFrame();
                SetCell(frame, 0, column, '#');
                return frame;
            };
        const auto steps = [](const SpriteFile::Sprite& sprite)
            {
                std::vector<int> frames;
                for (const SpriteFile::Step& step : sprite.Steps)
                    frames.push_back(step.Frame);
                return frames;
            };

        SpriteFile::Sprite sprite;
        sprite.Frames = { marked(0), marked(1), marked(2) };
        sprite.Steps = { { 0, 0, 0, false, 4 }, { 1, 0, 0, false, 4 }, { 2, 0, 0, false, 4 }, { 1, 0, 0, true, 2 } };
        sprite.Sleep = 2;

        CHECK(TotalTicks(sprite) == 14);
        CHECK(StepAtTick(sprite, 0) == 0 && StepAtTick(sprite, 3) == 0 && StepAtTick(sprite, 4) == 1 && StepAtTick(sprite, 12) == 3 && StepAtTick(sprite, 14) == 0);
        CHECK(StepAtTick(sprite, -1) == 3);

        CHECK(InsertFrame(sprite, 1, marked(3)));
        CHECK(sprite.Frames.size() == 4 && sprite.Frames[1] == marked(3) && sprite.Frames[2] == marked(1));
        CHECK(steps(sprite) == std::vector<int>({ 0, 2, 3, 2 }) && sprite.Sleep == 3);

        CHECK(RemoveFrame(sprite, 2));
        CHECK(sprite.Frames.size() == 3 && sprite.Frames[2] == marked(2));
        CHECK(steps(sprite) == std::vector<int>({ 0, 2 }) && sprite.Sleep == 2);

        CHECK(MoveFrame(sprite, 2, 0));
        CHECK(sprite.Frames[0] == marked(2) && sprite.Frames[1] == marked(0) && sprite.Frames[2] == marked(3));
        CHECK(steps(sprite) == std::vector<int>({ 1, 0 }) && sprite.Sleep == 0);
        CHECK(!MoveFrame(sprite, 0, 3) && !MoveFrame(sprite, 1, 1));

        CHECK(RemoveFrame(sprite, 0));
        CHECK(steps(sprite) == std::vector<int>({ 0 }) && sprite.Sleep == 0);
        CHECK(RemoveFrame(sprite, 0));
        CHECK(sprite.Frames.size() == 1 && sprite.Frames[0] == marked(3));
        CHECK(sprite.Steps.size() == 1 && sprite.Steps[0].Frame == 0 && sprite.Steps[0].Ticks == kStartTicks);
        CHECK(!RemoveFrame(sprite, 0));

        while (sprite.Frames.size() < static_cast<std::size_t>(SpriteFile::kMaxFrames))
            CHECK(InsertFrame(sprite, static_cast<int>(sprite.Frames.size()), EmptyFrame()));
        CHECK(!InsertFrame(sprite, 0, EmptyFrame()));

        CHECK(InsertStep(sprite, 1, { 3, 1, -1, true, 5 }));
        CHECK(InsertStep(sprite, 99, { 4, 0, 0, false, 1 }));
        CHECK(steps(sprite) == std::vector<int>({ 0, 3, 4 }));
        CHECK(MoveStep(sprite, 2, 0) && steps(sprite) == std::vector<int>({ 4, 0, 3 }));
        CHECK(RemoveStep(sprite, 1) && steps(sprite) == std::vector<int>({ 4, 3 }));
        CHECK(RemoveStep(sprite, 0) && !RemoveStep(sprite, 0) && sprite.Steps.size() == 1);
        CHECK(sprite.Steps[0].Lift == 1 && sprite.Steps[0].Shift == -1 && sprite.Steps[0].Blink && sprite.Steps[0].Ticks == 5);
    }

    std::filesystem::path FindCreatureFolder()
    {
        const std::filesystem::path reference = FindErrorReference();
        if (reference.empty())
            return {};
        return reference.parent_path().parent_path() / "Tools" / "CreatureForge" / "creatures";
    }

    bool SameColor(ImVec4 one, ImVec4 two)
    {
        return one.x == two.x && one.y == two.y && one.z == two.z && one.w == two.w;
    }

    bool SameSprite(const UI::CreatureSprites::Sprite& one, const UI::CreatureSprites::Sprite& two)
    {
        if (one.Id != two.Id || one.FrameCount != two.FrameCount || one.StepCount != two.StepCount || one.SleepFrame != two.SleepFrame || one.Facing != two.Facing ||
            one.Floats != two.Floats || one.Swims != two.Swims || one.Stays != two.Stays || !SameColor(one.Body, two.Body) || !SameColor(one.Second, two.Second) ||
            !SameColor(one.Third, two.Third))
            return false;
        for (int frame = 0; frame < one.FrameCount; ++frame)
        {
            for (std::size_t row = 0; row < one.Frames[frame].size(); ++row)
            {
                if (std::strcmp(one.Frames[frame][row], two.Frames[frame][row]) != 0)
                    return false;
            }
        }
        for (int step = 0; step < one.StepCount; ++step)
        {
            const UI::CreatureSprites::Step& a = one.Steps[step];
            const UI::CreatureSprites::Step& b = two.Steps[step];
            if (a.FrameIndex != b.FrameIndex || a.Lift != b.Lift || a.Shift != b.Shift || a.Blink != b.Blink || a.Ticks != b.Ticks)
                return false;
        }
        return true;
    }

    void TestCreatureFiles()
    {
        const std::filesystem::path folder = FindCreatureFolder();
        std::error_code ignored;
        const bool found = !folder.empty() && std::filesystem::is_directory(folder, ignored);
        CHECK(found);
        if (!found)
            return;

        std::size_t firsts = 0;
        std::size_t evolved = 0;
        for (const bool grown : { false, true })
        {
            for (const auto& entry : std::filesystem::directory_iterator(grown ? folder / "evolved" : folder, ignored))
            {
                if (!entry.is_regular_file(ignored) || entry.path().extension() != ".json")
                    continue;
                const std::string stem = entry.path().stem().string();
                const std::string key = (grown ? "evolved/" : "") + stem;
                std::ifstream in(entry.path(), std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());

                std::string problem;
                const auto sprite = SpriteFile::Read(text, problem);
                CHECK(sprite.has_value());
                if (!sprite)
                {
                    std::printf("  %s: %s\n", key.c_str(), problem.c_str());
                    continue;
                }
                const bool same = SpriteFile::Write(*sprite) == text;
                CHECK(same);
                if (!same)
                    std::printf("  %s is not written back as it was read\n", key.c_str());
                CHECK(sprite->Key == key);
                const SpriteFile::Findings findings = SpriteFile::Lint(*sprite);
                CHECK(findings.Errors.empty());
                for (const std::string& error : findings.Errors)
                    std::printf("  %s: %s\n", key.c_str(), error.c_str());

                const std::optional<CreatureId> id = CreatureWithKey(stem);
                CHECK(id.has_value());
                if (!id)
                    continue;
                const UI::SpriteView view(*sprite);
                const UI::CreatureSprites::Sprite* built = nullptr;
                if (!grown)
                {
                    built = &UI::CreatureSprites::kSprites[static_cast<std::size_t>(*id)];
                    ++firsts;
                }
                else
                {
                    for (const UI::CreatureSprites::Sprite& candidate : UI::CreatureSprites::kEvolvedSprites)
                    {
                        if (candidate.Id == *id)
                            built = &candidate;
                    }
                    ++evolved;
                }
                const bool matches = built && SameSprite(view.Get(), *built);
                CHECK(matches);
                if (!matches)
                    std::printf("  %s differs from Source/UI/CreatureSprites.h: write it again with forge.py header\n", key.c_str());
            }
        }
        CHECK(firsts == std::size(UI::CreatureSprites::kSprites));
        CHECK(evolved == std::size(UI::CreatureSprites::kEvolvedSprites));
    }

    void TestFileLock()
    {
        const std::filesystem::path settings = TestFile("-lock.ini");
        const std::filesystem::path lockFile = SettingsLockFile(settings);
        CHECK(lockFile.extension() == ".lock" && lockFile.parent_path() == settings.parent_path());
        CHECK(!SettingsInUse(settings));
        {
            FileLock first;
            CHECK(first.Take(lockFile) && first.Held());
            FileLock second;
            CHECK(!second.Take(lockFile) && !second.Held());
            CHECK(SettingsInUse(settings));
            first.Release();
            CHECK(!first.Held());
            CHECK(second.Take(lockFile) && second.Held());
        }
        CHECK(!SettingsInUse(settings));
        std::error_code ignored;
        std::filesystem::remove(lockFile, ignored);
    }

    void TestText()
    {
        const std::string_view jsonPlain = R"("plain")";
        const std::string_view jsonEmpty = R"("")";
        const std::string_view jsonMarks = R"("a\"b\\c\/d")";
        const std::string_view jsonControls = R"("line\nnext\ttab\r")";
        const std::string_view jsonMarkup = R"("\u003Cdiv\u003E")";
        const std::string_view jsonAccents = R"("\u00e4\u20ac")";
        const std::string_view jsonPair = R"("\ud83d\ude00")";
        const std::string_view jsonShortCode = R"("\u12")";
        const std::string_view jsonCut = R"("cut\")";
        const std::string_view textMarks = "a\"b\\c/d";
        const std::string_view jsonRaw = "\"\xC3\xA4\"";
        const std::string_view jsonOneQuote = "\"";
        CHECK(DecodeJsonString(jsonPlain) == "plain");
        CHECK(DecodeJsonString(jsonEmpty).empty());
        CHECK(DecodeJsonString(jsonMarks) == textMarks);
        CHECK(DecodeJsonString(jsonControls) == "line\nnext\ttab\r");
        CHECK(DecodeJsonString(jsonMarkup) == "<div>");
        CHECK(DecodeJsonString(jsonAccents) == "\xC3\xA4\xE2\x82\xAC");
        CHECK(DecodeJsonString(jsonPair) == "\xF0\x9F\x98\x80");
        CHECK(DecodeJsonString(jsonRaw) == "\xC3\xA4");
        CHECK(DecodeJsonString("null").empty());
        CHECK(DecodeJsonString("").empty());
        CHECK(DecodeJsonString(jsonOneQuote).empty());
        CHECK(DecodeJsonString(jsonShortCode).empty());
        CHECK(DecodeJsonString(jsonCut).empty());

        CHECK(DurationText(45) == "45 s");
        CHECK(DurationText(600) == "10 min");
        CHECK(DurationText(3 * 3600 + 720) == "3 h 12 min");
        CHECK(ShortDurationText(3 * 3600 + 720) == "3 h 12 min");
        CHECK(ShortDurationText(1032 * 3600 + 720) == "1032 h");
        CHECK(PlaytimeText(0) == "not played");
        CHECK(PlaytimeText(45) == "45 min");
        CHECK(PlaytimeText(90) == "1.5 h");
        CHECK(HoursText(3.0f) == "3 h");
        CHECK(HoursText(1.5f) == "1.5 h");
        CHECK(DateText(0).empty());
        CHECK(DateText(1700000000).size() == 10);
        CHECK(ParseUnsigned("42") == 42u);
        CHECK(!ParseUnsigned("").has_value());
        CHECK(!ParseUnsigned("4x").has_value());
        CHECK(!ParseUnsigned("-4").has_value());
        CHECK(!ParseUnsigned("99999999999").has_value());
        CHECK(Trim("  a b \r\n") == "a b");
        CHECK(Trim(" \t ").empty());
        CHECK(EqualsIgnoreCase("Apps", "aPPS"));
        CHECK(!EqualsIgnoreCase("apps", "app"));
        CHECK(FoldCase("\xC3\x84RGER Game") == "\xC3\xA4rger game");
        CHECK(LowerCodePoint(0x00C4) == 0x00E4);
        CHECK(LowerCodePoint(0x00D7) == 0x00D7);
        CHECK(LowerCodePoint(0x0141) == 0x0142);
        CHECK(LowerCodePoint(0x0178) == 0x00FF);
        CHECK(LowerCodePoint(0x03A3) == 0x03C3);
        CHECK(LowerCodePoint(0x0416) == 0x0436);
        CHECK(LowerCodePoint(0x0401) == 0x0451);
        CHECK(LowerCodePoint(0x1EA0) == 0x1EA1);
        CHECK(LowerCodePoint(0x0181) == 0x0253);
        CHECK(LowerCodePoint(0x1F59) == 0x1F51);
        CHECK(LowerCodePoint(0x1F5A) == 0x1F5A);
        CHECK(LowerCodePoint(0xFF21) == 0xFF41);
        CHECK(LowerCodePoint(0x00E4) == 0x00E4);
        CHECK(LowerCodePoint(0x4E00) == 0x4E00);
        CHECK(LowerCodePoint(0x1F600) == 0x1F600);
#ifdef _WIN32
        {
            int differences = 0;
            for (std::uint32_t code = 0; code <= 0xFFFF; ++code)
            {
                if (code >= 0xD800 && code <= 0xDFFF)
                    continue;
                wchar_t character = static_cast<wchar_t>(code);
                ::CharLowerBuffW(&character, 1);
                const std::uint32_t windows = static_cast<std::uint32_t>(character);
                if (LowerCodePoint(code) != windows && ++differences <= 5)
                    std::printf("  lower case of U+%04X: %04X here, %04X in Windows\n", code, LowerCodePoint(code), windows);
            }
            CHECK(differences == 0);
        }
#endif
        CHECK(WideToUtf8(Utf8ToWide("\xE6\x9D\xB1\xE6\x96\xB9")) == "\xE6\x9D\xB1\xE6\x96\xB9");

        CHECK(FontNeedsOf("Plain \xC3\x84 name") == 0);
        CHECK(FontNeedsOf("Game\xE2\x84\xA2 \xE2\x80\x94 two") == 0);
        CHECK(FontNeedsOf("\xD0\x98\xD0\xB3\xD1\x80\xD0\xB0") == 0);
        CHECK(FontNeedsOf("\xF0\x9F\x8E\xAE") == 0);
        CHECK(FontNeedsOf("\xE0\xBC\x80 name") == kFontNeedOtherScripts);
        CHECK(FontNeedsOf("\xE6\x9D\xB1\xE6\x96\xB9") == kFontNeedAsian);
        CHECK(FontNeedsOf("\xED\x95\x9C") == kFontNeedAsian);
        CHECK(FontNeedsOf("\xEF\xBC\x81") == 0);
        CHECK(FontNeedsOf("\xE0\xBC\x80\xE6\x9D\xB1") == (kFontNeedOtherScripts | kFontNeedAsian));
    }
}

namespace
{
    TerrainGame PlayWell(MiniGame game, std::uint32_t seed, int& dizzyMoments, bool chasePebbles = false)
    {
        TerrainGame play(game, seed);
        dizzyMoments = 0;
        bool wasDizzy = false;
        while (!play.Over())
        {
            const float middleX = play.X() + kPlaySpriteWidth * 0.5f;
            const float middleY = play.Y() + kPlaySpriteHeight * 0.5f;
            float pointerX = middleX;
            float pointerY = middleY;
            float nearest = 1e9f;
            for (const PlayItem& item : play.Items())
            {
                const bool wanted = chasePebbles ? item.Kind == PlayItemKind::Pebble
                    : item.Kind == PlayItemKind::Treat || item.Kind == PlayItemKind::GoldenTreat || item.Kind == PlayItemKind::Star || item.Kind == PlayItemKind::ShootingStar ||
                    item.Kind == PlayItemKind::Pearl || (item.Kind == PlayItemKind::Clam && item.Open > 0.3f);
                const float distance = std::hypot(item.X - middleX, item.Y - middleY);
                if (wanted && distance < nearest)
                {
                    nearest = distance;
                    pointerX = item.X;
                    pointerY = item.Kind == PlayItemKind::Clam ? item.Y - 4.0f : item.Y;
                }
            }
            play.Step(1.0 / 60.0, pointerX, pointerY);
            if (play.Dizzy() > 0.0f && !wasDizzy)
                ++dizzyMoments;
            wasDizzy = play.Dizzy() > 0.0f;
            CHECK(play.X() >= 0.0f && play.X() <= kPlayWidth - kPlaySpriteWidth && play.Y() >= 0.0f && play.Y() + kPlaySpriteHeight <= play.Ground() + 3.5f);
        }
        return play;
    }
}

void TestMiniGames()
{
    CHECK(TerrainGameOf(Habit::Walks) == MiniGame::TreatCatch && TerrainGameOf(Habit::Stays) == MiniGame::TreatCatch);
    CHECK(TerrainGameOf(Habit::Floats) == MiniGame::StarChase && TerrainGameOf(Habit::Swims) == MiniGame::PearlDive);
    for (std::size_t game = 0; game < kMiniGameCount; ++game)
    {
        CHECK(std::string_view(MiniGameName(static_cast<MiniGame>(game))).size() > 0 && std::string_view(MiniGameRules(static_cast<MiniGame>(game))).size() > 0);
        std::uint32_t before = 0;
        for (int score = 0; score <= 200; score += 5)
        {
            const std::uint32_t bond = PlayBondOf(static_cast<MiniGame>(game), score);
            CHECK(bond >= 3 && bond <= 10 && bond >= before);
            before = bond;
        }
        CHECK(PlayBondOf(static_cast<MiniGame>(game), -4) == 3);
    }

    PlayRecord record;
    CHECK(PlayBondLeft(record, 100) == kPlayBondPerDay);
    CHECK(GrantPlayBond(record, 100, 10) == 10 && GrantPlayBond(record, 100, 10) == 10);
    CHECK(GrantPlayBond(record, 100, 10) == kPlayBondPerDay - 20);
    CHECK(GrantPlayBond(record, 100, 10) == 0 && PlayBondLeft(record, 100) == 0);
    CHECK(record.Minutes == kPlayBondPerDay && record.MinutesThatDay == kPlayBondPerDay && record.Day == 100);
    CHECK(PlayBondLeft(record, 101) == kPlayBondPerDay);
    CHECK(GrantPlayBond(record, 101, 4) == 4 && record.MinutesThatDay == 4 && record.Minutes == kPlayBondPerDay + 4 && record.Day == 101);
    CHECK(std::fabs(BondHours(3600.0, 0, 30) - 1.5) < 1e-9 && BondHours(0.0, 0) == 0.0);

    for (const MiniGame game : { MiniGame::TreatCatch, MiniGame::StarChase, MiniGame::PearlDive })
    {
        int dizzy = 0;
        const TerrainGame good = PlayWell(game, 7u, dizzy);
        CHECK(good.Over() && good.TimeLeft() == 0.0);
        CHECK(good.Score() >= 15);
        int again = 0;
        CHECK(PlayWell(game, 7u, again).Score() == good.Score() && again == dizzy);

        TerrainGame idle(game, 7u);
        for (int step = 0; step < 60 * 50; ++step)
            idle.Step(1.0 / 60.0, 0.0f, 0.0f);
        CHECK(idle.Over() && idle.Score() < good.Score());
        const int ended = idle.Score();
        idle.TakeCatches();
        idle.Step(1.0, 50.0f, 50.0f);
        CHECK(idle.Score() == ended && idle.TakeCatches().empty());
    }
    {
        int dizzy = 0;
        const TerrainGame pebbles = PlayWell(MiniGame::TreatCatch, 11u, dizzy, true);
        CHECK(dizzy >= 2);
    }
    {
        TerrainGame slow(MiniGame::TreatCatch, 3u);
        TerrainGame fast(MiniGame::TreatCatch, 3u);
        for (int step = 0; step < 600; ++step)
            slow.Step(1.0 / 60.0, 88.0f, 0.0f);
        for (int step = 0; step < 100; ++step)
            fast.Step(0.1, 88.0f, 0.0f);
        CHECK(slow.Score() > 0 && std::abs(slow.Score() - fast.Score()) <= 1);
    }

    ShellGame shells(5u);
    CHECK(shells.CurrentPhase() == ShellGame::Phase::Show && shells.Round() == 1 && shells.Score() == 0);
    CHECK(shells.CupLift(shells.HidingCup()) == 1.0f);
    CHECK(!shells.PickSlot(0));
    int previousSwaps = 0;
    float previousSeconds = 10.0f;
    for (int round = 1; round <= 6; ++round)
    {
        CHECK(shells.Round() == round);
        CHECK(shells.SwapsThisRound() > previousSwaps && shells.SwapSeconds() < previousSeconds);
        previousSwaps = shells.SwapsThisRound();
        previousSeconds = shells.SwapSeconds();
        bool sawMove = false;
        while (shells.CurrentPhase() != ShellGame::Phase::Pick)
        {
            for (int cup = 0; cup < ShellGame::kCups; ++cup)
            {
                const float position = shells.CupPosition(cup);
                CHECK(position >= 0.0f && position <= 2.0f);
                sawMove |= position != std::round(position);
            }
            shells.Step(1.0 / 60.0);
        }
        CHECK(sawMove);
        CHECK(shells.SlotOf(0) != shells.SlotOf(1) && shells.SlotOf(1) != shells.SlotOf(2) && shells.SlotOf(0) != shells.SlotOf(2));
        CHECK(shells.CupLift(0) == 0.0f && shells.CupLift(1) == 0.0f && shells.CupLift(2) == 0.0f);
        CHECK(shells.PickSlot(shells.SlotOf(shells.HidingCup())));
        CHECK(shells.CurrentPhase() == ShellGame::Phase::Reveal && shells.PickedCup() == shells.HidingCup());
        CHECK(!shells.PickSlot(0));
        for (int step = 0; step < 120; ++step)
            shells.Step(1.0 / 60.0);
        CHECK(shells.Score() == round);
    }
    while (shells.CurrentPhase() != ShellGame::Phase::Pick)
        shells.Step(0.05);
    const int wrongSlot = (shells.SlotOf(shells.HidingCup()) + 1) % ShellGame::kCups;
    CHECK(shells.PickSlot(wrongSlot));
    CHECK(shells.PickedCup() != shells.HidingCup());
    shells.Step(2.0);
    CHECK(shells.CurrentPhase() == ShellGame::Phase::Reveal);
    for (int step = 0; step < 8; ++step)
        shells.Step(0.25);
    CHECK(shells.Over() && shells.Score() == 6);
    CHECK(shells.CupLift(shells.HidingCup()) == 1.0f && shells.CupLift(shells.PickedCup()) == 1.0f);
    ShellGame jumpy(9u);
    jumpy.Step(30.0);
    for (int step = 0; step < 200 && jumpy.CurrentPhase() != ShellGame::Phase::Pick; ++step)
        jumpy.Step(0.25);
    CHECK(jumpy.CurrentPhase() == ShellGame::Phase::Pick);
    for (int cup = 0; cup < ShellGame::kCups; ++cup)
        CHECK(jumpy.CupPosition(cup) == static_cast<float>(jumpy.SlotOf(cup)));
}

void TestFetch()
{
    const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
    constexpr double kStep = 0.05;
    const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
        {
            for (double passed = 0.0; passed < seconds; passed += kStep)
                box.Update(guests, size, kStep, true);
        };
    const auto walker = [](CreatureId id, int bond = 0) { return std::vector<TerrariumGuest>{ TerrariumGuest{ 1, Habit::Walks, false, id, bond } }; };
    const auto throwBall = [&](Terrarium& box, float distance)
        {
            const TerrariumBall before = box.Ball();
            CHECK(box.GrabBall(before.X + 1.0f, before.Y - 1.0f));
            CHECK(box.Ball().State == BallState::Held);
            for (int step = 1; step <= 6; ++step)
                box.CarryBall(before.X + distance * static_cast<float>(step) / 6.0f, before.Y - 2.0f * static_cast<float>(step), 1.0 / 60.0);
            box.DropBall(size);
        };
    const auto setUp = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests)
        {
            box.Update(guests, size, kStep, false);
            run(box, guests, 2.0);
            box.SetBall(true, size);
        };

    {
        Terrarium box(3u);
        const auto guests = walker(CreatureId::Crab);
        setUp(box, guests);
        CHECK(box.Ball().State == BallState::Lying);
        CHECK(box.Ball().Y > 130.0f - kTerrariumGroundDepth - 3.0f && box.Ball().Y < 130.0f - kTerrariumLakeDepth);
        CHECK(!box.GrabBall(box.Ball().X + 20.0f, box.Ball().Y));
        box.SetBall(false, size);
        CHECK(box.Ball().State == BallState::None);
    }

    {
        Terrarium box(5u);
        const auto guests = walker(CreatureId::Crab);
        setUp(box, guests);
        throwBall(box, 40.0f);
        CHECK(box.Ball().State == BallState::Flying);
        const float thrownFrom = box.Ball().ThrownFromX;
        bool carried = false;
        bool wentUp = false;
        std::vector<std::uint32_t> fetched;
        for (double passed = 0.0; passed < 25.0 && fetched.empty(); passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            carried |= box.Ball().State == BallState::Carried;
            wentUp |= box.Ball().Y < box.Ball().Floor - 5.0f;
            for (const std::uint32_t seed : box.TakeFetches())
                fetched.push_back(seed);
            CHECK(box.Ball().X >= kBallRadius && box.Ball().X <= size.Width - kBallRadius);
        }
        CHECK(carried && wentUp);
        CHECK(fetched.size() == 1 && fetched[0] == 1);
        CHECK(box.Ball().State == BallState::Lying && std::fabs(box.Ball().X - thrownFrom) < 12.0f);
        CHECK(box.Residents()[0].Reaction == TerrariumReaction::Joy && box.Residents()[0].Fetch == FetchStep::None);
        run(box, guests, 10.0);
        CHECK(box.TakeFetches().empty() && box.Ball().State == BallState::Lying);
    }

    {
        Terrarium box(5u);
        const auto guests = walker(CreatureId::Crab);
        setUp(box, guests);
        CHECK(box.GrabBall(box.Ball().X, box.Ball().Y));
        box.DropBall(size);
        run(box, guests, 8.0);
        CHECK(box.TakeFetches().empty() && box.Residents()[0].Fetch == FetchStep::None && box.Ball().State == BallState::Lying);
    }

    {
        Terrarium box(6u);
        const auto guests = walker(CreatureId::Capybara);
        setUp(box, guests);
        throwBall(box, 40.0f);
        run(box, guests, 12.0);
        CHECK(box.TakeFetches().empty() && box.Ball().Fetcher == 0 && box.Ball().State == BallState::Lying);
    }

    {
        Terrarium box(6u);
        const auto guests = walker(CreatureId::Bunny);
        setUp(box, guests);
        throwBall(box, -40.0f);
        bool waited = false;
        std::size_t fetched = 0;
        for (double passed = 0.0; passed < 30.0 && fetched == 0; passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            waited |= box.Residents()[0].Fetch == FetchStep::Waiting && box.Residents()[0].Reaction == TerrariumReaction::Puzzled;
            fetched += box.TakeFetches().size();
        }
        CHECK(waited && fetched == 1);
    }

    {
        Terrarium box(5u);
        const auto guests = walker(CreatureId::Crab);
        setUp(box, guests);
        throwBall(box, 40.0f);
        for (double passed = 0.0; passed < 25.0 && box.Ball().State != BallState::Carried; passed += kStep)
            box.Update(guests, size, kStep, true);
        CHECK(box.Ball().State == BallState::Carried);
        box.Grab(1);
        box.Update(guests, size, kStep, true);
        CHECK(box.Ball().State == BallState::Flying || box.Ball().State == BallState::Lying);
        CHECK(box.Ball().Fetcher == 0 && box.Residents()[0].Fetch == FetchStep::None);
        run(box, guests, 5.0);
        CHECK(box.TakeFetches().empty() && box.Ball().State == BallState::Lying);
    }

    {
        Terrarium box(5u);
        const auto guests = walker(CreatureId::Crab);
        setUp(box, guests);
        throwBall(box, 40.0f);
        box.Update(guests, size, kStep, false);
        CHECK(box.Ball().State == BallState::Lying && box.Ball().Fetcher == 0);
        run(box, guests, 8.0);
        CHECK(box.TakeFetches().empty());
    }
}

void TestSnailShell()
{
    const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
    constexpr double kStep = 0.05;
    const auto one = [](CreatureId id) { return std::vector<TerrariumGuest>{ TerrariumGuest{ 1, Habit::Walks, false, id, 0 } }; };
    const auto hurl = [&](Terrarium& box, float speed)
        {
            box.Grab(1);
            box.Carry(1, 100.0f, 40.0f, 0.0);
            for (int step = 1; step <= 6; ++step)
                box.Carry(1, 100.0f + speed * static_cast<float>(step) / 60.0f, 40.0f, 1.0 / 60.0);
            box.Drop(1, size);
        };
    const auto fresh = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests)
        {
            box.Update(guests, size, kStep, false);
            for (double passed = 0.0; passed < 1.0; passed += kStep)
                box.Update(guests, size, kStep, true);
        };
    struct Seen
    {
        bool CameOff = false;
        bool Sulked = false;
        bool Crawled = false;
    };
    const auto watch = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
        {
            Seen seen;
            for (double passed = 0.0; passed < seconds; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                const TerrariumResident& snail = box.Residents()[0];
                seen.CameOff |= snail.Bare && box.Shell().State != BallState::None;
                seen.Sulked |= snail.Bare && snail.Reaction == TerrariumReaction::Sulking;
                seen.Crawled |= snail.Bare && snail.State == ResidentState::Moving;
                if (seen.CameOff && !snail.Bare)
                    break;
            }
            return seen;
        };

    {
        Terrarium box(7u);
        const auto guests = one(CreatureId::Snail);
        fresh(box, guests);
        hurl(box, 400.0f);
        const Seen seen = watch(box, guests, 90.0);
        CHECK(seen.CameOff && seen.Sulked && seen.Crawled);
        CHECK(!box.Residents()[0].Bare && box.Shell().State == BallState::None);
        CHECK(box.Residents()[0].Reaction == TerrariumReaction::Joy);
    }

    {
        Terrarium box(7u);
        const auto guests = one(CreatureId::Snail);
        fresh(box, guests);
        hurl(box, 60.0f);
        CHECK(!watch(box, guests, 6.0).CameOff && box.Shell().State == BallState::None);
    }
    {
        Terrarium box(7u);
        const auto guests = one(CreatureId::Crab);
        fresh(box, guests);
        hurl(box, 400.0f);
        CHECK(!watch(box, guests, 6.0).CameOff && box.Shell().State == BallState::None && !box.Residents()[0].Bare);
    }

    {
        Terrarium box(7u);
        const auto guests = one(CreatureId::Snail);
        fresh(box, guests);
        hurl(box, 400.0f);
        for (double passed = 0.0; passed < 10.0 && box.Shell().State != BallState::Lying; passed += kStep)
            box.Update(guests, size, kStep, true);
        CHECK(box.Shell().State == BallState::Lying && box.Residents()[0].Bare);
        const TerrariumShell before = box.Shell();
        CHECK(!box.GrabShell(before.X + 20.0f, before.Y));
        CHECK(box.GrabShell(before.X, before.Y));
        const float to = before.X > 100.0f ? 30.0f : 170.0f;
        for (int step = 1; step <= 10; ++step)
            box.CarryShell(before.X + (to - before.X) * static_cast<float>(step) / 10.0f, before.Y - 10.0f, 1.0 / 60.0);
        box.DropShell(size);
        CHECK(box.Shell().State == BallState::Flying);
        const Seen seen = watch(box, guests, 120.0);
        CHECK(seen.Crawled && !box.Residents()[0].Bare && box.Shell().State == BallState::None);
    }

    {
        Terrarium box(7u);
        const auto guests = one(CreatureId::Snail);
        fresh(box, guests);
        hurl(box, 400.0f);
        for (double passed = 0.0; passed < 3.0 && !box.Residents()[0].Bare; passed += kStep)
            box.Update(guests, size, kStep, true);
        CHECK(box.Residents()[0].Bare);
        box.Update(guests, size, kStep, false);
        CHECK(!box.Residents()[0].Bare && box.Shell().State == BallState::None);
    }
}

void TestClock()
{
    const double realFrom = Clock::Seconds();
    const double ownFrom = Clock::OwnSeconds();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const double real = Clock::Seconds() - realFrom;
    const double own = Clock::OwnSeconds() - ownFrom;
    CHECK(real > 0.25 && std::fabs(real - own) < 0.04);
#ifdef _WIN32
    const std::uint64_t before = ::GetTickCount64();
    const std::uint64_t ticks = Clock::TickMilliseconds();
    const std::uint64_t after = ::GetTickCount64();
    CHECK(ticks >= before && ticks <= after);
#endif

    {
        Clock::PaceMeter pace;
        double ownAt = 100.0;
        double realAt = 5000.0;
        int changes = 0;
        const auto run = [&](double seconds, double factor)
            {
                for (double passed = 0.0; passed < seconds; passed += 1.0 / 60.0)
                {
                    realAt += 1.0 / 60.0;
                    ownAt += factor / 60.0;
                    changes += pace.Note(ownAt, std::floor(realAt * 64.0) / 64.0) ? 1 : 0;
                }
            };
        run(5.0, 1.0);
        CHECK(pace.Pace() == 1.0 && changes == 0);
        run(5.0, 2.0);
        CHECK(std::fabs(pace.Pace() - 2.0) < 0.05 && changes == 1);
        run(5.0, 0.5);
        CHECK(std::fabs(pace.Pace() - 0.5) < 0.03 && changes == 2);
        run(5.0, 1.02);
        CHECK(pace.Pace() == 1.0 && changes == 3);
        realAt += 3600.0;
        run(3.0, 1.0);
        CHECK(pace.Pace() == 1.0 && changes == 3);
        ownAt -= 50.0;
        run(3.0, 1.0);
        CHECK(pace.Pace() == 1.0 && changes == 3);
    }
}

void TestParkaKidBurst()
{
    const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
    constexpr double kStep = 0.05;
    const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
        {
            for (double passed = 0.0; passed < seconds; passed += kStep)
                box.Update(guests, size, kStep, true);
        };
    const auto kid = [&](Terrarium& box) -> const TerrariumResident&
        {
            return *std::find_if(box.Residents().begin(), box.Residents().end(), [](const TerrariumResident& resident) { return resident.Seed == 1; });
        };
    const auto feetInMess = [&](const Terrarium& box, const TerrariumResident& resident)
        {
            return box.InMess(resident.X + size.SpriteWidth * 0.5f, resident.Y + size.SpriteHeight);
        };

    {
        const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
        Terrarium box(9u);
        box.Update(guests, size, kStep, false);
        run(box, guests, 1.0);
        box.Grab(1);
        box.Carry(1, 100.0f, 40.0f, 0.0);
        for (int step = 1; step <= 6; ++step)
            box.Carry(1, 100.0f + 400.0f * static_cast<float>(step) / 60.0f, 40.0f, 1.0 / 60.0);
        box.Drop(1, size);
        const float wall = size.Width - size.SpriteWidth;
        bool atWall = false;
        bool bouncedBack = false;
        float lastX = 0.0f;
        float lastY = 0.0f;
        float lastTargetY = 0.0f;
        for (double passed = 0.0; passed < 3.0 && kid(box).State != ResidentState::Gone; passed += kStep)
        {
            lastX = kid(box).X;
            lastY = kid(box).Y;
            lastTargetY = kid(box).TargetY;
            box.Update(guests, size, kStep, true);
            if (kid(box).State == ResidentState::Gone)
                break;
            atWall |= kid(box).X >= wall - 0.5f;
            bouncedBack |= atWall && kid(box).VelocityX < 0.0f && kid(box).State == ResidentState::Settling;
        }
        CHECK(atWall && bouncedBack);
        CHECK(kid(box).State == ResidentState::Gone);
        CHECK(lastY >= lastTargetY - 6.0f);
        CHECK(lastX < wall - 4.0f);
        CHECK(box.Messes().size() == 1);
        if (box.Messes().size() == 1)
        {
            const TerrariumMess mess = box.Messes()[0];
            CHECK(std::fabs(mess.X - (lastX + size.SpriteWidth * 0.5f)) < 10.0f);
            CHECK(mess.Y >= 130.0f - kTerrariumGroundDepth && mess.Y <= 130.0f - kTerrariumLakeDepth);
            CHECK(box.InMess(mess.X, mess.Y) && !box.InMess(mess.X + mess.Radius + 1.0f, mess.Y) && !box.InMess(mess.X, mess.Y + mess.Radius * kMessDepthShare + 1.0f));
        }
        box.Grab(1);
        CHECK(kid(box).State == ResidentState::Gone);
        run(box, guests, 25.0);
        CHECK(kid(box).State == ResidentState::Gone && box.Messes().size() == 1);
        run(box, guests, 6.0);
        CHECK(kid(box).State != ResidentState::Gone && box.Messes().empty());
        CHECK(kid(box).State == ResidentState::Entering || kid(box).State == ResidentState::Moving || kid(box).State == ResidentState::Resting);
        CHECK(kid(box).NextBurst > box.Clock() + 100.0);
    }

    {
        const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
        Terrarium box(9u);
        box.Update(guests, size, kStep, false);
        run(box, guests, 1.0);
        box.Grab(1);
        box.Carry(1, 100.0f, 40.0f, 0.0);
        for (int step = 1; step <= 6; ++step)
            box.Carry(1, 100.0f + 400.0f * static_cast<float>(step) / 60.0f, 40.0f, 1.0 / 60.0);
        box.Drop(1, size);
        const float wall = size.Width - size.SpriteWidth;
        for (double passed = 0.0; passed < 1.0 && kid(box).X < wall - 0.5f; passed += 1.0 / 120.0)
            box.Update(guests, size, 1.0 / 120.0, true);
        CHECK(kid(box).State == ResidentState::Settling && kid(box).X >= wall - 0.5f);
        box.Grab(1);
        box.Carry(1, 120.0f, 60.0f, 0.0);
        box.Carry(1, 120.0f, 60.0f, 0.5);
        box.Drop(1, size);
        run(box, guests, 3.0);
        CHECK(kid(box).State != ResidentState::Gone && box.Messes().empty());
    }

    {
        std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
        for (std::uint32_t seed = 2; seed <= 7; ++seed)
            guests.push_back(TerrariumGuest{ seed, Habit::Walks, false, CreatureId::Crab, 0 });
        Terrarium box(11u);
        box.Update(guests, size, kStep, false);
        bool burst = false;
        bool startled = false;
        int insideLate = 0;
        for (double passed = 0.0; passed < 420.0; passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            burst |= kid(box).State == ResidentState::Gone;
            if (box.Messes().empty())
                continue;
            const double age = box.Clock() - box.Messes()[0].Began;
            for (const TerrariumResident& other : box.Residents())
            {
                if (other.Seed == 1)
                    continue;
                startled |= other.Reaction == TerrariumReaction::Startled;
                if (age > 4.0 && other.State == ResidentState::Resting && feetInMess(box, other))
                    ++insideLate;
            }
        }
        CHECK(burst);
        CHECK(insideLate == 0);
        (void)startled;
    }

    {
        const std::vector<TerrariumGuest> asleep{ TerrariumGuest{ 1, Habit::Walks, true, CreatureId::ParkaKid, 0 } };
        Terrarium box(13u);
        box.Update(asleep, size, kStep, false);
        run(box, asleep, 400.0);
        CHECK(kid(box).State != ResidentState::Gone && box.Messes().empty());

        const std::vector<TerrariumGuest> awake{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
        for (double passed = 0.0; passed < 420.0 && kid(box).State != ResidentState::Gone; passed += kStep)
            box.Update(awake, size, kStep, true);
        CHECK(kid(box).State == ResidentState::Gone);
        box.Update(awake, size, kStep, false);
        CHECK(kid(box).State == ResidentState::Resting && box.Messes().empty());
    }
}

void TestCovers()
{
    const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
    constexpr double kStep = 0.05;
    std::vector<TerrariumGuest> guests;
    for (std::uint32_t seed = 1; seed <= 4; ++seed)
        guests.push_back(TerrariumGuest{ seed, Habit::Walks, false, seed % 2 == 0 ? CreatureId::Bunny : CreatureId::Crab, 0 });
    Terrarium box(17u);
    const std::vector<TerrariumCover> covers{ { 60.0f, 100.0f, 3.0f, 12.0f }, { 140.0f, 90.0f, 2.0f, 0.0f } };
    box.SetCovers(covers);
    box.Update(guests, size, kStep, false);
    int inFoot = 0;
    int hidden = 0;
    for (double passed = 0.0; passed < 600.0; passed += kStep)
    {
        box.Update(guests, size, kStep, true);
        for (const TerrariumResident& resident : box.Residents())
        {
            if (resident.State != ResidentState::Resting)
                continue;
            const float middle = resident.X + size.SpriteWidth * 0.5f;
            const float feet = resident.Y + size.SpriteHeight;
            if (resident.Encounter == TerrariumEncounter::None && box.InFoot(middle, feet))
                ++inFoot;
            if (resident.Hiding && feet < covers[0].Ground && feet > covers[0].Ground - 4.0f && std::fabs(middle - covers[0].X) <= covers[0].Cover)
                ++hidden;
            CHECK(!(resident.Hiding && std::fabs(middle - covers[1].X) < 3.0f && feet < covers[1].Ground && feet > covers[1].Ground - 4.0f));
        }
    }
    CHECK(inFoot == 0);
    CHECK(hidden > 0);
    CHECK(box.InFoot(60.0f, 100.0f) && box.InFoot(62.5f, 101.0f) && !box.InFoot(64.0f, 100.0f) && !box.InFoot(60.0f, 104.0f));
}

void TestTug()
{
    const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
    constexpr double kStep = 0.05;
    const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::Demon, 0 } };
    const auto demon = [](const Terrarium& box) -> const TerrariumResident& { return box.Residents()[0]; };
    const auto throwIt = [&](Terrarium& box)
        {
            box.Grab(1);
            box.Carry(1, 100.0f, 20.0f, 0.0);
            for (int step = 1; step <= 4; ++step)
                box.Carry(1, 100.0f + 0.02f * static_cast<float>(step), 20.0f, 1.0 / 60.0);
            box.Drop(1, size);
        };
    const auto fresh = [&](Terrarium& box)
        {
            box.Update(guests, size, kStep, false);
            for (double passed = 0.0; passed < 1.0; passed += kStep)
                box.Update(guests, size, kStep, true);
        };

    {
        Terrarium box(21u);
        fresh(box);
        box.SetPointer(true, 40.0f, 70.0f);
        box.SetTugAllowed(true);
        throwIt(box);
        bool tugged = false;
        bool moved = false;
        float startX = 0.0f;
        for (double passed = 0.0; passed < 8.0; passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            const TerrariumTug tug = box.Tug();
            if (tug.Active && !tugged)
                startX = tug.X;
            if (tug.Active)
            {
                moved |= std::fabs(tug.X - startX) > 8.0f;
                box.SetPointer(true, tug.X - size.SpriteWidth * 0.5f, tug.Y - size.SpriteHeight * 0.5f);
            }
            tugged |= tug.Active;
        }
        CHECK(tugged && moved);
        CHECK(!box.Tug().Active && demon(box).HasTugged);
        for (double passed = 0.0; passed < 6.0; passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            CHECK(!box.Tug().Active);
        }
    }

    {
        Terrarium box(21u);
        fresh(box);
        box.SetPointer(true, 40.0f, 70.0f);
        box.SetTugAllowed(true);
        throwIt(box);
        for (double passed = 0.0; passed < 8.0 && !box.Tug().Active; passed += kStep)
            box.Update(guests, size, kStep, true);
        CHECK(box.Tug().Active);
        box.LetGoOfHand(true);
        CHECK(!box.Tug().Active && demon(box).Reaction == TerrariumReaction::Dizzy);
    }

    {
        Terrarium box(21u);
        fresh(box);
        box.SetPointer(true, 40.0f, 70.0f);
        box.SetTugAllowed(false);
        throwIt(box);
        bool tugged = false;
        bool furious = false;
        for (double passed = 0.0; passed < 8.0; passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            tugged |= box.Tug().Active;
            furious |= demon(box).Reaction == TerrariumReaction::Furious;
        }
        CHECK(furious && !tugged);
    }
}

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
        { "Vdf", TestVdf },
        { "Playtimes", TestPlaytimes },
        { "AppInfo", TestAppInfo },
        { "Library", TestLibrary },
        { "Queue", TestQueue },
        { "CardQueue", TestCardQueue },
        { "Creatures", TestCreatures },
        { "Terrarium", TestTerrarium },
        { "Web policy", TestWebPolicy },
        { "RuntimeLog", TestRuntimeLog },
        { "VariedTimes", TestVariedTimes },
        { "SteamRegistry", TestSteamRegistry },
        { "BondsAndSets", TestBondsAndSets },
        { "Encounters", TestEncounters },
        { "Characters", TestCharacters },
        { "Evolutions", TestEvolutions },
        { "MiniGames", TestMiniGames },
        { "Fetch", TestFetch },
        { "SnailShell", TestSnailShell },
        { "ParkaKidBurst", TestParkaKidBurst },
        { "Clock", TestClock },
        { "Covers", TestCovers },
        { "Tug", TestTug },
        { "LocalTime", TestLocalTime },
        { "ShownGame", TestShownGame },
        { "CardPages", TestCardPages },
        { "MarkGamesWithCards", TestMarkGamesWithCards },
        { "CardSet", TestCardSet },
        { "CardLog", TestCardLog },
        { "SmoothWheel", TestSmoothWheel },
        { "RefundGuard", TestRefundGuard },
        { "Forecast", TestForecast },
        { "HourWindow", TestHourWindow },
        { "Stats", TestStats },
        { "Settings", TestSettings },
        { "Text", TestText },
        { "Errors", TestErrors },
        { "Json", TestJson },
        { "SpriteFile", TestSpriteFile },
        { "SpriteEdit", TestSpriteEdit },
        { "SpriteTimeline", TestSpriteTimeline },
        { "CreatureFiles", TestCreatureFiles },
        { "FileLock", TestFileLock },
    };
    for (const auto& test : tests)
    {
        std::printf("%s\n", test.Name);
        test.Run();
    }
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
