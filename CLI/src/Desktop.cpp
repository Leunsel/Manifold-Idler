#include "App/Desktop.h"

#include "Core/Clock.h"
#include "Core/Text.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <vector>

extern char** environ;
#endif

namespace Idler::Desktop
{
    void SetSteamStatus(const wchar_t* status)
    {
        const std::wstring link = std::wstring(L"steam://friends/status/") + status;
#ifdef _WIN32
        ::ShellExecuteW(nullptr, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        std::string program = "steam";
        std::string argument = WideToUtf8(link);
        std::vector<char*> argv{ program.data(), argument.data(), nullptr };
        pid_t process = 0;
        if (::posix_spawnp(&process, program.c_str(), nullptr, nullptr, argv.data(), environ) == 0)
        {
            std::thread([process]()
                {
                    int ended = 0;
                    ::waitpid(process, &ended, 0);
                }).detach();
        }
#endif
    }

    double SecondsSinceInput()
    {
#ifdef _WIN32
        LASTINPUTINFO info{ sizeof(info), 0 };
        if (!::GetLastInputInfo(&info))
            return 0.0;
        return static_cast<double>(static_cast<DWORD>(Clock::TickMilliseconds()) - info.dwTime) / 1000.0;
#else
        return 1.0e9;
#endif
    }

    void KeepAwake(bool keep)
    {
#ifdef _WIN32
        ::SetThreadExecutionState(keep ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
#else
        static_cast<void>(keep);
#endif
    }

    bool ShutDown()
    {
        return false;
    }
}
