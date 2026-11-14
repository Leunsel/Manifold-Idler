#include "TestKit.h"

#include "Core/Cards.h"
#include "Core/Library.h"
#include "Core/Stats.h"
#include "Core/SmoothWheel.h"
#include "Core/WebPolicy.h"

using namespace Idler;

namespace Tests
{
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
}
