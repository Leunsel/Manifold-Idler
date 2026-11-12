#include "SpriteCreator/PaintCard.h"

#include "Core/Errors.h"
#include "Core/SpriteEdit.h"
#include "Generated/IconsFontAwesome5.h"
#include "SpriteCreator/Widgets.h"
#include "UI/IdleAnimation.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Idler::SpriteCreator
{
    namespace
    {
        using UI::Px;
        namespace Animation = UI::IdleAnimation;

        constexpr ImVec4 kCheckerLight = ImVec4(0.125f, 0.131f, 0.150f, 1.0f);
        constexpr ImVec4 kCheckerDark = ImVec4(0.100f, 0.105f, 0.121f, 1.0f);
        constexpr ImVec4 kCheckerMiddle = ImVec4(0.112f, 0.118f, 0.135f, 1.0f);

        struct ToolInfo
        {
            Tool Kind;
            const char* Id;
            const char* Icon;
            const char* Tip;
        };

        constexpr ToolInfo kTools[] = {
            { Tool::Pencil, "##pencil", ICON_FA_PENCIL_ALT, "Pencil (B): the left button paints the first colour, the right button the second" },
            { Tool::Eraser, "##eraser", ICON_FA_ERASER, "Eraser (E)" },
            { Tool::Fill, "##fill", ICON_FA_FILL_DRIP, "Fill an area (G)" },
            { Tool::Line, "##line", ICON_FA_SLASH, "Line (L)" },
            { Tool::Rectangle, "##rectangle", ICON_FA_VECTOR_SQUARE, "Rectangle (R), hold Shift for a filled one" },
            { Tool::Picker, "##picker", ICON_FA_EYE_DROPPER, "Pick a colour from the frame (I), or Alt+click with any tool" },
            { Tool::Move, "##move", ICON_FA_ARROWS_ALT, "Move the whole frame (M)" },
        };

        SpriteFile::Frame* CurrentFrame(CreatureFile& file, const PaintState& state)
        {
            if (!file.Sprite || state.Frame < 0 || state.Frame >= static_cast<int>(file.Sprite->Frames.size()))
                return nullptr;
            return &file.Sprite->Frames[static_cast<std::size_t>(state.Frame)];
        }

        char PaintCell(const PaintState& state)
        {
            if (state.Current == Tool::Eraser)
                return '.';
            return state.StrokeSecondary ? state.Secondary : state.Primary;
        }

        void Apply(CreatureFile& file, PaintState& state, int row, int column, bool first)
        {
            SpriteFile::Frame* frame = CurrentFrame(file, state);
            if (!frame)
                return;
            const SpriteFile::Frame before = *frame;
            const char cell = PaintCell(state);
            const int clampedRow = std::clamp(row, 0, SpriteFile::kRows - 1);
            const int clampedColumn = std::clamp(column, 0, SpriteFile::kColumns - 1);
            switch (state.Current)
            {
            case Tool::Pencil:
            case Tool::Eraser:
                if (first)
                    SpriteEdit::Paint(*frame, row, column, cell, state.Mirror);
                else
                    SpriteEdit::PaintLine(*frame, state.LastRow, state.LastColumn, row, column, cell, state.Mirror);
                break;
            case Tool::Fill:
                if (first)
                    SpriteEdit::Fill(*frame, row, column, cell, state.Mirror);
                break;
            case Tool::Line:
                *frame = state.Before;
                SpriteEdit::PaintLine(*frame, state.StartRow, state.StartColumn, clampedRow, clampedColumn, cell, state.Mirror);
                break;
            case Tool::Rectangle:
                *frame = state.Before;
                SpriteEdit::PaintRectangle(*frame, state.StartRow, state.StartColumn, clampedRow, clampedColumn, cell, ImGui::GetIO().KeyShift, state.Mirror);
                break;
            case Tool::Move:
                *frame = SpriteEdit::Shifted(state.Before, row - state.StartRow, column - state.StartColumn);
                break;
            case Tool::Picker:
                break;
            }
            state.LastRow = row;
            state.LastColumn = column;
            if (*frame != before)
                Refresh(file);
        }

        void DrawChecker(ImDrawList* drawList, ImVec2 min, float cell, int rows, int columns)
        {
            const ImU32 light = Color(kCheckerLight);
            const ImU32 dark = Color(kCheckerDark);
            for (int row = 0; row < rows; ++row)
            {
                for (int column = 0; column < columns; ++column)
                {
                    const ImVec2 cellMin(min.x + cell * static_cast<float>(column), min.y + cell * static_cast<float>(row));
                    drawList->AddRectFilled(cellMin, ImVec2(cellMin.x + cell, cellMin.y + cell), (row + column) % 2 == 0 ? light : dark);
                }
            }
        }

        void DrawSwatch(ImDrawList* drawList, ImVec2 min, float side, char cell, const UI::CreatureSprites::Sprite* sprite)
        {
            const ImVec2 max(min.x + side, min.y + side);
            if (cell == '.')
                DrawChecker(drawList, min, side * 0.5f, 2, 2);
            else if (sprite)
                drawList->AddRectFilled(min, max, Animation::CellColor(*sprite, cell));
            drawList->AddRect(min, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.16f)));
        }

        void FrameNavigation(const CreatureFile& file, PaintState& state)
        {
            const int count = static_cast<int>(file.Sprite->Frames.size());
            const std::string text = "Frame " + std::to_string(state.Frame + 1) + " of " + std::to_string(count);
            const float side = std::round(Px(22.0f));
            const float gap = Px(4.0f);
            const float textWidth = ImGui::CalcTextSize(text.c_str()).x;
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 resume = ImGui::GetCursorScreenPos();
            const float right = windowPos.x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
            const float top = windowPos.y + Px(8.0f);
            ImGui::SetCursorScreenPos(ImVec2(right - side * 2.0f - gap * 2.0f - textWidth, top));
            if (IconButton("##previous_frame", ICON_FA_CHEVRON_LEFT, side, ManifoldTheme::Text, false, count > 1))
                StepFrame(&file, state, -1);
            Tooltip("Previous frame (Page Up)");
            ImGui::SameLine(0.0f, gap);
            const ImVec2 textAt = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddText(ImVec2(textAt.x, std::round(textAt.y + (side - ImGui::GetTextLineHeight()) * 0.5f)), Color(ManifoldTheme::Muted), text.c_str());
            ImGui::Dummy(ImVec2(textWidth, side));
            ImGui::SameLine(0.0f, gap);
            if (IconButton("##next_frame", ICON_FA_CHEVRON_RIGHT, side, ManifoldTheme::Text, false, count > 1))
                StepFrame(&file, state, 1);
            Tooltip("Next frame (Page Down)");
            ImGui::SetCursorScreenPos(resume);
        }

        void DrawToolbar(CreatureFile& file, PaintState& state, PaintRequests& requests)
        {
            const float side = std::round(Px(26.0f));
            const float gap = Px(2.0f);
            const float group = Px(12.0f);
            bool firstButton = true;
            const auto place = [&](float spacing)
                {
                    if (!firstButton)
                        ImGui::SameLine(0.0f, spacing);
                    firstButton = false;
                };

            for (const ToolInfo& tool : kTools)
            {
                place(gap);
                if (IconButton(tool.Id, tool.Icon, side, ManifoldTheme::Text, state.Current == tool.Kind))
                    state.Current = tool.Kind;
                Tooltip(tool.Tip);
            }

            place(group);
            if (IconButton("##mirror", ICON_FA_COLUMNS, side, ManifoldTheme::Text, state.Mirror))
                state.Mirror = !state.Mirror;
            Tooltip("Paint mirrored around the middle (X)");
            place(gap);
            if (IconButton("##onion", ICON_FA_CLONE, side, ManifoldTheme::Text, state.Onion))
                state.Onion = !state.Onion;
            Tooltip("Show the previous frame faintly (O)");

            place(group);
            if (IconButton("##flip", ICON_FA_EXCHANGE_ALT, side, ManifoldTheme::Text))
                FlipFrame(file, state);
            Tooltip("Flip the frame left to right (H)");
            place(gap);
            if (IconButton("##clear", ICON_FA_TRASH_ALT, side, ManifoldTheme::Error))
                ClearFrame(file, state);
            Tooltip("Clear the frame");

            place(group);
            if (IconButton("##undo", ICON_FA_UNDO, side, ManifoldTheme::Text, false, !file.Undo.empty()))
                requests.Undo = true;
            Tooltip("Undo (Ctrl+Z)");
            place(gap);
            if (IconButton("##redo", ICON_FA_REDO, side, ManifoldTheme::Text, false, !file.Redo.empty()))
                requests.Redo = true;
            Tooltip("Redo (Ctrl+Y)");
            place(gap);
            if (IconButton("##save", ICON_FA_SAVE, side, ManifoldTheme::Accent, file.Dirty(), file.Dirty()))
                requests.Save = true;
            Tooltip("Save (Ctrl+S)");
        }

        void DrawPalette(CreatureFile& file, PaintState& state, ImVec2 size)
        {
            const UI::CreatureSprites::Sprite* sprite = file.View ? &file.View->Get() : nullptr;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##palette", size, ImGuiChildFlags_None, ImGuiWindowFlags_None);
            ImGui::PopStyleColor();
            const float rowHeight = std::round(Px(22.0f));
            const float side = std::round(Px(14.0f));
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (std::size_t index = 0; index < std::size(SpriteEdit::kCellKinds); ++index)
            {
                const SpriteEdit::CellKind& kind = SpriteEdit::kCellKinds[index];
                ImGui::PushID(static_cast<int>(index));
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const float width = ImGui::GetContentRegionAvail().x;
                if (ImGui::InvisibleButton("##swatch", ImVec2(width, rowHeight)))
                    state.Primary = kind.Cell;
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    state.Secondary = kind.Cell;
                const bool hovered = ImGui::IsItemHovered();
                Tooltip("Left click: first colour. Right click: second colour.");
                const ImVec2 max(pos.x + width, pos.y + rowHeight);
                const bool primary = state.Primary == kind.Cell;
                const bool secondary = state.Secondary == kind.Cell;
                if (primary || secondary)
                    drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, primary ? 0.16f : 0.08f)), Px(4.0f));
                else if (hovered)
                    drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.05f)), Px(4.0f));
                DrawSwatch(drawList, ImVec2(std::round(pos.x + Px(5.0f)), std::round(pos.y + (rowHeight - side) * 0.5f)), side, kind.Cell, sprite);

                std::string tags;
                if (primary)
                    tags += "L";
                if (secondary)
                    tags += tags.empty() ? "R" : " R";
                const float tagsWidth = tags.empty() ? 0.0f : ImGui::CalcTextSize(tags.c_str()).x;
                const float textX = pos.x + Px(5.0f) + side + Px(8.0f);
                const float textY = std::round(pos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f);
                drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(max.x - tagsWidth - Px(8.0f), max.y), true);
                drawList->AddText(ImVec2(textX, textY), Color(primary || secondary ? ManifoldTheme::Text : ManifoldTheme::Label), kind.Name.data(), kind.Name.data() + kind.Name.size());
                drawList->PopClipRect();
                if (!tags.empty())
                    drawList->AddText(ImVec2(max.x - tagsWidth - Px(5.0f), textY), Color(ManifoldTheme::Accent), tags.c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        void DrawCanvas(CreatureFile& file, PaintState& state, ImVec2 origin, ImVec2 room, std::string& hover)
        {
            const SpriteFile::Frame* frame = CurrentFrame(file, state);
            const float cell = std::floor(std::min(room.x / SpriteFile::kColumns, room.y / SpriteFile::kRows));
            if (!frame || cell < 2.0f)
                return;
            const ImVec2 size(cell * SpriteFile::kColumns, cell * SpriteFile::kRows);
            const ImVec2 min(std::round(origin.x + (room.x - size.x) * 0.5f), std::round(origin.y));
            const ImVec2 max(min.x + size.x, min.y + size.y);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            DrawChecker(drawList, min, cell, SpriteFile::kRows, SpriteFile::kColumns);
            if (file.View)
            {
                const UI::CreatureSprites::Sprite& sprite = file.View->Get();
                if (state.Onion && sprite.FrameCount > 1)
                {
                    Animation::SetHaze(0.72f, kCheckerMiddle);
                    Animation::DrawSpriteFrame(drawList, min, sprite, (state.Frame + sprite.FrameCount - 1) % sprite.FrameCount, cell, false, Animation::Tint::Colors);
                    Animation::SetHaze(0.0f, kCheckerMiddle);
                }
                Animation::DrawSpriteFrame(drawList, min, sprite, state.Frame, cell, false, Animation::Tint::Colors);
            }
            for (int column = 0; column <= SpriteFile::kColumns; ++column)
            {
                const float x = min.x + cell * static_cast<float>(column);
                drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, column % 4 == 0 ? 0.09f : 0.04f)));
            }
            for (int row = 0; row <= SpriteFile::kRows; ++row)
            {
                const float y = min.y + cell * static_cast<float>(row);
                drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, row == SpriteFile::kRows - 1 ? 0.09f : 0.04f)));
            }
            if (state.Mirror)
            {
                const float middle = min.x + cell * static_cast<float>(SpriteFile::kColumns) * 0.5f;
                drawList->AddLine(ImVec2(middle, min.y - Px(4.0f)), ImVec2(middle, max.y + Px(4.0f)), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.7f)), Px(1.5f));
            }

            ImGui::SetCursorScreenPos(min);
            ImGui::InvisibleButton("##canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const int column = static_cast<int>(std::floor((mouse.x - min.x) / cell));
            const int row = static_cast<int>(std::floor((mouse.y - min.y) / cell));
            const bool inside = row >= 0 && row < SpriteFile::kRows && column >= 0 && column < SpriteFile::kColumns;
            if (hovered && inside)
            {
                const auto outline = [&](int atColumn)
                    {
                        const ImVec2 cellMin(min.x + cell * static_cast<float>(atColumn), min.y + cell * static_cast<float>(row));
                        drawList->AddRect(cellMin, ImVec2(cellMin.x + cell, cellMin.y + cell), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.75f)), 0.0f, 0, Px(1.5f));
                    };
                outline(column);
                if (state.Mirror && SpriteEdit::MirroredColumn(column) != column)
                    outline(SpriteEdit::MirroredColumn(column));
                const char current = SpriteEdit::CellAt(*frame, row, column);
                hover = "Row " + std::to_string(row + 1) + ", column " + std::to_string(column + 1) + ": " + std::string(SpriteEdit::CellName(current));
            }

            if (ImGui::IsItemActivated())
            {
                const bool secondary = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
                if (state.Current == Tool::Picker || ImGui::GetIO().KeyAlt)
                {
                    if (inside)
                        (secondary ? state.Secondary : state.Primary) = SpriteEdit::CellAt(*frame, row, column);
                }
                else
                {
                    state.Stroking = true;
                    state.StrokeSecondary = secondary;
                    state.StartRow = state.LastRow = std::clamp(row, 0, SpriteFile::kRows - 1);
                    state.StartColumn = state.LastColumn = std::clamp(column, 0, SpriteFile::kColumns - 1);
                    state.Before = *frame;
                    Remember(file);
                    Apply(file, state, row, column, true);
                }
            }
            else if (state.Stroking && ImGui::IsItemActive())
            {
                const bool reshape = state.Current == Tool::Line || state.Current == Tool::Rectangle || state.Current == Tool::Move;
                if (reshape || row != state.LastRow || column != state.LastColumn)
                    Apply(file, state, row, column, false);
            }
            if (state.Stroking && !ImGui::IsItemActive())
            {
                state.Stroking = false;
                Forget(file);
            }
        }

        void DrawStatus(const CreatureFile& file, const PaintState& state, const std::string& hover, ImVec2 at, float width)
        {
            const UI::CreatureSprites::Sprite* sprite = file.View ? &file.View->Get() : nullptr;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float line = ImGui::GetTextLineHeight();
            const float side = std::round(line * 0.85f);
            std::string left = hover;
            ImVec4 leftColor = ManifoldTheme::Muted;
            if (left.empty() && file.Dirty())
            {
                left = "Not saved yet";
                leftColor = ManifoldTheme::Warning;
            }
            drawList->AddText(at, Color(leftColor), left.c_str());

            const std::string first(SpriteEdit::CellName(state.Primary));
            const std::string second(SpriteEdit::CellName(state.Secondary));
            const float firstWidth = ImGui::CalcTextSize(first.c_str()).x;
            const float secondWidth = ImGui::CalcTextSize(second.c_str()).x;
            float x = at.x + width - (side + Px(6.0f) + firstWidth + Px(16.0f) + side + Px(6.0f) + secondWidth);
            const float swatchY = std::round(at.y + (line - side) * 0.5f);
            DrawSwatch(drawList, ImVec2(x, swatchY), side, state.Primary, sprite);
            x += side + Px(6.0f);
            drawList->AddText(ImVec2(x, at.y), Color(ManifoldTheme::Text), first.c_str());
            x += firstWidth + Px(16.0f);
            DrawSwatch(drawList, ImVec2(x, swatchY), side, state.Secondary, sprite);
            x += side + Px(6.0f);
            drawList->AddText(ImVec2(x, at.y), Color(ManifoldTheme::Label), second.c_str());
        }
    }

    void ClampFrame(const CreatureFile* file, PaintState& state)
    {
        const int count = file && file->Sprite ? static_cast<int>(file->Sprite->Frames.size()) : 0;
        state.Frame = count == 0 ? 0 : std::clamp(state.Frame, 0, count - 1);
    }

    void StepFrame(const CreatureFile* file, PaintState& state, int step)
    {
        const int count = file && file->Sprite ? static_cast<int>(file->Sprite->Frames.size()) : 0;
        if (count == 0 || state.Stroking)
            return;
        state.Frame = ((state.Frame + step) % count + count) % count;
    }

    void FlipFrame(CreatureFile& file, const PaintState& state)
    {
        SpriteFile::Frame* frame = CurrentFrame(file, state);
        if (!frame || state.Stroking)
            return;
        Remember(file);
        SpriteEdit::FlipHorizontal(*frame);
        Forget(file);
        Refresh(file);
    }

    void ClearFrame(CreatureFile& file, const PaintState& state)
    {
        SpriteFile::Frame* frame = CurrentFrame(file, state);
        if (!frame || state.Stroking)
            return;
        Remember(file);
        *frame = SpriteEdit::EmptyFrame();
        Forget(file);
        Refresh(file);
    }

    PaintRequests DrawPaintCard(CreatureFile* file, PaintState& state, ImVec2 size)
    {
        PaintRequests requests;
        BeginCard("##paint", ICON_FA_PAINT_BRUSH, "Paint", size);
        if (!file)
        {
            MutedText("Choose a creature in the Explorer.");
            EndCard();
            return requests;
        }
        if (!file->Sprite)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Warning);
            ImGui::TextWrapped("%s", ErrorMessage(ErrorCode::SpriteNotRead, file->Problem).c_str());
            ImGui::PopStyleColor();
            EndCard();
            return requests;
        }
        ClampFrame(file, state);
        if (file->Sprite->Frames.empty())
        {
            MutedText("This creature has no frames.");
            EndCard();
            return requests;
        }

        FrameNavigation(*file, state);
        DrawToolbar(*file, state, requests);
        ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));

        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 room = ImGui::GetContentRegionAvail();
        const float statusHeight = std::round(ImGui::GetTextLineHeight() + Px(10.0f));
        const float paletteWidth = std::round(Px(128.0f));
        const float bodyHeight = std::max(Px(60.0f), room.y - statusHeight);

        DrawPalette(*file, state, ImVec2(paletteWidth, bodyHeight));
        std::string hover;
        const float canvasX = start.x + paletteWidth + Px(12.0f);
        DrawCanvas(*file, state, ImVec2(canvasX, start.y), ImVec2(room.x - paletteWidth - Px(12.0f), bodyHeight), hover);

        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + bodyHeight));
        ImGui::Dummy(ImVec2(room.x, statusHeight));
        DrawStatus(*file, state, hover, ImVec2(start.x, std::round(start.y + bodyHeight + Px(6.0f))), room.x);
        EndCard();
        return requests;
    }
}
