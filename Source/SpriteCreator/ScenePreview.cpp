#include "SpriteCreator/ScenePreview.h"

#include "UI/HomeScene.h"
#include "UI/IdleAnimation.h"
#include "UI/SceneKit.h"
#include "UI/StageScene.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Idler::SpriteCreator
{
    namespace
    {
        using UI::Px;

        struct PropsCache
        {
            int Columns = -1;
            int Rows = -1;
            SceneSeason Season = SceneSeason::ByDate;
            std::vector<UI::HomeScene::Prop> Props;
        };
        PropsCache g_props;

        double SceneTime()
        {
            return UI::IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
        }

        bool IsStage(Backdrop backdrop)
        {
            return backdrop == Backdrop::Meadow || backdrop == Backdrop::Clouds || backdrop == Backdrop::Underwater;
        }

        SceneSeason SeasonOf(Backdrop backdrop)
        {
            switch (backdrop)
            {
            case Backdrop::Spring:
                return SceneSeason::Spring;
            case Backdrop::Autumn:
                return SceneSeason::Autumn;
            case Backdrop::Winter:
                return SceneSeason::Winter;
            default:
                return SceneSeason::Summer;
            }
        }

        void DrawCreature(ImDrawList* drawList, const UI::CreatureSprites::Sprite* sprite, float middleX, float feetY, float pixel, std::uint32_t seed)
        {
            if (!sprite)
                return;
            const ImVec2 at(std::round(middleX - pixel * static_cast<float>(UI::CreatureSprites::kColumns) * 0.5f),
                std::round(feetY - pixel * static_cast<float>(UI::CreatureSprites::kRows)));
            UI::IdleAnimation::DrawSprite(drawList, at, *sprite, UI::IdleAnimation::Mood::Idling, seed, pixel);
        }

        void DrawStage(ImDrawList* drawList, ImVec2 min, ImVec2 max, const UI::CreatureSprites::Sprite* sprite, Backdrop stage, float daylight, std::uint32_t seed)
        {
            const float pixel = std::max(1.0f, std::round(Px(2.0f)));
            UI::StageScene::Frame frame;
            frame.Paint = UI::HomeScene::Canvas{ .List = drawList, .Min = min, .Pixel = pixel };
            frame.Columns = static_cast<int>(std::ceil((max.x - min.x) / pixel));
            frame.Rows = static_cast<int>(std::ceil((max.y - min.y) / pixel));
            frame.Ground = std::round(static_cast<float>(frame.Rows) * (stage == Backdrop::Clouds ? 0.78f : stage == Backdrop::Underwater ? 0.84f : 0.9f));
            frame.Daylight = daylight;
            frame.Season = SceneSeason::Summer;
            frame.Time = SceneTime();

            if (stage == Backdrop::Clouds)
                UI::StageScene::DrawCloudsBack(frame);
            else if (stage == Backdrop::Underwater)
                UI::StageScene::DrawUnderwaterBack(frame);
            else
                UI::StageScene::DrawMeadowBack(frame);

            const float creaturePixel = std::max(2.0f, std::round(Px(5.0f)));
            DrawCreature(drawList, sprite, (min.x + max.x) * 0.5f, min.y + frame.Ground * pixel, creaturePixel, seed);

            if (stage == Backdrop::Clouds)
                UI::StageScene::DrawCloudsFront(frame);
            else if (stage == Backdrop::Underwater)
                UI::StageScene::DrawUnderwaterFront(frame);
            else
                UI::StageScene::DrawMeadowFront(frame);
        }

        void DrawHome(ImDrawList* drawList, ImVec2 min, ImVec2 max, const UI::CreatureSprites::Sprite* sprite, SceneSeason season, float daylight, std::uint32_t seed)
        {
            namespace Home = UI::HomeScene;
            const float pixel = std::max(1.0f, std::round(Px(3.0f)));
            Home::Frame frame;
            frame.Paint = { drawList, min, pixel, 0.0f, ImVec4() };
            frame.Columns = static_cast<int>(std::ceil((max.x - min.x) / pixel));
            frame.Rows = static_cast<int>(std::ceil((max.y - min.y) / pixel));
            const float height = static_cast<float>(frame.Rows);
            const float groundDepth = std::round(height * 0.42f);
            const float lakeDepth = std::round(height * 0.14f);
            frame.Surface = std::floor(std::max(0.0f, height - groundDepth - 3.0f));
            frame.Shore = std::floor(height - lakeDepth);
            frame.Daylight = daylight;
            frame.Time = SceneTime();
            const UI::SceneKit::Palette palette = UI::SceneKit::PaletteAt(daylight, season);
            frame.HazeColor = UI::SceneKit::Mix(palette.HillFar, palette.Sky, 0.5f);
            frame.FeetBack = height - groundDepth;
            frame.FeetFront = height - lakeDepth - 1.0f;
            const float below = frame.Surface + 15.0f;
            const float high = std::round(std::max(16.0f, frame.Surface * 0.2f));
            if (daylight >= 0.3f)
            {
                frame.Sun.X = std::round(static_cast<float>(frame.Columns) * 0.2f);
                frame.Sun.Y = std::round(below + (high - below) * daylight);
                frame.Sun.Height = daylight;
                frame.Sun.Moon = false;
                frame.Sun.Strength = 1.0f;
            }
            else
            {
                frame.Sun.X = std::round(static_cast<float>(frame.Columns) * 0.8f);
                frame.Sun.Y = std::round(high + (below - high) * daylight);
                frame.Sun.Height = 1.0f - daylight;
                frame.Sun.Moon = true;
                frame.Sun.Strength = 0.45f;
            }

            Home::Scene& scene = Home::SceneOf(season);
            frame.HazeColor = scene.HazeColor(frame);
            if (g_props.Columns != frame.Columns || g_props.Rows != frame.Rows || g_props.Season != season)
            {
                g_props.Columns = frame.Columns;
                g_props.Rows = frame.Rows;
                g_props.Season = season;
                g_props.Props.clear();
                scene.PlaceProps(frame, g_props.Props);
                std::stable_sort(g_props.Props.begin(), g_props.Props.end(), [](const Home::Prop& lhs, const Home::Prop& rhs) { return lhs.Ground < rhs.Ground; });
            }

            scene.DrawBack(frame);

            const float middle = static_cast<float>(frame.Columns) * 0.5f;
            const float feet = std::round(frame.FeetBack + (frame.FeetFront - frame.FeetBack) * 0.6f);
            const float scale = frame.ScaleAt(feet);
            for (const Home::Prop& prop : g_props.Props)
            {
                const float propScale = frame.ScaleAt(prop.Ground);
                Home::DrawCastShadow(frame, prop.X, prop.Ground, prop.Foot * propScale, prop.Height * propScale, prop.Crown * propScale);
            }
            const float half = static_cast<float>(UI::CreatureSprites::kColumns) * 0.5f * scale;
            Home::DrawCastShadow(frame, middle, feet, half * 0.6f, static_cast<float>(UI::CreatureSprites::kRows) * 0.7f * scale, half * 0.65f);

            const auto drawProp = [&](const Home::Prop& prop)
                {
                    const float propScale = frame.ScaleAt(prop.Ground);
                    const Home::Canvas paint{ drawList, ImVec2(std::round(min.x + prop.X * pixel), std::round(min.y + prop.Ground * pixel)), pixel * propScale, frame.HazeAt(prop.Ground),
                        frame.HazeColor };
                    scene.DrawProp(frame, prop, paint);
                };
            std::size_t next = 0;
            while (next < g_props.Props.size() && g_props.Props[next].Ground <= feet)
                drawProp(g_props.Props[next++]);
            DrawCreature(drawList, sprite, min.x + middle * pixel, min.y + feet * pixel, pixel * scale, seed);
            while (next < g_props.Props.size())
                drawProp(g_props.Props[next++]);

            scene.DrawFront(frame);
        }
    }

    const char* BackdropName(Backdrop backdrop)
    {
        switch (backdrop)
        {
        case Backdrop::OwnStage:
            return "Its own stage";
        case Backdrop::Meadow:
            return "Meadow";
        case Backdrop::Clouds:
            return "Clouds";
        case Backdrop::Underwater:
            return "Underwater";
        case Backdrop::Spring:
            return "Home in spring";
        case Backdrop::Summer:
            return "Home in summer";
        case Backdrop::Autumn:
            return "Home in autumn";
        case Backdrop::Winter:
            return "Home in winter";
        case Backdrop::Plain:
            return "Plain card";
        }
        return "";
    }

    const char* BackdropKey(Backdrop backdrop)
    {
        switch (backdrop)
        {
        case Backdrop::OwnStage:
            return "own";
        case Backdrop::Meadow:
            return "meadow";
        case Backdrop::Clouds:
            return "clouds";
        case Backdrop::Underwater:
            return "underwater";
        case Backdrop::Spring:
            return "spring";
        case Backdrop::Summer:
            return "summer";
        case Backdrop::Autumn:
            return "autumn";
        case Backdrop::Winter:
            return "winter";
        case Backdrop::Plain:
            return "plain";
        }
        return "";
    }

    void DrawScenePreview(ImVec2 min, ImVec2 max, const UI::CreatureSprites::Sprite* sprite, Habit habit, Backdrop backdrop, float daylight, std::uint32_t seed)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(min, max, true);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::Sidebar));

        Backdrop shown = backdrop;
        if (shown == Backdrop::OwnStage)
            shown = habit == Habit::Floats ? Backdrop::Clouds : habit == Habit::Swims ? Backdrop::Underwater : Backdrop::Meadow;

        if (IsStage(shown))
        {
            DrawStage(drawList, min, max, sprite, shown, daylight, seed);
        }
        else if (shown == Backdrop::Plain)
        {
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::Card));
            const float pixel = std::max(2.0f, std::round(Px(5.0f)));
            DrawCreature(drawList, sprite, (min.x + max.x) * 0.5f, std::round((min.y + max.y) * 0.5f + pixel * static_cast<float>(UI::CreatureSprites::kRows) * 0.5f), pixel, seed);
        }
        else
        {
            DrawHome(drawList, min, max, sprite, SeasonOf(shown), daylight, seed);
        }

        if (UI::IdleAnimation::Enabled())
            UI::IdleAnimation::RequestFrames();
        drawList->PopClipRect();
    }
}
