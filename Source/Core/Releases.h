#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Idler::Releases
{
    inline constexpr std::string_view kLatestAddress = "https://api.github.com/repos/Leunsel/Manifold-Idler/releases/latest";
    inline constexpr std::string_view kReleasesPage = "https://github.com/Leunsel/Manifold-Idler/releases";
    inline constexpr std::uint32_t kCheckSeconds = 24u * 3600u;
    inline constexpr double kFirstCheckSeconds = 30.0;
    inline constexpr double kRetrySeconds = 3600.0;

    struct Release
    {
        std::string Version;
        std::string Page;
    };

    std::optional<std::array<int, 3>> ParseVersion(std::string_view text);
    bool IsNewer(std::string_view candidate, std::string_view current);
    bool IsReleasePage(std::string_view address);
    std::string PageOf(std::string_view version);
    std::optional<Release> ParseLatest(std::string_view json, std::string& problem);
    bool Due(std::uint32_t lastCheck, std::uint32_t now);
}
