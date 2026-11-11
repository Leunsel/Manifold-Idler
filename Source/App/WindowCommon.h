#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>

namespace Idler::WindowCommon
{
    inline constexpr int kClientWidth = 980;
    inline constexpr int kClientHeight = 600;
    inline constexpr float kResizeEdge = 5.0f;
    inline constexpr float kResizeCorner = 14.0f;
    inline constexpr float kZoomShare = 0.4f;
    inline constexpr float kZoomStep = 0.125f;
    inline constexpr float kMaxZoom = 2.0f;

    inline constexpr unsigned long long kBusyMilliseconds = 1500;
    inline constexpr unsigned kIdleFrameMilliseconds = 250;
    inline constexpr unsigned kAnimationFrameMilliseconds = 33;
    inline constexpr unsigned kHiddenUpdateMilliseconds = 250;

    inline constexpr double kCaptureTimeoutSeconds = 20.0;
    inline constexpr int kCaptureSettleFrames = 12;

    inline bool ClicksOrKeysQueued()
    {
        for (const ImGuiInputEvent& event : ImGui::GetCurrentContext()->InputEventsQueue)
        {
            if (event.Type != ImGuiInputEventType_MousePos && event.Type != ImGuiInputEventType_MouseWheel && event.Type != ImGuiInputEventType_MouseViewport)
                return true;
        }
        return false;
    }

    inline float ZoomFor(float width, float height, float scale)
    {
        const float growth = std::min(width / (kClientWidth * scale), height / (kClientHeight * scale));
        const float wanted = 1.0f + std::max(0.0f, growth - 1.0f) * kZoomShare;
        return std::min(kMaxZoom, std::floor(wanted / kZoomStep) * kZoomStep);
    }

    inline void ScaleStyle(const ImGuiStyle& base, float scale)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style = base;
        style.ScaleAllSizes(scale);

        if (scale >= 1.75f)
        {
            style.FrameBorderSize = 2.0f;
            style.ChildBorderSize = 2.0f;
            style.PopupBorderSize = 2.0f;
        }
        else if (scale < 1.0f)
        {
            style.WindowBorderSize = base.WindowBorderSize;
            style.ChildBorderSize = base.ChildBorderSize;
            style.PopupBorderSize = base.PopupBorderSize;
            style.FrameBorderSize = base.FrameBorderSize;
            style.ImageBorderSize = base.ImageBorderSize;
            style.TabBorderSize = base.TabBorderSize;
            style.TabBarBorderSize = base.TabBarBorderSize;
            style.TabBarOverlineSize = base.TabBarOverlineSize;
            style.TreeLinesSize = base.TreeLinesSize;
            style.DragDropTargetBorderSize = base.DragDropTargetBorderSize;
            style.InputTextCursorSize = base.InputTextCursorSize;
            style.SeparatorSize = base.SeparatorSize;
            style.SeparatorTextBorderSize = base.SeparatorTextBorderSize;
        }

        style.FontScaleDpi = scale;
    }
}
