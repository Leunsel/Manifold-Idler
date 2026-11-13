#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::Zip
{
    inline constexpr std::size_t kMaxEntryBytes = 64u * 1024u * 1024u;

    struct Entry
    {
        std::string Name;
        std::string Data;
        std::time_t Time = 0;
    };

    std::uint32_t Crc32(std::string_view data);
    std::optional<std::string> Write(const std::vector<Entry>& entries);
    std::optional<std::vector<Entry>> Read(std::string_view archive);
}
