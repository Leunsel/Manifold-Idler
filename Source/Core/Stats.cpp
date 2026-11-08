#include "Core/Stats.h"

#include "Core/Text.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace Idler
{
    std::int32_t DayNumber(int year, int month, int day)
    {
        year -= month <= 2 ? 1 : 0;
        const int era = (year >= 0 ? year : year - 399) / 400;
        const int yearOfEra = year - era * 400;
        const int dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
        return era * 146097 + dayOfEra - 719468;
    }

    std::string DayText(std::int32_t day)
    {
        const int shifted = day + 719468;
        const int era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
        const int dayOfEra = shifted - era * 146097;
        const int yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
        const int dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
        const int shiftedMonth = (5 * dayOfYear + 2) / 153;
        const int dayOfMonth = dayOfYear - (153 * shiftedMonth + 2) / 5 + 1;
        const int month = shiftedMonth + (shiftedMonth < 10 ? 3 : -9);
        const int year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);

        char text[32];
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d", year, month, dayOfMonth);
        return text;
    }

    std::optional<std::int32_t> ParseDay(std::string_view text)
    {
        if (text.size() != 10 || text[4] != '-' || text[7] != '-')
            return std::nullopt;
        const auto year = ParseUnsigned(text.substr(0, 4));
        const auto month = ParseUnsigned(text.substr(5, 2));
        const auto day = ParseUnsigned(text.substr(8, 2));
        if (!year || !month || !day || *month < 1 || *month > 12 || *day < 1 || *day > 31)
            return std::nullopt;
        return DayNumber(static_cast<int>(*year), static_cast<int>(*month), static_cast<int>(*day));
    }

    std::int32_t TodayNumber()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        if (!LocalTime(now, local))
            return 0;
        return DayNumber(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    }

    int CurrentHour()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        return LocalTime(now, local) ? local.tm_hour : 0;
    }

    int CurrentMonth()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        return LocalTime(now, local) ? local.tm_mon + 1 : 1;
    }

    int WeekdayOf(std::int32_t day)
    {
        return ((day + 3) % 7 + 7) % 7;
    }

    const char* GameOrderName(GameOrder order)
    {
        switch (order)
        {
        case GameOrder::Sessions: return "Most sessions";
        case GameOrder::LongestSession: return "Longest session";
        case GameOrder::Days: return "Most days";
        case GameOrder::Recent: return "Last idled";
        default: return "Most time";
        }
    }

    void SortFigures(std::vector<GameFigures>& games, GameOrder order)
    {
        const auto by = [&](auto value)
            {
                std::stable_sort(games.begin(), games.end(), [&](const GameFigures& lhs, const GameFigures& rhs) { return value(lhs) > value(rhs); });
            };
        std::stable_sort(games.begin(), games.end(), [](const GameFigures& lhs, const GameFigures& rhs)
            { return lhs.Seconds != rhs.Seconds ? lhs.Seconds > rhs.Seconds : lhs.AppId < rhs.AppId; });
        switch (order)
        {
        case GameOrder::Sessions: by([](const GameFigures& game) { return game.Sessions; }); break;
        case GameOrder::LongestSession: by([](const GameFigures& game) { return game.LongestSession; }); break;
        case GameOrder::Days: by([](const GameFigures& game) { return game.Days; }); break;
        case GameOrder::Recent: by([](const GameFigures& game) { return game.LastDay; }); break;
        default: break;
        }
    }

    void IdleStats::Add(std::uint32_t account, std::int32_t day, std::uint32_t appId, double seconds)
    {
        if (seconds > 0.0)
            m_rows[{ account, day, appId }].Seconds += seconds;
    }

    void IdleStats::AddSession(std::uint32_t account, std::int32_t day, std::uint32_t appId)
    {
        ++m_rows[{ account, day, appId }].Sessions;
    }

    void IdleStats::NoteSessionLength(std::uint32_t account, std::int32_t day, std::uint32_t appId, double seconds)
    {
        if (seconds <= 0.0)
            return;
        double& longest = m_rows[{ account, day, appId }].Longest;
        longest = std::max(longest, seconds);
    }

    void IdleStats::AddHour(std::uint32_t account, int hour, double seconds)
    {
        if (seconds > 0.0 && hour >= 0 && hour < 24)
            m_hours[account][static_cast<std::size_t>(hour)] += seconds;
    }

    std::vector<double> IdleStats::PerDay(std::uint32_t account, std::int32_t lastDay, int count) const
    {
        std::vector<double> days(static_cast<std::size_t>(std::max(count, 0)), 0.0);
        const std::int32_t firstDay = lastDay - count + 1;
        for (auto row = m_rows.lower_bound({ account, firstDay, 0u }); row != m_rows.end(); ++row)
        {
            const auto& [rowAccount, day, app] = row->first;
            if (rowAccount != account || day > lastDay)
                break;
            days[static_cast<std::size_t>(day - firstDay)] += row->second.Seconds;
        }
        return days;
    }

    std::vector<double> IdleStats::PerDay(std::uint32_t account, std::uint32_t appId, std::int32_t lastDay, int count) const
    {
        std::vector<double> days(static_cast<std::size_t>(std::max(count, 0)), 0.0);
        const std::int32_t firstDay = lastDay - count + 1;
        for (auto row = m_rows.lower_bound({ account, firstDay, 0u }); row != m_rows.end(); ++row)
        {
            const auto& [rowAccount, day, app] = row->first;
            if (rowAccount != account || day > lastDay)
                break;
            if (app == appId)
                days[static_cast<std::size_t>(day - firstDay)] += row->second.Seconds;
        }
        return days;
    }

    double IdleStats::Total(std::uint32_t account) const
    {
        return Total(account, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)());
    }

    double IdleStats::Total(std::uint32_t account, std::int32_t firstDay, std::int32_t lastDay) const
    {
        double total = 0.0;
        for (auto row = m_rows.lower_bound({ account, firstDay, 0u }); row != m_rows.end(); ++row)
        {
            const auto& [rowAccount, day, app] = row->first;
            if (rowAccount != account || day > lastDay)
                break;
            total += row->second.Seconds;
        }
        return total;
    }

    std::vector<GameTotal> IdleStats::ByGame(std::uint32_t account, std::optional<std::int32_t> day) const
    {
        std::unordered_map<std::uint32_t, double> totals;
        for (const auto& [key, row] : m_rows)
        {
            const auto& [rowAccount, rowDay, app] = key;
            if (rowAccount == account && (!day || rowDay == *day) && row.Seconds > 0.0)
                totals[app] += row.Seconds;
        }

        std::vector<GameTotal> games;
        games.reserve(totals.size());
        for (const auto& [app, seconds] : totals)
            games.push_back({ app, seconds });
        std::sort(games.begin(), games.end(), [](const GameTotal& lhs, const GameTotal& rhs)
            { return lhs.Seconds != rhs.Seconds ? lhs.Seconds > rhs.Seconds : lhs.AppId < rhs.AppId; });
        return games;
    }

    std::vector<HistoryRow> IdleStats::History(std::uint32_t account, std::optional<std::int32_t> day) const
    {
        std::vector<HistoryRow> rows;
        for (const auto& [key, row] : m_rows)
        {
            const auto& [rowAccount, rowDay, app] = key;
            if (rowAccount == account && (!day || rowDay == *day) && row.Seconds > 0.0)
                rows.push_back({ rowDay, app, row.Seconds });
        }
        std::sort(rows.begin(), rows.end(), [](const HistoryRow& lhs, const HistoryRow& rhs)
            {
                if (lhs.Day != rhs.Day)
                    return lhs.Day > rhs.Day;
                return lhs.Seconds != rhs.Seconds ? lhs.Seconds > rhs.Seconds : lhs.AppId < rhs.AppId;
            });
        return rows;
    }

    std::vector<GameFigures> IdleStats::Figures(std::uint32_t account) const
    {
        std::unordered_map<std::uint32_t, GameFigures> figures;
        for (const auto& [key, row] : m_rows)
        {
            const auto& [rowAccount, day, app] = key;
            if (rowAccount != account || row.Seconds <= 0.0)
                continue;
            GameFigures& game = figures[app];
            if (game.AppId == 0)
                game.FirstDay = day;
            game.AppId = app;
            game.LastDay = day;
            game.Seconds += row.Seconds;
            game.Sessions += row.Sessions;
            game.LongestSession = std::max(game.LongestSession, row.Longest);
            ++game.Days;
        }

        std::vector<GameFigures> games;
        games.reserve(figures.size());
        for (const auto& [app, game] : figures)
            games.push_back(game);
        SortFigures(games, GameOrder::Time);
        return games;
    }

    StatsSummary IdleStats::Summary(std::uint32_t account, std::int32_t today) const
    {
        StatsSummary summary;
        for (const GameFigures& game : Figures(account))
        {
            ++summary.Games;
            summary.Seconds += game.Seconds;
            summary.Sessions += game.Sessions;
            if (game.LongestSession > summary.LongestSession)
            {
                summary.LongestSession = game.LongestSession;
                summary.LongestSessionApp = game.AppId;
            }
            if (game.Sessions > summary.MostSessions)
            {
                summary.MostSessions = game.Sessions;
                summary.MostSessionsApp = game.AppId;
            }
        }

        std::map<std::int32_t, std::pair<double, int>> days;
        for (const auto& [key, row] : m_rows)
        {
            const auto& [rowAccount, day, app] = key;
            if (rowAccount != account)
                continue;
            if (row.Sessions > 0 && summary.FirstSessionDay == 0)
                summary.FirstSessionDay = day;
            if (row.Seconds <= 0.0)
                continue;
            days[day].first += row.Seconds;
            ++days[day].second;
            summary.Weekdays[static_cast<std::size_t>(WeekdayOf(day))] += row.Seconds;
        }

        int run = 0;
        std::int32_t previous = 0;
        for (const auto& [day, figures] : days)
        {
            if (summary.DaysIdled == 0)
                summary.FirstDay = day;
            ++summary.DaysIdled;
            if (figures.first > summary.BestDaySeconds)
            {
                summary.BestDaySeconds = figures.first;
                summary.BestDay = day;
            }
            if (figures.second > summary.MostGamesInADay)
            {
                summary.MostGamesInADay = figures.second;
                summary.MostGamesDay = day;
            }
            run = run > 0 && day == previous + 1 ? run + 1 : 1;
            summary.LongestStreak = std::max(summary.LongestStreak, run);
            previous = day;
        }

        std::int32_t expected = days.contains(today) ? today : today - 1;
        for (auto day = days.upper_bound(today); day != days.begin();)
        {
            --day;
            if (day->first != expected)
                break;
            ++summary.CurrentStreak;
            --expected;
        }
        return summary;
    }

    std::array<double, 24> IdleStats::PerHour(std::uint32_t account) const
    {
        const auto hours = m_hours.find(account);
        return hours != m_hours.end() ? hours->second : std::array<double, 24>{};
    }

    void IdleStats::Load(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return;

        std::string line;
        while (std::getline(file, line))
        {
            std::string_view text = Trim(line);
            std::string_view parts[6];
            int count = 0;
            while (count < 6)
            {
                const auto bar = text.find('|');
                parts[count++] = Trim(text.substr(0, bar));
                if (bar == std::string_view::npos)
                    break;
                text.remove_prefix(bar + 1);
            }
            if (count < 4)
                continue;
            const auto account = ParseUnsigned(parts[1]);
            const auto seconds = ParseUnsigned(parts[3]);
            if (parts[0].size() > 1 && parts[0].front() == 'H')
            {
                const auto hour = ParseUnsigned(parts[0].substr(1));
                if (hour && account && seconds)
                    AddHour(*account, static_cast<int>(*hour), static_cast<double>(*seconds));
                continue;
            }
            const auto day = ParseDay(parts[0]);
            const auto app = ParseUnsigned(parts[2]);
            if (!day || !account || !app || !seconds)
                continue;
            Add(*account, *day, *app, static_cast<double>(*seconds));
            if (count < 6)
                continue;
            const auto sessions = ParseUnsigned(parts[4]);
            const auto longest = ParseUnsigned(parts[5]);
            if (sessions && *sessions > 0)
                m_rows[{ *account, *day, *app }].Sessions += static_cast<std::uint32_t>(*sessions);
            if (longest)
                NoteSessionLength(*account, *day, *app, static_cast<double>(*longest));
        }
    }

    bool IdleStats::Save(const std::filesystem::path& path) const
    {
        auto temporary = path;
        temporary += L".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;
            file << "; Per day, account and game: seconds idled, sessions begun, seconds of the longest. Written by Manifold-Idler.\r\n";
            for (const auto& [key, row] : m_rows)
            {
                const auto& [account, day, app] = key;
                file << DayText(day) << " | " << account << " | " << app << " | " << static_cast<std::uint32_t>(row.Seconds) << " | " << row.Sessions << " | " << static_cast<std::uint32_t>(row.Longest) << "\r\n";
            }
            file << "; Per hour of the day and account: seconds idled.\r\n";
            for (const auto& [account, hours] : m_hours)
            {
                for (std::size_t hour = 0; hour < hours.size(); ++hour)
                {
                    if (hours[hour] >= 1.0)
                        file << 'H' << hour << " | " << account << " | 0 | " << static_cast<std::uint32_t>(hours[hour]) << "\r\n";
                }
            }
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
