#include "Core/Zip.h"

#include "Core/Deflate.h"
#include "Core/Text.h"

#include <array>

namespace Idler::Zip
{
    namespace
    {
        constexpr std::uint32_t kLocalSignature = 0x04034B50u;
        constexpr std::uint32_t kCentralSignature = 0x02014B50u;
        constexpr std::uint32_t kEndSignature = 0x06054B50u;
        constexpr std::uint32_t kVersion = 20;
        constexpr std::uint32_t kUtf8Names = 0x0800;
        constexpr std::uint32_t kEncrypted = 0x0001;
        constexpr std::uint32_t kStored = 0;
        constexpr std::uint32_t kDeflated = 8;
        constexpr std::uint32_t kTooLarge = 0xFFFFFFFFu;
        constexpr std::size_t kLocalSize = 30;
        constexpr std::size_t kCentralSize = 46;
        constexpr std::size_t kEndSize = 22;
        constexpr std::size_t kLongestComment = 0xFFFF;

        constexpr std::array<std::uint32_t, 256> kCrcTable = []()
        {
            std::array<std::uint32_t, 256> table{};
            for (std::uint32_t index = 0; index < 256; ++index)
            {
                std::uint32_t value = index;
                for (int bit = 0; bit < 8; ++bit)
                    value = (value & 1u) != 0 ? 0xEDB88320u ^ (value >> 1) : value >> 1;
                table[index] = value;
            }
            return table;
        }();

        void Put16(std::string& out, std::uint32_t value)
        {
            out.push_back(static_cast<char>(value & 0xFFu));
            out.push_back(static_cast<char>((value >> 8) & 0xFFu));
        }

        void Put32(std::string& out, std::uint32_t value)
        {
            Put16(out, value & 0xFFFFu);
            Put16(out, value >> 16);
        }

        std::uint32_t Get16(std::string_view data, std::size_t at)
        {
            return static_cast<std::uint32_t>(static_cast<unsigned char>(data[at])) | static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + 1])) << 8;
        }

        std::uint32_t Get32(std::string_view data, std::size_t at)
        {
            return Get16(data, at) | Get16(data, at + 2) << 16;
        }

        struct DosTime
        {
            std::uint32_t Time = 0;
            std::uint32_t Date = (1u << 5) | 1u;
        };

        DosTime DosOf(std::time_t time)
        {
            std::tm local{};
            if (time <= 0 || !LocalTime(time, local) || local.tm_year < 80 || local.tm_year > 207)
                return {};
            DosTime dos;
            dos.Time = static_cast<std::uint32_t>((local.tm_hour << 11) | (local.tm_min << 5) | (local.tm_sec / 2));
            dos.Date = static_cast<std::uint32_t>(((local.tm_year - 80) << 9) | ((local.tm_mon + 1) << 5) | local.tm_mday);
            return dos;
        }

        std::time_t TimeOf(std::uint32_t time, std::uint32_t date)
        {
            std::tm local{};
            local.tm_year = static_cast<int>((date >> 9) & 0x7Fu) + 80;
            local.tm_mon = static_cast<int>((date >> 5) & 0x0Fu) - 1;
            local.tm_mday = static_cast<int>(date & 0x1Fu);
            local.tm_hour = static_cast<int>((time >> 11) & 0x1Fu);
            local.tm_min = static_cast<int>((time >> 5) & 0x3Fu);
            local.tm_sec = static_cast<int>(time & 0x1Fu) * 2;
            local.tm_isdst = -1;
            const std::time_t result = std::mktime(&local);
            return result == static_cast<std::time_t>(-1) ? 0 : result;
        }
    }

    std::uint32_t Crc32(std::string_view data)
    {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (const char c : data)
            crc = kCrcTable[(crc ^ static_cast<unsigned char>(c)) & 0xFFu] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFFu;
    }

    std::optional<std::string> Write(const std::vector<Entry>& entries)
    {
        if (entries.size() > 0xFFFFu)
            return std::nullopt;

        std::string out;
        std::string central;
        for (const Entry& entry : entries)
        {
            if (entry.Name.empty() || entry.Name.size() > 0xFFFFu || entry.Data.size() > kMaxEntryBytes)
                return std::nullopt;

            const std::uint32_t crc = Crc32(entry.Data);
            const std::optional<std::string> packed = entry.Data.empty() ? std::nullopt : Deflate::Pack(entry.Data);
            const bool deflated = packed && packed->size() < entry.Data.size();
            const std::string_view kept = deflated ? std::string_view(*packed) : std::string_view(entry.Data);
            if (out.size() + kLocalSize + entry.Name.size() + kept.size() >= kTooLarge)
                return std::nullopt;

            const auto offset = static_cast<std::uint32_t>(out.size());
            const DosTime when = DosOf(entry.Time);
            const std::uint32_t method = deflated ? kDeflated : kStored;
            const auto keptSize = static_cast<std::uint32_t>(kept.size());
            const auto size = static_cast<std::uint32_t>(entry.Data.size());
            const auto nameSize = static_cast<std::uint32_t>(entry.Name.size());

            Put32(out, kLocalSignature);
            Put16(out, kVersion);
            Put16(out, kUtf8Names);
            Put16(out, method);
            Put16(out, when.Time);
            Put16(out, when.Date);
            Put32(out, crc);
            Put32(out, keptSize);
            Put32(out, size);
            Put16(out, nameSize);
            Put16(out, 0);
            out += entry.Name;
            out += kept;

            Put32(central, kCentralSignature);
            Put16(central, kVersion);
            Put16(central, kVersion);
            Put16(central, kUtf8Names);
            Put16(central, method);
            Put16(central, when.Time);
            Put16(central, when.Date);
            Put32(central, crc);
            Put32(central, keptSize);
            Put32(central, size);
            Put16(central, nameSize);
            Put16(central, 0);
            Put16(central, 0);
            Put16(central, 0);
            Put16(central, 0);
            Put32(central, 0);
            Put32(central, offset);
            central += entry.Name;
        }

        if (out.size() + central.size() + kEndSize >= kTooLarge)
            return std::nullopt;
        const auto centralOffset = static_cast<std::uint32_t>(out.size());
        const auto count = static_cast<std::uint32_t>(entries.size());
        const auto centralSize = static_cast<std::uint32_t>(central.size());
        out += central;
        Put32(out, kEndSignature);
        Put16(out, 0);
        Put16(out, 0);
        Put16(out, count);
        Put16(out, count);
        Put32(out, centralSize);
        Put32(out, centralOffset);
        Put16(out, 0);
        return out;
    }

    std::optional<std::vector<Entry>> Read(std::string_view archive)
    {
        if (archive.size() < kEndSize)
            return std::nullopt;

        std::size_t end = std::string_view::npos;
        const std::size_t lowest = archive.size() > kEndSize + kLongestComment ? archive.size() - kEndSize - kLongestComment : 0;
        for (std::size_t at = archive.size() - kEndSize + 1; at-- > lowest;)
        {
            if (Get32(archive, at) == kEndSignature && at + kEndSize + Get16(archive, at + 20) == archive.size())
            {
                end = at;
                break;
            }
        }
        if (end == std::string_view::npos)
            return std::nullopt;

        const std::uint32_t count = Get16(archive, end + 10);
        const std::uint32_t centralSize = Get32(archive, end + 12);
        const std::uint32_t centralOffset = Get32(archive, end + 16);
        if (Get16(archive, end + 4) != 0 || Get16(archive, end + 6) != 0 || Get16(archive, end + 8) != count)
            return std::nullopt;
        if (static_cast<std::size_t>(centralOffset) + centralSize > end)
            return std::nullopt;

        std::vector<Entry> entries;
        std::size_t at = centralOffset;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            if (at + kCentralSize > end || Get32(archive, at) != kCentralSignature)
                return std::nullopt;
            const std::uint32_t flags = Get16(archive, at + 8);
            const std::uint32_t method = Get16(archive, at + 10);
            const std::uint32_t time = Get16(archive, at + 12);
            const std::uint32_t date = Get16(archive, at + 14);
            const std::uint32_t crc = Get32(archive, at + 16);
            const std::uint32_t keptSize = Get32(archive, at + 20);
            const std::uint32_t size = Get32(archive, at + 24);
            const std::size_t nameSize = Get16(archive, at + 28);
            const std::size_t extraSize = Get16(archive, at + 30);
            const std::size_t commentSize = Get16(archive, at + 32);
            const std::uint32_t offset = Get32(archive, at + 42);
            if (at + kCentralSize + nameSize + extraSize + commentSize > end)
                return std::nullopt;
            if ((flags & kEncrypted) != 0 || (method != kStored && method != kDeflated))
                return std::nullopt;
            if (keptSize == kTooLarge || size == kTooLarge || offset == kTooLarge || size > kMaxEntryBytes)
                return std::nullopt;

            if (static_cast<std::size_t>(offset) + kLocalSize > centralOffset || Get32(archive, offset) != kLocalSignature)
                return std::nullopt;
            const std::size_t data = static_cast<std::size_t>(offset) + kLocalSize + Get16(archive, offset + 26) + Get16(archive, offset + 28);
            if (data + keptSize > centralOffset)
                return std::nullopt;

            Entry entry;
            entry.Name = std::string(archive.substr(at + kCentralSize, nameSize));
            entry.Time = TimeOf(time, date);
            const std::string_view kept = archive.substr(data, keptSize);
            if (method == kStored)
            {
                if (keptSize != size)
                    return std::nullopt;
                entry.Data = std::string(kept);
            }
            else
            {
                std::optional<std::string> unpacked = Deflate::Unpack(kept, size);
                if (!unpacked)
                    return std::nullopt;
                entry.Data = std::move(*unpacked);
            }
            if (Crc32(entry.Data) != crc)
                return std::nullopt;
            entries.push_back(std::move(entry));
            at += kCentralSize + nameSize + extraSize + commentSize;
        }
        return entries;
    }
}
