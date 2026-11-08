#include "App/Application.h"
#include "App/Desktop.h"
#include "App/Splash.h"
#include "Core/Cards.h"
#include "Core/RuntimeLog.h"
#include "Core/Settings.h"
#include "Steam/Worker.h"
#include "UI/ScreenMain.h"
#include "UI/TerrariumView.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <stdlib.h>
#include <string>
#include <wchar.h>

namespace
{
    constexpr const wchar_t* kInstanceName = Idler::kCardFarmEnabled ? L"Local\\ManifoldIdlerAlt" : L"Local\\ManifoldIdler";

    std::wstring Argument(const wchar_t* commandLine, const wchar_t* name)
    {
        const wchar_t* found = commandLine ? ::wcsstr(commandLine, name) : nullptr;
        if (!found)
            return {};
        found += ::wcslen(name);
        std::wstring value;
        if (*found == L'"')
        {
            for (++found; *found && *found != L'"'; ++found)
                value.push_back(*found);
        }
        else
        {
            for (; *found && *found != L' '; ++found)
                value.push_back(*found);
        }
        return value;
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR commandLine, _In_ int showCommand)
{
    if (Idler::Worker::IsWorkerCommand(commandLine))
        return Idler::Worker::Run(commandLine);

    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);

    const std::wstring settings = Argument(commandLine, L"--settings=");
    const std::wstring capture = Argument(commandLine, L"--capture=");

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
    const int takenOver = settings.empty() ? Idler::TakeOverOldName(settingsFile) : 0;
    if (capture.empty() || !settings.empty())
        Idler::RuntimeLog::OpenFile(std::filesystem::path(settingsFile).replace_extension(L".log"));
    if (takenOver > 0)
        LOG_INFO("Took over " + std::to_string(takenOver) + " files of the old name, Steam-Idler");

    Idler::Application app(settingsFile);

    if (const std::wstring scale = Argument(commandLine, L"--scale="); !scale.empty())
    {
        std::wstring number = scale;
        for (wchar_t& c : number)
            c = c == L',' ? L'.' : c;
        const double value = ::_wtof(number.c_str());
        if (value >= 0.5 && value <= 4.0)
            app.SetScaleOverride(static_cast<float>(value));
    }
    const bool minimised = commandLine && ::wcsstr(commandLine, L"--minimised");
    if (minimised)
        app.SetStartMinimised(true);
    if (!minimised && capture.empty() && (settings.empty() || ::wcsstr(commandLine, L"--splash")))
        app.SetSplash(true);
    if (!capture.empty())
        app.SetCapture(capture);
    if (!capture.empty() || !settings.empty())
        app.SetOpenInBackground(true);
    if (!settings.empty() && ::wcsstr(commandLine, L"--pretend-running"))
        app.Model().SetPretendRunning(true);
    if (!settings.empty() && ::wcsstr(commandLine, L"--pretend-login"))
        app.Model().Cards().SetPretendSignedIn();
    if (!settings.empty() && ::wcsstr(commandLine, L"--all-creatures"))
        app.Model().SetAllCreaturesFound();
    if (!settings.empty() && ::wcsstr(commandLine, L"--pretend-messages"))
        app.Model().SetPretendMessages(true);
    if (const std::wstring date = Argument(commandLine, L"--pretend-date="); !settings.empty() && date.size() == 5)
        app.Model().SetPretendDate(::_wtoi(date.substr(0, 2).c_str()), ::_wtoi(date.substr(3, 2).c_str()));
    if (const std::wstring fake = Argument(commandLine, L"--fake-shutdown="); !fake.empty())
        Idler::Desktop::SetFakeShutdownFile(fake);
    if (const std::wstring page = Argument(commandLine, L"--page="); !page.empty())
        Idler::UI::SetStartPage(page, static_cast<std::uint32_t>(::_wtoi(Argument(commandLine, L"--select=").c_str())));
    if (!settings.empty() && ::wcsstr(commandLine, L"--web-preview"))
        Idler::UI::SetWebPreview();
    if (const std::wstring also = Argument(commandLine, L"--select-also="); !settings.empty() && !also.empty())
        Idler::UI::SetStartSelection(also);
    if (!settings.empty() && ::wcsstr(commandLine, L"--pretend-running") && ::wcsstr(commandLine, L"--achievements-preview"))
        Idler::UI::SetAchievementsPreview();
    if (const std::wstring tab = Argument(commandLine, L"--game-window="); !settings.empty() && !tab.empty())
        Idler::UI::SetGameWindowPreview(tab);
    if (const std::wstring creature = Argument(commandLine, L"--creature="); !settings.empty() && !creature.empty())
        Idler::UI::SetStartCreature(creature);
    if (const std::wstring game = Argument(commandLine, L"--mini-game="); !settings.empty() && !game.empty())
        Idler::UI::SetMiniGamePreview(game);
    if (const std::wstring light = Argument(commandLine, L"--home-light="); !settings.empty() && !light.empty())
        Idler::UI::SetHomeLightPreview(light == L"night" ? 0.0f : light == L"dawn" ? 0.55f : light == L"dusk" ? 0.45f : 1.0f);

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
