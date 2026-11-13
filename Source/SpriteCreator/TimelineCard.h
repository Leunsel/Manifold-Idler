#pragma once

#include "SpriteCreator/CreatureLibrary.h"
#include "SpriteCreator/PaintCard.h"

#include "imgui.h"

namespace Idler::SpriteCreator
{
    struct TimelineState
    {
        bool Playing = false;
        double Started = 0.0;
        int Step = -1;
        int SeenFrame = -1;
    };

    void TogglePlaying(TimelineState& timeline);
    float TimelineCardHeight();
    void DrawTimelineCard(CreatureFile* file, PaintState& paint, TimelineState& timeline, ImVec2 size);
}
