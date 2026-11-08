#pragma once

#include "Core/Creatures.h"
#include "Core/Terrarium.h"

#include "imgui.h"

#include <cstdint>
#include <string>

namespace Idler::UI::IdleAnimation
{
    enum class Mood
    {
        Idling,
        Sleeping,
        Starting
    };

    enum class Look
    {
        Normal,
        Shiny,
        Unknown
    };

    float Width();
    float Height();
    float PixelSize();
    ImVec2 SizeAt(float pixel);

    void SetFound(const CreatureSet& found);
    void SetEvolved(const CreatureSet& evolved);

    enum class Form
    {
        Shown,
        First
    };
    void SetUpsideDown(bool upsideDown);

    void Draw(ImDrawList* drawList, ImVec2 at, Mood mood, std::uint32_t seed, bool mirrored = false);
    enum class Part
    {
        Whole,
        WithoutShell,
        ShellOnly
    };

    void DrawCreature(ImDrawList* drawList, ImVec2 at, CreatureId id, Mood mood, std::uint32_t seed, float pixel, Look look = Look::Normal, bool mirrored = false, float squash = 0.0f,
        Form form = Form::Shown, Part part = Part::Whole);
    void SetHaze(float amount, ImVec4 color);
    ImVec2 MotionOf(CreatureId id, std::uint32_t seed);

    void DrawShell(ImDrawList* drawList, ImVec2 center, CreatureId id, float pixel, bool mirrored);

    std::string NameOf(std::uint32_t seed);
    const char* ShownName(CreatureId id);
    const char* ShownFlavour(CreatureId id);
    CreatureId CreatureOf(std::uint32_t seed);
    struct Footprint
    {
        int Left = 0;
        int Right = 0;
        int Ground = 0;
    };
    Footprint FootprintOf(CreatureId id, bool mirrored);

    int FacingOf(CreatureId id);
    Habit HabitOf(CreatureId id);

    inline constexpr double kPetSeconds = 0.9;
    void DrawHeart(ImDrawList* drawList, ImVec2 at, float pixel, float alpha);
    void DrawAngerMark(ImDrawList* drawList, ImVec2 center, float pixel, bool beat);

    enum class Mark
    {
        Dots,
        Note,
        Question,
        Alarm
    };
    void DrawMark(ImDrawList* drawList, ImVec2 center, float pixel, Mark mark, float alpha = 1.0f);
    void DrawDizzy(ImDrawList* drawList, ImVec2 center, float pixel, float age);

    double Moment(std::uint32_t seed);

    void SetEnabled(bool enabled);

    void BeginFrame();
    bool NeedsFrames();
    void RequestFrames();
    void KeepFrames();
    bool NeedsFullRate();
    bool Enabled();
}
