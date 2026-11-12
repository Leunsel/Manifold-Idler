#include "UI/SceneKit.h"

#include "Core/Creatures.h"
#include "UI/HomeScene.h"
#include "UI/IdleAnimation.h"
#include "UI/StageScene.h"
#include "UI/Theme.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace Idler::UI
{
    using namespace SceneKit;

    namespace SceneKit
    {
        namespace
        {
            constexpr Palette kSeasonNight[] = {
                { Rgb(0.054f, 0.059f, 0.071f), Rgb(0.300f, 0.320f, 0.360f), Rgb(0.074f, 0.130f, 0.096f), Rgb(0.120f, 0.215f, 0.150f), Rgb(0.175f, 0.340f, 0.215f),
                  Rgb(0.056f, 0.114f, 0.150f), Rgb(0.040f, 0.086f, 0.118f), Rgb(0.100f, 0.200f, 0.250f), Rgb(0.085f, 0.165f, 0.210f), Rgb(0.070f, 0.082f, 0.092f), Rgb(0.068f, 0.098f, 0.088f), Rgb(0.070f, 0.140f, 0.100f) },
                { Rgb(0.054f, 0.059f, 0.071f), Rgb(0.300f, 0.320f, 0.360f), Rgb(0.070f, 0.118f, 0.104f), Rgb(0.112f, 0.196f, 0.164f), Rgb(0.150f, 0.300f, 0.235f),
                  Rgb(0.056f, 0.114f, 0.150f), Rgb(0.040f, 0.086f, 0.118f), Rgb(0.100f, 0.200f, 0.250f), Rgb(0.085f, 0.165f, 0.210f), Rgb(0.070f, 0.080f, 0.094f), Rgb(0.066f, 0.092f, 0.094f), Rgb(0.064f, 0.124f, 0.110f) },
                { Rgb(0.058f, 0.056f, 0.064f), Rgb(0.300f, 0.300f, 0.330f), Rgb(0.170f, 0.098f, 0.044f), Rgb(0.280f, 0.160f, 0.060f), Rgb(0.380f, 0.215f, 0.070f),
                  Rgb(0.052f, 0.100f, 0.130f), Rgb(0.038f, 0.076f, 0.104f), Rgb(0.110f, 0.180f, 0.210f), Rgb(0.085f, 0.150f, 0.180f), Rgb(0.082f, 0.072f, 0.076f), Rgb(0.098f, 0.074f, 0.062f), Rgb(0.150f, 0.085f, 0.050f) },
                { Rgb(0.060f, 0.068f, 0.090f), Rgb(0.420f, 0.450f, 0.520f), Rgb(0.150f, 0.175f, 0.215f), Rgb(0.260f, 0.295f, 0.350f), Rgb(0.200f, 0.235f, 0.285f),
                  Rgb(0.070f, 0.125f, 0.165f), Rgb(0.052f, 0.098f, 0.135f), Rgb(0.300f, 0.370f, 0.430f), Rgb(0.120f, 0.190f, 0.240f), Rgb(0.095f, 0.110f, 0.140f), Rgb(0.120f, 0.140f, 0.175f), Rgb(0.060f, 0.105f, 0.105f) },
            };

            constexpr Palette kSeasonDay[] = {
                { Rgb(0.300f, 0.480f, 0.640f), Rgb(0.300f, 0.480f, 0.640f), Rgb(0.150f, 0.320f, 0.175f), Rgb(0.270f, 0.480f, 0.270f), Rgb(0.400f, 0.660f, 0.350f),
                  Rgb(0.055f, 0.290f, 0.340f), Rgb(0.035f, 0.190f, 0.255f), Rgb(0.580f, 0.850f, 0.830f), Rgb(0.130f, 0.430f, 0.460f), Rgb(0.240f, 0.390f, 0.430f), Rgb(0.200f, 0.370f, 0.320f), Rgb(0.130f, 0.310f, 0.190f) },
                { Rgb(0.250f, 0.395f, 0.515f), Rgb(0.250f, 0.395f, 0.515f), Rgb(0.125f, 0.255f, 0.195f), Rgb(0.205f, 0.395f, 0.295f), Rgb(0.270f, 0.505f, 0.360f),
                  Rgb(0.050f, 0.265f, 0.315f), Rgb(0.030f, 0.175f, 0.240f), Rgb(0.520f, 0.800f, 0.790f), Rgb(0.115f, 0.400f, 0.440f), Rgb(0.190f, 0.310f, 0.370f), Rgb(0.165f, 0.300f, 0.310f), Rgb(0.105f, 0.245f, 0.200f) },
                { Rgb(0.310f, 0.450f, 0.570f), Rgb(0.310f, 0.450f, 0.570f), Rgb(0.640f, 0.340f, 0.075f), Rgb(0.900f, 0.540f, 0.120f), Rgb(0.980f, 0.660f, 0.160f),
                  Rgb(0.055f, 0.235f, 0.280f), Rgb(0.035f, 0.160f, 0.215f), Rgb(0.700f, 0.760f, 0.650f), Rgb(0.140f, 0.350f, 0.370f), Rgb(0.420f, 0.340f, 0.300f), Rgb(0.520f, 0.300f, 0.150f), Rgb(0.720f, 0.340f, 0.090f) },
                { Rgb(0.420f, 0.530f, 0.630f), Rgb(0.420f, 0.530f, 0.630f), Rgb(0.620f, 0.700f, 0.780f), Rgb(0.900f, 0.950f, 0.985f), Rgb(0.760f, 0.830f, 0.890f),
                  Rgb(0.160f, 0.340f, 0.450f), Rgb(0.100f, 0.250f, 0.360f), Rgb(0.880f, 0.950f, 0.985f), Rgb(0.320f, 0.520f, 0.620f), Rgb(0.470f, 0.570f, 0.660f), Rgb(0.540f, 0.640f, 0.720f), Rgb(0.120f, 0.280f, 0.250f) },
            };
        }

        std::uint32_t Noise(std::uint32_t value)
        {
            return MixSeed(value + 0x7E44A21Du);
        }

        ImVec4 Mix(ImVec4 night, ImVec4 day, float share)
        {
            return ImVec4(night.x + (day.x - night.x) * share, night.y + (day.y - night.y) * share, night.z + (day.z - night.z) * share, 1.0f);
        }

        ImVec4 Lit(ImVec4 color, float daylight)
        {
            const float share = 0.35f + 0.65f * daylight;
            return ImVec4(color.x * share, color.y * share, color.z * share, color.w);
        }

        Palette PaletteAt(float daylight, SceneSeason season)
        {
            const auto index = static_cast<std::size_t>(season) - 1u;
            const Palette& night = kSeasonNight[index];
            const Palette& day = kSeasonDay[index];
            return { Mix(night.Sky, day.Sky, daylight), Mix(night.Star, day.Star, daylight), Mix(night.Ground, day.Ground, daylight), Mix(night.GroundEdge, day.GroundEdge, daylight), Mix(night.Grass, day.Grass, daylight),
                Mix(night.Water, day.Water, daylight), Mix(night.WaterDeep, day.WaterDeep, daylight), Mix(night.WaterEdge, day.WaterEdge, daylight), Mix(night.Ripple, day.Ripple, daylight),
                Mix(night.HillFar, day.HillFar, daylight), Mix(night.HillNear, day.HillNear, daylight), Mix(night.Tree, day.Tree, daylight) };
        }

        void DrawDisc(const Canvas& canvas, float x, float y, float radius, ImVec4 color)
        {
            for (float row = -radius; row <= radius; row += 1.0f)
            {
                const float half = std::floor(std::sqrt(radius * radius - row * row));
                canvas.Fill(x - half, y + row, x + half + 1.0f, y + row + 1.0f, color);
            }
        }

        void DrawSkyBodies(const Canvas& canvas, const Palette& palette, int columns, float surface, float daylight)
        {
            const float below = surface + 15.0f;
            const float high = std::round(std::max(16.0f, surface * 0.2f));
            if (daylight < 1.0f)
            {
                const float x = std::round(static_cast<float>(columns) * 0.8f);
                const float y = std::round(high + (below - high) * daylight);
                DrawDisc(canvas, x, y, 9.0f, kMoon);
                canvas.Fill(x - 4.0f, y - 3.0f, x - 1.0f, y - 1.0f, kCrater);
                canvas.Fill(x + 2.0f, y + 1.0f, x + 5.0f, y + 4.0f, kCrater);
                canvas.Fill(x - 3.0f, y + 4.0f, x - 1.0f, y + 5.0f, kCrater);
            }
            if (daylight > 0.0f)
            {
                const float x = std::round(static_cast<float>(columns) * 0.2f);
                const float y = std::round(below + (high - below) * daylight);
                canvas.Fill(x - 1.0f, y - 13.0f, x + 2.0f, y - 10.0f, kSun);
                canvas.Fill(x - 1.0f, y + 11.0f, x + 2.0f, y + 14.0f, kSun);
                canvas.Fill(x - 13.0f, y - 1.0f, x - 10.0f, y + 2.0f, kSun);
                canvas.Fill(x + 11.0f, y - 1.0f, x + 14.0f, y + 2.0f, kSun);
                canvas.Fill(x - 10.0f, y - 10.0f, x - 8.0f, y - 8.0f, kSun);
                canvas.Fill(x + 9.0f, y - 10.0f, x + 11.0f, y - 8.0f, kSun);
                canvas.Fill(x - 10.0f, y + 9.0f, x - 8.0f, y + 11.0f, kSun);
                canvas.Fill(x + 9.0f, y + 9.0f, x + 11.0f, y + 11.0f, kSun);
                DrawDisc(canvas, x, y, 8.0f, kSun);

                const double drift = IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
                const ImVec4 cloud = Mix(palette.Sky, kCloud, daylight);
                constexpr double kSpacing = 110.0;
                constexpr std::uint32_t kPlay = 70u;
                for (std::uint32_t layer = 0; layer < 2u; ++layer)
                {
                    const double travelled = drift * (layer == 0u ? 0.8 : 1.3);
                    const auto first = static_cast<long long>(std::floor((-16.0 - static_cast<double>(kPlay) - travelled) / kSpacing));
                    const auto last = static_cast<long long>(std::floor((static_cast<double>(columns) - travelled) / kSpacing));
                    for (long long index = first; index <= last; ++index)
                    {
                        const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 53u + 5u + layer * 101u);
                        const auto left = static_cast<float>(std::floor(static_cast<double>(index) * kSpacing + static_cast<double>(mixed % kPlay) + travelled));
                        const float top = 4.0f + std::floor(static_cast<float>((mixed >> 8) & 255u) / 255.0f * surface * 0.4f);
                        canvas.Fill(left + 3.0f, top, left + 10.0f, top + 1.0f, cloud);
                        canvas.Fill(left, top + 1.0f, left + 15.0f, top + 2.0f, cloud);
                        canvas.Fill(left + 2.0f, top + 2.0f, left + 12.0f, top + 3.0f, cloud);
                    }
                }
            }
        }
    }

    namespace HomeScene
    {
        void Canvas::Fill(float left, float top, float right, float bottom, ImVec4 color) const
        {
            if (right <= left || bottom <= top)
                return;
            if (Haze > 0.0f)
            {
                color.x += (HazeColor.x - color.x) * Haze;
                color.y += (HazeColor.y - color.y) * Haze;
                color.z += (HazeColor.z - color.z) * Haze;
            }
            List->AddRectFilled(ImVec2(Min.x + Pixel * left, Min.y + Pixel * top), ImVec2(Min.x + Pixel * right, Min.y + Pixel * bottom), ImGui::GetColorU32(color));
        }

        float Frame::ScaleAt(float ground) const
        {
            const float share = std::clamp((ground - FeetBack) / std::max(1.0f, FeetFront - FeetBack), 0.0f, 1.0f);
            return kBackScale + (kFrontScale - kBackScale) * share;
        }

        float Frame::HazeAt(float ground) const
        {
            const float share = std::clamp((ground - FeetBack) / std::max(1.0f, FeetFront - FeetBack), 0.0f, 1.0f);
            return kBackHaze * std::max(0.0f, 1.0f - share / 0.7f);
        }

        std::uint32_t Noise(std::uint32_t value)
        {
            return UI::Noise(value);
        }

        ImVec4 Rgb(float red, float green, float blue)
        {
            return ImVec4(red, green, blue, 1.0f);
        }

        ImVec4 Mix(ImVec4 from, ImVec4 to, float share)
        {
            return ImVec4(from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share, from.z + (to.z - from.z) * share, from.w + (to.w - from.w) * share);
        }

        ImVec4 Lit(ImVec4 color, float daylight)
        {
            return UI::Lit(color, daylight);
        }

        ImVec4 DayNight(ImVec4 day, ImVec4 night, float daylight)
        {
            return Mix(night, day, daylight);
        }

        void DrawDisc(const Canvas& canvas, float x, float y, float radius, ImVec4 color)
        {
            for (float row = -radius; row <= radius; row += 1.0f)
            {
                const float half = std::floor(std::sqrt(radius * radius - row * row));
                canvas.Fill(x - half, y + row, x + half + 1.0f, y + row + 1.0f, color);
            }
        }

        void DrawSky(const Frame& frame, ImVec4 sky)
        {
            const UI::Canvas canvas{ frame.Paint.List, frame.Paint.Min, frame.Paint.Pixel };
            const ImVec4 star(0.560f, 0.590f, 0.660f, 1.0f - frame.Daylight);
            if (star.w > 0.0f)
            {
                Scatter(frame.Columns - 4, static_cast<int>(frame.Surface) - 16, 10, 10, 31u, 128u, [&](float x, float y, std::uint32_t)
                    {
                        canvas.Fill(x + 2.0f, y + 2.0f, x + 3.0f, y + 3.0f, star);
                    });
            }
            Palette palette{};
            palette.Sky = sky;
            DrawSkyBodies(canvas, palette, frame.Columns, frame.Surface, frame.Daylight);
        }

        void DrawCastShadow(const Frame& frame, float x, float ground, float foot, float height, float crown, float strength)
        {
            const Light& light = frame.Sun;
            const float away = x >= light.X ? 1.0f : -1.0f;
            const float length = height * (0.2f + 0.9f * (1.0f - std::clamp(light.Height, 0.0f, 1.0f)));
            const float alpha = (kShadowAlpha / 255.0f) * light.Strength * strength * (0.7f + 0.3f * light.Height);
            if (alpha <= 0.0f)
                return;
            const ImVec4 dark(0.0f, 0.0f, 0.0f, alpha);
            const Canvas& paint = frame.Paint;
            paint.Fill(std::round(x - foot), std::round(ground - 0.5f), std::round(x + foot), std::round(ground - 0.5f) + 1.0f, dark);
            const float endX = x + away * length * 0.75f;
            const float endY = ground + length * 0.08f;
            const float reach = std::max(crown, foot) + length * 0.15f;
            const float depth = std::max(0.75f, reach * 0.3f);
            const float from = std::min(x, endX);
            const float to = std::max(x, endX);
            paint.Fill(std::round(from), std::round(ground - 0.5f), std::round(to), std::round(ground - 0.5f) + 1.0f, ImVec4(0.0f, 0.0f, 0.0f, alpha * 0.8f));
            const float leafy = std::clamp(9.0f / std::max(1.0f, reach), 0.5f, 1.0f);
            for (float row = -depth; row <= depth; row += 1.0f)
            {
                const float share = row / (depth + 0.5f);
                const float half = std::round(reach * std::sqrt(std::max(0.0f, 1.0f - share * share)));
                if (half <= 0.0f)
                    continue;
                const float y = std::round(endY + row);
                paint.Fill(std::round(endX - half), y, std::round(endX + half), y + 1.0f, ImVec4(0.0f, 0.0f, 0.0f, alpha * 0.85f * leafy));
            }
        }

        Scene& SceneOf(SceneSeason season)
        {
            static const std::unique_ptr<Scene> spring = MakeSpring();
            static const std::unique_ptr<Scene> summer = MakeSummer();
            static const std::unique_ptr<Scene> autumn = MakeAutumn();
            static const std::unique_ptr<Scene> winter = MakeWinter();
            switch (season)
            {
            case SceneSeason::Spring:
                return *spring;
            case SceneSeason::Summer:
                return *summer;
            case SceneSeason::Winter:
                return *winter;
            default:
                return *autumn;
            }
        }
    }

    namespace StageScene
    {
        void DrawStageSky(const HomeScene::Canvas& canvas, int columns, float low, ImVec4 top, ImVec4 bottom, float daylight)
        {
            constexpr int kBands = 6;
            for (int band = 0; band < kBands; ++band)
            {
                const float from = std::floor(low * static_cast<float>(band) / kBands);
                const float to = std::floor(low * static_cast<float>(band + 1) / kBands);
                canvas.Fill(0.0f, from, static_cast<float>(columns), to, Mix(top, bottom, static_cast<float>(band) / (kBands - 1)));
            }
            if (daylight < 1.0f)
            {
                Scatter(columns - 2, static_cast<int>(low) - 3, 9, 7, 41u, 110u, [&](float x, float y, std::uint32_t mixed)
                    {
                        const float bright = ((mixed >> 8) & 3u) == 0u ? 1.0f : 0.6f;
                        canvas.Fill(x + 1.0f, y + 1.0f, x + 2.0f, y + 2.0f, ManifoldTheme::WithAlpha(Rgb(0.80f, 0.84f, 0.92f), (1.0f - daylight) * bright));
                    });
            }
        }

        void DrawStageLight(const HomeScene::Canvas& canvas, float x, float y, float daylight)
        {
            if (daylight > 0.0f)
            {
                const ImVec4 sun = ManifoldTheme::WithAlpha(kSun, daylight);
                canvas.Fill(x - 1.0f, y - 8.0f, x + 2.0f, y - 6.0f, sun);
                canvas.Fill(x - 1.0f, y + 7.0f, x + 2.0f, y + 9.0f, sun);
                canvas.Fill(x - 8.0f, y - 1.0f, x - 6.0f, y + 2.0f, sun);
                canvas.Fill(x + 7.0f, y - 1.0f, x + 9.0f, y + 2.0f, sun);
                HomeScene::DrawDisc(canvas, x, y, 4.0f, sun);
            }
            if (daylight < 1.0f)
            {
                const float night = 1.0f - daylight;
                HomeScene::DrawDisc(canvas, x, y, 4.0f, ManifoldTheme::WithAlpha(kMoon, night));
                canvas.Fill(x - 2.0f, y - 2.0f, x, y, ManifoldTheme::WithAlpha(kCrater, night));
                canvas.Fill(x + 1.0f, y + 1.0f, x + 3.0f, y + 2.0f, ManifoldTheme::WithAlpha(kCrater, night));
            }
        }

        void DrawPuff(const HomeScene::Canvas& canvas, float x, float base, float width, ImVec4 light, ImVec4 shade, std::uint32_t seed)
        {
            const int puffs = std::max(2, static_cast<int>(width / 7.0f));
            for (int index = 0; index < puffs; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 31u + static_cast<std::uint32_t>(index));
                const float radius = 3.0f + static_cast<float>(mixed % 4u);
                const float center = std::round(x + width * (static_cast<float>(index) + 0.5f) / static_cast<float>(puffs));
                HomeScene::DrawDisc(canvas, center, base - radius + 1.0f, radius, light);
            }
            canvas.Fill(x, base - 2.0f, x + width, base + 1.0f, light);
            canvas.Fill(x + 2.0f, base, x + width - 2.0f, base + 1.0f, shade);
        }
    }
}
