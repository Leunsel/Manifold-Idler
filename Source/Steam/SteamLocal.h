#pragma once

#include "Core/AppInfo.h"
#include "Core/Library.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Idler::Steam
{
    struct Status
    {
        std::filesystem::path Root;
        std::uint32_t ProcessId = 0;
        std::uint32_t Account = 0;
        bool AccountFromClient = false;

        bool Installed() const { return !Root.empty(); }
        bool Running() const { return ProcessId != 0; }
        bool LoggedOn() const { return Running() && Account != 0; }
        bool MayConnect() const { return LoggedOn() || (Running() && AccountFromClient); }
    };

    Status ReadStatus();

    bool ProcessLives(std::uint32_t processId);

    std::filesystem::path ClientLibrary(const Status& status);

    std::vector<std::uint32_t> RunningApps();

    std::vector<std::uint8_t> ReadAppInfoFile(const Status& status);
    std::string ReadLocalConfigFile(const Status& status);
    AchievementMap ReadAchievementCounts(const Status& status, const std::vector<Ownership>& games);
    std::vector<AchievementIcons> ReadAchievementIcons(const Status& status, std::uint32_t appId);
    std::vector<StatSchema> ReadStatSchema(const Status& status, std::uint32_t appId);
    void ReadUnlockRules(const Status& status, std::vector<Game>& games);
    std::vector<SteamCollection> ReadCollections(const Status& status);
    std::filesystem::path LibraryPicture(const std::filesystem::path& root, std::uint32_t appId);
}
