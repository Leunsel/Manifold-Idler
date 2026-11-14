#include "TestKit.h"

#include "Core/AppInfo.h"
#include "Core/Library.h"
#include "Core/Text.h"
#include "Core/Vdf.h"

using namespace Idler;

namespace
{
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
}

namespace Tests
{
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
}
