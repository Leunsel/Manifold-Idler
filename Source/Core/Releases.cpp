#include "Core/Releases.h"

#include "Core/Json.h"

namespace Idler::Releases
{
    std::optional<std::array<int, 3>> ParseVersion(std::string_view text)
    {
        if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
            text.remove_prefix(1);
        std::array<int, 3> parts{};
        std::size_t part = 0;
        std::size_t digits = 0;
        for (const char c : text)
        {
            if (c >= '0' && c <= '9')
            {
                if (++digits > 6)
                    return std::nullopt;
                parts[part] = parts[part] * 10 + (c - '0');
            }
            else if (c == '.' && digits > 0 && part < 2)
            {
                ++part;
                digits = 0;
            }
            else
            {
                return std::nullopt;
            }
        }
        if (part != 2 || digits == 0)
            return std::nullopt;
        return parts;
    }

    bool IsNewer(std::string_view candidate, std::string_view current)
    {
        const std::optional<std::array<int, 3>> newer = ParseVersion(candidate);
        const std::optional<std::array<int, 3>> now = ParseVersion(current);
        return newer && now && *newer > *now;
    }

    bool IsReleasePage(std::string_view address)
    {
        if (!address.starts_with(kReleasesPage))
            return false;
        const std::string_view rest = address.substr(kReleasesPage.size());
        if (rest.empty())
            return true;
        if (!rest.starts_with("/tag/v"))
            return false;
        return ParseVersion(rest.substr(5)).has_value();
    }

    std::string PageOf(std::string_view version)
    {
        if (!ParseVersion(version))
            return std::string(kReleasesPage);
        const std::string_view plain = version.front() == 'v' || version.front() == 'V' ? version.substr(1) : version;
        return std::string(kReleasesPage) + "/tag/v" + std::string(plain);
    }

    std::optional<Release> ParseLatest(std::string_view json, std::string& problem)
    {
        const std::optional<Json::Value> root = Json::Parse(json, problem);
        if (!root)
            return std::nullopt;
        if (!root->Is(Json::Kind::Object))
        {
            problem = "the answer is not a release";
            return std::nullopt;
        }
        const Json::Value* tag = root->Find("tag_name");
        if (!tag || !tag->Is(Json::Kind::String))
        {
            problem = "the answer names no release";
            return std::nullopt;
        }
        const std::optional<std::array<int, 3>> parts = ParseVersion(tag->Text);
        if (!parts)
        {
            problem = "the newest release has no version number: " + tag->Text.substr(0, 40);
            return std::nullopt;
        }
        for (const char* flag : { "draft", "prerelease" })
        {
            const Json::Value* value = root->Find(flag);
            if (value && value->Is(Json::Kind::Boolean) && value->Flag)
            {
                problem = "the newest release is not final";
                return std::nullopt;
            }
        }

        Release release;
        release.Version = std::to_string((*parts)[0]) + "." + std::to_string((*parts)[1]) + "." + std::to_string((*parts)[2]);
        const Json::Value* page = root->Find("html_url");
        release.Page = page && page->Is(Json::Kind::String) && IsReleasePage(page->Text) ? page->Text : PageOf(release.Version);
        return release;
    }

    bool Due(std::uint32_t lastCheck, std::uint32_t now)
    {
        return lastCheck == 0 || now < lastCheck || now - lastCheck >= kCheckSeconds;
    }
}
