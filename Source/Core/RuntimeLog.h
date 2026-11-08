#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Idler
{
    enum class LogLevel
    {
        Info,
        Success,
        Warning,
        Error
    };

    struct LogEntry
    {
        std::string Time;
        LogLevel Level = LogLevel::Info;
        std::string Trace;
        std::string Message;
    };

    namespace RuntimeLog
    {
        inline constexpr std::size_t kMaxEntries = 1000;

        const char* LevelName(LogLevel level);
        bool EntryMatches(const LogEntry& entry, LogLevel lowest, std::string_view text);
        std::string TraceText(const char* file, const char* function, int line);

        void OpenFile(const std::filesystem::path& path);
        void Write(LogLevel level, const char* file, const char* function, int line, std::string_view message);
        void Clear();
        std::vector<LogEntry> Entries();
        std::uint64_t Generation();
    }
}

#define LOG_INFO(message)    ::Idler::RuntimeLog::Write(::Idler::LogLevel::Info, __FILE__, __FUNCTION__, __LINE__, (message))
#define LOG_SUCCESS(message) ::Idler::RuntimeLog::Write(::Idler::LogLevel::Success, __FILE__, __FUNCTION__, __LINE__, (message))
#define LOG_WARN(message)    ::Idler::RuntimeLog::Write(::Idler::LogLevel::Warning, __FILE__, __FUNCTION__, __LINE__, (message))
#define LOG_ERROR(message)   ::Idler::RuntimeLog::Write(::Idler::LogLevel::Error, __FILE__, __FUNCTION__, __LINE__, (message))
