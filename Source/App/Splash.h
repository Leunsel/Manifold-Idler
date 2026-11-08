#pragma once

#include "Core/Creatures.h"

#include <windows.h>

namespace Idler::Splash
{
    enum class Stage
    {
        Starting,
        ReadingLibrary
    };

    void Show(HINSTANCE instance, RECT around, CreatureId creature, bool evolved, bool animate, bool activate);
    void SetStage(Stage stage);
    void Close();
}
