#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        bool GoalShown(GoalKind kind)
        {
            return kCardFarmEnabled || kind != GoalKind::CardDrops;
        }
        float g_selectedRowsHeight = 0.0f;
        std::uint32_t g_selectionAnchor = 0;

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

        bool IsSelected(std::uint32_t appId)
        {
            return std::find(g_selection.begin(), g_selection.end(), appId) != g_selection.end();
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

    CoverGridShape CoverGridOf(float space, float gap)
    {
        const int columns = std::max(1, static_cast<int>((space + gap) / (Px(122.0f) + gap)));
        return { columns, std::floor((space - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns)) };
    }

    void ClearSelection()
    {
        g_selection.clear();
        g_selectedEntry = 0;
        g_selectionAnchor = 0;
    }

    void FocusOnCtrlF(bool inDialog)
    {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F) && (inDialog || !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)))
            ImGui::SetKeyboardFocusHere();
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

    void GoalControls(const char* id, Goal& goal, bool& changed, bool mixedKind, bool mixedHours)
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

    void QueueEntryRows(IdlerModel& model, const QueueEntry& entry, const Game* game, bool withCreature)
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
}
