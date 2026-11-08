#pragma once

#include "imgui.h"

namespace ManifoldTheme
{
    inline constexpr ImVec4 Accent = ImVec4(0.18f, 0.86f, 0.62f, 1.00f);
    inline constexpr ImVec4 AccentHovered = ImVec4(0.26f, 0.94f, 0.70f, 1.00f);

    inline constexpr ImVec4 Sidebar = ImVec4(0.043f, 0.046f, 0.054f, 1.00f);
    inline constexpr ImVec4 Page = ImVec4(0.058f, 0.061f, 0.071f, 1.00f);
    inline constexpr ImVec4 Card = ImVec4(0.082f, 0.086f, 0.100f, 1.00f);
    inline constexpr ImVec4 CardBorder = ImVec4(1.00f, 1.00f, 1.00f, 0.055f);
    inline constexpr ImVec4 RowSeparator = ImVec4(1.00f, 1.00f, 1.00f, 0.045f);
    inline constexpr ImVec4 Border = ImVec4(0.20f, 0.21f, 0.24f, 0.80f);
    inline constexpr ImVec4 Text = ImVec4(0.90f, 0.91f, 0.93f, 1.00f);
    inline constexpr ImVec4 Label = ImVec4(0.78f, 0.79f, 0.82f, 1.00f);
    inline constexpr ImVec4 Muted = ImVec4(0.50f, 0.52f, 0.56f, 1.00f);
    inline constexpr ImVec4 Warning = ImVec4(0.95f, 0.70f, 0.32f, 1.00f);
    inline constexpr ImVec4 Error = ImVec4(0.95f, 0.42f, 0.36f, 1.00f);
    inline constexpr ImVec4 Bond = ImVec4(0.95f, 0.36f, 0.50f, 1.00f);
    inline ImVec4 WithAlpha(ImVec4 color, float alpha)
    {
        color.w = alpha;
        return color;
    }

    inline void Apply(ImGuiStyle& style)
    {
        ImVec4* colors = style.Colors;

        const ImVec4 surface(0.075f, 0.080f, 0.092f, 1.00f);
        const ImVec4 frame(0.13f, 0.14f, 0.16f, 1.00f);

        colors[ImGuiCol_Text]                   = Text;
        colors[ImGuiCol_TextDisabled]           = Muted;
        colors[ImGuiCol_WindowBg]               = ImVec4(0.055f, 0.058f, 0.068f, 1.00f);
        colors[ImGuiCol_ChildBg]                = surface;
        colors[ImGuiCol_PopupBg]                = ImVec4(0.09f, 0.095f, 0.11f, 0.98f);
        colors[ImGuiCol_Border]                 = Border;
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = frame;
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.17f, 0.18f, 0.21f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.06f, 0.065f, 0.075f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.085f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.05f, 0.05f, 0.06f, 0.60f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.08f, 0.085f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.24f, 0.25f, 0.28f, 0.80f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.32f, 0.33f, 0.37f, 0.90f);
        colors[ImGuiCol_ScrollbarGrabActive]    = WithAlpha(Accent, 0.70f);
        colors[ImGuiCol_CheckMark]              = Accent;
        colors[ImGuiCol_SliderGrab]             = WithAlpha(Accent, 0.85f);
        colors[ImGuiCol_SliderGrabActive]       = Accent;
        colors[ImGuiCol_Button]                 = ImVec4(0.15f, 0.16f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = WithAlpha(Accent, 0.35f);
        colors[ImGuiCol_Header]                 = WithAlpha(Accent, 0.16f);
        colors[ImGuiCol_HeaderHovered]          = WithAlpha(Accent, 0.24f);
        colors[ImGuiCol_HeaderActive]           = WithAlpha(Accent, 0.32f);
        colors[ImGuiCol_Separator]              = Border;
        colors[ImGuiCol_SeparatorHovered]       = WithAlpha(Accent, 0.55f);
        colors[ImGuiCol_SeparatorActive]        = Accent;
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered]      = WithAlpha(Accent, 0.45f);
        colors[ImGuiCol_ResizeGripActive]       = WithAlpha(Accent, 0.80f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.10f, 0.105f, 0.12f, 1.00f);
        colors[ImGuiCol_TabHovered]             = WithAlpha(Accent, 0.24f);
        colors[ImGuiCol_TabSelected]            = WithAlpha(Accent, 0.18f);
        colors[ImGuiCol_TabDimmed]              = ImVec4(0.09f, 0.095f, 0.11f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.65f, 0.67f, 0.70f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = Accent;
        colors[ImGuiCol_PlotHistogram]          = WithAlpha(Accent, 0.80f);
        colors[ImGuiCol_PlotHistogramHovered]   = Accent;
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.10f, 0.105f, 0.12f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.18f, 0.19f, 0.22f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.14f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.025f);
        colors[ImGuiCol_TextSelectedBg]         = WithAlpha(Accent, 0.30f);
        colors[ImGuiCol_DragDropTarget]         = Accent;
        colors[ImGuiCol_NavCursor]              = WithAlpha(Accent, 0.80f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);

        style.WindowRounding    = 6.0f;
        style.ChildRounding     = 4.0f;
        style.FrameRounding     = 3.0f;
        style.GrabRounding      = 3.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.TabRounding       = 3.0f;
        style.ScrollbarSize     = 11.0f;
        style.GrabMinSize       = 9.0f;
        style.FramePadding      = ImVec2(8.0f, 5.0f);
        style.ItemSpacing       = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
        style.CellPadding       = ImVec2(6.0f, 4.0f);
        style.WindowBorderSize  = 1.0f;
        style.ChildBorderSize   = 1.0f;
        style.FrameBorderSize   = 0.0f;
        style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    }
}
