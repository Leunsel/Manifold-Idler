#pragma once

#include "SpriteCreator/CreatureLibrary.h"

#include "imgui.h"

#include <string>
#include <string_view>

namespace Idler::SpriteCreator
{
    struct PropertiesState
    {
        bool ColorDrag = false;
    };

    struct PropertiesRequests
    {
        int JumpFrame = -1;
        int JumpStep = -1;
    };

    std::string ShownFinding(std::string_view text);
    PropertiesRequests DrawPropertiesCard(CreatureFile* file, CreatureFile* counterpart, PropertiesState& state, ImVec2 size);
}
