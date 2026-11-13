#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::CrashReport
{
    inline constexpr std::size_t kKeepReports = 10;
    inline constexpr std::size_t kMostFrames = 64;

    struct Frame
    {
        std::string Module;
        std::uint64_t Offset = 0;
        std::string Function;
        std::uint64_t Displacement = 0;
        std::string File;
        std::uint32_t Line = 0;
    };

    struct Crash
    {
        std::time_t Time = 0;
        std::string Problem;
        std::string Program;
        std::string Version;
        std::string Built;
        std::uint64_t Thread = 0;
        std::vector<Frame> Stack;
        std::string Dump;
    };

    struct LastCrash
    {
        std::filesystem::path Report;
        std::string Problem;
        std::vector<std::string> Stack;
    };

    std::filesystem::path FolderOf(const std::filesystem::path& settings);
    std::filesystem::path MarkerOf(const std::filesystem::path& settings);
    std::string ReportName(std::time_t time);

    std::string Hex(std::uint64_t value);
    std::string ExceptionText(std::uint32_t code);
    std::string AccessText(std::uint64_t kind, std::uint64_t address);
    std::string SignalText(int signal, std::uint64_t address);
    std::string FrameText(std::size_t index, const Frame& frame);
    std::vector<std::string> StackLines(const std::vector<Frame>& stack);

    std::string ReportText(const Crash& crash);
    std::string LogLines(const Crash& crash, const std::filesystem::path& report);

    std::optional<LastCrash> ReadReport(std::string_view text);
    std::optional<LastCrash> TakeLastCrash(const std::filesystem::path& settings);
    std::size_t Prune(const std::filesystem::path& settings);
}
