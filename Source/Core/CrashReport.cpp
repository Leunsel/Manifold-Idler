#include "Core/CrashReport.h"

#include "Core/Errors.h"
#include "Core/Text.h"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <system_error>

namespace Idler::CrashReport
{
    namespace
    {
        constexpr std::string_view kProblem = "Problem: ";
        constexpr std::string_view kNameFormat = "0000-00-00 00-00-00";

        std::string Utf8Of(const std::filesystem::path& path)
        {
            const std::u8string text = path.u8string();
            return std::string(text.begin(), text.end());
        }

        std::filesystem::path PathOf(std::string_view utf8)
        {
            return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
        }

        std::string Stamp(std::time_t time, const char* format)
        {
            std::tm local{};
            if (!LocalTime(time, local))
                return {};
            char text[40];
            const std::size_t length = std::strftime(text, sizeof(text), format, &local);
            return std::string(text, length);
        }

        bool IsReportStem(std::string_view stem)
        {
            if (stem.size() != kNameFormat.size())
                return false;
            for (std::size_t index = 0; index < stem.size(); ++index)
            {
                const char shape = kNameFormat[index];
                const char c = stem[index];
                if (shape == '0' ? (c < '0' || c > '9') : c != shape)
                    return false;
            }
            return true;
        }

        std::string LogLine(std::string_view time, std::string_view message)
        {
            std::string line = "[" + std::string(time) + "] [ERROR] [Crash] ";
            for (const char c : message)
                line.push_back(static_cast<unsigned char>(c) < 0x20 ? ' ' : c);
            line += "\n";
            return line;
        }
    }

    std::filesystem::path FolderOf(const std::filesystem::path& settings)
    {
        std::filesystem::path folder = settings;
        folder.replace_extension(".crashes");
        return folder;
    }

    std::filesystem::path MarkerOf(const std::filesystem::path& settings)
    {
        std::filesystem::path marker = settings;
        marker.replace_extension(".crashed");
        return marker;
    }

    std::string ReportName(std::time_t time)
    {
        return Stamp(time, "%Y-%m-%d %H-%M-%S");
    }

    std::string Hex(std::uint64_t value)
    {
        char text[24];
        std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
        return text;
    }

    std::string ExceptionText(std::uint32_t code)
    {
        switch (code)
        {
        case 0xC0000005u: return "access violation";
        case 0xC0000006u: return "a page could not be read in";
        case 0xC000001Du: return "illegal instruction";
        case 0xC0000025u: return "an exception that may not go on was continued";
        case 0xC000008Cu: return "array index out of bounds";
        case 0xC000008Eu: return "floating-point division by zero";
        case 0xC0000094u: return "integer division by zero";
        case 0xC0000095u: return "integer overflow";
        case 0xC0000096u: return "privileged instruction";
        case 0xC00000FDu: return "stack overflow";
        case 0xC0000374u: return "the heap is damaged";
        case 0xC0000409u: return "a security check failed, a buffer overrun most likely";
        case 0xC0000417u: return "a C runtime function got an invalid parameter";
        case 0x80000003u: return "a breakpoint";
        case 0xE06D7363u: return "a C++ exception nobody caught";
        case 0xE0000001u: return "the program called abort";
        case 0xE0000002u: return "std::terminate was called";
        case 0xE0000003u: return "a pure virtual function was called";
        case 0xE0000004u: return "a C runtime function got an invalid parameter";
        case 0xE0000005u: return "a crash on purpose, as a check";
        default: break;
        }
        return "exception " + Hex(code);
    }

    std::string AccessText(std::uint64_t kind, std::uint64_t address)
    {
        const char* what = kind == 1 ? "writing" : kind == 8 ? "running code at" : "reading";
        return std::string(what) + " address " + Hex(address);
    }

    std::string SignalText(int signal, std::uint64_t address)
    {
        switch (signal)
        {
        case SIGSEGV: return "segmentation fault at address " + Hex(address);
        case SIGILL: return "illegal instruction at address " + Hex(address);
        case SIGFPE: return "arithmetic error, a division by zero perhaps, at address " + Hex(address);
        case SIGABRT: return "the program called abort";
#ifdef SIGBUS
        case SIGBUS: return "bus error at address " + Hex(address);
#endif
        default: break;
        }
        return "signal " + std::to_string(signal);
    }

    std::string FrameText(std::size_t index, const Frame& frame)
    {
        std::string text = "#" + std::to_string(index) + " " + (frame.Module.empty() ? std::string("?") : frame.Module) + "+" + Hex(frame.Offset);
        if (!frame.Function.empty())
        {
            text += " " + frame.Function;
            if (frame.Displacement != 0)
                text += "+" + Hex(frame.Displacement);
        }
        if (!frame.File.empty())
            text += " (" + frame.File + (frame.Line != 0 ? ":" + std::to_string(frame.Line) : std::string()) + ")";
        return text;
    }

    std::vector<std::string> StackLines(const std::vector<Frame>& stack)
    {
        std::vector<std::string> lines;
        std::size_t index = 0;
        while (index < stack.size())
        {
            lines.push_back(FrameText(index, stack[index]));
            std::size_t same = index + 1;
            while (same < stack.size() && stack[same].Module == stack[index].Module && stack[same].Offset == stack[index].Offset)
                ++same;
            if (same - index > 2)
            {
                lines.push_back("#" + std::to_string(index + 1) + " to #" + std::to_string(same - 1) + " the same as #" + std::to_string(index));
                index = same;
            }
            else
            {
                ++index;
            }
        }
        return lines;
    }

    std::string ReportText(const Crash& crash)
    {
        std::string text = "Manifold Idler ended unexpectedly on " + Stamp(crash.Time, "%Y-%m-%d at %H:%M:%S") + ".\n";
        text += std::string(kProblem) + crash.Problem + "\n";
        if (!crash.Program.empty())
            text += "Program: " + crash.Program + (crash.Built.empty() ? std::string() : ", built " + crash.Built) + "\n";
        if (crash.Thread != 0)
            text += "Thread: " + std::to_string(crash.Thread) + "\n";
        text += "\nStack:\n";
        if (crash.Stack.empty())
            text += "(none could be read)\n";
        for (const std::string& line : StackLines(crash.Stack))
            text += line + "\n";
        if (!crash.Dump.empty())
            text += "\nDump: " + crash.Dump + "\n";
        return text;
    }

    std::string LogLines(const Crash& crash, const std::filesystem::path& report)
    {
        const std::string time = Stamp(crash.Time, "%H:%M:%S");
        std::string lines = LogLine(time, ErrorMessage(ErrorCode::Crashed, crash.Problem));
        for (const std::string& line : StackLines(crash.Stack))
            lines += LogLine(time, line);
        if (!report.empty())
            lines += LogLine(time, "The report is in " + Utf8Of(report));
        return lines;
    }

    std::optional<LastCrash> ReadReport(std::string_view text)
    {
        LastCrash crash;
        bool known = false;
        std::istringstream lines{ std::string(text) };
        std::string line;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.starts_with("Manifold Idler ended unexpectedly"))
                known = true;
            else if (line.starts_with(kProblem))
                crash.Problem = line.substr(kProblem.size());
            else if (line.starts_with("#"))
                crash.Stack.push_back(line);
        }
        if (!known)
            return std::nullopt;
        return crash;
    }

    std::optional<LastCrash> TakeLastCrash(const std::filesystem::path& settings)
    {
        const std::filesystem::path marker = MarkerOf(settings);
        std::error_code error;
        if (!std::filesystem::is_regular_file(marker, error))
            return std::nullopt;

        std::string name;
        {
            std::ifstream file(marker, std::ios::binary);
            std::getline(file, name);
        }
        std::filesystem::remove(marker, error);
        while (!name.empty() && (name.back() == '\r' || name.back() == ' '))
            name.pop_back();

        LastCrash crash;
        if (IsReportStem(name))
        {
            crash.Report = FolderOf(settings) / PathOf(name + ".txt");
            std::ifstream file(crash.Report, std::ios::binary);
            if (file)
            {
                const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                if (std::optional<LastCrash> read = ReadReport(text))
                {
                    read->Report = crash.Report;
                    return read;
                }
            }
        }
        return crash;
    }

    std::size_t Prune(const std::filesystem::path& settings)
    {
        std::map<std::string, std::vector<std::filesystem::path>> reports;
        std::error_code error;
        std::filesystem::directory_iterator item(FolderOf(settings), error);
        for (; !error && item != std::filesystem::directory_iterator(); item.increment(error))
        {
            std::error_code ignored;
            if (!item->is_regular_file(ignored))
                continue;
            const std::string extension = Utf8Of(item->path().extension());
            const std::string stem = Utf8Of(item->path().stem());
            if ((extension == ".txt" || extension == ".dmp") && IsReportStem(stem))
                reports[stem].push_back(item->path());
        }

        std::size_t removed = 0;
        std::size_t kept = 0;
        for (auto report = reports.rbegin(); report != reports.rend(); ++report)
        {
            if (++kept <= kKeepReports)
                continue;
            for (const std::filesystem::path& file : report->second)
            {
                std::error_code ignored;
                if (std::filesystem::remove(file, ignored))
                    ++removed;
            }
        }
        return removed;
    }
}
