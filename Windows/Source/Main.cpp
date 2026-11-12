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
    const int takenOver = settings.empty() ? Idler::TakeOverOldName(settingsFile) : 0;
    if (capture.empty() || !settings.empty())
        Idler::RuntimeLog::OpenFile(std::filesystem::path(settingsFile).replace_extension(L".log"));
    if (takenOver > 0)
        LOG_INFO("Took over " + std::to_string(takenOver) + " files of the old name, Steam-Idler");

    Idler::Application app(settingsFile);

    Idler::StartOptions::Apply(app, commandLine, settings, capture);

    if (!app.Initialise(instance, showCommand))
    {
        Idler::Splash::Close();
        ::MessageBoxW(nullptr, app.LastError(), L"Manifold Idler could not be started", MB_ICONERROR | MB_OK);
        return 1;
    }
    const int result = app.Run();
    if (single)
        ::CloseHandle(single);
    return result;
}
