#include "Web.h"

#include "Core/Errors.h"
#include "Http.h"

#include <string>
#include <string_view>

namespace Idler::Cli
{
    namespace
    {
        constexpr std::string_view kErrorPlace = "{{MI-";

        void ReplaceAll(std::string& page, std::string_view place, std::string_view text)
        {
            for (std::size_t at = page.find(place); at != std::string::npos; at = page.find(place, at + text.size()))
                page.replace(at, place.size(), text);
        }

        void ReplaceOnce(std::string& page, std::string_view place, std::string_view text)
        {
            if (const std::size_t at = page.find(place); at != std::string::npos)
                page.replace(at, place.size(), text);
        }

        void PutErrors(std::string& page)
        {
            for (std::size_t at = page.find(kErrorPlace); at != std::string::npos; at = page.find(kErrorPlace, at))
            {
                const std::size_t end = page.find("}}", at);
                const ErrorInfo* info = end == std::string::npos ? nullptr : FindError(std::string_view(page).substr(at + 2, end - at - 2));
                if (!info)
                {
                    at += kErrorPlace.size();
                    continue;
                }
                const std::string text = JsonString(ErrorMessage(info->Code, "{}"));
                page.replace(at, end + 2 - at, text);
                at += text.size();
            }
        }
    }

    std::string WebPage(std::string_view nonce, const ThemeSet& themes)
    {
        std::string page = WebPageHtml();
        ReplaceOnce(page, "{{style}}", WebPageStyle());
        std::string script = WebPageScript();
        PutErrors(script);
        ReplaceOnce(page, "{{script}}", script);
        ReplaceAll(page, "{{logo}}", WebLogo());
        ReplaceAll(page, "{{nonce}}", nonce);
        ReplaceOnce(page, "{{themes}}", ThemesJson(themes));
        return page;
    }
}
