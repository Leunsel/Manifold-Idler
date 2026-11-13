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

    bool InsertFrame(SpriteFile::Sprite& sprite, int at, SpriteFile::Frame frame)
    {
        const int count = static_cast<int>(sprite.Frames.size());
        if (count >= SpriteFile::kMaxFrames)
            return false;
        at = std::clamp(at, 0, count);
        sprite.Frames.insert(sprite.Frames.begin() + at, std::move(frame));
        for (SpriteFile::Step& step : sprite.Steps)
        {
            if (step.Frame >= at)
                ++step.Frame;
        }
        if (count > 0 && sprite.Sleep >= at)
            ++sprite.Sleep;
        if (sprite.Steps.empty())
            sprite.Steps.push_back({ 0, 0, 0, false, kStartTicks });
        return true;
    }

    bool RemoveFrame(SpriteFile::Sprite& sprite, int at)
    {
        const int count = static_cast<int>(sprite.Frames.size());
        if (count <= 1 || at < 0 || at >= count)
            return false;
        sprite.Frames.erase(sprite.Frames.begin() + at);
        std::erase_if(sprite.Steps, [&](const SpriteFile::Step& step) { return step.Frame == at; });
        for (SpriteFile::Step& step : sprite.Steps)
        {
            if (step.Frame > at)
                --step.Frame;
        }
        if (sprite.Steps.empty())
            sprite.Steps.push_back({ 0, 0, 0, false, kStartTicks });
        if (sprite.Sleep == at)
            sprite.Sleep = std::max(0, at - 1);
        else if (sprite.Sleep > at)
            --sprite.Sleep;
        return true;
    }

    bool MoveFrame(SpriteFile::Sprite& sprite, int from, int to)
    {
        const int count = static_cast<int>(sprite.Frames.size());
        if (from < 0 || from >= count || to < 0 || to >= count || from == to)
            return false;
        std::vector<int> order(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index)
            order[static_cast<std::size_t>(index)] = index;
        const int moved = order[static_cast<std::size_t>(from)];
        order.erase(order.begin() + from);
        order.insert(order.begin() + to, moved);

        std::vector<int> place(static_cast<std::size_t>(count));
        std::vector<SpriteFile::Frame> frames;
        frames.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index)
        {
            const int old = order[static_cast<std::size_t>(index)];
            place[static_cast<std::size_t>(old)] = index;
            frames.push_back(std::move(sprite.Frames[static_cast<std::size_t>(old)]));
        }
        sprite.Frames = std::move(frames);
        for (SpriteFile::Step& step : sprite.Steps)
        {
            if (step.Frame >= 0 && step.Frame < count)
                step.Frame = place[static_cast<std::size_t>(step.Frame)];
        }
        if (sprite.Sleep >= 0 && sprite.Sleep < count)
            sprite.Sleep = place[static_cast<std::size_t>(sprite.Sleep)];
        return true;
    }

    bool InsertStep(SpriteFile::Sprite& sprite, int at, SpriteFile::Step step)
    {
        at = std::clamp(at, 0, static_cast<int>(sprite.Steps.size()));
        sprite.Steps.insert(sprite.Steps.begin() + at, step);
        return true;
    }

    bool RemoveStep(SpriteFile::Sprite& sprite, int at)
    {
        const int count = static_cast<int>(sprite.Steps.size());
        if (count <= 1 || at < 0 || at >= count)
            return false;
        sprite.Steps.erase(sprite.Steps.begin() + at);
        return true;
    }

    bool MoveStep(SpriteFile::Sprite& sprite, int from, int to)
    {
        const int count = static_cast<int>(sprite.Steps.size());
        if (from < 0 || from >= count || to < 0 || to >= count || from == to)
            return false;
        const SpriteFile::Step step = sprite.Steps[static_cast<std::size_t>(from)];
        sprite.Steps.erase(sprite.Steps.begin() + from);
        sprite.Steps.insert(sprite.Steps.begin() + to, step);
        return true;
    }

    int TotalTicks(const SpriteFile::Sprite& sprite)
    {
        int total = 0;
        for (const SpriteFile::Step& step : sprite.Steps)
            total += std::max(0, step.Ticks);
        return total;
    }

    int TicksBefore(const SpriteFile::Sprite& sprite, int step)
    {
        int total = 0;
        const std::size_t end = static_cast<std::size_t>(std::clamp(step, 0, static_cast<int>(sprite.Steps.size())));
        for (std::size_t index = 0; index < end; ++index)
            total += std::max(0, sprite.Steps[index].Ticks);
        return total;
    }

    int StepAtTick(const SpriteFile::Sprite& sprite, long long tick)
    {
        const int total = TotalTicks(sprite);
        if (total <= 0)
            return sprite.Steps.empty() ? -1 : 0;
        tick %= total;
        if (tick < 0)
            tick += total;
        for (std::size_t index = 0; index < sprite.Steps.size(); ++index)
        {
            const int ticks = std::max(0, sprite.Steps[index].Ticks);
            if (tick < ticks)
                return static_cast<int>(index);
            tick -= ticks;
        }
        return static_cast<int>(sprite.Steps.size()) - 1;
    }
}
