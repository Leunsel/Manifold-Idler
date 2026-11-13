#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        constexpr int kCardColumns = 5;
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
}
