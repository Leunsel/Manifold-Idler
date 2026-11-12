#pragma once

#include "Core/SpriteFile.h"
#include "SpriteCreator/CreatureLibrary.h"

#include "imgui.h"

namespace Idler::SpriteCreator
{
    enum class Tool
    {
        Pencil,
        Eraser,
        Fill,
        Line,
        Rectangle,
        Picker,
        Move
    };

    struct PaintState
    {
        Tool Current = Tool::Pencil;
        char Primary = '#';
        char Secondary = '.';
        bool Mirror = false;
        bool Onion = false;
        int Frame = 0;
        bool Stroking = false;
        bool StrokeSecondary = false;
        int StartRow = 0;
        int StartColumn = 0;
        int LastRow = 0;
        int LastColumn = 0;
        SpriteFile::Frame Before;
    };

    struct PaintRequests
    {
        bool Undo = false;
        bool Redo = false;
        bool Save = false;
    };

    void ClampFrame(const CreatureFile* file, PaintState& state);
    void StepFrame(const CreatureFile* file, PaintState& state, int step);
    void FlipFrame(CreatureFile& file, const PaintState& state);
    void ClearFrame(CreatureFile& file, const PaintState& state);
    PaintRequests DrawPaintCard(CreatureFile* file, PaintState& state, ImVec2 size);
}
