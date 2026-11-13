#include "App/CrashHandler.h"
#include "App/Desktop.h"
#include "App/SdlApplication.h"
#include "App/StartOptions.h"
#include "Core/Errors.h"
#include "Core/FileLock.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Steam/Worker.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    bool IsWorkerArgument(std::string_view argument)
    {
        for (const std::string_view prefix : { "--idle=", "--scan", "--achievements=", "--change-achievements=" })
        {
            if (argument.substr(0, prefix.size()) == prefix)
                return true;
        }
        return false;
    }

    std::wstring CommandLine(int argc, char** argv)
    {
#ifdef _WIN32
        static_cast<void>(argc);
        static_cast<void>(argv);
        return ::GetCommandLineW();
#else
        std::wstring line;
        for (int index = 1; index < argc; ++index)
        {
            const std::wstring word = Idler::Utf8ToWide(argv[index]);
            if (!line.empty())
                line.push_back(L' ');
            const std::size_t equals = word.find(L'=');
            if (word.find(L' ') != std::wstring::npos && equals != std::wstring::npos)
                line += word.substr(0, equals + 1) + L"\"" + word.substr(equals + 1) + L"\"";
            else
                line += word;
        }
        return line;
#endif
    }

    void StartAgain(int argc, char** argv)
    {
#ifdef _WIN32
        static_cast<void>(argc);
        static_cast<void>(argv);
        std::wstring program(MAX_PATH, L'\0');
        const DWORD length = ::GetModuleFileNameW(nullptr, program.data(), static_cast<DWORD>(program.size()));
        if (length == 0 || length >= program.size())
            return;
        program.resize(length);
        std::wstring line = ::GetCommandLineW();
        for (std::size_t at = line.find(L" --minimised"); at != std::wstring::npos; at = line.find(L" --minimised"))
            line.erase(at, 12);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (::CreateProcessW(program.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
        {
            ::CloseHandle(process.hThread);
            ::CloseHandle(process.hProcess);
        }
#else
        std::vector<char*> arguments;
        for (int index = 0; index < argc; ++index)
        {
            if (index > 0 && std::string_view(argv[index]) == "--minimised")
                continue;
            arguments.push_back(argv[index]);
        }
        arguments.push_back(nullptr);
        ::execv("/proc/self/exe", arguments.data());
#endif
    }

    std::filesystem::path PathOf(const std::wstring& text)
    {
#ifdef _WIN32
        return std::filesystem::path(text);
#else
        return std::filesystem::path(Idler::WideToUtf8(text));
#endif
    }
}

int main(int argc, char** argv)
{
    if (argc >= 2 && IsWorkerArgument(argv[1]))
    {
#ifdef _WIN32
        const std::wstring command = ::GetCommandLineW();
#else
        std::string joined;
        for (int index = 1; index < argc; ++index)
            joined += (index > 1 ? " " : "") + std::string(argv[index]);
        const std::wstring command = Idler::Utf8ToWide(joined);
#endif
        return Idler::Worker::Run(command.c_str());
    }

    const std::wstring commandLine = CommandLine(argc, argv);
    const std::wstring settings = Idler::StartOptions::Argument(commandLine.c_str(), L"--settings=");
    const std::wstring capture = Idler::StartOptions::Argument(commandLine.c_str(), L"--capture=");

    const std::filesystem::path settingsFile = settings.empty() ? Idler::Desktop::SettingsPath() : PathOf(settings);
    Idler::FileLock settingsLock;
    if (capture.empty() && !settingsLock.Take(Idler::SettingsLockFile(settingsFile)))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Manifold Idler runs already", Idler::ErrorMessage(Idler::ErrorCode::SettingsInUse).c_str(), nullptr);
        return 1;
    }
    const std::filesystem::path logFile = capture.empty() || !settings.empty() ? std::filesystem::path(settingsFile).replace_extension(".log") : std::filesystem::path();
    if (!logFile.empty())
        Idler::RuntimeLog::OpenFile(logFile);
    Idler::CrashHandler::Install(settingsFile, logFile);

    int result = 0;
    bool restart = false;
    {
        Idler::SdlApplication app(settingsFile);
        Idler::StartOptions::Apply(app, commandLine.c_str(), settings, capture);
        if (!capture.empty())
            app.SetCapture(PathOf(capture));

        if (!app.Initialise())
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Manifold Idler could not be started", app.LastError().c_str(), nullptr);
            return 1;
        }
        result = app.Run();
        restart = app.RestartRequested();
    }
    Idler::RuntimeLog::OpenFile({});
    settingsLock.Release();
    if (restart)
        StartAgain(argc, argv);
    return result;
}
