#include "UI/MiniGameView.h"

#include "App/IdlerModel.h"
#include "Generated/IconsFontAwesome5.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/MenuWidgets.h"
#include "UI/TerrariumView.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace Idler::UI
{
    namespace
    {
        enum class Stage
        {
            Ready,
            Playing,
            Done
        };

        struct Pop
        {
            float X = 0.0f;
            float Y = 0.0f;
            std::string Text;
            ImVec4 Color;
            float Age = 0.0f;
        };

        struct PlayState
        {
            bool WantsOpen = false;
            CreatureId Creature = CreatureId::Slime;
            MiniGame Game = MiniGame::TreatCatch;
            Stage Now = Stage::Ready;
            std::optional<TerrainGame> Terrain;
            std::optional<ShellGame> Shells;
            float PointerX = kPlayWidth * 0.5f;
            float PointerY = kPlayHeight * 0.5f;
            std::vector<Pop> Pops;
            IdlerModel::PlayResult Result;
            int Score = 0;
            float Time = 0.0f;
            std::uint32_t Rounds = 0;
        };
        PlayState g_play;

        constexpr float kPopSeconds = 0.8f;

        ImVec4 Rgb(float r, float g, float b, float a = 1.0f)
        {
            return ImVec4(r, g, b, a);
        }

        ImVec4 Faded(ImVec4 color, float alpha)
        {
            color.w *= alpha;
            return color;
        }

        struct Field
        {
            ImDrawList* List = nullptr;
            ImVec2 Min;
            float Scale = 1.0f;

            ImVec2 At(float x, float y) const
            {
                return ImVec2(std::round(Min.x + x * Scale), std::round(Min.y + y * Scale));
            }

            void Fill(float x0, float y0, float x1, float y1, ImVec4 color) const
            {
                List->AddRectFilled(At(x0, y0), At(x1, y1), ImGui::GetColorU32(color));
            }

            void Picture(float x, float y, std::initializer_list<const char*> rows, std::initializer_list<ImVec4> colors, float alpha = 1.0f) const
            {
                const ImVec2 origin = At(x, y);
                float row = 0.0f;
                for (const char* line : rows)
                {
                    for (int column = 0; line[column] != '\0'; ++column)
                    {
                        const int index = line[column] - 'a';
                        if (index < 0 || index >= static_cast<int>(colors.size()))
                            continue;
                        const ImVec2 min(origin.x + static_cast<float>(column) * Scale, origin.y + row * Scale);
                        List->AddRectFilled(min, ImVec2(min.x + Scale, min.y + Scale), ImGui::GetColorU32(Faded(colors.begin()[index], alpha)));
                    }
                    row += 1.0f;
                }
            }
        };

        Habit SceneOf(CreatureId id)
        {
            return IdleAnimation::HabitOf(id);
        }

        void NewRound()
        {
            const auto clock = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            const std::uint32_t seed = clock * 2654435761u + ++g_play.Rounds;
            g_play.Terrain.reset();
            g_play.Shells.reset();
            if (g_play.Game == MiniGame::ShellGame)
                g_play.Shells.emplace(seed);
            else
                g_play.Terrain.emplace(g_play.Game, seed);
            g_play.Pops.clear();
            g_play.Score = 0;
            g_play.Time = 0.0f;
        }

        void Start()
        {
            NewRound();
            g_play.Now = Stage::Playing;
        }

        void DrawItem(const Field& field, const PlayItem& item, float time)
        {
            switch (item.Kind)
            {
            case PlayItemKind::Treat:
                field.Picture(item.X - 2.0f, item.Y - 2.0f, { ".aa.", "abaa", "aaba", ".aa." }, { Rgb(0.82f, 0.56f, 0.30f), Rgb(0.42f, 0.25f, 0.14f) });
                break;
            case PlayItemKind::GoldenTreat:
                field.Picture(item.X - 2.0f, item.Y - 2.0f, { ".ca.", "aaaa", "aaab", ".bb." }, { Rgb(1.00f, 0.80f, 0.22f), Rgb(0.85f, 0.56f, 0.10f), Rgb(1.0f, 1.0f, 0.92f) });
                break;
            case PlayItemKind::Pebble:
                field.Picture(item.X - 2.0f, item.Y - 1.5f, { ".aa.", "aaab", ".bb." }, { Rgb(0.60f, 0.61f, 0.64f), Rgb(0.40f, 0.41f, 0.45f) });
                break;
            case PlayItemKind::Star:
            {
                const float left = item.Life - item.Age;
                const float alpha = left < 1.0f ? (std::fmod(item.Age * 8.0f, 1.0f) < 0.5f ? 1.0f : 0.35f) : std::min(1.0f, item.Age * 4.0f);
                field.Picture(item.X - 2.5f, item.Y - 2.5f, { "..a..", "abbba", ".bbb.", ".a.a." }, { Rgb(1.0f, 0.86f, 0.30f), Rgb(1.0f, 0.95f, 0.60f) }, alpha);
                break;
            }
            case PlayItemKind::ShootingStar:
            {
                const float direction = item.VelocityX > 0.0f ? -1.0f : 1.0f;
                for (int step = 1; step <= 6; ++step)
                {
                    const auto back = static_cast<float>(step);
                    field.Fill(item.X + direction * back * 1.4f - 0.5f, item.Y - back * 0.25f - 0.5f, item.X + direction * back * 1.4f + 0.5f, item.Y - back * 0.25f + 0.5f,
                        Rgb(1.0f, 0.9f, 0.6f, 0.75f - back * 0.11f));
                }
                field.Picture(item.X - 2.5f, item.Y - 2.5f, { "..a..", "abbba", ".bbb.", ".a.a." }, { Rgb(1.0f, 0.75f, 0.35f), Rgb(1.0f, 1.0f, 0.85f) });
                break;
            }
            case PlayItemKind::StormCloud:
            {
                const bool flash = std::fmod(time * 1.3f + item.X * 0.01f, 1.0f) < 0.12f;
                field.Picture(item.X - 6.0f, item.Y - 3.5f, { "...aaaa.....", ".aaaaaaaaa..", "aaaaaaaaaaaa", "aaaaaaaaaaaa", ".bbbbbbbbbb." },
                    { flash ? Rgb(0.55f, 0.57f, 0.66f) : Rgb(0.36f, 0.38f, 0.46f), Rgb(0.27f, 0.29f, 0.36f) });
                if (flash)
                    field.Picture(item.X - 2.0f, item.Y + 1.5f, { "..a", ".a.", "aa.", ".a." }, { Rgb(1.0f, 0.92f, 0.35f) });
                break;
            }
            case PlayItemKind::Pearl:
                field.List->AddCircle(field.At(item.X, item.Y), field.Scale * 2.6f, ImGui::GetColorU32(Rgb(0.80f, 0.93f, 1.0f, 0.75f)), 16, std::max(1.0f, field.Scale * 0.5f));
                field.Picture(item.X - 1.0f, item.Y - 1.0f, { "ab", "bb" }, { Rgb(1.0f, 1.0f, 1.0f), Rgb(0.88f, 0.86f, 0.92f) });
                break;
            case PlayItemKind::Clam:
            {
                const ImVec4 shell = Rgb(0.80f, 0.50f, 0.70f);
                const ImVec4 dark = Rgb(0.58f, 0.33f, 0.52f);
                if (item.Open > 0.0f)
                {
                    field.Picture(item.X - 3.5f, item.Y - 4.0f, { ".aaaaa.", "bbbbbbb", "..cd...", ".dcdd..", "aaaaaaa", ".bbbbb." },
                        { shell, dark, Rgb(1.0f, 1.0f, 1.0f), Rgb(0.42f, 0.22f, 0.36f) });
                }
                else
                {
                    field.Picture(item.X - 3.5f, item.Y - 1.0f, { ".aaaaa.", "aaaaaaa", ".bbbbb." }, { shell, dark });
                }
                break;
            }
            case PlayItemKind::Jellyfish:
            {
                const bool sway = std::fmod(item.Age * 2.0f, 1.0f) < 0.5f;
                field.Picture(item.X - 3.0f, item.Y - 3.0f, { ".aaaa.", "aaaaaa", "acaaca", sway ? "b.b.b." : ".b.b.b", sway ? ".b.b.b" : "b.b.b." },
                    { Rgb(0.95f, 0.55f, 0.78f, 0.9f), Rgb(0.98f, 0.72f, 0.88f, 0.75f), Rgb(0.35f, 0.15f, 0.30f) });
                break;
            }
            }
        }

        void DrawPlayer(const Field& field, CreatureId id, float x, float y, int direction, float dizzy, float time)
        {
            const int facing = IdleAnimation::FacingOf(id);
            const bool mirrored = facing != 0 && facing != direction;
            IdleAnimation::DrawCreature(field.List, field.At(x, y), id, IdleAnimation::Mood::Idling, static_cast<std::uint32_t>(id) + 1u, field.Scale,
                IdleAnimation::Look::Normal, mirrored);
            if (dizzy > 0.0f)
                IdleAnimation::DrawDizzy(field.List, field.At(x + kPlaySpriteWidth * 0.5f, y - 1.0f), std::max(1.0f, std::round(field.Scale * 0.5f)), time);
        }

        void DrawPops(const Field& field)
        {
            ImFont* font = Fonts::Bold();
            const float size = ImGui::GetFontSize() * 1.15f;
            for (const Pop& pop : g_play.Pops)
            {
                const float share = pop.Age / kPopSeconds;
                const ImVec2 at = field.At(pop.X, pop.Y - 4.0f - share * 7.0f);
                const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, pop.Text.c_str());
                const ImVec2 corner(std::round(at.x - extent.x * 0.5f), std::round(at.y - extent.y));
                const float alpha = 1.0f - share * share;
                field.List->AddText(font, size, ImVec2(corner.x + 1.0f, corner.y + 1.0f), ImGui::GetColorU32(Rgb(0.0f, 0.0f, 0.0f, 0.6f * alpha)), pop.Text.c_str());
                field.List->AddText(font, size, corner, ImGui::GetColorU32(Faded(pop.Color, alpha)), pop.Text.c_str());
            }
        }

        void ShadowText(ImDrawList* list, ImVec2 at, ImVec4 color, const std::string& text, float size)
        {
            ImFont* font = Fonts::Bold();
            list->AddText(font, size, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, 150), text.c_str());
            list->AddText(font, size, at, ImGui::GetColorU32(color), text.c_str());
        }

        void StepTerrain(float elapsed)
        {
            TerrainGame& game = *g_play.Terrain;
            game.Step(elapsed, g_play.PointerX, g_play.PointerY);
            for (const PlayCatch& caught : game.TakeCatches())
            {
                if (caught.Points <= 0)
                    continue;
                const ImVec4 color = caught.Points >= 5 ? ManifoldTheme::Warning : caught.Kind == PlayItemKind::GoldenTreat || caught.Kind == PlayItemKind::ShootingStar ? Rgb(1.0f, 0.85f, 0.3f)
                    : ManifoldTheme::Accent;
                g_play.Pops.push_back({ caught.X, caught.Y, "+" + std::to_string(caught.Points), color, 0.0f });
            }
            g_play.Score = game.Score();
        }

        void DrawTerrain(const Field& field)
        {
            const TerrainGame& game = *g_play.Terrain;
            for (const PlayItem& item : game.Items())
            {
                if (item.Kind == PlayItemKind::Clam)
                    DrawItem(field, item, g_play.Time);
            }
            DrawPlayer(field, g_play.Creature, game.X(), game.Y(), game.Direction(), game.Dizzy(), g_play.Time);
            for (const PlayItem& item : game.Items())
            {
                if (item.Kind != PlayItemKind::Clam)
                    DrawItem(field, item, g_play.Time);
            }
        }

        float SlotX(float position)
        {
            return kPlayWidth * (0.3f + 0.2f * position);
        }

        constexpr float kCupWidth = 20.0f;
        constexpr float kCupHeight = 16.0f;

        float ShellGround()
        {
            return PlayGroundOf(TerrainGameOf(SceneOf(g_play.Creature)));
        }

        void DrawCup(const Field& field, float middle, float bottom, bool pointed)
        {
            const ImVec4 rim = Rgb(0.60f, 0.17f, 0.19f);
            const ImVec4 body = pointed ? Rgb(0.90f, 0.33f, 0.32f) : Rgb(0.82f, 0.26f, 0.26f);
            const ImVec4 light = pointed ? Rgb(0.98f, 0.50f, 0.46f) : Rgb(0.92f, 0.42f, 0.40f);
            const ImVec4 band = Rgb(0.96f, 0.87f, 0.70f);
            const auto rows = static_cast<int>(kCupHeight);
            for (int row = 0; row < rows; ++row)
            {
                const float share = static_cast<float>(row) / static_cast<float>(rows - 1);
                const float half = std::round((13.0f + (kCupWidth - 13.0f) * share) * 0.5f);
                const float y = bottom - kCupHeight + static_cast<float>(row);
                const ImVec4 color = row <= 1 || row == rows - 1 ? rim : row >= 7 && row <= 9 ? band : body;
                field.Fill(middle - half, y, middle + half, y + 1.0f, color);
                if (row > 1 && row < rows - 1 && !(row >= 7 && row <= 9))
                    field.Fill(middle - half + 1.0f, y, middle - half + 3.0f, y + 1.0f, light);
            }
        }

        int PointedCup(const ShellGame& game, float ground)
        {
            if (game.CurrentPhase() != ShellGame::Phase::Pick)
                return -1;
            for (int cup = 0; cup < ShellGame::kCups; ++cup)
            {
                const float middle = SlotX(game.CupPosition(cup));
                if (std::fabs(g_play.PointerX - middle) <= kCupWidth * 0.5f && g_play.PointerY >= ground - kCupHeight - 2.0f && g_play.PointerY <= ground + 2.0f)
                    return cup;
            }
            return -1;
        }

        void DrawShells(const Field& field, bool hovered)
        {
            const ShellGame& game = *g_play.Shells;
            const float ground = ShellGround();
            const int pointed = hovered ? PointedCup(game, ground) : -1;
            if (pointed >= 0)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            const float middle = SlotX(game.CupPosition(game.HidingCup()));
            const bool happy = game.CurrentPhase() == ShellGame::Phase::Reveal && game.PickedCup() == game.HidingCup();
            const float hop = happy ? std::max(0.0f, std::sin(g_play.Time * 9.0f)) * 3.0f : 0.0f;
            if (game.CupLift(game.HidingCup()) > 0.0f)
                DrawPlayer(field, g_play.Creature, std::round(middle - kPlaySpriteWidth * 0.5f), ground - kPlaySpriteHeight - hop, 1, 0.0f, g_play.Time);

            std::array<int, ShellGame::kCups> order{ 0, 1, 2 };
            std::sort(order.begin(), order.end(), [&](int lhs, int rhs) { return game.CupDepth(lhs) < game.CupDepth(rhs); });
            for (const int cup : order)
            {
                const float x = SlotX(game.CupPosition(cup));
                const float lift = game.CupLift(cup) * (kCupHeight + 4.0f);
                const float bottom = ground - lift + std::round(game.CupDepth(cup) * 2.0f);
                field.List->AddRectFilled(field.At(x - kCupWidth * 0.5f + 1.0f, ground - 0.5f), field.At(x + kCupWidth * 0.5f - 1.0f, ground + 1.0f),
                    IM_COL32(0, 0, 0, static_cast<int>(70.0f * (1.0f - game.CupLift(cup) * 0.6f))));
                DrawCup(field, x, bottom, cup == pointed);
            }

            if (happy)
            {
                const float share = std::min(1.0f, g_play.Time - std::floor(g_play.Time));
                IdleAnimation::DrawHeart(field.List, field.At(middle + 6.0f, ground - kPlaySpriteHeight - 3.0f - share * 4.0f), std::max(1.0f, std::round(field.Scale * 0.5f)), 1.0f - share);
            }
            else if (game.CurrentPhase() == ShellGame::Phase::Reveal || game.Over())
            {
                if (game.PickedCup() >= 0 && game.PickedCup() != game.HidingCup())
                {
                    const float empty = SlotX(game.CupPosition(game.PickedCup()));
                    IdleAnimation::DrawMark(field.List, field.At(empty, ground - 6.0f), std::max(1.0f, std::round(field.Scale * 0.75f)), IdleAnimation::Mark::Question, 0.9f);
                }
            }
        }

        const char* ShellMessage(const ShellGame& game, ImVec4& color)
        {
            color = ManifoldTheme::Text;
            switch (game.CurrentPhase())
            {
            case ShellGame::Phase::Show:
            case ShellGame::Phase::Cover:
                return game.Round() == 1 ? "Watch where it hides" : "Again, faster";
            case ShellGame::Phase::Shuffle:
                return "";
            case ShellGame::Phase::Pick:
                return "Which cup?";
            case ShellGame::Phase::Reveal:
                if (game.PickedCup() == game.HidingCup())
                {
                    color = ManifoldTheme::Accent;
                    return "Found it!";
                }
                color = ManifoldTheme::Error;
                return "Not there";
            case ShellGame::Phase::Over:
                color = ManifoldTheme::Error;
                return "Not there";
            }
            return "";
        }

        std::string ResultText(const IdlerModel& model, std::string& bondLine)
        {
            const std::uint32_t best = model.PlayOf(g_play.Creature).Best[static_cast<std::size_t>(g_play.Game)];
            std::string text = g_play.Game == MiniGame::ShellGame ? (g_play.Score == 1 ? std::string("1 round won.") : std::to_string(g_play.Score) + " rounds won.")
                                                                  : "Score " + std::to_string(g_play.Score) + ".";
            if (g_play.Result.NewBest)
                text += " A new best!";
            else if (best > 0)
                text += " Best " + std::to_string(best) + ".";
            const std::uint32_t bond = g_play.Result.Bond;
            if (bond > 0)
                bondLine = "+" + std::to_string(bond) + (bond == 1 ? " minute" : " minutes") + " of bond" + (g_play.Result.DayFull ? ", and today's limit is reached." : ".");
            else
                bondLine = "No bond from this one: today's " + std::to_string(kPlayBondPerDay) + " minutes are used up.";
            return text;
        }
    }

    const char* MiniGameIcon(MiniGame game)
    {
        switch (game)
        {
        case MiniGame::TreatCatch:
            return ICON_FA_COOKIE_BITE;
        case MiniGame::StarChase:
            return ICON_FA_STAR;
        case MiniGame::PearlDive:
            return ICON_FA_WATER;
        case MiniGame::ShellGame:
            return ICON_FA_RANDOM;
        }
        return ICON_FA_GAMEPAD;
    }

    void OpenMiniGame(CreatureId id, MiniGame game)
    {
        g_play.WantsOpen = true;
        g_play.Creature = id;
        g_play.Game = game;
        g_play.Now = Stage::Ready;
        g_play.Result = {};
        NewRound();
    }

    void PlayMiniGameAhead(float seconds)
    {
        Start();
        if (!g_play.Terrain)
            return;
        for (float passed = 0.0f; passed < seconds; passed += 1.0f / 60.0f)
            StepTerrain(1.0f / 60.0f);
        g_play.Time = seconds;
    }

    void DrawMiniGame(IdlerModel& model)
    {
        if (g_play.WantsOpen)
        {
            g_play.WantsOpen = false;
            ImGui::OpenPopup("##minigame");
        }
        if (!ImGui::IsPopupOpen("##minigame"))
            return;

        const std::uint32_t best = model.PlayOf(g_play.Creature).Best[static_cast<std::size_t>(g_play.Game)];
        const std::string title = std::string(MiniGameName(g_play.Game)) + " with " + IdleAnimation::ShownName(g_play.Creature);
        const std::string note = best > 0 ? "Best " + std::to_string(best) : std::string();
        bool closed = false;
        if (!BeginDialog("##minigame", MiniGameIcon(g_play.Game), title, Px(760.0f, 530.0f), note, &closed, g_play.Now != Stage::Playing))
            return;
        if (closed)
        {
            g_play.Terrain.reset();
            g_play.Shells.reset();
            ImGui::EndPopup();
            return;
        }

        const float elapsed = std::min(0.1f, ImGui::GetIO().DeltaTime / static_cast<float>(model.ClockPace()));
        g_play.Time += elapsed;
        for (Pop& pop : g_play.Pops)
            pop.Age += elapsed;
        std::erase_if(g_play.Pops, [](const Pop& pop) { return pop.Age >= kPopSeconds; });

        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 room = ImGui::GetContentRegionAvail();
        const float footer = ImGui::GetFrameHeight() + ImGui::GetTextLineHeightWithSpacing() + Px(14.0f);
        const float scale = std::max(2.0f, std::floor(std::min(room.x / kPlayWidth, (room.y - footer) / kPlayHeight)));
        const ImVec2 size(kPlayWidth * scale, kPlayHeight * scale);
        const ImVec2 min(std::round(start.x + (room.x - size.x) * 0.5f), start.y);
        const ImVec2 max(min.x + size.x, min.y + size.y);
        ImGui::SetCursorScreenPos(min);
        const bool clicked = ImGui::InvisibleButton("##field", size);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            g_play.PointerX = (mouse.x - min.x) / scale;
            g_play.PointerY = (mouse.y - min.y) / scale;
        }

        if (g_play.Now == Stage::Playing)
        {
            IdleAnimation::KeepFrames();
            if (g_play.Terrain)
            {
                StepTerrain(elapsed);
                if (g_play.Terrain->Over())
                {
                    g_play.Now = Stage::Done;
                    g_play.Result = model.FinishGame(g_play.Creature, g_play.Game, g_play.Score);
                }
            }
            else if (g_play.Shells)
            {
                ShellGame& game = *g_play.Shells;
                if (clicked)
                {
                    const int cup = PointedCup(game, ShellGround());
                    if (cup >= 0)
                        game.PickSlot(game.SlotOf(cup));
                }
                game.Step(elapsed);
                g_play.Score = game.Score();
                if (game.Over())
                {
                    g_play.Now = Stage::Done;
                    g_play.Result = model.FinishGame(g_play.Creature, g_play.Game, g_play.Score);
                }
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const Habit scene = g_play.Game == MiniGame::ShellGame ? SceneOf(g_play.Creature) : g_play.Game == MiniGame::StarChase ? Habit::Floats
            : g_play.Game == MiniGame::PearlDive ? Habit::Swims : Habit::Walks;
        DrawCreatureStage(model, min, max, scene, true);
        drawList->PushClipRect(min, max, true);
        const Field field{ drawList, min, scale };
        if (g_play.Terrain)
            DrawTerrain(field);
        else if (g_play.Shells)
            DrawShells(field, hovered && g_play.Now == Stage::Playing);
        DrawPops(field);
        drawList->PopClipRect();
        DrawCreatureStageFront(min, max, scene, ImGui::GetColorU32(ImGuiCol_PopupBg));

        const float big = ImGui::GetFontSize() * 1.3f;
        const ImVec2 corner(min.x + Px(10.0f), min.y + Px(10.0f));
        if (g_play.Terrain && g_play.Now != Stage::Ready)
        {
            ShadowText(drawList, corner, ManifoldTheme::Text, std::to_string(g_play.Score), big);
            if (const int streak = g_play.Terrain->Streak(); streak >= 3)
                ShadowText(drawList, ImVec2(corner.x, corner.y + big + Px(2.0f)), ManifoldTheme::Label, std::to_string(streak) + " in a row", ImGui::GetFontSize());
            const auto left = static_cast<float>(g_play.Terrain->TimeLeft() / kPlayRoundSeconds);
            const float barHeight = std::round(Px(4.0f));
            drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, min.y + barHeight), IM_COL32(0, 0, 0, 90));
            drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + size.x * left, min.y + barHeight),
                ImGui::GetColorU32(g_play.Terrain->TimeLeft() < 10.0 ? ManifoldTheme::Warning : ManifoldTheme::Accent));
        }
        else if (g_play.Shells && g_play.Now != Stage::Ready)
        {
            ShadowText(drawList, corner, ManifoldTheme::Text, "Round " + std::to_string(g_play.Shells->Round()), big);
            ImVec4 color;
            const char* message = ShellMessage(*g_play.Shells, color);
            if (message[0] != '\0')
            {
                const ImVec2 extent = Fonts::Bold()->CalcTextSizeA(big, FLT_MAX, 0.0f, message);
                ShadowText(drawList, ImVec2(std::round((min.x + max.x - extent.x) * 0.5f), corner.y), color, message, big);
            }
        }

        const float buttonsTop = max.y + Px(10.0f);
        const ImGuiStyle& style = ImGui::GetStyle();
        const char* startLabel = g_play.Now == Stage::Done ? ICON_FA_REDO " Play again" : ICON_FA_PLAY " Start";
        const float startWidth = ImGui::CalcTextSize(startLabel).x + style.FramePadding.x * 2.0f;
        const float closeWidth = ImGui::CalcTextSize("Close").x + style.FramePadding.x * 2.0f;
        const float buttons = g_play.Now == Stage::Playing ? 0.0f : startWidth + (g_play.Now == Stage::Done ? closeWidth + style.ItemSpacing.x : 0.0f);
        if (g_play.Now != Stage::Playing)
        {
            ImGui::SetCursorScreenPos(ImVec2(max.x - buttons, buttonsTop));
            const bool keyed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
            if (PositiveButton(startLabel) || keyed)
                Start();
            if (g_play.Now == Stage::Done)
            {
                ImGui::SameLine();
                if (ImGui::Button("Close"))
                {
                    g_play.Terrain.reset();
                    g_play.Shells.reset();
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        std::string first;
        std::string second;
        ImVec4 firstColor = ManifoldTheme::Text;
        ImVec4 secondColor = ManifoldTheme::Muted;
        if (g_play.Now == Stage::Done)
        {
            first = ResultText(model, second);
            firstColor = g_play.Result.NewBest ? ManifoldTheme::Accent : ManifoldTheme::Text;
            secondColor = g_play.Result.Bond > 0 ? ManifoldTheme::Bond : ManifoldTheme::Muted;
        }
        else
        {
            first = MiniGameRules(g_play.Game);
            firstColor = g_play.Now == Stage::Playing ? ManifoldTheme::Muted : ManifoldTheme::Label;
            if (g_play.Now == Stage::Ready)
            {
                const std::uint32_t left = model.PlayBondLeftToday(g_play.Creature);
                second = left > 0 ? "Bond from play today: " + std::to_string(left) + " of " + std::to_string(kPlayBondPerDay) + " minutes left."
                                  : "Today's bond from play is used up; scores still count.";
            }
        }
        const float textRight = max.x - buttons - (buttons > 0.0f ? Px(16.0f) : 0.0f);
        ImGui::SetCursorScreenPos(ImVec2(min.x, buttonsTop + std::round((ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f)));
        const float textLeft = ImGui::GetCursorPosX();
        ImGui::PushTextWrapPos(textLeft + (textRight - min.x));
        ImGui::TextColored(firstColor, "%s", first.c_str());
        if (!second.empty())
        {
            ImGui::SetCursorPosX(textLeft);
            ImGui::TextColored(secondColor, "%s", second.c_str());
        }
        ImGui::PopTextWrapPos();

        ImGui::EndPopup();
    }
}
