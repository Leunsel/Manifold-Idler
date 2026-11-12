#pragma once

#include "Core/Terrarium.h"
#include "UI/CreatureSprites.h"

#include "imgui.h"

#include <cstdint>

namespace Idler::SpriteCreator
{
    enum class Backdrop
    {
        OwnStage,
        Meadow,
        Clouds,
        Underwater,
        Spring,
        Summer,
        Autumn,
        Winter,
        Plain
    };

    inline constexpr Backdrop kBackdrops[] = { Backdrop::OwnStage, Backdrop::Meadow, Backdrop::Clouds, Backdrop::Underwater, Backdrop::Spring, Backdrop::Summer,
        Backdrop::Autumn, Backdrop::Winter, Backdrop::Plain };

    const char* BackdropName(Backdrop backdrop);
    const char* BackdropKey(Backdrop backdrop);
    void DrawScenePreview(ImVec2 min, ImVec2 max, const UI::CreatureSprites::Sprite* sprite, Habit habit, Backdrop backdrop, float daylight, std::uint32_t seed);
}
