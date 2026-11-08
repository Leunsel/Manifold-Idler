#pragma once

#include "Core/AppInfo.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Idler
{
    enum class UnlockRule : std::uint8_t
    {
        Unknown,
        Here,
        Mixed,
        Server
    };

    struct Game
    {
        std::uint32_t AppId = 0;
        std::string Name;
        std::string Folded;
        bool HasCards = false;
        bool HasAchievements = false;
        bool Installed = false;
        bool Software = false;
        std::uint32_t PlaytimeMinutes = 0;
        std::uint32_t LastPlayed = 0;
        std::uint32_t Purchased = 0;
        std::uint32_t ReviewPercent = 0;
        std::uint32_t Metacritic = 0;
        std::uint32_t Released = 0;
        std::uint32_t AchievementsDone = 0;
        std::uint32_t AchievementsTotal = 0;
        UnlockRule Unlock = UnlockRule::Unknown;
    };

    struct Ownership
    {
        std::uint32_t AppId = 0;
        std::uint32_t Purchased = 0;
        bool Installed = false;
    };

    struct Playtime
    {
        std::uint32_t Minutes = 0;
        std::uint32_t LastPlayed = 0;
    };

    using PlaytimeMap = std::unordered_map<std::uint32_t, Playtime>;

    struct AchievementCount
    {
        std::uint32_t Done = 0;
        std::uint32_t Total = 0;
    };

    using AchievementMap = std::unordered_map<std::uint32_t, AchievementCount>;

    AchievementCount ParseAchievementCounts(std::string_view json);

    struct SteamRegistry
    {
        std::uint32_t ProcessId = 0;
        std::uint32_t Account = 0;
        std::string ClientLibrary;
        std::vector<std::uint32_t> RunningApps;
    };
    SteamRegistry ParseSteamRegistry(std::string_view text);

    bool IsLibraryType(std::string_view type);

    PlaytimeMap ParsePlaytimes(std::string_view localConfig);

    bool MarkGamesWithCards(std::vector<Game>& games, const std::unordered_map<std::uint32_t, std::uint32_t>& drops);

    std::vector<Game> BuildLibrary(const std::vector<AppInfoEntry>& apps, const std::vector<Ownership>& owned, const PlaytimeMap& playtimes,
        std::uint32_t freePackageDate, const AchievementMap& achievements = {});

    enum class LibraryFilter
    {
        All,
        Cards,
        Installed,
        Played,
        NotPlayed,
        Count
    };

    enum class LibrarySort
    {
        Name,
        Playtime,
        LastPlayed,
        Purchased,
        Reviews,
        Metacritic,
        Released,
        Achievements,
        Count
    };

    const char* FilterName(LibraryFilter filter);
    const char* SortName(LibrarySort sort);

    struct AchievementIcons
    {
        std::string Id;
        std::string Icon;
        std::string IconLocked;
        std::uint32_t Permission = 0;
        std::string ProgressStat;
        double ProgressMin = 0.0;
        double ProgressMax = 0.0;
    };
    std::vector<AchievementIcons> ParseAchievementIcons(std::string_view schema);
    bool MaySetAchievement(std::string_view id, const std::vector<AchievementIcons>& schema);
    UnlockRule UnlockRuleOf(const std::vector<AchievementIcons>& schema);

    enum class StatKind : std::uint8_t
    {
        Int,
        Float,
        AverageRate
    };

    struct StatSchema
    {
        std::string Name;
        std::string Label;
        StatKind Kind = StatKind::Int;
        std::uint32_t Permission = 0;
        bool IncrementOnly = false;
        std::optional<double> Min;
        std::optional<double> Max;
        std::optional<double> MaxChange;
    };
    std::vector<StatSchema> ParseStatSchema(std::string_view schema);
    const StatSchema* FindStat(const std::vector<StatSchema>& stats, std::string_view name);
    bool MaySetStat(const StatSchema& stat);
    std::string StatChangeProblem(const StatSchema& stat, double current, double value);
    std::optional<double> ParseNumber(std::string_view text);
    std::string NumberText(double value, StatKind kind);

    struct AchievementChange
    {
        std::string Id;
        bool Unlock = false;
        bool SetsStat = false;
        double Value = 0.0;
    };
    std::string WriteAchievementChanges(const std::vector<AchievementChange>& changes);
    std::vector<AchievementChange> ParseAchievementChanges(std::string_view text);
    std::string AchievementChangeText(std::size_t unlocked, std::size_t locked, std::size_t refused);

    struct SteamCollection
    {
        std::string Name;
        std::vector<std::uint32_t> Apps;
    };
    std::vector<SteamCollection> ParseSteamCollections(std::string_view json);

    void ClickSelection(std::unordered_set<std::uint32_t>& selected, std::uint32_t& current, std::uint32_t& anchor, const std::vector<std::uint32_t>& shown, std::uint32_t appId, bool ctrl, bool shift);

    bool MatchesSearch(std::string_view foldedName, std::uint32_t appId, std::string_view needle);

    std::vector<int> LibraryView(const std::vector<Game>& games, std::string_view search, LibraryFilter filter, LibrarySort sort, std::uint32_t minReview = 0);
}
