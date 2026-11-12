#pragma once

#include "Core/Settings.h"

#include "imgui.h"

#include <cstdint>

namespace Idler::UI::SceneKit
{
    inline constexpr ImVec4 kMoon = ImVec4(0.560f, 0.590f, 0.560f, 1.0f);
    inline constexpr ImVec4 kCrater = ImVec4(0.460f, 0.490f, 0.470f, 1.0f);
    inline constexpr ImVec4 kSun = ImVec4(0.960f, 0.800f, 0.400f, 1.0f);
    inline constexpr ImVec4 kCloud = ImVec4(0.620f, 0.720f, 0.780f, 1.0f);
    inline constexpr float kShadowAlpha = 70.0f;
    inline constexpr float kBackScale = 0.72f;
    inline constexpr float kFrontScale = 1.08f;
    inline constexpr float kBackHaze = 0.38f;

    std::uint32_t Noise(std::uint32_t value);

    template <typename Draw>
    void Scatter(int columns, int rows, int cellWidth, int cellHeight, std::uint32_t salt, std::uint32_t share, Draw&& draw)
    {
        for (int row = 0; row * cellHeight < rows; ++row)
        {
            for (int column = 0; column * cellWidth < columns; ++column)
            {
                const std::uint32_t mixed = Noise((static_cast<std::uint32_t>(column) * 73856093u) ^ (static_cast<std::uint32_t>(row) * 19349663u) ^ (salt * 83492791u));
                if ((mixed >> 24) >= share)
                    continue;
                const int x = column * cellWidth + static_cast<int>(mixed % static_cast<std::uint32_t>(cellWidth));
                const int y = row * cellHeight + static_cast<int>((mixed >> 12) % static_cast<std::uint32_t>(cellHeight));
                if (x < columns && y < rows)
                    draw(static_cast<float>(x), static_cast<float>(y), mixed);
            }
        }
    }

    ImVec4 Mix(ImVec4 night, ImVec4 day, float share);
    ImVec4 Lit(ImVec4 color, float daylight);

    struct Palette
    {
        ImVec4 Sky, Star, Ground, GroundEdge, Grass, Water, WaterDeep, WaterEdge, Ripple, HillFar, HillNear, Tree;
    };

    constexpr ImVec4 Rgb(float red, float green, float blue)
    {
        return ImVec4(red, green, blue, 1.0f);
    }

    Palette PaletteAt(float daylight, SceneSeason season);

    struct Canvas
    {
        ImDrawList* List;
        ImVec2 Min;
        float Pixel;

        void Fill(float left, float top, float right, float bottom, ImVec4 color) const
        {
            List->AddRectFilled(ImVec2(Min.x + Pixel * left, Min.y + Pixel * top), ImVec2(Min.x + Pixel * right, Min.y + Pixel * bottom), ImGui::GetColorU32(color));
        }
    };

    void DrawDisc(const Canvas& canvas, float x, float y, float radius, ImVec4 color);
    void DrawSkyBodies(const Canvas& canvas, const Palette& palette, int columns, float surface, float daylight);
}
