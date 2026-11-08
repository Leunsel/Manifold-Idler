#include "Core/Text.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
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

    namespace
    {
        struct CaseRun
        {
            std::uint32_t First;
            std::uint32_t Last;
            std::int32_t Delta;
            std::uint32_t Step;
        };

        constexpr CaseRun kLowerRuns[] = {
            { 0x00C0, 0x00D6, 32, 1 }, { 0x00D8, 0x00DE, 32, 1 }, { 0x0100, 0x012E, 1, 2 }, { 0x0132, 0x0136, 1, 2 },
            { 0x0139, 0x0147, 1, 2 }, { 0x014A, 0x0176, 1, 2 }, { 0x0178, 0x0178, -121, 1 }, { 0x0179, 0x017D, 1, 2 },
            { 0x0181, 0x0181, 210, 1 }, { 0x0182, 0x0184, 1, 2 }, { 0x0186, 0x0186, 206, 1 }, { 0x0187, 0x0187, 1, 1 },
            { 0x0189, 0x018A, 205, 1 }, { 0x018B, 0x018B, 1, 1 }, { 0x018E, 0x018E, 79, 1 }, { 0x018F, 0x018F, 202, 1 },
            { 0x0190, 0x0190, 203, 1 }, { 0x0191, 0x0191, 1, 1 }, { 0x0193, 0x0193, 205, 1 }, { 0x0194, 0x0194, 207, 1 },
            { 0x0196, 0x0196, 211, 1 }, { 0x0197, 0x0197, 209, 1 }, { 0x0198, 0x0198, 1, 1 }, { 0x019C, 0x019C, 211, 1 },
            { 0x019D, 0x019D, 213, 1 }, { 0x019F, 0x019F, 214, 1 }, { 0x01A0, 0x01A4, 1, 2 }, { 0x01A6, 0x01A6, 218, 1 },
            { 0x01A7, 0x01A7, 1, 1 }, { 0x01A9, 0x01A9, 218, 1 }, { 0x01AC, 0x01AC, 1, 1 }, { 0x01AE, 0x01AE, 218, 1 },
            { 0x01AF, 0x01AF, 1, 1 }, { 0x01B1, 0x01B2, 217, 1 }, { 0x01B3, 0x01B5, 1, 2 }, { 0x01B7, 0x01B7, 219, 1 },
            { 0x01B8, 0x01B8, 1, 1 }, { 0x01BC, 0x01BC, 1, 1 }, { 0x01C4, 0x01C4, 2, 1 }, { 0x01C7, 0x01C7, 2, 1 },
            { 0x01CA, 0x01CA, 2, 1 }, { 0x01CD, 0x01DB, 1, 2 }, { 0x01DE, 0x01EE, 1, 2 }, { 0x01F1, 0x01F1, 2, 1 },
            { 0x01F4, 0x01F4, 1, 1 }, { 0x01F6, 0x01F6, -97, 1 }, { 0x01F7, 0x01F7, -56, 1 }, { 0x01F8, 0x021E, 1, 2 },
            { 0x0220, 0x0220, -130, 1 }, { 0x0222, 0x0232, 1, 2 }, { 0x023A, 0x023A, 10795, 1 }, { 0x023B, 0x023B, 1, 1 },
            { 0x023D, 0x023D, -163, 1 }, { 0x023E, 0x023E, 10792, 1 }, { 0x0241, 0x0241, 1, 1 }, { 0x0243, 0x0243, -195, 1 },
            { 0x0244, 0x0244, 69, 1 }, { 0x0245, 0x0245, 71, 1 }, { 0x0246, 0x024E, 1, 2 }, { 0x0370, 0x0372, 1, 2 },
            { 0x0376, 0x0376, 1, 1 }, { 0x0386, 0x0386, 38, 1 }, { 0x0388, 0x038A, 37, 1 }, { 0x038C, 0x038C, 64, 1 },
            { 0x038E, 0x038F, 63, 1 }, { 0x0391, 0x03A1, 32, 1 }, { 0x03A3, 0x03AB, 32, 1 }, { 0x03CF, 0x03CF, 8, 1 },
            { 0x03D8, 0x03EE, 1, 2 }, { 0x03F7, 0x03F7, 1, 1 }, { 0x03F9, 0x03F9, -7, 1 }, { 0x03FA, 0x03FA, 1, 1 },
            { 0x03FD, 0x03FF, -130, 1 }, { 0x0400, 0x040F, 80, 1 }, { 0x0410, 0x042F, 32, 1 }, { 0x0460, 0x0480, 1, 2 },
            { 0x048A, 0x04BE, 1, 2 }, { 0x04C0, 0x04C0, 15, 1 }, { 0x04C1, 0x04CD, 1, 2 }, { 0x04D0, 0x0522, 1, 2 },
            { 0x0531, 0x0556, 48, 1 }, { 0x10A0, 0x10C5, 7264, 1 }, { 0x1E00, 0x1E94, 1, 2 }, { 0x1EA0, 0x1EFE, 1, 2 },
            { 0x1F08, 0x1F0F, -8, 1 }, { 0x1F18, 0x1F1D, -8, 1 }, { 0x1F28, 0x1F2F, -8, 1 }, { 0x1F38, 0x1F3F, -8, 1 },
            { 0x1F48, 0x1F4D, -8, 1 }, { 0x1F59, 0x1F5F, -8, 2 }, { 0x1F68, 0x1F6F, -8, 1 }, { 0x1F88, 0x1F8F, -8, 1 },
            { 0x1F98, 0x1F9F, -8, 1 }, { 0x1FA8, 0x1FAF, -8, 1 }, { 0x1FB8, 0x1FB9, -8, 1 }, { 0x1FBA, 0x1FBB, -74, 1 },
            { 0x1FBC, 0x1FBC, -9, 1 }, { 0x1FC8, 0x1FCB, -86, 1 }, { 0x1FCC, 0x1FCC, -9, 1 }, { 0x1FD8, 0x1FD9, -8, 1 },
            { 0x1FDA, 0x1FDB, -100, 1 }, { 0x1FE8, 0x1FE9, -8, 1 }, { 0x1FEA, 0x1FEB, -112, 1 }, { 0x1FEC, 0x1FEC, -7, 1 },
            { 0x1FF8, 0x1FF9, -128, 1 }, { 0x1FFA, 0x1FFB, -126, 1 }, { 0x1FFC, 0x1FFC, -9, 1 }, { 0x2132, 0x2132, 28, 1 },
            { 0x2160, 0x216F, 16, 1 }, { 0x2183, 0x2183, 1, 1 }, { 0x24B6, 0x24CF, 26, 1 }, { 0x2C00, 0x2C2E, 48, 1 },
            { 0x2C60, 0x2C60, 1, 1 }, { 0x2C62, 0x2C62, -10743, 1 }, { 0x2C63, 0x2C63, -3814, 1 }, { 0x2C64, 0x2C64, -10727, 1 },
            { 0x2C67, 0x2C6B, 1, 2 }, { 0x2C6D, 0x2C6D, -10780, 1 }, { 0x2C6E, 0x2C6E, -10749, 1 }, { 0x2C6F, 0x2C6F, -10783, 1 },
            { 0x2C72, 0x2C72, 1, 1 }, { 0x2C75, 0x2C75, 1, 1 }, { 0x2C80, 0x2CE2, 1, 2 }, { 0xA640, 0xA65E, 1, 2 },
            { 0xA662, 0xA66C, 1, 2 }, { 0xA680, 0xA696, 1, 2 }, { 0xA722, 0xA72E, 1, 2 }, { 0xA732, 0xA76E, 1, 2 },
            { 0xA779, 0xA77B, 1, 2 }, { 0xA77D, 0xA77D, -35332, 1 }, { 0xA77E, 0xA786, 1, 2 }, { 0xA78B, 0xA78B, 1, 1 },
            { 0xFF21, 0xFF3A, 32, 1 },
        };
    }

    std::uint32_t LowerCodePoint(std::uint32_t code)
    {
        if (code < 0x80)
            return code >= 'A' && code <= 'Z' ? code + 0x20 : code;
        const CaseRun* run = std::upper_bound(std::begin(kLowerRuns), std::end(kLowerRuns), code,
            [](std::uint32_t value, const CaseRun& item) { return value < item.First; });
        if (run == std::begin(kLowerRuns))
            return code;
        --run;
        if (code > run->Last || (code - run->First) % run->Step != 0)
            return code;
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(code) + run->Delta);
    }

    std::string FoldCase(std::string_view text)
    {
        std::wstring wide = Utf8ToWide(text);
#ifdef _WIN32
        if (!wide.empty())
            ::CharLowerBuffW(wide.data(), static_cast<DWORD>(wide.size()));
#else
        for (wchar_t& character : wide)
            character = static_cast<wchar_t>(LowerCodePoint(static_cast<std::uint32_t>(character)));
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
