#include "Core/RuntimeLog.h"
#include "Core/Text.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <deque>
#include <mutex>
#ifdef _WIN32
#include <share.h>
#endif
#include <system_error>
#include <utility>

namespace Idler::RuntimeLog
{
    namespace
    {
        std::mutex g_lock;
        std::deque<LogEntry> g_entries;
        std::uint64_t g_generation = 0;
        std::FILE* g_file = nullptr;

        std::string TimeText()
        {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            char text[16] = "??:??:??";
            if (LocalTime(now, local))
                std::strftime(text, sizeof(text), "%H:%M:%S", &local);
            return text;
        }

        std::string OneLine(std::string_view message)
        {
            std::string line(message);
            for (char& c : line)
            {
                if (static_cast<unsigned char>(c) < 0x20)
                    c = ' ';
            }
            return line;
        }
    }

    const char* LevelName(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Success: return "SUCCESS";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "INFO";
        }
    }

    bool EntryMatches(const LogEntry& entry, LogLevel lowest, std::string_view text)
    {
        if (static_cast<int>(entry.Level) < static_cast<int>(lowest))
            return false;
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        const auto contains = [&](std::string_view where)
            {
                return std::search(where.begin(), where.end(), text.begin(), text.end(), [&](char a, char b) { return lower(a) == lower(b); }) != where.end();
            };
        return text.empty() || contains(entry.Message) || contains(entry.Trace);
    }

    std::string TraceText(const char* file, const char* function, int line)
    {
        const std::string_view path = file ? file : "unknown";
        const std::string_view name = function ? function : "unknown";
        const auto slash = path.find_last_of("\\/");
        const auto scope = name.rfind("::");
        std::string trace(slash == std::string_view::npos ? path : path.substr(slash + 1));
        trace += ':';
        trace += scope == std::string_view::npos ? name : name.substr(scope + 2);
        trace += ':';
        trace += std::to_string(line);
        return trace;
    }

    void OpenFile(const std::filesystem::path& path)
    {
        std::lock_guard lock(g_lock);
        if (g_file)
        {
            std::fclose(g_file);
            g_file = nullptr;
        }
        if (path.empty())
            return;

        std::error_code ignored;
        std::filesystem::rename(path, std::filesystem::path(path).replace_extension(L".previous.log"), ignored);
#ifdef _WIN32
        g_file = ::_wfsopen(path.c_str(), L"w", _SH_DENYNO);
#else
        g_file = std::fopen(path.c_str(), "w");
#endif
    }

    void Write(LogLevel level, const char* file, const char* function, int line, std::string_view message)
    {
        LogEntry entry;
        entry.Time = TimeText();
        entry.Level = level;
        entry.Trace = TraceText(file, function, line);
        entry.Message = OneLine(message);

        std::lock_guard lock(g_lock);
        if (g_file)
        {
            std::fprintf(g_file, "[%s] [%s] [%s] %s\n", entry.Time.c_str(), LevelName(level), entry.Trace.c_str(), entry.Message.c_str());
            std::fflush(g_file);
        }
        g_entries.push_back(std::move(entry));
        if (g_entries.size() > kMaxEntries)
            g_entries.pop_front();
        ++g_generation;
    }

    void Clear()
    {
        std::lock_guard lock(g_lock);
        g_entries.clear();
        ++g_generation;
    }

    std::vector<LogEntry> Entries()
    {
        std::lock_guard lock(g_lock);
        return { g_entries.begin(), g_entries.end() };
    }

    std::uint64_t Generation()
    {
        std::lock_guard lock(g_lock);
        return g_generation;
    }
}
