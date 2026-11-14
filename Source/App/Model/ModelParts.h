#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace Idler::ModelParts
{
    inline constexpr double kSecondsBetweenStarts = 0.4;
    inline constexpr double kSteamCheckSeconds = 1.0;
    inline constexpr double kPlayingCheckSeconds = 3.0;
    inline constexpr double kSaveSeconds = 60.0;
    inline constexpr double kRescanSeconds = 600.0;
    inline constexpr double kSteamRetrySeconds = 10.0;
    inline constexpr double kStopGraceSeconds = 3.0;
    inline constexpr double kStoppedGraceSeconds = 15.0;
    inline constexpr int kMaxCrashes = 3;
    inline constexpr double kMaxElapsed = 5.0;
    inline constexpr double kBackOnlineSeconds = 20.0;
    inline constexpr double kShutdownCountdownSeconds = 60.0;
    inline constexpr double kAchievementsTimeoutSeconds = 30.0;
    inline constexpr std::size_t kMaxListNameBytes = 40;
    inline constexpr double kShownRestartSeconds = 120.0;
    inline constexpr double kCardCheckSeconds = 15.0 * 60.0;
    inline constexpr double kCardFullReadSeconds = 60.0 * 60.0;
    inline constexpr double kCardSetCheckSeconds = 5.0;
    inline constexpr char kCardListName[] = "Cards";
    inline constexpr double kCreatureCheckSeconds = 5.0;
    inline constexpr double kBondCheckSeconds = 30.0;
    inline constexpr int kCreatureDaysLooked = 60;
    inline constexpr std::uint64_t kSteamIdBase = 76561197960265728ull;

    std::string PathText(const std::filesystem::path& path);
    std::string CleanListName(std::string_view name);
}
