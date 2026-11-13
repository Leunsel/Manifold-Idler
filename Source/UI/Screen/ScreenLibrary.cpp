#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        constexpr const char* kCardIcon = ICON_FA_CLONE;

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
        std::uint32_t g_gameAnchor = 0;

        char g_search[128] = "";
        LibraryFilter g_filter = LibraryFilter::All;
        LibrarySort g_sort = LibrarySort::Name;
        std::uint32_t g_minReview = 0;
        std::vector<int> g_view;
        std::uint64_t g_viewGeneration = ~0ull;
        bool g_viewStale = true;

        std::string RefundText(const Game& game)
        {
            const auto now = static_cast<std::uint32_t>(std::time(nullptr));
            const double played = static_cast<double>(game.PlaytimeMinutes) * 60.0;
            if (!IsRefundable(game.Purchased, played, now))
                return {};
            return "Until " + DateText(game.Purchased + kRefundWindowSeconds) + ", " + DurationText(static_cast<std::uint64_t>(kRefundPlaySeconds - played)) + " left";
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
}
