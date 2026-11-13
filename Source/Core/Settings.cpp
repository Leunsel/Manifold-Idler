#include "Core/Settings.h"

#include "Core/Text.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace Idler
{
    namespace
    {
        bool ParseBool(std::string_view text, bool fallback)
        {
            if (EqualsIgnoreCase(text, "true") || text == "1")
                return true;
            if (EqualsIgnoreCase(text, "false") || text == "0")
                return false;
            return fallback;
        }

        int ParseInt(std::string_view text, int fallback)
        {
            int value = 0;
            const char* end = text.data() + text.size();
            const auto [at, error] = std::from_chars(text.data(), end, value);
            return error == std::errc{} && at == end ? value : fallback;
        }

        std::vector<std::string_view> SplitBars(std::string_view text)
        {
            std::vector<std::string_view> parts;
            while (true)
            {
                const auto bar = text.find('|');
                parts.push_back(Trim(text.substr(0, bar)));
                if (bar == std::string_view::npos)
                    return parts;
                text.remove_prefix(bar + 1);
            }
        }

        float ParseHours(std::string_view text, float fallback)
        {
            const std::string number(text);
            char* end = nullptr;
            const float value = std::strtof(number.c_str(), &end);
            if (end == number.c_str() || *end != '\0' || !(value >= 0.0f) || value > 100000.0f)
                return fallback;
            return value;
        }

        bool ParseListLine(std::string_view text, StoredList& list)
        {
            const auto parts = SplitBars(text);
            if (parts.size() != 2 || parts[1].empty())
                return false;
            const auto account = ParseUnsigned(parts[0]);
            if (!account)
                return false;
            list.Account = *account;
            list.Name = parts[1];
            return true;
        }

        const char* GoalToken(GoalKind kind)
        {
            switch (kind)
            {
            case GoalKind::ForHours: return "for";
            case GoalKind::UntilTotal: return "until";
            case GoalKind::CardDrops: return "cards";
            default: return "none";
            }
        }

        constexpr std::string_view kSeasonKeys[] = { "date", "spring", "summer", "autumn", "winter" };

        std::string_view SeasonKey(SceneSeason season)
        {
            return kSeasonKeys[static_cast<std::size_t>(season)];
        }

        SceneSeason ParseSeason(std::string_view text)
        {
            for (std::size_t index = 0; index < std::size(kSeasonKeys); ++index)
            {
                if (text == kSeasonKeys[index])
                    return static_cast<SceneSeason>(index);
            }
            return SceneSeason::ByDate;
        }

        GoalKind ParseGoalKind(std::string_view text)
        {
            if (text == "for")
                return GoalKind::ForHours;
            if (text == "until")
                return GoalKind::UntilTotal;
            if (text == "cards")
                return GoalKind::CardDrops;
            return GoalKind::None;
        }

        bool ParseQueueLine(std::string_view text, StoredEntry& stored)
        {
            const auto parts = SplitBars(text);
            if (parts.size() != 6 && parts.size() != 8 && parts.size() != 9)
                return false;
            const auto account = ParseUnsigned(parts[0]);
            const auto app = ParseUnsigned(parts[1]);
            if (!account || !app || *app == 0)
                return false;
            stored.Account = *account;
            stored.Entry.AppId = *app;
            stored.Entry.Goal.Kind = ParseGoalKind(parts[2]);
            stored.Entry.Goal.Hours = ParseHours(parts[3], stored.Entry.Goal.Hours);
            stored.Entry.IdledSeconds = static_cast<double>(ParseUnsigned(parts[4]).value_or(0));
            stored.Entry.BaselineMinutes = ParseUnsigned(parts[5]).value_or(0);
            if (parts.size() >= 8)
            {
                stored.Entry.IgnoreRefund = ParseBool(parts[6], false);
                if (!parts[7].empty())
                    stored.List = parts[7];
            }
            if (parts.size() == 9)
                stored.Entry.GoalFactor = std::clamp(ParseHours(parts[8], 1.0f), 0.1f, 10.0f);
            return true;
        }
    }

    const char* SceneSeasonName(SceneSeason season)
    {
        switch (season)
        {
        case SceneSeason::Spring: return "Spring";
        case SceneSeason::Summer: return "Summer";
        case SceneSeason::Autumn: return "Autumn";
        case SceneSeason::Winter: return "Winter";
        default: return "By the date";
        }
    }

    SceneSeason SeasonIn(SceneSeason choice, int month)
    {
        if (choice != SceneSeason::ByDate)
            return choice;
        if (month >= 3 && month <= 5)
            return SceneSeason::Spring;
        if (month >= 6 && month <= 8)
            return SceneSeason::Summer;
        if (month >= 9 && month <= 11)
            return SceneSeason::Autumn;
        return SceneSeason::Winter;
    }

    void Settings::Load(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return;
        Read(std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
    }

    void Settings::Read(std::string_view source)
    {
        std::string content(source);
        if (content.starts_with("\xEF\xBB\xBF"))
            content.erase(0, 3);

        std::istringstream lines(content);
        std::string line;
        while (std::getline(lines, line))
        {
            const std::string_view text = Trim(line);
            const auto equals = text.find('=');
            if (text.empty() || text.front() == ';' || equals == std::string_view::npos)
                continue;

            const std::string_view key = Trim(text.substr(0, equals));
            const std::string_view value = Trim(text.substr(equals + 1));
            if (key == "X")
            {
                X = ParseInt(value, X);
                Placed = true;
            }
            else if (key == "Y")
            {
                Y = ParseInt(value, Y);
                Placed = true;
            }
            else if (key == "Width")
                Width = std::clamp(ParseInt(value, Width), 0, 32000);
            else if (key == "Height")
                Height = std::clamp(ParseInt(value, Height), 0, 32000);
            else if (key == "Maximised")
                Maximised = ParseBool(value, Maximised);
            else if (key == "Slots")
                Slots = std::clamp(ParseInt(value, Slots), 1, kMaxSlots);
            else if (key == "GoalKind")
                DefaultGoal.Kind = ParseGoalKind(value);
            else if (key == "GoalHours")
                DefaultGoal.Hours = ParseHours(value, DefaultGoal.Hours);
            else if (key == "PauseWhilePlaying")
                PauseWhilePlaying = ParseBool(value, PauseWhilePlaying);
            else if (key == "KeepInTray")
                KeepInTray = ParseBool(value, KeepInTray);
            else if (key == "ResumeOnStart")
                ResumeOnStart = ParseBool(value, ResumeOnStart);
            else if (key == "WasActive")
                WasActive = ParseBool(value, WasActive);
            else if (key == "KeepAwake")
                KeepAwake = ParseBool(value, KeepAwake);
            else if (key == "RefundGuard")
                RefundGuard = ParseBool(value, RefundGuard);
            else if (key == "OnlyWhenAway")
                OnlyWhenAway = ParseBool(value, OnlyWhenAway);
            else if (key == "AwayMinutes")
                AwayMinutes = ParseHours(value, AwayMinutes);
            else if (key == "OnlyInHours")
                OnlyInHours = ParseBool(value, OnlyInHours);
            else if (key == "FromHour")
                FromHour = std::clamp(ParseInt(value, FromHour), 0, 23);
            else if (key == "ToHour")
                ToHour = std::clamp(ParseInt(value, ToHour), 0, 23);
            else if (key == "VaryTimes")
                VaryTimes = ParseBool(value, VaryTimes);
            else if (key == "VarySpread")
                VarySpread = std::clamp(ParseHours(value, VarySpread), 0.0f, 90.0f);
            else if (key == "CardWarmupHours")
                CardWarmupHours = ParseHours(value, CardWarmupHours);
            else if (key == "ShownGame")
            {
                const auto parts = SplitBars(value);
                if (parts.size() == 2)
                {
                    const auto account = ParseUnsigned(parts[0]);
                    const auto app = ParseUnsigned(parts[1]);
                    if (account && app)
                        ShownGames.push_back({ *account, *app });
                }
            }
            else if (key == "InvisibleWhileIdling")
                InvisibleWhileIdling = ParseBool(value, InvisibleWhileIdling);
            else if (key == "SetInvisible")
                SetInvisible = ParseBool(value, SetInvisible);
            else if (key == "List")
            {
                StoredList list;
                if (ParseListLine(value, list))
                    Lists.push_back(std::move(list));
            }
            else if (key == "ActiveList")
            {
                StoredList list;
                if (ParseListLine(value, list))
                    ActiveLists.push_back(std::move(list));
            }
            else if (key == "Queue")
            {
                StoredEntry stored;
                if (ParseQueueLine(value, stored))
                    Queue.push_back(std::move(stored));
            }
            else if (key == "ShowTerrarium")
                ShowTerrarium = ParseBool(value, ShowTerrarium);
            else if (key == "LibraryCovers")
                LibraryCovers = ParseBool(value, LibraryCovers);
            else if (key == "QueueCovers")
                QueueCovers = ParseBool(value, QueueCovers);
            else if (key == "ShowLog")
                ShowLog = ParseBool(value, ShowLog);
            else if (key == "Season")
                Season = ParseSeason(value);
            else if (key == "CreatureHome")
                CreatureHome = ParseBool(value, CreatureHome);
            else if (key == "HomeBall")
                HomeBall = ParseBool(value, HomeBall);
            else if (key == "CreaturesTugMouse")
                CreaturesTugMouse = ParseBool(value, CreaturesTugMouse);
            else if (key == "Creature")
            {
                const auto parts = SplitBars(value);
                if (parts.size() >= 2 && !parts[0].empty())
                {
                    const std::string_view form = parts.size() >= 4 ? parts[3] : std::string_view();
                    Creatures.push_back({ std::string(parts[0]), ParseUnsigned(parts[1]).value_or(0), parts.size() >= 3 ? ParseUnsigned(parts[2]).value_or(0) : 0,
                        form == "evolved" || form == "first", form == "first" });
                }
            }
            else if (key == "CreaturePlay")
            {
                const auto parts = SplitBars(value);
                if (!parts.empty() && !parts[0].empty())
                {
                    StoredPlay play{ std::string(parts[0]), {} };
                    const auto number = [&](std::size_t index) { return index < parts.size() ? ParseUnsigned(parts[index]).value_or(0) : 0u; };
                    play.Record.Minutes = number(1);
                    play.Record.Day = static_cast<std::int32_t>(number(2));
                    play.Record.MinutesThatDay = number(3);
                    play.Record.Games = number(4);
                    play.Record.Fetches = number(5);
                    for (std::size_t game = 0; game < kMiniGameCount; ++game)
                        play.Record.Best[game] = number(6 + game);
                    std::erase_if(CreaturePlays, [&](const StoredPlay& each) { return each.Key == play.Key; });
                    CreaturePlays.push_back(std::move(play));
                }
            }
            else if (key == "CreatureBuddy")
                CreatureBuddy = std::string(value);
            else if (key == "CreatureGoals")
                CreatureGoals = ParseUnsigned(value).value_or(0);
            else if (key == "CreatureBreaks")
                CreatureBreaks = ParseUnsigned(value).value_or(0);
            else if (key == "CreaturePets")
                CreaturePets = ParseUnsigned(value).value_or(0);
        }
    }

    bool Settings::Save(const std::filesystem::path& path) const
    {
        auto temporary = path;
        temporary += L".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;

            file << "; Remembered by Manifold-Idler. Safe to delete.\r\n";
            if (Placed)
                file << "X=" << X << "\r\nY=" << Y << "\r\n";
            if (Width > 0 && Height > 0)
                file << "Width=" << Width << "\r\nHeight=" << Height << "\r\n";
            file << "Maximised=" << (Maximised ? "true" : "false") << "\r\n";
            file << "Slots=" << Slots << "\r\n";
            file << "GoalKind=" << GoalToken(DefaultGoal.Kind) << "\r\n";
            file << "GoalHours=" << DefaultGoal.Hours << "\r\n";
            file << "PauseWhilePlaying=" << (PauseWhilePlaying ? "true" : "false") << "\r\n";
            file << "KeepInTray=" << (KeepInTray ? "true" : "false") << "\r\n";
            file << "ResumeOnStart=" << (ResumeOnStart ? "true" : "false") << "\r\n";
            file << "WasActive=" << (WasActive ? "true" : "false") << "\r\n";
            file << "KeepAwake=" << (KeepAwake ? "true" : "false") << "\r\n";
            file << "RefundGuard=" << (RefundGuard ? "true" : "false") << "\r\n";
            file << "OnlyWhenAway=" << (OnlyWhenAway ? "true" : "false") << "\r\n";
            file << "AwayMinutes=" << AwayMinutes << "\r\n";
            file << "OnlyInHours=" << (OnlyInHours ? "true" : "false") << "\r\n";
            file << "FromHour=" << FromHour << "\r\n";
            file << "ToHour=" << ToHour << "\r\n";
            file << "VaryTimes=" << (VaryTimes ? "true" : "false") << "\r\n";
            file << "VarySpread=" << VarySpread << "\r\n";
            file << "CardWarmupHours=" << CardWarmupHours << "\r\n";
            for (const StoredShown& shown : ShownGames)
                file << "ShownGame=" << shown.Account << " | " << shown.AppId << "\r\n";
            file << "InvisibleWhileIdling=" << (InvisibleWhileIdling ? "true" : "false") << "\r\n";
            file << "SetInvisible=" << (SetInvisible ? "true" : "false") << "\r\n";
            for (const StoredList& list : Lists)
                file << "List=" << list.Account << " | " << list.Name << "\r\n";
            for (const StoredList& list : ActiveLists)
                file << "ActiveList=" << list.Account << " | " << list.Name << "\r\n";
            for (const StoredEntry& stored : Queue)
            {
                file << "Queue=" << stored.Account << " | " << stored.Entry.AppId << " | " << GoalToken(stored.Entry.Goal.Kind) << " | " << stored.Entry.Goal.Hours
                     << " | " << static_cast<std::uint32_t>(stored.Entry.IdledSeconds) << " | " << stored.Entry.BaselineMinutes
                     << " | " << (stored.Entry.IgnoreRefund ? "true" : "false") << " | " << stored.List << " | " << stored.Entry.GoalFactor << "\r\n";
            }
            file << "ShowTerrarium=" << (ShowTerrarium ? "true" : "false") << "\r\n";
            file << "LibraryCovers=" << (LibraryCovers ? "true" : "false") << "\r\n";
            file << "QueueCovers=" << (QueueCovers ? "true" : "false") << "\r\n";
            file << "ShowLog=" << (ShowLog ? "true" : "false") << "\r\n";
            file << "Season=" << SeasonKey(Season) << "\r\n";
            file << "CreatureHome=" << (CreatureHome ? "true" : "false") << "\r\n";
            file << "HomeBall=" << (HomeBall ? "true" : "false") << "\r\n";
            file << "CreaturesTugMouse=" << (CreaturesTugMouse ? "true" : "false") << "\r\n";
            for (const StoredCreature& creature : Creatures)
            {
                file << "Creature=" << creature.Key << " | " << creature.Found << " | " << creature.Pets;
                if (creature.Evolved)
                    file << (creature.FirstForm ? " | first" : " | evolved");
                file << "\r\n";
            }
            for (const StoredPlay& play : CreaturePlays)
            {
                file << "CreaturePlay=" << play.Key << " | " << play.Record.Minutes << " | " << play.Record.Day << " | " << play.Record.MinutesThatDay << " | "
                     << play.Record.Games << " | " << play.Record.Fetches;
                for (const std::uint32_t best : play.Record.Best)
                    file << " | " << best;
                file << "\r\n";
            }
            if (!CreatureBuddy.empty())
                file << "CreatureBuddy=" << CreatureBuddy << "\r\n";
            file << "CreatureGoals=" << CreatureGoals << "\r\n";
            file << "CreatureBreaks=" << CreatureBreaks << "\r\n";
            file << "CreaturePets=" << CreaturePets << "\r\n";
            if (!file)
                return false;
        }

        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }
}
