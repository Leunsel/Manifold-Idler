#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        bool g_shutdownDialogOpen = false;

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
}
