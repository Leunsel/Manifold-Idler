#include "TestKit.h"

#include "Core/Backups.h"
#include "Core/Cards.h"
#include "Core/Clock.h"
#include "Core/CrashReport.h"
#include "Core/Creatures.h"
#include "Core/Errors.h"
#include "Core/FileLock.h"
#include "Core/Json.h"
#include "Core/Library.h"
#include "Core/Releases.h"
#include "Core/RuntimeLog.h"
#include "Core/Settings.h"
#include "Core/WebPolicy.h"
#include "Core/Text.h"
#include "Core/Version.h"
#include "Core/Zip.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Idler;

namespace
{
    constexpr std::string_view kPythonZip =
        "504b0304140000000800c5ab6c5d301314a337000000a5000000120000004d616e69666f6c642d49646c65722e696e69b356084acd4dcd4d4a2d4a4de1e50acec92f29b635e1e5722e"
        "4a4d2c292d4ab54d2bca4f57a8513034378002851a05635e2e6bba6a0300504b0304140000000000c6ab6c5d7f2445982800000028000000140000004d616e69666f6c642d49646c6572"
        "2e7374617473323032362d31312d3132207c2037207c20343830207c2033363030207c2032207c20313830300d0a504b01021400140000000800c5ab6c5d301314a337000000a50000"
        "001200000000000000000000008001000000004d616e69666f6c642d49646c65722e696e69504b01021400140000000000c6ab6c5d7f24459828000000280000001400000000000000"
        "000000008001670000004d616e69666f6c642d49646c65722e7374617473504b0506000000000200020082000000c10000000000";

    constexpr std::string_view kPythonZipWithDescriptor =
        "504b0304140008000800c5ab6c5d000000000000000000000000120000004d616e69666f6c642d49646c65722e696e69b356084acd4dcd4d4a2d4a4de1e50acec92f29b635e1e5722e"
        "4a4d2c292d4ab54d2bca4f57a8513034378002851a05635e2e6bba6a0300504b0708301314a337000000a5000000504b0304140008000000c6ab6c5d0000000000000000000000001400"
        "00004d616e69666f6c642d49646c65722e7374617473323032362d31312d3132207c2037207c20343830207c2033363030207c2032207c20313830300d0a504b07087f244598280000"
        "0028000000504b01021400140008000800c5ab6c5d301314a337000000a50000001200000000000000000000008001000000004d616e69666f6c642d49646c65722e696e69504b0102"
        "1400140008000000c6ab6c5d7f24459828000000280000001400000000000000000000008001770000004d616e69666f6c642d49646c65722e7374617473504b050600000000020002"
        "0082000000e10000000000";

    std::string FromHex(std::string_view hex)
    {
        std::string bytes;
        for (std::size_t at = 0; at + 1 < hex.size(); at += 2)
            bytes.push_back(static_cast<char>(std::stoi(std::string(hex.substr(at, 2)), nullptr, 16)));
        return bytes;
    }

    std::string ReadText(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    void WriteText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
}

namespace Tests
{
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
        CHECK(entries[0].Trace.starts_with("ProgramTests.cpp:TestRuntimeLog:"));

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

        CHECK(read(previous).find("[INFO] [ProgramTests.cpp:TestRuntimeLog:") != std::string::npos);
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

    void TestZip()
    {
        CHECK(Zip::Crc32("") == 0u);
        CHECK(Zip::Crc32("123456789") == 0xCBF43926u);

        std::string lines;
        for (int line = 0; line < 2000; ++line)
            lines += "2026-11-" + std::to_string(10 + line % 20) + " | 7 | " + std::to_string(400 + line) + " | 3600 | 2 | 1800\r\n";
        std::string noise;
        std::uint32_t seed = 12345u;
        for (int index = 0; index < 4096; ++index)
        {
            seed = seed * 1664525u + 1013904223u;
            noise.push_back(static_cast<char>(seed >> 24));
        }
        const std::time_t when = 1794000000;
        const std::vector<Zip::Entry> entries = {
            { "Manifold-Idler.stats", lines, when },
            { "empty.txt", "", when },
            { "noise.bin", noise, when },
            { "Grüße.ini", "Slots=2\r\n", when },
        };
        const std::optional<std::string> archive = Zip::Write(entries);
        CHECK(archive.has_value());
        if (archive)
        {
            CHECK(archive->size() < lines.size() / 3 + noise.size() + 1000);
            const std::optional<std::vector<Zip::Entry>> back = Zip::Read(*archive);
            CHECK(back && back->size() == entries.size());
            if (back && back->size() == entries.size())
            {
                bool same = true;
                for (std::size_t index = 0; index < entries.size(); ++index)
                {
                    const Zip::Entry& read = (*back)[index];
                    same &= read.Name == entries[index].Name && read.Data == entries[index].Data;
                    same &= std::abs(static_cast<long long>(read.Time) - static_cast<long long>(when)) <= 2;
                }
                CHECK(same);
            }
            std::string damaged = *archive;
            damaged[60] = static_cast<char>(damaged[60] ^ 0x55);
            CHECK(!Zip::Read(damaged));
            CHECK(!Zip::Read(archive->substr(0, archive->size() - 1)));
            CHECK(!Zip::Read(archive->substr(0, archive->size() / 2)));
        }
        CHECK(!Zip::Read(""));
        CHECK(!Zip::Read(std::string_view("PK\x05\x06", 4)));
        CHECK(!Zip::Write({ { "", "x", 0 } }));

        std::string ini;
        for (int copy = 0; copy < 3; ++copy)
            ini += "; Remembered\r\nSlots=4\r\nCreature=frog | 1700000000 | 3\r\n";
        for (const std::string_view hex : { kPythonZip, kPythonZipWithDescriptor })
        {
            const std::optional<std::vector<Zip::Entry>> read = Zip::Read(FromHex(hex));
            CHECK(read && read->size() == 2);
            if (!read || read->size() != 2)
                continue;
            CHECK((*read)[0].Name == "Manifold-Idler.ini" && (*read)[0].Data == ini);
            CHECK((*read)[1].Name == "Manifold-Idler.stats" && (*read)[1].Data == "2026-11-12 | 7 | 480 | 3600 | 2 | 1800\r\n");
            std::tm local{};
            CHECK(LocalTime((*read)[0].Time, local) && local.tm_year == 126 && local.tm_mon == 10 && local.tm_mday == 12 && local.tm_hour == 21 &&
                local.tm_min == 30 && local.tm_sec == 10);
        }
    }

    void TestBackups()
    {
        using namespace Backups;

        const std::filesystem::path folder = TestFile("-backups");
        std::error_code error;
        std::filesystem::remove_all(folder, error);
        std::filesystem::create_directories(folder, error);
        const std::filesystem::path settings = folder / "Manifold-Idler.ini";
        const std::filesystem::path stats = folder / "Manifold-Idler.stats";
        const std::filesystem::path backups = folder / "Manifold-Idler.backups";

        std::tm noon{};
        noon.tm_year = 126;
        noon.tm_mon = 10;
        noon.tm_mday = 13;
        noon.tm_hour = 12;
        noon.tm_min = 14;
        noon.tm_sec = 5;
        noon.tm_isdst = -1;
        const std::time_t day = std::mktime(&noon);
        constexpr std::time_t kDay = 86400;

        CHECK(FolderOf(settings) == backups);
        CHECK(FilesOf(settings).size() == 3 && FilesOf(settings)[1] == stats && FilesOf(settings)[2] == folder / "Manifold-Idler.cards");
        CHECK(FileName(BackupKind::Daily, day) == "2026-11-13.zip");
        CHECK(FileName(BackupKind::Saved, day) == "2026-11-13 12-14-05.zip");
        CHECK(FileName(BackupKind::BeforeRestore, day) == "2026-11-13 12-14-05 before restore.zip");
        bool parsed = true;
        for (const BackupKind kind : { BackupKind::Daily, BackupKind::Saved, BackupKind::BeforeRestore })
        {
            const std::optional<Backup> backup = FromFile(backups / FileName(kind, day));
            parsed &= backup && backup->Kind == kind && backup->Time <= day && day - backup->Time < kDay;
        }
        CHECK(parsed);
        CHECK(FromFile("2026-11-13 12-14-05.zip") && FromFile("2026-11-13 12-14-05.zip")->Time == day);
        CHECK(!FromFile("notes.zip") && !FromFile("2026-11-13.txt") && !FromFile("2026-13-01.zip") && !FromFile("2026-11-13 25-00-00.zip"));
        CHECK(!FromFile("2026-11-13 12-14-05 copy.zip") && !FromFile("2026-11-13 (2).zip") && !FromFile("2026-11-13.zip.saving"));

        CHECK(Make(settings, BackupKind::Daily, day).What == Outcome::NothingToBackUp);
        CHECK(List(settings).empty() && !MadeToday(settings, day));

        const std::string firstSettings = "Slots=2\r\nCreature=frog | 1700000000 | 3\r\n";
        const std::string firstStats = "2026-11-12 | 7 | 480 | 7200 | 2 | 3600\r\n";
        WriteText(settings, firstSettings);
        WriteText(stats, firstStats);
        const Result daily = Make(settings, BackupKind::Daily, day);
        CHECK(daily.What == Outcome::Made && daily.File == backups / "2026-11-13.zip" && MadeToday(settings, day));
        CHECK(Make(settings, BackupKind::Daily, day + 3600).What == Outcome::Unchanged);
        CHECK(Make(settings, BackupKind::Saved, day + 60).What == Outcome::Unchanged);

        std::size_t startCreatures = 0;
        for (const CreatureInfo& info : AllCreatures())
            startCreatures += info.Kind == UnlockKind::Start ? 1 : 0;
        const std::optional<Contents> contents = Inspect(settings, daily.File);
        CHECK(contents && contents->HasSettings && contents->HasStats && !contents->HasCards);
        CHECK(contents && contents->Seconds == 7200.0 && contents->Creatures == startCreatures + 1);

        WriteText(stats, firstStats + "2026-11-13 | 7 | 620 | 60 | 1 | 60\r\n");
        const Result saved = Make(settings, BackupKind::Saved, day + 120);
        CHECK(saved.What == Outcome::Made && saved.File == backups / FileName(BackupKind::Saved, day + 120));
        CHECK(List(settings).size() == 2 && List(settings).front().Kind == BackupKind::Saved && List(settings).front().Bytes > 0);
        CHECK(Make(settings, BackupKind::Daily, day + kDay).What == Outcome::Made);
        CHECK(Make(settings, BackupKind::Daily, day + 2 * kDay).What == Outcome::Unchanged && !MadeToday(settings, day + 2 * kDay));

        WriteText(settings, "Slots=9\r\n");
        WriteText(stats, "");
        WriteText(backups / "notes.txt", "mine");
        WriteText(backups / "mine.zip", "mine");
        const Result restored = Restore(settings, daily.File, day + 3 * kDay);
        CHECK(restored.What == Outcome::Made && restored.File == backups / FileName(BackupKind::BeforeRestore, day + 3 * kDay));
        CHECK(ReadText(settings) == firstSettings && ReadText(stats) == firstStats);
        const std::optional<Contents> before = Inspect(settings, restored.File);
        CHECK(before && before->HasSettings && before->HasStats && before->Seconds == 0.0 && before->Creatures == startCreatures);
        CHECK(Restore(settings, restored.File, day + 3 * kDay + 5).Ok() && ReadText(settings) == "Slots=9\r\n" && ReadText(stats).empty());

        const std::filesystem::path broken = backups / "2026-11-01.zip";
        WriteText(broken, "not a zip");
        const std::size_t listed = List(settings).size();
        const Result refused = Restore(settings, broken, day + 4 * kDay);
        CHECK(!refused.Ok() && refused.Problem.find("MI-5018") != std::string::npos);
        CHECK(ReadText(settings) == "Slots=9\r\n" && List(settings).size() == listed);
        CHECK(!Inspect(settings, broken));

        for (int extra = 5; extra < 20; ++extra)
        {
            WriteText(stats, firstStats + std::to_string(extra));
            Make(settings, BackupKind::Daily, day + extra * kDay);
        }
        for (int extra = 0; extra < 8; ++extra)
        {
            WriteText(stats, firstStats + "saved " + std::to_string(extra));
            Make(settings, BackupKind::Saved, day + 20 * kDay + extra * 60);
        }
        std::size_t dailies = 0;
        std::size_t others = 0;
        for (const Backup& backup : List(settings))
            (backup.Kind == BackupKind::Daily ? dailies : others) += 1;
        CHECK(dailies == kKeepDaily && others == kKeepOthers);
        CHECK(List(settings).front().File == backups / FileName(BackupKind::Saved, day + 20 * kDay + 7 * 60));
        CHECK(std::filesystem::exists(backups / "notes.txt") && std::filesystem::exists(backups / "mine.zip") && !std::filesystem::exists(broken));
        CHECK(!std::filesystem::exists(backups / "2026-11-13.zip") && std::filesystem::exists(backups / FileName(BackupKind::Daily, day + 19 * kDay)));

        std::filesystem::remove_all(folder, error);
    }

    void TestCrashReport()
    {
        using namespace CrashReport;

        const std::filesystem::path root = TestFile("-crash");
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);
        const std::filesystem::path settings = root / "Manifold-Idler.ini";
        const std::filesystem::path folder = FolderOf(settings);

        CHECK(folder == root / "Manifold-Idler.crashes");
        CHECK(MarkerOf(settings) == root / "Manifold-Idler.crashed");
        CHECK(Hex(0) == "0x0" && Hex(0x1a2b) == "0x1a2b");
        CHECK(ExceptionText(0xC0000005u) == "access violation" && ExceptionText(0xC00000FDu) == "stack overflow");
        CHECK(ExceptionText(0xE06D7363u) == "a C++ exception nobody caught" && ExceptionText(0x12345678u) == "exception 0x12345678");
        CHECK(AccessText(0, 0x10) == "reading address 0x10" && AccessText(1, 0) == "writing address 0x0" && AccessText(8, 0x7ff) == "running code at address 0x7ff");
        CHECK(SignalText(SIGSEGV, 0x8) == "segmentation fault at address 0x8" && SignalText(SIGABRT, 0) == "the program called abort");

        const Frame named{ "Manifold-Idler.exe", 0x1a2b3c, "Idler::UI::DrawMain", 0x12, "ScreenMain.cpp", 5910 };
        const Frame bare{ "ntdll.dll", 0x9f00, "", 0, "", 0 };
        CHECK(FrameText(0, named) == "#0 Manifold-Idler.exe+0x1a2b3c Idler::UI::DrawMain+0x12 (ScreenMain.cpp:5910)");
        CHECK(FrameText(7, bare) == "#7 ntdll.dll+0x9f00");
        CHECK(FrameText(1, Frame{}) == "#1 ?+0x0");
        const Frame again{ "Manifold-Idler.exe", 0x40, "Deeper", 0x29, "", 0 };
        const std::vector<std::string> folded = StackLines({ bare, again, again, again, again, named });
        CHECK(folded.size() == 4 && folded[1] == FrameText(1, again) && folded[2] == "#2 to #4 the same as #1" && folded[3] == FrameText(5, named));
        CHECK(StackLines({ again, again }).size() == 2 && StackLines({}).empty());

        std::tm noon{};
        noon.tm_year = 126;
        noon.tm_mon = 10;
        noon.tm_mday = 13;
        noon.tm_hour = 12;
        noon.tm_min = 14;
        noon.tm_sec = 5;
        noon.tm_isdst = -1;
        const std::time_t time = std::mktime(&noon);
        CHECK(ReportName(time) == "2026-11-13 12-14-05");

        Crash crash;
        crash.Time = time;
        crash.Problem = "access violation, reading address 0x0";
        crash.Program = "C:\\Games\\Manifold-Idler.exe";
        crash.Version = "1.3.0";
        crash.Built = "Nov 13 2026";
        crash.Thread = 4242;
        crash.Stack = { named, bare };
        crash.Dump = "2026-11-13 12-14-05.dmp";
        const std::string text = ReportText(crash);
        CHECK(text.starts_with("Manifold Idler ended unexpectedly on 2026-11-13 at 12:14:05.\n"));
        CHECK(text.find("Problem: access violation, reading address 0x0\n") != std::string::npos);
        CHECK(text.find("Program: C:\\Games\\Manifold-Idler.exe, version 1.3.0, built Nov 13 2026\n") != std::string::npos && text.find("Thread: 4242\n") != std::string::npos);
        crash.Version.clear();
        CHECK(ReportText(crash).find("Program: C:\\Games\\Manifold-Idler.exe, built Nov 13 2026\n") != std::string::npos);
        crash.Version = "1.3.0";
        CHECK(text.find("\nStack:\n#0 Manifold-Idler.exe+0x1a2b3c") != std::string::npos && text.find("\nDump: 2026-11-13 12-14-05.dmp\n") != std::string::npos);
        Crash empty;
        empty.Time = time;
        empty.Problem = "the program called abort";
        CHECK(ReportText(empty).find("Stack:\n(none could be read)\n") != std::string::npos && ReportText(empty).find("Dump:") == std::string::npos);

        const std::string lines = LogLines(crash, folder / "2026-11-13 12-14-05.txt");
        CHECK(lines.starts_with("[12:14:05] [ERROR] [Crash] The program ended unexpectedly: access violation, reading address 0x0. (MI-5020)\n"));
        CHECK(lines.find("[12:14:05] [ERROR] [Crash] #1 ntdll.dll+0x9f00\n") != std::string::npos);
        CHECK(lines.find("[12:14:05] [ERROR] [Crash] The report is in ") != std::string::npos && std::count(lines.begin(), lines.end(), '\n') == 4);
        const std::string withoutReport = LogLines(crash, {});
        CHECK(std::count(withoutReport.begin(), withoutReport.end(), '\n') == 3);

        const std::optional<LastCrash> read = ReadReport(text);
        CHECK(read && read->Problem == crash.Problem && read->Stack.size() == 2 && read->Stack[0] == FrameText(0, named));
        CHECK(!ReadReport("hello\n#0 nothing\n"));

        CHECK(!TakeLastCrash(settings));
        std::filesystem::create_directories(folder, error);
        WriteText(folder / "2026-11-13 12-14-05.txt", text);
        WriteText(MarkerOf(settings), "2026-11-13 12-14-05\r\n");
        const std::optional<LastCrash> last = TakeLastCrash(settings);
        CHECK(last && last->Report == folder / "2026-11-13 12-14-05.txt" && last->Problem == crash.Problem && last->Stack.size() == 2);
        CHECK(!std::filesystem::exists(MarkerOf(settings)) && !TakeLastCrash(settings));
        WriteText(MarkerOf(settings), "../../somewhere\n");
        const std::optional<LastCrash> odd = TakeLastCrash(settings);
        CHECK(odd && odd->Report.empty() && odd->Stack.empty() && !std::filesystem::exists(MarkerOf(settings)));

        for (int index = 0; index < 12; ++index)
        {
            const std::string name = ReportName(time + index * 60);
            WriteText(folder / (name + ".txt"), "report");
            WriteText(folder / (name + ".dmp"), "dump");
        }
        WriteText(folder / "notes.txt", "mine");
        CHECK(Prune(settings) == 4);
        CHECK(!std::filesystem::exists(folder / "2026-11-13 12-14-05.txt") && !std::filesystem::exists(folder / "2026-11-13 12-15-05.dmp"));
        CHECK(std::filesystem::exists(folder / "2026-11-13 12-16-05.txt") && std::filesystem::exists(folder / "2026-11-13 12-25-05.dmp"));
        CHECK(std::filesystem::exists(folder / "notes.txt") && Prune(settings) == 0);

        std::filesystem::remove_all(root, error);
    }

    void TestReleases()
    {
        using namespace Releases;

        CHECK(ParseVersion("1.3.0") && (*ParseVersion("1.3.0") == std::array<int, 3>{ 1, 3, 0 }));
        CHECK(ParseVersion("v12.0.7") && (*ParseVersion("v12.0.7") == std::array<int, 3>{ 12, 0, 7 }));
        CHECK(!ParseVersion("") && !ParseVersion("v") && !ParseVersion("1.3") && !ParseVersion("1.3.0.1") && !ParseVersion("1.3.0-beta"));
        CHECK(!ParseVersion("1..3") && !ParseVersion(".1.3") && !ParseVersion("1.3.") && !ParseVersion("1.3.x") && !ParseVersion("1234567.0.0"));
        CHECK(IsNewer("1.4.0", "1.3.0") && IsNewer("v1.3.1", "1.3.0") && IsNewer("2.0.0", "1.99.99") && IsNewer("1.10.0", "1.9.0"));
        CHECK(!IsNewer("1.3.0", "1.3.0") && !IsNewer("1.2.0", "1.3.0") && !IsNewer("", "1.3.0") && !IsNewer("1.4.0", "") && !IsNewer("1.4", "1.3.0"));

        CHECK(PageOf("1.4.0") == "https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0");
        CHECK(PageOf("v1.4.0") == "https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0");
        CHECK(PageOf("nonsense") == "https://github.com/Leunsel/Manifold-Idler/releases");
        CHECK(IsReleasePage("https://github.com/Leunsel/Manifold-Idler/releases") && IsReleasePage("https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0"));
        CHECK(!IsReleasePage("https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0/../../evil") && !IsReleasePage("https://github.com/Leunsel/Manifold-Idler/releasesx"));
        CHECK(!IsReleasePage("https://github.com.evil.example/Leunsel/Manifold-Idler/releases") && !IsReleasePage("http://github.com/Leunsel/Manifold-Idler/releases"));
        CHECK(!IsReleasePage("https://github.com/Leunsel/Manifold-Idler/releases/download/v1.4.0/x.zip"));

        const std::string answer =
            "{\"url\":\"https://api.github.com/repos/Leunsel/Manifold-Idler/releases/1\",\"html_url\":\"https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0\","
            "\"id\":1,\"tag_name\":\"v1.4.0\",\"name\":\"v1.4.0\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-20T18:00:00Z\","
            "\"assets\":[{\"name\":\"Manifold-Idler-windows-x64.zip\",\"size\":1234}],\"body\":\"What's new\\n- Backups\"}";
        std::string problem;
        const std::optional<Release> release = ParseLatest(answer, problem);
        CHECK(release && release->Version == "1.4.0" && release->Page == "https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0");

        std::string elsewhere = answer;
        elsewhere.replace(elsewhere.find("https://github.com/Leunsel/Manifold-Idler/releases/tag/v1.4.0"), 61, "https://evil.example/download/1.4.0/whatever/now");
        const std::optional<Release> safe = ParseLatest(elsewhere, problem);
        CHECK(safe && safe->Page == PageOf("1.4.0"));

        std::string draft = answer;
        draft.replace(draft.find("\"draft\":false"), 13, "\"draft\":true");
        CHECK(!ParseLatest(draft, problem) && problem == "the newest release is not final");
        std::string early = answer;
        early.replace(early.find("\"prerelease\":false"), 18, "\"prerelease\":true");
        CHECK(!ParseLatest(early, problem));
        CHECK(!ParseLatest("{\"message\":\"Not Found\"}", problem) && problem == "the answer names no release");
        CHECK(!ParseLatest("{\"tag_name\":\"nightly\"}", problem) && problem.starts_with("the newest release has no version number"));
        CHECK(!ParseLatest("[1,2]", problem) && !ParseLatest("not json", problem) && !ParseLatest("", problem));

        CHECK(Due(0, 1794000000u) && Due(1794000000u, 1794000000u + kCheckSeconds) && Due(1794000000u, 1793000000u));
        CHECK(!Due(1794000000u, 1794000000u) && !Due(1794000000u, 1794000000u + kCheckSeconds - 1));

        Settings options;
        CHECK(options.UpdateCheck && options.UpdateCheckedAt == 0 && options.UpdateLatest.empty());
        options.Read("UpdateCheck=false\r\nUpdateCheckedAt=1794000000\r\nUpdateLatest=1.4.0\r\nUpdateNoted=1.4.0\r\n");
        CHECK(!options.UpdateCheck && options.UpdateCheckedAt == 1794000000u && options.UpdateLatest == "1.4.0" && options.UpdateNoted == "1.4.0");
        const std::filesystem::path saved = TestFile("-updates.ini");
        CHECK(options.Save(saved));
        Settings again;
        again.Load(saved);
        CHECK(!again.UpdateCheck && again.UpdateCheckedAt == 1794000000u && again.UpdateLatest == "1.4.0" && again.UpdateNoted == "1.4.0");
        std::error_code error;
        std::filesystem::remove(saved, error);
    }

    void TestVersion()
    {
        const std::string numbers = std::to_string(IDLER_VERSION_MAJOR) + "." + std::to_string(IDLER_VERSION_MINOR) + "." + std::to_string(IDLER_VERSION_PATCH);
        CHECK(std::string(kVersion) == numbers && std::string(IDLER_VERSION_TEXT) == numbers);
    }
}
