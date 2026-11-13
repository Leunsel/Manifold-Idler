#include "Commands.h"

#include "App/IdlerModel.h"
#include "Core/Library.h"
#include "Core/Stats.h"
#include "Core/Text.h"
#include "Request.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Idler::Cli
{
    namespace
    {
        constexpr int kCalendarDays = 378;
        constexpr int kGameChartDays = 30;
        constexpr std::size_t kHistoryDays = 400;

        constexpr std::pair<std::string_view, LibraryFilter> kFilters[] = {
            { "all", LibraryFilter::All }, { "cards", LibraryFilter::Cards }, { "installed", LibraryFilter::Installed },
            { "played", LibraryFilter::Played }, { "notplayed", LibraryFilter::NotPlayed } };

        constexpr std::pair<std::string_view, LibrarySort> kSorts[] = {
            { "name", LibrarySort::Name }, { "playtime", LibrarySort::Playtime }, { "last", LibrarySort::LastPlayed },
            { "purchased", LibrarySort::Purchased }, { "reviews", LibrarySort::Reviews }, { "metacritic", LibrarySort::Metacritic },
            { "released", LibrarySort::Released }, { "achievements", LibrarySort::Achievements } };

        constexpr std::pair<std::string_view, GameOrder> kOrders[] = {
            { "time", GameOrder::Time }, { "sessions", GameOrder::Sessions }, { "longest", GameOrder::LongestSession },
            { "days", GameOrder::Days }, { "recent", GameOrder::Recent } };

        template <typename Value, std::size_t Count>
        std::optional<Value> Lookup(const std::pair<std::string_view, Value> (&table)[Count], std::string_view word)
        {
            for (const auto& [name, value] : table)
            {
                if (name == word)
                    return value;
            }
            return std::nullopt;
        }

        std::string Number(double value)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%.0f", value);
            return text;
        }

        std::string YesNo(bool value)
        {
            return value ? "yes" : "no";
        }

        std::string DateOrEmpty(std::uint32_t unixTime)
        {
            return unixTime != 0 ? DateText(unixTime) : std::string();
        }

        std::string DayOrEmpty(std::int32_t day)
        {
            return day != 0 ? DayText(day) : std::string();
        }

        std::unordered_map<std::uint32_t, double> IdledByGame(const IdlerModel& model)
        {
            std::unordered_map<std::uint32_t, double> idled;
            for (const GameTotal& total : model.Stats().ByGame(model.Account()))
                idled[total.AppId] = total.Seconds;
            return idled;
        }

        CommandResult Library(const IdlerModel& model, const std::vector<std::string>& words)
        {
            std::string problem;
            if (!LibraryReady(model, problem))
                return Failed(problem);
            const auto filter = words.size() >= 2 ? Lookup(kFilters, words[1]) : LibraryFilter::All;
            if (!filter)
                return Failed("Show which games: all, cards, installed, played or notplayed.");
            const auto sort = words.size() >= 3 ? Lookup(kSorts, words[2]) : LibrarySort::Name;
            if (!sort)
                return Failed("Sort by name, playtime, last, purchased, reviews, metacritic, released or achievements.");
            const auto rating = words.size() >= 4 ? ParseUnsigned(words[3]) : std::optional<std::uint32_t>(0);
            if (!rating || *rating > 100)
                return Failed("A rating is from 0 to 100.");
            const std::string search = words.size() >= 5 ? words[4] : std::string();

            const auto idled = IdledByGame(model);
            CommandResult result;
            result.Columns = { "app", "name", "playtime", "minutes", "last", "purchased", "released", "installed", "cards", "achieved", "achievements",
                "review", "metacritic", "queued", "idled", "software" };
            for (const int index : LibraryView(model.Games(), search, *filter, *sort, *rating))
            {
                const Game& game = model.Games()[static_cast<std::size_t>(index)];
                const std::string app = std::to_string(game.AppId);
                const std::string playtime = PlaytimeText(game.PlaytimeMinutes);
                const auto found = idled.find(game.AppId);
                result.Text += Padded(app, 10) + Padded(playtime, 14) + game.Name + "\n";
                result.Rows.push_back({ app, game.Name, playtime, std::to_string(game.PlaytimeMinutes), DateOrEmpty(game.LastPlayed), DateOrEmpty(game.Purchased),
                    DateOrEmpty(game.Released), YesNo(game.Installed), YesNo(game.HasCards), std::to_string(game.AchievementsDone), std::to_string(game.AchievementsTotal),
                    game.ReviewPercent ? std::to_string(game.ReviewPercent) : std::string(), game.Metacritic ? std::to_string(game.Metacritic) : std::string(),
                    YesNo(model.FindEntry(game.AppId) != nullptr), found != idled.end() ? Number(found->second) : std::string("0"), YesNo(game.Software) });
            }
            result.Text += GamesText(result.Rows.size()) + ".\n";
            return result;
        }

        CommandResult GameDetail(const IdlerModel& model, const std::vector<std::string>& words)
        {
            std::string problem;
            if (!LibraryReady(model, problem))
                return Failed(problem);
            const auto appId = words.size() == 2 ? ParseUnsigned(words[1]) : std::nullopt;
            const Game* game = appId ? model.FindGame(*appId) : nullptr;
            if (!game)
                return Failed((words.size() >= 2 ? words[1] : std::string("That")) + " is not a game of this account.");

            CommandResult result;
            result.Columns = { "key", "label", "value" };
            const auto line = [&](const char* key, const char* label, const std::string& value)
                {
                    if (*label)
                        result.Text += Padded(label, 16) + value + "\n";
                    result.Rows.push_back({ key, label, value });
                };
            line("name", "Name", game->Name);
            line("app", "App id", std::to_string(game->AppId));
            line("playtime", "Played", PlaytimeText(game->PlaytimeMinutes));
            line("last", "Last played", game->LastPlayed ? DateText(game->LastPlayed) : std::string("never"));
            if (game->Purchased)
                line("purchased", "Bought", DateText(game->Purchased));
            if (game->Released)
                line("released", "Released", DateText(game->Released));
            if (game->ReviewPercent)
                line("review", "Reviews", std::to_string(game->ReviewPercent) + " % positive");
            if (game->Metacritic)
                line("metacritic", "Metacritic", std::to_string(game->Metacritic));
            if (game->AchievementsTotal)
                line("achievements", "Achievements", std::to_string(game->AchievementsDone) + " of " + std::to_string(game->AchievementsTotal));
            line("cards", "Trading cards", game->HasCards ? "yes" : "no");
            line("installed", "Installed", game->Installed ? "yes" : "no");

            const std::vector<GameFigures> figures = model.Stats().Figures(model.Account());
            const auto figure = std::find_if(figures.begin(), figures.end(), [&](const GameFigures& each) { return each.AppId == game->AppId; });
            if (figure != figures.end())
            {
                line("idled", "Idled here", DurationText(static_cast<std::uint64_t>(figure->Seconds)));
                line("sessions", "Sessions", std::to_string(figure->Sessions));
                if (figure->LongestSession > 0.0)
                    line("longest", "Longest run", DurationText(static_cast<std::uint64_t>(figure->LongestSession)));
                line("days", "Days idled", std::to_string(figure->Days));
                line("first", "First idled", DayOrEmpty(figure->FirstDay));
                line("lastidled", "Last idled", DayOrEmpty(figure->LastDay));
            }
            else
            {
                line("idled", "Idled here", "not yet");
            }
            line("queued", "", YesNo(model.FindEntry(game->AppId) != nullptr));

            const std::int32_t today = TodayNumber();
            const std::vector<double> days = model.Stats().PerDay(model.Account(), game->AppId, today, kGameChartDays);
            for (std::size_t index = 0; index < days.size(); ++index)
                result.Rows.push_back({ "day", DayText(today - static_cast<std::int32_t>(days.size() - 1 - index)), Number(days[index]) });
            return result;
        }

        CommandResult Overview(const IdlerModel& model)
        {
            const IdleStats& stats = model.Stats();
            const std::uint32_t account = model.Account();
            const std::int32_t today = TodayNumber();
            const StatsSummary summary = stats.Summary(account, today);
            const std::vector<double> calendar = stats.PerDay(account, today, kCalendarDays);

            CommandResult result;
            result.Columns = { "series", "key", "value", "extra" };
            const auto row = [&](const char* series, std::string key, std::string value, std::string extra = {})
                { result.Rows.push_back({ series, std::move(key), std::move(value), std::move(extra) }); };

            const double todaySeconds = calendar.empty() ? 0.0 : calendar.back();
            const double yesterdaySeconds = calendar.size() >= 2 ? calendar[calendar.size() - 2] : 0.0;
            row("tile", "today", Number(todaySeconds));
            row("tile", "yesterday", Number(yesterdaySeconds));
            row("tile", "week", Number(stats.Total(account, today - 6, today)));
            row("tile", "weekbefore", Number(stats.Total(account, today - 13, today - 7)));
            row("tile", "total", Number(summary.Seconds), DayOrEmpty(summary.DaysIdled > 0 ? summary.FirstDay : 0));
            row("tile", "games", std::to_string(summary.Games));
            row("tile", "days", std::to_string(summary.DaysIdled));
            row("tile", "sessions", std::to_string(summary.Sessions), DayOrEmpty(summary.FirstSessionDay));
            row("tile", "streak", std::to_string(summary.CurrentStreak));
            row("tile", "beststreak", std::to_string(summary.LongestStreak));

            if (summary.DaysIdled > 0)
            {
                row("record", "bestday", Number(summary.BestDaySeconds), DayText(summary.BestDay));
                row("record", "mostgames", std::to_string(summary.MostGamesInADay), DayText(summary.MostGamesDay));
                row("record", "average", Number(summary.Seconds / static_cast<double>(summary.DaysIdled)));
                if (summary.LongestSession > 0.0)
                    row("record", "longest", Number(summary.LongestSession), GameName(model, summary.LongestSessionApp));
                if (summary.MostSessions > 0)
                    row("record", "mostsessions", std::to_string(summary.MostSessions), GameName(model, summary.MostSessionsApp));
            }

            for (std::size_t index = 0; index < calendar.size(); ++index)
                row("day", DayText(today - static_cast<std::int32_t>(calendar.size() - 1 - index)), Number(calendar[index]));
            const std::array<double, 24> hours = stats.PerHour(account);
            for (std::size_t hour = 0; hour < hours.size(); ++hour)
                row("hour", std::to_string(hour), Number(hours[hour]));
            for (std::size_t weekday = 0; weekday < summary.Weekdays.size(); ++weekday)
                row("weekday", std::to_string(weekday), Number(summary.Weekdays[weekday]));
            const std::vector<GameFigures> figures = stats.Figures(account);
            for (std::size_t index = 0; index < std::min<std::size_t>(3, figures.size()); ++index)
                row("top", std::to_string(figures[index].AppId), Number(figures[index].Seconds), GameName(model, figures[index].AppId));

            result.Text = Padded("Today", 16) + DurationText(static_cast<std::uint64_t>(todaySeconds)) + "\n" +
                Padded("Last 7 days", 16) + DurationText(static_cast<std::uint64_t>(stats.Total(account, today - 6, today))) + "\n" +
                Padded("All time", 16) + DurationText(static_cast<std::uint64_t>(summary.Seconds)) + "\n" +
                Padded("Games", 16) + std::to_string(summary.Games) + "\n" +
                Padded("Sessions", 16) + std::to_string(summary.Sessions) + "\n" +
                Padded("Days in a row", 16) + std::to_string(summary.CurrentStreak) + ", best " + std::to_string(summary.LongestStreak) + "\n";
            return result;
        }

        CommandResult GameFiguresList(const IdlerModel& model, const std::vector<std::string>& words)
        {
            const auto order = words.size() >= 3 ? Lookup(kOrders, words[2]) : GameOrder::Time;
            if (!order)
                return Failed("Order by time, sessions, longest, days or recent.");
            std::vector<GameFigures> figures = model.Stats().Figures(model.Account());
            SortFigures(figures, *order);

            CommandResult result;
            result.Columns = { "app", "name", "seconds", "sessions", "longest", "days", "first", "last" };
            for (const GameFigures& figure : figures)
            {
                const std::string name = GameName(model, figure.AppId);
                result.Text += Padded(DurationText(static_cast<std::uint64_t>(figure.Seconds)), 14) + name + "\n";
                result.Rows.push_back({ std::to_string(figure.AppId), name, Number(figure.Seconds), std::to_string(figure.Sessions), Number(figure.LongestSession),
                    std::to_string(figure.Days), DayOrEmpty(figure.FirstDay), DayOrEmpty(figure.LastDay) });
            }
            if (figures.empty())
                result.Text = "Nothing idled yet.\n";
            return result;
        }

        CommandResult Statistics(const IdlerModel& model, const std::vector<std::string>& words)
        {
            if (words.size() >= 2 && words[1] == "games")
                return GameFiguresList(model, words);
            if (words.size() >= 2)
                return Failed("stats, or stats games [time, sessions, longest, days or recent].");
            return Overview(model);
        }

        CommandResult History(const IdlerModel& model, const std::vector<std::string>& words)
        {
            CommandResult result;
            if (words.size() >= 2)
            {
                const auto day = ParseDay(words[1]);
                if (!day)
                    return Failed("A day is written as 2026-11-12.");
                result.Columns = { "app", "name", "seconds" };
                std::vector<GameTotal> games = model.Stats().ByGame(model.Account(), *day);
                std::sort(games.begin(), games.end(), [](const GameTotal& a, const GameTotal& b) { return a.Seconds > b.Seconds; });
                for (const GameTotal& game : games)
                {
                    const std::string name = GameName(model, game.AppId);
                    result.Text += Padded(DurationText(static_cast<std::uint64_t>(game.Seconds)), 14) + name + "\n";
                    result.Rows.push_back({ std::to_string(game.AppId), name, Number(game.Seconds) });
                }
                if (games.empty())
                    result.Text = "Nothing idled on " + words[1] + ".\n";
                return result;
            }

            std::map<std::int32_t, std::pair<double, int>, std::greater<>> days;
            for (const HistoryRow& row : model.Stats().History(model.Account()))
            {
                auto& [seconds, games] = days[row.Day];
                seconds += row.Seconds;
                ++games;
            }
            result.Columns = { "day", "seconds", "games" };
            for (const auto& [day, totals] : days)
            {
                if (result.Rows.size() >= kHistoryDays)
                    break;
                result.Text += DayText(day) + "  " + Padded(DurationText(static_cast<std::uint64_t>(totals.first)), 14) + GamesText(static_cast<std::size_t>(totals.second)) + "\n";
                result.Rows.push_back({ DayText(day), Number(totals.first), std::to_string(totals.second) });
            }
            if (days.empty())
                result.Text = "Nothing idled yet.\n";
            return result;
        }

        struct Setting
        {
            const char* Key;
            const char* Label;
            const char* Note;
            const char* Kind;
            double Min = 0.0;
            double Max = 0.0;
            double Step = 0.0;
        };

        constexpr Setting kSettings[] = {
            { "goal", "Goal for new games", "When a game added to the queue is done. For a time counts from its start, until a playtime includes the time played before.", "goal" },
            { "slots", "Games at once", "How many games of the queue idle at the same time. Steam counts up to 32.", "int", 1.0, 32.0, 1.0 },
            { "pause", "Pause while a game runs", "Stops idling while Steam runs another game, and goes on afterwards.", "bool" },
            { "resume", "Continue after a restart", "When the idler was idling as it ended, it starts idling again on its next start.", "bool" },
#ifdef _WIN32
            { "awake", "Keep the computer awake", "No standby while games idle. The screen may still go off.", "bool" },
#endif
            { "refund", "Protect refunds", "Steam refunds a game within 14 days of buying it, with under 2 hours played. Such a game stops 10 minutes before that limit.", "bool" },
            { "invisible", "Invisible while idling", "You appear offline to friends while games idle. Afterwards your status goes back to online, also if you had set another one.", "bool" },
            { "vary", "Vary the times", "Games start at different moments and take breaks of their own, and their goals differ a little, so their hours do not come out the same.", "bool" },
            { "spread", "Goals differ by", "How far a game's goal may lie from the one set, up or down, in percent.", "float", 5.0, 50.0, 5.0 },
#ifdef _WIN32
            { "away", "Only when I am away", "Idling starts after this long without mouse or keyboard, and stops at the next input.", "bool" },
            { "awayminutes", "Away for", "Minutes without mouse or keyboard before idling starts.", "float", 1.0, 1440.0, 1.0 },
#endif
            { "hours", "Only at certain hours", "Idling only runs from the first hour up to the second, also over midnight. The computer's clock counts.", "bool" },
            { "from", "From", "The hour idling may start.", "int", 0.0, 23.0, 1.0 },
            { "until", "Until", "The hour idling stops.", "int", 0.0, 23.0, 1.0 },
            { "updates", "Look for new versions", "Once a day the idler asks GitHub for its newest release and says so in its status and log. The question carries only the version. Nothing is downloaded.", "bool" },
        };

        std::string GoalSetting(const Goal& goal)
        {
            char hours[32];
            std::snprintf(hours, sizeof(hours), "%g", static_cast<double>(goal.Hours));
            switch (goal.Kind)
            {
            case GoalKind::ForHours: return std::string("for ") + hours;
            case GoalKind::UntilTotal: return std::string("until ") + hours;
            default: return "none";
            }
        }

        std::string SettingValue(const IdlerModel& model, std::string_view key)
        {
            const Settings& options = model.Options();
            char text[32];
            const auto number = [&](double value) { std::snprintf(text, sizeof(text), "%g", value); return std::string(text); };
            if (key == "goal") return GoalSetting(options.DefaultGoal);
            if (key == "slots") return std::to_string(model.Slots());
            if (key == "pause") return YesNo(options.PauseWhilePlaying);
            if (key == "resume") return YesNo(options.ResumeOnStart);
            if (key == "awake") return YesNo(options.KeepAwake);
            if (key == "refund") return YesNo(options.RefundGuard);
            if (key == "invisible") return YesNo(options.InvisibleWhileIdling);
            if (key == "vary") return YesNo(options.VaryTimes);
            if (key == "spread") return number(options.VarySpread);
            if (key == "away") return YesNo(options.OnlyWhenAway);
            if (key == "awayminutes") return number(options.AwayMinutes);
            if (key == "hours") return YesNo(options.OnlyInHours);
            if (key == "from") return std::to_string(options.FromHour);
            if (key == "until") return std::to_string(options.ToHour);
            if (key == "updates") return YesNo(options.UpdateCheck);
            return {};
        }

        const Setting* FindSetting(std::string_view key)
        {
            for (const Setting& setting : kSettings)
            {
                if (key == setting.Key)
                    return &setting;
            }
            return nullptr;
        }

        CommandResult ShowSettings(const IdlerModel& model)
        {
            CommandResult result;
            result.Columns = { "key", "label", "note", "kind", "value", "min", "max", "step" };
            char min[32];
            char max[32];
            char step[32];
            for (const Setting& setting : kSettings)
            {
                const std::string value = SettingValue(model, setting.Key);
                std::snprintf(min, sizeof(min), "%g", setting.Min);
                std::snprintf(max, sizeof(max), "%g", setting.Max);
                std::snprintf(step, sizeof(step), "%g", setting.Step);
                result.Text += Padded(setting.Key, 14) + Padded(value, 12) + setting.Label + "\n";
                result.Rows.push_back({ setting.Key, setting.Label, setting.Note, setting.Kind, value, min, max, step });
            }
            return result;
        }

        std::optional<bool> ParseSwitch(std::string_view word)
        {
            if (word == "on" || word == "yes" || word == "true")
                return true;
            if (word == "off" || word == "no" || word == "false")
                return false;
            return std::nullopt;
        }

        std::optional<double> ParseNumber(const std::string& word, double min, double max)
        {
            if (word.empty())
                return std::nullopt;
            char* end = nullptr;
            const double value = std::strtod(word.c_str(), &end);
            if (!end || *end != '\0' || !std::isfinite(value) || value < min || value > max)
                return std::nullopt;
            return value;
        }

        CommandResult SetSetting(IdlerModel& model, const std::vector<std::string>& words)
        {
            const Setting* setting = words.size() >= 3 ? FindSetting(words[1]) : nullptr;
            if (!setting)
                return Failed("Which setting? idler settings lists them.");
            const std::string_view kind = setting->Kind;
            const std::string_view key = setting->Key;
            Settings& options = model.Options();

            if (kind == "goal")
            {
                Goal goal;
                std::string problem;
                if (!ParseGoal(words, 2, goal, problem))
                    return Failed(problem);
                options.DefaultGoal = goal;
            }
            else if (kind == "bool")
            {
                const auto value = words.size() == 3 ? ParseSwitch(words[2]) : std::nullopt;
                if (!value)
                    return Failed(std::string(setting->Label) + ": on or off.");
                bool* target = key == "pause" ? &options.PauseWhilePlaying : key == "resume" ? &options.ResumeOnStart : key == "awake" ? &options.KeepAwake
                    : key == "refund" ? &options.RefundGuard : key == "invisible" ? &options.InvisibleWhileIdling : key == "vary" ? &options.VaryTimes
                    : key == "away" ? &options.OnlyWhenAway : key == "updates" ? &options.UpdateCheck : &options.OnlyInHours;
                *target = *value;
            }
            else
            {
                const auto value = words.size() == 3 ? ParseNumber(words[2], setting->Min, setting->Max) : std::nullopt;
                if (!value || (kind == "int" && std::floor(*value) != *value))
                {
                    char text[96];
                    std::snprintf(text, sizeof(text), "%s: a number from %g to %g.", setting->Label, setting->Min, setting->Max);
                    return Failed(text);
                }
                if (key == "slots")
                    model.SetSlots(static_cast<int>(*value));
                else if (key == "spread")
                    options.VarySpread = static_cast<float>(*value);
                else if (key == "awayminutes")
                    options.AwayMinutes = static_cast<float>(*value);
                else if (key == "from")
                    options.FromHour = static_cast<int>(*value);
                else
                    options.ToHour = static_cast<int>(*value);
            }
            model.SaveNow();
            return Done(std::string(setting->Label) + ": " + SettingValue(model, key) + "\n");
        }
    }

    bool RunPageCommand(IdlerModel& model, const std::vector<std::string>& words, CommandResult& result)
    {
        const std::string command = words.empty() ? std::string() : words[0];
        if (command == "library")
            result = Library(model, words);
        else if (command == "game")
            result = GameDetail(model, words);
        else if (command == "stats")
            result = Statistics(model, words);
        else if (command == "history")
            result = History(model, words);
        else if (command == "settings")
            result = ShowSettings(model);
        else if (command == "set")
            result = SetSetting(model, words);
        else
            return false;
        return true;
    }
}
