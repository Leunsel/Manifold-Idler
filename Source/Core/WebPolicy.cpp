#include "Core/WebPolicy.h"

#include "Core/Text.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <system_error>

namespace Idler
{
    namespace
    {
        constexpr std::string_view kScheme = "://";

        std::string_view AuthorityOf(std::string_view url)
        {
            const auto scheme = url.find(kScheme);
            if (scheme == std::string_view::npos)
                return {};
            const std::string_view rest = url.substr(scheme + kScheme.size());
            return rest.substr(0, rest.find_first_of("/\\?#"));
        }

        bool EndsWithDomain(std::string_view host, std::string_view domain)
        {
            if (host == domain)
                return true;
            return host.size() > domain.size() && host.ends_with(domain) && host[host.size() - domain.size() - 1] == '.';
        }
    }

    std::string HostOf(std::string_view url)
    {
        std::string_view authority = AuthorityOf(url);
        if (const auto at = authority.rfind('@'); at != std::string_view::npos)
            authority = authority.substr(at + 1);
        authority = authority.substr(0, authority.find(':'));
        std::string host(authority);
        std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return host;
    }

    namespace
    {
        constexpr std::string_view kIconRoot = "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/";

        bool IsIconFileName(std::string_view file)
        {
            if (file.size() < 5 || file.size() > 128 || file.find("..") != std::string_view::npos)
                return false;
            for (const char letter : file)
            {
                const bool plain = (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9') || letter == '_' || letter == '-' || letter == '.';
                if (!plain)
                    return false;
            }
            return file.ends_with(".jpg") || file.ends_with(".png");
        }
    }

    std::string AchievementIconAddress(std::uint32_t appId, std::string_view file)
    {
        if (appId == 0 || !IsIconFileName(file))
            return {};
        return std::string(kIconRoot) + std::to_string(appId) + "/" + std::string(file);
    }

    bool IsAchievementIconAddress(std::string_view address)
    {
        if (!address.starts_with(kIconRoot))
            return false;
        const std::string_view rest = address.substr(kIconRoot.size());
        const auto slash = rest.find('/');
        if (slash == std::string_view::npos || slash == 0)
            return false;
        const std::string_view app = rest.substr(0, slash);
        const bool digits = std::all_of(app.begin(), app.end(), [](char letter) { return letter >= '0' && letter <= '9'; });
        return digits && IsIconFileName(rest.substr(slash + 1));
    }

    std::string WithoutQuery(std::string_view url)
    {
        return std::string(url.substr(0, url.find_first_of("?#")));
    }

    PageUser ParsePageUser(std::string_view text)
    {
        constexpr std::uint64_t kFirstPersonalId = 76561197960265729ull;
        constexpr std::size_t kIdDigits = 17;
        PageUser user;
        const auto bar = text.find('|');
        const std::string_view id = text.substr(0, bar);
        std::uint64_t value = 0;
        const auto [end, error] = std::from_chars(id.data(), id.data() + id.size(), value);
        if (id.size() != kIdDigits || error != std::errc{} || end != id.data() + id.size() || value < kFirstPersonalId)
            return user;
        user.SteamId = value;
        if (bar != std::string_view::npos)
            user.Name = text.substr(bar + 1);
        return user;
    }

    bool IsHttps(std::string_view url)
    {
        return url.size() > 8 && EqualsIgnoreCase(url.substr(0, 8), "https://");
    }

    bool IsSteamPage(std::string_view url)
    {
        if (!IsHttps(url) || AuthorityOf(url).find('@') != std::string_view::npos)
            return false;
        const std::string host = HostOf(url);
        constexpr std::array<std::string_view, 2> kDomains = { "steamcommunity.com", "steampowered.com" };
        return std::any_of(kDomains.begin(), kDomains.end(), [&](std::string_view domain) { return EndsWithDomain(host, domain); });
    }
}
