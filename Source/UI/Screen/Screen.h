#pragma once

#include "App/CrashHandler.h"
#include "App/IdlerModel.h"
#include "App/Shell.h"
#include "App/Desktop.h"
#include "App/WebPanel.h"
#include "Core/Errors.h"
#include "Core/Releases.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Core/Version.h"
#include "Core/WebPolicy.h"
#include "Generated/IconsFontAwesome5.h"
#include "UI/Covers.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/MenuWidgets.h"
#include "UI/MiniGameView.h"
#include "UI/TerrariumView.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <deque>
#include <filesystem>
#include <optional>
#include <system_error>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Idler::UI::Screen
{
    enum class Page
    {
        Idling,
        Library,
        Cards,
        Statistics,
        Creatures,
        Log,
        Settings
    };

    extern bool g_logTabShown;
    extern bool g_crashLogShown;
    extern ImVec2 g_statusCorner;
    extern std::wstring g_crashTest;

    constexpr int kChartDays = 30;

    extern Page g_page;
    extern std::uint32_t g_selectedEntry;
    extern std::uint32_t g_selectedGame;
    extern std::unordered_set<std::uint32_t> g_gameSelection;

    extern bool g_webWantsOpen;
    extern bool g_webPreview;
    extern bool g_webDialogOpen;

    extern std::vector<std::uint32_t> g_selection;

    struct ListEditor
    {
        bool WantsOpen = false;
        bool IsNew = false;
        char Name[64] = "";
        std::vector<std::uint32_t> Apps;
        std::string OpenedName;
        std::vector<std::uint32_t> OpenedApps;
        char Search[128] = "";
        LibraryFilter Filter = LibraryFilter::All;
        std::vector<int> View;
        bool ViewStale = true;
        std::uint64_t ViewGeneration = 0;
        int LastRow = -1;
        std::vector<SteamCollection> Collections;
    };
    extern ListEditor g_editor;
    constexpr float kDetailColumnWidth = 484.0f;

    enum class GameTab
    {
        Overview,
        Statistics,
        Achievements,
        GameStats
    };

    struct GameWindowState
    {
        bool WantsOpen = false;
        std::uint32_t App = 0;
        GameTab Tab = GameTab::Overview;
        bool ReadAtStart = false;
        std::string EditingStat;
        char StatText[64] = {};
        bool FocusStat = false;
        double StatsAt = -1.0;
        bool HasFigures = false;
        GameFigures Figures;
        double AllSeconds = 0.0;
        std::vector<double> Days;
    };
    extern GameWindowState g_gameWindow;

    constexpr double kPageFadeSeconds = 0.14;
    extern Page g_shownPage;
    extern double g_pageShownAt;

    struct CoverGridShape
    {
        int Columns;
        float Width;
    };

    enum class GameAction
    {
        None,
        Details,
        Achievements,
        IdleNow,
        AddToQueue,
        AddToList,
        RemoveFromQueue,
        MoveToTop,
        ShowToFriends,
        TryAgain,
        StorePage,
        OpenInSteam
    };

    struct GameRequest
    {
        GameAction Action = GameAction::None;
        std::uint32_t App = 0;
        int List = -1;
    };

    template <typename Choice>
    bool ChoiceCombo(const char* id, Choice& value, Choice count, const char* (*name)(Choice), float width)
    {
        bool changed = false;
        ImGui::SetNextItemWidth(width);
        if (ImGui::BeginCombo(id, name(value)))
        {
            for (int index = 0; index < static_cast<int>(count); ++index)
            {
                const auto choice = static_cast<Choice>(index);
                if (ImGui::Selectable(name(choice), choice == value))
                {
                    value = choice;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    extern CreatureId g_creature;
    constexpr ImVec4 kBondColor = ManifoldTheme::Bond;

    bool PageShown(Page page);
    void OpenGameWindow(std::uint32_t appId, GameTab tab = GameTab::Overview);
    void Notice(const char* id, const char* icon, ImVec4 color, std::string_view text);
    void EmptyState(const char* id, std::uint32_t seed, std::string_view text);
    bool MainButton(const char* label, float width);
    void QueueButton(IdlerModel& model, float width, float height, float textScale = 1.0f);
    void QueueByKey(IdlerModel& model);
    std::string GameName(const IdlerModel& model, std::uint32_t appId);
    std::string ClockText(double seconds);
    std::string SteamLabel(const IdlerModel& model);
    void Sidebar(Shell& app);
    bool BeginPage(Shell& app, std::string_view status, ImVec4 statusColor = ManifoldTheme::Muted, bool queueButton = true, const char* statusTip = nullptr);
    std::pair<std::string, ImVec4> EntryDetail(const QueueEntry& entry, RunState state, double restSeconds);
    CoverGridShape CoverGridOf(float space, float gap);
    void ClearSelection();
    void FocusOnCtrlF(bool inDialog = false);
    void OpenListEditor(const IdlerModel& model, bool isNew);
    bool HasAchievements(const Game* game);
    void GameMenuItems(const IdlerModel& model, std::uint32_t appId, bool queuePage, GameRequest& request);
    void RunGameRequest(IdlerModel& model, const GameRequest& request);
    void GameButtons(IdlerModel& model, std::uint32_t appId, bool ways);
    void GoalControls(const char* id, Goal& goal, bool& changed, bool mixedKind = false, bool mixedHours = false);
    void QueueEntryRows(IdlerModel& model, const QueueEntry& entry, const Game* game, bool withCreature = true);
    void IdlingPage(Shell& app);
    std::string GameDetail(const Game& game);
    void GameFactRows(IdlerModel& model, const Game& game);
    void LibraryPage(Shell& app);
    void ListEditorDialog(IdlerModel& model);
    void WebDialog(Shell& app);
    void ShutdownDialog(IdlerModel& model);
    void CardsPage(Shell& app);
    void OpenStatsDay(std::int32_t day);
    bool StatsPill(const char* label, bool active);
    void GameStatsRows(const GameFigures& game, double allSeconds, const std::vector<double>& days, std::int32_t today, float below,
        const std::function<void(std::int32_t)>& clickedDay);
    void GameWindowDialog(IdlerModel& model);
    void StatisticsPage(Shell& app);
    ImVec4 RarityColor(Rarity rarity);
    void CreaturesPage(Shell& app);
    void DrawUndoToast(IdlerModel& model);
    void DrawToasts(IdlerModel& model);
    void LogPage(Shell& app);
    void SettingsPage(Shell& app);
}
