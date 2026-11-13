#include "App/CrashHandler.h"

#include "Core/CrashReport.h"
#include "Core/Text.h"
#include "Core/Version.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <unwind.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <typeinfo>
#include <ctime>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

namespace Idler::CrashHandler
{
    namespace
    {
        constexpr unsigned kReportSeconds = 10;
        constexpr std::size_t kAltStackBytes = 128 * 1024;
        constexpr int kSignals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };

        std::filesystem::path g_settings;
        std::filesystem::path g_log;
        std::atomic<bool> g_crashing{ false };
        alignas(16) char g_altStack[kAltStackBytes];
        char g_why[768];

        bool WriteText(const std::filesystem::path& path, std::string_view text, bool append)
        {
            const int file = ::open(path.c_str(), O_WRONLY | O_CLOEXEC | (append ? O_APPEND : O_CREAT | O_TRUNC), 0644);
            if (file < 0)
                return false;
            std::size_t done = 0;
            while (done < text.size())
            {
                const ssize_t written = ::write(file, text.data() + done, text.size() - done);
                if (written <= 0)
                    break;
                done += static_cast<std::size_t>(written);
            }
            ::close(file);
            return done == text.size();
        }

        struct Collected
        {
            void** Frames = nullptr;
            int Count = 0;
            int Most = 0;
        };

        _Unwind_Reason_Code Collect(_Unwind_Context* context, void* data)
        {
            Collected& collected = *static_cast<Collected*>(data);
            const std::uintptr_t address = static_cast<std::uintptr_t>(_Unwind_GetIP(context));
            if (address == 0 || collected.Count >= collected.Most)
                return _URC_END_OF_STACK;
            collected.Frames[collected.Count++] = reinterpret_cast<void*>(address);
            return _URC_NO_REASON;
        }

        int Backtrace(void** frames, int most)
        {
            Collected collected;
            collected.Frames = frames;
            collected.Most = most;
            _Unwind_Backtrace(Collect, &collected);
            return collected.Count;
        }

        std::string ProgramPath()
        {
            char path[4096];
            const ssize_t length = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
            return length > 0 ? std::string(path, static_cast<std::size_t>(length)) : std::string();
        }

        std::string Demangled(const char* name)
        {
            int status = 0;
            char* plain = abi::__cxa_demangle(name, nullptr, nullptr, &status);
            std::string text = status == 0 && plain ? std::string(plain) : std::string(name);
            std::free(plain);
            return text;
        }

        std::uintptr_t FaultAddress(void* context)
        {
#if defined(__x86_64__)
            if (context)
                return static_cast<std::uintptr_t>(static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP]);
#else
            static_cast<void>(context);
#endif
            return 0;
        }

        void OnSignal(int signal, siginfo_t* info, void* context)
        {
            if (g_crashing.exchange(true))
                return;
            ::alarm(kReportSeconds);

            CrashReport::Crash crash;
            crash.Time = std::time(nullptr);
            crash.Thread = static_cast<std::uint64_t>(::gettid());
            crash.Version = kVersion;
            crash.Program = ProgramPath();
            const std::uintptr_t address = info && signal != SIGABRT ? reinterpret_cast<std::uintptr_t>(info->si_addr) : 0;
            crash.Problem = signal == SIGABRT && g_why[0] != 0 ? std::string(g_why) : CrashReport::SignalText(signal, address);

            void* frames[CrashReport::kMostFrames];
            const int count = Backtrace(frames, static_cast<int>(CrashReport::kMostFrames));
            const std::uintptr_t fault = FaultAddress(context);
            int first = 0;
            for (int index = 0; fault != 0 && index < count; ++index)
            {
                if (reinterpret_cast<std::uintptr_t>(frames[index]) == fault)
                {
                    first = index;
                    break;
                }
            }
            for (int index = first; index < count; ++index)
            {
                const auto where = reinterpret_cast<std::uintptr_t>(frames[index]);
                CrashReport::Frame frame;
                Dl_info found{};
                if (::dladdr(frames[index], &found) && found.dli_fname)
                {
                    const std::string module = found.dli_fname;
                    frame.Module = module.substr(module.find_last_of('/') + 1);
                    frame.Offset = where - reinterpret_cast<std::uintptr_t>(found.dli_fbase);
                    if (found.dli_sname && found.dli_saddr)
                    {
                        frame.Function = Demangled(found.dli_sname);
                        frame.Displacement = where - reinterpret_cast<std::uintptr_t>(found.dli_saddr);
                    }
                }
                else
                {
                    frame.Offset = where;
                }
                crash.Stack.push_back(std::move(frame));
            }

            const std::string name = CrashReport::ReportName(crash.Time);
            const std::filesystem::path folder = CrashReport::FolderOf(g_settings);
            std::error_code error;
            std::filesystem::create_directories(folder, error);
            const std::filesystem::path report = folder / (name + ".txt");
            const bool written = WriteText(report, CrashReport::ReportText(crash), false);
            if (written)
                WriteText(CrashReport::MarkerOf(g_settings), name + "\n", false);
            if (!g_log.empty())
                WriteText(g_log, CrashReport::LogLines(crash, written ? report : std::filesystem::path()), true);

            ::raise(signal);
        }

        [[noreturn]] void OnTerminate()
        {
            std::string why = "std::terminate was called";
            if (const std::type_info* type = abi::__cxa_current_exception_type())
            {
                why = "a C++ exception nobody caught: " + Demangled(type->name());
                try
                {
                    std::rethrow_exception(std::current_exception());
                }
                catch (const std::exception& error)
                {
                    why += ", \"" + std::string(error.what()) + "\"";
                }
                catch (...)
                {
                }
            }
            const std::size_t length = std::min(why.size(), sizeof(g_why) - 1);
            std::memcpy(g_why, why.data(), length);
            g_why[length] = 0;
            std::abort();
        }

        void Throws()
        {
            throw std::logic_error("thrown through a noexcept function, as a check");
        }

        void Refuses() noexcept
        {
            Throws();
        }

        int Deeper(int depth)
        {
            volatile char pad[4096];
            pad[0] = static_cast<char>(depth);
            if (depth >= 0)
                return Deeper(depth + 1) + pad[0];
            return pad[0];
        }
    }

    void Install(const std::filesystem::path& settings, const std::filesystem::path& log)
    {
        g_settings = settings;
        g_log = log;

        void* warm[4];
        Backtrace(warm, 4);

        stack_t alternate{};
        alternate.ss_sp = g_altStack;
        alternate.ss_size = sizeof(g_altStack);
        ::sigaltstack(&alternate, nullptr);

        struct sigaction action{};
        action.sa_sigaction = OnSignal;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
        sigemptyset(&action.sa_mask);
        for (const int signal : kSignals)
            ::sigaction(signal, &action, nullptr);
        std::set_terminate(OnTerminate);

        CrashReport::Prune(settings);
    }

    void CrashOnPurpose(std::wstring_view how)
    {
        if (how == L"abort")
            std::abort();
        if (how == L"terminate")
            std::terminate();
        if (how == L"throw")
            throw std::runtime_error("a crash on purpose, as a check");
        if (how == L"noexcept")
            Refuses();
        if (how == L"stack")
        {
            Deeper(0);
            return;
        }
        if (how == L"thread")
        {
            std::thread([]()
                {
                    volatile int* nowhere = nullptr;
                    *nowhere = 1;
                }).join();
            return;
        }
        volatile int* nowhere = nullptr;
        *nowhere = 1;
    }
}
