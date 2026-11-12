#include "Core/AppInfo.h"

#include "Core/Errors.h"
#include "Core/Text.h"

#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

namespace Idler
{
    namespace
    {
        constexpr std::uint32_t kMagic27 = 0x07564427;
        constexpr std::uint32_t kMagic28 = 0x07564428;
        constexpr std::uint32_t kMagic29 = 0x07564429;

        constexpr std::uint8_t kObject = 0;
        constexpr std::uint8_t kString = 1;
        constexpr std::uint8_t kInt32 = 2;
        constexpr std::uint8_t kFloat = 3;
        constexpr std::uint8_t kPointer = 4;
        constexpr std::uint8_t kWideString = 5;
        constexpr std::uint8_t kColor = 6;
        constexpr std::uint8_t kUInt64 = 7;
        constexpr std::uint8_t kEnd = 8;
        constexpr std::uint8_t kInt64 = 10;
        constexpr std::uint8_t kAlternateEnd = 11;

        constexpr int kMaxDepth = 64;

        class Reader
        {
        public:
            Reader(std::span<const std::uint8_t> data, std::size_t at = 0) : m_data(data), m_at(at) {}

            bool Ok() const { return m_ok; }
            std::size_t At() const { return m_at; }

            template <typename T>
            T Read()
            {
                T value{};
                if (!m_ok || m_data.size() < sizeof(T) || m_at > m_data.size() - sizeof(T))
                {
                    m_ok = false;
                    return value;
                }
                std::memcpy(&value, m_data.data() + m_at, sizeof(T));
                m_at += sizeof(T);
                return value;
            }

            void Skip(std::size_t count)
            {
                if (!m_ok || m_at > m_data.size() || count > m_data.size() - m_at)
                    m_ok = false;
                else
                    m_at += count;
            }

            std::string_view String()
            {
                if (!m_ok || m_at >= m_data.size())
                {
                    m_ok = false;
                    return {};
                }
                const auto* start = reinterpret_cast<const char*>(m_data.data() + m_at);
                const auto* end = static_cast<const char*>(std::memchr(start, 0, m_data.size() - m_at));
                if (!end)
                {
                    m_ok = false;
                    return {};
                }
                const std::string_view text(start, static_cast<std::size_t>(end - start));
                m_at += text.size() + 1;
                return text;
            }

            void SkipWideString()
            {
                while (m_ok && Read<std::uint16_t>() != 0)
                {
                }
            }

        private:
            std::span<const std::uint8_t> m_data;
            std::size_t m_at = 0;
            bool m_ok = true;
        };

        class EntryParser
        {
        public:
            explicit EntryParser(const std::vector<std::string_view>* strings) : m_strings(strings) {}

            bool Root(Reader& reader, AppInfoEntry& entry, int depth = 0)
            {
                while (true)
                {
                    const std::uint8_t type = reader.Read<std::uint8_t>();
                    if (!reader.Ok())
                        return false;
                    if (type == kEnd || type == kAlternateEnd)
                        return true;

                    const std::string_view key = Key(reader);
                    if (type == kObject && depth == 0 && EqualsIgnoreCase(key, "appinfo"))
                    {
                        if (!Root(reader, entry, 1))
                            return false;
                    }
                    else if (type == kObject && EqualsIgnoreCase(key, "common"))
                    {
                        if (!Common(reader, entry))
                            return false;
                    }
                    else if (!SkipValue(reader, type, depth + 1))
                    {
                        return false;
                    }
                }
            }

        private:
            std::string_view Key(Reader& reader)
            {
                if (!m_strings)
                    return reader.String();
                const std::uint32_t index = reader.Read<std::uint32_t>();
                if (index >= m_strings->size())
                    return {};
                return (*m_strings)[index];
            }

            bool Common(Reader& reader, AppInfoEntry& entry)
            {
                while (true)
                {
                    const std::uint8_t type = reader.Read<std::uint8_t>();
                    if (!reader.Ok())
                        return false;
                    if (type == kEnd || type == kAlternateEnd)
                        return true;

                    const std::string_view key = Key(reader);
                    if (type == kString && EqualsIgnoreCase(key, "name"))
                    {
                        entry.Name = reader.String();
                    }
                    else if (type == kString && EqualsIgnoreCase(key, "type"))
                    {
                        entry.Type = reader.String();
                        for (char& c : entry.Type)
                            c = c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
                    }
                    else if (type == kObject && EqualsIgnoreCase(key, "category"))
                    {
                        if (!Category(reader, entry))
                            return false;
                    }
                    else if ((type == kInt32 || type == kString) && EqualsIgnoreCase(key, "review_percentage"))
                    {
                        entry.ReviewPercent = Number(reader, type);
                    }
                    else if ((type == kInt32 || type == kString) && EqualsIgnoreCase(key, "metacritic_score"))
                    {
                        entry.Metacritic = Number(reader, type);
                    }
                    else if ((type == kInt32 || type == kString) && EqualsIgnoreCase(key, "steam_release_date"))
                    {
                        entry.Released = Number(reader, type);
                    }
                    else if ((type == kInt32 || type == kString) && EqualsIgnoreCase(key, "original_release_date"))
                    {
                        const std::uint32_t original = Number(reader, type);
                        if (original != 0 && (entry.Released == 0 || original < entry.Released))
                            entry.Released = original;
                    }
                    else if (!SkipValue(reader, type, 2))
                    {
                        return false;
                    }
                }
            }

            static std::uint32_t Number(Reader& reader, std::uint8_t type)
            {
                if (type == kInt32)
                    return reader.Read<std::uint32_t>();
                return ParseUnsigned(reader.String()).value_or(0);
            }

            bool Category(Reader& reader, AppInfoEntry& entry)
            {
                while (true)
                {
                    const std::uint8_t type = reader.Read<std::uint8_t>();
                    if (!reader.Ok())
                        return false;
                    if (type == kEnd || type == kAlternateEnd)
                        return true;

                    const std::string_view key = Key(reader);
                    if (EqualsIgnoreCase(key, "category_29"))
                        entry.HasCards = true;
                    else if (EqualsIgnoreCase(key, "category_22"))
                        entry.HasAchievements = true;
                    if (!SkipValue(reader, type, 3))
                        return false;
                }
            }

            bool SkipValue(Reader& reader, std::uint8_t type, int depth)
            {
                switch (type)
                {
                case kObject:
                    return SkipObject(reader, depth + 1);
                case kString:
                    reader.String();
                    break;
                case kWideString:
                    reader.SkipWideString();
                    break;
                case kInt32:
                case kFloat:
                case kPointer:
                case kColor:
                    reader.Skip(4);
                    break;
                case kUInt64:
                case kInt64:
                    reader.Skip(8);
                    break;
                default:
                    return false;
                }
                return reader.Ok();
            }

            bool SkipObject(Reader& reader, int depth)
            {
                if (depth > kMaxDepth)
                    return false;
                while (true)
                {
                    const std::uint8_t type = reader.Read<std::uint8_t>();
                    if (!reader.Ok())
                        return false;
                    if (type == kEnd || type == kAlternateEnd)
                        return true;
                    Key(reader);
                    if (!SkipValue(reader, type, depth))
                        return false;
                }
            }

            const std::vector<std::string_view>* m_strings;
        };

        bool ReadStringTable(std::span<const std::uint8_t> file, std::int64_t offset, std::vector<std::string_view>& strings)
        {
            if (offset < 0 || static_cast<std::uint64_t>(offset) >= file.size())
                return false;
            Reader reader(file, static_cast<std::size_t>(offset));
            const std::uint32_t count = reader.Read<std::uint32_t>();
            if (!reader.Ok() || count > file.size())
                return false;
            strings.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                strings.push_back(reader.String());
                if (!reader.Ok())
                    return false;
            }
            return true;
        }
    }

    AppInfoResult ParseAppInfo(std::span<const std::uint8_t> file)
    {
        AppInfoResult result;
        Reader reader(file);
        const std::uint32_t magic = reader.Read<std::uint32_t>();
        reader.Skip(4);
        if (!reader.Ok())
        {
            result.Error = ErrorMessage(ErrorCode::AppInfoTooShort);
            return result;
        }
        if (magic != kMagic27 && magic != kMagic28 && magic != kMagic29)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%08X", magic);
            result.Error = ErrorMessage(ErrorCode::AppInfoNewerFormat, text);
            return result;
        }

        std::vector<std::string_view> strings;
        std::size_t tableStart = file.size();
        if (magic == kMagic29)
        {
            const std::int64_t offset = reader.Read<std::int64_t>();
            if (!reader.Ok() || !ReadStringTable(file, offset, strings))
            {
                result.Error = ErrorMessage(ErrorCode::AppInfoStringsDamaged);
                return result;
            }
            tableStart = static_cast<std::size_t>(offset);
        }
        EntryParser parser(magic == kMagic29 ? &strings : nullptr);

        const std::size_t headerSize = 4 + 4 + 8 + 20 + 4 + (magic >= kMagic28 ? 20u : 0u);
        while (true)
        {
            const std::uint32_t appId = reader.Read<std::uint32_t>();
            if (!reader.Ok())
            {
                result.Error = ErrorMessage(ErrorCode::AppInfoEndsEarly);
                return result;
            }
            if (appId == 0)
                return result;

            const std::uint32_t size = reader.Read<std::uint32_t>();
            if (!reader.Ok() || size > tableStart || reader.At() > tableStart - size)
            {
                result.Error = ErrorMessage(ErrorCode::AppInfoEntryDamaged);
                return result;
            }
            const std::size_t end = reader.At() + size;

            AppInfoEntry entry;
            entry.AppId = appId;
            Reader body(file.first(end), reader.At());
            body.Skip(headerSize);
            if (body.Ok() && parser.Root(body, entry))
                result.Apps.push_back(std::move(entry));
            else
                ++result.Damaged;

            reader.Skip(size);
        }
    }
}
