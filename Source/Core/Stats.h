#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace Idler
{
    std::int32_t DayNumber(int year, int month, int day);
    std::string DayText(std::int32_t day);
    std::optional<std::int32_t> ParseDay(std::string_view text);
    std::int32_t TodayNumber();
    int CurrentHour();
    int CurrentMonth();
    int WeekdayOf(std::int32_t day);

    struct GameTotal
    {
        std::uint32_t AppId = 0;
        double Seconds = 0.0;
    };

    struct HistoryRow
    {
        std::int32_t Day = 0;
        std::uint32_t AppId = 0;
        double Seconds = 0.0;
    };

    struct GameFigures
    {
        std::uint32_t AppId = 0;
        double Seconds = 0.0;
        std::uint32_t Sessions = 0;
        double LongestSession = 0.0;
        int Days = 0;
        std::int32_t FirstDay = 0;
        std::int32_t LastDay = 0;
    };

    enum class GameOrder
    {
        Time,
        Sessions,
        LongestSession,
        Days,
        Recent,
        Count
    };
    const char* GameOrderName(GameOrder order);
    void SortFigures(std::vector<GameFigures>& games, GameOrder order);

    struct StatsSummary
    {
        double Seconds = 0.0;
        int Games = 0;
        std::uint32_t Sessions = 0;
        int DaysIdled = 0;
        std::int32_t FirstDay = 0;
        std::int32_t FirstSessionDay = 0;
        double BestDaySeconds = 0.0;
        std::int32_t BestDay = 0;
        int MostGamesInADay = 0;
        std::int32_t MostGamesDay = 0;
        int LongestStreak = 0;
        int CurrentStreak = 0;
        double LongestSession = 0.0;
        std::uint32_t LongestSessionApp = 0;
        std::uint32_t MostSessions = 0;
        std::uint32_t MostSessionsApp = 0;
        std::array<double, 7> Weekdays{};
    };

    class IdleStats
    {
    public:
        void Add(std::uint32_t account, std::int32_t day, std::uint32_t appId, double seconds);
        void AddSession(std::uint32_t account, std::int32_t day, std::uint32_t appId);
        void NoteSessionLength(std::uint32_t account, std::int32_t day, std::uint32_t appId, double seconds);
        void AddHour(std::uint32_t account, int hour, double seconds);

        std::vector<double> PerDay(std::uint32_t account, std::int32_t lastDay, int count) const;
        std::vector<double> PerDay(std::uint32_t account, std::uint32_t appId, std::int32_t lastDay, int count) const;
        double Total(std::uint32_t account) const;
        double Total(std::uint32_t account, std::int32_t firstDay, std::int32_t lastDay) const;

        std::vector<GameTotal> ByGame(std::uint32_t account, std::optional<std::int32_t> day = std::nullopt) const;
        std::vector<HistoryRow> History(std::uint32_t account, std::optional<std::int32_t> day = std::nullopt) const;
        std::vector<GameFigures> Figures(std::uint32_t account) const;
        StatsSummary Summary(std::uint32_t account, std::int32_t today) const;
        std::array<double, 24> PerHour(std::uint32_t account) const;

        void Load(const std::filesystem::path& path);
        bool Save(const std::filesystem::path& path) const;

    private:
        struct Row
        {
            double Seconds = 0.0;
            std::uint32_t Sessions = 0;
            double Longest = 0.0;
        };
        std::map<std::tuple<std::uint32_t, std::int32_t, std::uint32_t>, Row> m_rows;
        std::map<std::uint32_t, std::array<double, 24>> m_hours;
    };
}
