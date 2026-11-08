#include "UI/IdleAnimation.h"

#include "UI/CreatureSprites.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Idler::UI::IdleAnimation
{
    namespace
    {
        float g_squash = 0.0f;

        using namespace CreatureSprites;

        constexpr double kTickSeconds = 0.125;

        constexpr ImVec4 kRed = ImVec4(0.90f, 0.30f, 0.26f, 1.0f);
        constexpr ImVec4 kYellow = ImVec4(1.00f, 0.82f, 0.30f, 1.0f);
        constexpr ImVec4 kWhite = ImVec4(0.94f, 0.95f, 0.97f, 1.0f);
        constexpr ImVec4 kDark = ImVec4(0.27f, 0.27f, 0.33f, 1.0f);
        constexpr ImVec4 kGold = ImVec4(1.00f, 0.78f, 0.22f, 1.0f);
        constexpr ImVec4 kGoldLight = ImVec4(1.00f, 0.92f, 0.60f, 1.0f);

        struct Palette
        {
            ImU32 Body, Shade, Light, Second, SecondShade, SecondLight, Third, ThirdShade, Eye, Red, Yellow, White, Dark;
        };

        constexpr std::array<const char*, 4> kZ = { "####", "..#.", ".#..", "####" };

        CreatureSet g_found = CreatureSet().set(0);
        CreatureSet g_evolved;

        static_assert(std::size(kEvolutions) == std::size(kEvolvedSprites), "every evolution in Core/Creatures.h needs its picture in Tools/CreatureForge/creatures/evolved");
        constexpr bool EvolutionsHavePictures()
        {
            for (std::size_t index = 0; index < std::size(kEvolutions); ++index)
            {
                if (kEvolutions[index].Id != kEvolvedSprites[index].Id)
                    return false;
            }
            return true;
        }
        static_assert(EvolutionsHavePictures(), "every evolution in Core/Creatures.h needs its picture in Tools/CreatureForge/creatures/evolved");

        const Sprite& SpriteOf(CreatureId id, Form form)
        {
            const auto index = static_cast<std::size_t>(id);
            if (form == Form::Shown && g_evolved.test(index))
            {
                if (index < std::size(kEvolvedSprites) && kEvolvedSprites[index].Id == id)
                    return kEvolvedSprites[index];
                for (const Sprite& sprite : kEvolvedSprites)
                {
                    if (sprite.Id == id)
                        return sprite;
                }
            }
            return kSprites[static_cast<std::size_t>(id)];
        }
        bool g_upsideDown = false;
        bool g_enabled = true;
        bool g_needsFrames = false;
        bool g_fullRate = false;
        Part g_part = Part::Whole;
        float g_haze = 0.0f;
        ImVec4 g_hazeColor;

        ImU32 Hazed(ImU32 color)
        {
            if (g_haze <= 0.0f || color == 0)
                return color;
            const ImVec4 own = ImGui::ColorConvertU32ToFloat4(color);
            return ImGui::GetColorU32(ImVec4(own.x + (g_hazeColor.x - own.x) * g_haze, own.y + (g_hazeColor.y - own.y) * g_haze, own.z + (g_hazeColor.z - own.z) * g_haze, own.w));
        }

        bool IsSecondColor(char cell)
        {
            return cell == 'a' || cell == 'd' || cell == 'l';
        }

        int ShellLeftOf(const Frame& frame)
        {
            int left = kColumns;
            for (const char* line : frame)
            {
                for (int column = 0; column < kColumns && line[column] != 0; ++column)
                {
                    if (IsSecondColor(line[column]))
                        left = std::min(left, column);
                }
            }
            return left;
        }

        bool IsShellCell(char cell, int column, int shellLeft)
        {
            if (IsSecondColor(cell))
                return true;
            return column >= shellLeft && (cell == 'r' || cell == 'w' || cell == 'k' || cell == 'y');
        }

        ImU32 Scaled(ImVec4 color, float brightness)
        {
            return ImGui::GetColorU32(ImVec4(std::min(1.0f, color.x * brightness), std::min(1.0f, color.y * brightness), std::min(1.0f, color.z * brightness), 1.0f));
        }

        ImU32 Lightened(ImVec4 color)
        {
            return ImGui::GetColorU32(ImVec4(color.x + (1.0f - color.x) * 0.6f, color.y + (1.0f - color.y) * 0.6f, color.z + (1.0f - color.z) * 0.6f, 1.0f));
        }

        Palette ColoredPalette(ImVec4 body, ImVec4 second, ImVec4 third)
        {
            if (second.w <= 0.0f)
                second = body;
            if (third.w <= 0.0f)
                third = second;

            Palette palette;
            palette.Body = Scaled(body, 1.0f);
            palette.Shade = Scaled(body, 0.62f);
            palette.Light = Lightened(body);
            palette.Second = Scaled(second, 1.0f);
            palette.SecondShade = Scaled(second, 0.62f);
            palette.SecondLight = Lightened(second);
            palette.Third = Scaled(third, 1.0f);
            palette.ThirdShade = Scaled(third, 0.62f);
            palette.Eye = ImGui::GetColorU32(ManifoldTheme::Sidebar);
            palette.Red = ImGui::GetColorU32(kRed);
            palette.Yellow = ImGui::GetColorU32(kYellow);
            palette.White = ImGui::GetColorU32(kWhite);
            palette.Dark = ImGui::GetColorU32(kDark);
            return palette;
        }

        Palette GreyPalette()
        {
            const ImVec4 grey = ManifoldTheme::Muted;
            Palette palette = ColoredPalette(grey, ManifoldTheme::Label, ImVec4(grey.x * 0.85f, grey.y * 0.85f, grey.z * 0.85f, 1.0f));
            palette.Red = palette.Yellow = Scaled(grey, 1.15f);
            palette.White = ImGui::GetColorU32(ManifoldTheme::Label);
            return palette;
        }

        Palette ShadowPalette()
        {
            const ImU32 shadow = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
            return { shadow, shadow, shadow, shadow, shadow, shadow, shadow, shadow, shadow, shadow, shadow, shadow, shadow };
        }

        void DrawFrame(ImDrawList* drawList, ImVec2 at, const Frame& frame, float pixel, bool blink, const Palette& palette, bool mirrored = false)
        {
            const auto colorOf = [&](char cell) -> ImU32
                {
                    switch (cell)
                    {
                    case '#': return palette.Body;
                    case 'o': return palette.Shade;
                    case '+': return palette.Light;
                    case 'a': return palette.Second;
                    case 'd': return palette.SecondShade;
                    case 'l': return palette.SecondLight;
                    case 'c': return palette.Third;
                    case 'C': return palette.ThirdShade;
                    case '*': return blink ? palette.Shade : palette.Eye;
                    case 'e': return blink ? palette.SecondShade : palette.Eye;
                    case 'r': return palette.Red;
                    case 'y': return palette.Yellow;
                    case 'w': return palette.White;
                    case 'k': return palette.Dark;
                    default: return 0;
                    }
                };

            const float tall = pixel * (1.0f - g_squash * 0.55f);
            const float wide = pixel * (1.0f + g_squash * 0.25f);
            const float ground = at.y + pixel * static_cast<float>(kRows);
            const float middle = at.x + pixel * static_cast<float>(kColumns) * 0.5f;

            const int shellLeft = g_part == Part::Whole ? kColumns : ShellLeftOf(frame);
            for (int row = 0; row < kRows; ++row)
            {
                const char* line = frame[static_cast<std::size_t>(g_upsideDown ? kRows - 1 - row : row)];
                const auto colorAt = [&](int at_)
                    {
                        const int source = mirrored ? kColumns - 1 - at_ : at_;
                        const char cell = line[source];
                        if (g_part != Part::Whole && IsShellCell(cell, source, shellLeft) != (g_part == Part::ShellOnly))
                            return ImU32{ 0 };
                        return Hazed(colorOf(cell));
                    };
                int column = 0;
                while (column < kColumns)
                {
                    const ImU32 color = colorAt(column);
                    int end = column + 1;
                    while (end < kColumns && colorAt(end) == color)
                        ++end;
                    if (color != 0 && g_squash <= 0.0f)
                    {
                        const ImVec2 min(at.x + pixel * static_cast<float>(column), at.y + pixel * static_cast<float>(row));
                        drawList->AddRectFilled(min, ImVec2(at.x + pixel * static_cast<float>(end), min.y + pixel), color);
                    }
                    else if (color != 0)
                    {
                        const float half = static_cast<float>(kColumns) * 0.5f;
                        drawList->AddRectFilled(ImVec2(std::round(middle + wide * (static_cast<float>(column) - half)), std::round(ground - tall * static_cast<float>(kRows - row))),
                            ImVec2(std::round(middle + wide * (static_cast<float>(end) - half)), std::round(ground - tall * static_cast<float>(kRows - row - 1))), color);
                    }
                    column = end;
                }
            }
        }

        const Step& StepAt(const Sprite& sprite, double time, double offsetTicks)
        {
            int total = 0;
            for (int i = 0; i < sprite.StepCount; ++i)
                total += sprite.Steps[i].Ticks;
            auto tick = static_cast<long long>(std::floor(time / kTickSeconds + offsetTicks));
            tick %= total;
            for (int i = 0; i < sprite.StepCount; ++i)
            {
                if (tick < sprite.Steps[i].Ticks)
                    return sprite.Steps[i];
                tick -= sprite.Steps[i].Ticks;
            }
            return sprite.Steps[0];
        }

        void DrawSleeping(ImDrawList* drawList, ImVec2 at, const Sprite& sprite, float pixel, double time, double offset, bool mirrored)
        {
            const bool breathOut = std::fmod(time / 1.6 + offset, 2.0) >= 1.0;
            DrawFrame(drawList, ImVec2(at.x, at.y + (breathOut ? pixel : 0.0f)), sprite.Frames[sprite.SleepFrame], pixel, true, GreyPalette(), mirrored);

            for (int letter = 0; letter < 2; ++letter)
            {
                const double phase = std::fmod(time / 3.0 + offset + letter * 0.5, 1.0);
                const float alpha = static_cast<float>(phase < 0.2 ? phase / 0.2 : 1.0 - (phase - 0.2) / 0.8);
                const float x = std::round(at.x + pixel * (static_cast<float>(kColumns) - 3.0f + static_cast<float>(phase) * 2.0f));
                const float y = std::round(at.y + pixel * (static_cast<float>(kRows) - 7.0f - static_cast<float>(phase) * 6.0f));
                const ImU32 color = ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Label, alpha));
                const float small = std::max(1.0f, std::round(pixel * 0.5f));
                for (int row = 0; row < static_cast<int>(kZ.size()); ++row)
                {
                    for (int column = 0; column < 4; ++column)
                    {
                        if (kZ[static_cast<std::size_t>(row)][column] != '#')
                            continue;
                        const ImVec2 min(x + small * static_cast<float>(column), y + small * static_cast<float>(row));
                        drawList->AddRectFilled(min, ImVec2(min.x + small, min.y + small), color);
                    }
                }
            }
        }

        void DrawStarting(ImDrawList* drawList, ImVec2 at, float pixel, double time, double offset)
        {
            const float size = pixel * 2.0f;
            const float y = at.y + pixel * static_cast<float>(kRows) - size - pixel * 3.0f;
            for (int dot = 0; dot < 3; ++dot)
            {
                const double phase = std::fmod(time * 1.2 + offset - dot * 0.18 + 2.0, 1.0);
                const float rise = phase < 0.35 ? std::round(pixel * 2.0f * static_cast<float>(std::sin(phase / 0.35 * 3.14159265))) : 0.0f;
                const float x = at.x + pixel * (static_cast<float>(kColumns - 10) * 0.5f + 4.0f * static_cast<float>(dot));
                drawList->AddRectFilled(ImVec2(x, y - rise), ImVec2(x + size, y - rise + size), ImGui::GetColorU32(ManifoldTheme::Muted));
            }
        }

        void DrawSparkle(ImDrawList* drawList, ImVec2 at, float pixel, double time, std::uint32_t seed)
        {
            const double beat = time / 0.5;
            const auto number = static_cast<std::uint32_t>(std::floor(beat));
            const std::uint32_t place = MixSeed(seed ^ number);
            const double phase = beat - std::floor(beat);
            const auto shine = static_cast<float>(1.0 - std::fabs(phase * 2.0 - 1.0));
            const float x = at.x + pixel * static_cast<float>(place % kColumns);
            const float y = at.y + pixel * static_cast<float>((place >> 8) % kRows);
            const ImU32 color = ImGui::GetColorU32(ManifoldTheme::WithAlpha(kWhite, shine));
            drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + pixel, y + pixel), color);
            if (shine > 0.5f)
            {
                drawList->AddRectFilled(ImVec2(x - pixel, y), ImVec2(x + pixel * 2.0f, y + pixel), color);
                drawList->AddRectFilled(ImVec2(x, y - pixel), ImVec2(x + pixel, y + pixel * 2.0f), color);
            }
        }
    }

    float PixelSize()
    {
        return std::max(1.0f, std::round(Px(2.0f)));
    }

    ImVec2 SizeAt(float pixel)
    {
        return ImVec2(pixel * static_cast<float>(kColumns), pixel * static_cast<float>(kRows));
    }

    float Width()
    {
        return SizeAt(PixelSize()).x;
    }

    float Height()
    {
        return SizeAt(PixelSize()).y;
    }

    void SetFound(const CreatureSet& found)
    {
        g_found = found;
    }

    void SetEvolved(const CreatureSet& evolved)
    {
        g_evolved = evolved;
    }

    void SetUpsideDown(bool upsideDown)
    {
        g_upsideDown = upsideDown;
    }

    void DrawCreature(ImDrawList* drawList, ImVec2 at, CreatureId id, Mood mood, std::uint32_t seed, float pixel, Look look, bool mirrored, float squash, Form form, Part part)
    {
        g_part = part;
        struct Unpart
        {
            ~Unpart() { g_part = Part::Whole; }
        } unpart;
        const ImVec2 origin(std::round(at.x), std::round(at.y));
        g_squash = squash;
        struct Unsquash
        {
            ~Unsquash() { g_squash = 0.0f; }
        } unsquash;
        const Sprite& sprite = SpriteOf(id, look == Look::Unknown ? Form::First : form);
        if (look == Look::Unknown)
        {
            DrawFrame(drawList, origin, sprite.Frames[0], pixel, false, ShadowPalette());
            return;
        }

        const std::uint32_t mixed = MixSeed(seed);
        const double time = g_enabled ? ImGui::GetTime() : 0.0;
        const double offset = static_cast<double>(mixed >> 16) / 65536.0;
        if (g_enabled)
            g_needsFrames = true;

        switch (mood)
        {
        case Mood::Sleeping:
            DrawSleeping(drawList, origin, sprite, pixel, time, offset, mirrored);
            break;
        case Mood::Starting:
            DrawStarting(drawList, origin, pixel, time, offset);
            break;
        default:
        {
            const double speed = 0.92 + 0.16 * static_cast<double>((mixed >> 8) & 0xFF) / 255.0;
            const Step& step = StepAt(sprite, time * speed, offset * 64.0);
            const ImVec2 where(origin.x + pixel * static_cast<float>(mirrored ? -step.Shift : step.Shift), origin.y - pixel * static_cast<float>(step.Lift));
            const Palette palette = look == Look::Shiny ? ColoredPalette(kGold, kGoldLight, kGold) : ColoredPalette(sprite.Body, sprite.Second, sprite.Third);
            DrawFrame(drawList, where, sprite.Frames[step.FrameIndex], pixel, step.Blink, palette, mirrored);
            if (look == Look::Shiny && g_enabled)
                DrawSparkle(drawList, where, pixel, time, seed);
            break;
        }
        }
    }

    void Draw(ImDrawList* drawList, ImVec2 at, Mood mood, std::uint32_t seed, bool mirrored)
    {
        DrawCreature(drawList, at, PickCreature(seed, g_found), mood, seed, PixelSize(), IsShiny(seed) ? Look::Shiny : Look::Normal, mirrored);
    }

    CreatureId CreatureOf(std::uint32_t seed)
    {
        return PickCreature(seed, g_found);
    }

    Footprint FootprintOf(CreatureId id, bool mirrored)
    {
        const Frame& frame = SpriteOf(id, Form::Shown).Frames[0];
        const auto lineAt = [&](int row) { return frame[static_cast<std::size_t>(g_upsideDown ? kRows - 1 - row : row)]; };
        const auto filled = [](const char* line, int column) { return line[column] != '\0' && line[column] != '.'; };
        int lowest = -1;
        for (int row = kRows - 1; row >= 0 && lowest < 0; --row)
        {
            for (int column = 0; column < kColumns && lineAt(row)[column] != 0; ++column)
            {
                if (filled(lineAt(row), column))
                {
                    lowest = row;
                    break;
                }
            }
        }
        Footprint footprint{ kColumns / 2 - 3, kColumns / 2 + 2, kRows };
        if (lowest < 0)
            return footprint;
        int left = kColumns;
        int right = -1;
        for (int row = std::max(0, lowest - 1); row <= lowest; ++row)
        {
            for (int column = 0; column < kColumns && lineAt(row)[column] != 0; ++column)
            {
                if (filled(lineAt(row), column))
                {
                    left = std::min(left, column);
                    right = std::max(right, column);
                }
            }
        }
        footprint.Ground = lowest + 1;
        footprint.Left = mirrored ? kColumns - 1 - right : left;
        footprint.Right = mirrored ? kColumns - 1 - left : right;
        return footprint;
    }

    void SetHaze(float amount, ImVec4 color)
    {
        g_haze = std::clamp(amount, 0.0f, 1.0f);
        g_hazeColor = color;
    }

    ImVec2 MotionOf(CreatureId id, std::uint32_t seed)
    {
        const Sprite& sprite = SpriteOf(id, Form::Shown);
        const std::uint32_t mixed = MixSeed(seed);
        const double time = g_enabled ? ImGui::GetTime() : 0.0;
        const double offset = static_cast<double>(mixed >> 16) / 65536.0;
        const double speed = 0.92 + 0.16 * static_cast<double>((mixed >> 8) & 0xFF) / 255.0;
        const Step& step = StepAt(sprite, time * speed, offset * 64.0);
        return ImVec2(static_cast<float>(step.Shift), static_cast<float>(step.Lift));
    }

    void DrawShell(ImDrawList* drawList, ImVec2 center, CreatureId id, float pixel, bool mirrored)
    {
        const Sprite& sprite = SpriteOf(id, Form::Shown);
        const Frame& frame = sprite.Frames[0];
        const int shellLeft = ShellLeftOf(frame);
        if (shellLeft >= kColumns)
            return;
        int left = kColumns;
        int right = -1;
        int top = kRows;
        int bottom = -1;
        for (int row = 0; row < kRows; ++row)
        {
            const char* line = frame[static_cast<std::size_t>(g_upsideDown ? kRows - 1 - row : row)];
            for (int column = 0; column < kColumns && line[column] != 0; ++column)
            {
                if (!IsShellCell(line[column], column, shellLeft))
                    continue;
                const int drawn = mirrored ? kColumns - 1 - column : column;
                left = std::min(left, drawn);
                right = std::max(right, drawn);
                top = std::min(top, row);
                bottom = std::max(bottom, row);
            }
        }
        const ImVec2 origin(std::round(center.x - pixel * static_cast<float>(left + right + 1) * 0.5f), std::round(center.y - pixel * static_cast<float>(top + bottom + 1) * 0.5f));
        g_part = Part::ShellOnly;
        DrawFrame(drawList, origin, frame, pixel, false, ColoredPalette(sprite.Body, sprite.Second, sprite.Third), mirrored);
        g_part = Part::Whole;
    }

    int FacingOf(CreatureId id)
    {
        return kSprites[static_cast<std::size_t>(id)].Facing;
    }

    Habit HabitOf(CreatureId id)
    {
        const Sprite& sprite = kSprites[static_cast<std::size_t>(id)];
        return sprite.Stays ? Habit::Stays : sprite.Swims ? Habit::Swims : sprite.Floats ? Habit::Floats : Habit::Walks;
    }

    void DrawMark(ImDrawList* drawList, ImVec2 center, float pixel, Mark mark, float alpha)
    {
        constexpr std::array<const char*, 5> kNote = { "..##.", "..#.#", "..#..", "###..", "###.." };
        constexpr std::array<const char*, 5> kQuestion = { ".###.", "...#.", "..#..", ".....", "..#.." };
        constexpr std::array<const char*, 5> kAlarm = { "..#..", "..#..", "..#..", ".....", "..#.." };
        constexpr std::array<const char*, 5> kDots = { ".....", ".....", ".....", ".....", "#.#.#" };
        const auto& shape = mark == Mark::Note ? kNote : mark == Mark::Question ? kQuestion : mark == Mark::Alarm ? kAlarm : kDots;
        const ImVec4 tone = mark == Mark::Note ? ManifoldTheme::Accent : mark == Mark::Dots ? ManifoldTheme::Label : ManifoldTheme::Warning;
        const ImU32 color = ImGui::GetColorU32(ManifoldTheme::WithAlpha(tone, alpha));
        const ImU32 edge = ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.08f, 0.85f * alpha));
        const float left = std::round(center.x - pixel * 2.5f);
        const float top = std::round(center.y - pixel * 5.0f);
        for (const bool edgePass : { true, false })
        {
            const float grow = edgePass ? std::max(1.0f, std::round(pixel * 0.5f)) : 0.0f;
            for (int row = 0; row < 5; ++row)
            {
                for (int column = 0; column < 5; ++column)
                {
                    if (shape[static_cast<std::size_t>(row)][column] != '#')
                        continue;
                    const ImVec2 min(left + pixel * static_cast<float>(column), top + pixel * static_cast<float>(row));
                    drawList->AddRectFilled(ImVec2(min.x - grow, min.y - grow), ImVec2(min.x + pixel + grow, min.y + pixel + grow), edgePass ? edge : color);
                }
            }
        }
    }

    void DrawDizzy(ImDrawList* drawList, ImVec2 center, float pixel, float age)
    {
        const ImU32 color = ImGui::GetColorU32(ManifoldTheme::Warning);
        const ImU32 edge = ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.08f, 0.85f));
        for (const bool edgePass : { true, false })
        {
            const float grow = edgePass ? std::max(1.0f, std::round(pixel * 0.5f)) : 0.0f;
            for (int star = 0; star < 3; ++star)
            {
                const float angle = age * 6.0f + static_cast<float>(star) * 2.0944f;
                const float x = std::round(center.x + std::cos(angle) * pixel * 7.0f);
                const float y = std::round(center.y + std::sin(angle) * pixel * 2.0f);
                drawList->AddRectFilled(ImVec2(x - pixel - grow, y - grow), ImVec2(x + pixel * 2.0f + grow, y + pixel + grow), edgePass ? edge : color);
                drawList->AddRectFilled(ImVec2(x - grow, y - pixel - grow), ImVec2(x + pixel + grow, y + pixel * 2.0f + grow), edgePass ? edge : color);
            }
        }
    }

    void DrawAngerMark(ImDrawList* drawList, ImVec2 center, float pixel, bool beat)
    {
        constexpr std::array<const char*, 8> kMark = { "..#..#..", "..#..#..", "########", "..#..#..", "..#..#..", "########", "..#..#..", "..#..#.." };
        const float cell = std::max(2.0f, std::round(pixel * 0.75f)) + (beat ? 1.0f : 0.0f);
        const ImVec2 origin(std::round(center.x - cell * 4.0f), std::round(center.y - cell * 4.0f));
        const auto square = [&](int column, int row, ImU32 color)
            {
                const ImVec2 min(origin.x + cell * static_cast<float>(column), origin.y + cell * static_cast<float>(row));
                drawList->AddRectFilled(min, ImVec2(min.x + cell, min.y + cell), color);
            };
        for (int row = 0; row < 8; ++row)
        {
            for (int column = 0; column < 8; ++column)
            {
                if (kMark[static_cast<std::size_t>(row)][column] != '#')
                    continue;
                for (const auto [x, y] : { std::pair{ -1, 0 }, std::pair{ 1, 0 }, std::pair{ 0, -1 }, std::pair{ 0, 1 } })
                    square(column + x, row + y, IM_COL32(245, 240, 236, 255));
            }
        }
        for (int row = 0; row < 8; ++row)
        {
            for (int column = 0; column < 8; ++column)
            {
                if (kMark[static_cast<std::size_t>(row)][column] == '#')
                    square(column, row, IM_COL32(226, 48, 48, 255));
            }
        }
    }

    void DrawHeart(ImDrawList* drawList, ImVec2 at, float pixel, float alpha)
    {
        constexpr std::array<const char*, 4> kHeart = { "##.##", "#####", ".###.", "..#.." };
        const ImU32 color = ImGui::GetColorU32(ImVec4(0.95f, 0.36f, 0.50f, alpha));
        for (int row = 0; row < static_cast<int>(kHeart.size()); ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if (kHeart[static_cast<std::size_t>(row)][column] != '#')
                    continue;
                const ImVec2 min(at.x + pixel * static_cast<float>(column), at.y + pixel * static_cast<float>(row));
                drawList->AddRectFilled(min, ImVec2(min.x + pixel, min.y + pixel), color);
            }
        }
    }

    std::string NameOf(std::uint32_t seed)
    {
        const char* name = ShownName(PickCreature(seed, g_found));
        return IsShiny(seed) ? std::string("Shiny ") + name : std::string(name);
    }

    const char* ShownName(CreatureId id)
    {
        const CreatureEvolution* evolution = g_evolved.test(static_cast<std::size_t>(id)) ? EvolutionOf(id) : nullptr;
        return evolution ? evolution->Name : InfoOf(id).Name;
    }

    const char* ShownFlavour(CreatureId id)
    {
        const CreatureEvolution* evolution = g_evolved.test(static_cast<std::size_t>(id)) ? EvolutionOf(id) : nullptr;
        return evolution ? evolution->Flavour : InfoOf(id).Flavour;
    }

    double Moment(std::uint32_t seed)
    {
        return static_cast<double>(MixSeed(seed ^ 0x9E3779B9u) >> 16) / 65536.0;
    }

    void SetEnabled(bool enabled)
    {
        g_enabled = enabled;
    }

    bool Enabled()
    {
        return g_enabled;
    }

    void BeginFrame()
    {
        g_needsFrames = false;
        g_fullRate = false;
    }

    bool NeedsFrames()
    {
        return g_needsFrames;
    }

    void RequestFrames()
    {
        if (g_enabled)
            g_needsFrames = true;
    }

    void KeepFrames()
    {
        g_needsFrames = true;
        g_fullRate = true;
    }

    bool NeedsFullRate()
    {
        return g_fullRate;
    }
}
