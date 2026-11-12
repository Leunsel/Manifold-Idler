#include "SpriteCreator/CreatorScreen.h"

#include "Core/SpriteFile.h"
#include "Core/Text.h"
#include "Generated/IconsFontAwesome5.h"
#include "UI/Fonts.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <system_error>

namespace Idler::SpriteCreator
{
    namespace
    {
        using UI::Px;
        using UI::PxWhole;

        ImU32 Color(ImVec4 color)
        {
            return ImGui::GetColorU32(color);
        }

        bool WindowButton(const char* id, const char* icon, float side, ImVec4 hoverColor)
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(id, ImVec2(side, side));
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (hovered)
                drawList->AddRectFilled(pos, ImVec2(pos.x + side, pos.y + side), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.07f)), Px(4.0f));
            const ImVec2 size = ImGui::CalcTextSize(icon);
            drawList->AddText(ImVec2(std::round(pos.x + (side - size.x) * 0.5f), std::round(pos.y + (side - size.y) * 0.5f)),
                Color(hovered ? hoverColor : ManifoldTheme::Label), icon);
            return clicked;
        }

        void TitleBar(WindowControls& window, const std::filesystem::path& folder, bool found)
        {
            const ImVec2 origin = ImGui::GetWindowPos();
            const float width = ImGui::GetWindowWidth();
            const float height = std::round(Px(40.0f));
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Color(ManifoldTheme::Sidebar), window.CornerRadius(), ImDrawFlags_RoundCornersTop);
            drawList->AddLine(ImVec2(origin.x, origin.y + height - 1.0f), ImVec2(origin.x + width, origin.y + height - 1.0f), Color(ManifoldTheme::RowSeparator));

            const float side = std::round(Px(26.0f));
            const float gap = Px(2.0f);
            const float buttonsX = std::round(origin.x + width - Px(10.0f) - side * 3.0f - gap * 2.0f);
            const float textY = std::round(origin.y + (height - ImGui::GetTextLineHeight()) * 0.5f);

            float x = origin.x + Px(16.0f);
            drawList->AddText(ImVec2(x, textY), Color(ManifoldTheme::Accent), ICON_FA_CUBE);
            x += ImGui::CalcTextSize(ICON_FA_CUBE).x + Px(9.0f);
            constexpr const char* kTitle = "Sprite Creator";
            ImFont* bold = Fonts::Bold();
            if (bold)
            {
                drawList->AddText(bold, ImGui::GetFontSize(), ImVec2(x, textY), Color(ManifoldTheme::Text), kTitle);
                x += bold->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, kTitle).x;
            }
            else
            {
                drawList->AddText(ImVec2(x, textY), Color(ManifoldTheme::Text), kTitle);
                x += ImGui::CalcTextSize(kTitle).x;
            }

            const std::string status = found ? WideToUtf8(folder.wstring())
                : folder.empty() ? std::string("No creature folder found. Start it with --creatures=<folder>.")
                : "The creature folder " + WideToUtf8(folder.wstring()) + " does not exist.";
            const float statusLeft = x + Px(18.0f);
            const float statusRight = buttonsX - Px(16.0f);
            if (statusRight > statusLeft)
            {
                drawList->PushClipRect(ImVec2(statusLeft, origin.y), ImVec2(statusRight, origin.y + height), true);
                drawList->AddText(ImVec2(statusLeft, textY), Color(found ? ManifoldTheme::Muted : ManifoldTheme::Warning), status.c_str());
                drawList->PopClipRect();
            }

            const float buttonsY = std::round(origin.y + (height - side) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(buttonsX, buttonsY));
            if (WindowButton("##minimise", ICON_FA_MINUS, side, ManifoldTheme::Text))
                window.Minimise();
            ImGui::SameLine(0.0f, gap);
            if (WindowButton("##maximise", window.Maximised() ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE, side, ManifoldTheme::Text))
                window.ToggleMaximised();
            ImGui::SameLine(0.0f, gap);
            if (WindowButton("##close", ICON_FA_TIMES, side, ManifoldTheme::Error))
                window.Close();

            window.AddDragArea(origin.x, origin.y, buttonsX, origin.y + height);
            window.AddDragArea(buttonsX, origin.y, origin.x + width, buttonsY);
            ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
        }

        void BeginCard(const char* id, const char* icon, const char* title, ImVec2 size)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ManifoldTheme::Card);
            ImGui::PushStyleColor(ImGuiCol_Border, ManifoldTheme::CardBorder);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Px(6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(14.0f, 12.0f));
            ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Accent);
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, Px(8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Label);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
        }

        void EndCard()
        {
            ImGui::EndChild();
        }

        void DrawCanvas()
        {
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const float cell = std::floor(std::min(room.x / SpriteFile::kColumns, room.y / SpriteFile::kRows));
            if (cell < 2.0f)
                return;
            const ImVec2 size(cell * SpriteFile::kColumns, cell * SpriteFile::kRows);
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 min(std::round(cursor.x + (room.x - size.x) * 0.5f), std::round(cursor.y + (room.y - size.y) * 0.5f));
            const ImVec2 max(min.x + size.x, min.y + size.y);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, Color(ManifoldTheme::Sidebar));
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
            ImGui::Dummy(room);
        }
    }

    void Screen::SetCreatureFolder(std::filesystem::path folder)
    {
        std::error_code ignored;
        m_folderExists = !folder.empty() && std::filesystem::is_directory(folder, ignored);
        m_folder = std::move(folder);
    }

    std::filesystem::path FindCreatureFolder(const std::filesystem::path& start)
    {
        std::error_code ignored;
        if (start.empty())
            return {};
        if (std::filesystem::is_directory(start / "creatures", ignored))
            return start / "creatures";
        for (std::filesystem::path folder = start; !folder.empty(); folder = folder.parent_path())
        {
            const std::filesystem::path candidate = folder / "Tools" / "CreatureForge" / "creatures";
            if (std::filesystem::is_directory(candidate, ignored))
                return candidate;
            if (folder == folder.parent_path())
                break;
        }
        return {};
    }

    void Screen::Draw(WindowControls& window)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::Begin("##creator", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        TitleBar(window, m_folder, m_folderExists);

        const float pad = std::round(Px(14.0f));
        const float gap = std::round(Px(12.0f));
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 top = ImGui::GetCursorScreenPos();
        const ImVec2 start(windowPos.x + pad, top.y + pad);
        const float width = ImGui::GetWindowWidth() - pad * 2.0f;
        const float height = windowPos.y + ImGui::GetWindowHeight() - pad - start.y;

        const float explorer = std::round(Px(250.0f));
        const float properties = std::round(Px(280.0f));
        const float timeline = std::round(Px(150.0f));
        const float upper = std::max(Px(100.0f), height - timeline - gap);
        const float paint = std::max(Px(100.0f), width - explorer - properties - gap * 2.0f);

        ImGui::SetCursorScreenPos(start);
        BeginCard("##explorer", ICON_FA_FOLDER_OPEN, "Explorer", ImVec2(explorer, upper));
        EndCard();

        ImGui::SetCursorScreenPos(ImVec2(start.x + explorer + gap, start.y));
        BeginCard("##paint", ICON_FA_PAINT_BRUSH, "Paint", ImVec2(paint, upper));
        DrawCanvas();
        EndCard();

        ImGui::SetCursorScreenPos(ImVec2(start.x + explorer + gap + paint + gap, start.y));
        BeginCard("##properties", ICON_FA_SLIDERS_H, "Properties", ImVec2(properties, upper));
        EndCard();

        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + upper + gap));
        BeginCard("##timeline", ICON_FA_FILM, "Timeline", ImVec2(width, timeline));
        EndCard();

        ImGui::End();
    }
}
