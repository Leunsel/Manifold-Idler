#include "UI/SeasonFrame.h"

#include "UI/HomeScene.h"
#include "UI/IdleAnimation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Idler::UI
{
    namespace
    {
        using HomeScene::Canvas;
        using HomeScene::Lit;
        using HomeScene::Mix;
        using HomeScene::Noise;
        using HomeScene::Rgb;

        constexpr double kEventSeconds = 3.2;
        constexpr double kEventEvery = 11.0;
        constexpr float kPi = 3.14159265f;

        double g_hoverStarted = -100.0;
        bool g_wasHovered = false;
        float g_push = 0.0f;
        float g_pushSpeed = 0.0f;
        float g_mouse = 0.0f;

        float EventShare(double now, double offset)
        {
            if (!IdleAnimation::Enabled())
                return -1.0f;
            const double hovered = now - g_hoverStarted;
            if (hovered >= 0.0 && hovered < kEventSeconds)
                return static_cast<float>(hovered / kEventSeconds);
            const double cycle = std::fmod(now + offset, kEventEvery);
            return cycle < kEventSeconds ? static_cast<float>(cycle / kEventSeconds) : -1.0f;
        }

        void UpdatePush(bool hovered)
        {
            const float target = hovered ? 1.0f : 0.0f;
            if (!IdleAnimation::Enabled())
            {
                g_push = target;
                g_pushSpeed = 0.0f;
                return;
            }
            const float step = std::min(ImGui::GetIO().DeltaTime, 0.05f) / 4.0f;
            for (int pass = 0; pass < 4; ++pass)
            {
                g_pushSpeed += (90.0f * (target - g_push) - 11.0f * g_pushSpeed) * step;
                g_push += g_pushSpeed * step;
            }
            if (std::fabs(target - g_push) < 0.002f && std::fabs(g_pushSpeed) < 0.01f)
            {
                g_push = target;
                g_pushSpeed = 0.0f;
                return;
            }
            IdleAnimation::RequestFrames();
        }

        float Wave(float x, float scale, float phase)
        {
            return std::sin(x * scale + phase);
        }

        std::uint32_t Hash(int value, std::uint32_t salt)
        {
            return Noise(static_cast<std::uint32_t>(value + 1000) * 2654435761u + salt);
        }

        void Dot(const Canvas& canvas, float x, float y, ImVec4 color)
        {
            canvas.Fill(x, y, x + 1.0f, y + 1.0f, color);
        }

        struct Step
        {
            float X = 0.0f;
            float Y = 0.0f;
            float Tx = 0.0f;
            float Ty = 0.0f;
            float Ox = 0.0f;
            float Oy = 0.0f;
            bool Top = false;
        };

        struct Vine
        {
            int Width = 0;
            int Height = 0;
            int Left = 0;
            int Top = 0;
            int Right = 0;

            Vine(float width, float height)
                : Width(static_cast<int>(width)), Height(static_cast<int>(height)), Left(Height), Top(Width + 2), Right(std::max(2, Height / 3))
            {
            }

            int Count() const { return Left + Top + Right; }

            Step At(int index) const
            {
                if (index < Left)
                {
                    const auto y = static_cast<float>(Height - index);
                    return { Wave(y, 0.8f, 0.3f) > 0.45f ? 0.0f : -1.0f, y, 0.0f, -1.0f, -1.0f, 0.0f, false };
                }
                index -= Left;
                if (index < Top)
                {
                    const auto x = static_cast<float>(index - 1);
                    return { x, Wave(x, 0.45f, 0.4f) > 0.35f ? -2.0f : -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, true };
                }
                index -= Top;
                const auto y = static_cast<float>(index);
                const bool last = index >= Right - 1;
                return { static_cast<float>(Width) + (!last && Wave(y, 0.9f, 1.1f) > 0.4f ? 1.0f : 0.0f), y, 0.0f, 1.0f, 1.0f, 0.0f, false };
            }

            bool NearCurl(int index) const
            {
                const int top = index - Left;
                for (const int curl : { Top * 28 / 100, Top * 66 / 100 })
                {
                    if (std::abs(top - curl) <= 2)
                        return true;
                }
                return false;
            }
        };

        void Put(const Canvas& canvas, const Step& step, float along, float out, ImVec4 color)
        {
            Dot(canvas, step.X + step.Tx * along + step.Ox * out, step.Y + step.Ty * along + step.Oy * out, color);
        }

        Step Flipped(Step step)
        {
            step.Ox = -step.Ox;
            step.Oy = -step.Oy;
            return step;
        }

        enum class LeafSize
        {
            Tiny,
            Small,
            Big
        };

        struct Leafage
        {
            ImVec4 Leaf;
            ImVec4 Light;
            ImVec4 Vein;
        };

        void DrawLeaf(const Canvas& canvas, const Step& step, float side, LeafSize size, float sway, ImVec4 stem, const Leafage& colors)
        {
            const float lean = side + sway;
            if (size == LeafSize::Tiny)
            {
                Put(canvas, step, sway, 1.0f, colors.Leaf);
                Put(canvas, step, lean, 1.0f, colors.Leaf);
                Put(canvas, step, lean, 2.0f, colors.Light);
                return;
            }
            if (size == LeafSize::Small)
            {
                Put(canvas, step, sway, 1.0f, colors.Leaf);
                Put(canvas, step, lean, 1.0f, colors.Vein);
                Put(canvas, step, lean, 2.0f, colors.Light);
                Put(canvas, step, lean + side, 2.0f, colors.Leaf);
                Put(canvas, step, lean + side, 1.0f, colors.Leaf);
                return;
            }
            Put(canvas, step, 0.0f, 1.0f, stem);
            for (float along = -1.0f; along <= 1.0f; along += 1.0f)
            {
                Put(canvas, step, along + sway, 2.0f, along == 0.0f ? colors.Vein : colors.Leaf);
                Put(canvas, step, along + sway, 3.0f, along == -side ? colors.Light : along == 0.0f ? colors.Vein : colors.Leaf);
            }
            Put(canvas, step, lean, 4.0f, colors.Leaf);
        }

        void DrawCurl(const Canvas& canvas, const Step& step, float side, ImVec4 color)
        {
            constexpr float kCells[][2] = { { 0.0f, 1.0f }, { 0.0f, 2.0f }, { 1.0f, 3.0f }, { 2.0f, 3.0f }, { 3.0f, 2.0f }, { 2.0f, 1.0f } };
            for (const auto& cell : kCells)
                Put(canvas, step, cell[0] * side, cell[1], color);
        }

        void DrawEndHook(const Canvas& canvas, const Step& step, ImVec4 color)
        {
            constexpr float kCells[][2] = { { 1.0f, 0.0f }, { 2.0f, 0.0f }, { 3.0f, 1.0f }, { 2.0f, 2.0f }, { 1.0f, 1.0f } };
            for (const auto& cell : kCells)
                Put(canvas, step, cell[0], cell[1], color);
        }

        struct Style
        {
            ImVec4 Stem;
            ImVec4 Tendril;
            Leafage Leaves[4];
            int LeafKinds = 1;
            int Spacing = 0;
            LeafSize TopLeaf = LeafSize::Small;
            int BareShare = 0;
            bool Snow = false;
            ImVec4 SnowColor;
            ImVec4 Ice;
        };

        const Leafage& LeafOf(const Style& style, std::uint32_t mixed)
        {
            return style.Leaves[(mixed >> 12) % static_cast<std::uint32_t>(std::max(1, style.LeafKinds))];
        }

        bool Bare(const Style& style, std::uint32_t mixed)
        {
            return style.BareShare > 0 && static_cast<int>((mixed >> 8) % 100u) < style.BareShare;
        }

        float Sway(int index, float front)
        {
            return front > -100.0f && std::fabs(static_cast<float>(index) - front) < 2.5f ? 1.0f : 0.0f;
        }

        void DrawVine(const Canvas& canvas, const Vine& vine, const Style& style, float front, float clearFrom, float clearTo)
        {
            for (int index = 0; index < vine.Count(); ++index)
            {
                const Step step = vine.At(index);
                Dot(canvas, step.X, step.Y, style.Stem);
            }
            Dot(canvas, -1.0f, 0.0f, style.Stem);

            int curlNumber = 0;
            for (const int curl : { vine.Top * 28 / 100, vine.Top * 66 / 100 })
                DrawCurl(canvas, vine.At(vine.Left + curl), curlNumber++ % 2 == 0 ? 1.0f : -1.0f, style.Tendril);
            DrawEndHook(canvas, vine.At(vine.Count() - 1), style.Tendril);

            if (style.Spacing <= 0)
                return;
            const int creature = std::min(vine.Width / 3, 17);
            int number = 0;
            for (int base = 3; base < vine.Left + vine.Top - 1; base += style.Spacing, ++number)
            {
                const std::uint32_t mixed = Hash(base, 71u);
                const int index = base + static_cast<int>(mixed % 3u) - 1;
                const Step step = vine.At(index);
                if ((step.Top && vine.NearCurl(index)) || (!step.Top && index > vine.Left - 2) || Bare(style, mixed))
                    continue;
                const float side = (mixed >> 4) % 2u == 0u ? 1.0f : -1.0f;
                const Leafage& colors = LeafOf(style, mixed);
                const float sway = Sway(index, front);
                if (!step.Top)
                {
                    DrawLeaf(canvas, step, side, LeafSize::Tiny, sway, style.Stem, colors);
                    continue;
                }
                const bool hanging = number % 2 == 1 && step.X > static_cast<float>(creature) && (step.X < clearFrom || step.X > clearTo);
                if (hanging)
                    DrawLeaf(canvas, Flipped(step), side, LeafSize::Small, sway, style.Stem, colors);
                else
                    DrawLeaf(canvas, step, side, style.TopLeaf, sway, style.Stem, colors);
            }
        }

        struct Motion
        {
            float Push = 0.0f;
            float Mouse = 0.0f;
            float Sway = 0.0f;
            float Time = 0.0f;
            float Right = 0.0f;
            float Reach = 1.0f;
        };

        ImVec2 Moved(ImVec2 rest, const Motion& motion)
        {
            const float depth = std::max(0.0f, rest.y + 1.0f);
            const float share = std::min(1.0f, depth / motion.Reach);
            const float away = rest.x >= motion.Mouse ? 1.0f : -1.0f;
            const float swing = motion.Sway * share * std::sin(motion.Time * 1.9f - depth * 0.3f);
            const float x = std::clamp(rest.x + swing + motion.Push * away * depth * 0.6f, -1.0f, motion.Right);
            const float y = rest.y - motion.Push * depth * 0.75f;
            return ImVec2(std::round(x), std::round(y));
        }

        void Line(const Canvas& canvas, ImVec2 from, ImVec2 to, ImVec4 color)
        {
            const int steps = static_cast<int>(std::max(std::fabs(to.x - from.x), std::fabs(to.y - from.y)));
            for (int step = 1; step <= steps; ++step)
            {
                const float share = static_cast<float>(step) / static_cast<float>(steps);
                Dot(canvas, std::round(from.x + (to.x - from.x) * share), std::round(from.y + (to.y - from.y) * share), color);
            }
        }

        struct Strand
        {
            bool Swag = false;
            float From = 0.0f;
            float To = 0.0f;
            float Depth = 0.0f;
            std::uint32_t Seed = 0;

            ImVec2 Rest(float share) const
            {
                if (Swag)
                    return ImVec2(From + (To - From) * share, -1.0f + Depth * 4.0f * share * (1.0f - share));
                return ImVec2(From + 1.2f * std::sin(share * 4.4f + static_cast<float>(Seed)), -1.0f + Depth * share);
            }

            float Length() const { return Swag ? To - From : Depth; }
        };

        struct Strands
        {
            Strand List[3];
            int Count = 0;
        };

        Strands PlanStrands(const Vine& vine, float textEnd)
        {
            Strands plan;
            const auto width = static_cast<float>(vine.Width);
            const auto height = static_cast<float>(vine.Height);
            const float creature = std::min(width / 3.0f, 17.0f) + 2.0f;
            const float swagTo = std::min(width - 8.0f, creature + std::max(14.0f, width * 0.4f));
            if (swagTo - creature >= 12.0f)
                plan.List[plan.Count++] = { true, creature, swagTo, 3.0f, 1u };
            const float from = std::max(textEnd + 3.0f, creature);
            const float right = width - 3.0f;
            if (right - from >= 6.0f)
                plan.List[plan.Count++] = { false, right - 1.0f, 0.0f, height - 4.0f, 2u };
            if (right - from >= 15.0f)
                plan.List[plan.Count++] = { false, from + 3.0f, 0.0f, height - 7.0f, 3u };
            return plan;
        }

        void DrawStrand(const Canvas& canvas, const Strand& strand, const Style& style, const Motion& motion)
        {
            const int samples = std::max(4, static_cast<int>(strand.Length() * 1.5f));
            ImVec2 previous = Moved(strand.Rest(0.0f), motion);
            Dot(canvas, previous.x, previous.y, style.Stem);
            for (int sample = 1; sample <= samples; ++sample)
            {
                const ImVec2 at = Moved(strand.Rest(static_cast<float>(sample) / static_cast<float>(samples)), motion);
                Line(canvas, previous, at, style.Stem);
                if (style.Snow && strand.Swag)
                {
                    const std::uint32_t mixed = Hash(sample, strand.Seed * 977u);
                    if (mixed % 10u < 6u)
                        Dot(canvas, at.x, at.y - 1.0f, style.SnowColor);
                    if (mixed % 7u == 0u)
                        Dot(canvas, at.x, at.y + 1.0f, style.Ice);
                }
                previous = at;
            }

            if (!strand.Swag)
            {
                const float side = strand.Seed % 2u == 0u ? 1.0f : -1.0f;
                Dot(canvas, previous.x, previous.y + 1.0f, style.Tendril);
                Dot(canvas, previous.x + side, previous.y + 2.0f, style.Tendril);
                Dot(canvas, previous.x + 2.0f * side, previous.y + 1.0f, style.Tendril);
                Dot(canvas, previous.x + 2.0f * side, previous.y, style.Tendril);
                if (style.Snow)
                    Dot(canvas, previous.x + side, previous.y + 3.0f, style.Ice);
            }

            if (style.Spacing <= 0)
                return;
            const int leaves = std::max(1, static_cast<int>(strand.Length() / (strand.Swag ? 4.0f : 3.0f)));
            for (int leaf = 1; leaf <= leaves; ++leaf)
            {
                const std::uint32_t mixed = Hash(leaf, strand.Seed * 977u);
                if (Bare(style, mixed))
                    continue;
                const float share = (static_cast<float>(leaf) - 0.6f + 0.2f * static_cast<float>(mixed % 3u)) / (static_cast<float>(leaves) + 0.4f);
                const ImVec2 at = Moved(strand.Rest(share), motion);
                const float side = leaf % 2 == 0 ? 1.0f : -1.0f;
                const Leafage& colors = LeafOf(style, mixed);
                if (strand.Swag)
                {
                    const float out = leaf % 2 == 0 ? 1.0f : -1.0f;
                    Dot(canvas, at.x, at.y + out, colors.Leaf);
                    Dot(canvas, at.x + side, at.y + out, colors.Light);
                    Dot(canvas, at.x + side, at.y + 2.0f * out, colors.Leaf);
                    continue;
                }
                Dot(canvas, at.x + side, at.y, colors.Leaf);
                Dot(canvas, at.x + 2.0f * side, at.y, colors.Light);
                Dot(canvas, at.x + side, at.y + 1.0f, colors.Vein);
                Dot(canvas, at.x + 2.0f * side, at.y + 1.0f, colors.Leaf);
                if (style.TopLeaf == LeafSize::Big)
                {
                    Dot(canvas, at.x + 3.0f * side, at.y + 1.0f, colors.Leaf);
                    Dot(canvas, at.x + 2.0f * side, at.y + 2.0f, colors.Leaf);
                }
            }
        }

        float WindFront(const Vine& vine, float share)
        {
            return share < 0.0f ? -1000.0f : share * static_cast<float>(vine.Count() + 8) - 4.0f;
        }

        Leafage Shades(ImVec4 leaf, ImVec4 stem, float light)
        {
            return { Lit(leaf, light), Lit(Mix(leaf, Rgb(1.0f, 0.95f, 0.75f), 0.3f), light), Lit(Mix(leaf, stem, 0.35f), light) };
        }

        struct Scene
        {
            const Canvas& Paint;
            const Vine& Shape;
            const Strands& Hanging;
            const Motion& Movement;
            float Width = 0.0f;
            float Height = 0.0f;
            float Daylight = 1.0f;
            float Light = 1.0f;
            float Share = -1.0f;
        };

        void DrawAll(const Scene& scene, const Style& style, float front)
        {
            float clearFrom = 1.0e6f;
            float clearTo = -1.0e6f;
            for (int index = 0; index < scene.Hanging.Count; ++index)
            {
                const Strand& strand = scene.Hanging.List[index];
                if (strand.Swag)
                {
                    clearFrom = strand.From - 2.0f;
                    clearTo = strand.To + 2.0f;
                }
            }
            DrawVine(scene.Paint, scene.Shape, style, front, clearFrom, clearTo);
            for (int index = 0; index < scene.Hanging.Count; ++index)
                DrawStrand(scene.Paint, scene.Hanging.List[index], style, scene.Movement);
        }

        void DrawSpring(const Scene& scene)
        {
            Style style;
            style.Stem = Lit(Rgb(0.40f, 0.62f, 0.28f), scene.Light);
            style.Tendril = Lit(Rgb(0.52f, 0.76f, 0.34f), scene.Light);
            style.Leaves[0] = { Lit(Rgb(0.56f, 0.84f, 0.38f), scene.Light), Lit(Rgb(0.74f, 0.94f, 0.52f), scene.Light), Lit(Rgb(0.48f, 0.76f, 0.32f), scene.Light) };
            style.Spacing = 5;
            style.TopLeaf = LeafSize::Small;
            DrawAll(scene, style, WindFront(scene.Shape, scene.Share));
        }

        void DrawSummer(const Scene& scene)
        {
            const ImVec4 stem = Rgb(0.28f, 0.44f, 0.20f);
            Style style;
            style.Stem = Lit(stem, scene.Light);
            style.Tendril = Lit(Rgb(0.36f, 0.56f, 0.24f), scene.Light);
            style.Leaves[0] = Shades(Rgb(0.30f, 0.58f, 0.24f), stem, scene.Light);
            style.Leaves[1] = Shades(Rgb(0.36f, 0.66f, 0.28f), stem, scene.Light);
            style.LeafKinds = 2;
            style.Spacing = 3;
            style.TopLeaf = LeafSize::Big;
            DrawAll(scene, style, WindFront(scene.Shape, scene.Share));
        }

        void DrawAutumn(const Scene& scene)
        {
            const ImVec4 stem = Rgb(0.42f, 0.28f, 0.18f);
            Style style;
            style.Stem = Lit(stem, scene.Light);
            style.Tendril = Lit(Rgb(0.50f, 0.34f, 0.20f), scene.Light);
            style.Leaves[0] = Shades(Rgb(0.84f, 0.22f, 0.14f), stem, scene.Light);
            style.Leaves[1] = Shades(Rgb(0.93f, 0.50f, 0.14f), stem, scene.Light);
            style.Leaves[2] = Shades(Rgb(0.96f, 0.74f, 0.26f), stem, scene.Light);
            style.Leaves[3] = Shades(Rgb(0.66f, 0.14f, 0.12f), stem, scene.Light);
            style.LeafKinds = 4;
            style.Spacing = 3;
            style.TopLeaf = LeafSize::Big;
            style.BareShare = 30;
            DrawAll(scene, style, WindFront(scene.Shape, scene.Share));
            if (scene.Share < 0.0f)
                return;
            float origin = scene.Width * 0.6f;
            for (int index = 0; index < scene.Hanging.Count; ++index)
            {
                if (!scene.Hanging.List[index].Swag)
                    origin = scene.Hanging.List[index].From - 2.0f;
            }
            const float share = scene.Share;
            const float y = std::round(1.0f + share * (scene.Height + 1.0f));
            const float x = std::round(origin + Wave(share, 6.0f, 0.0f) * 3.0f - share * 4.0f);
            const Leafage& colors = style.Leaves[1];
            const bool turned = std::fmod(share * 8.0f, 1.0f) < 0.5f;
            scene.Paint.Fill(x, y, x + 2.0f, y + 1.0f, colors.Leaf);
            scene.Paint.Fill(x + (turned ? 1.0f : -1.0f), y + 1.0f, x + (turned ? 3.0f : 1.0f), y + 2.0f, colors.Leaf);
            Dot(scene.Paint, x, y + 1.0f, colors.Vein);
            Dot(scene.Paint, x + (turned ? 0.0f : 1.0f), y - 1.0f, colors.Light);
        }

        void DrawWinter(const Scene& scene)
        {
            const Canvas& canvas = scene.Paint;
            const Vine& vine = scene.Shape;
            const float dark = 1.0f - scene.Daylight;
            const ImVec4 snow = Mix(Rgb(0.95f, 0.97f, 1.0f), Rgb(0.62f, 0.67f, 0.80f), dark * 0.6f);
            const ImVec4 shade = Mix(Rgb(0.76f, 0.84f, 0.95f), Rgb(0.44f, 0.50f, 0.66f), dark * 0.6f);
            const ImVec4 ice = Mix(Rgb(0.80f, 0.92f, 1.0f), Rgb(0.45f, 0.55f, 0.72f), dark * 0.6f);
            const ImVec4 stem = Rgb(0.40f, 0.33f, 0.30f);
            Style style;
            style.Stem = Lit(stem, scene.Light);
            style.Tendril = Lit(Rgb(0.46f, 0.38f, 0.33f), scene.Light);
            style.Snow = true;
            style.SnowColor = snow;
            style.Ice = ice;
            DrawAll(scene, style, -1000.0f);

            const Leafage dried = Shades(Rgb(0.58f, 0.44f, 0.30f), stem, scene.Light);
            for (const int spot : { vine.Top * 14 / 100, vine.Top * 52 / 100, vine.Top * 84 / 100 })
                DrawLeaf(canvas, vine.At(vine.Left + spot), spot % 2 == 0 ? 1.0f : -1.0f, LeafSize::Small, 0.0f, style.Stem, dried);

            const int glint = scene.Share < 0.0f ? -100 : static_cast<int>(scene.Share * static_cast<float>(vine.Top));
            for (int index = 0; index < vine.Left + vine.Top; ++index)
            {
                const Step step = vine.At(index);
                const std::uint32_t mixed = Hash(index, 313u);
                if (!step.Top)
                {
                    if (mixed % 4u == 0u)
                        Put(canvas, step, 0.0f, 1.0f, shade);
                    continue;
                }
                const int top = index - vine.Left;
                if (mixed % 10u < 7u)
                    Put(canvas, step, 0.0f, 1.0f, std::abs(top - glint) <= 1 ? Rgb(1.0f, 1.0f, 1.0f) : snow);
                if (mixed % 5u == 0u)
                    Put(canvas, step, 0.0f, 2.0f, top == glint ? Rgb(1.0f, 1.0f, 1.0f) : snow);
                if (mixed % 9u == 0u)
                {
                    Put(canvas, step, 0.0f, -1.0f, ice);
                    if (mixed % 2u == 0u)
                        Put(canvas, step, 0.0f, -2.0f, ice);
                }
            }
            if (scene.Share < 0.0f)
                return;
            for (int flake = 0; flake < 2; ++flake)
            {
                const float fall = scene.Share * 1.35f - 0.2f * static_cast<float>(flake);
                if (fall < 0.0f || fall > 1.0f)
                    continue;
                const float x = std::round(scene.Width * (0.28f + 0.42f * static_cast<float>(flake)) + Wave(fall, 7.0f, static_cast<float>(flake)) * 2.0f);
                const float y = std::round(-6.0f + fall * (scene.Height + 7.0f));
                Dot(canvas, x, y, snow);
            }
        }
    }

    void DrawSeasonFrame(ImDrawList* drawList, ImVec2 min, ImVec2 max, float textEnd, SceneSeason season, float daylight, bool hovered)
    {
        const double now = ImGui::GetTime();
        if (hovered && !g_wasHovered)
            g_hoverStarted = now;
        g_wasHovered = hovered;

        const float pixel = IdleAnimation::PixelSize();
        const ImVec2 origin(std::round(min.x), std::round(min.y));
        const Canvas canvas{ drawList, origin, pixel, 0.0f, ImVec4() };
        const float width = std::floor((max.x - min.x) / pixel);
        const float height = std::round((max.y - min.y) / pixel);
        if (hovered)
            g_mouse = (ImGui::GetIO().MousePos.x - origin.x) / pixel;
        const bool alreadyMoving = IdleAnimation::NeedsFrames();
        UpdatePush(hovered);

        const Vine vine(width, height);
        const Strands strands = PlanStrands(vine, std::ceil((textEnd - origin.x) / pixel));
        const float share = EventShare(now, static_cast<double>(season) * 2.7);
        if (share >= 0.0f)
            IdleAnimation::RequestFrames();
        Motion motion;
        motion.Push = g_push;
        motion.Mouse = g_mouse;
        motion.Time = static_cast<float>(now);
        motion.Right = width + 1.0f;
        motion.Reach = std::max(1.0f, height - 3.0f);
        motion.Sway = (share >= 0.0f ? 1.6f * std::sin(share * kPi) : 0.0f) + (alreadyMoving && IdleAnimation::Enabled() ? 0.7f : 0.0f);

        const Scene scene{ canvas, vine, strands, motion, width, height, daylight, 0.45f + 0.55f * daylight, share };
        switch (season)
        {
        case SceneSeason::Spring:
            DrawSpring(scene);
            break;
        case SceneSeason::Summer:
            DrawSummer(scene);
            break;
        case SceneSeason::Autumn:
            DrawAutumn(scene);
            break;
        default:
            DrawWinter(scene);
            break;
        }
    }
}
