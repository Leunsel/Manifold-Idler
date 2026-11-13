#include "App/Application.h"
#include "App/Desktop.h"
#include "App/Splash.h"
#include "App/StartOptions.h"
#include "Core/Cards.h"
#include "Core/Errors.h"
#include "Core/FileLock.h"
#include "Core/RuntimeLog.h"
#include "Core/Settings.h"
#include "Core/Text.h"
#include "Steam/Worker.h"

#include <windows.h>

#include <filesystem>
#include <string>

namespace
{
    constexpr const wchar_t* kInstanceName = Idler::kCardFarmEnabled ? L"Local\\ManifoldIdlerAlt" : L"Local\\ManifoldIdler";

    void StartAgain()
    {
        std::wstring program(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(nullptr, program.data(), static_cast<DWORD>(program.size()));
            if (length == 0)
                return;
            if (length < program.size())
            {
                program.resize(length);
                break;
            }
            program.resize(program.size() * 2);
        }

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
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR commandLine, _In_ int showCommand)
{
    if (Idler::Worker::IsWorkerCommand(commandLine))
        return Idler::Worker::Run(commandLine);

    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);

    const std::wstring settings = Idler::StartOptions::Argument(commandLine, L"--settings=");
    const std::wstring capture = Idler::StartOptions::Argument(commandLine, L"--capture=");

    HANDLE single = nullptr;
    if (settings.empty() && capture.empty())
    {
        single = ::CreateMutexW(nullptr, TRUE, kInstanceName);
        if (::GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (const HWND window = ::FindWindowW(Idler::Application::WindowClass(), nullptr))
            {
                DWORD process = 0;
                ::GetWindowThreadProcessId(window, &process);
                ::AllowSetForegroundWindow(process);
                ::PostMessageW(window, Idler::Application::ShowWindowMessage(), 0, 0);
            }
            return 0;
        }
    }

    const std::filesystem::path settingsFile = settings.empty() ? Idler::Desktop::SettingsPath() : std::filesystem::path(settings);
    Idler::FileLock settingsLock;
    if (capture.empty() && !settingsLock.Take(Idler::SettingsLockFile(settingsFile)))
    {
        const std::wstring text = Idler::Utf8ToWide(Idler::ErrorMessage(Idler::ErrorCode::SettingsInUse));
        ::MessageBoxW(nullptr, text.c_str(), L"Manifold Idler runs already", MB_ICONINFORMATION | MB_OK);
        if (single)
            ::CloseHandle(single);
        return 1;
    }
    if (capture.empty() || !settings.empty())
        Idler::RuntimeLog::OpenFile(std::filesystem::path(settingsFile).replace_extension(L".log"));

    int result = 0;
    bool restart = false;
    {
        Idler::Application app(settingsFile);

        Idler::StartOptions::Apply(app, commandLine, settings, capture);

        if (!app.Initialise(instance, showCommand))
        {
            Idler::Splash::Close();
            ::MessageBoxW(nullptr, app.LastError(), L"Manifold Idler could not be started", MB_ICONERROR | MB_OK);
            return 1;
        }
        result = app.Run();
        restart = app.RestartRequested();
    }
    Idler::RuntimeLog::OpenFile({});
    settingsLock.Release();
    if (single)
        ::CloseHandle(single);
    if (restart)
        StartAgain();
    return result;
}
