#pragma once

#include "Core/Terrarium.h"

#include "imgui.h"

namespace Idler
{
    class IdlerModel;
}

namespace Idler::UI
{
    void DrawTerrarium(IdlerModel& model, ImVec2 size);
    void DrawCreatureHome(IdlerModel& model, ImVec2 size);
    void SetHomeLightPreview(float daylight);
    float HomeDaylight(const IdlerModel& model);

    struct CreatureStage
    {
        float Ground = 0.0f;
    };
    CreatureStage DrawCreatureStage(IdlerModel& model, ImVec2 min, ImVec2 max, Habit habit, bool game = false);
    void DrawCreatureStageFront(ImVec2 min, ImVec2 max, Habit habit, ImU32 corners = 0);
}
