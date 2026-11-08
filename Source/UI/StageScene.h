#pragma once

#include "Core/Settings.h"
#include "UI/HomeScene.h"

#include "imgui.h"

namespace Idler::UI::StageScene
{
    struct Frame
    {
        HomeScene::Canvas Paint;
        int Columns = 0;
        int Rows = 0;
        float Ground = 0.0f;
        float Daylight = 1.0f;
        SceneSeason Season = SceneSeason::Summer;
        double Time = 0.0;
        bool Game = false;
    };

    void DrawMeadowBack(const Frame& frame);
    void DrawMeadowFront(const Frame& frame);
    void DrawCloudsBack(const Frame& frame);
    void DrawCloudsFront(const Frame& frame);
    void DrawUnderwaterBack(const Frame& frame);
    void DrawUnderwaterFront(const Frame& frame);

    void DrawStageSky(const HomeScene::Canvas& canvas, int columns, float low, ImVec4 top, ImVec4 bottom, float daylight);
    void DrawStageLight(const HomeScene::Canvas& canvas, float x, float y, float daylight);
    void DrawPuff(const HomeScene::Canvas& canvas, float x, float base, float width, ImVec4 light, ImVec4 shade, std::uint32_t seed);
}
