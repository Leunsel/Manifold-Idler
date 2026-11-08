#pragma once

#include "Core/Creatures.h"
#include "Core/MiniGames.h"

namespace Idler
{
    class IdlerModel;
}

namespace Idler::UI
{
    void OpenMiniGame(CreatureId id, MiniGame game);
    void PlayMiniGameAhead(float seconds);
    void DrawMiniGame(IdlerModel& model);
    const char* MiniGameIcon(MiniGame game);
}
