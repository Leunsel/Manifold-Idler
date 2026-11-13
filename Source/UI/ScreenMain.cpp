#include "UI/ScreenMain.h"

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

namespace Idler::UI
{
    namespace
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

        struct PageEntry
        {
            Page Id;
            const char* Icon;
            const char* Label;
        };

        constexpr PageEntry kPages[] = {
            { Page::Idling, ICON_FA_PLAY, "Idling" },
            { Page::Library, ICON_FA_GAMEPAD, "Library" },
            { Page::Cards, ICON_FA_CLONE, "Cards" },
            { Page::Statistics, ICON_FA_CHART_BAR, "Statistics" },
            { Page::Creatures, ICON_FA_PAW, "Creatures" },
            { Page::Log, ICON_FA_TERMINAL, "Log" },
            { Page::Settings, ICON_FA_COG, "Settings" },
        };

        constexpr const char* kCardIcon = ICON_FA_CLONE;

        bool g_logTabShown = false;
        bool g_crashLogShown = false;
        std::wstring g_crashTest;

        bool PageShown(Page page)
        {
            if (page == Page::Log)
                return g_logTabShown;
            return kCardFarmEnabled || page != Page::Cards;
        }

        bool GoalShown(GoalKind kind)
        {
            return kCardFarmEnabled || kind != GoalKind::CardDrops;
        }

        struct RatingChoice
        {
            std::uint32_t Percent;
            const char* Label;
        };

        constexpr RatingChoice kRatings[] = {
            { 0, "Any rating" },
            { 70, "70 %+" },
            { 80, "80 %+" },
            { 90, "90 %+" },
        };

        constexpr int kChartDays = 30;

        Page g_page = Page::Idling;
        std::uint32_t g_selectedEntry = 0;
        float g_selectedRowsHeight = 0.0f;
        std::uint32_t g_selectedGame = 0;
        std::unordered_set<std::uint32_t> g_gameSelection;
        std::uint32_t g_gameAnchor = 0;

        char g_search[128] = "";
        LibraryFilter g_filter = LibraryFilter::All;
        LibrarySort g_sort = LibrarySort::Name;
        std::uint32_t g_minReview = 0;
        std::vector<int> g_view;
        std::uint64_t g_viewGeneration = ~0ull;
        bool g_viewStale = true;

        bool g_webWantsOpen = false;
        bool g_webPreview = false;
        bool g_webDialogOpen = false;

        std::vector<std::uint32_t> g_selection;
        std::uint32_t g_selectionAnchor = 0;

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
        ListEditor g_editor;

        enum class StatsTab
        {
            Overview,
            Games,
            History
        };
        StatsTab g_statsTab = StatsTab::Overview;
        std::optional<std::int32_t> g_statsDay;
        enum class StatsChart
        {
            Days30,
            Days90,
            Weekdays,
            Hours
        };
        StatsChart g_statsChart = StatsChart::Days30;
        GameOrder g_statsOrder = GameOrder::Time;
        std::uint32_t g_statsGame = 0;
        double g_statsRefreshAt = -1.0;
        constexpr int kCalendarDays = 378;
        std::vector<double> g_statsCalendar;
        std::vector<double> g_statsGameDays;
        std::vector<GameTotal> g_statsGames;
        std::vector<HistoryRow> g_statsHistory;
        std::vector<GameFigures> g_statsFigures;
        std::vector<GameFigures> g_statsTop;
        StatsSummary g_statsSummary;
        std::array<double, 24> g_statsHours{};
        double g_statsWeek = 0.0;
        double g_statsWeekBefore = 0.0;

        constexpr int kCardColumns = 5;
        constexpr const char* kProgramLabel = kCardFarmEnabled ? "Idler (test)" : "Idler";
        constexpr float kDetailColumnWidth = 484.0f;
        constexpr float kSettingsWidth = 1120.0f;
        constexpr int kCardFullWidth = 224;
        constexpr int kCardFullHeight = 261;
        constexpr int kCardThumbWidth = 112;
        constexpr int kCardThumbHeight = 130;
        constexpr double kFreshCardSeconds = 12.0 * 60.0 * 60.0;

        struct CardRow
        {
            std::uint32_t AppId = 0;
            std::uint32_t Drops = 0;
        };
        std::vector<CardRow> g_cardRows;
        std::uint64_t g_cardRowsGeneration = ~0ull;
        std::uint64_t g_cardRowsLibrary = ~0ull;

        std::vector<ReceivedCard> g_received;
        std::uint64_t g_receivedGeneration = ~0ull;
        std::uint64_t g_receivedAccount = 0;
        std::uint64_t g_picturesGeneration = 0;

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
        GameWindowState g_gameWindow;

        void OpenGameWindow(std::uint32_t appId, GameTab tab = GameTab::Overview)
        {
            g_gameWindow.WantsOpen = true;
            g_gameWindow.App = appId;
            g_gameWindow.Tab = tab;
            g_gameWindow.StatsAt = -1.0;
            g_gameWindow.EditingStat.clear();
        }

        bool g_shutdownDialogOpen = false;

        void Notice(const char* id, const char* icon, ImVec4 color, std::string_view text)
        {
            BeginRow(id);
            const float wrap = ImGui::GetCursorPosX() + RowWidth();
            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (icon)
            {
                ImGui::TextUnformatted(icon);
                ImGui::SameLine(0.0f, Px(8.0f));
            }
            ImGui::PushTextWrapPos(wrap);
            ImGui::TextUnformatted(text.data(), text.data() + text.size());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::EndGroup();
            ErrorTooltip(text);
            ImGui::PopID();
        }

        void EmptyState(const char* id, std::uint32_t seed, std::string_view text)
        {
            BeginRow(id);
            const float width = RowWidth();
            const float pixel = std::round(Px(3.0f));
            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, picture.y + Px(8.0f)));
            IdleAnimation::DrawCreature(ImGui::GetWindowDrawList(), ImVec2(std::round(start.x + (width - picture.x) * 0.5f), start.y + Px(6.0f)),
                IdleAnimation::CreatureOf(seed), IdleAnimation::Mood::Sleeping, seed, pixel);

            const float wrap = ImGui::GetCursorPosX() + width;
            const float textWidth = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
            if (textWidth < width)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::round((width - textWidth) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::PushTextWrapPos(wrap);
            ImGui::TextUnformatted(text.data(), text.data() + text.size());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        bool MainButton(const char* label, float width)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            const bool clicked = ImGui::Button(label, ImVec2(width, Px(30.0f)));
            ImGui::PopStyleColor(3);
            return clicked;
        }

        std::string QueueStateLabel(const IdlerModel& model)
        {
            switch (model.Phase())
            {
            case QueuePhase::Starting:
                if (model.WantedCount() > 1)
                    return std::string(ICON_FA_HOURGLASS_HALF "  Starting ") + std::to_string(model.ReadyCount()) + "/" + std::to_string(model.WantedCount());
                return ICON_FA_HOURGLASS_HALF "  Starting";
            case QueuePhase::Stopping:
                return std::string(ICON_FA_HOURGLASS_HALF "  Stopping ") + std::to_string(model.AliveCount());
            case QueuePhase::Running:
                return std::string(ICON_FA_PLAY "  Idling ") + std::to_string(model.RunningCount());
            case QueuePhase::Waiting:
                return ICON_FA_PAUSE "  Waiting";
            default:
                return ICON_FA_PLAY;
            }
        }

        const char* QueueActionLabel(const IdlerModel& model)
        {
            return model.Active() ? ICON_FA_STOP "  Stop" : ICON_FA_PLAY "  Start";
        }

        bool CanSwitchQueue(const IdlerModel& model)
        {
            return model.Phase() != QueuePhase::Stopping && (model.Active() || HasWorkLeft(model.Queue()));
        }

        struct QueueButtonLook
        {
            QueuePhase Phase = QueuePhase::Stopped;
            ImVec4 Color{ 0.0f, 0.0f, 0.0f, 0.0f };
            float Filled = 0.0f;
            float Alive = 0.0f;
            float Working = 0.0f;
            float Pointed = 0.0f;
            int StoppingFrom = 1;
        };
        QueueButtonLook g_queueLook;

        ImVec4 QueueColor(QueuePhase phase, bool can, bool pointed)
        {
            switch (phase)
            {
            case QueuePhase::Stopping:
                return ManifoldTheme::Error;
            case QueuePhase::Starting:
            case QueuePhase::Running:
                return pointed ? ManifoldTheme::Error : ManifoldTheme::Accent;
            case QueuePhase::Waiting:
                return pointed ? ManifoldTheme::Error : ManifoldTheme::Warning;
            default:
                return can ? ManifoldTheme::Accent : ManifoldTheme::Muted;
            }
        }

        ImVec4 Blend(const ImVec4& from, const ImVec4& to, float share)
        {
            return ImVec4(from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share, from.z + (to.z - from.z) * share, from.w + (to.w - from.w) * share);
        }

        void QueueButton(IdlerModel& model, float width, float height, float textScale = 1.0f)
        {
            const QueuePhase phase = model.Phase();
            const bool can = CanSwitchQueue(model);
            const bool working = phase == QueuePhase::Starting || phase == QueuePhase::Stopping;
            const bool alive = phase == QueuePhase::Running || phase == QueuePhase::Waiting;
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 max(pos.x + width, pos.y + height);

            const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, clear);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, clear);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, clear);
            ImGui::BeginDisabled(!can);
            const bool clicked = ImGui::Button("###queue_button", ImVec2(width, height));
            const bool pointed = can && (ImGui::IsItemHovered() || (ImGui::GetIO().NavVisible && ImGui::IsItemFocused()));
            const bool held = can && ImGui::IsItemActive();
            if (can)
                ItemTooltip("Also with Space.");
            else if (phase != QueuePhase::Stopping)
                ItemTooltip("Nothing in the queue can run.");
            ImGui::EndDisabled();
            ImGui::PopStyleColor(3);

            QueueButtonLook& look = g_queueLook;
            if (phase != look.Phase)
            {
                if (phase == QueuePhase::Starting)
                    look.Filled = 0.0f;
                else if (phase == QueuePhase::Stopping)
                    look.Filled = 1.0f;
                look.StoppingFrom = std::max(1, model.AliveCount());
                look.Phase = phase;
            }
            float filled = 1.0f;
            if (phase == QueuePhase::Starting && model.WantedCount() > 0)
                filled = static_cast<float>(model.ReadyCount()) / static_cast<float>(model.WantedCount());
            else if (phase == QueuePhase::Stopping)
                filled = static_cast<float>(model.AliveCount()) / static_cast<float>(look.StoppingFrom);
            const ImVec4 wanted = QueueColor(phase, can, pointed);
            if (look.Color.w == 0.0f)
                look.Color = wanted;
            const float dt = ImGui::GetIO().DeltaTime;
            const float step = 1.0f - std::exp(-dt * 14.0f);
            const float slow = 1.0f - std::exp(-dt * 5.0f);
            look.Color = Blend(look.Color, wanted, step);
            look.Filled += (filled - look.Filled) * step;
            look.Alive += ((alive ? 1.0f : 0.0f) - look.Alive) * slow;
            look.Working += ((working ? 1.0f : 0.0f) - look.Working) * slow;
            look.Pointed += ((pointed ? 1.0f : 0.0f) - look.Pointed) * step;
            if (working || std::abs((pointed ? 1.0f : 0.0f) - look.Pointed) > 0.004f || std::abs(filled - look.Filled) > 0.002f || std::abs((alive ? 1.0f : 0.0f) - look.Alive) > 0.004f || std::abs((working ? 1.0f : 0.0f) - look.Working) > 0.004f
                || std::abs(wanted.x - look.Color.x) + std::abs(wanted.y - look.Color.y) + std::abs(wanted.z - look.Color.z) > 0.004f)
                IdleAnimation::RequestFrames();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rounding = Px(5.0f);
            const auto tint = [&](float alpha) { return ImGui::GetColorU32(ManifoldTheme::WithAlpha(look.Color, alpha)); };

            const float calm = 1.0f - look.Alive;
            if (calm > 0.01f)
            {
                drawList->AddRectFilled(pos, max, tint(0.12f * calm), rounding);
                drawList->AddRect(pos, max, tint(0.62f * calm), rounding);
            }
            if (look.Alive > 0.01f)
                DrawIdleHighlight(drawList, pos, max, look.Alive * 1.5f, 0, look.Color);
            if (pointed)
                drawList->AddRectFilled(pos, max, tint(held ? 0.20f : 0.10f), rounding);

            if (look.Working > 0.01f)
            {
                const float split = std::round(pos.x + width * std::clamp(look.Filled, 0.0f, 1.0f));
                if (split > pos.x)
                {
                    drawList->PushClipRect(pos, ImVec2(split, max.y), true);
                    drawList->AddRectFilled(pos, max, tint(0.26f * look.Working), rounding);
                    drawList->PopClipRect();
                }
                if (working && IdleAnimation::Enabled())
                {
                    const float band = width * 0.22f;
                    const float x = pos.x - band + std::fmod(static_cast<float>(ImGui::GetTime()) * 0.75f, 1.0f) * (width + band * 2.0f);
                    DrawSheen(drawList, pos, max, rounding, x, band, ManifoldTheme::WithAlpha(look.Color, 0.30f));
                }
            }

            ImFont* font = Fonts::Bold();
            const float size = std::round(ImGui::GetFontSize() * textScale);
            const ImVec4 ink = Blend(look.Color, ManifoldTheme::Text, phase == QueuePhase::Stopped ? 0.0f : 0.7f);
            const auto drawLabel = [&](const char* label, float alpha)
                {
                    if (alpha < 0.01f)
                        return;
                    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
                    const ImVec2 textPos(std::round(pos.x + (width - textSize.x) * 0.5f), std::round(pos.y + (height - textSize.y) * 0.5f));
                    drawList->AddText(font, size, textPos, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ink, alpha)), label);
                };
            drawLabel(QueueStateLabel(model).c_str(), 1.0f - look.Pointed);
            drawLabel(QueueActionLabel(model), look.Pointed);

            if (clicked)
                model.SetActive(!model.Active());
        }

        float QueueButtonWidth()
        {
            return std::round(Fonts::Bold()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, ICON_FA_HOURGLASS_HALF "  Starting 32/32").x + Px(24.0f));
        }

        void QueueByKey(IdlerModel& model)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (!io.WantTextInput && !io.NavVisible && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyPressed(ImGuiKey_Space, false) && CanSwitchQueue(model))
                model.SetActive(!model.Active());
        }

        std::string GameName(const IdlerModel& model, std::uint32_t appId)
        {
            const Game* game = model.FindGame(appId);
            return game ? game->Name : "App " + std::to_string(appId);
        }

        std::string GoalText(const Goal& goal)
        {
            switch (goal.Kind)
            {
            case GoalKind::ForHours: return "For " + HoursText(goal.Hours);
            case GoalKind::UntilTotal: return "Until " + HoursText(goal.Hours) + " played";
            case GoalKind::CardDrops: return "Until the cards dropped";
            default: return "No limit";
            }
        }

        std::string ClockText(double seconds)
        {
            const std::time_t when = std::time(nullptr) + static_cast<std::time_t>(seconds);
            std::tm local{};
            if (!LocalTime(when, local))
                return {};
            char text[32];
            std::strftime(text, sizeof(text), "%a %H:%M", &local);
            return text;
        }

        std::string RefundText(const Game& game)
        {
            const auto now = static_cast<std::uint32_t>(std::time(nullptr));
            const double played = static_cast<double>(game.PlaytimeMinutes) * 60.0;
            if (!IsRefundable(game.Purchased, played, now))
                return {};
            return "Until " + DateText(game.Purchased + kRefundWindowSeconds) + ", " + DurationText(static_cast<std::uint64_t>(kRefundPlaySeconds - played)) + " left";
        }

        std::string SteamLabel(const IdlerModel& model)
        {
            const Steam::Status& steam = model.SteamStatus();
            if (!steam.Installed())
                return "Steam not found";
            if (!steam.Running())
                return "Steam is closed";
            if (!steam.LoggedOn())
                return "Not logged on";
            return model.Persona().empty() ? "Steam" : model.Persona();
        }

        constexpr int kLogoClicksWanted = 7;
        constexpr double kLogoClickSeconds = 1.5;
        int g_logoClicks = 0;
        double g_logoClickedAt = 0.0;

        constexpr double kPageFadeSeconds = 0.14;
        float g_pageMarker = -1.0f;

        bool IsWaiting(const IdlerModel& model)
        {
            return model.Active() && model.CurrentHold() != Hold::None;
        }

        bool IsIdling(const IdlerModel& model)
        {
            return !IsWaiting(model) && (model.Active() || model.RunningCount() > 0);
        }

        const char* PageIcon(const IdlerModel& model, const PageEntry& page)
        {
            if (page.Id != Page::Idling)
                return page.Icon;
            return IsIdling(model) ? ICON_FA_PLAY : ICON_FA_PAUSE;
        }
        Page g_shownPage = Page::Idling;
        double g_pageShownAt = -100.0;

        void CountLogoClick(IdlerModel& model)
        {
            const double now = ImGui::GetTime();
            g_logoClicks = now - g_logoClickedAt < kLogoClickSeconds ? g_logoClicks + 1 : 1;
            g_logoClickedAt = now;
            if (g_logoClicks >= kLogoClicksWanted)
                model.AwardCreature(CreatureId::Manifold);
        }

        void Sidebar(Shell& app)
        {
            IdlerModel& model = app.Model();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##sidebar", ImVec2(Px(kSidebarWidth), 0.0f), ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, Px(2.0f)));
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const float windowWidth = ImGui::GetWindowWidth();

            ImGui::Dummy(ImVec2(0.0f, Px(14.0f)));
            ImGui::SetCursorPosX(Px(20.0f));
            ImGui::TextColored(ManifoldTheme::Accent, "%s", ICON_FA_CUBE);
            const ImVec2 cubeMin = ImGui::GetItemRectMin();
            const ImVec2 cubeMax = ImGui::GetItemRectMax();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(cubeMin, cubeMax))
                CountLogoClick(model);
            ImGui::SameLine(0.0f, Px(10.0f));
            ImGui::TextUnformatted("Manifold");
            ImGui::SameLine(0.0f, Px(6.0f));
            ImGui::TextDisabled("%s", kProgramLabel);
            ImGui::Dummy(ImVec2(0.0f, Px(14.0f)));
            const float logoBottom = ImGui::GetCursorScreenPos().y;
            app.AddDragArea(windowPos.x, windowPos.y, windowPos.x + windowWidth, cubeMin.y);
            app.AddDragArea(windowPos.x, cubeMax.y, windowPos.x + windowWidth, logoBottom);
            app.AddDragArea(windowPos.x, cubeMin.y, cubeMin.x, cubeMax.y);
            app.AddDragArea(cubeMax.x, cubeMin.y, windowPos.x + windowWidth, cubeMax.y);

            {
                const ImVec2 first = ImGui::GetCursorScreenPos();
                const float itemHeight = Px(34.0f);
                const float itemStep = itemHeight + ImGui::GetStyle().ItemSpacing.y;
                int row = 0;
                int currentRow = 0;
                for (const PageEntry& entry : kPages)
                {
                    if (!PageShown(entry.Id))
                        continue;
                    if (entry.Id == g_page)
                        currentRow = row;
                    ++row;
                }
                const float target = itemStep * static_cast<float>(currentRow);
                if (g_pageMarker < 0.0f || !IdleAnimation::Enabled())
                    g_pageMarker = target;
                g_pageMarker += (target - g_pageMarker) * std::min(1.0f, ImGui::GetIO().DeltaTime * 18.0f);
                if (std::fabs(target - g_pageMarker) < 0.5f)
                    g_pageMarker = target;
                else
                    IdleAnimation::RequestFrames();

                const ImVec2 markerMin(first.x + Px(8.0f), std::round(first.y + g_pageMarker));
                const ImVec2 markerMax(first.x + windowWidth - Px(8.0f), markerMin.y + itemHeight);
                ImDrawList* markerList = ImGui::GetWindowDrawList();
                markerList->AddRectFilled(markerMin, markerMax, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.08f)), Px(4.0f));
                markerList->AddRectFilled(ImVec2(markerMin.x, markerMin.y + Px(7.0f)), ImVec2(markerMin.x + Px(3.0f), markerMax.y - Px(7.0f)), ImGui::GetColorU32(ManifoldTheme::Accent), Px(1.5f));
            }

            for (const PageEntry& entry : kPages)
            {
                if (!PageShown(entry.Id))
                    continue;
                if (SidebarItem(PageIcon(model, entry), entry.Label, g_page == entry.Id))
                    g_page = entry.Id;
                const bool waiting = entry.Id == Page::Idling && IsWaiting(model);
                if (waiting || (entry.Id == Page::Idling && IsIdling(model)) || (entry.Id == Page::Creatures && model.HasNewCreatures()))
                {
                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    const ImVec2 itemMax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(itemMax.x - Px(18.0f), (itemMin.y + itemMax.y) * 0.5f), Px(3.0f),
                        ImGui::GetColorU32(waiting ? ManifoldTheme::Warning : ManifoldTheme::Accent));
                }
            }

            const float footerY = ImGui::GetWindowHeight() - Px(52.0f);

            const float terrariumTop = ImGui::GetCursorPosY() + Px(10.0f);
            const float terrariumHeight = footerY - Px(10.0f) - terrariumTop;
            const bool homeShown = g_page == Page::Creatures && model.Options().CreatureHome;
            if (model.Options().ShowTerrarium && !homeShown && terrariumHeight >= Px(90.0f))
            {
                ImGui::SetCursorPos(ImVec2(Px(12.0f), terrariumTop));
                DrawTerrarium(model, ImVec2(std::round(windowWidth - Px(24.0f)), std::round(terrariumHeight)));
            }

            if (ImGui::GetCursorPosY() < footerY)
                ImGui::SetCursorPosY(footerY);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));

            const bool ready = model.SteamStatus().LoggedOn();
            const int running = model.RunningCount();
            const float footerHeight = Px(30.0f);
            const float reloadWidth = Px(30.0f);
            const float labelWidth = width - reloadWidth - Px(10.0f);
            const ImVec2 labelPos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##steam", ImVec2(labelWidth, footerHeight)))
                Desktop::OpenSteam();
            const ImVec2 badgeMin(labelPos.x + Px(8.0f), labelPos.y);
            const ImVec2 badgeMax(labelPos.x + labelWidth, labelPos.y + footerHeight);
            if (ImGui::IsItemHovered())
                drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(255, 255, 255, 9), Px(4.0f));
            const std::string badgeTip = running > 0 ? std::to_string(running) + (running == 1 ? " game idling. Opens Steam." : " games idling. Opens Steam.") : std::string("Opens Steam.");
            ItemTooltip(badgeTip.c_str());

            const float glow = Fade(ImGui::GetID("##badge_glow"), running > 0, 3.0f);
            if (glow > 0.0f)
            {
                DrawIdleHighlight(drawList, badgeMin, badgeMax, glow);
                IdleAnimation::RequestFrames();
            }
            const float labelY = labelPos.y + (footerHeight - ImGui::GetTextLineHeight()) * 0.5f;
            if (running > 0)
            {
                const ImVec2 at(labelPos.x + Px(12.0f), std::round(labelPos.y + (footerHeight - IdleAnimation::Height()) * 0.5f));
                if (const auto buddy = model.Buddy())
                    IdleAnimation::DrawCreature(drawList, at, *buddy, IdleAnimation::Mood::Idling, model.Account(), IdleAnimation::PixelSize());
                else
                    IdleAnimation::Draw(drawList, at, IdleAnimation::Mood::Idling, model.Account());
            }
            else if (const ImTextureID avatar = ready ? Covers::Avatar(model.SteamId()) : ImTextureID{})
            {
                const float side = std::round(Px(22.0f));
                const ImVec2 at(labelPos.x + Px(17.0f), std::round(labelPos.y + (footerHeight - side) * 0.5f));
                drawList->AddImageRounded(avatar, at, ImVec2(at.x + side, at.y + side), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f - glow)), Px(4.0f));
            }
            else
            {
                drawList->AddText(ImVec2(labelPos.x + Px(23.0f), labelY), ready ? IM_COL32(112, 116, 124, 255) : ImGui::GetColorU32(ManifoldTheme::Warning),
                    ready ? ICON_FA_USER : ICON_FA_EXCLAMATION_TRIANGLE);
            }
            const std::string label = SteamLabel(model);
            drawList->PushClipRect(labelPos, ImVec2(labelPos.x + labelWidth - Px(4.0f), labelPos.y + footerHeight), true);
            DrawCutText(drawList, ImVec2(labelPos.x + Px(50.0f), labelY), labelPos.x + labelWidth - Px(4.0f), !ready ? ManifoldTheme::Warning : running > 0 ? ManifoldTheme::Text : ManifoldTheme::Label, label);
            drawList->PopClipRect();

            ImGui::SameLine(0.0f, 0.0f);
            if (IconLink("##reload", ICON_FA_SYNC, ImVec2(reloadWidth, footerHeight), "Reads the library again."))
                model.Rescan();

            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        void BeginPage(Shell& app, std::string_view status, ImVec4 statusColor = ManifoldTheme::Muted, bool queueButton = true)
        {
            const PageEntry* page = &kPages[0];
            for (const PageEntry& entry : kPages)
            {
                if (entry.Id == g_page)
                    page = &entry;
            }

            const float button = ImGui::GetFrameHeight();
            const float gap = Px(2.0f);
            const float buttons = button * 3.0f + gap * 2.0f;
            const float headerHeight = Px(34.0f);
            const float queueHeight = Px(26.0f);
            const float queueWidth = queueButton ? QueueButtonWidth() : 0.0f;
            const float queueGap = queueButton ? Px(10.0f) : 0.0f;
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            PageHeader(PageIcon(app.Model(), *page), page->Label, status, statusColor, buttons + queueWidth + queueGap);
            const ImVec2 body = ImGui::GetCursorScreenPos();

            const float buttonsX = pos.x + width - Px(6.0f) - buttons;
            const float queueX = buttonsX - queueGap - queueWidth;
            if (queueButton)
            {
                ImGui::SetCursorScreenPos(ImVec2(queueX, pos.y + std::round((headerHeight - queueHeight) * 0.5f)));
                QueueButton(app.Model(), queueWidth, queueHeight);
            }

            ImGui::SetCursorScreenPos(ImVec2(buttonsX, pos.y + std::round((headerHeight - button) * 0.5f)));
            if (IconLink("##minimise", ICON_FA_MINUS, ImVec2(button, button), nullptr))
                app.Minimise();
            ImGui::SameLine(0.0f, gap);
            if (IconLink("##maximise", app.Maximised() ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE, ImVec2(button, button), nullptr))
                app.ToggleMaximised();
            ImGui::SameLine(0.0f, gap);
            if (IconLink("##close", ICON_FA_TIMES, ImVec2(button, button), nullptr, ManifoldTheme::Error))
                app.Close();
            ImGui::SetCursorScreenPos(body);

            const ImVec2 area = ImGui::GetWindowPos();
            app.AddDragArea(area.x, area.y, queueX, pos.y + headerHeight);
            app.AddDragArea(queueX, area.y, area.x + ImGui::GetWindowWidth(), pos.y);
            BeginPageBody();
        }

        std::pair<std::string, ImVec4> EntryDetail(const QueueEntry& entry, RunState state, double restSeconds)
        {
            if (entry.Failed)
                return { entry.Error, ManifoldTheme::Error };
            if (IsHeld(entry))
                return { "Held before the refund limit", ManifoldTheme::Warning };

            const auto idled = static_cast<std::uint64_t>(entry.IdledSeconds);
            if (entry.Goal.Kind == GoalKind::CardDrops)
            {
                if (entry.DropsLeft == 0)
                    return { "All cards dropped", ManifoldTheme::Muted };
                if (entry.DropsLeft < 0)
                    return { "Drops not known yet", ManifoldTheme::Muted };
                return { std::to_string(entry.DropsLeft) + (entry.DropsLeft == 1 ? " drop left" : " drops left"), ManifoldTheme::Muted };
            }
            if (IsFinished(entry))
                return { "Reached " + HoursText(static_cast<float>(GoalSeconds(entry) / 3600.0)), ManifoldTheme::Muted };
            if (restSeconds > 0.0)
            {
                const std::string left = DurationText(static_cast<std::uint64_t>(std::ceil(restSeconds)));
                return { idled > 0 ? "On a break, back in " + left : "Starts in " + left, ManifoldTheme::Muted };
            }
            if (entry.Goal.Kind == GoalKind::None)
                return { idled > 0 ? DurationText(idled) + " idled" : GoalText(entry.Goal), ManifoldTheme::Muted };

            if (state != RunState::Running && idled == 0)
                return { GoalText(entry.Goal), ManifoldTheme::Muted };
            std::string left = DurationText(static_cast<std::uint64_t>(std::ceil(SecondsLeft(entry)))) + " left";
            const auto hours = static_cast<float>(GoalSeconds(entry) / 3600.0);
            left += entry.Goal.Kind == GoalKind::ForHours ? " of " + HoursText(hours) : " until " + HoursText(hours) + " played";
            return { std::move(left), ManifoldTheme::Muted };
        }

        std::vector<RowTag> EntryTags(const QueueEntry& entry, RunState state)
        {
            if (entry.Failed)
                return { { "failed", ManifoldTheme::Error } };
            if (IsHeld(entry))
                return { { "refund", ManifoldTheme::Warning } };
            switch (state)
            {
            case RunState::Running: return { AnimatedTag("Idling", IdleAnimation::Mood::Idling, entry.AppId) };
            case RunState::Starting: return { AnimatedTag("Starting", IdleAnimation::Mood::Starting, entry.AppId) };
            case RunState::Stopping: return { { "stopping", ManifoldTheme::Muted } };
            default: break;
            }
            if (IsFinished(entry))
                return { { ICON_FA_CHECK, ManifoldTheme::Accent } };
            return {};
        }

        struct CoverGridShape
        {
            int Columns;
            float Width;
        };

        CoverGridShape CoverGridOf(float space, float gap)
        {
            const int columns = std::max(1, static_cast<int>((space + gap) / (Px(122.0f) + gap)));
            return { columns, std::floor((space - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns)) };
        }

        bool IsSelected(std::uint32_t appId)
        {
            return std::find(g_selection.begin(), g_selection.end(), appId) != g_selection.end();
        }

        void ClearSelection()
        {
            g_selection.clear();
            g_selectedEntry = 0;
            g_selectionAnchor = 0;
        }

        void SyncSelection(const IdlerModel& model)
        {
            std::erase_if(g_selection, [&](std::uint32_t appId) { return !model.FindEntry(appId); });
            if (g_selectedEntry != 0 && !model.FindEntry(g_selectedEntry))
                g_selectedEntry = 0;
            if (g_selectedEntry == 0)
                g_selectedEntry = g_selection.empty() ? 0 : g_selection.back();
            else if (!IsSelected(g_selectedEntry))
                g_selection = { g_selectedEntry };
        }

        void FocusOnCtrlF(bool inDialog = false)
        {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F) && (inDialog || !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)))
                ImGui::SetKeyboardFocusHere();
        }

        char g_queueSearch[128] = "";
        std::string g_queueSearchList;

        bool IsSearchingQueue()
        {
            return !Trim(g_queueSearch).empty();
        }

        std::vector<std::size_t> ShownEntries(const IdlerModel& model)
        {
            const auto& queue = model.Queue();
            const std::string needle = FoldCase(Trim(g_queueSearch));
            std::vector<std::size_t> shown;
            shown.reserve(queue.size());
            for (std::size_t index = 0; index < queue.size(); ++index)
            {
                const std::uint32_t appId = queue[index].AppId;
                const Game* game = model.FindGame(appId);
                if (needle.empty() || MatchesSearch(game ? game->Folded : FoldCase(GameName(model, appId)), appId, needle))
                    shown.push_back(index);
            }
            return shown;
        }

        void ClickEntry(const std::vector<QueueEntry>& queue, const std::vector<std::size_t>& shown, std::uint32_t appId)
        {
            const ImGuiIO& io = ImGui::GetIO();
            const auto place = [&](std::uint32_t app) { return std::find_if(shown.begin(), shown.end(), [&](std::size_t index) { return queue[index].AppId == app; }) - shown.begin(); };
            if (io.KeyShift && g_selectionAnchor != 0 && IsSelected(g_selectionAnchor) && place(g_selectionAnchor) < static_cast<std::ptrdiff_t>(shown.size()))
            {
                const auto from = std::min(place(g_selectionAnchor), place(appId));
                const auto to = std::max(place(g_selectionAnchor), place(appId));
                g_selection.clear();
                for (auto index = from; index <= to && index < static_cast<std::ptrdiff_t>(shown.size()); ++index)
                    g_selection.push_back(queue[shown[static_cast<std::size_t>(index)]].AppId);
                g_selectedEntry = appId;
                return;
            }
            g_selectionAnchor = appId;
            if (io.KeyCtrl)
            {
                if (IsSelected(appId))
                    std::erase(g_selection, appId);
                else
                    g_selection.push_back(appId);
                g_selectedEntry = g_selection.empty() ? 0 : g_selection.back();
                return;
            }
            g_selection = { appId };
            g_selectedEntry = appId;
        }

        void RemoveShown(IdlerModel& model, const std::vector<std::size_t>& shown, bool finishedOnly)
        {
            std::vector<std::uint32_t> apps;
            for (const std::size_t index : shown)
            {
                const QueueEntry& entry = model.Queue()[index];
                if (!finishedOnly || IsFinished(entry))
                    apps.push_back(entry.AppId);
            }
            for (const std::uint32_t appId : apps)
                model.Remove(appId);
        }

        void RemoveSelected(IdlerModel& model)
        {
            const std::vector<std::uint32_t> selected = g_selection;
            for (const std::uint32_t appId : selected)
                model.Remove(appId);
            ClearSelection();
        }

        void RemoveSelectedAndPickNext(IdlerModel& model, const std::vector<std::size_t>& shown)
        {
            const auto first = std::find_if(shown.begin(), shown.end(), [&](std::size_t index) { return IsSelected(model.Queue()[index].AppId); });
            const auto place = static_cast<std::size_t>(first - shown.begin());
            RemoveSelected(model);
            const std::vector<std::size_t> left = ShownEntries(model);
            if (left.empty())
                return;
            const std::uint32_t next = model.Queue()[left[std::min(place, left.size() - 1)]].AppId;
            g_selection = { next };
            g_selectedEntry = next;
            g_selectionAnchor = next;
        }

        void MoveSelectedToTop(IdlerModel& model)
        {
            int target = 0;
            for (std::size_t index = 0; index < model.Queue().size(); ++index)
            {
                if (IsSelected(model.Queue()[index].AppId))
                    model.MoveEntry(static_cast<int>(index), target++);
            }
        }

        void OpenListEditor(const IdlerModel& model, bool isNew)
        {
            g_editor.WantsOpen = true;
            g_editor.IsNew = isNew;
            g_editor.Name[0] = '\0';
            g_editor.Apps.clear();
            g_editor.Search[0] = '\0';
            g_editor.ViewStale = true;
            g_editor.LastRow = -1;
            g_editor.Collections = Steam::ReadCollections(model.SteamStatus());
            for (SteamCollection& collection : g_editor.Collections)
                std::erase_if(collection.Apps, [&](std::uint32_t appId) { return !model.FindGame(appId); });
            std::erase_if(g_editor.Collections, [](const SteamCollection& collection) { return collection.Apps.empty(); });
            if (isNew)
                return;
            std::snprintf(g_editor.Name, sizeof(g_editor.Name), "%s", model.Lists()[static_cast<std::size_t>(model.ActiveList())].Name.c_str());
            for (const QueueEntry& entry : model.Queue())
                g_editor.Apps.push_back(entry.AppId);
        }

        void ListPicker(IdlerModel& model)
        {
            const auto& lists = model.Lists();
            const int active = model.ActiveList();
            const float button = ImGui::GetFrameHeight();
            const float gap = ImGui::GetStyle().ItemSpacing.x;

            BeginRow("##lists");
            ImGui::SetNextItemWidth(std::max(Px(60.0f), RowWidth() - (button + gap) * 4.0f));
            if (ImGui::BeginCombo("##list", lists[static_cast<std::size_t>(active)].Name.c_str()))
            {
                for (std::size_t index = 0; index < lists.size(); ++index)
                {
                    if (model.IsListHidden(static_cast<int>(index)))
                        continue;
                    ImGui::PushID(static_cast<int>(index));
                    const std::string label = lists[index].Name + "  (" + std::to_string(lists[index].Entries.size()) + ")";
                    if (ImGui::Selectable(label.c_str(), static_cast<int>(index) == active))
                    {
                        model.SelectList(static_cast<int>(index));
                        ClearSelection();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ItemTooltip("Your saved lists. The one chosen here is the queue.");

            ImGui::SameLine();
            if (IconButton(ICON_FA_PLUS "##new_list", button))
                OpenListEditor(model, true);
            ItemTooltip("New list");
            ImGui::SameLine();
            if (IconButton(ICON_FA_PEN "##edit_list", button))
                OpenListEditor(model, false);
            ItemTooltip("Change this list");
            ImGui::SameLine();
            ImGui::BeginDisabled(model.VisibleListCount() < 2);
            if (IconButton(ICON_FA_TRASH "##delete_list", button, true))
            {
                if (lists[static_cast<std::size_t>(active)].Entries.empty())
                    model.DeleteList(active);
                else
                    ImGui::OpenPopup("##list_delete");
            }
            ImGui::EndDisabled();
            ItemTooltip("Delete this list");
            ImGui::SameLine();
            bool& covers = model.Options().QueueCovers;
            if (IconButton(covers ? ICON_FA_LIST "##view" : ICON_FA_TH_LARGE "##view", button))
            {
                covers = !covers;
                model.SaveNow();
            }
            ItemTooltip(covers ? "Show as a list" : "Show as covers");

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            if (ImGui::BeginPopup("##list_delete"))
            {
                const QueueList& list = lists[static_cast<std::size_t>(active)];
                ImGui::Text("Delete \"%s\" with its %d games?", list.Name.c_str(), static_cast<int>(list.Entries.size()));
                if (DangerButton(ICON_FA_TRASH " Delete"))
                {
                    model.DeleteList(active);
                    ClearSelection();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        void QueueTools(IdlerModel& model)
        {
            const auto& queue = model.Queue();
            const std::string& list = model.Lists()[static_cast<std::size_t>(model.ActiveList())].Name;
            if (g_queueSearchList != list)
            {
                g_queueSearchList = list;
                g_queueSearch[0] = '\0';
            }
            if (queue.size() < 2 && g_queueSearch[0] == '\0')
                return;

            const float button = ImGui::GetFrameHeight();
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const std::string count = IsSearchingQueue() ? std::to_string(ShownEntries(model).size()) + " of " + std::to_string(queue.size()) : std::string();
            const float extra = count.empty() ? 0.0f : ImGui::CalcTextSize(count.c_str()).x + button + gap * 2.0f;

            BeginRow("##queue_tools");
            ImGui::SetNextItemWidth(std::max(Px(60.0f), RowWidth() - button - gap - extra));
            FocusOnCtrlF();
            ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " Search", g_queueSearch, sizeof(g_queueSearch));
            ItemTooltip("Ctrl+F");
            if (!count.empty())
            {
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ManifoldTheme::Muted, "%s", count.c_str());
                ImGui::SameLine();
                if (IconButton(ICON_FA_TIMES "##clear", button))
                    g_queueSearch[0] = '\0';
                ItemTooltip("Clear the search");
            }
            ImGui::SameLine();
            if (IconButton(ICON_FA_SORT_AMOUNT_DOWN "##sort", button))
                ImGui::OpenPopup("##sort_menu");
            ItemTooltip("Sort this list");

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            if (ImGui::BeginPopup("##sort_menu"))
            {
                ImGui::TextColored(ManifoldTheme::Muted, "Sort once");
                for (int order = 0; order < static_cast<int>(QueueOrder::Count); ++order)
                {
                    if (ImGui::MenuItem(QueueOrderName(static_cast<QueueOrder>(order))))
                        model.SortQueue(static_cast<QueueOrder>(order));
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

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

        bool HasAchievements(const Game* game)
        {
            return game && (game->HasAchievements || game->AchievementsTotal > 0 || game->Unlock != UnlockRule::Unknown);
        }

        void GameMenuItems(const IdlerModel& model, std::uint32_t appId, bool queuePage, GameRequest& request)
        {
            const auto ask = [&](GameAction action, int list = -1) { request = { action, appId, list }; };
            const QueueEntry* entry = model.FindEntry(appId);
            const auto& queue = model.Queue();

            if (ImGui::MenuItem(ICON_FA_INFO_CIRCLE " Details"))
                ask(GameAction::Details);
            if (HasAchievements(model.FindGame(appId)) && ImGui::MenuItem(ICON_FA_TROPHY " Achievements"))
                ask(GameAction::Achievements);
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_PLAY " Idle now"))
                ask(GameAction::IdleNow);
            if (queuePage && entry && ImGui::MenuItem(ICON_FA_ARROW_UP " Move to the top", nullptr, false, !queue.empty() && queue.front().AppId != appId))
                ask(GameAction::MoveToTop);
            if (entry && !model.QueueFarmsCards() && ImGui::MenuItem(ICON_FA_EYE " Show to friends", nullptr, appId == model.ShownApp()))
                ask(GameAction::ShowToFriends);
            if (entry && entry->Failed && ImGui::MenuItem(ICON_FA_SYNC " Try again"))
                ask(GameAction::TryAgain);
            if (model.VisibleListCount() < 2)
            {
                if (!entry && ImGui::MenuItem(ICON_FA_PLUS " Add to the queue"))
                    ask(GameAction::AddToQueue);
            }
            else if (ImGui::BeginMenu(ICON_FA_PLUS " Add to list"))
            {
                const auto& lists = model.Lists();
                for (std::size_t index = 0; index < lists.size(); ++index)
                {
                    if (model.IsListHidden(static_cast<int>(index)))
                        continue;
                    const bool has = std::any_of(lists[index].Entries.begin(), lists[index].Entries.end(), [&](const QueueEntry& listed) { return listed.AppId == appId; });
                    ImGui::PushID(static_cast<int>(index));
                    if (ImGui::MenuItem(lists[index].Name.c_str(), nullptr, has, !has))
                        ask(GameAction::AddToList, static_cast<int>(index));
                    ImGui::PopID();
                }
                ImGui::EndMenu();
            }
            if (entry && DangerMenuItem(ICON_FA_TIMES " Remove from the queue"))
                ask(GameAction::RemoveFromQueue);
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_EXTERNAL_LINK_ALT " Store page"))
                ask(GameAction::StorePage);
            if (ImGui::MenuItem(ICON_FA_BOOK " Open in Steam"))
                ask(GameAction::OpenInSteam);
        }

        void RunGameRequest(IdlerModel& model, const GameRequest& request)
        {
            const std::uint32_t appId = request.App;
            switch (request.Action)
            {
            case GameAction::Details:
                OpenGameWindow(appId);
                break;
            case GameAction::Achievements:
                OpenGameWindow(appId, GameTab::Achievements);
                break;
            case GameAction::IdleNow:
                model.IdleNow(appId);
                break;
            case GameAction::AddToQueue:
                model.AddToQueue(appId);
                break;
            case GameAction::AddToList:
                model.AddToList(request.List, appId);
                break;
            case GameAction::RemoveFromQueue:
                model.Remove(appId);
                break;
            case GameAction::MoveToTop:
            {
                const auto& queue = model.Queue();
                const auto at = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& entry) { return entry.AppId == appId; });
                if (at != queue.end())
                    model.MoveEntry(static_cast<int>(at - queue.begin()), 0);
                break;
            }
            case GameAction::ShowToFriends:
                model.SetShownApp(appId);
                break;
            case GameAction::TryAgain:
                model.Retry(appId);
                break;
            case GameAction::StorePage:
                Desktop::OpenStorePage(appId);
                break;
            case GameAction::OpenInSteam:
                Desktop::OpenInLibrary(appId);
                break;
            default:
                break;
            }
        }

        void GameButtons(IdlerModel& model, std::uint32_t appId, bool ways)
        {
            const QueueEntry* entry = model.FindEntry(appId);
            const bool queued = entry != nullptr;
            const bool failed = entry && entry->Failed;
            const bool held = entry && IsHeld(*entry);
            GameRequest request;
            const auto ask = [&](GameAction action) { request = { action, appId, -1 }; };

            if (ways)
            {
                if (ActionButton(ICON_FA_INFO_CIRCLE, "Details"))
                    ask(GameAction::Details);
                ActionTooltip("Everything about the game in one window. Also with a double-click or Enter.");
            }
            if (ActionButton(ICON_FA_PLAY, "Idle now"))
                ask(GameAction::IdleNow);
            ActionTooltip("Puts the game first in the queue and starts it.");
            if (queued)
            {
                if (ActionButton(ICON_FA_TIMES, "Remove", true))
                    ask(GameAction::RemoveFromQueue);
                ActionTooltip("Takes it out of the queue. Also with Delete.");
            }
            else if (ActionButton(ICON_FA_PLUS, "Add to queue"))
            {
                ask(GameAction::AddToQueue);
            }
            if (ways && HasAchievements(model.FindGame(appId)) && ActionButton(ICON_FA_TROPHY, "Achievements"))
                ask(GameAction::Achievements);
            if (ActionButton(ICON_FA_EXTERNAL_LINK_ALT, "Store page"))
                ask(GameAction::StorePage);
            if (ActionButton(ICON_FA_BOOK, "Open in Steam"))
                ask(GameAction::OpenInSteam);
            if (failed && ActionButton(ICON_FA_SYNC, "Try again"))
                ask(GameAction::TryAgain);
            if (held)
            {
                if (ActionButton(ICON_FA_FORWARD, "Idle anyway", true))
                    model.AllowPastRefund(appId);
                ActionTooltip("Goes on past the refund limit. Steam will not refund this game after that.");
            }
            RunGameRequest(model, request);
        }

        void QueueCard(IdlerModel& model)
        {
            BeginCard("Queue");
            ListPicker(model);
            QueueTools(model);
            const auto& queue = model.Queue();
            const bool searching = IsSearchingQueue();
            std::vector<std::size_t> shown = ShownEntries(model);
            SyncSelection(model);
            bool removeByKey = false;
            if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            {
                if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A))
                {
                    g_selection.clear();
                    for (const std::size_t index : shown)
                        g_selection.push_back(queue[index].AppId);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && g_selection.size() > 1)
                {
                    g_selection = { g_selectedEntry };
                    g_selectionAnchor = g_selectedEntry;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    g_queueSearch[0] = '\0';
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    removeByKey = !g_selection.empty();
                }
                else if ((ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) && !ImGui::GetIO().NavVisible &&
                    g_selection.size() == 1 && model.FindEntry(g_selectedEntry))
                {
                    OpenGameWindow(g_selectedEntry);
                }
            }

            BeginCardList("##queue", CardRowsHeight(1));
            if (queue.empty())
                EmptyState("##none", 101u, "Nothing queued. Add some games to start.");
            else if (shown.empty())
                EmptyState("##nothing", 104u, "No game fits.");

            int moveFrom = -1;
            int moveTo = -1;
            GameRequest request;
            std::uint32_t opened = 0;
            bool moveSelected = false;
            bool removeSelected = false;
            const auto entryMenu = [&](const QueueEntry& entry, const std::string& name)
                {
                    const bool several = g_selection.size() > 1 && IsSelected(entry.AppId);
                    const std::string count = std::to_string(g_selection.size()) + " games";
                    if (!BeginContextMenu("##entry_menu", several ? count : name))
                        return;
                    if (several)
                    {
                        moveSelected = ImGui::MenuItem(ICON_FA_ARROW_UP " Move to the top");
                        removeSelected = DangerMenuItem((std::string(ICON_FA_TIMES " Remove ") + count).c_str());
                        ImGui::EndPopup();
                        return;
                    }
                    GameMenuItems(model, entry.AppId, true, request);
                    ImGui::EndPopup();
                };
            const auto openedByDoubleClick = [&](std::uint32_t appId)
                {
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        opened = appId;
                };

            if (model.Options().QueueCovers)
            {
                const float gap = Px(6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
                const CoverGridShape grid = CoverGridOf(ImGui::GetContentRegionAvail().x, gap);
                const float height = CoverTileHeight(grid.Width, true);
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                const auto columns = static_cast<std::size_t>(grid.Columns);
                for (std::size_t position = 0; position < shown.size(); ++position)
                {
                    const std::size_t index = shown[position];
                    const QueueEntry& entry = queue[index];
                    if (position % columns != 0)
                        ImGui::SameLine();

                    const RunState state = model.StateOf(entry.AppId);
                    const std::string name = GameName(model, entry.AppId);
                    const auto [detail, detailColor] = EntryDetail(entry, state, model.RestSecondsLeft(entry.AppId));
                    const bool resting = entry.Resting && entry.IdledSeconds > 0.0 && state == RunState::Off && !entry.Failed && !IsHeld(entry) && !IsFinished(entry);

                    CoverTileOptions options;
                    options.Detailed = true;
                    options.Detail = detail;
                    options.DetailColor = detailColor;
                    options.Progress = entry.Failed ? -1.0f : Progress(entry);
                    options.Glow = state == RunState::Running;
                    options.Seed = entry.AppId;
                    options.ShowCreature = state == RunState::Running || state == RunState::Starting || resting;
                    options.Mood = state == RunState::Running ? IdleAnimation::Mood::Idling : state == RunState::Starting ? IdleAnimation::Mood::Starting : IdleAnimation::Mood::Sleeping;
                    options.MarkTop = entry.AppId == model.ShownApp() && !model.QueueFarmsCards() ? ICON_FA_EYE : nullptr;

                    const ImTextureID cover = ImGui::IsRectVisible(ImVec2(grid.Width, height)) ? Covers::Get(entry.AppId, Covers::Size::Thumb) : ImTextureID{};
                    ImGui::PushID(static_cast<int>(entry.AppId));
                    if (CoverTile("##cover", IsSelected(entry.AppId), cover, name, grid.Width, options))
                        ClickEntry(queue, shown, entry.AppId);
                    openedByDoubleClick(entry.AppId);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s\n%s", name.c_str(), detail.c_str());

                    if (searching && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                        ImGui::SetTooltip("Clear the search to drag.");
                    else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        const ImVec2 mouse = ImGui::GetMousePos();
                        const int column = std::clamp(static_cast<int>((mouse.x - origin.x) / (grid.Width + gap)), 0, grid.Columns - 1);
                        const int row = std::max(0, static_cast<int>((mouse.y - origin.y) / (height + gap)));
                        const int target = std::min(row * grid.Columns + column, static_cast<int>(queue.size()) - 1);
                        if (target != static_cast<int>(index))
                        {
                            moveFrom = static_cast<int>(index);
                            moveTo = target;
                        }
                    }
                    entryMenu(entry, name);
                    ImGui::PopID();
                }
                ImGui::PopStyleVar();
            }
            for (std::size_t position = 0; !model.Options().QueueCovers && position < shown.size(); ++position)
            {
                const std::size_t index = shown[position];
                const QueueEntry& entry = queue[index];
                const RunState state = model.StateOf(entry.AppId);
                const bool selected = IsSelected(entry.AppId);
                const std::string name = GameName(model, entry.AppId);
                const auto [detail, detailColor] = EntryDetail(entry, state, model.RestSecondsLeft(entry.AppId));
                std::vector<RowTag> tags = EntryTags(entry, state);
                if (entry.Resting && entry.IdledSeconds > 0.0 && state == RunState::Off && !entry.Failed && !IsHeld(entry) && !IsFinished(entry))
                    tags.push_back(AnimatedTag("On a break", IdleAnimation::Mood::Sleeping, entry.AppId));
                if (entry.AppId == model.ShownApp() && !model.QueueFarmsCards())
                    tags.insert(tags.begin(), { ICON_FA_EYE, ManifoldTheme::Accent });
                const ImVec4 nameColor = selected || state == RunState::Running ? ManifoldTheme::Text : ManifoldTheme::Label;

                GameRowOptions options;
                options.Progress = entry.Failed ? -1.0f : Progress(entry);
                options.Glow = state == RunState::Running;
                options.GlowSeed = entry.AppId;

                ImGui::PushID(static_cast<int>(entry.AppId));
                if (GameRow("##entry", selected, Covers::Get(entry.AppId, Covers::Size::Thumb), name, nameColor, detail, detailColor, tags, options))
                    ClickEntry(queue, shown, entry.AppId);
                openedByDoubleClick(entry.AppId);

                if (searching && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    ImGui::SetTooltip("Clear the search to drag.");
                else if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
                {
                    const int next = static_cast<int>(index) + (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.0f ? -1 : 1);
                    if (next >= 0 && next < static_cast<int>(queue.size()))
                    {
                        moveFrom = static_cast<int>(index);
                        moveTo = next;
                        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                    }
                }

                entryMenu(entry, name);
                ImGui::PopID();
            }
            EndCardList();

            if (moveFrom >= 0)
                model.MoveEntry(moveFrom, moveTo);
            if (moveSelected)
                MoveSelectedToTop(model);
            if (removeSelected)
                RemoveSelected(model);
            else if (removeByKey)
                RemoveSelectedAndPickNext(model, shown);
            RunGameRequest(model, request);
            if (opened != 0)
            {
                g_selection = { opened };
                g_selectedEntry = opened;
                g_selectionAnchor = opened;
                OpenGameWindow(opened);
            }

            BeginRow("##actions");
            if (ActionButton(ICON_FA_PLUS, "Add games"))
                OpenListEditor(model, false);
            shown = ShownEntries(model);
            const bool anyFinished = std::any_of(shown.begin(), shown.end(), [&](std::size_t index) { return IsFinished(queue[index]); });
            ImGui::BeginDisabled(!anyFinished);
            if (ActionButton(ICON_FA_CHECK, "Remove finished", true))
            {
                if (searching)
                    RemoveShown(model, shown, true);
                else
                    model.RemoveFinished();
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(shown.empty());
            if (ActionButton(ICON_FA_TIMES, searching ? "Remove shown" : "Remove all", true))
            {
                if (searching)
                    RemoveShown(model, shown, false);
                else
                    model.RemoveAll();
            }
            ImGui::EndDisabled();
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        std::string HoldText(const IdlerModel& model)
        {
            char text[96];
            switch (model.CurrentHold())
            {
            case Hold::Steam:
                return "Waiting for Steam. Idling goes on when it is back.";
            case Hold::Playing:
                return "Paused while " + GameName(model, model.PlayingApp()) + " runs.";
            case Hold::Hours:
                std::snprintf(text, sizeof(text), "Waiting for the set hours, %02d:00 to %02d:00.", model.Options().FromHour, model.Options().ToHour);
                return text;
            case Hold::Away:
                return "Waiting until you are away for " + DurationText(static_cast<std::uint64_t>(model.Options().AwayMinutes * 60.0f)) + ".";
            default:
                return {};
            }
        }

        void ControlCard(IdlerModel& model)
        {
            const auto& queue = model.Queue();
            const bool active = model.Active();
            const Hold hold = model.CurrentHold();

            BeginCard("Control");
            BeginRow("##main");
            QueueButton(model, RowWidth(), Px(44.0f), 1.3f);
            ImGui::PopID();

            if (active && hold != Hold::None)
                Notice("##hold", hold == Hold::Steam ? ICON_FA_HOURGLASS_HALF : ICON_FA_PAUSE, ManifoldTheme::Warning, HoldText(model));
            if (model.ShutdownFailed())
                Notice("##shutdown_failed", ICON_FA_TIMES_CIRCLE, ManifoldTheme::Error, ErrorMessage(ErrorCode::ShutdownFailed));

            ControlRow("Games at once", "How many games of the queue idle at the same time. Steam counts up to 32.", Px(110.0f), [&]()
                {
                    int slots = model.Slots();
                    const int step = 1;
                    const int fastStep = 4;
                    if (ImGui::InputScalar("##slots", ImGuiDataType_S32, &slots, &step, &fastStep, "%d"))
                        model.SetSlots(slots);
                });

            char running[48];
            std::snprintf(running, sizeof(running), "%d of %d", model.RunningCount(), static_cast<int>(queue.size()));
            InfoRow("Running", running, model.RunningCount() > 0 ? ManifoldTheme::Accent : ManifoldTheme::Muted);
            InfoRow("This session", DurationText(static_cast<std::uint64_t>(model.SessionSeconds())), ManifoldTheme::Label);

            const QueueForecast forecast = model.Forecast();
            if (forecast.Endless)
            {
                InfoRow("Time left", model.QueueFarmsCards() ? "Until the cards dropped" : "No end: a game has no limit", ManifoldTheme::Muted);
            }
            else if (forecast.Seconds > 0.0)
            {
                std::string left = DurationText(static_cast<std::uint64_t>(std::ceil(forecast.Seconds)));
                if (model.Options().VaryTimes)
                    left = "About " + left + " plus breaks";
                else if (active && hold == Hold::None)
                    left += ", done " + ClockText(forecast.Seconds);
                InfoRow("Time left", left, ManifoldTheme::Label);
            }

            bool armed = model.ShutdownArmed();
            ImGui::BeginDisabled(forecast.Endless);
            if (ToggleRow("Shut down when finished",
                "Turns the computer off 60 seconds after the last game reached its goal, unless you cancel. For this run only.", armed))
                model.SetShutdownArmed(armed);
            ImGui::EndDisabled();
            EndCard();
        }

        void GoalControls(const char* id, Goal& goal, bool& changed, bool mixedKind = false, bool mixedHours = false)
        {
            ImGui::PushID(id);
            ControlRow("Goal", "When the game is done. \"For a time\" counts from now, \"Until a playtime\" includes the time played before.", Px(150.0f), [&]()
                {
                    if (ImGui::BeginCombo("##kind", mixedKind ? "Mixed" : GoalName(goal.Kind)))
                    {
                        for (int kind = 0; kind < static_cast<int>(GoalKind::Count); ++kind)
                        {
                            const auto value = static_cast<GoalKind>(kind);
                            if (!GoalShown(value))
                                continue;
                            if (ImGui::Selectable(GoalName(value), value == goal.Kind))
                            {
                                goal.Kind = value;
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                });
            if (goal.Kind == GoalKind::ForHours || goal.Kind == GoalKind::UntilTotal)
            {
                ControlRow("Hours", nullptr, Px(150.0f), [&]()
                    {
                        const float step = 0.5f;
                        const float fastStep = 1.0f;
                        if (ImGui::InputScalar("##hours", ImGuiDataType_Float, &goal.Hours, &step, &fastStep, mixedHours ? "Mixed" : "%.1f h"))
                        {
                            goal.Hours = std::clamp(goal.Hours, 0.1f, 100000.0f);
                            changed = true;
                        }
                    });
            }
            ImGui::PopID();
        }

        void SelectionCard(IdlerModel& model)
        {
            BeginCard("Selected games");

            constexpr std::size_t kShownCovers = 4;
            std::vector<ImTextureID> covers;
            for (const QueueEntry& entry : model.Queue())
            {
                if (IsSelected(entry.AppId) && covers.size() < kShownCovers)
                    covers.push_back(Covers::Get(entry.AppId, Covers::Size::Thumb));
            }
            CoverStripRow("##covers", covers, static_cast<int>(g_selection.size()) - static_cast<int>(covers.size()));

            double idled = 0.0;
            const QueueEntry* first = nullptr;
            bool mixedKind = false;
            bool mixedHours = false;
            for (const QueueEntry& entry : model.Queue())
            {
                if (!IsSelected(entry.AppId))
                    continue;
                idled += entry.IdledSeconds;
                if (!first)
                    first = &entry;
                mixedKind = mixedKind || entry.Goal.Kind != first->Goal.Kind;
                mixedHours = mixedHours || entry.Goal.Hours != first->Goal.Hours;
            }
            InfoRow("Games", std::to_string(g_selection.size()), ManifoldTheme::Label);
            InfoRow("Idled", DurationText(static_cast<std::uint64_t>(idled)), ManifoldTheme::Label);
            if (first)
            {
                Goal goal = first->Goal;
                bool changed = false;
                GoalControls("##goal", goal, changed, mixedKind, mixedHours);
                if (changed)
                {
                    for (const std::uint32_t appId : g_selection)
                        model.SetGoal(appId, goal);
                }
            }

            BeginRow("##actions");
            if (ActionButton(ICON_FA_ARROW_UP, "To the top"))
                MoveSelectedToTop(model);
            const std::string remove = "Remove " + std::to_string(g_selection.size()) + " games";
            if (ActionButton(ICON_FA_TIMES, remove, true))
                RemoveSelected(model);
            ActionTooltip("Also with Delete.");
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        ImVec4 RarityColor(Rarity rarity);

        void QueueEntryRows(IdlerModel& model, const QueueEntry& entry, const Game* game, bool withCreature = true)
        {
            if (withCreature)
                InfoRow("Creature", IdleAnimation::NameOf(entry.AppId), RarityColor(RarityOf(IdleAnimation::CreatureOf(entry.AppId))));
            Goal goal = entry.Goal;
            bool goalChanged = false;
            GoalControls("##goal", goal, goalChanged);
            if (goalChanged)
                model.SetGoal(entry.AppId, goal);
            std::string idled = DurationText(static_cast<std::uint64_t>(entry.IdledSeconds));
            if (game)
                idled += ", playtime " + PlaytimeText(game->PlaytimeMinutes);
            InfoRow("In the queue", idled, ManifoldTheme::Label);
            if (!model.QueueFarmsCards())
            {
                bool shown = model.ShownApp() == entry.AppId;
                if (ToggleRow("Show to friends", "Steam shows friends the game that started last. This one starts after the others, so it is the one they see.", shown))
                    model.SetShownApp(entry.AppId);
            }
            if (entry.Failed)
                Notice("##error", ICON_FA_TIMES_CIRCLE, ManifoldTheme::Error, entry.Error);
            if (IsHeld(entry))
                Notice("##held", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, "This game can still be refunded. More idling would end that.");
        }

        void SelectedEntryCard(IdlerModel& model)
        {
            if (g_selection.size() > 1)
            {
                SelectionCard(model);
                return;
            }

            BeginCard("Selected game");
            const QueueEntry* found = model.FindEntry(g_selectedEntry);
            if (!found)
            {
                EmptyState("##none", 102u, "Pick a game in the queue. Ctrl or Shift picks several.");
                EndCard();
                return;
            }

            const QueueEntry entry = *found;
            const Game* game = model.FindGame(entry.AppId);

            const float spacing = ImGui::GetStyle().ItemSpacing.y;
            const float buttons = CardRowsHeight(1);
            const float room = ImGui::GetContentRegionAvail().y - spacing * 2.0f - buttons - 1.0f;
            CoverRow("##cover", Covers::Get(entry.AppId, Covers::Size::Full), std::max(Px(56.0f), room - g_selectedRowsHeight));

            BeginCardList("##rows", std::max(buttons, ImGui::GetContentRegionAvail().y - g_selectedRowsHeight - spacing - 1.0f));
            InfoRow("Name", GameName(model, entry.AppId));
            QueueEntryRows(model, entry, game);
            g_selectedRowsHeight = std::ceil(ImGui::GetCursorPosY() - spacing);
            EndCardList();

            BeginRow("##actions");
            GameButtons(model, entry.AppId, true);
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        void IdlingPage(Shell& app)
        {
            IdlerModel& model = app.Model();
            std::string status;
            ImVec4 color = ManifoldTheme::Muted;
            switch (model.Phase())
            {
            case QueuePhase::Stopped:
                status = "stopped";
                break;
            case QueuePhase::Stopping:
                status = "stopping";
                break;
            case QueuePhase::Waiting:
                status = "waiting";
                color = ManifoldTheme::Warning;
                break;
            case QueuePhase::Starting:
                status = "starting";
                color = ManifoldTheme::Accent;
                break;
            default:
            {
                const int running = model.RunningCount();
                status = std::to_string(running) + (running == 1 ? " game idling" : " games idling");
                color = ManifoldTheme::Accent;
                break;
            }
            }

            BeginPage(app, status, color, false);
            CardColumns("##idling_columns",
                [&]() { QueueCard(model); },
                [&]()
                {
                    ControlCard(model);
                    SelectedEntryCard(model);
                }, 0.58f, Px(kDetailColumnWidth));
            EndPageBody();
        }

        const char* UnlockText(UnlockRule rule)
        {
            switch (rule)
            {
            case UnlockRule::Here:
                return "Possible";
            case UnlockRule::Mixed:
                return "Partly possible";
            case UnlockRule::Server:
                return "Only by the game's server";
            default:
                return "Known after reading the list";
            }
        }

        const char* UnlockTooltip(UnlockRule rule)
        {
            switch (rule)
            {
            case UnlockRule::Here:
                return "Each achievement can be unlocked in its list.";
            case UnlockRule::Mixed:
                return "Some achievements can be unlocked in the list. The game's own server sets the others.";
            case UnlockRule::Server:
                return "The game's own server sets its achievements, and Steam refuses them from here.";
            default:
                return "Steam brings its file about the game's achievements the first time their list is read.";
            }
        }

        std::string GameDetail(const Game& game)
        {
            char text[96];
            switch (g_sort)
            {
            case LibrarySort::Reviews:
                if (game.ReviewPercent == 0)
                    return "No reviews known";
                std::snprintf(text, sizeof(text), "%u %% positive reviews", game.ReviewPercent);
                return text;
            case LibrarySort::Metacritic:
                if (game.Metacritic == 0)
                    return "No Metacritic score";
                std::snprintf(text, sizeof(text), "Metacritic %u", game.Metacritic);
                return text;
            case LibrarySort::Released:
                return game.Released != 0 ? "Released " + DateText(game.Released) : "Release date not known";
            case LibrarySort::Purchased:
                return game.Purchased != 0 ? "Purchased " + DateText(game.Purchased) : "Free";
            case LibrarySort::Achievements:
                if (game.AchievementsTotal == 0)
                    return "No achievements known";
                std::snprintf(text, sizeof(text), "%u of %u achievements%s", game.AchievementsDone, game.AchievementsTotal,
                    game.Unlock == UnlockRule::Server ? ", only by the game's server" : game.Unlock == UnlockRule::Mixed ? ", partly by the game's server" : "");
                return text;
            default:
                break;
            }
            std::string detail = PlaytimeText(game.PlaytimeMinutes);
            if (const std::string last = DateText(game.LastPlayed); !last.empty())
                detail += ", last played " + last;
            return detail;
        }

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

        bool RatingCombo(float width)
        {
            const char* preview = kRatings[0].Label;
            for (const RatingChoice& choice : kRatings)
            {
                if (choice.Percent == g_minReview)
                    preview = choice.Label;
            }
            bool changed = false;
            ImGui::SetNextItemWidth(width);
            if (ImGui::BeginCombo("##rating", preview))
            {
                for (const RatingChoice& choice : kRatings)
                {
                    if (ImGui::Selectable(choice.Label, choice.Percent == g_minReview))
                    {
                        g_minReview = choice.Percent;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ItemTooltip("Share of positive user reviews in the store.");
            return changed;
        }


        bool IsGameSelected(std::uint32_t appId)
        {
            return g_gameSelection.contains(appId);
        }

        void ClickGame(const IdlerModel& model, std::uint32_t appId)
        {
            const auto& games = model.Games();
            std::vector<std::uint32_t> shown;
            shown.reserve(g_view.size());
            for (const int index : g_view)
                shown.push_back(games[static_cast<std::size_t>(index)].AppId);
            ClickSelection(g_gameSelection, g_selectedGame, g_gameAnchor, shown, appId, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
        }

        std::vector<std::uint32_t> SelectedGames(const IdlerModel& model)
        {
            const auto& games = model.Games();
            std::unordered_set<std::uint32_t> rest = g_gameSelection;
            std::vector<std::uint32_t> apps;
            apps.reserve(rest.size());
            for (const int index : g_view)
            {
                const std::uint32_t appId = games[static_cast<std::size_t>(index)].AppId;
                if (rest.erase(appId) > 0)
                    apps.push_back(appId);
            }
            apps.insert(apps.end(), rest.begin(), rest.end());
            return apps;
        }

        int SelectedInList(const std::vector<QueueEntry>& entries)
        {
            return static_cast<int>(std::count_if(entries.begin(), entries.end(), [](const QueueEntry& entry) { return IsGameSelected(entry.AppId); }));
        }

        void TakeSelectedOut(IdlerModel& model)
        {
            for (const std::uint32_t appId : SelectedGames(model))
            {
                if (model.FindEntry(appId))
                    model.Remove(appId);
            }
        }

        void AddSelectedToListItems(IdlerModel& model)
        {
            const auto& lists = model.Lists();
            for (std::size_t index = 0; index < lists.size(); ++index)
            {
                if (model.IsListHidden(static_cast<int>(index)))
                    continue;
                const bool has = SelectedInList(lists[index].Entries) == static_cast<int>(g_gameSelection.size());
                ImGui::PushID(static_cast<int>(index));
                if (ImGui::MenuItem(lists[index].Name.c_str(), nullptr, has, !has))
                    model.AddGamesToList(static_cast<int>(index), SelectedGames(model));
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_PLUS " New list"))
            {
                OpenListEditor(model, true);
                g_editor.Apps = SelectedGames(model);
            }
        }

        void GameMenu(IdlerModel& model, const Game& game)
        {
            const bool several = g_gameSelection.size() > 1 && IsGameSelected(game.AppId);
            const auto picked = static_cast<int>(g_gameSelection.size());
            if (!BeginContextMenu("##game_menu", several ? std::to_string(picked) + " games" : game.Name))
                return;
            if (several)
            {
                const int inQueue = SelectedInList(model.Queue());
                if (ImGui::MenuItem(ICON_FA_PLUS " Add to the queue", nullptr, false, inQueue < picked))
                    model.AddGamesToList(model.ActiveList(), SelectedGames(model));
                if (ImGui::BeginMenu(ICON_FA_PLUS " Add to list"))
                {
                    AddSelectedToListItems(model);
                    ImGui::EndMenu();
                }
                if (inQueue > 0 && DangerMenuItem((std::string(ICON_FA_TIMES " Take ") + std::to_string(inQueue) + " out of the queue").c_str()))
                    TakeSelectedOut(model);
                ImGui::EndPopup();
                return;
            }
            GameRequest request;
            GameMenuItems(model, game.AppId, false, request);
            ImGui::EndPopup();
            RunGameRequest(model, request);
        }

        void OpenGameOnDoubleClick(std::uint32_t appId)
        {
            if (!ImGui::IsItemHovered() || !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                return;
            g_gameSelection = { appId };
            g_selectedGame = appId;
            g_gameAnchor = appId;
            OpenGameWindow(appId);
        }

        void LibraryCoverGrid(IdlerModel& model)
        {
            const auto& games = model.Games();
            const float gap = Px(6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
            const auto [columns, width] = CoverGridOf(ImGui::GetContentRegionAvail().x, gap);
            const int rows = (static_cast<int>(g_view.size()) + columns - 1) / columns;

            ImGuiListClipper clipper;
            clipper.Begin(rows, CoverTileHeight(width) + gap);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    for (int column = 0; column < columns; ++column)
                    {
                        const auto index = static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column);
                        if (index >= g_view.size())
                            break;
                        if (column > 0)
                            ImGui::SameLine();

                        const Game& game = games[static_cast<std::size_t>(g_view[index])];
                        const RunState state = model.StateOf(game.AppId);
                        const QueueEntry* entry = model.FindEntry(game.AppId);

                        const RowAction action{ entry ? ICON_FA_MINUS : ICON_FA_PLUS, entry ? "Take out of the queue" : "Add to the queue", entry != nullptr };
                        bool actionClicked = false;
                        CoverTileOptions options;
                        options.Action = &action;
                        options.ActionClicked = &actionClicked;
                        options.Seed = game.AppId;
                        const bool resting = entry && entry->Resting && entry->IdledSeconds > 0.0;
                        options.ShowCreature = state == RunState::Running || state == RunState::Starting || resting;
                        options.Mood = state == RunState::Running ? IdleAnimation::Mood::Idling : state == RunState::Starting ? IdleAnimation::Mood::Starting : IdleAnimation::Mood::Sleeping;
                        options.MarkTop = entry && !options.ShowCreature ? ICON_FA_CHECK : nullptr;
                        options.MarkBottom = game.HasCards ? kCardIcon : nullptr;

                        ImGui::PushID(static_cast<int>(game.AppId));
                        if (CoverTile("##cover", IsGameSelected(game.AppId), Covers::Get(game.AppId, Covers::Size::Thumb), game.Name, width, options))
                            ClickGame(model, game.AppId);
                        OpenGameOnDoubleClick(game.AppId);
                        if (ImGui::IsItemHovered())
                        {
                            if (options.ShowCreature && options.Mood != IdleAnimation::Mood::Starting)
                                ImGui::SetTooltip("%s\n%s\n%s", game.Name.c_str(), GameDetail(game).c_str(), IdleAnimation::NameOf(game.AppId).c_str());
                            else
                                ImGui::SetTooltip("%s\n%s", game.Name.c_str(), GameDetail(game).c_str());
                        }
                        if (actionClicked)
                        {
                            if (entry)
                                model.Remove(game.AppId);
                            else
                                model.AddToQueue(game.AppId);
                        }
                        GameMenu(model, game);
                        ImGui::PopID();
                    }
                }
            }
            clipper.End();
            ImGui::PopStyleVar();
        }

        void LibraryCard(IdlerModel& model)
        {
            const auto& games = model.Games();
            bool& covers = model.Options().LibraryCovers;

            BeginCard("Games");
            BeginRow("##tools");
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const float filterWidth = Px(88.0f);
            const float ratingWidth = Px(76.0f);
            const float sortWidth = Px(100.0f);
            const float viewButton = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(std::max(Px(60.0f), RowWidth() - filterWidth - ratingWidth - sortWidth - viewButton - gap * 4.0f));
            FocusOnCtrlF();
            g_viewStale |= ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " Search", g_search, sizeof(g_search));
            ItemTooltip("Ctrl+F");
            ImGui::SameLine();
            g_viewStale |= ChoiceCombo("##filter", g_filter, LibraryFilter::Count, FilterName, filterWidth);
            ImGui::SameLine();
            g_viewStale |= RatingCombo(ratingWidth);
            ImGui::SameLine();
            g_viewStale |= ChoiceCombo("##sort", g_sort, LibrarySort::Count, SortName, sortWidth);
            ImGui::SameLine();
            if (IconLink("##view", covers ? ICON_FA_LIST : ICON_FA_TH_LARGE, ImVec2(viewButton, viewButton), covers ? "Show as a list" : "Show as covers"))
            {
                covers = !covers;
                model.SaveNow();
            }
            ImGui::PopID();

            if (g_viewStale || g_viewGeneration != model.LibraryGeneration())
            {
                g_view = LibraryView(games, g_search, g_filter, g_sort, g_minReview);
                if (g_viewGeneration != model.LibraryGeneration() && !games.empty())
                    std::erase_if(g_gameSelection, [&](std::uint32_t appId) { return !model.FindGame(appId); });
                g_viewGeneration = model.LibraryGeneration();
                g_viewStale = false;
            }

            if (g_selectedGame != 0 && !IsGameSelected(g_selectedGame))
                g_gameSelection = { g_selectedGame };
            if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            {
                if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A) && !g_view.empty())
                {
                    g_gameSelection.clear();
                    for (const int index : g_view)
                        g_gameSelection.insert(games[static_cast<std::size_t>(index)].AppId);
                    if (!IsGameSelected(g_selectedGame))
                        g_selectedGame = games[static_cast<std::size_t>(g_view.front())].AppId;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && g_gameSelection.size() > 1)
                {
                    g_gameSelection = { g_selectedGame };
                    g_gameAnchor = g_selectedGame;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    TakeSelectedOut(model);
                }
                else if ((ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) && !ImGui::GetIO().NavVisible &&
                    g_gameSelection.size() == 1 && model.FindGame(g_selectedGame))
                {
                    OpenGameWindow(g_selectedGame);
                }
            }

            BeginCardList("##games", CardRowsHeight(1));
            if (!model.ScanProblem().empty())
            {
                Notice("##problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, model.ScanProblem());
            }
            else if (games.empty())
            {
                if (!model.SteamStatus().LoggedOn())
                    Notice("##steam", nullptr, ManifoldTheme::Muted, "The library shows when Steam is running and you are logged on.");
                else
                    Notice("##scanning", nullptr, ManifoldTheme::Muted, model.Scanning() ? "Reading the library ..." : "No games found.");
            }
            else if (g_view.empty())
            {
                EmptyState("##nothing", 104u, "No game fits.");
            }

            if (covers)
                LibraryCoverGrid(model);

            ImGuiListClipper clipper;
            clipper.Begin(covers ? 0 : static_cast<int>(g_view.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const Game& game = games[static_cast<std::size_t>(g_view[static_cast<std::size_t>(row)])];
                    const bool selected = IsGameSelected(game.AppId);
                    const RunState state = model.StateOf(game.AppId);

                    const QueueEntry* entry = model.FindEntry(game.AppId);

                    std::vector<RowTag> tags;
                    if (game.HasCards)
                        tags.push_back({ kCardIcon, ManifoldTheme::Muted });
                    if (state == RunState::Running)
                        tags.push_back(AnimatedTag("Idling", IdleAnimation::Mood::Idling, game.AppId));
                    else if (state == RunState::Starting)
                        tags.push_back(AnimatedTag("Starting", IdleAnimation::Mood::Starting, game.AppId));
                    else if (entry && entry->Resting && entry->IdledSeconds > 0.0)
                        tags.push_back(AnimatedTag("On a break", IdleAnimation::Mood::Sleeping, game.AppId));
                    else if (entry)
                        tags.push_back({ "queued", ManifoldTheme::Muted });

                    const RowAction action{ entry ? ICON_FA_MINUS : ICON_FA_PLUS, entry ? "Take out of the queue" : "Add to the queue", entry != nullptr };
                    bool actionClicked = false;
                    GameRowOptions options;
                    options.Action = &action;
                    options.ActionClicked = &actionClicked;
                    ImGui::PushID(static_cast<int>(game.AppId));
                    if (GameRow("##game", selected, Covers::Get(game.AppId, Covers::Size::Thumb), game.Name, selected ? ManifoldTheme::Text : ManifoldTheme::Label,
                        GameDetail(game), ManifoldTheme::Muted, tags, options))
                        ClickGame(model, game.AppId);
                    OpenGameOnDoubleClick(game.AppId);
                    if (actionClicked)
                    {
                        if (entry)
                            model.Remove(game.AppId);
                        else
                            model.AddToQueue(game.AppId);
                    }
                    GameMenu(model, game);
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();

            BeginRow("##actions");
            ImGui::BeginDisabled(g_view.empty());
            if (ActionButton(ICON_FA_PLUS, "Add all shown"))
                model.AddAllToQueue(g_view);
            ImGui::EndDisabled();
            ActionTooltip("Adds every game in the list above to the queue. Search and filters decide which those are.");
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        void SelectedGamesCard(IdlerModel& model)
        {
            BeginCard("Selected games");
            const std::vector<std::uint32_t> apps = SelectedGames(model);
            constexpr std::size_t kShownCovers = 4;
            std::vector<ImTextureID> covers;
            for (std::size_t index = 0; index < std::min(apps.size(), kShownCovers); ++index)
                covers.push_back(Covers::Get(apps[index], Covers::Size::Thumb));
            CoverStripRow("##covers", covers, static_cast<int>(apps.size() - covers.size()));

            std::uint32_t minutes = 0;
            int withCards = 0;
            for (const Game& game : model.Games())
            {
                if (!IsGameSelected(game.AppId))
                    continue;
                minutes += game.PlaytimeMinutes;
                withCards += game.HasCards ? 1 : 0;
            }
            const int inQueue = SelectedInList(model.Queue());
            InfoRow("Games", std::to_string(apps.size()), ManifoldTheme::Label);
            InfoRow("Playtime", PlaytimeText(minutes), ManifoldTheme::Label);
            InfoRow("With cards", std::to_string(withCards), ManifoldTheme::Label);
            InfoRow("In the queue", std::to_string(inQueue), ManifoldTheme::Label);

            BeginRow("##actions");
            ImGui::BeginDisabled(inQueue == static_cast<int>(apps.size()));
            if (ActionButton(ICON_FA_PLUS, "Add to queue"))
                model.AddGamesToList(model.ActiveList(), apps);
            ImGui::EndDisabled();
            if (ActionButton(ICON_FA_LIST, "Add to list"))
                ImGui::OpenPopup("##add_to_list");
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            if (ImGui::BeginPopup("##add_to_list"))
            {
                AddSelectedToListItems(model);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            if (inQueue > 0)
            {
                const std::string takeOut = "Take " + std::to_string(inQueue) + " out of the queue";
                if (ActionButton(ICON_FA_TIMES, takeOut, true))
                    TakeSelectedOut(model);
                ActionTooltip("Also with Delete.");
            }
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        void GameFactRows(IdlerModel& model, const Game& game);

        void SelectedGameCard(IdlerModel& model)
        {
            BeginCard("Selected game");
            const Game* found = model.FindGame(g_selectedGame);
            if (!found)
            {
                EmptyState("##none", 103u, "Pick a game in the list.");
                EndCard();
                return;
            }

            const Game game = *found;
            CoverRow("##cover", Covers::Get(game.AppId, Covers::Size::Full));
            InfoRow("Name", game.Name);
            GameFactRows(model, game);

            BeginRow("##actions");
            GameButtons(model, game.AppId, true);
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        void GameFactRows(IdlerModel& model, const Game& game)
        {
            InfoRow("App ID", std::to_string(game.AppId), ManifoldTheme::Label);
            InfoRow("Playtime", PlaytimeText(game.PlaytimeMinutes), ManifoldTheme::Label);
            if (game.LastPlayed != 0)
                InfoRow("Last played", DateText(game.LastPlayed), ManifoldTheme::Label);
            if (game.Purchased != 0)
                InfoRow("Purchased", DateText(game.Purchased), ManifoldTheme::Label);
            if (game.Released != 0)
                InfoRow("Released", DateText(game.Released), ManifoldTheme::Label);
            if (game.ReviewPercent != 0 || game.Metacritic != 0)
            {
                std::string rating;
                if (game.ReviewPercent != 0)
                    rating = std::to_string(game.ReviewPercent) + " % positive";
                if (game.Metacritic != 0)
                    rating += (rating.empty() ? "" : ", ") + std::string("Metacritic ") + std::to_string(game.Metacritic);
                InfoRow("Reviews", rating, ManifoldTheme::Label);
            }
            InfoRow("Trading cards", game.HasCards ? "Yes" : model.Cards().LastFullRead() != 0 ? "No" : "Not listed", ManifoldTheme::Label);
            if (game.AchievementsTotal > 0)
                InfoRow("Achievements", std::to_string(game.AchievementsDone) + " of " + std::to_string(game.AchievementsTotal), ManifoldTheme::Label);
            if (game.HasAchievements || game.AchievementsTotal > 0 || game.Unlock != UnlockRule::Unknown)
            {
                InfoRow("Unlocking", UnlockText(game.Unlock), game.Unlock == UnlockRule::Here || game.Unlock == UnlockRule::Mixed ? ManifoldTheme::Label : ManifoldTheme::Muted);
                ItemTooltip(UnlockTooltip(game.Unlock));
            }
            if (const std::string refund = RefundText(game); !refund.empty())
                InfoRow("Refund", refund, ManifoldTheme::Warning);
        }

        void LibraryPage(Shell& app)
        {
            IdlerModel& model = app.Model();
            const auto& games = model.Games();
            std::string status;
            if (model.Scanning() && games.empty())
            {
                status = "reading";
            }
            else
            {
                const auto cards = std::count_if(games.begin(), games.end(), [](const Game& game) { return game.HasCards; });
                status = std::to_string(games.size()) + " games, " + std::to_string(cards) + " with cards";
            }
            BeginPage(app, status);
            CardColumns("##library_columns",
                [&]() { LibraryCard(model); },
                [&]()
                {
                    if (g_gameSelection.size() > 1)
                        SelectedGamesCard(model);
                    else
                        SelectedGameCard(model);
                }, 0.58f, Px(kDetailColumnWidth));
            EndPageBody();
        }

        std::uint64_t g_achievementIconsSeen = 0;

        void AchievementsToolRow(IdlerModel& model, const std::vector<AchievementRow>& rows, bool writing)
        {
            std::size_t settable = 0;
            std::size_t hidden = 0;
            bool server = false;
            std::vector<std::string> locked;
            for (const AchievementRow& row : rows)
            {
                server |= row.Access == AchievementAccess::Server;
                if (row.Access != AchievementAccess::Here)
                    continue;
                ++settable;
                if (!row.Achieved)
                {
                    locked.push_back(row.Id);
                    hidden += row.Hidden ? 1 : 0;
                }
            }

            std::string text;
            ImVec4 color = ManifoldTheme::Muted;
            if (writing)
            {
                text = "Saving on Steam ...";
            }
            else if (!model.AchievementsNotice().empty())
            {
                text = model.AchievementsNotice();
                color = model.AchievementsNoticeIsProblem() ? ManifoldTheme::Error : ManifoldTheme::Label;
            }
            else if (settable == 0)
            {
                text = server ? "Only the game's own server can unlock these." : "Steam's file does not say who may unlock these.";
            }
            else
            {
                text = "Point at one to unlock or lock it.";
            }

            const ImGuiStyle& style = ImGui::GetStyle();
            const char* undoLabel = ICON_FA_UNDO " Undo";
            const char* allLabel = ICON_FA_UNLOCK " Unlock all";
            const bool undo = model.UndoableUnlocks() > 0;
            const bool all = !locked.empty();
            float buttons = 0.0f;
            if (undo)
                buttons += ImGui::CalcTextSize(undoLabel).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
            if (all)
                buttons += ImGui::CalcTextSize(allLabel).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = ImGui::GetFrameHeight();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(pos, ImVec2(std::max(pos.x, pos.x + width - buttons), pos.y + height), true);
            drawList->AddText(ImVec2(pos.x, std::round(pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f)), ImGui::GetColorU32(color), text.c_str());
            drawList->PopClipRect();
            ImGui::Dummy(ImVec2(std::max(0.0f, width - buttons), height));
            ErrorTooltip(text);

            ImGui::BeginDisabled(writing);
            if (undo)
            {
                ImGui::SameLine();
                if (ImGui::Button(undoLabel))
                    model.UndoUnlocks();
                ItemTooltip("Locks again what Unlock all just unlocked.");
            }
            if (all)
            {
                ImGui::SameLine();
                if (PositiveButton(allLabel))
                    ImGui::OpenPopup("##unlock_all");
            }
            ImGui::EndDisabled();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            if (ImGui::BeginPopup("##unlock_all"))
            {
                const bool one = locked.size() == 1;
                if (one)
                    ImGui::TextUnformatted(hidden > 0 ? "Unlock the last locked achievement? It is a hidden one." : "Unlock the last locked achievement?");
                else
                    ImGui::Text("Unlock the %d locked achievements?", static_cast<int>(locked.size()));
                if (!one && hidden > 0)
                    ImGui::TextDisabled("%d of them are hidden ones.", static_cast<int>(hidden));
                ImGui::TextDisabled(one ? "It appears on your profile with today's date." : "They appear on your profile with today's date.");
                ImGui::TextDisabled(one ? "Undo locks it again while this list is open." : "Undo locks them again while this list is open.");
                ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
                if (PositiveButton(ICON_FA_UNLOCK " Unlock all"))
                {
                    model.ChangeAchievements(locked, {}, true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
        }

        bool AchievementsRead(IdlerModel& model, std::uint32_t appId, const char* readLabel)
        {
            if (model.AchievementsPhase() == AchievementPhase::Idle && model.StateOf(appId) == RunState::Running)
                model.LoadAchievements(appId);
            const AchievementPhase phase = model.AchievementsPhase();
            if (phase == AchievementPhase::Idle)
            {
                ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);
                ImGui::TextUnformatted("Steam only gives achievements and stats to a running game, so reading them counts as a short start of this one: friends can see it for a moment, and its last played date may change.");
                ImGui::PopTextWrapPos();
                ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
                if (ImGui::Button(readLabel))
                    model.LoadAchievements(appId);
                return false;
            }
            if (phase == AchievementPhase::Busy)
            {
                ImGui::TextDisabled("Asking Steam ...");
                return false;
            }
            if (phase == AchievementPhase::Failed)
            {
                ImGui::TextColored(ManifoldTheme::Error, "%s", model.AchievementsProblem().c_str());
                ErrorTooltip(model.AchievementsProblem());
                if (ImGui::Button(ICON_FA_SYNC " Try again"))
                    model.LoadAchievements(appId);
                return false;
            }
            return true;
        }

        std::optional<std::pair<std::string, float>> ProgressOf(const IdlerModel& model, const AchievementRow& row)
        {
            if (row.Achieved || row.ProgressStat.empty() || row.ProgressMax <= row.ProgressMin)
                return std::nullopt;
            const StatRow* stat = model.FindStatRow(row.ProgressStat);
            if (!stat)
                return std::nullopt;
            const double value = std::clamp(stat->Value, row.ProgressMin, row.ProgressMax);
            const StatKind kind = stat->Schema.Kind;
            return std::make_pair(NumberText(value, kind) + " / " + NumberText(row.ProgressMax, kind),
                static_cast<float>((value - row.ProgressMin) / (row.ProgressMax - row.ProgressMin)));
        }

        void AchievementsSection(IdlerModel& model, std::uint32_t appId)
        {
            if (!AchievementsRead(model, appId, ICON_FA_TROPHY " Read the list"))
                return;
            const auto& rows = model.AchievementList();
            const AchievementPhase phase = model.AchievementsPhase();

            if (rows.empty())
            {
                ImGui::TextDisabled("This game has no achievements.");
            }
            else
            {
                const bool writing = phase == AchievementPhase::Writing;
                AchievementsToolRow(model, rows, writing);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::BeginChild("##achievement_list");
                ImGui::PopStyleColor();

                if (g_achievementIconsSeen != model.AchievementIconsGeneration())
                {
                    g_achievementIconsSeen = model.AchievementIconsGeneration();
                    Covers::RetryFiles();
                }

                const float line = ImGui::GetTextLineHeight();
                const float rowHeight = std::round(line * 2.0f + Px(14.0f));
                const float iconSize = rowHeight - std::round(Px(4.0f));
                const float width = ImGui::GetContentRegionAvail().x;
                const float button = ImGui::GetFrameHeight();
                const bool anySettable = std::any_of(rows.begin(), rows.end(), [](const AchievementRow& row) { return row.Access == AchievementAccess::Here; });
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(rows.size()), rowHeight + ImGui::GetStyle().ItemSpacing.y);
                while (clipper.Step())
                {
                    for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                    {
                        const AchievementRow& row = rows[static_cast<std::size_t>(index)];
                        const ImVec2 pos = ImGui::GetCursorScreenPos();
                        const ImVec2 rowMax(pos.x + width, pos.y + rowHeight);
                        const bool pointed = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, rowMax);
                        const bool settable = row.Access == AchievementAccess::Here;
                        ImGui::PushID(index);

                        const bool showButton = settable && pointed;
                        if (showButton)
                        {
                            ImGui::SetCursorScreenPos(ImVec2(rowMax.x - button - Px(2.0f), std::round(pos.y + (rowHeight - button) * 0.5f)));
                            ImGui::BeginDisabled(writing);
                            const bool achieved = row.Achieved;
                            if (achieved ? IconButton(ICON_FA_LOCK "##set", button, true) : PositiveIconButton(ICON_FA_UNLOCK "##set", button))
                            {
                                if (achieved)
                                    model.ChangeAchievements({}, { row.Id });
                                else
                                    model.ChangeAchievements({ row.Id }, {});
                            }
                            ImGui::EndDisabled();
                            ItemTooltip(achieved ? "Lock it again on Steam" : "Unlock it on Steam");
                            ImGui::SetCursorScreenPos(pos);
                        }
                        ImGui::Dummy(ImVec2(width, rowHeight));
                        ImGui::PopID();

                        const bool secret = row.Hidden && !row.Achieved;
                        const char* name = secret ? "Hidden achievement" : row.Name.c_str();
                        const char* description = secret ? "" : row.Description.c_str();

                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        const float rowTop = std::round(pos.y + (rowHeight - line * 2.0f - Px(2.0f)) * 0.5f);

                        const ImVec2 iconMin(pos.x + Px(2.0f), std::round(pos.y + (rowHeight - iconSize) * 0.5f));
                        const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
                        const ImTextureID icon = row.Icon.empty() ? ImTextureID{} : Covers::File(model.AchievementIconFile(row.Icon), static_cast<int>(iconSize), static_cast<int>(iconSize));
                        if (icon)
                        {
                            drawList->AddImageRounded(icon, iconMin, iconMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), row.Achieved ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 120), Px(4.0f));
                        }
                        else
                        {
                            drawList->AddRectFilled(iconMin, iconMax, ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(4.0f));
                            const char* sign = row.Achieved ? ICON_FA_CHECK : ICON_FA_LOCK;
                            const ImVec2 signSize = ImGui::CalcTextSize(sign);
                            drawList->AddText(ImVec2(std::round(iconMin.x + (iconSize - signSize.x) * 0.5f), std::round(iconMin.y + (iconSize - signSize.y) * 0.5f)), ImGui::GetColorU32(row.Achieved ? ManifoldTheme::Accent : ManifoldTheme::Muted), sign);
                        }
                        float right = pos.x + width - Px(4.0f);
                        if (showButton)
                        {
                            right -= button + Px(8.0f);
                        }
                        else if (row.Achieved && row.UnlockTime != 0)
                        {
                            const std::string date = DateText(row.UnlockTime);
                            right -= ImGui::CalcTextSize(date.c_str()).x;
                            drawList->AddText(ImVec2(right, rowTop), ImGui::GetColorU32(ManifoldTheme::Muted), date.c_str());
                            right -= Px(10.0f);
                        }
                        else if (const auto progress = (row.Hidden ? std::nullopt : ProgressOf(model, row)))
                        {
                            const float barWidth = std::max(ImGui::CalcTextSize(progress->first.c_str()).x, std::round(Px(64.0f)));
                            right -= barWidth;
                            const float textWidth = ImGui::CalcTextSize(progress->first.c_str()).x;
                            drawList->AddText(ImVec2(right + barWidth - textWidth, rowTop), ImGui::GetColorU32(ManifoldTheme::Label), progress->first.c_str());
                            const float barTop = std::round(rowTop + line + Px(6.0f));
                            const float barHeight = std::round(Px(4.0f));
                            drawList->AddRectFilled(ImVec2(right, barTop), ImVec2(right + barWidth, barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), barHeight);
                            if (progress->second > 0.0f)
                                drawList->AddRectFilled(ImVec2(right, barTop), ImVec2(right + std::max(barHeight, std::round(barWidth * progress->second)), barTop + barHeight),
                                    ImGui::GetColorU32(ManifoldTheme::Accent), barHeight);
                            right -= Px(10.0f);
                        }
                        const float textX = iconMax.x + Px(10.0f);
                        drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(std::max(textX, right), pos.y + rowHeight), true);
                        drawList->AddText(ImVec2(textX, rowTop), ImGui::GetColorU32(row.Achieved ? ManifoldTheme::Text : ManifoldTheme::Label), name);
                        drawList->AddText(ImVec2(textX, rowTop + line + Px(2.0f)), ImGui::GetColorU32(ManifoldTheme::Muted), description);
                        drawList->PopClipRect();

                        if (!settable && anySettable)
                            ItemTooltip(row.Access == AchievementAccess::Server ? "Only the game's own server can set this one." : "Steam's file does not say who may set this one.");
                        else if (description[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && textX + ImGui::CalcTextSize(description).x > right)
                            ItemTooltip(description);
                    }
                }
                clipper.End();
                ImGui::EndChild();
            }
        }

        std::string StatDetail(const StatRow& stat, int counting)
        {
            std::string detail = stat.Schema.Label.empty() ? std::string() : stat.Schema.Name;
            const auto add = [&](const std::string& part) { detail += (detail.empty() ? "" : "  \xc2\xb7  ") + part; };
            const StatKind kind = stat.Schema.Kind;
            if (stat.Schema.Min && stat.Schema.Max)
                add(NumberText(*stat.Schema.Min, kind) + " to " + NumberText(*stat.Schema.Max, kind));
            else if (stat.Schema.Min)
                add("at least " + NumberText(*stat.Schema.Min, kind));
            else if (stat.Schema.Max)
                add("at most " + NumberText(*stat.Schema.Max, kind));
            if (kind == StatKind::Float)
                add("decimal");
            if (stat.Schema.IncrementOnly)
                add("only counts up");
            if (counting > 0)
                add(counting == 1 ? "counts toward 1 achievement" : "counts toward " + std::to_string(counting) + " achievements");
            return detail;
        }

        void StartEditingStat(const StatRow& stat)
        {
            g_gameWindow.EditingStat = stat.Schema.Name;
            const std::string text = NumberText(stat.Value, stat.Schema.Kind);
            std::snprintf(g_gameWindow.StatText, sizeof(g_gameWindow.StatText), "%s", text.c_str());
            g_gameWindow.FocusStat = true;
        }

        void GameStatsSection(IdlerModel& model, std::uint32_t appId)
        {
            if (!AchievementsRead(model, appId, ICON_FA_LIST_OL " Read the stats"))
                return;
            const std::vector<StatRow>& stats = model.StatList();
            if (stats.empty())
            {
                ImGui::TextDisabled("This game keeps no stats on Steam.");
                return;
            }
            const bool writing = model.AchievementsPhase() == AchievementPhase::Writing;
            const bool anySettable = std::any_of(stats.begin(), stats.end(), [](const StatRow& row) { return row.Settable; });
            if (!model.FindStatRow(g_gameWindow.EditingStat))
                g_gameWindow.EditingStat.clear();

            {
                std::string text;
                ImVec4 color = ManifoldTheme::Muted;
                if (writing)
                {
                    text = "Saving on Steam ...";
                }
                else if (!model.AchievementsNotice().empty())
                {
                    text = model.AchievementsNotice();
                    color = model.AchievementsNoticeIsProblem() ? ManifoldTheme::Error : ManifoldTheme::Label;
                }
                else
                {
                    const bool server = std::any_of(stats.begin(), stats.end(), [](const StatRow& row) { return (row.Schema.Permission & 3u) != 0; });
                    text = anySettable ? "Click a value to change it." : server ? "Only the game's own server can set these." : "The game works these out by itself.";
                }
                const float height = ImGui::GetFrameHeight();
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, std::round(pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f)), ImGui::GetColorU32(color), text.c_str());
                ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, height));
                ErrorTooltip(text);
                ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##stat_list");
            ImGui::PopStyleColor();

            const auto& achievements = model.AchievementList();
            const float line = ImGui::GetTextLineHeight();
            const float rowHeight = std::round(line * 2.0f + Px(14.0f));
            const float width = ImGui::GetContentRegionAvail().x;
            const float button = ImGui::GetFrameHeight();
            const float inputWidth = std::round(Px(150.0f));
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(stats.size()), rowHeight + ImGui::GetStyle().ItemSpacing.y);
            if (!g_gameWindow.EditingStat.empty())
            {
                const auto editing = std::find_if(stats.begin(), stats.end(), [](const StatRow& row) { return row.Schema.Name == g_gameWindow.EditingStat; });
                clipper.IncludeItemByIndex(static_cast<int>(editing - stats.begin()));
            }
            std::optional<std::pair<std::string, double>> setTo;
            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                {
                    const StatRow& stat = stats[static_cast<std::size_t>(index)];
                    const StatKind kind = stat.Schema.Kind;
                    const ImVec2 pos = ImGui::GetCursorScreenPos();
                    const ImVec2 rowMax(pos.x + width, pos.y + rowHeight);
                    const bool editing = g_gameWindow.EditingStat == stat.Schema.Name;
                    const bool pointed = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, rowMax);
                    const float rowTop = std::round(pos.y + (rowHeight - line * 2.0f - Px(2.0f)) * 0.5f);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImGui::PushID(index);

                    int counting = 0;
                    for (const AchievementRow& achievement : achievements)
                        counting += achievement.ProgressStat == stat.Schema.Name ? 1 : 0;

                    std::string hint;
                    ImVec4 hintColor = ManifoldTheme::Muted;
                    float right = pos.x + width - Px(4.0f);
                    if (editing)
                    {
                        const float y = std::round(pos.y + (rowHeight - button) * 0.5f);
                        const float cancelX = rowMax.x - button - Px(2.0f);
                        const float setX = cancelX - button - Px(4.0f);
                        const float inputX = setX - inputWidth - Px(6.0f);
                        ImGui::BeginDisabled(writing);
                        ImGui::SetCursorScreenPos(ImVec2(inputX, y));
                        ImGui::SetNextItemWidth(inputWidth);
                        if (g_gameWindow.FocusStat)
                        {
                            ImGui::SetKeyboardFocusHere();
                            g_gameWindow.FocusStat = false;
                        }
                        const bool entered = ImGui::InputText("##value", g_gameWindow.StatText, sizeof(g_gameWindow.StatText),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsScientific);
                        const bool escaped = ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

                        const std::optional<double> value = ParseNumber(g_gameWindow.StatText);
                        std::string problem;
                        if (!value)
                            problem = Trim(g_gameWindow.StatText).empty() ? std::string() : std::string("That is no number.");
                        else
                            problem = StatChangeProblem(stat.Schema, stat.Value, *value);
                        const bool same = value && problem.empty() && NumberText(*value, kind) == NumberText(stat.Value, kind);
                        if (!problem.empty())
                        {
                            hint = problem;
                            hintColor = ManifoldTheme::Error;
                        }
                        else if (value && !same)
                        {
                            int unlocks = 0;
                            for (const AchievementRow& achievement : achievements)
                            {
                                if (!achievement.Achieved && achievement.ProgressStat == stat.Schema.Name && achievement.ProgressMax > achievement.ProgressMin && *value >= achievement.ProgressMax)
                                    ++unlocks;
                            }
                            if (unlocks > 0)
                            {
                                hint = unlocks == 1 ? "Steam unlocks 1 achievement with this." : "Steam unlocks " + std::to_string(unlocks) + " achievements with this.";
                                hintColor = ManifoldTheme::Accent;
                            }
                            else if (stat.Schema.IncrementOnly && *value > stat.Value)
                            {
                                hint = "It only counts up: it cannot go back to " + NumberText(stat.Value, kind) + " afterwards.";
                                hintColor = ManifoldTheme::Warning;
                            }
                            else
                            {
                                hint = "Enter sets it on Steam, Esc keeps " + NumberText(stat.Value, kind) + ".";
                            }
                        }
                        else
                        {
                            hint = "Enter sets it on Steam, Esc keeps " + NumberText(stat.Value, kind) + ".";
                        }

                        const bool ready = value && problem.empty() && !same;
                        ImGui::SetCursorScreenPos(ImVec2(setX, y));
                        ImGui::BeginDisabled(!ready);
                        const bool setClicked = PositiveIconButton(ICON_FA_CHECK "##set", button);
                        ImGui::EndDisabled();
                        ItemTooltip("Set it on Steam");
                        ImGui::SetCursorScreenPos(ImVec2(cancelX, y));
                        const bool cancelClicked = IconButton(ICON_FA_TIMES "##cancel", button);
                        ItemTooltip("Keep the value it has");
                        ImGui::EndDisabled();

                        if ((entered || setClicked) && ready)
                            setTo = std::make_pair(stat.Schema.Name, *value);
                        else if (escaped || cancelClicked)
                            g_gameWindow.EditingStat.clear();
                        if (entered || escaped)
                            ImGui::SetNavCursorVisible(false);
                        right = inputX - Px(10.0f);
                        ImGui::SetCursorScreenPos(pos);
                        ImGui::Dummy(ImVec2(width, rowHeight));
                    }
                    else
                    {
                        const std::string value = NumberText(stat.Value, kind);
                        const float valueWidth = ImGui::CalcTextSize(value.c_str()).x;
                        if (stat.Settable)
                        {
                            ImGui::BeginDisabled(writing);
                            if (ImGui::InvisibleButton("##row", ImVec2(width, rowHeight)))
                                StartEditingStat(stat);
                            ImGui::EndDisabled();
                            if (pointed && !writing)
                            {
                                const char* pen = ICON_FA_PEN;
                                const float penWidth = ImGui::CalcTextSize(pen).x;
                                const ImVec2 frameMin(right - valueWidth - penWidth - Px(18.0f), std::round(pos.y + (rowHeight - button) * 0.5f));
                                const ImVec2 frameMax(right + Px(4.0f), frameMin.y + button);
                                drawList->AddRectFilled(frameMin, frameMax, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
                                drawList->AddText(ImVec2(frameMin.x + Px(8.0f), std::round(frameMin.y + (button - line) * 0.5f)), ImGui::GetColorU32(ManifoldTheme::Muted), pen);
                            }
                        }
                        else
                        {
                            ImGui::Dummy(ImVec2(width, rowHeight));
                        }
                        const float valueY = std::round(pos.y + (rowHeight - line) * 0.5f);
                        drawList->AddText(ImVec2(right - valueWidth, valueY), ImGui::GetColorU32(stat.Settable ? ManifoldTheme::Text : ManifoldTheme::Label), value.c_str());
                        right -= valueWidth + Px(10.0f);
                        if (!stat.Settable)
                        {
                            const char* lock = ICON_FA_LOCK;
                            right -= ImGui::CalcTextSize(lock).x;
                            drawList->AddText(ImVec2(right, valueY), ImGui::GetColorU32(ManifoldTheme::Muted), lock);
                            right -= Px(10.0f);
                        }
                        else if (pointed && !writing)
                        {
                            right -= ImGui::CalcTextSize(ICON_FA_PEN).x + Px(14.0f);
                        }
                    }
                    ImGui::PopID();

                    const std::string label = stat.Schema.Label.empty() ? stat.Schema.Name : stat.Schema.Label;
                    const std::string detail = hint.empty() ? StatDetail(stat, counting) : hint;
                    const float textX = pos.x + Px(4.0f);
                    drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(std::max(textX, right), pos.y + rowHeight), true);
                    drawList->AddText(ImVec2(textX, rowTop), ImGui::GetColorU32(ManifoldTheme::Text), label.c_str());
                    drawList->AddText(ImVec2(textX, rowTop + line + Px(2.0f)), ImGui::GetColorU32(hintColor), detail.c_str());
                    drawList->PopClipRect();

                    if (!stat.Settable && anySettable && pointed)
                        ItemTooltip(kind == StatKind::AverageRate ? "The game works this one out by itself." : "Only the game's own server can set this one.");
                }
            }
            clipper.End();
            ImGui::EndChild();

            if (setTo)
            {
                model.SetGameStat(setTo->first, setTo->second);
                g_gameWindow.EditingStat.clear();
            }
        }

        void SaveListEditor(IdlerModel& model)
        {
            if (g_editor.IsNew)
                model.CreateList(g_editor.Name);
            else if (model.Lists()[static_cast<std::size_t>(model.ActiveList())].Name != g_editor.Name)
                model.RenameList(model.ActiveList(), g_editor.Name);

            const std::vector<QueueEntry> before = model.Queue();
            for (const QueueEntry& entry : before)
            {
                if (std::find(g_editor.Apps.begin(), g_editor.Apps.end(), entry.AppId) == g_editor.Apps.end())
                    model.Remove(entry.AppId);
            }
            for (const std::uint32_t appId : g_editor.Apps)
                model.AddToList(model.ActiveList(), appId);
            ClearSelection();
            model.SaveNow();
        }

        void ListEditorDialog(IdlerModel& model)
        {
            if (g_editor.WantsOpen)
            {
                g_editor.WantsOpen = false;
                g_editor.OpenedName = g_editor.Name;
                g_editor.OpenedApps = g_editor.Apps;
                ImGui::OpenPopup("##list_editor");
            }
            if (!ImGui::IsPopupOpen("##list_editor"))
                return;

            const std::string count = std::to_string(g_editor.Apps.size()) + (g_editor.Apps.size() == 1 ? " game" : " games");
            const bool unchanged = g_editor.OpenedName == g_editor.Name && g_editor.OpenedApps == g_editor.Apps;
            if (!BeginDialog("##list_editor", g_editor.IsNew ? ICON_FA_PLUS : ICON_FA_PEN, g_editor.IsNew ? "New list" : "Change list", Px(860.0f, 580.0f), count, nullptr, unchanged))
                return;

            const auto& games = model.Games();
            const auto& apps = g_editor.Apps;
            const auto holds = [&](std::uint32_t appId) { return std::find(apps.begin(), apps.end(), appId) != apps.end(); };
            const auto put = [&](std::uint32_t appId, bool in)
                {
                    if (in && !holds(appId))
                        g_editor.Apps.push_back(appId);
                    else if (!in)
                        std::erase(g_editor.Apps, appId);
                };

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Name");
            ImGui::SameLine(0.0f, Px(10.0f));
            ImGui::SetNextItemWidth(Px(300.0f));
            if (ImGui::IsWindowAppearing() && g_editor.IsNew)
                ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##name", "What this list is for", g_editor.Name, sizeof(g_editor.Name));
            if (!g_editor.Collections.empty())
            {
                ImGui::SameLine(0.0f, Px(18.0f));
                ImGui::TextDisabled("From Steam");
                ImGui::SameLine(0.0f, Px(10.0f));
                ImGui::SetNextItemWidth(Px(240.0f));
                if (ImGui::BeginCombo("##collection", "One of your collections"))
                {
                    int index = 0;
                    for (const SteamCollection& collection : g_editor.Collections)
                    {
                        ImGui::PushID(index++);
                        const std::string label = collection.Name + "  (" + std::to_string(collection.Apps.size()) + ")";
                        if (ImGui::Selectable(label.c_str()))
                        {
                            for (const std::uint32_t appId : collection.Apps)
                                put(appId, true);
                            if (g_editor.Name[0] == '\0')
                                std::snprintf(g_editor.Name, sizeof(g_editor.Name), "%s", collection.Name.c_str());
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ItemTooltip("Adds the games of a collection you have in Steam.");
            }
            ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

            const float footerButton = std::round(Px(30.0f));
            const float footer = footerButton + Px(14.0f);
            CardColumns("##editor_columns",
                [&]()
                {
                    BeginCard("Library");
                    BeginRow("##tools");
                    const float filterWidth = Px(110.0f);
                    ImGui::SetNextItemWidth(std::max(Px(60.0f), RowWidth() - filterWidth - ImGui::GetStyle().ItemSpacing.x));
                    FocusOnCtrlF(true);
                    g_editor.ViewStale |= ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " Search", g_editor.Search, sizeof(g_editor.Search));
                    ItemTooltip("Ctrl+F");
                    ImGui::SameLine();
                    g_editor.ViewStale |= ChoiceCombo("##filter", g_editor.Filter, LibraryFilter::Count, FilterName, filterWidth);
                    ImGui::PopID();
                    if (g_editor.ViewStale || g_editor.ViewGeneration != model.LibraryGeneration())
                    {
                        g_editor.View = LibraryView(games, g_editor.Search, g_editor.Filter, LibrarySort::Name, 0);
                        g_editor.ViewGeneration = model.LibraryGeneration();
                        g_editor.ViewStale = false;
                        g_editor.LastRow = -1;
                    }

                    BeginCardList("##library", CardRowsHeight(1) + footer);
                    if (g_editor.View.empty())
                        EmptyState("##nothing", 104u, "No game fits.");
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(g_editor.View.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
                    while (clipper.Step())
                    {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                        {
                            const Game& game = games[static_cast<std::size_t>(g_editor.View[static_cast<std::size_t>(row)])];
                            const bool in = holds(game.AppId);
                            const RowAction action{ in ? ICON_FA_MINUS : ICON_FA_PLUS, in ? "Take out of the list" : "Add to the list", in };
                            bool actionClicked = false;
                            GameRowOptions options;
                            options.Action = &action;
                            options.ActionClicked = &actionClicked;
                            std::vector<RowTag> tags;
                            if (in)
                                tags.push_back({ ICON_FA_CHECK, ManifoldTheme::Accent });
                            ImGui::PushID(static_cast<int>(game.AppId));
                            const bool clicked = GameRow("##game", in, Covers::Get(game.AppId, Covers::Size::Thumb), game.Name, in ? ManifoldTheme::Text : ManifoldTheme::Label, GameDetail(game), ManifoldTheme::Muted, tags, options);
                            if (clicked || actionClicked)
                            {
                                if (ImGui::GetIO().KeyShift && g_editor.LastRow >= 0)
                                {
                                    for (int other = std::min(row, g_editor.LastRow); other <= std::max(row, g_editor.LastRow); ++other)
                                        put(games[static_cast<std::size_t>(g_editor.View[static_cast<std::size_t>(other)])].AppId, !in);
                                }
                                else
                                {
                                    put(game.AppId, !in);
                                }
                                g_editor.LastRow = row;
                            }
                            ImGui::PopID();
                        }
                    }
                    clipper.End();
                    EndCardList();

                    BeginRow("##actions");
                    ImGui::BeginDisabled(g_editor.View.empty());
                    if (ImGui::Button(ICON_FA_PLUS " Add all shown"))
                    {
                        for (const int index : g_editor.View)
                            put(games[static_cast<std::size_t>(index)].AppId, true);
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    EndCard();
                },
                [&]()
                {
                    BeginCard("In this list");
                    BeginCardList("##holds", CardRowsHeight(1) + footer);
                    if (apps.empty())
                        EmptyState("##empty", 101u, "Click games on the left to add them.");
                    std::uint32_t takeOut = 0;
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(apps.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
                    while (clipper.Step())
                    {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                        {
                            const std::uint32_t appId = apps[static_cast<std::size_t>(row)];
                            const QueueEntry* entry = g_editor.IsNew ? nullptr : model.FindEntry(appId);
                            const std::string detail = entry && entry->IdledSeconds > 0.0 ? DurationText(static_cast<std::uint64_t>(entry->IdledSeconds)) + " idled" : "New";
                            const RowAction action{ ICON_FA_MINUS, "Take out of the list", true };
                            bool actionClicked = false;
                            GameRowOptions options;
                            options.Action = &action;
                            options.ActionClicked = &actionClicked;
                            ImGui::PushID(static_cast<int>(appId));
                            GameRow("##held", false, Covers::Get(appId, Covers::Size::Thumb), GameName(model, appId), ManifoldTheme::Label, detail, ManifoldTheme::Muted, {}, options);
                            if (actionClicked)
                                takeOut = appId;
                            ImGui::PopID();
                        }
                    }
                    clipper.End();
                    EndCardList();
                    if (takeOut != 0)
                        put(takeOut, false);

                    BeginRow("##actions");
                    ImGui::BeginDisabled(apps.empty());
                    if (DangerButton(ICON_FA_TIMES " Take all out"))
                        g_editor.Apps.clear();
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    EndCard();
                }, 0.55f);

            const bool named = !Trim(g_editor.Name).empty();
            const char* saveLabel = g_editor.IsNew ? ICON_FA_CHECK " Create list" : ICON_FA_CHECK " Save";
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            const float saveWidth = Px(130.0f);
            const float cancelWidth = ImGui::CalcTextSize("Cancel").x + padding.x * 2.0f;
            const float right = ImGui::GetWindowContentRegionMax().x;
            ImGui::SetCursorPos(ImVec2(right - saveWidth - cancelWidth - ImGui::GetStyle().ItemSpacing.x, ImGui::GetWindowContentRegionMax().y - footerButton));
            if (ImGui::Button("Cancel", ImVec2(cancelWidth, footerButton)))
                ImGui::CloseCurrentPopup();
            ImGui::SameLine();
            ImGui::BeginDisabled(!named);
            if (MainButton(saveLabel, saveWidth))
            {
                SaveListEditor(model);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        void WebDialog(Shell& app)
        {
            CardService& cards = app.Model().Cards();
            WebPanel& web = cards.Web();
            if (g_webWantsOpen)
            {
                g_webWantsOpen = false;
                ImGui::OpenPopup("##web");
                cards.OpenSignIn(app.NativeWindow(), g_webPreview);
            }
            g_webDialogOpen = ImGui::IsPopupOpen("##web");
            if (!g_webDialogOpen)
                return;

            const std::string host = HostOf(web.Address());
            const std::string note = host.empty() ? std::string() : std::string(IsHttps(web.Address()) ? ICON_FA_LOCK " " : "") + host;
            bool closed = false;
            if (!BeginDialog("##web", ICON_FA_SIGN_IN_ALT, "Sign in to Steam", Px(780.0f, 620.0f), note, &closed))
                return;

            if (g_webPreview)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Warning);
                ImGui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE " Only a preview. Do not sign in here.");
                ImGui::PopStyleColor();
            }

            const ImVec2 start = ImGui::GetCursorScreenPos();
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const WebPanel::State state = web.CurrentState();
            if (state == WebPanel::State::Failed)
            {
                Notice("##web_failed", ICON_FA_TIMES_CIRCLE, ManifoldTheme::Error, web.Error());
            }
            else if (state == WebPanel::State::Starting)
            {
                IdleAnimation::RequestFrames();
                EmptyState("##web_starting", 103u, "Opening Steam's page.");
            }
            web.SetBounds(WebBounds{ static_cast<long>(start.x), static_cast<long>(start.y), static_cast<long>(start.x + room.x), static_cast<long>(start.y + room.y) });
            web.SetZoom(app.Zoom());

            if (!closed && (web.TakeEscape() || (!g_webPreview && cards.State() == LoginState::SignedIn)))
            {
                closed = true;
                ImGui::CloseCurrentPopup();
            }
            if (closed)
                cards.CloseSignIn();
            ImGui::EndPopup();
        }

        void ShutdownDialog(IdlerModel& model)
        {
            const double left = model.ShutdownSecondsLeft();
            if (left >= 0.0 && !g_shutdownDialogOpen)
            {
                g_shutdownDialogOpen = true;
                ImGui::OpenPopup("##shutdown");
            }

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(18.0f, 16.0f));
            const bool open = ImGui::BeginPopupModal("##shutdown", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::PopStyleVar();
            if (!open)
                return;

            if (left < 0.0)
            {
                g_shutdownDialogOpen = false;
                ImGui::CloseCurrentPopup();
            }
            else
            {
                ImGui::TextColored(ManifoldTheme::Warning, "%s", ICON_FA_POWER_OFF);
                ImGui::SameLine(0.0f, Px(8.0f));
                ImGui::Text("The queue is finished. The computer shuts down in %d s.", static_cast<int>(std::ceil(left)));
                ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
                if (MainButton(ICON_FA_TIMES "  Cancel", ImGui::GetContentRegionAvail().x))
                {
                    model.CancelShutdown();
                    g_shutdownDialogOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        void LoginCard(IdlerModel& model)
        {
            CardService& cards = model.Cards();
            const LoginState state = cards.State();

            BeginCard("Steam login");
            if (state == LoginState::SignedOut)
            {
                Notice("##why", nullptr, ManifoldTheme::Label,
                    "Steam only says how many cards are left on your badge pages, and those need a login. You sign in on Steam's own page, and this program never sees your password.");
                if (const std::string problem = cards.Problem(); !problem.empty())
                    Notice("##problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, problem);
                BeginRow("##sign_in");
                if (MainButton(ICON_FA_SIGN_IN_ALT "  Sign in to Steam", RowWidth()))
                    g_webWantsOpen = true;
                ImGui::PopID();
            }
            else if (state == LoginState::Checking)
            {
                Notice("##checking", ICON_FA_HOURGLASS_HALF, ManifoldTheme::Muted, "Checking the login ...");
            }
            else
            {
                const std::string name = cards.AccountName();
                InfoRow("Account", name.empty() ? model.Persona() : name, ManifoldTheme::Label);
                const std::uint32_t read = cards.LastFullRead();
                InfoRow("Last read", cards.Reading() ? std::string("Reading now ...") : read != 0 ? DateText(read) + ", " + ClockText(static_cast<double>(read) - static_cast<double>(std::time(nullptr))) : std::string("Not yet"), ManifoldTheme::Muted);
                if (model.CardLoginIsOtherAccount())
                    Notice("##other", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, ErrorMessage(ErrorCode::OtherAccount));
                if (const std::string problem = cards.Problem(); !problem.empty())
                    Notice("##problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, problem);
                BeginRow("##actions");
                ImGui::BeginDisabled(cards.Reading());
                if (ActionButton(ICON_FA_SYNC, "Read again"))
                    cards.ReadAll();
                ImGui::EndDisabled();
                if (ActionButton(ICON_FA_SIGN_OUT_ALT, "Sign out", true))
                    cards.SignOut();
                ActionTooltip("Removes the login from this computer. Steam lists it under your authorized devices until you remove it there.");
                EndActionButtons();
                ImGui::PopID();
            }
            EndCard();
        }

        void FarmCard(IdlerModel& model)
        {
            Settings& options = model.Options();
            std::uint32_t total = 0;
            for (const CardRow& row : g_cardRows)
                total += row.Drops;
            const bool farming = model.Active() && model.QueueFarmsCards();

            BeginCard("Farming");
            BeginRow("##farm");
            ImGui::BeginDisabled(!farming && (g_cardRows.empty() || model.CardLoginIsOtherAccount()));
            if (MainButton(farming ? ICON_FA_STOP "  Stop" : ICON_FA_PLAY "  Farm all", RowWidth()))
            {
                if (farming)
                    model.SetActive(false);
                else
                    model.FarmCards();
            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ItemTooltip("Puts every game with drops left into the list \"Cards\" and starts it. A game leaves when its last card dropped.");

            InfoRow("Drops left", std::to_string(total) + " in " + std::to_string(g_cardRows.size()) + (g_cardRows.size() == 1 ? " game" : " games"), ManifoldTheme::Label);
            InfoRow("Dropped", std::to_string(model.CardsDropped()) + " since the start", ManifoldTheme::Label);
            ControlRow("Warm-up", "Games with this much playtime run first, one at a time. The others idle together until they get there, because some accounts only get drops after a few hours.", Px(110.0f), [&]()
                {
                    const float step = 0.5f;
                    if (ImGui::InputScalar("##warmup", ImGuiDataType_Float, &options.CardWarmupHours, &step, nullptr, "%.1f h"))
                    {
                        options.CardWarmupHours = std::clamp(options.CardWarmupHours, 0.0f, 24.0f);
                        model.SaveNow();
                    }
                });
            EndCard();
        }

        void DropsCard(IdlerModel& model)
        {
            BeginCard("Games with drops");
            BeginCardList("##drops", 0.0f);
            const LoginState state = model.Cards().State();
            if (g_cardRows.empty())
            {
                const char* text = state != LoginState::SignedIn ? "Shows after you signed in." : model.Cards().Reading() || model.Cards().LastFullRead() == 0 ? "Reading your badge pages ..." : "No game has drops left.";
                Notice("##none", nullptr, ManifoldTheme::Muted, text);
            }

            const double warmup = static_cast<double>(model.Options().CardWarmupHours) * 3600.0;
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_cardRows.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                {
                    const CardRow& row = g_cardRows[static_cast<std::size_t>(index)];
                    const Game* game = model.FindGame(row.AppId);
                    const QueueEntry* entry = model.FindEntry(row.AppId);
                    const double played = std::max(game ? static_cast<double>(game->PlaytimeMinutes) * 60.0 : 0.0, entry ? PlayedSeconds(*entry) : 0.0);
                    std::string detail = std::to_string(row.Drops) + (row.Drops == 1 ? " drop left, " : " drops left, ") + (played > 0.0 ? DurationText(static_cast<std::uint64_t>(played)) + " played" : std::string("not played"));

                    std::vector<RowTag> tags;
                    if (entry && IsHeld(*entry))
                        tags.push_back({ "refund", ManifoldTheme::Warning });
                    else if (model.StateOf(row.AppId) == RunState::Running)
                    {
                        if (entry && PlayedSeconds(*entry) < warmup)
                            tags.push_back({ "warm-up", ManifoldTheme::Muted });
                        tags.push_back(AnimatedTag("Idling", IdleAnimation::Mood::Idling, row.AppId));
                    }

                    ImGui::PushID(static_cast<int>(row.AppId));
                    GameRow("##game", false, Covers::Get(row.AppId, Covers::Size::Thumb), GameName(model, row.AppId), ManifoldTheme::Label, detail, ManifoldTheme::Muted, tags);
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void DrawReceivedCard(const IdlerModel& model, const ReceivedCard& card, float width, float height)
        {
            const float rounding = Px(3.0f);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + width, min.y + height);
            ImGui::InvisibleButton("##card", ImVec2(width, height));
            const bool hovered = ImGui::IsItemHovered();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const std::filesystem::path file = model.Cards().PictureFile(card.Picture);
            if (const ImTextureID picture = Covers::File(file, kCardThumbWidth, kCardThumbHeight))
            {
                drawList->AddImageRounded(picture, min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32_WHITE, rounding);
            }
            else
            {
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::Sidebar), rounding);
                const char* icon = card.Picture.empty() ? ICON_FA_STAR : ICON_FA_CLONE;
                const ImVec2 size = ImGui::CalcTextSize(icon);
                drawList->AddText(ImVec2(std::round((min.x + max.x - size.x) * 0.5f), std::round((min.y + max.y - size.y) * 0.5f)),
                    ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Muted, 0.6f)), icon);
            }

            if (static_cast<double>(std::time(nullptr)) - static_cast<double>(card.Time) < kFreshCardSeconds)
            {
                DrawIdleHighlight(drawList, min, max, 0.8f, card.Time ^ card.AppId);
                IdleAnimation::RequestFrames();
            }
            if (hovered)
            {
                drawList->AddRect(min, max, ImGui::GetColorU32(ManifoldTheme::Accent), rounding, 0, Px(1.5f));
                ShowTooltip([&]()
                    {
                        const float previewWidth = std::round(Px(168.0f));
                        const ImVec2 preview(previewWidth, std::round(previewWidth * static_cast<float>(kCardFullHeight) / static_cast<float>(kCardFullWidth)));
                        if (const ImTextureID picture = Covers::File(file, kCardFullWidth, kCardFullHeight))
                            ImGui::Image(picture, preview);
                        else if (!card.Picture.empty())
                            ImGui::Dummy(preview);
                        ImGui::TextUnformatted(card.Name.empty() ? "A foil card, or one already gone" : card.Name.c_str());
                        ImGui::TextDisabled("%s", GameName(model, card.AppId).c_str());
                        ImGui::TextDisabled("%s", (DateText(card.Time) + ", " + ClockText(static_cast<double>(card.Time) - static_cast<double>(std::time(nullptr)))).c_str());
                    });
            }
        }

        void ReceivedCards(const IdlerModel& model)
        {
            BeginCard("Received cards");
            BeginCardList("##received", 0.0f);
            if (g_received.empty())
                Notice("##none", nullptr, ManifoldTheme::Muted, "Cards show here when they drop.");

            const float gap = Px(6.0f);
            const float width = std::floor((RowWidth() - gap * static_cast<float>(kCardColumns - 1)) / static_cast<float>(kCardColumns));
            const float height = std::round(width * static_cast<float>(kCardFullHeight) / static_cast<float>(kCardFullWidth));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
            ImGuiListClipper clipper;
            clipper.Begin((static_cast<int>(g_received.size()) + kCardColumns - 1) / kCardColumns, height + gap);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    for (int column = 0; column < kCardColumns; ++column)
                    {
                        const int index = row * kCardColumns + column;
                        if (index >= static_cast<int>(g_received.size()))
                            break;
                        if (column > 0)
                            ImGui::SameLine();
                        ImGui::PushID(index);
                        DrawReceivedCard(model, g_received[static_cast<std::size_t>(index)], width, height);
                        ImGui::PopID();
                    }
                }
            }
            clipper.End();
            ImGui::PopStyleVar();
            EndCardList();
            EndCard();
        }

        void CardsPage(Shell& app)
        {
            IdlerModel& model = app.Model();
            const CardService& cards = model.Cards();
            if (g_receivedGeneration != cards.ReceivedGeneration() || g_receivedAccount != model.SteamId())
            {
                g_receivedGeneration = cards.ReceivedGeneration();
                g_receivedAccount = model.SteamId();
                g_received.clear();
                for (const ReceivedCard& card : cards.Received())
                {
                    if (card.Account == g_receivedAccount)
                        g_received.push_back(card);
                }
                std::reverse(g_received.begin(), g_received.end());
            }
            if (g_picturesGeneration != cards.PicturesGeneration())
            {
                g_picturesGeneration = cards.PicturesGeneration();
                Covers::RetryFiles();
            }
            if (g_cardRowsGeneration != cards.Generation() || g_cardRowsLibrary != model.LibraryGeneration())
            {
                g_cardRowsGeneration = cards.Generation();
                g_cardRowsLibrary = model.LibraryGeneration();
                g_cardRows.clear();
                for (const auto& [appId, drops] : cards.Drops())
                {
                    if (drops > 0 && model.FindGame(appId))
                        g_cardRows.push_back({ appId, drops });
                }
                std::sort(g_cardRows.begin(), g_cardRows.end(), [&](const CardRow& lhs, const CardRow& rhs)
                    {
                        if (lhs.Drops != rhs.Drops)
                            return lhs.Drops > rhs.Drops;
                        return lhs.AppId < rhs.AppId;
                    });
            }

            std::string status = "not signed in";
            if (cards.State() == LoginState::SignedIn)
                status = cards.Reading() ? "reading" : std::to_string(g_cardRows.size()) + (g_cardRows.size() == 1 ? " game with drops" : " games with drops");
            BeginPage(app, status);
            CardColumns("##cards_columns",
                [&]()
                {
                    LoginCard(model);
                    const LoginState state = cards.State();
                    if (state == LoginState::SignedIn)
                        FarmCard(model);
                    if (state == LoginState::SignedIn || (state == LoginState::SignedOut && !g_received.empty()))
                        ReceivedCards(model);
                },
                [&]() { DropsCard(model); }, 0.46f);
            EndPageBody();
        }

        void RefreshStats(const IdlerModel& model, std::int32_t today)
        {
            const double now = ImGui::GetTime();
            if (now < g_statsRefreshAt)
                return;
            g_statsRefreshAt = now + 1.0;

            const IdleStats& stats = model.Stats();
            const std::uint32_t account = model.Account();
            g_statsCalendar = stats.PerDay(account, today, kCalendarDays);
            g_statsWeek = stats.Total(account, today - 6, today);
            g_statsWeekBefore = stats.Total(account, today - 13, today - 7);
            g_statsSummary = stats.Summary(account, today);
            g_statsHours = stats.PerHour(account);
            g_statsGames = stats.ByGame(account, g_statsDay);
            g_statsHistory = stats.History(account, g_statsDay);

            g_statsFigures = stats.Figures(account);
            g_statsTop.assign(g_statsFigures.begin(), g_statsFigures.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(3, g_statsFigures.size())));
            SortFigures(g_statsFigures, g_statsOrder);
            if (std::none_of(g_statsFigures.begin(), g_statsFigures.end(), [](const GameFigures& game) { return game.AppId == g_statsGame; }))
                g_statsGame = g_statsFigures.empty() ? 0u : g_statsFigures.front().AppId;
            g_statsGameDays = stats.PerDay(account, g_statsGame, today, kChartDays);
        }

        std::string SecondsText(double seconds)
        {
            return DurationText(static_cast<std::uint64_t>(seconds));
        }

        std::string ShortSecondsText(double seconds)
        {
            return ShortDurationText(static_cast<std::uint64_t>(seconds));
        }

        std::string RoughSecondsText(double seconds)
        {
            return seconds < 3600.0 ? SecondsText(seconds) : std::to_string(static_cast<std::uint64_t>(std::round(seconds / 3600.0))) + " h";
        }

        void OpenStatsDay(std::int32_t day)
        {
            g_statsDay = day;
            g_statsTab = StatsTab::History;
            g_statsRefreshAt = -1.0;
        }

        void OpenStatsGame(std::uint32_t appId)
        {
            g_statsGame = appId;
            g_statsTab = StatsTab::Games;
            g_statsRefreshAt = -1.0;
        }

        struct CardRowHeight
        {
            float Wanted = 0.0f;
            float Next = 0.0f;
        };
        CardRowHeight g_statsTopRow;
        CardRowHeight g_statsMiddleRow;
        CardRowHeight g_statsLowRow;

        void FillCard(CardRowHeight& row, float top)
        {
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float used = cursor.y - top;
            row.Next = std::max(row.Next, used);
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, top + std::max(row.Wanted, used)));
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }

        void EndCardRow(CardRowHeight& row)
        {
            row.Wanted = std::exchange(row.Next, 0.0f);
        }

        bool StatsPill(const char* label, bool active)
        {
            const ImVec2 text = ImGui::CalcTextSize(label);
            const ImVec2 size(std::round(text.x + Px(20.0f)), ImGui::GetFrameHeight());
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(label, size);
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (active || hovered)
                drawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), active ? ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.16f)) : IM_COL32(255, 255, 255, 12), Px(5.0f));
            drawList->AddText(ImVec2(std::round(min.x + (size.x - text.x) * 0.5f), std::round(min.y + (size.y - text.y) * 0.5f)),
                ImGui::GetColorU32(active ? ManifoldTheme::Accent : hovered ? ManifoldTheme::Text : ManifoldTheme::Label), label);
            return clicked;
        }

        float PillWidth(const char* label)
        {
            return std::round(ImGui::CalcTextSize(label).x + Px(20.0f));
        }

        void GoToTitleLine(float width, float height)
        {
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(cursor.x + RowWidth() - width, std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetTextLineHeight() + height) * 0.5f)));
        }

        void StatTile(const char* icon, const char* label, const std::string& value, const std::string& note, const char* tooltip, float width, float height)
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + width, min.y + height);
            ImGui::Dummy(ImVec2(width, height));
            if (tooltip)
                ItemTooltip(tooltip);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::Card), Px(6.0f));
            drawList->AddRect(min, max, ImGui::GetColorU32(ManifoldTheme::CardBorder), Px(6.0f));

            const float left = min.x + Px(11.0f);
            const float right = max.x - Px(8.0f);
            const float line = ImGui::GetTextLineHeight();
            float y = min.y + Px(8.0f);
            drawList->AddText(ImVec2(left, y), ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.85f)), icon);
            DrawCutText(drawList, ImVec2(left + std::round(line * 1.45f), y), right, ManifoldTheme::Muted, label);
            y += line + Px(3.0f);
            const float size = std::round(ImGui::GetFontSize() * 1.5f);
            drawList->AddText(ImGui::GetFont(), size, ImVec2(left, y), ImGui::GetColorU32(ManifoldTheme::Text), value.c_str());
            y += size + Px(3.0f);
            DrawCutText(drawList, ImVec2(left, y), right, ManifoldTheme::Muted, note);
        }

        void StatTiles()
        {
            const StatsSummary& summary = g_statsSummary;
            const double today = g_statsCalendar.back();
            const double yesterday = g_statsCalendar[g_statsCalendar.size() - 2];

            std::string trend = g_statsWeek > 0.0 ? "Nothing before" : "";
            if (g_statsWeekBefore > 0.0)
            {
                const auto percent = static_cast<int>(std::round((g_statsWeek / g_statsWeekBefore - 1.0) * 100.0));
                trend = (percent >= 0 ? "+" : "") + std::to_string(percent) + " %";
            }
            static char sessionsNote[80];
            std::snprintf(sessionsNote, sizeof(sessionsNote), "One start of a game. Counted since %s.", summary.FirstSessionDay != 0 ? DayText(summary.FirstSessionDay).c_str() : "the first one");

            constexpr int kTiles = 6;
            const float gap = Px(8.0f);
            const float space = ImGui::GetContentRegionAvail().x;
            const int perRow = (space - gap * (kTiles - 1)) / kTiles >= Px(112.0f) ? kTiles : kTiles / 2;
            const float width = std::floor((space - gap * static_cast<float>(perRow - 1)) / static_cast<float>(perRow));
            const float height = std::round(Px(16.0f) + ImGui::GetTextLineHeight() * 2.0f + Px(6.0f) + ImGui::GetFontSize() * 1.5f);
            int index = 0;
            const auto tile = [&](const char* icon, const char* label, const std::string& value, const std::string& note, const char* tooltip)
                {
                    if (index++ % perRow != 0)
                        ImGui::SameLine(0.0f, gap);
                    StatTile(icon, label, value, note, tooltip, width, height);
                };
            tile(ICON_FA_CALENDAR_DAY, "Today", ShortSecondsText(today), "Yesterday " + RoughSecondsText(yesterday), nullptr);
            tile(ICON_FA_CALENDAR_ALT, "Last 7 days", ShortSecondsText(g_statsWeek), trend, "Against the 7 days before.");
            tile(ICON_FA_HOURGLASS_HALF, "All time", ShortSecondsText(summary.Seconds), summary.DaysIdled > 0 ? "Since " + DayText(summary.FirstDay) : std::string("Nothing yet"), nullptr);
            tile(ICON_FA_GAMEPAD, "Games", std::to_string(summary.Games), "On " + std::to_string(summary.DaysIdled) + (summary.DaysIdled == 1 ? " day" : " days"), nullptr);
            tile(ICON_FA_REDO, "Sessions", std::to_string(summary.Sessions), summary.LongestSession > 0.0 ? "Longest " + RoughSecondsText(summary.LongestSession) : std::string("None yet"), sessionsNote);
            tile(ICON_FA_FIRE, "Days in a row", std::to_string(summary.CurrentStreak), "Best " + std::to_string(summary.LongestStreak), nullptr);
        }

        double ChartTop(double seconds)
        {
            const double hours = seconds / 3600.0;
            double top = 0.5;
            for (const double step : { 0.5, 1.0, 2.0, 4.0, 10.0, 20.0, 40.0, 100.0, 200.0, 400.0, 1000.0, 2000.0, 4000.0, 10000.0 })
            {
                top = step;
                if (step >= hours)
                    break;
            }
            return top;
        }

        std::string AxisText(double hours)
        {
            return hours < 1.0 ? std::to_string(static_cast<int>(std::round(hours * 60.0))) + " min" : HoursText(static_cast<float>(hours));
        }

        struct BarChartInput
        {
            const std::vector<double>& Seconds;
            float Height = 0.0f;
            std::function<std::string(int)> Label{};
            std::function<void(int)> Tooltip{};
            std::function<void(int)> Clicked{};
            int Selected = -1;
            const std::vector<double>* Line = nullptr;
            const char* Empty = "Nothing idled yet.";
        };

        void BarChart(const char* id, const BarChartInput& chart)
        {
            BeginRow(id);
            const float width = RowWidth();
            const float line = ImGui::GetTextLineHeight();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 plotMin(pos.x + Px(40.0f), pos.y + Px(4.0f));
            const ImVec2 plotMax(pos.x + width, pos.y + chart.Height - line - Px(5.0f));
            const float plotHeight = plotMax.y - plotMin.y;
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const int count = static_cast<int>(chart.Seconds.size());
            const double most = count > 0 ? *std::max_element(chart.Seconds.begin(), chart.Seconds.end()) : 0.0;
            const double top = ChartTop(most);
            for (const double part : { 0.0, 0.5, 1.0 })
            {
                const float y = std::round(plotMax.y - plotHeight * static_cast<float>(part));
                drawList->AddLine(ImVec2(plotMin.x, y), ImVec2(plotMax.x, y), ImGui::GetColorU32(ManifoldTheme::RowSeparator));
                const std::string label = part == 0.0 ? std::string("0") : AxisText(top * part);
                const ImVec2 size = ImGui::CalcTextSize(label.c_str());
                drawList->AddText(ImVec2(plotMin.x - size.x - Px(6.0f), y - size.y * 0.5f), ImGui::GetColorU32(ManifoldTheme::Muted), label.c_str());
            }

            const float slot = (plotMax.x - plotMin.x) / static_cast<float>(std::max(count, 1));
            const float barWidth = std::max(1.0f, std::round(std::min(slot - Px(2.0f), Px(24.0f))));
            const float rounding = std::min(Px(4.0f), barWidth * 0.5f);
            const float toY = plotHeight / static_cast<float>(top * 3600.0);
            int hoveredBar = -1;
            for (int index = 0; index < count; ++index)
            {
                const double seconds = chart.Seconds[static_cast<std::size_t>(index)];
                const float x = plotMin.x + slot * static_cast<float>(index);

                ImGui::SetCursorScreenPos(ImVec2(x, plotMin.y));
                ImGui::PushID(index);
                const bool clicked = ImGui::InvisibleButton("##bar", ImVec2(std::max(slot, 1.0f), std::max(plotHeight, 1.0f)));
                ImGui::PopID();
                if (clicked && chart.Clicked)
                    chart.Clicked(index);
                const bool hovered = ImGui::IsItemHovered();
                if (hovered)
                {
                    hoveredBar = index;
                    drawList->AddRectFilled(ImVec2(x, plotMin.y), ImVec2(x + slot, plotMax.y), IM_COL32(255, 255, 255, 9));
                }

                if (seconds > 0.0)
                {
                    const float barHeight = std::max(Px(2.0f), static_cast<float>(seconds) * toY);
                    const float left = std::round(x + (slot - barWidth) * 0.5f);
                    const bool dimmed = chart.Selected >= 0 && chart.Selected != index;
                    const ImVec4 color = hovered ? ManifoldTheme::AccentHovered : dimmed ? ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.4f) : ManifoldTheme::Accent;
                    drawList->AddRectFilled(ImVec2(left, plotMax.y - barHeight), ImVec2(left + barWidth, plotMax.y), ImGui::GetColorU32(color), std::min(rounding, barHeight), ImDrawFlags_RoundCornersTop);
                }

                const std::string label = chart.Label ? chart.Label(index) : std::string();
                if (!label.empty())
                {
                    const ImVec2 size = ImGui::CalcTextSize(label.c_str());
                    const float labelX = std::clamp(x + (slot - size.x) * 0.5f, plotMin.x, std::max(plotMin.x, plotMax.x - size.x));
                    drawList->AddText(ImVec2(labelX, plotMax.y + Px(4.0f)), ImGui::GetColorU32(ManifoldTheme::Muted), label.c_str());
                }
            }

            if (chart.Line && static_cast<int>(chart.Line->size()) == count && count > 1 && most > 0.0)
            {
                std::vector<ImVec2> points;
                points.reserve(chart.Line->size());
                for (int index = 0; index < count; ++index)
                    points.emplace_back(plotMin.x + slot * (static_cast<float>(index) + 0.5f), plotMax.y - static_cast<float>((*chart.Line)[static_cast<std::size_t>(index)]) * toY);
                drawList->AddPolyline(points.data(), count, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.75f)), ImDrawFlags_None, std::max(1.0f, Px(1.5f)));
            }

            if (hoveredBar >= 0 && chart.Tooltip)
                ShowTooltip([&]() { chart.Tooltip(hoveredBar); });

            if (most <= 0.0 && chart.Empty)
            {
                const ImVec2 size = ImGui::CalcTextSize(chart.Empty);
                drawList->AddText(ImVec2((plotMin.x + plotMax.x - size.x) * 0.5f, (plotMin.y + plotMax.y - size.y) * 0.5f), ImGui::GetColorU32(ManifoldTheme::Muted), chart.Empty);
            }

            ImGui::SetCursorScreenPos(pos);
            ImGui::Dummy(ImVec2(width, chart.Height));
            ImGui::PopID();
        }

        std::vector<double> WeekdaySeconds()
        {
            return { g_statsSummary.Weekdays.begin(), g_statsSummary.Weekdays.end() };
        }

        void WeekdayChart(const char* id, float height)
        {
            static constexpr const char* kDays[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
            const std::vector<double> seconds = WeekdaySeconds();
            BarChartInput chart{ .Seconds = seconds };
            chart.Height = height;
            chart.Label = [](int index) { return std::string(kDays[index]); };
            chart.Tooltip = [&](int index)
                {
                    const double value = seconds[static_cast<std::size_t>(index)];
                    ImGui::TextUnformatted(SecondsText(value).c_str());
                    ImGui::TextDisabled("%s, %d %% of all", kDays[index], g_statsSummary.Seconds > 0.0 ? static_cast<int>(std::round(value / g_statsSummary.Seconds * 100.0)) : 0);
                };
            BarChart(id, chart);
        }

        void HourChart(const char* id, float height)
        {
            const std::vector<double> seconds(g_statsHours.begin(), g_statsHours.end());
            const auto hourText = [](int hour)
                {
                    char text[8];
                    std::snprintf(text, sizeof(text), "%02d:00", hour);
                    return std::string(text);
                };
            BarChartInput chart{ .Seconds = seconds };
            chart.Height = height;
            chart.Empty = "Fills up while games idle.";
            chart.Label = [&](int index) { return index % 6 == 0 ? hourText(index) : std::string(); };
            chart.Tooltip = [&](int index)
                {
                    ImGui::TextUnformatted(SecondsText(seconds[static_cast<std::size_t>(index)]).c_str());
                    ImGui::TextDisabled("%s to %s", hourText(index).c_str(), hourText((index + 1) % 24).c_str());
                };
            BarChart(id, chart);
        }

        void DaysChart(std::int32_t today, float height, int days)
        {
            const std::size_t first = g_statsCalendar.size() - static_cast<std::size_t>(days);
            const std::vector<double> seconds(g_statsCalendar.begin() + static_cast<std::ptrdiff_t>(first), g_statsCalendar.end());
            std::vector<double> average(seconds.size(), 0.0);
            for (std::size_t index = 0; index < average.size(); ++index)
            {
                double sum = 0.0;
                for (std::size_t back = 0; back < 7; ++back)
                    sum += g_statsCalendar[first + index - back];
                average[index] = sum / 7.0;
            }

            const std::int32_t firstDay = today - days + 1;
            BarChartInput chart{ .Seconds = seconds };
            chart.Height = height;
            chart.Line = &average;
            chart.Label = [=](int index) -> std::string
                {
                    if ((days - 1 - index) % (days > 30 ? 21 : 7) != 0)
                        return {};
                    const std::int32_t day = firstDay + index;
                    return day == today ? std::string("Today") : DayText(day).substr(5);
                };
            chart.Tooltip = [&](int index)
                {
                    ImGui::TextUnformatted(SecondsText(seconds[static_cast<std::size_t>(index)]).c_str());
                    ImGui::TextDisabled("%s", DayText(firstDay + index).c_str());
                    ImGui::TextDisabled("7-day average %s", SecondsText(average[static_cast<std::size_t>(index)]).c_str());
                };
            chart.Clicked = [=](int index) { OpenStatsDay(firstDay + index); };
            if (g_statsDay && *g_statsDay >= firstDay && *g_statsDay <= today)
                chart.Selected = *g_statsDay - firstDay;
            BarChart("##days", chart);
        }

        void HoursCard(std::int32_t today, float height, bool withTimes)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Hours idled");
            if (!withTimes && (g_statsChart == StatsChart::Weekdays || g_statsChart == StatsChart::Hours))
                g_statsChart = StatsChart::Days30;

            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                struct Choice
                {
                    const char* Label;
                    StatsChart Chart;
                };
                static constexpr Choice kChoices[] = { { "30 days", StatsChart::Days30 }, { "90 days", StatsChart::Days90 }, { "Weekdays", StatsChart::Weekdays }, { "Hours", StatsChart::Hours } };
                const int shown = withTimes ? 4 : 2;
                const float gap = Px(2.0f);
                float pills = -gap;
                for (int index = 0; index < shown; ++index)
                    pills += PillWidth(kChoices[index].Label) + gap;
                const float button = ImGui::GetFrameHeight();
                const float room = RowWidth();
                GoToTitleLine(pills, button);
                const ImVec2 at = ImGui::GetCursorScreenPos();
                for (int index = 0; index < shown; ++index)
                {
                    if (index > 0)
                        ImGui::SameLine(0.0f, gap);
                    if (StatsPill(kChoices[index].Label, g_statsChart == kChoices[index].Chart))
                        g_statsChart = kChoices[index].Chart;
                }

                constexpr const char* kLegend = "7-day average";
                const ImVec2 size = ImGui::CalcTextSize(kLegend);
                const bool days = g_statsChart == StatsChart::Days30 || g_statsChart == StatsChart::Days90;
                if (days && room - pills - ImGui::CalcTextSize("Hours idled").x > size.x + Px(48.0f))
                {
                    const float textX = at.x - size.x - Px(12.0f);
                    const float y = std::round(at.y + (button - size.y) * 0.5f);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddText(ImVec2(textX, y), ImGui::GetColorU32(ManifoldTheme::Muted), kLegend);
                    const float lineY = std::round(y + size.y * 0.5f);
                    drawList->AddLine(ImVec2(textX - Px(20.0f), lineY), ImVec2(textX - Px(5.0f), lineY), ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.75f)), std::max(1.0f, Px(1.5f)));
                }
                ImGui::SetCursorScreenPos(cursor);
            }

            switch (g_statsChart)
            {
            case StatsChart::Weekdays: WeekdayChart("##weekdays", height); break;
            case StatsChart::Hours: HourChart("##hours", height); break;
            case StatsChart::Days90: DaysChart(today, height, 90); break;
            default: DaysChart(today, height, 30); break;
            }
            FillCard(g_statsTopRow, top);
            EndCard();
        }

        void PodiumCard(const IdlerModel& model, float height)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Most idled");
            if (g_statsTop.empty())
            {
                EmptyState("##none", 105u, "Nothing idled yet.");
            }
            else
            {
                BeginRow("##podium");
                const float width = RowWidth();
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const float line = ImGui::GetTextLineHeight();
                const float pixel = std::clamp(std::round(height / 62.0f), std::round(Px(3.0f)), std::round(Px(6.0f)));
                const ImVec2 picture = IdleAnimation::SizeAt(pixel);
                const float labels = line * 2.0f + Px(8.0f);
                const float highest = std::max(Px(30.0f), height - picture.y - labels);
                const float column = std::floor(width / 3.0f);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                static constexpr int kPlaces[3] = { 1, 0, 2 };
                static constexpr float kSteps[3] = { 1.0f, 0.64f, 0.36f };
                static constexpr float kFill[3] = { 0.46f, 0.28f, 0.18f };
                const float ground = pos.y + picture.y + highest;

                const auto centred = [&](float left, float y, ImVec4 color, const std::string& text)
                    {
                        const float room = column - Px(6.0f);
                        const float size = ImGui::CalcTextSize(text.c_str()).x;
                        DrawCutText(drawList, ImVec2(std::round(left + Px(3.0f) + std::max(0.0f, (room - size) * 0.5f)), y), left + column - Px(3.0f), color, text);
                    };

                for (std::size_t rank = 0; rank < g_statsTop.size(); ++rank)
                {
                    const GameFigures& game = g_statsTop[rank];
                    const float left = pos.x + column * static_cast<float>(kPlaces[rank]);
                    const std::string name = GameName(model, game.AppId);
                    ImGui::SetCursorScreenPos(ImVec2(left, pos.y));
                    ImGui::PushID(static_cast<int>(rank));
                    const bool clicked = ImGui::InvisibleButton("##place", ImVec2(column, height));
                    ImGui::PopID();
                    const bool hovered = ImGui::IsItemHovered();
                    if (hovered)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ShowTooltip([&]()
                            {
                                ImGui::TextUnformatted(name.c_str());
                                ImGui::TextDisabled("%s", SecondsText(game.Seconds).c_str());
                            });
                    }
                    if (clicked)
                        OpenStatsGame(game.AppId);

                    const float step = std::round(highest * kSteps[rank]);
                    const float inset = Px(5.0f);
                    drawList->AddRectFilled(ImVec2(left + inset, ground - step), ImVec2(left + column - inset, ground),
                        ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, hovered ? kFill[rank] + 0.12f : kFill[rank])), Px(4.0f), ImDrawFlags_RoundCornersTop);
                    const std::string place = std::to_string(rank + 1);
                    const float size = std::round(ImGui::GetFontSize() * (rank == 0 ? 1.7f : 1.35f));
                    drawList->AddText(ImGui::GetFont(), size, ImVec2(std::round(left + (column - ImGui::CalcTextSize(place.c_str()).x * size / ImGui::GetFontSize()) * 0.5f), ground - step + Px(5.0f)),
                        ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, rank == 0 ? 0.95f : 0.75f)), place.c_str());

                    const IdleAnimation::Look look = IsShiny(game.AppId) ? IdleAnimation::Look::Shiny : IdleAnimation::Look::Normal;
                    IdleAnimation::DrawCreature(drawList, ImVec2(std::round(left + (column - picture.x) * 0.5f), ground - step - picture.y), IdleAnimation::CreatureOf(game.AppId), IdleAnimation::Mood::Idling, game.AppId, pixel, look);

                    centred(left, ground + Px(5.0f), ManifoldTheme::Label, name);
                    centred(left, ground + Px(5.0f) + line + Px(2.0f), ManifoldTheme::Muted, ShortSecondsText(game.Seconds));
                }
                IdleAnimation::RequestFrames();
                ImGui::SetCursorScreenPos(pos);
                ImGui::Dummy(ImVec2(width, height));
                ImGui::PopID();
            }
            FillCard(g_statsTopRow, top);
            EndCard();
        }

        void CalendarCard(std::int32_t today)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Calendar");
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const auto cellColor = [](double share) -> ImU32
                {
                    if (share <= 0.0)
                        return IM_COL32(255, 255, 255, 9);
                    const float level = std::ceil(static_cast<float>(std::sqrt(share)) * 4.0f);
                    static constexpr float kAlpha[4] = { 0.26f, 0.46f, 0.72f, 1.0f };
                    return ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, kAlpha[std::clamp(static_cast<int>(level) - 1, 0, 3)]));
                };

            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float square = std::round(Px(9.0f));
                const float gap = std::max(1.0f, std::round(Px(2.0f)));
                const ImVec2 less = ImGui::CalcTextSize("Less");
                const ImVec2 more = ImGui::CalcTextSize("More");
                const float legend = less.x + more.x + Px(12.0f) + square * 4.0f + gap * 3.0f;
                GoToTitleLine(legend, ImGui::GetTextLineHeight());
                const ImVec2 at = ImGui::GetCursorScreenPos();
                drawList->AddText(at, ImGui::GetColorU32(ManifoldTheme::Muted), "Less");
                float x = at.x + less.x + Px(6.0f);
                const float y = std::round(at.y + (less.y - square) * 0.5f);
                for (const double share : { 0.05, 0.2, 0.5, 1.0 })
                {
                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + square, y + square), cellColor(share), Px(2.0f));
                    x += square + gap;
                }
                drawList->AddText(ImVec2(x + Px(6.0f) - gap, at.y), ImGui::GetColorU32(ManifoldTheme::Muted), "More");
                ImGui::SetCursorScreenPos(cursor);
            }

            BeginRow("##calendar");
            const float width = RowWidth();
            const float labels = Px(30.0f);
            const float gap = std::max(1.0f, std::round(Px(2.0f)));
            const float cell = std::clamp(std::floor((width - labels) / 26.0f - gap), Px(8.0f), Px(16.0f));
            const int weeks = std::clamp(static_cast<int>((width - labels + gap) / (cell + gap)), 1, 53);
            const float line = ImGui::GetTextLineHeight();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 grid(pos.x + labels, pos.y + line + Px(3.0f));
            const float height = line + Px(3.0f) + (cell + gap) * 7.0f - gap;

            const bool clicked = ImGui::InvisibleButton("##days", ImVec2(width, height));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 mouse = ImGui::GetMousePos();
            std::int32_t underMouse = 0;
            bool found = false;

            const double best = *std::max_element(g_statsCalendar.begin(), g_statsCalendar.end());
            const std::int32_t lastMonday = today - WeekdayOf(today);
            static constexpr const char* kMonths[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            int previousMonth = -1;
            int labelledAt = -100;
            for (int week = 0; week < weeks; ++week)
            {
                const std::int32_t monday = lastMonday - 7 * (weeks - 1 - week);
                const float x = grid.x + (cell + gap) * static_cast<float>(week);

                const std::string date = DayText(monday);
                const int month = (date[5] - '0') * 10 + (date[6] - '0') - 1;
                if (month != previousMonth)
                {
                    if (week - labelledAt >= 3 && month >= 0 && month < 12 && (previousMonth >= 0 || week == 0))
                    {
                        drawList->AddText(ImVec2(x, pos.y), ImGui::GetColorU32(ManifoldTheme::Muted), kMonths[month]);
                        labelledAt = week;
                    }
                    previousMonth = month;
                }

                for (int row = 0; row < 7; ++row)
                {
                    const std::int32_t day = monday + row;
                    if (day > today)
                        continue;
                    const double seconds = g_statsCalendar[g_statsCalendar.size() - 1 - static_cast<std::size_t>(today - day)];
                    const ImVec2 min(x, grid.y + (cell + gap) * static_cast<float>(row));
                    const ImVec2 max(min.x + cell, min.y + cell);
                    drawList->AddRectFilled(min, max, cellColor(best > 0.0 ? seconds / best : 0.0), Px(2.0f));
                    const bool under = hovered && mouse.x >= min.x && mouse.x < max.x + gap && mouse.y >= min.y && mouse.y < max.y + gap;
                    if (under)
                    {
                        underMouse = day;
                        found = true;
                    }
                    if (under || (g_statsDay && *g_statsDay == day))
                        drawList->AddRect(min, max, ImGui::GetColorU32(under ? ManifoldTheme::Text : ManifoldTheme::Label), Px(2.0f));
                }
            }
            for (const int row : { 0, 2, 4 })
            {
                static constexpr const char* kDays[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
                drawList->AddText(ImVec2(pos.x, std::round(grid.y + (cell + gap) * static_cast<float>(row) + (cell - line) * 0.5f)), ImGui::GetColorU32(ManifoldTheme::Muted), kDays[row]);
            }
            if (found)
            {
                const double seconds = g_statsCalendar[g_statsCalendar.size() - 1 - static_cast<std::size_t>(today - underMouse)];
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ShowTooltip([&]()
                    {
                        ImGui::TextUnformatted(seconds > 0.0 ? SecondsText(seconds).c_str() : "Nothing idled");
                        ImGui::TextDisabled("%s", DayText(underMouse).c_str());
                    });
                if (clicked)
                    OpenStatsDay(underMouse);
            }
            ImGui::PopID();
            FillCard(g_statsMiddleRow, top);
            EndCard();
        }

        void RecordsCard(const IdlerModel& model)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Records");
            const StatsSummary& summary = g_statsSummary;
            if (summary.DaysIdled == 0)
            {
                EmptyState("##none", 106u, "Nothing idled yet.");
            }
            else
            {
                InfoRow("Best day", ShortSecondsText(summary.BestDaySeconds) + ", " + DayText(summary.BestDay), ManifoldTheme::Label);
                InfoRow("Most games", std::to_string(summary.MostGamesInADay) + " on " + DayText(summary.MostGamesDay), ManifoldTheme::Label);
                InfoRow("Most in a row", std::to_string(summary.LongestStreak) + (summary.LongestStreak == 1 ? " day" : " days"), ManifoldTheme::Label);
                InfoRow("Average day", ShortSecondsText(summary.Seconds / static_cast<double>(summary.DaysIdled)), ManifoldTheme::Label);
                if (summary.LongestSession > 0.0)
                    InfoRow("Longest run", ShortSecondsText(summary.LongestSession) + ", " + GameName(model, summary.LongestSessionApp), ManifoldTheme::Label);
                if (summary.MostSessions > 0)
                    InfoRow("Most sessions", std::to_string(summary.MostSessions) + ", " + GameName(model, summary.MostSessionsApp), ManifoldTheme::Label);
            }
            FillCard(g_statsMiddleRow, top);
            EndCard();
        }

        void WeekdayCard(float height)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("By day of the week");
            WeekdayChart("##weekdays", height);
            FillCard(g_statsLowRow, top);
            EndCard();
        }

        void HourCard(float height)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("By hour of the day");
            HourChart("##hours", height);
            FillCard(g_statsLowRow, top);
            EndCard();
        }

        float g_statsTilesUsed = 80.0f;
        float g_statsTopChrome = 70.0f;
        float g_statsMiddleUsed = 200.0f;
        float g_statsLowChrome = 60.0f;

        void StatsOverview(const IdlerModel& model, std::int32_t today)
        {
            const float page = ImGui::GetContentRegionAvail().y;
            const float spare = page - g_statsTilesUsed - g_statsTopChrome - g_statsMiddleUsed - Px(2.0f);
            const bool threeRows = spare - g_statsLowChrome >= Px(130.0f) + Px(96.0f);
            const float free = threeRows ? spare - g_statsLowChrome : spare;
            const float tall = std::max(Px(100.0f), threeRows ? std::round(free * 0.62f) : free);
            const float low = std::max(Px(80.0f), free - tall);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##overview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, spare >= Px(100.0f) ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None);
            ImGui::PopStyleColor();
            SetScrollSurface(ManifoldTheme::Page);

            const float start = ImGui::GetCursorScreenPos().y;
            StatTiles();
            const float afterTiles = ImGui::GetCursorScreenPos().y;

            const float closer = Px(18.0f);
            CardColumns("##top", [&]() { HoursCard(today, tall, !threeRows); }, [&]() { PodiumCard(model, tall); }, 0.66f);
            EndCardRow(g_statsTopRow);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - closer);
            const float afterTop = ImGui::GetCursorScreenPos().y;

            CardColumns("##middle", [&]() { CalendarCard(today); }, [&]() { RecordsCard(model); }, 0.66f);
            EndCardRow(g_statsMiddleRow);
            const bool lastRowIsMiddle = !threeRows;
            if (!lastRowIsMiddle)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - closer);
            const float afterMiddle = ImGui::GetCursorScreenPos().y;
            g_statsMiddleUsed = afterMiddle - afterTop + (lastRowIsMiddle ? -closer : 0.0f);

            if (threeRows)
            {
                CardColumns("##low", [&]() { WeekdayCard(low); }, [&]() { HourCard(low); }, 0.5f);
                EndCardRow(g_statsLowRow);
                g_statsLowChrome = ImGui::GetCursorScreenPos().y - afterMiddle - low - closer;
            }
            g_statsTilesUsed = afterTiles - start;
            g_statsTopChrome = afterTop - afterTiles - tall;
            ImGui::EndChild();
        }

        void StatsGameList(const IdlerModel& model)
        {
            BeginCard("Games");
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float width = Px(136.0f);
                GoToTitleLine(width, ImGui::GetFrameHeight());
                if (ChoiceCombo("##order", g_statsOrder, GameOrder::Count, GameOrderName, width))
                    SortFigures(g_statsFigures, g_statsOrder);
                ImGui::SetCursorScreenPos(cursor);
            }

            const auto measure = [](const GameFigures& game) -> double
                {
                    switch (g_statsOrder)
                    {
                    case GameOrder::Sessions: return static_cast<double>(game.Sessions);
                    case GameOrder::LongestSession: return game.LongestSession;
                    case GameOrder::Days: return static_cast<double>(game.Days);
                    default: return game.Seconds;
                    }
                };
            double most = 0.0;
            for (const GameFigures& game : g_statsFigures)
                most = std::max(most, measure(game));

            BeginCardList("##games", 0.0f);
            if (g_statsFigures.empty())
                EmptyState("##none", 105u, "Nothing idled yet.");
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_statsFigures.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const GameFigures& game = g_statsFigures[static_cast<std::size_t>(row)];
                    const std::string time = SecondsText(game.Seconds);
                    const std::string sessions = std::to_string(game.Sessions) + (game.Sessions == 1 ? " session" : " sessions");
                    std::string detail = time;
                    switch (g_statsOrder)
                    {
                    case GameOrder::Sessions: detail = sessions + ", " + time; break;
                    case GameOrder::LongestSession: detail = "Longest " + SecondsText(game.LongestSession) + ", " + time; break;
                    case GameOrder::Days: detail = std::to_string(game.Days) + (game.Days == 1 ? " day, " : " days, ") + time; break;
                    case GameOrder::Recent: detail = DayText(game.LastDay) + ", " + time; break;
                    default:
                        if (game.Sessions > 0)
                            detail = time + ", " + sessions;
                        break;
                    }
                    GameRowOptions options;
                    if (g_statsOrder != GameOrder::Recent && most > 0.0)
                        options.Progress = static_cast<float>(measure(game) / most);
                    const bool selected = game.AppId == g_statsGame;
                    ImGui::PushID(static_cast<int>(game.AppId));
                    if (GameRow("##game", selected, Covers::Get(game.AppId, Covers::Size::Thumb), GameName(model, game.AppId), selected ? ManifoldTheme::Text : ManifoldTheme::Label,
                        detail, ManifoldTheme::Muted, {}, options))
                    {
                        g_statsGame = game.AppId;
                        g_statsRefreshAt = -1.0;
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        OpenGameWindow(game.AppId, GameTab::Statistics);
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void GameStatsRows(const GameFigures& game, double allSeconds, const std::vector<double>& days, std::int32_t today, float below,
            const std::function<void(std::int32_t)>& clickedDay)
        {
            InfoRow("Idled", SecondsText(game.Seconds), ManifoldTheme::Label);
            const double share = allSeconds > 0.0 ? game.Seconds / allSeconds * 100.0 : 0.0;
            InfoRow("Of all time", std::to_string(static_cast<int>(std::round(share))) + " %", ManifoldTheme::Label);
            if (game.Sessions > 0)
                InfoRow("Sessions", std::to_string(game.Sessions) + ", longest " + SecondsText(game.LongestSession), ManifoldTheme::Label);
            InfoRow("Days", std::to_string(game.Days) + ", " + DayText(game.FirstDay) + " to " + DayText(game.LastDay), ManifoldTheme::Label);

            BeginRow("##chart_title");
            ImGui::TextDisabled("Last 30 days");
            ImGui::PopID();
            const std::int32_t firstDay = today - kChartDays + 1;
            BarChartInput chart{ .Seconds = days };
            chart.Height = std::clamp(ImGui::GetContentRegionAvail().y - below, Px(84.0f), Px(300.0f));
            chart.Empty = "Not in the last 30 days.";
            chart.Label = [=](int index) -> std::string
                {
                    if ((kChartDays - 1 - index) % 7 != 0)
                        return {};
                    const std::int32_t day = firstDay + index;
                    return day == today ? std::string("Today") : DayText(day).substr(5);
                };
            chart.Tooltip = [&](int index)
                {
                    ImGui::TextUnformatted(SecondsText(days[static_cast<std::size_t>(index)]).c_str());
                    ImGui::TextDisabled("%s", DayText(firstDay + index).c_str());
                };
            if (clickedDay)
                chart.Clicked = [=](int index) { clickedDay(firstDay + index); };
            BarChart("##game_days", chart);
        }

        void StatsGameCard(const IdlerModel& model, std::int32_t today)
        {
            BeginCard("Selected game");
            const auto found = std::find_if(g_statsFigures.begin(), g_statsFigures.end(), [](const GameFigures& game) { return game.AppId == g_statsGame; });
            if (found == g_statsFigures.end())
            {
                EmptyState("##none", 103u, "Pick a game in the list.");
                EndCard();
                return;
            }

            const GameFigures game = *found;
            CoverRow("##cover", Covers::Get(game.AppId, Covers::Size::Full), Px(84.0f));
            InfoRow("Name", GameName(model, game.AppId));
            GameStatsRows(game, g_statsSummary.Seconds, g_statsGameDays, today, ImGui::GetStyle().ItemSpacing.y * 2.0f + Px(4.0f), [](std::int32_t day) { OpenStatsDay(day); });
            EndCard();
        }

        std::string GameStateText(const IdlerModel& model, std::uint32_t appId)
        {
            const RunState state = model.StateOf(appId);
            if (state == RunState::Running)
                return "Idling";
            if (state == RunState::Starting)
                return "Starting";
            const QueueEntry* entry = model.FindEntry(appId);
            if (!entry)
                return "Not in the queue";
            if (entry->Failed)
                return "Did not start";
            if (IsFinished(*entry))
                return "Finished";
            if (entry->Resting && entry->IdledSeconds > 0.0)
                return "On a break";
            return "In the queue";
        }

        void RefreshGameWindowStats(const IdlerModel& model, std::int32_t today)
        {
            const double now = ImGui::GetTime();
            if (now < g_gameWindow.StatsAt)
                return;
            g_gameWindow.StatsAt = now + 1.0;

            const IdleStats& stats = model.Stats();
            const std::uint32_t account = model.Account();
            const std::vector<GameFigures> figures = stats.Figures(account);
            const auto found = std::find_if(figures.begin(), figures.end(), [](const GameFigures& game) { return game.AppId == g_gameWindow.App; });
            g_gameWindow.HasFigures = found != figures.end();
            if (g_gameWindow.HasFigures)
                g_gameWindow.Figures = *found;
            g_gameWindow.AllSeconds = stats.Total(account);
            g_gameWindow.Days = stats.PerDay(account, g_gameWindow.App, today, kChartDays);
        }

        void GameOverview(IdlerModel& model, std::uint32_t appId, const Game* game)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##overview_tab");
            ImGui::PopStyleColor();
            if (ImGui::BeginTable("##overview_columns", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("Game");
                if (game)
                    GameFactRows(model, *game);
                else
                    InfoRow("App ID", std::to_string(appId), ManifoldTheme::Label);

                ImGui::TableNextColumn();
                ImGui::TextDisabled("Queue");
                if (const QueueEntry* found = model.FindEntry(appId))
                {
                    const QueueEntry entry = *found;
                    QueueEntryRows(model, entry, game, false);
                }
                else
                {
                    Notice("##not_queued", nullptr, ManifoldTheme::Muted, "Not in the queue. Its goal and the rest show here once it is.");
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        void GameStatistics(IdlerModel& model, std::uint32_t appId)
        {
            const std::int32_t today = TodayNumber();
            RefreshGameWindowStats(model, today);
            if (!g_gameWindow.HasFigures)
            {
                EmptyState("##none", appId, "Not idled yet.");
                return;
            }
            std::optional<std::int32_t> day;
            GameStatsRows(g_gameWindow.Figures, g_gameWindow.AllSeconds, g_gameWindow.Days, today, ImGui::GetStyle().WindowPadding.y + Px(4.0f),
                [&](std::int32_t clicked) { day = clicked; });
            if (day)
            {
                OpenStatsDay(*day);
                g_page = Page::Statistics;
                model.CloseAchievements();
                ImGui::CloseCurrentPopup();
            }
        }

        void GameWindowHeader(IdlerModel& model, std::uint32_t appId, const Game* game)
        {
            const float height = std::round(Px(112.0f));
            const float width = std::round(height * Covers::kAspect);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float right = start.x + ImGui::GetContentRegionAvail().x;
            CoverPicture(Covers::Get(appId, Covers::Size::Full), ImVec2(width, height));

            const float left = start.x + width + Px(16.0f);
            const float line = ImGui::GetTextLineHeight();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(ImVec2(left, start.y), ImVec2(right, start.y + height), true);

            const RunState state = model.StateOf(appId);
            const IdleAnimation::Mood mood = state == RunState::Running ? IdleAnimation::Mood::Idling : state == RunState::Starting ? IdleAnimation::Mood::Starting : IdleAnimation::Mood::Sleeping;
            const float creature = IdleAnimation::Height();
            IdleAnimation::Draw(drawList, ImVec2(left, start.y), mood, appId);
            const std::string creatureName = IdleAnimation::NameOf(appId);
            drawList->AddText(ImVec2(std::round(left + IdleAnimation::Width() + Px(8.0f)), std::round(start.y + (creature - line) * 0.5f)),
                ImGui::GetColorU32(RarityColor(RarityOf(IdleAnimation::CreatureOf(appId)))), creatureName.c_str());

            float y = std::round(start.y + creature + Px(8.0f));
            if (game)
            {
                std::string played = game->PlaytimeMinutes > 0 ? PlaytimeText(game->PlaytimeMinutes) + " played" : std::string("Not played");
                if (game->LastPlayed != 0)
                    played += ", last on " + DateText(game->LastPlayed);
                drawList->AddText(ImVec2(left, y), ImGui::GetColorU32(ManifoldTheme::Label), played.c_str());
                y += line + Px(4.0f);
            }
            std::string where = "Not in the queue";
            ImVec4 whereColor = ManifoldTheme::Muted;
            if (const QueueEntry* entry = model.FindEntry(appId))
                std::tie(where, whereColor) = EntryDetail(*entry, state, model.RestSecondsLeft(appId));
            drawList->AddText(ImVec2(left, y), ImGui::GetColorU32(whereColor), where.c_str());
            drawList->PopClipRect();

            ImGui::SetCursorScreenPos(ImVec2(left, start.y + height - ImGui::GetFrameHeight()));
            BeginRow("##actions");
            GameButtons(model, appId, false);
            EndActionButtons();
            ImGui::PopID();
            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height + Px(10.0f)));
        }

        void GameWindowDialog(IdlerModel& model)
        {
            if (g_gameWindow.WantsOpen)
            {
                g_gameWindow.WantsOpen = false;
                model.CloseAchievements();
                ImGui::OpenPopup("##game");
                if (g_gameWindow.ReadAtStart)
                {
                    g_gameWindow.ReadAtStart = false;
                    model.LoadAchievements(g_gameWindow.App);
                }
            }
            if (!ImGui::IsPopupOpen("##game"))
                return;

            const std::uint32_t appId = g_gameWindow.App;
            bool closed = false;
            if (!BeginDialog("##game", ICON_FA_GAMEPAD, GameName(model, appId), Px(760.0f, 560.0f), GameStateText(model, appId), &closed))
                return;
            if (closed)
            {
                model.CloseAchievements();
                ImGui::EndPopup();
                return;
            }

            const Game* found = model.FindGame(appId);
            const std::optional<Game> game = found ? std::optional<Game>(*found) : std::nullopt;

            GameWindowHeader(model, appId, game ? &*game : nullptr);

            const bool achievements = !game || HasAchievements(&*game);
            const AchievementPhase readPhase = model.AchievementsPhase();
            const bool read = readPhase == AchievementPhase::Ready || readPhase == AchievementPhase::Writing;
            const bool stats = achievements || (read && !model.StatList().empty());
            if (!achievements && g_gameWindow.Tab == GameTab::Achievements)
                g_gameWindow.Tab = GameTab::Overview;
            if (!stats && g_gameWindow.Tab == GameTab::GameStats)
                g_gameWindow.Tab = GameTab::Overview;
            const auto tab = [](const char* label, GameTab which)
                {
                    if (StatsPill(label, g_gameWindow.Tab == which))
                        g_gameWindow.Tab = which;
                };
            tab(ICON_FA_INFO_CIRCLE "  Overview", GameTab::Overview);
            ImGui::SameLine(0.0f, Px(2.0f));
            tab(ICON_FA_CHART_BAR "  Statistics", GameTab::Statistics);
            if (achievements)
            {
                ImGui::SameLine(0.0f, Px(2.0f));
                std::string label = ICON_FA_TROPHY "  Achievements";
                const AchievementPhase phase = model.AchievementsPhase();
                if (phase == AchievementPhase::Ready || phase == AchievementPhase::Writing)
                {
                    const auto& rows = model.AchievementList();
                    const auto done = std::count_if(rows.begin(), rows.end(), [](const AchievementRow& row) { return row.Achieved; });
                    label += "  " + std::to_string(done) + "/" + std::to_string(rows.size());
                }
                else if (game && game->AchievementsTotal > 0)
                {
                    label += "  " + std::to_string(game->AchievementsDone) + "/" + std::to_string(game->AchievementsTotal);
                }
                ImGui::PushID("##achievements_tab");
                tab(label.c_str(), GameTab::Achievements);
                ImGui::PopID();
            }
            if (stats)
            {
                ImGui::SameLine(0.0f, Px(2.0f));
                std::string label = ICON_FA_LIST_OL "  Game stats";
                if (read)
                    label += "  " + std::to_string(model.StatList().size());
                ImGui::PushID("##stats_tab");
                tab(label.c_str(), GameTab::GameStats);
                ImGui::PopID();
            }
            ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

            switch (g_gameWindow.Tab)
            {
            case GameTab::Overview:
                GameOverview(model, appId, game ? &*game : nullptr);
                break;
            case GameTab::Statistics:
                GameStatistics(model, appId);
                break;
            case GameTab::Achievements:
                AchievementsSection(model, appId);
                break;
            case GameTab::GameStats:
                GameStatsSection(model, appId);
                break;
            }
            ImGui::EndPopup();
        }

        void ByGameCard(const IdlerModel& model)
        {
            BeginCard("By game");
            BeginCardList("##by_game", 0.0f);
            if (g_statsGames.empty())
                EmptyState("##none", 105u, g_statsDay ? "Nothing idled on this day." : "Nothing idled yet.");

            const double most = g_statsGames.empty() ? 1.0 : g_statsGames.front().Seconds;
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_statsGames.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const GameTotal& total = g_statsGames[static_cast<std::size_t>(row)];
                    GameRowOptions options;
                    options.Progress = static_cast<float>(total.Seconds / most);
                    ImGui::PushID(static_cast<int>(total.AppId));
                    GameRow("##game", false, Covers::Get(total.AppId, Covers::Size::Thumb), GameName(model, total.AppId), ManifoldTheme::Label,
                        SecondsText(total.Seconds), ManifoldTheme::Muted, {}, options);
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void HistoryCard(const IdlerModel& model)
        {
            BeginCard("History");
            BeginCardList("##history", 0.0f);
            if (g_statsHistory.empty())
                EmptyState("##none", 106u, g_statsDay ? "Nothing idled on this day." : "Nothing idled yet.");

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_statsHistory.size()), ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const HistoryRow& entry = g_statsHistory[static_cast<std::size_t>(row)];
                    ImGui::PushID(row);
                    ListRow("##row", false, ICON_FA_CLOCK, ManifoldTheme::Muted, GameName(model, entry.AppId), ManifoldTheme::Label,
                        { { SecondsText(entry.Seconds), ManifoldTheme::Label }, { DayText(entry.Day), ManifoldTheme::Muted } });
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void StatsTabs()
        {
            const auto tab = [](const char* label, StatsTab which)
                {
                    if (StatsPill(label, g_statsTab == which))
                        g_statsTab = which;
                };
            tab(ICON_FA_CHART_BAR "  Overview", StatsTab::Overview);
            ImGui::SameLine(0.0f, Px(2.0f));
            tab(ICON_FA_GAMEPAD "  Games", StatsTab::Games);
            ImGui::SameLine(0.0f, Px(2.0f));
            tab(ICON_FA_HISTORY "  History", StatsTab::History);

            if (g_statsTab == StatsTab::History && g_statsDay)
            {
                ImGui::SameLine(0.0f, Px(12.0f));
                const std::string label = std::string(ICON_FA_TIMES "  ") + DayText(*g_statsDay);
                if (ImGui::Button(label.c_str()))
                {
                    g_statsDay.reset();
                    g_statsRefreshAt = -1.0;
                }
                ItemTooltip("Back to all days.");
            }
        }

        void StatisticsPage(Shell& app)
        {
            IdlerModel& model = app.Model();
            const std::int32_t today = TodayNumber();
            RefreshStats(model, today);

            BeginPage(app, g_statsTab == StatsTab::History && g_statsDay ? DayText(*g_statsDay) : std::string("idled by this program"));
            StatsTabs();
            switch (g_statsTab)
            {
            case StatsTab::Games:
                CardColumns("##stats_games",
                    [&]() { StatsGameList(model); },
                    [&]() { StatsGameCard(model, today); }, 0.58f, Px(kDetailColumnWidth));
                break;
            case StatsTab::History:
                CardColumns("##stats_columns",
                    [&]() { ByGameCard(model); },
                    [&]() { HistoryCard(model); });
                break;
            default:
                StatsOverview(model, today);
                break;
            }
            EndPageBody();
        }

        constexpr ImGuiKey kCode[] = { ImGuiKey_UpArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_B, ImGuiKey_A };
        constexpr double kPetSeconds = IdleAnimation::kPetSeconds;
        constexpr double kToastSeconds = 6.0;
        constexpr double kUndoSeconds = 10.0;

        CreatureId g_creature = CreatureId::Slime;
        std::size_t g_codeStep = 0;
        double g_pettedAt = -100.0;
        int g_petStreak = 0;
        PetOutcome g_petOutcome = PetOutcome::Joy;
        std::vector<CreatureId> g_nextFinds;
        CreatureFacts g_nextFacts;
        double g_nextFindsAt = -100.0;
        bool g_showSets = false;
        constexpr ImVec4 kBondColor = ManifoldTheme::Bond;

        struct Toast
        {
            CreatureId Creature = CreatureId::Slime;
            std::uint32_t Seed = 1u;
            std::string Title;
            std::string Line;
            Page Opens = Page::Creatures;
            std::uint32_t SelectEntry = 0;
            ImVec4 TitleColor = ManifoldTheme::Accent;
        };
        std::deque<Toast> g_toasts;
        double g_toastAt = -1.0;
        std::uint64_t g_undoGeneration = 0;
        double g_undoAt = 0.0;
        float g_undoLift = 0.0f;

        void ListenForCode(IdlerModel& model)
        {
            for (const ImGuiKey key : { ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_A, ImGuiKey_B })
            {
                if (!ImGui::IsKeyPressed(key, false))
                    continue;
                if (key == kCode[g_codeStep])
                    ++g_codeStep;
                else if (key != ImGuiKey_UpArrow || g_codeStep != 2)
                    g_codeStep = key == kCode[0] ? 1 : 0;
                if (g_codeStep == std::size(kCode))
                {
                    g_codeStep = 0;
                    model.AwardCreature(CreatureId::Bug);
                }
            }
        }

        ImVec4 RarityColor(Rarity rarity)
        {
            switch (rarity)
            {
            case Rarity::Common: return ImVec4(0.72f, 0.74f, 0.78f, 1.0f);
            case Rarity::Rare: return ImVec4(0.36f, 0.64f, 1.0f, 1.0f);
            case Rarity::Epic: return ImVec4(0.74f, 0.47f, 1.0f, 1.0f);
            case Rarity::Legendary: return ImVec4(1.0f, 0.77f, 0.25f, 1.0f);
            default: return ImVec4(0.38f, 0.84f, 0.45f, 1.0f);
            }
        }

        float RarityGemSize()
        {
            return std::max(1.0f, std::round(Px(1.5f))) * 5.0f;
        }

        void DrawRarityGem(ImDrawList* drawList, ImVec2 at, Rarity rarity)
        {
            const float cell = std::max(1.0f, std::round(Px(1.5f)));
            const ImVec4 color = RarityColor(rarity);
            const auto fill = [&](float left, float top, float right, float bottom, ImU32 tone)
                {
                    drawList->AddRectFilled(ImVec2(at.x + left * cell, at.y + top * cell), ImVec2(at.x + right * cell, at.y + bottom * cell), tone);
                };
            const ImU32 tone = ImGui::GetColorU32(color);
            fill(2.0f, 0.0f, 3.0f, 1.0f, tone);
            fill(1.0f, 1.0f, 4.0f, 2.0f, tone);
            fill(0.0f, 2.0f, 5.0f, 3.0f, tone);
            fill(1.0f, 3.0f, 4.0f, 4.0f, tone);
            fill(2.0f, 4.0f, 3.0f, 5.0f, tone);
            fill(1.0f, 1.0f, 2.0f, 2.0f, IM_COL32(255, 255, 255, 170));
        }

        void CreatureCell(IdlerModel& model, const CreatureInfo& info, ImVec2 size, float pixel)
        {
            const bool found = model.FoundCreatures().test(static_cast<std::size_t>(info.Id));
            ImGui::PushID(static_cast<int>(info.Id));
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);
            if (ImGui::InvisibleButton("##cell", size))
            {
                if (g_creature != info.Id)
                    g_pettedAt = -100.0;
                g_creature = info.Id;
                model.MarkCreatureSeen(info.Id);
            }
            const bool hovered = ImGui::IsItemHovered();
            const bool visible = ImGui::IsItemVisible();
            ImGui::PopID();
            if (!visible)
                return;

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rounding = Px(4.0f);
            if (g_creature == info.Id)
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.14f)), rounding);
            else if (hovered)
                drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, 9), rounding);

            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const ImVec2 at(std::round(min.x + (size.x - picture.x) * 0.5f), min.y + Px(8.0f));
            IdleAnimation::DrawCreature(drawList, at, info.Id, IdleAnimation::Mood::Idling, static_cast<std::uint32_t>(info.Id) + 1u, pixel, found ? IdleAnimation::Look::Normal : IdleAnimation::Look::Unknown);

            const char* name = found ? IdleAnimation::ShownName(info.Id) : "???";
            const float nameWidth = ImGui::CalcTextSize(name).x;
            const float right = max.x - Px(4.0f);
            const ImVec2 namePos(std::max(min.x + Px(4.0f), std::round(min.x + (size.x - nameWidth) * 0.5f)), max.y - ImGui::GetTextLineHeight() - Px(6.0f));
            DrawCutText(drawList, namePos, right, found ? ManifoldTheme::Label : ManifoldTheme::Muted, name);
            DrawRarityGem(drawList, ImVec2(min.x + Px(5.0f), min.y + Px(5.0f)), RarityOf(info.Id));
            if (model.IsNewCreature(info.Id))
            {
                drawList->AddCircleFilled(ImVec2(max.x - Px(8.0f), min.y + Px(8.0f)), Px(3.0f), ImGui::GetColorU32(ManifoldTheme::Accent));
            }
            else if (const int bond = found ? BondLevel(model.CreatureBondHours(info.Id)) : 0; bond > 0)
            {
                const float heart = std::max(1.0f, std::round(Px(1.0f)));
                for (int index = 0; index < bond; ++index)
                    IdleAnimation::DrawHeart(drawList, ImVec2(max.x - Px(4.0f) - heart * 6.0f * static_cast<float>(index + 1) + heart, min.y + Px(5.0f)), heart, 0.9f);
            }
        }

        void RarityCounts(const IdlerModel& model, float right)
        {
            std::array<int, static_cast<std::size_t>(Rarity::Count)> all{};
            std::array<int, static_cast<std::size_t>(Rarity::Count)> found{};
            const CreatureSet& set = model.FoundCreatures();
            for (const CreatureInfo& info : AllCreatures())
            {
                const auto rarity = static_cast<std::size_t>(RarityOf(info.Id));
                ++all[rarity];
                found[rarity] += set.test(static_cast<std::size_t>(info.Id)) ? 1 : 0;
            }

            const float gem = RarityGemSize();
            const float line = ImGui::GetTextLineHeight();
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float y = std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - line);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float x = right;
            for (int index = static_cast<int>(Rarity::Count) - 1; index >= 0; --index)
            {
                const auto at = static_cast<std::size_t>(index);
                const std::string text = std::to_string(found[at]) + "/" + std::to_string(all[at]);
                const float width = ImGui::CalcTextSize(text.c_str()).x;
                const float left = x - width - gem - Px(4.0f);
                DrawRarityGem(drawList, ImVec2(left, std::round(y + (line - gem) * 0.5f)), static_cast<Rarity>(index));
                drawList->AddText(ImVec2(left + gem + Px(4.0f), y), ImGui::GetColorU32(ManifoldTheme::Muted), text.c_str());
                if (ImGui::IsMouseHoveringRect(ImVec2(left, y), ImVec2(x, y + line)) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                {
                    ShowTooltip([&]()
                        {
                            ImGui::TextUnformatted(RarityName(static_cast<Rarity>(index)));
                            ImGui::TextDisabled("%d of %d found", found[at], all[at]);
                        });
                }
                x = left - Px(10.0f);
            }
        }

        void CreatureGrid(IdlerModel& model)
        {
            BeginCard("Collection");
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float button = ImGui::GetFrameHeight();
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + RowWidth() - button, std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetTextLineHeight() + button) * 0.5f)));
                if (IconLink("##home", ICON_FA_HOME, ImVec2(button, button), "Show their home"))
                {
                    model.Options().CreatureHome = true;
                    model.SaveNow();
                }
                ImGui::SetCursorScreenPos(cursor);
                RarityCounts(model, cursor.x + RowWidth() - button - Px(10.0f));
            }
            BeginCardList("##creatures", 0.0f);
            const float gap = Px(4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
            const ImVec2 room = ImGui::GetContentRegionAvail();

            const auto cellAt = [&](float size) { return ImVec2(std::round(size * 16.0f + Px(28.0f)), std::round(size * 14.0f + Px(32.0f))); };
            const auto columnsAt = [&](float size) { return std::max(1, static_cast<int>((room.x + gap) / (cellAt(size).x + gap))); };
            const float largest = std::round(Px(8.0f));
            float pixel = std::round(Px(3.0f));
            while (pixel < largest)
            {
                const int rows = (static_cast<int>(kCreatureCount) + columnsAt(pixel + 1.0f) - 1) / columnsAt(pixel + 1.0f);
                if (static_cast<float>(rows) * (cellAt(pixel + 1.0f).y + gap) - gap > room.y)
                    break;
                pixel += 1.0f;
            }

            const ImVec2 cell = cellAt(pixel);
            const int columns = columnsAt(pixel);
            int column = 0;
            for (const CreatureInfo& info : AllCreatures())
            {
                if (column > 0)
                    ImGui::SameLine();
                CreatureCell(model, info, cell, pixel);
                column = (column + 1) % columns;
            }
            ImGui::PopStyleVar();
            EndCardList();
            EndCard();
        }

        void CreatureDetail(IdlerModel& model)
        {
            const CreatureInfo& info = InfoOf(g_creature);
            const bool found = model.FoundCreatures().test(static_cast<std::size_t>(info.Id));
            const CreatureFacts facts = found ? CreatureFacts{} : model.CreatureFactsNow();

            BeginCard(found ? IdleAnimation::ShownName(info.Id) : "???");

            BeginRow("##stage");
            const ImVec2 stageSize(RowWidth(), std::round(Px(136.0f)));
            const ImVec2 stageMin = ImGui::GetCursorScreenPos();
            const bool petting = ImGui::GetTime() - g_pettedAt < kPetSeconds;
            if (ImGui::InvisibleButton("##pet", stageSize) && found && !petting)
            {
                const double now = ImGui::GetTime();
                g_petStreak = now - g_pettedAt < 12.0 ? g_petStreak + 1 : 1;
                g_petOutcome = OutcomeOfPet(info.Id, BondLevel(model.CreatureBondHours(info.Id)), g_petStreak);
                g_pettedAt = now;
                model.PetCreature(info.Id);
            }
            if (found)
                ItemTooltip("Click to pet it.");
            ImGui::PopID();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const Habit habit = IdleAnimation::HabitOf(info.Id);
            const ImVec2 stageMax(stageMin.x + stageSize.x, stageMin.y + stageSize.y);
            const CreatureStage stage = DrawCreatureStage(model, stageMin, stageMax, habit);
            const double petted = ImGui::GetTime() - g_pettedAt;
            const bool reacting = found && petted < kPetSeconds;
            const PetOutcome outcome = reacting ? g_petOutcome : PetOutcome::Shrug;
            const float pixel = std::round(Px(7.0f)) + (reacting && outcome == PetOutcome::Puff ? std::round(Px(1.0f)) : 0.0f);
            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const ImVec2 normal = IdleAnimation::SizeAt(std::round(Px(7.0f)));
            float top = std::round(stage.Ground - normal.y);
            if (habit == Habit::Floats)
                top = std::round(std::max(stageMin.y + Px(2.0f), stage.Ground - normal.y + Px(6.0f)));
            else if (habit == Habit::Swims)
                top = std::round((stageMin.y + Px(4.0f) + stage.Ground - normal.y) * 0.5f);
            top -= picture.y - normal.y;
            const float petShare = static_cast<float>(petted / kPetSeconds);
            float hop = 0.0f;
            float aside = 0.0f;
            if (reacting)
            {
                if (outcome == PetOutcome::Joy && petted < kPetSeconds * 0.4)
                    hop = std::round(pixel * 1.5f * static_cast<float>(std::sin(petted / (kPetSeconds * 0.4) * 3.14159265)));
                else if (outcome == PetOutcome::Dodge)
                    aside = std::round(pixel * 3.0f * static_cast<float>(std::sin(std::min(1.0f, petShare * 1.3f) * 3.14159265)));
                else if (outcome == PetOutcome::Bite || outcome == PetOutcome::Puff)
                    aside = std::round(Px(2.0f) * static_cast<float>(std::sin(petted * 45.0)));
            }
            const ImVec2 at(std::round(stageMin.x + (stageSize.x - picture.x) * 0.5f) + aside, top - hop);
            IdleAnimation::DrawCreature(drawList, at, info.Id, IdleAnimation::Mood::Idling, static_cast<std::uint32_t>(info.Id) + 1u, pixel, found ? IdleAnimation::Look::Normal : IdleAnimation::Look::Unknown);
            DrawCreatureStageFront(stageMin, stageMax, habit);
            if (reacting)
            {
                IdleAnimation::RequestFrames();
                const float heartPixel = std::round(Px(2.0f));
                const ImVec2 mark(at.x + picture.x + Px(14.0f), at.y + Px(24.0f));
                switch (outcome)
                {
                case PetOutcome::Joy:
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x - pixel, at.y + pixel * 2.0f - Px(30.0f) * petShare), heartPixel, 1.0f - petShare);
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x + picture.x, at.y + pixel * 3.0f - Px(40.0f) * petShare), heartPixel, 1.0f - petShare);
                    break;
                case PetOutcome::Shrug:
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x + picture.x, at.y + pixel * 3.0f - Px(24.0f) * petShare), std::round(Px(1.0f)), 1.0f - petShare);
                    break;
                case PetOutcome::Dodge:
                    IdleAnimation::DrawMark(drawList, mark, heartPixel, IdleAnimation::Mark::Dots, 1.0f - petShare * petShare);
                    break;
                case PetOutcome::Bite:
                    IdleAnimation::DrawAngerMark(drawList, mark, pixel * 0.5f, std::fmod(petted, 0.3) < 0.15);
                    break;
                case PetOutcome::Puff:
                    IdleAnimation::DrawMark(drawList, mark, heartPixel, IdleAnimation::Mark::Alarm, 1.0f - petShare * petShare);
                    break;
                }
            }

            const auto wrapped = [](const char* id, const char* text, ImVec4 color)
                {
                    BeginRow(id);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + RowWidth());
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text);
                    ImGui::PopStyleColor();
                    ImGui::PopTextWrapPos();
                    ImGui::PopID();
                };
            wrapped("##text", found ? IdleAnimation::ShownFlavour(info.Id) : info.Hint, found ? ManifoldTheme::Text : ManifoldTheme::Label);
            InfoRow("Rarity", RarityName(RarityOf(info.Id)), RarityColor(RarityOf(info.Id)));

            if (found)
            {
                const std::uint32_t foundAt = model.CreatureFoundAt(info.Id);
                wrapped("##how", info.How, ManifoldTheme::Muted);
                if (foundAt != 0)
                    InfoRow("Found", DateText(foundAt), ManifoldTheme::Label);

                const double bondHours = model.CreatureBondHours(info.Id);
                const int bond = BondLevel(bondHours);
                std::string bondText = BondName(bond);
                if (bond < kMaxBondLevel)
                    bondText += ", " + HoursText(static_cast<float>(std::round(bondHours * 10.0) / 10.0)) + " of " + HoursText(static_cast<float>(BondHoursOfLevel(bond + 1)));
                InfoRow("Bond", bondText, bond > 0 ? kBondColor : ManifoldTheme::Muted);
                ItemTooltip("Grows with the hours of the games that show it, and a little with every pet and every game played with it.");
                if (bond < kMaxBondLevel)
                {
                    BeginRow("##bond_bar");
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(5.0f));
                    const ImVec2 barMin = ImGui::GetCursorScreenPos();
                    const float barWidth = RowWidth();
                    const float barHeight = std::round(Px(3.0f));
                    ImGui::Dummy(ImVec2(barWidth, barHeight));
                    drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth, barMin.y + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(1.5f));
                    if (const float bondShare = BondShare(bondHours); bondShare > 0.0f)
                        drawList->AddRectFilled(barMin, ImVec2(barMin.x + std::max(barHeight, barWidth * bondShare), barMin.y + barHeight), ImGui::GetColorU32(kBondColor), Px(1.5f));
                    ImGui::PopID();
                }

                std::string sets;
                for (const CreatureFamily& family : AllFamilies())
                {
                    if (IsInFamily(family, info.Id))
                        sets += (sets.empty() ? "" : ", ") + std::string(family.Name);
                }
                if (!sets.empty())
                    InfoRow("Set", sets, ManifoldTheme::Label);
                if (EvolutionOf(info.Id))
                {
                    if (model.CreatureEvolved(info.Id))
                    {
                        bool evolved = !model.CreatureInFirstForm(info.Id);
                        if (ToggleRow("Evolved form", "Off shows its first form.", evolved))
                            model.SetCreatureFirstForm(info.Id, !evolved);
                    }
                    else
                        InfoRow("Evolves at", BondName(kEvolveAtBond), ManifoldTheme::Muted);
                }
                bool buddy = model.Buddy() == info.Id;
                if (ToggleRow("At your account", "Stands next to your name in the sidebar while games idle.", buddy))
                    model.SetBuddy(buddy ? std::optional<CreatureId>(info.Id) : std::nullopt);

                const std::uint32_t playLeft = model.PlayBondLeftToday(info.Id);
                InfoRow("Play", playLeft > 0 ? std::to_string(playLeft) + " min of bond left today" : std::string("Today's bond from play is used up"),
                    playLeft > 0 ? ManifoldTheme::Label : ManifoldTheme::Muted);
                ItemTooltip("Every game brings a little bond, up to 30 minutes a day for each creature.");
                BeginRow("##play");
                const MiniGame terrain = TerrainGameOf(habit);
                if (ActionButton(MiniGameIcon(terrain), MiniGameName(terrain)))
                    OpenMiniGame(info.Id, terrain);
                if (ActionButton(MiniGameIcon(MiniGame::ShellGame), MiniGameName(MiniGame::ShellGame)))
                    OpenMiniGame(info.Id, MiniGame::ShellGame);
                EndActionButtons();
                ImGui::PopID();
            }
            else if (const float share = ProgressShare(info, facts); share >= 0.0f)
            {
                BeginRow("##progress");
                const ImVec2 barMin = ImGui::GetCursorScreenPos();
                const float barWidth = RowWidth();
                const float barHeight = std::round(Px(6.0f));
                ImGui::Dummy(ImVec2(barWidth, barHeight));
                drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth, barMin.y + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(3.0f));
                if (share > 0.0f)
                    drawList->AddRectFilled(barMin, ImVec2(barMin.x + std::max(barHeight, barWidth * share), barMin.y + barHeight), ImGui::GetColorU32(ManifoldTheme::Accent), Px(3.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
                ImGui::TextUnformatted(ProgressText(info, facts).c_str());
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            EndCard();
        }

        void SetRows(IdlerModel& model)
        {
            const CreatureSet& found = model.FoundCreatures();
            BeginCardList("##sets", 0.0f);
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = std::round(ImGui::GetTextLineHeight() + Px(5.0f));
            for (const CreatureFamily& family : AllFamilies())
            {
                ImGui::PushID(family.Name);
                const int have = FoundInFamily(family, found);
                const int all = static_cast<int>(family.Members.size());
                const ImVec2 min = ImGui::GetCursorScreenPos();
                const ImVec2 max(min.x + width, min.y + height);
                if (ImGui::InvisibleButton("##set", ImVec2(width, height)))
                {
                    g_creature = family.Members.front();
                    g_pettedAt = -100.0;
                    for (const CreatureId id : family.Members)
                    {
                        if (!found.test(static_cast<std::size_t>(id)))
                        {
                            g_creature = id;
                            break;
                        }
                    }
                }
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (ImGui::IsItemHovered())
                {
                    drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, 9), Px(4.0f));
                    ShowTooltip([&]()
                        {
                            for (const CreatureId id : family.Members)
                            {
                                if (found.test(static_cast<std::size_t>(id)))
                                    ImGui::TextUnformatted(InfoOf(id).Name);
                                else
                                    ImGui::TextDisabled("???");
                            }
                        });
                }
                const float textY = std::round(min.y + (height - ImGui::GetTextLineHeight()) * 0.5f);
                const bool complete = have == all;
                const std::string count = std::to_string(have) + "/" + std::to_string(all);
                const float countWidth = ImGui::CalcTextSize(count.c_str()).x;
                drawList->AddText(ImVec2(max.x - Px(4.0f) - countWidth, textY), ImGui::GetColorU32(complete ? ManifoldTheme::Accent : ManifoldTheme::Muted), count.c_str());
                DrawCutText(drawList, ImVec2(min.x + Px(4.0f), textY), max.x - Px(12.0f) - countWidth, complete ? ManifoldTheme::Text : ManifoldTheme::Label, family.Name);
                ImGui::PopID();
            }
            EndCardList();
        }

        void NextFindsCard(IdlerModel& model)
        {
            const double now = ImGui::GetTime();
            if (now - g_nextFindsAt > 2.0)
            {
                g_nextFindsAt = now;
                g_nextFacts = model.CreatureFactsNow();
                g_nextFinds = NextFinds(model.FoundCreatures(), g_nextFacts, 3);
            }

            const bool showSets = g_showSets || g_nextFinds.empty();
            BeginCard(showSets ? "Sets" : "Next finds");
            if (!g_nextFinds.empty())
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float button = ImGui::GetFrameHeight();
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + RowWidth() - button, std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetTextLineHeight() + button) * 0.5f)));
                if (IconLink("##sets_switch", showSets ? ICON_FA_HOURGLASS_HALF : ICON_FA_LAYER_GROUP, ImVec2(button, button), showSets ? "Show the next finds" : "Show the sets"))
                    g_showSets = !g_showSets;
                ImGui::SetCursorScreenPos(cursor);
            }
            if (showSets)
            {
                SetRows(model);
                EndCard();
                return;
            }
            for (const CreatureId id : g_nextFinds)
            {
                const CreatureInfo& info = InfoOf(id);
                BeginRow(info.Key);
                const float width = RowWidth();
                const float pixel = IdleAnimation::PixelSize();
                const ImVec2 picture = IdleAnimation::SizeAt(pixel);
                const ImVec2 min = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##find", ImVec2(width, picture.y)))
                {
                    g_creature = id;
                    g_pettedAt = -100.0;
                }
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (ImGui::IsItemHovered())
                    drawList->AddRectFilled(min, ImVec2(min.x + width, min.y + picture.y), IM_COL32(255, 255, 255, 9), Px(4.0f));
                IdleAnimation::DrawCreature(drawList, min, id, IdleAnimation::Mood::Idling, 1u, pixel, IdleAnimation::Look::Unknown);

                const float textX = min.x + picture.x + Px(8.0f);
                const float right = min.x + width - Px(4.0f);
                DrawCutText(drawList, ImVec2(textX, min.y), right, ManifoldTheme::Label, info.Hint);
                const float barTop = min.y + ImGui::GetTextLineHeight() + Px(2.0f);
                const float barHeight = std::round(Px(3.0f));
                const float barWidth = right - textX;
                drawList->AddRectFilled(ImVec2(textX, barTop), ImVec2(right, barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(1.5f));
                const float share = ProgressShare(info, g_nextFacts);
                if (share > 0.0f)
                    drawList->AddRectFilled(ImVec2(textX, barTop), ImVec2(textX + std::max(barHeight, barWidth * share), barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Accent), Px(1.5f));
                DrawCutText(drawList, ImVec2(textX, barTop + barHeight + Px(1.0f)), right, ManifoldTheme::Muted, ProgressText(info, g_nextFacts));
                ImGui::PopID();
            }
            EndCard();
        }

        void CreaturesPage(Shell& app)
        {
            IdlerModel& model = app.Model();
            ListenForCode(model);
            model.MarkCreatureSeen(g_creature);

            const std::size_t found = model.FoundCreatures().count();
            BeginPage(app, std::to_string(found) + " of " + std::to_string(kCreatureCount) + " found");
            if (model.Options().CreatureHome)
            {
                const ImVec2 corner = ImGui::GetCursorScreenPos();
                const ImVec2 room = ImGui::GetContentRegionAvail();
                DrawCreatureHome(model, ImVec2(room.x, std::round(room.y - 1.0f)));
                const float button = ImGui::GetFrameHeight();
                const float margin = Px(8.0f);
                ImGui::SetCursorScreenPos(ImVec2(corner.x + room.x - button - margin, corner.y + margin));
                if (IconLink("##collection", ICON_FA_TH_LARGE, ImVec2(button, button), "Show the collection"))
                {
                    model.Options().CreatureHome = false;
                    model.SaveNow();
                }
                const bool ball = model.Options().HomeBall;
                ImGui::SetCursorScreenPos(ImVec2(corner.x + room.x - button * 2.0f - margin - Px(4.0f), corner.y + margin));
                if (IconLink("##ball", ICON_FA_BASEBALL_BALL, ImVec2(button, button), ball ? "Put the ball away" : "Bring a ball to throw",
                    ball ? ManifoldTheme::AccentHovered : ImVec4(0.77f, 0.78f, 0.80f, 1.0f), ball ? ManifoldTheme::Accent : ImVec4(0.44f, 0.45f, 0.49f, 1.0f)))
                {
                    model.Options().HomeBall = !ball;
                    model.SaveNow();
                }
                EndPageBody();
                return;
            }
            CardColumns("##creature_columns",
                [&]() { CreatureGrid(model); },
                [&]() { CreatureDetail(model); NextFindsCard(model); }, 0.62f, Px(kDetailColumnWidth));
            EndPageBody();
        }

        void CollectToasts(IdlerModel& model)
        {
            const std::vector<CreatureId> fresh = model.TakeFreshCreatures();
            if (fresh.size() == 1)
            {
                const Rarity rarity = RarityOf(fresh.back());
                const std::string title = rarity == Rarity::Common ? std::string("New creature!") : "New " + FoldCase(RarityName(rarity)) + " creature!";
                g_toasts.push_back({ fresh.back(), 1u, title, InfoOf(fresh.back()).Name, Page::Creatures, 0, rarity == Rarity::Common ? ManifoldTheme::Accent : RarityColor(rarity) });
            }
            else if (fresh.size() > 1)
                g_toasts.push_back({ fresh.back(), 1u, "New creatures!", std::to_string(fresh.size()) + " joined your collection", Page::Creatures, 0 });

            const std::vector<CreatureId> evolved = model.TakeEvolutionNews();
            if (evolved.size() == 1)
            {
                const CreatureEvolution* evolution = EvolutionOf(evolved[0]);
                g_toasts.push_back({ evolved[0], 1u, "Evolved!", std::string(InfoOf(evolved[0]).Name) + " evolved into " + (evolution ? evolution->Name : ""), Page::Creatures, 0, kBondColor });
            }
            else if (evolved.size() > 1)
                g_toasts.push_back({ evolved.back(), 1u, "Evolved!", std::to_string(evolved.size()) + " creatures evolved", Page::Creatures, 0, kBondColor });
            std::vector<CreatureId> closer = model.TakeBondNews();
            std::erase_if(closer, [&](CreatureId id) { return std::find(evolved.begin(), evolved.end(), id) != evolved.end(); });
            if (closer.size() == 1)
            {
                g_toasts.push_back({ closer[0], 1u, "Closer now", std::string(InfoOf(closer[0]).Name) + ": " + BondName(BondLevel(model.CreatureBondHours(closer[0]))), Page::Creatures, 0, kBondColor });
            }
            else if (closer.size() > 1)
            {
                g_toasts.push_back({ closer.back(), 1u, "Closer now", std::to_string(closer.size()) + " creatures grew closer", Page::Creatures, 0, kBondColor });
            }
            for (const CreatureFamily* family : model.TakeFinishedFamilies())
                g_toasts.push_back({ family->Members.front(), 1u, "Set complete", family->Name, Page::Creatures, 0 });

            std::vector<std::uint32_t> goals;
            bool queueFinished = false;
            bool crashed = false;
            for (const WindowMessage& message : model.TakeWindowMessages())
            {
                if (message.What == WindowMessage::Kind::GoalReached)
                    goals.push_back(message.AppId);
                else if (message.What == WindowMessage::Kind::QueueFinished)
                    queueFinished = true;
                else
                    crashed = true;
            }
            if (model.TakeUpdateNews())
            {
                const CreatureId creature = model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());
                g_toasts.push_back({ creature, 4u, "New version", "Version " + model.NewerVersion().value_or(std::string()) + " is out", Page::Settings, 0 });
            }
            if (crashed)
            {
                g_crashLogShown = true;
                const CreatureId creature = model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());
                g_toasts.push_back({ creature, 3u, "Last run crashed", "The log shows where", Page::Log, 0, ManifoldTheme::Warning });
            }
            if (goals.size() == 1)
                g_toasts.push_back({ IdleAnimation::CreatureOf(goals[0]), goals[0], "Goal reached", GameName(model, goals[0]), Page::Idling, goals[0] });
            else if (goals.size() > 1)
                g_toasts.push_back({ IdleAnimation::CreatureOf(goals[0]), goals[0], "Goals reached", std::to_string(goals.size()) + " games reached their goal", Page::Idling, 0 });
            if (queueFinished)
            {
                const CreatureId creature = model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());
                g_toasts.push_back({ creature, 2u, "Queue finished", "Every game reached its goal", Page::Idling, 0 });
            }
        }

        void BeginToast(const char* id, float alpha, float lift)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - Px(18.0f), viewport->Pos.y + viewport->Size.y - Px(18.0f) - lift), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ManifoldTheme::Card);
            ImGui::PushStyleColor(ImGuiCol_Border, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.6f));
            ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }

        void DrawUndoToast(IdlerModel& model)
        {
            g_undoLift = 0.0f;
            const UndoRecord& record = model.Undoable();
            if (!record.Any())
                return;

            const double now = ImGui::GetTime();
            if (g_undoGeneration != model.UndoGeneration())
            {
                g_undoGeneration = model.UndoGeneration();
                g_undoAt = now;
            }
            const double age = now - g_undoAt;
            if (age > kUndoSeconds)
            {
                model.ForgetUndo();
                return;
            }
            IdleAnimation::RequestFrames();

            const std::size_t games = record.Entries.size();
            const bool wholeList = record.ListPlace >= 0;
            const bool sorted = !record.Order.empty();
            const char* title = sorted ? "Queue sorted" : wholeList ? "List deleted" : games == 1 ? "Game removed" : "Games removed";
            const std::string line = sorted ? std::string(QueueOrderName(record.SortedBy)) : wholeList ? record.List :
                games == 1 ? GameName(model, record.Entries[0].Entry.AppId) : std::to_string(games) + " games from " + record.List;
            const std::uint32_t seed = games > 0 ? record.Entries.back().Entry.AppId : 1u;
            const CreatureId creature = games > 0 ? IdleAnimation::CreatureOf(seed) : model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());

            BeginToast("##undo", static_cast<float>(std::min({ 1.0, age / 0.25, (kUndoSeconds - age) / 0.5 })), 0.0f);
            const float pixel = std::round(Px(3.0f));
            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const float textWidth = std::min(Px(230.0f), std::max(ImGui::CalcTextSize(title).x, ImGui::CalcTextSize(line.c_str()).x));
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(picture.x + Px(12.0f) + textWidth, picture.y));
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            IdleAnimation::DrawCreature(drawList, min, creature, sorted ? IdleAnimation::Mood::Idling : IdleAnimation::Mood::Sleeping, seed, pixel);
            const float textX = min.x + picture.x + Px(12.0f);
            const float lineHeight = ImGui::GetTextLineHeight();
            const float top = std::round(min.y + (picture.y - lineHeight * 2.0f - Px(3.0f)) * 0.5f);
            DrawCutText(drawList, ImVec2(textX, top), textX + textWidth, ManifoldTheme::Accent, title);
            DrawCutText(drawList, ImVec2(textX, top + lineHeight + Px(3.0f)), textX + textWidth, ManifoldTheme::Text, line);

            ImGui::SameLine(0.0f, Px(14.0f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (picture.y - ImGui::GetFrameHeight()) * 0.5f);
            bool undo = ImGui::Button(ICON_FA_UNDO " Undo");
            ItemTooltip(sorted ? "Puts the order back. Also Ctrl+Z." : "Puts it back with its progress. Also Ctrl+Z.");
            if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
                undo = true;

            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const float barLeft = windowPos.x + Px(12.0f);
            const float barBottom = windowPos.y + windowSize.y - Px(4.0f);
            const float barWidth = (windowSize.x - Px(24.0f)) * static_cast<float>(1.0 - age / kUndoSeconds);
            drawList->AddRectFilled(ImVec2(barLeft, barBottom - Px(2.0f)), ImVec2(barLeft + barWidth, barBottom), ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.7f)), Px(1.0f));

            if (age > 0.25 && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                g_undoAt = std::max(g_undoAt + ImGui::GetIO().DeltaTime, now - kUndoSeconds + 1.0);
            g_undoLift = windowSize.y + Px(8.0f);
            ImGui::End();

            if (undo)
            {
                model.Undo();
                g_undoLift = 0.0f;
            }
        }

        void DrawToasts(IdlerModel& model)
        {
            CollectToasts(model);
            if (g_toasts.empty())
                return;

            const double now = ImGui::GetTime();
            if (g_toastAt < 0.0)
                g_toastAt = now;
            const double age = now - g_toastAt;
            const double life = g_toasts.size() > 1 ? kToastSeconds * 0.5 : kToastSeconds;
            if (age > life)
            {
                g_toasts.pop_front();
                g_toastAt = -1.0;
                return;
            }
            IdleAnimation::RequestFrames();
            const Toast toast = g_toasts.front();

            BeginToast("##toast", static_cast<float>(std::min({ 1.0, age / 0.25, (life - age) / 0.5 })), g_undoLift);

            const float pixel = std::round(Px(3.0f));
            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const float textWidth = std::min(Px(230.0f), std::max(ImGui::CalcTextSize(toast.Title.c_str()).x, ImGui::CalcTextSize(toast.Line.c_str()).x));
            const ImVec2 size(picture.x + Px(12.0f) + textWidth, picture.y);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton("##open", size);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            IdleAnimation::DrawCreature(drawList, min, toast.Creature, IdleAnimation::Mood::Idling, toast.Seed, pixel);
            const float textX = min.x + picture.x + Px(12.0f);
            const float lineHeight = ImGui::GetTextLineHeight();
            const float top = std::round(min.y + (picture.y - lineHeight * 2.0f - Px(3.0f)) * 0.5f);
            DrawCutText(drawList, ImVec2(textX, top), textX + textWidth, toast.TitleColor, toast.Title);
            DrawCutText(drawList, ImVec2(textX, top + lineHeight + Px(3.0f)), textX + textWidth, ManifoldTheme::Text, toast.Line);
            ImGui::End();

            if (clicked)
            {
                g_page = toast.Opens;
                if (toast.Opens == Page::Creatures)
                    g_creature = toast.Creature;
                if (toast.SelectEntry != 0)
                    g_selectedEntry = toast.SelectEntry;
                g_toasts.pop_front();
                g_toastAt = -1.0;
            }
        }

        std::vector<LogEntry> g_logEntries;
        std::vector<std::size_t> g_logShown;
        std::uint64_t g_logGeneration = ~0ull;
        std::uint64_t g_logScrolled = ~0ull;
        bool g_logAutoScroll = true;
        bool g_logFilterChanged = true;
        LogLevel g_logLowest = LogLevel::Info;
        char g_logSearch[64] = "";

        struct LogLevelChoice
        {
            LogLevel Lowest;
            const char* Label;
        };

        constexpr LogLevelChoice kLogLevels[] = {
            { LogLevel::Info, "All entries" },
            { LogLevel::Warning, "Warnings and errors" },
            { LogLevel::Error, "Errors only" },
        };

        ImVec4 LogColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Success: return ImVec4(0.34f, 1.0f, 0.52f, 1.0f);
            case LogLevel::Warning: return ImVec4(1.0f, 0.82f, 0.28f, 1.0f);
            case LogLevel::Error: return ImVec4(1.0f, 0.34f, 0.34f, 1.0f);
            default: return ImVec4(0.34f, 0.94f, 1.0f, 1.0f);
            }
        }

        void LogPage(Shell& app)
        {
            const std::uint64_t generation = RuntimeLog::Generation();
            if (g_logGeneration != generation)
            {
                g_logGeneration = generation;
                g_logEntries = RuntimeLog::Entries();
                g_logFilterChanged = true;
            }
            if (g_logFilterChanged)
            {
                g_logFilterChanged = false;
                g_logShown.clear();
                for (std::size_t index = 0; index < g_logEntries.size(); ++index)
                {
                    if (RuntimeLog::EntryMatches(g_logEntries[index], g_logLowest, g_logSearch))
                        g_logShown.push_back(index);
                }
            }

            const std::string total = std::to_string(g_logEntries.size()) + (g_logEntries.size() == 1 ? " entry" : " entries");
            BeginPage(app, g_logShown.size() == g_logEntries.size() ? total : std::to_string(g_logShown.size()) + " of " + total);
            BeginCard("Runtime Log");

            BeginRow("##log_tools");
            const float toolsStart = ImGui::GetCursorPosX();
            const float toolsWidth = RowWidth();
            if (IconButton(ICON_FA_TRASH "##clear_log", ImGui::GetFrameHeight(), true))
                RuntimeLog::Clear();
            ItemTooltip("Clear Log: empties this list. The log file stays as it is.");
            ImGui::SameLine();

            const char* levelLabel = kLogLevels[0].Label;
            for (const LogLevelChoice& choice : kLogLevels)
            {
                if (choice.Lowest == g_logLowest)
                    levelLabel = choice.Label;
            }
            ImGui::SetNextItemWidth(Px(160.0f));
            if (ImGui::BeginCombo("##log_level", levelLabel))
            {
                for (const LogLevelChoice& choice : kLogLevels)
                {
                    if (ImGui::Selectable(choice.Label, choice.Lowest == g_logLowest))
                    {
                        g_logLowest = choice.Lowest;
                        g_logFilterChanged = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(Px(180.0f));
            FocusOnCtrlF();
            if (ImGui::InputTextWithHint("##log_search", ICON_FA_SEARCH " Search", g_logSearch, sizeof(g_logSearch)))
                g_logFilterChanged = true;
            ItemTooltip("Ctrl+F");
            ImGui::SameLine();
            constexpr const char* kAutoScroll = "Auto-scroll";
            ImGui::SetCursorPosX(toolsStart + toolsWidth - ToggleSwitchWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize(kAutoScroll).x);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(kAutoScroll);
            ImGui::SameLine();
            if (ToggleSwitch("##auto_scroll", g_logAutoScroll) && g_logAutoScroll)
                g_logScrolled = ~0ull;
            ItemTooltip("Goes to the newest entry when one comes in.");
            ImGui::PopID();

            BeginRow("##log_view");
            const float height = std::max(ImGui::GetFrameHeight() * 2.0f, ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y - 1.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.030f, 0.032f, 0.040f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(8.0f, 6.0f));
            ImGui::BeginChild("##console", ImVec2(RowWidth(), height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar);
            SetScrollSurface(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Px(6.0f, 2.0f));
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_logShown.size()), ImGui::GetTextLineHeightWithSpacing());
            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                {
                    const LogEntry& entry = g_logEntries[g_logShown[static_cast<std::size_t>(index)]];
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "[%s]", entry.Time.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(LogColor(entry.Level), "[%s]", RuntimeLog::LevelName(entry.Level));
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.58f, 0.58f, 1.0f), "[%s]", entry.Trace.c_str());
                    ImGui::SameLine();
                    ImGui::TextUnformatted(entry.Message.c_str());
                    ErrorTooltip(entry.Message);
                }
            }
            clipper.End();

            if (g_logAutoScroll && g_logScrolled != generation)
            {
                g_logScrolled = generation;
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopID();

            EndCard();
            EndPageBody();
        }

        void SteamCard(IdlerModel& model)
        {
            const Steam::Status& steam = model.SteamStatus();
            BeginCard("Steam");
            InfoRow("State", steam.LoggedOn() ? "Running" : SteamLabel(model), steam.LoggedOn() ? ManifoldTheme::Accent : ManifoldTheme::Warning);
            if (!model.Persona().empty())
                InfoRow("Account", model.Persona(), ManifoldTheme::Label);
            if (steam.Installed())
                InfoRow("Folder", WideToUtf8(steam.Root.wstring()), ManifoldTheme::Muted);
            if (!model.ScanProblem().empty())
                Notice("##problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, model.ScanProblem());

            BeginRow("##actions");
            if (!steam.Running())
            {
                ImGui::BeginDisabled(!steam.Installed());
                if (ActionButton(ICON_FA_POWER_OFF, "Start Steam"))
                    Desktop::OpenSteam();
                ImGui::EndDisabled();
            }
            ImGui::BeginDisabled(!steam.LoggedOn() || model.Scanning());
            if (ActionButton(ICON_FA_SYNC, "Read library again"))
                model.Rescan();
            ImGui::EndDisabled();
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        void HourRow(const char* label, int& hour, bool& changed)
        {
            ControlRow(label, nullptr, Px(110.0f), [&]()
                {
                    const int step = 1;
                    ImGui::PushID(label);
                    if (ImGui::InputScalar("##hour", ImGuiDataType_S32, &hour, &step, nullptr, "%d:00"))
                    {
                        hour = std::clamp(hour, 0, 23);
                        changed = true;
                    }
                    ImGui::PopID();
                });
        }

        void WhenCard(IdlerModel& model)
        {
            Settings& options = model.Options();
            bool changed = false;

            BeginCard("When to idle");
            if (Desktop::KnowsInputIdle())
                changed |= ToggleRow("Only when I am away", "Idling starts after this long without mouse or keyboard, and stops at the next input.", options.OnlyWhenAway);
            if (options.OnlyWhenAway && Desktop::KnowsInputIdle())
            {
                ControlRow("Away for", nullptr, Px(110.0f), [&]()
                    {
                        const float step = 1.0f;
                        const float fastStep = 5.0f;
                        if (ImGui::InputScalar("##away", ImGuiDataType_Float, &options.AwayMinutes, &step, &fastStep, "%.0f min"))
                        {
                            options.AwayMinutes = std::clamp(options.AwayMinutes, 1.0f, 1440.0f);
                            changed = true;
                        }
                    });
            }
            changed |= ToggleRow("Only at certain hours", "Idling only runs from the first hour up to the second, also over midnight. Your computer's clock counts.", options.OnlyInHours);
            if (options.OnlyInHours)
            {
                HourRow("From", options.FromHour, changed);
                HourRow("Until", options.ToHour, changed);
            }
            EndCard();

            if (changed)
                model.SaveNow();
        }

        void BehaviourCard(Shell& app)
        {
            IdlerModel& model = app.Model();
            Settings& options = model.Options();
            bool changed = false;

            BeginCard("Idling");
            changed |= ToggleRow("Pause while a game runs", "Stops idling while Steam runs another game, and goes on afterwards.", options.PauseWhilePlaying);
            changed |= ToggleRow("Continue after a restart", "When the program was idling as it closed, it starts idling again on its next start.", options.ResumeOnStart);
            changed |= ToggleRow("Keep the computer awake", "No standby while games idle. The screen may still go off.", options.KeepAwake);
            changed |= ToggleRow("Terrarium", "The creatures of the games that idle live in the sidebar. A click pets one.", options.ShowTerrarium);
            ControlRow("Season", "The time of year in the terrarium and in the creatures' home.", Px(140.0f), [&]()
                {
                    changed |= ChoiceCombo("##season", options.Season, SceneSeason::Count, SceneSeasonName, Px(140.0f));
                });
            if (Desktop::CanMovePointer())
                changed |= ToggleRow("Creatures may tug the mouse", "One that is angry after a throw and reaches the pointer pulls it a little way off. Moving the mouse frees it.",
                    options.CreaturesTugMouse);
            changed |= ToggleRow("Log tab", "Shows what the program does while it runs. The file is written either way.", options.ShowLog);
            changed |= ToggleRow("Vary the times",
                "Games start at different moments and take breaks of their own, and their goals differ a little, so their hours do not come out the same.", options.VaryTimes);
            if (options.VaryTimes)
            {
                ControlRow("Goals differ by", "How far a game's goal may lie from the one you set, up or down.", Px(110.0f), [&]()
                    {
                        const float step = 5.0f;
                        if (ImGui::InputScalar("##spread", ImGuiDataType_Float, &options.VarySpread, &step, nullptr, "%.0f %%"))
                        {
                            options.VarySpread = std::clamp(options.VarySpread, 5.0f, 50.0f);
                            changed = true;
                        }
                    });
            }
            changed |= ToggleRow("Protect refunds",
                "Steam refunds a game within 14 days of buying it, with under 2 hours played. Such a game stops 10 minutes before that limit.", options.RefundGuard);
            changed |= ToggleRow("Invisible while idling",
                "You appear offline to friends while games idle. Afterwards your status goes back to online, also if you had set another one.", options.InvisibleWhileIdling);
            EndCard();

            BeginCard("Window");
            if (app.HasTray())
                changed |= ToggleRow("Keep idling in the tray", "Closing the window while games idle moves it to the tray instead of ending the program.", options.KeepInTray);
            bool startsAtLogin = Desktop::StartsAtLogin();
            if (ToggleRow(Desktop::StartAtLoginLabel(), Desktop::StartAtLoginNote(), startsAtLogin))
                Desktop::SetStartsAtLogin(startsAtLogin);
            changed |= ToggleRow("Look for new versions",
                "Once a day the program asks GitHub for its newest release and says so when there is a newer one. The question carries only the program's version. "
                "Nothing is downloaded.", options.UpdateCheck);
            if (const std::optional<std::string> newer = model.NewerVersion())
            {
                Notice("##newer", ICON_FA_GIFT, ManifoldTheme::Accent, "Version " + *newer + " is out.");
                BeginRow("##release");
                if (const std::string page = Releases::PageOf(*newer); ActionButton(ICON_FA_EXTERNAL_LINK_ALT, "Open the release page") && Releases::IsReleasePage(page))
                    Desktop::OpenWebPage(page);
                EndActionButtons();
                ImGui::PopID();
            }
            EndCard();

            if (changed)
                model.SaveNow();
        }

        constexpr double kBackupListSeconds = 5.0;
        constexpr double kBackupNoticeSeconds = 8.0;

        struct BackupDialogState
        {
            bool WantsOpen = false;
            std::vector<Backups::Backup> List;
            std::vector<std::optional<Backups::Contents>> Contents;
            int Chosen = -1;
            std::string Problem;
        };

        BackupDialogState g_backups;
        std::vector<Backups::Backup> g_backupList;
        double g_backupListAt = -100.0;
        std::string g_backupNotice;
        bool g_backupNoticeProblem = false;
        double g_backupNoticeAt = -100.0;

        std::string FileNameText(const std::filesystem::path& path)
        {
            const std::u8string text = path.filename().u8string();
            return std::string(text.begin(), text.end());
        }

        std::string BackupDayText(std::time_t time)
        {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            std::tm today{};
            std::tm yesterday{};
            if (!LocalTime(time, local) || !LocalTime(now, today) || !LocalTime(now - 86400, yesterday))
                return {};
            const auto sameDay = [](const std::tm& first, const std::tm& second) { return first.tm_year == second.tm_year && first.tm_yday == second.tm_yday; };
            if (sameDay(local, today))
                return "Today";
            if (sameDay(local, yesterday))
                return "Yesterday";
            char text[40];
            const std::size_t length = std::strftime(text, sizeof(text), "%a %d %b %Y", &local);
            return std::string(text, length);
        }

        std::string BackupWhenText(const Backups::Backup& backup)
        {
            std::string text = BackupDayText(backup.Time);
            std::tm local{};
            if (backup.Kind != Backups::BackupKind::Daily && LocalTime(backup.Time, local))
            {
                char clock[16];
                const std::size_t length = std::strftime(clock, sizeof(clock), "%H:%M", &local);
                text += ", " + std::string(clock, length);
            }
            return text;
        }

        const char* BackupKindText(Backups::BackupKind kind)
        {
            switch (kind)
            {
            case Backups::BackupKind::Daily:
                return "Daily";
            case Backups::BackupKind::Saved:
                return "By hand";
            case Backups::BackupKind::BeforeRestore:
                return "Before a restore";
            }
            return "";
        }

        const char* BackupIcon(Backups::BackupKind kind)
        {
            switch (kind)
            {
            case Backups::BackupKind::Daily:
                return ICON_FA_CALENDAR_DAY;
            case Backups::BackupKind::Saved:
                return ICON_FA_SAVE;
            case Backups::BackupKind::BeforeRestore:
                return ICON_FA_UNDO;
            }
            return ICON_FA_ARCHIVE;
        }

        void NoteBackup(const Backups::Result& result)
        {
            g_backupNoticeAt = ImGui::GetTime();
            g_backupListAt = -100.0;
            g_backupNoticeProblem = !result.Ok();
            switch (result.What)
            {
            case Backups::Outcome::Made:
                g_backupNotice = "Backed up as " + FileNameText(result.File) + ".";
                break;
            case Backups::Outcome::Unchanged:
                g_backupNotice = "Nothing changed since the last backup.";
                break;
            case Backups::Outcome::NothingToBackUp:
                g_backupNotice = "Nothing is saved yet that could be backed up.";
                break;
            case Backups::Outcome::Failed:
                g_backupNotice = result.Problem;
                break;
            }
        }

        void OpenBackupDialog(const IdlerModel& model)
        {
            g_backups = {};
            g_backups.WantsOpen = true;
            g_backups.List = Backups::List(model.SettingsPath());
            for (const Backups::Backup& backup : g_backups.List)
                g_backups.Contents.push_back(Backups::Inspect(model.SettingsPath(), backup.File));
            g_backups.Chosen = g_backups.List.empty() ? -1 : 0;
        }

        void BackupDialog(Shell& app)
        {
            IdlerModel& model = app.Model();
            if (g_backups.WantsOpen)
            {
                ImGui::OpenPopup("##backups");
                g_backups.WantsOpen = false;
            }
            const std::size_t count = g_backups.List.size();
            const std::string note = std::to_string(count) + (count == 1 ? " backup" : " backups");
            if (!BeginDialog("##backups", ICON_FA_HISTORY, "Restore a backup", Px(660.0f, 480.0f), note))
                return;

            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);
            ImGui::TextUnformatted("The program starts again with the backup you choose. What it holds now is backed up first, so you can go back to it.");
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();

            const float spacing = ImGui::GetStyle().ItemSpacing.y;
            const float footer = ImGui::GetFrameHeight() + spacing + (g_backups.Problem.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing() * 2.0f + spacing);
            BeginCardList("##backup_list", footer);
            for (std::size_t index = 0; index < count; ++index)
            {
                const Backups::Backup& backup = g_backups.List[index];
                const std::optional<Backups::Contents>& contents = g_backups.Contents[index];
                std::vector<RowTag> tags;
                if (!contents)
                {
                    tags.push_back({ "Cannot be read", ManifoldTheme::Warning });
                }
                else
                {
                    if (contents->HasSettings)
                        tags.push_back({ std::to_string(contents->Creatures) + (contents->Creatures == 1 ? " creature" : " creatures"), ManifoldTheme::Label });
                    if (contents->HasStats)
                        tags.push_back({ ShortDurationText(static_cast<std::uint64_t>(contents->Seconds)) + " idled", ManifoldTheme::Label });
                }
                tags.push_back({ BackupKindText(backup.Kind), ManifoldTheme::Muted });
                const bool chosen = g_backups.Chosen == static_cast<int>(index);
                ImGui::PushID(static_cast<int>(index));
                if (ListRow("##backup", chosen, BackupIcon(backup.Kind), chosen ? ManifoldTheme::Accent : ManifoldTheme::Muted, BackupWhenText(backup),
                        contents ? ManifoldTheme::Text : ManifoldTheme::Muted, tags))
                {
                    g_backups.Chosen = static_cast<int>(index);
                    g_backups.Problem.clear();
                }
                ImGui::PopID();
            }
            EndCardList();

            if (!g_backups.Problem.empty())
                Notice("##restore_problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Error, g_backups.Problem);

            const bool readable = g_backups.Chosen >= 0 && static_cast<std::size_t>(g_backups.Chosen) < count &&
                g_backups.Contents[static_cast<std::size_t>(g_backups.Chosen)].has_value();
            ImGui::BeginDisabled(!readable || model.SavesBlocked());
            if (ImGui::Button(ICON_FA_HISTORY "  Restore and start again"))
            {
                const Backups::Result result = model.RestoreBackup(g_backups.List[static_cast<std::size_t>(g_backups.Chosen)].File);
                if (result.Ok())
                {
                    ImGui::CloseCurrentPopup();
                    app.Restart();
                }
                else
                {
                    g_backups.Problem = result.Problem;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        void BackupsCard(Shell& app)
        {
            IdlerModel& model = app.Model();
            const double now = ImGui::GetTime();
            if (now - g_backupListAt > kBackupListSeconds)
            {
                g_backupList = Backups::List(model.SettingsPath());
                g_backupListAt = now;
            }

            BeginCard("Backups");
            if (g_backupList.empty())
            {
                InfoRow("Last backup", "None yet", ManifoldTheme::Muted);
            }
            else
            {
                InfoRow("Last backup", BackupWhenText(g_backupList.front()));
                std::string kept = std::to_string(g_backupList.size());
                if (g_backupList.size() > 1)
                {
                    std::string oldest = BackupDayText(g_backupList.back().Time);
                    if (oldest == "Today" || oldest == "Yesterday")
                        oldest[0] = static_cast<char>(oldest[0] - 'A' + 'a');
                    kept += ", since " + oldest;
                }
                InfoRow("Kept", kept, ManifoldTheme::Label);
            }
            ItemTooltip("Each day the program runs, it backs up its settings and statistics as a ZIP next to them. The last 7 days are kept, "
                        "and the last 5 made by hand or before a restore.");
            if (!g_backupNotice.empty() && now - g_backupNoticeAt < kBackupNoticeSeconds)
                Notice("##backup_notice", g_backupNoticeProblem ? ICON_FA_EXCLAMATION_TRIANGLE : ICON_FA_CHECK, g_backupNoticeProblem ? ManifoldTheme::Warning : ManifoldTheme::Accent,
                    g_backupNotice);

            BeginRow("##backup_actions");
            ImGui::BeginDisabled(model.SavesBlocked());
            if (ActionButton(ICON_FA_SAVE, "Back up now"))
                NoteBackup(model.BackUpNow());
            ImGui::BeginDisabled(g_backupList.empty());
            if (ActionButton(ICON_FA_HISTORY, "Restore a backup"))
                OpenBackupDialog(model);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            if (ActionButton(ICON_FA_FOLDER_OPEN, "Open the folder"))
            {
                const std::filesystem::path folder = Backups::FolderOf(model.SettingsPath());
                std::error_code ignored;
                std::filesystem::create_directories(folder, ignored);
                Desktop::OpenFolder(folder);
            }
            EndActionButtons();
            ImGui::PopID();
            EndCard();

            BackupDialog(app);
        }

        void NewGamesCard(IdlerModel& model)
        {
            bool changed = false;
            BeginCard("New games in the queue");
            GoalControls("##default_goal", model.Options().DefaultGoal, changed);
            EndCard();
            if (changed)
                model.SaveNow();
        }

        void SettingsPage(Shell& app)
        {
            const std::optional<std::string> newer = app.Model().NewerVersion();
            const std::string version = std::string("Version ") + kVersion + (newer ? ", " + *newer + " is out" : std::string());
            BeginPage(app, version, newer ? ManifoldTheme::Accent : ManifoldTheme::Muted);
            ImGui::BeginChild("##settings_width", ImVec2(std::min(ImGui::GetContentRegionAvail().x, Px(kSettingsWidth)), 0.0f), ImGuiChildFlags_None,
                ImGuiWindowFlags_NoBackground);
            SetScrollSurface(ManifoldTheme::Page);
            CardColumns("##settings_columns",
                [&]()
                {
                    SteamCard(app.Model());
                    WhenCard(app.Model());
                    NewGamesCard(app.Model());
                    BackupsCard(app);
                },
                [&]() { BehaviourCard(app); });
            ImGui::EndChild();
            EndPageBody();
        }
    }

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
