#include "Serve.h"

#include "App/IdlerModel.h"
#include "Commands.h"
#include "Core/RuntimeLog.h"
#include "Core/Settings.h"
#include "Core/Text.h"
#include "Ipc.h"
#include "Paths.h"
#include "Request.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <system_error>
#include <thread>

namespace Idler::Cli
{
    namespace
    {
        constexpr int kCommandWaitMilliseconds = 100;

        std::atomic<bool> g_quit{ false };
        std::atomic<bool> g_finished{ false };

#ifdef _WIN32
        BOOL WINAPI OnConsoleEvent(DWORD event)
        {
            g_quit = true;
            if (event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT || event == CTRL_SHUTDOWN_EVENT)
            {
                for (int waited = 0; waited < 450 && !g_finished; ++waited)
                    ::Sleep(10);
            }
            return TRUE;
        }

        void WatchForTheEnd()
        {
            ::SetConsoleCtrlHandler(OnConsoleEvent, TRUE);
        }

        bool WindowProgramRuns()
        {
            for (const wchar_t* name : { L"Local\\ManifoldIdler", L"Local\\ManifoldIdlerAlt" })
            {
                if (HANDLE mutex = ::OpenMutexW(SYNCHRONIZE, FALSE, name))
                {
                    ::CloseHandle(mutex);
                    return true;
                }
            }
            return false;
        }
#else
        void OnSignal(int)
        {
            g_quit = true;
        }

        void WatchForTheEnd()
        {
            struct sigaction action{};
            action.sa_handler = OnSignal;
            sigemptyset(&action.sa_mask);
            for (const int signal : { SIGINT, SIGTERM, SIGHUP })
                ::sigaction(signal, &action, nullptr);
        }

        bool WindowProgramRuns()
        {
            return false;
        }
#endif

        class LogEcho
        {
        public:
            void Print()
            {
                const std::uint64_t generation = RuntimeLog::Generation();
                if (generation == m_seen)
                    return;
                const std::size_t fresh = static_cast<std::size_t>(generation - m_seen);
                m_seen = generation;
                const std::vector<LogEntry> entries = RuntimeLog::Entries();
                const std::size_t first = entries.size() > fresh ? entries.size() - fresh : 0;
                for (std::size_t index = first; index < entries.size(); ++index)
                {
                    const LogEntry& entry = entries[index];
                    const bool quiet = entry.Level == LogLevel::Info || entry.Level == LogLevel::Success;
                    std::fprintf(quiet ? stdout : stderr, "%s  %s\n", entry.Time.c_str(), entry.Message.c_str());
                }
                std::fflush(stdout);
            }

        private:
            std::uint64_t m_seen = 0;
        };
    }

    int Serve(const ServeOptions& options)
    {
        const std::filesystem::path socketFile = SocketFile(options.Settings);

        if (!options.Pretend && WindowProgramRuns())
        {
            std::fprintf(stderr, "The window program runs: two idlers would idle the same queue twice. Close it first.\n");
            return 1;
        }
        std::string reply;
        if (Ask(socketFile, "ping", reply))
        {
            std::fprintf(stderr, "An idler already runs with these settings.\n");
            return 1;
        }

        std::error_code ignored;
        std::filesystem::create_directories(options.Settings.parent_path(), ignored);

        Listener listener;
        std::string error;
        if (!listener.Open(socketFile, error))
        {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }

        WatchForTheEnd();
        RuntimeLog::OpenFile(std::filesystem::path(options.Settings).replace_extension(L".log"));

        Settings settings;
        settings.Load(options.Settings);
        IdlerModel model(settings, options.Settings);
        if (options.Pretend)
            model.SetPretendRunning(true);

        LogEcho echo;
        bool startPending = options.Start;
        bool quit = false;
        std::printf("Settings: %s\nEnds with Ctrl+C or: idler quit\n", PathToUtf8(options.Settings).c_str());
        std::fflush(stdout);

        while (!quit && !g_quit)
        {
            model.Update();
            if (startPending && model.ScannedOnce())
            {
                startPending = false;
                if (HasWorkLeft(model.Queue()))
                    model.SetActive(true);
                else
                    LOG_WARN("Nothing in the queue can run");
            }
            echo.Print();
            listener.Serve(kCommandWaitMilliseconds, [&](const std::string& line)
                {
                    const CommandResult result = RunCommand(model, SplitRequest(line));
                    quit = quit || result.Quit;
                    return MakeReply(result.Ok, result.Text);
                });
        }

        model.Finish();
        echo.Print();
        g_finished = true;
        return 0;
    }
}
