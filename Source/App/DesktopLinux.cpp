#include "App/Desktop.h"

#include "Core/Text.h"

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace Idler::Desktop
{
    namespace
    {
        constexpr char kAutostartName[] = "manifold-idler.desktop";

        std::filesystem::path g_fakeShutdownFile;
        pid_t g_inhibitor = 0;

        std::filesystem::path Home()
        {
            if (const char* home = std::getenv("HOME"); home && home[0])
                return home;
            return ".";
        }

        std::filesystem::path ConfigFolder()
        {
            if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && config[0] == '/')
                return config;
            return Home() / ".config";
        }

        std::string ExecutablePath()
        {
            std::error_code ignored;
            return std::filesystem::read_symlink("/proc/self/exe", ignored).string();
        }

        pid_t Start(std::vector<std::string> words)
        {
            std::vector<char*> argv;
            for (std::string& word : words)
                argv.push_back(word.data());
            argv.push_back(nullptr);
            pid_t process = 0;
            if (::posix_spawnp(&process, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
                return 0;
            return process;
        }

        bool StartAndForget(std::vector<std::string> words)
        {
            const pid_t process = Start(std::move(words));
            if (process <= 0)
                return false;
            std::thread([process]()
                {
                    int ended = 0;
                    ::waitpid(process, &ended, 0);
                }).detach();
            return true;
        }

        void OpenLink(const std::string& link)
        {
            StartAndForget({ "steam", link });
        }

        SDL_Window* WindowOf(void* window)
        {
            return static_cast<SDL_Window*>(window);
        }

        std::string Quoted(const std::string& text)
        {
            std::string quoted = "\"";
            for (const char c : text)
            {
                if (c == '"' || c == '\\' || c == '`' || c == '$')
                    quoted.push_back('\\');
                quoted.push_back(c);
            }
            quoted.push_back('"');
            return quoted;
        }
    }

    std::filesystem::path SettingsPath()
    {
        const std::filesystem::path folder = ConfigFolder() / "manifold-idler";
        std::error_code ignored;
        std::filesystem::create_directories(folder, ignored);
        return folder / "Manifold-Idler.ini";
    }

    void OpenSteam()
    {
        OpenLink("steam://open/main");
    }

    void OpenStorePage(std::uint32_t appId)
    {
        OpenLink("steam://store/" + std::to_string(appId));
    }

    void OpenInLibrary(std::uint32_t appId)
    {
        OpenLink("steam://nav/games/details/" + std::to_string(appId));
    }

    void SetSteamStatus(const wchar_t* status)
    {
        OpenLink("steam://friends/status/" + WideToUtf8(status));
    }

    bool KnowsInputIdle()
    {
        return false;
    }

    double SecondsSinceInput()
    {
        return 1.0e9;
    }

    void KeepAwake(bool keep)
    {
        if (keep && g_inhibitor <= 0)
        {
            g_inhibitor = Start({ "systemd-inhibit", "--what=sleep", "--who=Manifold Idler", "--why=Games are idling", "--mode=block",
                "tail", "--pid=" + std::to_string(::getpid()), "-f", "/dev/null" });
        }
        else if (!keep && g_inhibitor > 0)
        {
            ::kill(g_inhibitor, SIGTERM);
            int ended = 0;
            ::waitpid(g_inhibitor, &ended, 0);
            g_inhibitor = 0;
        }
    }

    void SetFakeShutdownFile(std::filesystem::path file)
    {
        g_fakeShutdownFile = std::move(file);
        std::ofstream marker(g_fakeShutdownFile, std::ios::binary | std::ios::trunc);
        marker << "dry run on\n";
    }

    bool ShutDown()
    {
        if (!g_fakeShutdownFile.empty())
        {
            std::ofstream file(g_fakeShutdownFile, std::ios::binary | std::ios::trunc);
            file << "shutdown, systemctl poweroff\n";
            return true;
        }
        return StartAndForget({ "systemctl", "poweroff" });
    }

    const char* StartAtLoginLabel()
    {
        return "Start on login";
    }

    const char* StartAtLoginNote()
    {
        return "Starts minimised when you log in.";
    }

    bool StartsAtLogin()
    {
        std::error_code error;
        return std::filesystem::is_regular_file(ConfigFolder() / "autostart" / kAutostartName, error);
    }

    bool SetStartsAtLogin(bool start)
    {
        const std::filesystem::path folder = ConfigFolder() / "autostart";
        const std::filesystem::path file = folder / kAutostartName;
        std::error_code error;
        if (!start)
        {
            std::filesystem::remove(file, error);
            return !error;
        }
        std::filesystem::create_directories(folder, error);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Manifold Idler\n"
            << "Exec=" << Quoted(ExecutablePath()) << " --minimised\n"
            << "Terminal=false\n"
            << "X-GNOME-Autostart-enabled=true\n";
        return static_cast<bool>(out);
    }

    bool CanMovePointer()
    {
        const char* driver = SDL_GetCurrentVideoDriver();
        return driver && std::strcmp(driver, "wayland") != 0;
    }

    bool PointerIn(void* window, float& x, float& y)
    {
        SDL_Window* sdl = WindowOf(window);
        if (!sdl)
            return false;
        float globalX = 0.0f;
        float globalY = 0.0f;
        int left = 0;
        int top = 0;
        SDL_GetGlobalMouseState(&globalX, &globalY);
        if (!SDL_GetWindowPosition(sdl, &left, &top))
            return false;
        x = globalX - static_cast<float>(left);
        y = globalY - static_cast<float>(top);
        return true;
    }

    void MovePointer(void* window, float x, float y)
    {
        if (SDL_Window* sdl = WindowOf(window); sdl && CanMovePointer())
            SDL_WarpMouseInWindow(sdl, x, y);
    }

    bool IsInFront(void* window)
    {
        SDL_Window* sdl = WindowOf(window);
        return sdl && (SDL_GetWindowFlags(sdl) & SDL_WINDOW_INPUT_FOCUS) != 0;
    }
}
