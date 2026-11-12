#pragma once

#include "Core/SpriteFile.h"

#include <string_view>

namespace Idler::SpriteEdit
{
    struct CellKind
    {
        char Cell;
        std::string_view Name;
    };

    inline constexpr CellKind kCellKinds[] = {
        { '.', "Empty" },
        { '#', "Body" },
        { 'o', "Body shade" },
        { '+', "Body light" },
        { 'a', "Second" },
        { 'd', "Second shade" },
        { 'l', "Second light" },
        { 'c', "Third" },
        { 'C', "Third shade" },
        { '*', "Eye on the body" },
        { 'e', "Eye on the second" },
        { 'r', "Red" },
        { 'y', "Yellow" },
        { 'w', "White" },
        { 'k', "Dark grey" },
    };

    std::string_view CellName(char cell);

    char CellAt(const SpriteFile::Frame& frame, int row, int column);
    bool SetCell(SpriteFile::Frame& frame, int row, int column, char cell);
    int MirroredColumn(int column);

    void Paint(SpriteFile::Frame& frame, int row, int column, char cell, bool mirror);
    void PaintLine(SpriteFile::Frame& frame, int fromRow, int fromColumn, int toRow, int toColumn, char cell, bool mirror);
    void PaintRectangle(SpriteFile::Frame& frame, int fromRow, int fromColumn, int toRow, int toColumn, char cell, bool filled, bool mirror);
    void Fill(SpriteFile::Frame& frame, int row, int column, char cell, bool mirror);
    SpriteFile::Frame Shifted(const SpriteFile::Frame& frame, int rows, int columns);
    void FlipHorizontal(SpriteFile::Frame& frame);
    SpriteFile::Frame EmptyFrame();
}
