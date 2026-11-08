#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <ctime>

namespace Idler
{
    std::wstring Utf8ToWide(std::string_view text);
    std::string WideToUtf8(std::wstring_view text);
    std::string DecodeJsonString(std::string_view json);

    std::string_view Trim(std::string_view text);
    bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs);
    std::string FoldCase(std::string_view text);
    std::optional<std::uint32_t> ParseUnsigned(std::string_view text);

    inline constexpr unsigned kFontNeedAsian = 1;
    inline constexpr unsigned kFontNeedOtherScripts = 2;
    unsigned FontNeedsOf(std::string_view text);

    std::string DurationText(std::uint64_t seconds);
    std::string ShortDurationText(std::uint64_t seconds);
    std::string PlaytimeText(std::uint32_t minutes);
    std::string HoursText(float hours);
    bool LocalTime(std::time_t time, std::tm& local);
    std::string DateText(std::uint32_t unixTime);
}
