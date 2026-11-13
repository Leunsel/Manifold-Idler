#pragma once

#include "Core/Settings.h"

#include "imgui.h"

namespace Idler::UI
{
    void DrawSeasonFrame(ImDrawList* drawList, ImVec2 min, ImVec2 max, float textEnd, SceneSeason season, float daylight, bool hovered);
}
