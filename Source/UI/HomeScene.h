#pragma once

#include "Core/Settings.h"

#include "imgui.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Idler::UI::HomeScene
{
    struct Canvas
    {
        ImDrawList* List = nullptr;
        ImVec2 Min;
        float Pixel = 1.0f;
        float Haze = 0.0f;
        ImVec4 HazeColor;

        void Fill(float left, float top, float right, float bottom, ImVec4 color) const;
    };

    struct Light
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Height = 1.0f;
        bool Moon = false;
        float Strength = 1.0f;
    };

    struct Frame
    {
        Canvas Paint;
        int Columns = 0;
        int Rows = 0;
        float Surface = 0.0f;
        float Shore = 0.0f;
        float Daylight = 1.0f;
        Light Sun;
        double Time = 0.0;
        ImVec4 HazeColor;
        float FeetBack = 0.0f;
        float FeetFront = 0.0f;

        float ScaleAt(float ground) const;
        float HazeAt(float ground) const;
    };

    struct Prop
    {
        float X = 0.0f;
        float Ground = 0.0f;
        float Foot = 0.0f;
        float Cover = 0.0f;
        float CoverHeight = 0.0f;
        float Height = 0.0f;
        float Crown = 0.0f;
        std::uint32_t Kind = 0;
        std::uint32_t Seed = 0;
    };

    class Scene
    {
    public:
        virtual ~Scene() = default;
        virtual void DrawBack(const Frame& frame) = 0;
        virtual void PlaceProps(const Frame& frame, std::vector<Prop>& props) = 0;
        virtual void DrawProp(const Frame& frame, const Prop& prop, const Canvas& paint) = 0;
        virtual void DrawFront(const Frame& frame) = 0;
        virtual ImVec4 HazeColor(const Frame& frame) { return frame.HazeColor; }
    };

    std::unique_ptr<Scene> MakeSpring();
    std::unique_ptr<Scene> MakeSummer();
    std::unique_ptr<Scene> MakeAutumn();
    std::unique_ptr<Scene> MakeWinter();
    Scene& SceneOf(SceneSeason season);

    std::uint32_t Noise(std::uint32_t value);
    ImVec4 Rgb(float red, float green, float blue);
    ImVec4 Mix(ImVec4 from, ImVec4 to, float share);
    ImVec4 Lit(ImVec4 color, float daylight);
    ImVec4 DayNight(ImVec4 day, ImVec4 night, float daylight);
    void DrawDisc(const Canvas& canvas, float x, float y, float radius, ImVec4 color);
    void DrawSky(const Frame& frame, ImVec4 sky);
    void DrawCastShadow(const Frame& frame, float x, float ground, float foot, float height, float crown, float strength = 1.0f);
    void DrawClassicBack(const Frame& frame, SceneSeason season);
    void DrawClassicFront(const Frame& frame, SceneSeason season);
}
