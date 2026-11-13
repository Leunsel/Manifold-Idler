#pragma once

#include "App/CommandLine.h"
#include "App/Desktop.h"
#include "UI/ScreenMain.h"
#include "UI/TerrariumView.h"

#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <string>

namespace Idler::StartOptions
{
    using CommandLine::Argument;
    using CommandLine::Has;

    template <typename App>
    void Apply(App& app, const wchar_t* commandLine, const std::wstring& settings, const std::wstring& capture)
    {
        if (const std::wstring scale = Argument(commandLine, L"--scale="); !scale.empty())
        {
            std::wstring number = scale;
            for (wchar_t& c : number)
                c = c == L',' ? L'.' : c;
            const double value = std::wcstod(number.c_str(), nullptr);
            if (value >= 0.5 && value <= 4.0)
                app.SetScaleOverride(static_cast<float>(value));
        }
        const bool minimised = Has(commandLine, L"--minimised");
        if (minimised)
            app.SetStartMinimised(true);
        if (!minimised && capture.empty() && (settings.empty() || Has(commandLine, L"--splash")))
            app.SetSplash(true);
        if (!capture.empty())
            app.SetCapture(capture);
        if (!capture.empty() || !settings.empty())
            app.SetOpenInBackground(true);
        if (!settings.empty() && Has(commandLine, L"--pretend-running"))
            app.Model().SetPretendRunning(true);
        if (!settings.empty() && Has(commandLine, L"--pretend-login"))
            app.Model().Cards().SetPretendSignedIn();
        if (!settings.empty() && Has(commandLine, L"--all-creatures"))
            app.Model().SetAllCreaturesFound();
        if (!settings.empty() && Has(commandLine, L"--pretend-messages"))
            app.Model().SetPretendMessages(true);
        if (const std::wstring date = Argument(commandLine, L"--pretend-date="); !settings.empty() && date.size() == 5)
            app.Model().SetPretendDate(static_cast<int>(std::wcstol(date.substr(0, 2).c_str(), nullptr, 10)), static_cast<int>(std::wcstol(date.substr(3, 2).c_str(), nullptr, 10)));
        if (const std::wstring fake = Argument(commandLine, L"--fake-shutdown="); !fake.empty())
            Desktop::SetFakeShutdownFile(fake);
        if (const std::wstring page = Argument(commandLine, L"--page="); !page.empty())
            UI::SetStartPage(page, static_cast<std::uint32_t>(std::wcstol(Argument(commandLine, L"--select=").c_str(), nullptr, 10)));
        if (!settings.empty() && Has(commandLine, L"--web-preview"))
            UI::SetWebPreview();
        if (const std::wstring also = Argument(commandLine, L"--select-also="); !settings.empty() && !also.empty())
            UI::SetStartSelection(also);
        if (!settings.empty() && Has(commandLine, L"--pretend-running") && Has(commandLine, L"--achievements-preview"))
            UI::SetAchievementsPreview();
        if (const std::wstring tab = Argument(commandLine, L"--game-window="); !settings.empty() && !tab.empty())
            UI::SetGameWindowPreview(tab);
        if (const std::wstring creature = Argument(commandLine, L"--creature="); !settings.empty() && !creature.empty())
            UI::SetStartCreature(creature);
        if (const std::wstring game = Argument(commandLine, L"--mini-game="); !settings.empty() && !game.empty())
            UI::SetMiniGamePreview(game);
        if (const std::wstring crash = Argument(commandLine, L"--crash-test="); !settings.empty() && !crash.empty())
            UI::SetCrashTest(crash);
        if (const std::wstring light = Argument(commandLine, L"--home-light="); !settings.empty() && !light.empty())
            UI::SetHomeLightPreview(light == L"night" ? 0.0f : light == L"dawn" ? 0.55f : light == L"dusk" ? 0.45f : 1.0f);
    }
}
