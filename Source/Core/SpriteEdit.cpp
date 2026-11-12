#include "Core/SpriteEdit.h"

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

namespace Idler::SpriteEdit
{
    namespace
    {
        bool Inside(const SpriteFile::Frame& frame, int row, int column)
        {
            return row >= 0 && column >= 0 && static_cast<std::size_t>(row) < frame.size() && static_cast<std::size_t>(column) < frame[static_cast<std::size_t>(row)].size();
        }

        void FillFrom(SpriteFile::Frame& frame, int row, int column, char cell)
        {
            if (!Inside(frame, row, column))
                return;
            const char replaced = CellAt(frame, row, column);
            if (replaced == cell)
                return;
            std::vector<std::pair<int, int>> open{ { row, column } };
            while (!open.empty())
            {
                const auto [y, x] = open.back();
                open.pop_back();
                if (!Inside(frame, y, x) || CellAt(frame, y, x) != replaced)
                    continue;
                SetCell(frame, y, x, cell);
                open.emplace_back(y - 1, x);
                open.emplace_back(y + 1, x);
                open.emplace_back(y, x - 1);
                open.emplace_back(y, x + 1);
            }
        }
    }

    std::string_view CellName(char cell)
    {
        for (const CellKind& kind : kCellKinds)
        {
            if (kind.Cell == cell)
                return kind.Name;
        }
        return "Unknown";
    }

    char CellAt(const SpriteFile::Frame& frame, int row, int column)
    {
        return Inside(frame, row, column) ? frame[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] : '\0';
    }

    bool SetCell(SpriteFile::Frame& frame, int row, int column, char cell)
    {
        if (!Inside(frame, row, column))
            return false;
        char& target = frame[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
        if (target == cell)
            return false;
        target = cell;
        return true;
    }

    int MirroredColumn(int column)
    {
        return SpriteFile::kColumns - 1 - column;
    }

    void Paint(SpriteFile::Frame& frame, int row, int column, char cell, bool mirror)
    {
        SetCell(frame, row, column, cell);
        if (mirror)
            SetCell(frame, row, MirroredColumn(column), cell);
    }

    void PaintLine(SpriteFile::Frame& frame, int fromRow, int fromColumn, int toRow, int toColumn, char cell, bool mirror)
    {
        const int dx = std::abs(toColumn - fromColumn);
        const int dy = -std::abs(toRow - fromRow);
        const int stepX = fromColumn < toColumn ? 1 : -1;
        const int stepY = fromRow < toRow ? 1 : -1;
        int error = dx + dy;
        int x = fromColumn;
        int y = fromRow;
        while (true)
        {
            Paint(frame, y, x, cell, mirror);
            if (x == toColumn && y == toRow)
                break;
            const int twice = error * 2;
            if (twice >= dy)
            {
                error += dy;
                x += stepX;
            }
            if (twice <= dx)
            {
                error += dx;
                y += stepY;
            }
        }
    }

    void PaintRectangle(SpriteFile::Frame& frame, int fromRow, int fromColumn, int toRow, int toColumn, char cell, bool filled, bool mirror)
    {
        const int top = std::min(fromRow, toRow);
        const int bottom = std::max(fromRow, toRow);
        const int left = std::min(fromColumn, toColumn);
        const int right = std::max(fromColumn, toColumn);
        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                if (filled || y == top || y == bottom || x == left || x == right)
                    Paint(frame, y, x, cell, mirror);
            }
        }
    }

    void Fill(SpriteFile::Frame& frame, int row, int column, char cell, bool mirror)
    {
        FillFrom(frame, row, column, cell);
        if (mirror)
            FillFrom(frame, row, MirroredColumn(column), cell);
    }

    SpriteFile::Frame Shifted(const SpriteFile::Frame& frame, int rows, int columns)
    {
        SpriteFile::Frame moved = frame;
        for (std::size_t y = 0; y < moved.size(); ++y)
        {
            for (std::size_t x = 0; x < moved[y].size(); ++x)
            {
                const int fromRow = static_cast<int>(y) - rows;
                const int fromColumn = static_cast<int>(x) - columns;
                const char source = CellAt(frame, fromRow, fromColumn);
                moved[y][x] = source == '\0' ? '.' : source;
            }
        }
        return moved;
    }

    void FlipHorizontal(SpriteFile::Frame& frame)
    {
        for (std::string& line : frame)
            std::reverse(line.begin(), line.end());
    }

    SpriteFile::Frame EmptyFrame()
    {
        return SpriteFile::Frame(static_cast<std::size_t>(SpriteFile::kRows), std::string(static_cast<std::size_t>(SpriteFile::kColumns), '.'));
    }
}
