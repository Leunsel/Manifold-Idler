#include "UI/ScreenMain.h"

#include "UI/Screen/Screen.h"

namespace Idler::UI
{
    using namespace Screen;

    void SetStartPage(std::wstring_view page, std::uint32_t selectedApp)
    {
        if (page == L"library")
            g_page = Page::Library;
        else if (page == L"cards" && PageShown(Page::Cards))
            g_page = Page::Cards;
        else if (page == L"statistics")
            g_page = Page::Statistics;
        else if (page == L"creatures")
            g_page = Page::Creatures;
        else if (page == L"log")
            g_page = Page::Log;
        else if (page == L"settings")
            g_page = Page::Settings;
        g_selectedEntry = selectedApp;
        g_selectedGame = selectedApp;
    }

    void SetCrashTest(std::wstring_view how)
    {
        g_crashTest = how;
    }

    void SetWebPreview()
    {
        g_webWantsOpen = true;
        g_webPreview = true;
    }

    void SetAchievementsPreview()
    {
        OpenGameWindow(g_selectedGame, GameTab::Achievements);
        g_gameWindow.ReadAtStart = true;
    }

    void SetStartCreature(std::wstring_view key)
    {
        std::string narrow;
        for (const wchar_t c : key)
            narrow.push_back(static_cast<char>(c));
        if (const auto id = CreatureWithKey(narrow))
            g_creature = *id;
    }

    void SetMiniGamePreview(std::wstring_view game)
    {
        g_page = Page::Creatures;
        const bool playing = game.ends_with(L":play");
        if (playing)
            game.remove_suffix(5);
        const MiniGame which = game == L"star" ? MiniGame::StarChase : game == L"pearl" ? MiniGame::PearlDive : game == L"shell" ? MiniGame::ShellGame : MiniGame::TreatCatch;
        OpenMiniGame(g_creature, which);
        if (playing)
            PlayMiniGameAhead(8.0f);
    }

    void SetGameWindowPreview(std::wstring_view tab)
    {
        const GameTab which = tab == L"statistics" ? GameTab::Statistics : tab == L"achievements" ? GameTab::Achievements : tab == L"stats" ? GameTab::GameStats : GameTab::Overview;
        const bool read = g_gameWindow.ReadAtStart;
        OpenGameWindow(g_selectedGame, which);
        g_gameWindow.ReadAtStart = read;
    }

    void SetStartSelection(std::wstring_view apps)
    {
        g_selection = { g_selectedEntry };
        g_gameSelection = { g_selectedGame };
        while (!apps.empty())
        {
            const auto comma = apps.find(L',');
            const std::wstring number(apps.substr(0, comma));
            if (const long appId = std::wcstol(number.c_str(), nullptr, 10); appId > 0)
            {
                g_selection.push_back(static_cast<std::uint32_t>(appId));
                g_gameSelection.insert(static_cast<std::uint32_t>(appId));
            }
            apps = comma == std::wstring_view::npos ? std::wstring_view{} : apps.substr(comma + 1);
        }
    }

    void DrawMain(Shell& app)
    {
        QueueByKey(app.Model());
        g_logTabShown = app.Model().Options().ShowLog || g_crashLogShown;
        if (!g_crashTest.empty())
            CrashHandler::CrashOnPurpose(std::exchange(g_crashTest, {}));
        if (!PageShown(g_page))
            g_page = Page::Settings;

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::SetNavCursorVisible(false);

        IdleAnimation::SetFound(app.Model().FoundCreatures());
        IdleAnimation::SetEvolved(app.Model().EvolvedCreatures());
        IdleAnimation::SetUpsideDown(app.Model().IsUpsideDownDay());

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::Begin("##idler", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        DrawWindowBackground(app.CornerRadius());

        Sidebar(app);
        ImGui::SameLine(0.0f, 0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(18.0f, 14.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (g_page != g_shownPage)
        {
            g_shownPage = g_page;
            g_pageShownAt = ImGui::GetTime();
        }
        const double pageAge = ImGui::GetTime() - g_pageShownAt;
        const bool pageFading = IdleAnimation::Enabled() && pageAge < kPageFadeSeconds;
        if (pageFading)
            IdleAnimation::RequestFrames();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pageFading ? 0.25f + 0.75f * static_cast<float>(pageAge / kPageFadeSeconds) : 1.0f);

        switch (g_page)
        {
        case Page::Idling: IdlingPage(app); break;
        case Page::Library: LibraryPage(app); break;
        case Page::Cards: CardsPage(app); break;
        case Page::Statistics: StatisticsPage(app); break;
        case Page::Creatures: CreaturesPage(app); break;
        case Page::Log: LogPage(app); break;
        case Page::Settings: SettingsPage(app); break;
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();

        GameWindowDialog(app.Model());
        DrawMiniGame(app.Model());
        ListEditorDialog(app.Model());
        WebDialog(app);
        ShutdownDialog(app.Model());
        ImGui::End();

        if (!g_webDialogOpen)
        {
            DrawUndoToast(app.Model());
            DrawToasts(app.Model());
        }
        DrawScrollShadows();
    }
}
