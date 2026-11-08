#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Idler
{
    std::string HostOf(std::string_view url);
    bool IsSteamPage(std::string_view url);
    bool IsHttps(std::string_view url);
    std::string AchievementIconAddress(std::uint32_t appId, std::string_view file);
    bool IsAchievementIconAddress(std::string_view address);
    std::string WithoutQuery(std::string_view url);

    struct PageUser
    {
        std::uint64_t SteamId = 0;
        std::string Name;
    };

    PageUser ParsePageUser(std::string_view text);
}
