#include "Core/Text.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cwctype>
#endif

#include <charconv>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <system_error>

namespace Idler
{
#ifdef _WIN32
    std::wstring Utf8ToWide(std::string_view text)
    {
        if (text.empty())
            return {};
        const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
        return wide;
    }

    std::string WideToUtf8(std::wstring_view text)
    {
        if (text.empty())
            return {};
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string narrow(static_cast<std::size_t>(size), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
        return narrow;
    }
#else
    std::wstring Utf8ToWide(std::string_view text)
    {
        std::wstring wide;
        wide.reserve(text.size());
        for (std::size_t index = 0; index < text.size();)
        {
            const auto lead = static_cast<unsigned char>(text[index]);
            const std::size_t length = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 0;
            std::uint32_t code = length == 1 ? lead : length == 2 ? lead & 0x1Fu : length == 3 ? lead & 0x0Fu : lead & 0x07u;
            bool good = length != 0 && index + length <= text.size();
            for (std::size_t next = 1; good && next < length; ++next)
            {
                const auto byte = static_cast<unsigned char>(text[index + next]);
                good = (byte & 0xC0u) == 0x80u;
                code = (code << 6) | (byte & 0x3Fu);
            }
            wide.push_back(good ? static_cast<wchar_t>(code) : L'\xFFFD');
            index += good ? length : 1;
        }
        return wide;
    }

    std::string WideToUtf8(std::wstring_view text)
    {
        std::string narrow;
        narrow.reserve(text.size());
        for (const wchar_t character : text)
        {
            const auto code = static_cast<std::uint32_t>(character);
            if (code < 0x80)
            {
                narrow.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                narrow.push_back(static_cast<char>(0xC0 | (code >> 6)));
                narrow.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                narrow.push_back(static_cast<char>(0xE0 | (code >> 12)));
                narrow.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                narrow.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                narrow.push_back(static_cast<char>(0xF0 | (code >> 18)));
                narrow.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                narrow.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                narrow.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }
        return narrow;
    }
#endif

    bool LocalTime(std::time_t time, std::tm& local)
    {
#ifdef _WIN32
        return localtime_s(&local, &time) == 0;
#else
        return localtime_r(&time, &local) != nullptr;
#endif
    }

    std::string_view Trim(std::string_view text)
    {
        constexpr std::string_view space = " \t\r\n";
        const auto first = text.find_first_not_of(space);
        if (first == std::string_view::npos)
            return {};
        return text.substr(first, text.find_last_not_of(space) - first + 1);
    }

    bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t index = 0; index < lhs.size(); ++index)
        {
            const char a = lhs[index] >= 'A' && lhs[index] <= 'Z' ? static_cast<char>(lhs[index] + 32) : lhs[index];
            const char b = rhs[index] >= 'A' && rhs[index] <= 'Z' ? static_cast<char>(rhs[index] + 32) : rhs[index];
            if (a != b)
                return false;
        }
        return true;
    }

    std::string FoldCase(std::string_view text)
    {
        std::wstring wide = Utf8ToWide(text);
#ifdef _WIN32
        if (!wide.empty())
            ::CharLowerBuffW(wide.data(), static_cast<DWORD>(wide.size()));
#else
        for (wchar_t& character : wide)
            character = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(character)));
#endif
        return WideToUtf8(wide);
    }

    std::optional<std::uint32_t> ParseUnsigned(std::string_view text)
    {
        std::uint32_t value = 0;
        const char* end = text.data() + text.size();
        const auto [at, error] = std::from_chars(text.data(), end, value);
        if (error != std::errc{} || at != end || text.empty())
            return std::nullopt;
        return value;
    }

    unsigned FontNeedsOf(std::string_view text)
    {
        unsigned needs = 0;
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            const auto byte = static_cast<unsigned char>(text[index]);
            const auto next = index + 1 < text.size() ? static_cast<unsigned char>(text[index + 1]) : 0u;
            if (byte == 0xE0 || byte == 0xE1)
                needs |= kFontNeedOtherScripts;
            else if ((byte >= 0xE3 && byte <= 0xED) || (byte == 0xEF && next >= 0xA4 && next <= 0xAB) || (byte == 0xF0 && next >= 0xA0))
                needs |= kFontNeedAsian;
        }
        return needs;
    }

    std::string DurationText(std::uint64_t seconds)
    {
        char text[48];
        if (seconds < 60)
            std::snprintf(text, sizeof(text), "%llu s", static_cast<unsigned long long>(seconds));
        else if (seconds < 3600)
            std::snprintf(text, sizeof(text), "%llu min", static_cast<unsigned long long>(seconds / 60));
        else
            std::snprintf(text, sizeof(text), "%llu h %llu min", static_cast<unsigned long long>(seconds / 3600), static_cast<unsigned long long>(seconds % 3600 / 60));
        return text;
    }

    std::string ShortDurationText(std::uint64_t seconds)
    {
        return seconds < 100 * 3600 ? DurationText(seconds) : std::to_string(seconds / 3600) + " h";
    }

    std::string PlaytimeText(std::uint32_t minutes)
    {
        if (minutes == 0)
            return "not played";
        char text[32];
        if (minutes < 60)
            std::snprintf(text, sizeof(text), "%u min", minutes);
        else
            std::snprintf(text, sizeof(text), "%.1f h", static_cast<double>(minutes) / 60.0);
        return text;
    }

    std::string HoursText(float hours)
    {
        char text[32];
        if (std::fabs(hours - std::round(hours)) < 0.01f)
            std::snprintf(text, sizeof(text), "%.0f h", static_cast<double>(hours));
        else
            std::snprintf(text, sizeof(text), "%.1f h", static_cast<double>(hours));
        return text;
    }

    std::string DateText(std::uint32_t unixTime)
    {
        if (unixTime == 0)
            return {};
        const std::time_t time = unixTime;
        std::tm local{};
        if (!LocalTime(time, local))
            return {};
        char text[16];
        std::strftime(text, sizeof(text), "%Y-%m-%d", &local);
        return text;
    }

    std::string DecodeJsonString(std::string_view json)
    {
        if (json.size() < 2 || json.front() != '"' || json.back() != '"')
            return {};

        const auto hex = [&](std::size_t at, unsigned& value)
            {
                if (at + 4 > json.size() - 1)
                    return false;
                value = 0;
                for (std::size_t index = at; index < at + 4; ++index)
                {
                    const char c = json[index];
                    const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                    if (digit < 0)
                        return false;
                    value = value * 16 + static_cast<unsigned>(digit);
                }
                return true;
            };
        const auto append = [](std::string& out, unsigned code)
            {
                if (code < 0x80)
                {
                    out += static_cast<char>(code);
                }
                else if (code < 0x800)
                {
                    out += static_cast<char>(0xC0 | (code >> 6));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                else if (code < 0x10000)
                {
                    out += static_cast<char>(0xE0 | (code >> 12));
                    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                else
                {
                    out += static_cast<char>(0xF0 | (code >> 18));
                    out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
            };

        std::string out;
        out.reserve(json.size());
        for (std::size_t at = 1; at + 1 < json.size(); ++at)
        {
            if (json[at] != '\\')
            {
                out += json[at];
                continue;
            }
            if (++at + 1 >= json.size())
                return {};
            switch (json[at])
            {
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
            {
                unsigned code = 0;
                if (!hex(at + 1, code))
                    return {};
                at += 4;
                unsigned low = 0;
                if (code >= 0xD800 && code < 0xDC00 && at + 2 < json.size() && json[at + 1] == '\\' && json[at + 2] == 'u' && hex(at + 3, low) && low >= 0xDC00 && low < 0xE000)
                {
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    at += 6;
                }
                append(out, code);
                break;
            }
            default: out += json[at]; break;
            }
        }
        return out;
    }
}
