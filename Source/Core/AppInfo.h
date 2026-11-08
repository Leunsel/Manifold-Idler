#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Idler
{
    struct AppInfoEntry
    {
        std::uint32_t AppId = 0;
        std::string Type;
        std::string Name;
        bool HasCards = false;
        bool HasAchievements = false;
        std::uint32_t ReviewPercent = 0;
        std::uint32_t Metacritic = 0;
        std::uint32_t Released = 0;
    };

    struct AppInfoResult
    {
        std::vector<AppInfoEntry> Apps;
        std::size_t Damaged = 0;
        std::string Error;
    };

    AppInfoResult ParseAppInfo(std::span<const std::uint8_t> file);
}
