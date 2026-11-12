#include "SpriteCreator/Widgets.h"

#include "UI/Fonts.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <cfloat>
#include <cmath>

namespace Idler::SpriteCreator
{
    using UI::Px;
    using UI::PxWhole;

    ImU32 Color(ImVec4 color)
    {
        return ImGui::GetColorU32(color);
    }

    bool IconButton(const char* id, const char* icon, float side, ImVec4 hoverColor, bool active, bool enabled)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::BeginDisabled(!enabled);
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(side, side));
        ImGui::EndDisabled();
        const bool hovered = enabled && ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (active)
            drawList->AddRectFilled(pos, ImVec2(pos.x + side, pos.y + side), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.18f)), Px(4.0f));
        else if (hovered)
            drawList->AddRectFilled(pos, ImVec2(pos.x + side, pos.y + side), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.07f)), Px(4.0f));
        const ImVec2 size = ImGui::CalcTextSize(icon);
        const ImVec4 color = !enabled ? ManifoldTheme::WithAlpha(ManifoldTheme::Muted, 0.5f) : active ? ManifoldTheme::Accent : hovered ? hoverColor : ManifoldTheme::Label;
        drawList->AddText(ImVec2(std::round(pos.x + (side - size.x) * 0.5f), std::round(pos.y + (side - size.y) * 0.5f)), Color(color), icon);
        return clicked && enabled;
    }

    void Tooltip(const char* text)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", text);
    }

    void BoldText(ImDrawList* drawList, ImVec2 at, ImVec4 color, const char* text)
    {
        ImFont* bold = Fonts::Bold();
        if (bold)
            drawList->AddText(bold, ImGui::GetFontSize(), at, Color(color), text);
        else
            drawList->AddText(at, Color(color), text);
    }

    float BoldWidth(const char* text)
    {
        ImFont* bold = Fonts::Bold();
        return bold ? bold->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, text).x : ImGui::CalcTextSize(text).x;
    }

    void BeginCard(const char* id, const char* icon, const char* title, ImVec2 size, const char* note, ImVec4 noteColor)
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
        if (note && *note)
        {
            const float right = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
            ImGui::SameLine(right - ImGui::CalcTextSize(note).x);
            ImGui::PushStyleColor(ImGuiCol_Text, noteColor);
            ImGui::TextUnformatted(note);
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
    }

    void EndCard()
    {
        ImGui::EndChild();
    }

    void MutedText(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }
}
