#include "UI/TerrariumView.h"

#include "App/Desktop.h"
#include "App/IdlerModel.h"
#include "Core/Creatures.h"
#include "Core/Terrarium.h"
#include "UI/HomeScene.h"
#include "UI/StageScene.h"
#include "UI/IdleAnimation.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Idler::UI
{
    namespace
    {
        constexpr ImVec4 kMoon = ImVec4(0.560f, 0.590f, 0.560f, 1.0f);
        constexpr ImVec4 kCrater = ImVec4(0.460f, 0.490f, 0.470f, 1.0f);
        constexpr ImVec4 kFlowerOne = ImVec4(0.700f, 0.620f, 0.300f, 1.0f);
        constexpr ImVec4 kFlowerTwo = ImVec4(0.640f, 0.400f, 0.520f, 1.0f);
        constexpr ImVec4 kFlowerThree = ImVec4(0.900f, 0.880f, 0.820f, 1.0f);
        constexpr ImVec4 kBlossom = ImVec4(0.950f, 0.620f, 0.740f, 1.0f);
        constexpr ImVec4 kSnow = ImVec4(0.930f, 0.960f, 1.000f, 1.0f);
        constexpr ImVec4 kBark = ImVec4(0.300f, 0.180f, 0.100f, 1.0f);
        constexpr ImVec4 kFirefly = ImVec4(0.900f, 0.950f, 0.350f, 1.0f);
        constexpr ImVec4 kLeaves[] = { ImVec4(0.950f, 0.470f, 0.100f, 1.0f), ImVec4(0.860f, 0.240f, 0.120f, 1.0f), ImVec4(0.980f, 0.720f, 0.160f, 1.0f) };
        constexpr ImVec4 kSun = ImVec4(0.960f, 0.800f, 0.400f, 1.0f);
        constexpr ImVec4 kCloud = ImVec4(0.620f, 0.720f, 0.780f, 1.0f);

        constexpr float kDawnSeconds = 4.0f;
        constexpr double kCrossSeconds = 1.8;
        constexpr double kStampSeconds = 0.7;

        Terrarium g_terrarium;
        Terrarium g_home(2u, static_cast<int>(kCreatureCount));
        constexpr std::size_t kHomeSleepers = 6;
        constexpr double kSleeperTurnSeconds = 40.0;
        std::unordered_map<std::uint32_t, double> g_pettedAt;
        std::unordered_map<std::uint32_t, double> g_pettedAtHome;
        std::uint32_t g_grabbed = 0;
        ImVec2 g_grabbedAt;
        bool g_carrying = false;
        bool g_ballInHand = false;
        bool g_shellInHand = false;
        constexpr float kShadowAlpha = 70.0f;
        std::unordered_map<std::uint32_t, double> g_crossUntil;
        float g_daylight = -1.0f;
        float g_lightPreview = -1.0f;
        std::vector<HomeScene::Prop> g_props;
        struct PropsKey
        {
            int Columns = -1;
            int Rows = -1;
            float Surface = -1.0f;
            float Shore = -1.0f;
            SceneSeason Season = SceneSeason::ByDate;
            bool operator==(const PropsKey&) const = default;
        };
        PropsKey g_propsKey;
        TerrariumSize g_sceneCells;
        ImVec2 g_sceneMax;

        void MaskCorners(ImDrawList* drawList, ImVec2 min, ImVec2 max, float radius, ImU32 color);

        struct TipWait
        {
            std::uint32_t Seed = 0;
            double Since = 0.0;
        };
        TipWait g_tipWait[2];
        constexpr double kTipDelaySeconds = 0.6;
        constexpr std::uint32_t kThingTip = 0xFFFFFFFFu;

        bool TipDue(bool home, std::uint32_t seed, double now)
        {
            TipWait& wait = g_tipWait[home ? 1 : 0];
            if (wait.Seed != seed)
            {
                wait.Seed = seed;
                wait.Since = now;
            }
            return now - wait.Since >= kTipDelaySeconds;
        }

        void ForgetTip(bool home)
        {
            g_tipWait[home ? 1 : 0].Seed = 0;
        }

        bool g_tugPlaced = false;
        ImVec2 g_tugPlacedAt;
        constexpr float kTugSlack = 3.0f;

        struct HomeWork
        {
            std::uint32_t Game = 0;
            int Games = 0;
        };
        std::array<HomeWork, kCreatureCount> g_work;

        std::uint32_t Noise(std::uint32_t value)
        {
            return MixSeed(value + 0x7E44A21Du);
        }

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

        ImVec4 Mix(ImVec4 night, ImVec4 day, float share)
        {
            return ImVec4(night.x + (day.x - night.x) * share, night.y + (day.y - night.y) * share, night.z + (day.z - night.z) * share, 1.0f);
        }

        ImVec4 Lit(ImVec4 color, float daylight)
        {
            const float share = 0.35f + 0.65f * daylight;
            return ImVec4(color.x * share, color.y * share, color.z * share, color.w);
        }

        struct Palette
        {
            ImVec4 Sky, Star, Ground, GroundEdge, Grass, Water, WaterDeep, WaterEdge, Ripple, HillFar, HillNear, Tree;
        };

        constexpr ImVec4 Rgb(float red, float green, float blue)
        {
            return ImVec4(red, green, blue, 1.0f);
        }

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

        Palette PaletteAt(float daylight, SceneSeason season)
        {
            const auto index = static_cast<std::size_t>(season) - 1u;
            const Palette& night = kSeasonNight[index];
            const Palette& day = kSeasonDay[index];
            return { Mix(night.Sky, day.Sky, daylight), Mix(night.Star, day.Star, daylight), Mix(night.Ground, day.Ground, daylight), Mix(night.GroundEdge, day.GroundEdge, daylight), Mix(night.Grass, day.Grass, daylight),
                Mix(night.Water, day.Water, daylight), Mix(night.WaterDeep, day.WaterDeep, daylight), Mix(night.WaterEdge, day.WaterEdge, daylight), Mix(night.Ripple, day.Ripple, daylight),
                Mix(night.HillFar, day.HillFar, daylight), Mix(night.HillNear, day.HillNear, daylight), Mix(night.Tree, day.Tree, daylight) };
        }

        float UpdateDaylight(const IdlerModel& model)
        {
            if (g_lightPreview >= 0.0f)
                return g_daylight = g_lightPreview;
            const float target = model.RunningCount() > 0 ? 1.0f : 0.0f;
            if (g_daylight < 0.0f || !IdleAnimation::Enabled())
                g_daylight = target;
            const float step = std::min(ImGui::GetIO().DeltaTime, 0.1f) / kDawnSeconds;
            g_daylight = std::clamp(target, g_daylight - step, g_daylight + step);
            if (g_daylight != target)
                IdleAnimation::RequestFrames();
            return g_daylight;
        }

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

        void DrawDistance(const Canvas& canvas, const Palette& palette, int columns, float surface, SceneSeason season, float daylight)
        {
            const ImVec4 mountain = Mix(palette.HillFar, palette.Sky, 0.6f);
            const ImVec4 peak = season == SceneSeason::Winter ? Mix(Lit(kSnow, daylight), palette.Sky, 0.5f) : Mix(palette.HillFar, palette.Sky, 0.45f);
            for (int column = 0; column < columns; ++column)
            {
                const auto at = static_cast<double>(column);
                const auto high = static_cast<float>(21.0 + 7.0 * std::sin(at * 0.023 + 0.4) + 4.0 * std::sin(at * 0.071 + 2.0) + 1.5 * std::sin(at * 0.19));
                const auto x = static_cast<float>(column);
                canvas.Fill(x, surface - std::round(high), x + 1.0f, surface + 4.0f, mountain);
                canvas.Fill(x, surface - std::round(high), x + 1.0f, surface - std::round(high) + 1.0f, peak);
            }
            for (int column = 0; column < columns; ++column)
            {
                const auto at = static_cast<double>(column);
                const auto farHill = static_cast<float>(9.0 + 6.0 * std::sin(at * 0.037 + 1.3) + 3.0 * std::sin(at * 0.091));
                const auto x = static_cast<float>(column);
                canvas.Fill(x, surface - std::round(farHill), x + 1.0f, surface + 4.0f, palette.HillFar);
            }
            constexpr int kFarTreeSpacing = 9;
            for (int tree = 0; tree * kFarTreeSpacing < columns; ++tree)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(tree) * 613u + 29u);
                if ((mixed >> 24) % 3u == 0u)
                    continue;
                const auto x = static_cast<float>(tree * kFarTreeSpacing + static_cast<int>(mixed % static_cast<std::uint32_t>(kFarTreeSpacing)));
                const int height = 7 + static_cast<int>((mixed >> 8) % 5u);
                ImVec4 crown = palette.Tree;
                if (season == SceneSeason::Autumn)
                    crown = Lit(kLeaves[(mixed >> 16) % std::size(kLeaves)], daylight);
                else if (season == SceneSeason::Winter)
                    crown = Lit(kSnow, daylight);
                crown = Mix(crown, palette.HillFar, 0.5f);
                for (int row = 0; row < height; ++row)
                {
                    const auto half = static_cast<float>(row / 3);
                    canvas.Fill(x - half, surface - 2.0f - static_cast<float>(height - row), x + half + 1.0f, surface - 1.0f - static_cast<float>(height - row), crown);
                }
            }
            for (int column = 0; column < columns; ++column)
            {
                const auto at = static_cast<double>(column);
                const auto nearHill = static_cast<float>(5.0 + 3.5 * std::sin(at * 0.061 + 4.0) + 1.5 * std::sin(at * 0.17));
                const auto x = static_cast<float>(column);
                canvas.Fill(x, surface - std::round(nearHill), x + 1.0f, surface + 4.0f, palette.HillNear);
            }

            constexpr int kTreeSpacing = 18;
            for (int tree = 0; tree * kTreeSpacing < columns; ++tree)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(tree) * 977u + 11u);
                if ((mixed >> 24) % 4u == 0u)
                    continue;
                const auto x = static_cast<float>(tree * kTreeSpacing + static_cast<int>(mixed % static_cast<std::uint32_t>(kTreeSpacing)));
                const int height = 12 + static_cast<int>((mixed >> 8) % 10u);
                const float base = surface + 3.0f;
                ImVec4 crown = palette.Tree;
                if (season == SceneSeason::Autumn)
                    crown = Lit(kLeaves[(mixed >> 16) % std::size(kLeaves)], daylight);
                else if (season == SceneSeason::Spring && (mixed >> 16) % 3u == 0u)
                    crown = Lit(kBlossom, daylight);
                canvas.Fill(x - 1.0f, base - 3.0f, x + 1.0f, base + 1.0f, season == SceneSeason::Autumn ? Lit(kBark, daylight) : palette.Tree);
                const int tier = std::max(1, height / 3);
                for (int row = 0; row < height; ++row)
                {
                    const auto half = static_cast<float>(1 + row / tier + row % tier);
                    const float y = base - 2.0f - static_cast<float>(height - row);
                    const bool snowy = season == SceneSeason::Winter && row % tier <= 1;
                    canvas.Fill(x - half, y, x + half, y + 1.0f, snowy ? Lit(kSnow, daylight) : crown);
                }
            }
        }

        void DrawFalling(int columns, float shore, int width, int spacing, double speed, float sway, std::uint32_t salt, const std::function<void(float, float, std::uint32_t, double)>& draw)
        {
            const double time = IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
            const auto gap = static_cast<double>(spacing);
            for (int column = 0; column * width < columns; ++column)
            {
                const std::uint32_t mixed = Noise((static_cast<std::uint32_t>(column) * 40503u) ^ (salt * 83492791u));
                const double fallen = static_cast<double>(mixed & 1023u) + time * speed * (0.7 + static_cast<double>((mixed >> 10) & 7u) * 0.1);
                const auto first = static_cast<long long>(std::floor(-fallen / gap));
                const auto last = static_cast<long long>(std::floor((static_cast<double>(shore) - fallen) / gap));
                for (long long index = first; index <= last; ++index)
                {
                    const std::uint32_t own = Noise(mixed ^ (static_cast<std::uint32_t>(index) * 2654435761u));
                    const auto phase = static_cast<double>(own & 255u);
                    const auto y = static_cast<float>(std::floor(fallen + static_cast<double>(index) * gap));
                    const float drift = std::round(static_cast<float>(std::sin(time * 0.8 + phase)) * sway);
                    const auto x = static_cast<float>(column * width + static_cast<int>((own >> 8) % static_cast<std::uint32_t>(width))) + drift;
                    if (y >= 0.0f && y < shore - 1.0f)
                        draw(x, y, own, time);
                }
            }
        }

        void DrawWeather(const Canvas& canvas, const TerrariumSize& cells, int columns, SceneSeason season, float daylight)
        {
            const float surface = std::floor(std::max(0.0f, cells.Height - cells.GroundDepth - 3.0f));
            const float shore = std::floor(cells.Height - cells.LakeDepth);
            switch (season)
            {
            case SceneSeason::Winter:
                IdleAnimation::RequestFrames();
                DrawFalling(columns, shore, 7, 30, 5.0, 2.0f, 91u, [&](float x, float y, std::uint32_t own, double)
                    {
                        const float size = own >> 28 == 0u ? 2.0f : 1.0f;
                        canvas.Fill(x, y, x + size, y + size, ManifoldTheme::WithAlpha(Lit(kSnow, daylight), 0.9f));
                    });
                break;
            case SceneSeason::Autumn:
                IdleAnimation::RequestFrames();
                DrawFalling(columns, shore, 13, 42, 4.0, 5.0f, 37u, [&](float x, float y, std::uint32_t own, double time)
                    {
                        const bool flat = std::fmod(time * 1.4 + static_cast<double>(own & 255u), 2.0) < 1.0;
                        canvas.Fill(x, y, x + (flat ? 2.0f : 1.0f), y + (flat ? 1.0f : 2.0f), Lit(kLeaves[(own >> 20) % std::size(kLeaves)], daylight));
                    });
                break;
            case SceneSeason::Spring:
                IdleAnimation::RequestFrames();
                DrawFalling(columns, shore, 17, 54, 2.6, 4.0f, 53u, [&](float x, float y, std::uint32_t, double)
                    {
                        canvas.Fill(x, y, x + 1.0f, y + 1.0f, Lit(kBlossom, daylight));
                    });
                break;
            default:
                if (daylight < 0.5f)
                {
                    IdleAnimation::RequestFrames();
                    const double time = IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
                    const float top = std::max(0.0f, surface - 14.0f);
                    Scatter(columns, static_cast<int>(shore - top) - 4, 15, 15, 67u, 150u, [&](float x, float y, std::uint32_t mixed)
                        {
                            const auto phase = static_cast<double>(mixed & 255u);
                            if (std::sin(time * 1.3 + phase) < 0.1)
                                return;
                            const float wander = std::round(static_cast<float>(std::sin(time * 0.5 + phase)) * 2.0f);
                            const float rise = std::round(static_cast<float>(std::cos(time * 0.4 + phase)) * 2.0f);
                            canvas.Fill(x + wander, top + y + rise, x + wander + 1.0f, top + y + rise + 1.0f, ManifoldTheme::WithAlpha(kFirefly, 1.0f - daylight * 2.0f));
                        });
                }
                break;
            }
        }

        void DrawScene(const Canvas& canvas, ImVec2 max, const TerrariumSize& cells, bool home, float daylight, SceneSeason season)
        {
            const float pixel = canvas.Pixel;
            ImDrawList* drawList = canvas.List;
            const Palette palette = PaletteAt(daylight, season);
            drawList->AddRectFilled(canvas.Min, max, ImGui::GetColorU32(palette.Sky), Px(5.0f));

            const int columns = static_cast<int>(std::ceil((max.x - canvas.Min.x) / pixel));
            const int rows = static_cast<int>(std::ceil((max.y - canvas.Min.y) / pixel));
            const float surface = std::floor(std::max(0.0f, cells.Height - cells.GroundDepth - 3.0f));
            Scatter(columns - 4, static_cast<int>(surface) - 16, 10, 10, 31u, 128u, [&](float x, float y, std::uint32_t)
                {
                    canvas.Fill(x + 2.0f, y + 2.0f, x + 3.0f, y + 3.0f, palette.Star);
                });
            DrawSkyBodies(canvas, palette, columns, surface, daylight);
            if (home)
                DrawDistance(canvas, palette, columns, surface, season, daylight);

            const auto bottom = static_cast<float>((max.y - canvas.Min.y) / pixel);
            const auto insetOf = [&](int column)
                {
                    const int fromEdge = std::min(column, columns - 1 - column);
                    return fromEdge < 2 ? static_cast<float>(2 - fromEdge) : 0.0f;
                };
            for (int column = 0; column < columns; ++column)
            {
                const auto wave = static_cast<float>(std::sin(static_cast<double>(column) * 0.17)) * 1.6f + static_cast<float>(std::sin(static_cast<double>(column) * 0.43 + 1.0)) * 0.8f;
                const float top = std::round(surface + wave);
                const auto x = static_cast<float>(column);
                canvas.Fill(x, top, x + 1.0f, bottom - insetOf(column), palette.Ground);
                canvas.Fill(x, top, x + 1.0f, top + 1.0f, palette.GroundEdge);

                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(column));
                if (mixed % (season == SceneSeason::Winter ? 19u : 7u) == 0u && insetOf(column) == 0.0f)
                    canvas.Fill(x, top - static_cast<float>(1u + (mixed >> 8) % 2u), x + 1.0f, top, palette.Grass);
            }

            const float shore = std::floor(cells.Height - cells.LakeDepth);
            const int land = static_cast<int>(shore - surface) - 6;
            const bool autumn = season == SceneSeason::Autumn;
            const bool spring = season == SceneSeason::Spring;
            const ImVec4 distance = Mix(palette.HillFar, palette.Sky, 0.5f);
            if (home)
            {
                constexpr int kBands = 6;
                const float hazed = static_cast<float>(land) * 0.6f;
                for (int band = 0; band < kBands; ++band)
                {
                    const float from = std::round(surface + 2.0f + hazed * static_cast<float>(band) / kBands);
                    const float to = std::round(surface + 2.0f + hazed * static_cast<float>(band + 1) / kBands);
                    canvas.Fill(0.0f, from, static_cast<float>(columns), to, ManifoldTheme::WithAlpha(distance, 0.32f * (1.0f - static_cast<float>(band) / kBands)));
                }
            }
            Scatter(columns - 8, land, 6, 6, 7u, autumn ? 190u : 128u, [&](float x, float y, std::uint32_t mixed)
                {
                    const float share = land > 0 ? y / static_cast<float>(land) : 1.0f;
                    const float width = !home ? 2.0f : share < 0.3f ? 1.0f : share < 0.7f ? 2.0f : 3.0f;
                    ImVec4 fleck = autumn ? Lit(kLeaves[(mixed >> 20) % std::size(kLeaves)], daylight) : palette.GroundEdge;
                    if (home)
                        fleck = Mix(fleck, distance, 0.4f * std::max(0.0f, 1.0f - share / 0.6f));
                    canvas.Fill(x + 3.0f, surface + 4.0f + y, x + 3.0f + width, surface + 5.0f + y, fleck);
                });
            if (spring || (home && season == SceneSeason::Summer))
            {
                Scatter(columns - 8, land, spring ? 12 : 20, spring ? 12 : 20, 3u, spring ? 220u : 256u, [&](float x, float y, std::uint32_t mixed)
                    {
                        const ImVec4 flower = (mixed >> 20) % 3u == 0u ? kFlowerOne : (mixed >> 20) % 3u == 1u ? kFlowerTwo : spring ? kFlowerThree : kFlowerOne;
                        canvas.Fill(x + 3.0f, surface + 5.0f + y, x + 4.0f, surface + 6.0f + y, palette.Grass);
                        canvas.Fill(x + 3.0f, surface + 4.0f + y, x + 4.0f, surface + 5.0f + y, Lit(flower, spring ? daylight : 1.0f));
                    });
            }

            const float deepFrom = shore + std::floor((bottom - shore) * 0.45f);
            for (int column = 0; column < columns; ++column)
            {
                const auto at = static_cast<double>(column);
                const auto wave = std::abs(static_cast<float>(std::sin(at * 0.11 + 2.0))) * 1.5f;
                const float top = std::round(shore + wave);
                const float deep = deepFrom + std::round(static_cast<float>(std::sin(at * 0.07 + 1.0)) * 1.5f);
                const auto x = static_cast<float>(column);
                canvas.Fill(x, top, x + 1.0f, deep, palette.Water);
                canvas.Fill(x, deep, x + 1.0f, bottom - insetOf(column), palette.WaterDeep);
                canvas.Fill(x, top, x + 1.0f, top + 1.0f, palette.WaterEdge);
            }
            const auto beat = IdleAnimation::Enabled() ? static_cast<std::uint32_t>(ImGui::GetTime() * 1.5) : 0u;
            Scatter(columns - 9, rows - static_cast<int>(shore) - 5, 12, 8, 29u, 128u, [&](float x, float y, std::uint32_t mixed)
                {
                    if (autumn && (mixed >> 20) % 4u == 0u)
                        canvas.Fill(x + 3.0f, shore + 3.0f + y, x + 5.0f, shore + 4.0f + y, Lit(kLeaves[(mixed >> 24) % std::size(kLeaves)], daylight));
                    else if (season == SceneSeason::Winter)
                        canvas.Fill(x + 3.0f, shore + 3.0f + y, x + 7.0f, shore + 4.0f + y, palette.Ripple);
                    else if (((mixed >> 16) + beat) % 3u != 0u)
                        canvas.Fill(x + 3.0f, shore + 3.0f + y, x + 6.0f, shore + 4.0f + y, palette.Ripple);
                });
        }

        std::vector<TerrariumGuest> GuestsOf(const IdlerModel& model)
        {
            std::vector<TerrariumGuest> guests;
            for (const QueueEntry& entry : model.Queue())
            {
                const bool running = model.StateOf(entry.AppId) == RunState::Running;
                if (!running && !entry.Resting)
                    continue;
                const CreatureId creature = IdleAnimation::CreatureOf(entry.AppId);
                guests.push_back({ entry.AppId, IdleAnimation::HabitOf(creature), !running, creature, BondLevel(model.CreatureBondHours(creature)), model.CreatureEvolutions(creature) });
            }
            return guests;
        }

        std::uint32_t HomeSeed(CreatureId id)
        {
            return static_cast<std::uint32_t>(id) + 1u;
        }

        CreatureId HomeCreature(std::uint32_t seed)
        {
            return static_cast<CreatureId>(seed - 1u);
        }

        std::vector<TerrariumGuest> GuestsAtHome(const IdlerModel& model)
        {
            g_work.fill({});
            for (const QueueEntry& entry : model.Queue())
            {
                if (model.StateOf(entry.AppId) != RunState::Running)
                    continue;
                HomeWork& work = g_work[static_cast<std::size_t>(IdleAnimation::CreatureOf(entry.AppId))];
                if (work.Games++ == 0)
                    work.Game = entry.AppId;
            }

            const CreatureSet found = model.FoundCreatures();
            std::vector<TerrariumGuest> guests;
            std::vector<CreatureId> sleepers;
            const auto busy = [](CreatureId id)
                {
                    const std::uint32_t seed = HomeSeed(id);
                    const auto resident = std::find_if(g_home.Residents().begin(), g_home.Residents().end(), [&](const TerrariumResident& each) { return each.Seed == seed; });
                    if (resident == g_home.Residents().end() || resident->State == ResidentState::Leaving)
                        return false;
                    return resident->State == ResidentState::Held || resident->State == ResidentState::Settling || resident->State == ResidentState::Gone || resident->Bare ||
                        resident->Fetch != FetchStep::None || g_home.Ball().Fetcher == seed;
                };
            const auto add = [&](const CreatureInfo& info, bool asleep)
                {
                    guests.push_back({ HomeSeed(info.Id), IdleAnimation::HabitOf(info.Id), asleep, info.Id, BondLevel(model.CreatureBondHours(info.Id)), model.CreatureEvolutions(info.Id) });
                };
            for (const CreatureInfo& info : AllCreatures())
            {
                if (!found.test(static_cast<std::size_t>(info.Id)))
                    continue;
                const bool asleep = g_work[static_cast<std::size_t>(info.Id)].Games == 0;
                if (!asleep || busy(info.Id))
                    add(info, asleep);
                else
                    sleepers.push_back(info.Id);
            }
            if (!sleepers.empty())
            {
                const auto turn = static_cast<std::size_t>(ImGui::GetTime() / kSleeperTurnSeconds);
                for (std::size_t index = 0; index < std::min(kHomeSleepers, sleepers.size()); ++index)
                    add(InfoOf(sleepers[(turn + index) % sleepers.size()]), true);
            }
            return guests;
        }

        void DrawSplash(ImDrawList* drawList, ImVec2 center, float pixel, float age)
        {
            constexpr float kSeconds = 0.33f;
            if (age < 0.0f || age >= kSeconds)
                return;
            const float share = age / kSeconds;
            const ImU32 color = ImGui::GetColorU32(ImVec4(0.75f, 0.90f, 0.98f, 1.0f - share));
            for (int drop = -2; drop <= 2; ++drop)
            {
                const float side = static_cast<float>(drop);
                const float x = std::round(center.x + side * pixel * (1.0f + 5.0f * share));
                const float rise = pixel * (6.0f - std::abs(side)) * share * (1.0f - share) * 4.0f;
                const float y = std::round(center.y - rise);
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + pixel, y + pixel), color);
            }
        }

        constexpr int kMessBits = 44;
        constexpr int kMessSplats = 72;
        constexpr float kBurstFlightSeconds = 0.75f;
        constexpr float kMessFadeSeconds = 5.0f;

        struct MessBit
        {
            float X;
            float Y;
            float Lands;
            float Height;
            int Kind;
        };

        MessBit BitOf(const TerrariumMess& mess, int index, float back)
        {
            const std::uint32_t mixed = Noise(mess.Seed + static_cast<std::uint32_t>(index) * 7919u);
            const float angle = static_cast<float>(mixed % 6283u) / 1000.0f;
            const float reach = std::sqrt(static_cast<float>((mixed >> 13) % 1000u) / 1000.0f);
            MessBit bit;
            bit.X = mess.X + std::cos(angle) * reach * mess.Radius;
            bit.Y = std::max(back, mess.Y + std::sin(angle) * reach * mess.Radius * kMessDepthShare);
            bit.Lands = 0.18f + reach * (kBurstFlightSeconds - 0.18f);
            bit.Height = 6.0f + static_cast<float>((mixed >> 23) % 9u);
            const std::uint32_t kind = (mixed >> 3) % 10u;
            bit.Kind = kind < 3 ? 0 : kind < 6 ? 1 : kind < 8 ? 2 : kind < 9 ? 3 : 4;
            return bit;
        }

        ImVec4 MessColor(int kind, float daylight)
        {
            static const ImVec4 kColors[] = { Rgb(0.42f, 0.03f, 0.06f), Rgb(0.68f, 0.07f, 0.09f), Rgb(0.86f, 0.17f, 0.16f), Rgb(0.94f, 0.54f, 0.14f), Rgb(0.61f, 0.42f, 0.24f) };
            return Lit(kColors[std::clamp(kind, 0, 4)], daylight);
        }

        void DrawMessGround(const Canvas& canvas, const TerrariumMess& mess, double clock, float daylight, float back, float water)
        {
            const auto age = static_cast<float>(clock - mess.Began);
            const float fade = std::clamp(static_cast<float>(mess.Ends - clock) / kMessFadeSeconds, 0.0f, 1.0f);
            if (fade <= 0.0f)
                return;
            const auto faded = [&](ImVec4 color) { return ManifoldTheme::WithAlpha(color, color.w * fade); };
            const float spread = std::min(7.0f, age * 0.6f);
            const float sink = std::min(4.0f, age * 0.18f);
            const float thin = 0.55f * fade * std::max(0.3f, 1.0f - age / 20.0f);
            const auto fill = [&](float left, float y, float right, ImVec4 color)
                {
                    if (y < back)
                        return;
                    if (y < water)
                    {
                        canvas.Fill(left, y, right, y + 1.0f, faded(color));
                        return;
                    }
                    canvas.Fill(std::round(left - spread), std::round(y + sink), std::round(right + spread), std::round(y + sink) + 1.0f, ManifoldTheme::WithAlpha(color, thin));
                };

            const float grow = std::min(1.0f, 0.25f + age / 3.0f);
            const float across = std::round(mess.Radius * 0.5f * grow);
            const float deep = std::max(1.0f, std::round(across * kMessDepthShare));
            for (float row = -deep; row <= deep; row += 1.0f)
            {
                const float share = row / (deep + 0.5f);
                const float half = std::round(across * std::sqrt(std::max(0.0f, 1.0f - share * share)));
                const std::uint32_t ragged = Noise(mess.Seed + static_cast<std::uint32_t>(row + 64.0f) * 31u);
                const float left = mess.X - half - static_cast<float>(ragged % 3u);
                const float right = mess.X + half + static_cast<float>((ragged >> 5) % 3u);
                fill(std::round(left), std::round(mess.Y + row), std::round(right), MessColor(row < 0.0f ? 0 : 1, daylight));
            }
            for (int index = 0; index < kMessSplats; ++index)
            {
                const MessBit bit = BitOf(mess, index, back);
                if (age < bit.Lands)
                    continue;
                const std::uint32_t mixed = Noise(mess.Seed + static_cast<std::uint32_t>(index) * 104729u);
                const float size = 1.0f + static_cast<float>(mixed % 3u);
                const float x = std::round(bit.X - size * 0.5f);
                const float y = std::round(bit.Y);
                fill(x, y, x + size, MessColor(bit.Kind, daylight));
                if (size >= 2.0f && bit.Kind < 3)
                {
                    const float away = bit.X >= mess.X ? 1.0f : -1.0f;
                    fill(x + (away > 0.0f ? size : -1.0f), y, x + (away > 0.0f ? size + 1.0f : 0.0f), MessColor(0, daylight));
                }
            }
        }

        void DrawMessAir(const Canvas& canvas, const TerrariumMess& mess, double clock, float daylight, float spriteHeight, float back)
        {
            const auto age = static_cast<float>(clock - mess.Began);
            if (age >= kBurstFlightSeconds)
                return;
            const float middleY = mess.Y - spriteHeight * 0.5f;
            if (age < 0.14f)
            {
                const float size = std::round(3.0f + age * 40.0f);
                canvas.Fill(mess.X - size, middleY - 1.0f, mess.X + size, middleY + 1.0f, Rgb(1.0f, 0.95f, 0.75f));
                canvas.Fill(mess.X - 1.0f, middleY - size, mess.X + 1.0f, middleY + size, Rgb(1.0f, 0.95f, 0.75f));
                canvas.Fill(mess.X - size * 0.5f, middleY - size * 0.5f, mess.X + size * 0.5f, middleY + size * 0.5f, ManifoldTheme::WithAlpha(Rgb(1.0f, 0.55f, 0.20f), 0.85f));
            }
            for (int index = 0; index < kMessBits; ++index)
            {
                const MessBit bit = BitOf(mess, index, back);
                if (age >= bit.Lands)
                    continue;
                const float share = age / bit.Lands;
                const float x = mess.X + (bit.X - mess.X) * share;
                const float y = middleY + (bit.Y - middleY) * share - 4.0f * bit.Height * share * (1.0f - share);
                const float size = bit.Kind >= 3 ? 2.0f : 1.0f + static_cast<float>(index % 2);
                canvas.Fill(std::round(x), std::round(y), std::round(x) + size, std::round(y) + size, MessColor(bit.Kind, daylight));
            }
        }

        HomeScene::Frame HomeFrame(ImDrawList* drawList, ImVec2 min, float pixel, ImVec2 size, const TerrariumSize& cells, float daylight, SceneSeason season)
        {
            HomeScene::Frame frame;
            frame.Paint = { drawList, min, pixel, 0.0f, ImVec4() };
            frame.Columns = static_cast<int>(std::ceil(size.x / pixel));
            frame.Rows = static_cast<int>(std::ceil(size.y / pixel));
            frame.Surface = std::floor(std::max(0.0f, cells.Height - cells.GroundDepth - 3.0f));
            frame.Shore = std::floor(cells.Height - cells.LakeDepth);
            frame.Daylight = daylight;
            frame.Time = IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
            const Palette palette = PaletteAt(daylight, season);
            frame.HazeColor = Mix(palette.HillFar, palette.Sky, 0.5f);
            frame.FeetBack = cells.Height - cells.GroundDepth;
            frame.FeetFront = cells.Height - cells.LakeDepth - 1.0f;
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
            return frame;
        }

        void FollowTug(Terrarium& box, const HomeScene::Frame& frame, const TerrariumSize& cells, ImVec2 min, float pixel, void* window, bool mayTug)
        {
            const TerrariumTug tug = box.Tug();
            const auto resident = std::find_if(box.Residents().begin(), box.Residents().end(), [&](const TerrariumResident& each) { return each.Seed == tug.Seed; });
            if (!tug.Active || resident == box.Residents().end())
            {
                g_tugPlaced = false;
                return;
            }
            if (!mayTug)
            {
                box.LetGoOfHand(ImGui::IsAnyMouseDown());
                g_tugPlaced = false;
                return;
            }
            float x = 0.0f;
            float y = 0.0f;
            if (g_tugPlaced && Desktop::PointerIn(window, x, y) && (std::fabs(x - g_tugPlacedAt.x) > kTugSlack || std::fabs(y - g_tugPlacedAt.y) > kTugSlack))
            {
                box.LetGoOfHand(true);
                g_tugPlaced = false;
                return;
            }
            const CreatureId id = HomeCreature(resident->Seed);
            const int facing = IdleAnimation::FacingOf(id);
            const bool mirrored = facing != 0 && resident->Direction != facing;
            const float scaled = pixel * frame.ScaleAt(resident->Y + cells.SpriteHeight);
            const bool walking = resident->Habit == Habit::Walks && resident->State == ResidentState::Moving;
            const float bounce = walking && std::fmod(ImGui::GetTime() * 5.0 + IdleAnimation::Moment(resident->Seed), 1.0) < 0.5 ? scaled : 0.0f;
            const ImVec2 picture = IdleAnimation::SizeAt(scaled);
            const ImVec2 feet(min.x + pixel * (std::round(resident->X + resident->ShiftX) + cells.SpriteWidth * 0.5f),
                min.y + pixel * (std::round(resident->Y) + cells.SpriteHeight) - bounce - scaled * std::round(resident->Lift));
            const ImVec2 at(std::round(feet.x - picture.x * 0.5f), std::round(feet.y - picture.y));
            const ImVec2 motion = IdleAnimation::MotionOf(id, resident->Seed);
            const float side = resident->Direction > 0 ? cells.SpriteWidth - 2.0f : 2.0f;
            const ImVec2 grip(std::round(at.x + (side + (mirrored ? -motion.x : motion.x)) * scaled), std::round(at.y + (cells.SpriteHeight * 0.55f - motion.y) * scaled));
            Desktop::MovePointer(window, grip.x, grip.y);
            g_tugPlacedAt = grip;
            g_tugPlaced = true;
            IdleAnimation::KeepFrames();
        }

        bool OnThing(const TerrariumThing& thing, ImVec2 hand)
        {
            const float reach = thing.Radius + 2.5f;
            return thing.State != BallState::None && (hand.x - thing.X) * (hand.x - thing.X) + (hand.y - thing.Y) * (hand.y - thing.Y) <= reach * reach;
        }


        void DrawGroundShadow(ImDrawList* drawList, ImVec2 min, float pixel, float left, float right, float ground, float height)
        {
            const float shrink = std::min((right - left) * 0.3f, height * 0.15f);
            left += shrink;
            right -= shrink;
            const int alpha = static_cast<int>(std::max(20.0f, kShadowAlpha - height * 2.0f));
            const auto at = [&](float x, float y) { return ImVec2(std::round(min.x + x * pixel), std::round(min.y + y * pixel)); };
            drawList->AddRectFilled(at(left, ground - 0.5f), at(right, ground + 0.5f), IM_COL32(0, 0, 0, alpha));
            drawList->AddRectFilled(at(left - 1.0f, ground - 0.5f), at(left, ground + 0.5f), IM_COL32(0, 0, 0, alpha / 2));
            drawList->AddRectFilled(at(right, ground - 0.5f), at(right + 1.0f, ground + 0.5f), IM_COL32(0, 0, 0, alpha / 2));
        }

        void DrawBallAt(ImDrawList* drawList, ImVec2 center, float pixel, float turned)
        {
            static constexpr const char* kTurns[3][5] = {
                { ".aaa.", "abbaa", "aabba", "aaabb", ".aaa." },
                { ".aaa.", "aabba", "aaabb", "baaab", ".aaa." },
                { ".bba.", "aabba", "aaabb", "aaaab", ".aaa." },
            };
            const int turn = ((static_cast<int>(std::floor(turned / 2.0f)) % 3) + 3) % 3;
            const ImVec4 colors[2] = { ImVec4(0.88f, 0.26f, 0.26f, 1.0f), ImVec4(0.97f, 0.94f, 0.88f, 1.0f) };
            const ImVec2 corner(std::round(center.x - kBallRadius * pixel), std::round(center.y - kBallRadius * pixel));
            for (int row = 0; row < 5; ++row)
            {
                for (int column = 0; column < 5; ++column)
                {
                    const char cell = kTurns[turn][row][column];
                    if (cell == '.')
                        continue;
                    const ImVec2 at(corner.x + static_cast<float>(column) * pixel, corner.y + static_cast<float>(row) * pixel);
                    drawList->AddRectFilled(at, ImVec2(at.x + pixel, at.y + pixel), ImGui::GetColorU32(colors[cell == 'b' ? 1 : 0]));
                }
            }
        }

        void DrawLooseShell(ImDrawList* drawList, ImVec2 center, float pixel, const TerrariumShell& shell)
        {
            if (shell.State == BallState::None)
                return;
            const bool turned = (static_cast<int>(std::floor(shell.Turn / 6.0f)) & 1) != 0;
            IdleAnimation::DrawShell(drawList, center, CreatureId::Snail, pixel, turned);
        }

        constexpr float kBackScale = 0.72f;
        constexpr float kFrontScale = 1.08f;
        constexpr float kFlyingScale = 0.9f;
        constexpr float kBackHaze = 0.38f;

        struct Depth
        {
            float Scale = 1.0f;
            float Haze = 0.0f;
        };

        void DrawFrontEdge(const Canvas& canvas, const Palette& palette, int columns, float shore, SceneSeason season, float daylight)
        {
            const double time = IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
            const bool autumn = season == SceneSeason::Autumn;
            const ImVec4 blade = autumn ? Lit(Rgb(0.560f, 0.470f, 0.230f), daylight) : palette.Grass;
            const ImVec4 darkBlade = autumn ? Lit(Rgb(0.380f, 0.300f, 0.140f), daylight) : Mix(palette.Grass, palette.Ground, 0.45f);
            const ImVec4 stone = Lit(Rgb(0.46f, 0.45f, 0.44f), daylight);
            const ImVec4 stoneTop = Lit(Rgb(0.62f, 0.61f, 0.59f), daylight);
            for (int column = 2; column < columns - 2; ++column)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(column) * 131u + 17u);
                if (mixed % 6u != 0u)
                    continue;
                const auto x = static_cast<float>(column);
                const std::uint32_t kind = (mixed >> 12) % 12u;
                if (kind == 11u)
                {
                    canvas.Fill(x, shore - 2.0f, x + 3.0f, shore + 1.0f, stone);
                    canvas.Fill(x + 1.0f, shore - 3.0f, x + 3.0f, shore - 2.0f, stone);
                    canvas.Fill(x + 1.0f, shore - 3.0f, x + 2.0f, shore - 2.0f, stoneTop);
                    continue;
                }
                if (kind >= 9u && autumn)
                {
                    canvas.Fill(x, shore - 1.0f, x + 2.0f, shore, Lit(kLeaves[(mixed >> 20) % std::size(kLeaves)], daylight));
                    continue;
                }
                const int height = 3 + static_cast<int>((mixed >> 8) % 4u);
                const double phase = static_cast<double>((mixed >> 16) & 255u) * 0.05;
                for (int bladeIndex = 0; bladeIndex < 1 + static_cast<int>((mixed >> 4) % 3u); ++bladeIndex)
                {
                    const float bx = x + static_cast<float>(bladeIndex);
                    const int tall = height - bladeIndex;
                    for (int row = 0; row < tall; ++row)
                    {
                        const float sway = row * 2 >= tall ? std::round(static_cast<float>(std::sin(time * 1.4 + phase + bladeIndex)) * 0.8f) : 0.0f;
                        canvas.Fill(bx + sway, shore - static_cast<float>(row) - 1.0f, bx + sway + 1.0f, shore - static_cast<float>(row), bladeIndex == 1 ? darkBlade : blade);
                    }
                }
            }
        }

        void DrawBox(IdlerModel& model, Terrarium& box, std::unordered_map<std::uint32_t, double>& pettedAt, const std::vector<TerrariumGuest>& guests,
            ImVec2 size, float pixel, const TerrariumSize& cells, bool home)
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);
            ImGui::SetNextItemAllowOverlap();
            const bool clicked = ImGui::InvisibleButton("##terrarium", size);
            const bool hovered = ImGui::IsItemHovered();
            const bool pressed = ImGui::IsItemActivated();
            const bool holding = ImGui::IsItemActive();
            const ImVec2 mouse = ImGui::GetMousePos();
            const double elapsed = std::min(static_cast<double>(ImGui::GetIO().DeltaTime) / model.ClockPace(), 0.1);

            const ImVec2 hand((mouse.x - min.x) / pixel, (mouse.y - min.y) / pixel);
            bool tookBall = false;
            if (home && pressed && box.GrabBall(hand.x, hand.y))
            {
                g_ballInHand = true;
                tookBall = true;
            }
            else if (home && pressed && box.GrabShell(hand.x, hand.y))
            {
                g_shellInHand = true;
                tookBall = true;
            }
            if (home && g_shellInHand)
            {
                const float radius = box.Shell().Radius;
                if (holding)
                {
                    box.CarryShell(std::clamp(hand.x, radius, std::max(radius, cells.Width - radius)), std::clamp(hand.y, radius, std::max(radius, cells.Height - radius)), elapsed);
                }
                else
                {
                    box.DropShell(cells);
                    g_shellInHand = false;
                }
            }
            if (home && g_ballInHand)
            {
                if (holding)
                {
                    box.CarryBall(std::clamp(hand.x, kBallRadius, std::max(kBallRadius, cells.Width - kBallRadius)), std::clamp(hand.y, kBallRadius, std::max(kBallRadius, cells.Height - kBallRadius)), elapsed);
                }
                else
                {
                    box.DropBall(cells);
                    g_ballInHand = false;
                }
            }

            bool dropped = false;
            if (home && g_grabbed != 0)
            {
                if (holding && !g_carrying && ImGui::IsMouseDragging(ImGuiMouseButton_Left, Px(3.0f)))
                {
                    box.Grab(g_grabbed);
                    g_carrying = true;
                }
                if (holding && g_carrying)
                {
                    box.Carry(g_grabbed, std::clamp((mouse.x - min.x) / pixel - g_grabbedAt.x, 0.0f, std::max(0.0f, cells.Width - cells.SpriteWidth)),
                        std::clamp((mouse.y - min.y) / pixel - g_grabbedAt.y, 0.0f, std::max(0.0f, cells.Height - cells.SpriteHeight)), elapsed);
                }
                if (!holding)
                {
                    if (g_carrying)
                        box.Drop(g_grabbed, cells);
                    dropped = g_carrying;
                    g_grabbed = 0;
                    g_carrying = false;
                }
            }

            const double now = ImGui::GetTime();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float daylight = UpdateDaylight(model);
            const SceneSeason season = model.CurrentSeason();
            HomeScene::Frame frame;
            if (home)
            {
                frame = HomeFrame(drawList, min, pixel, size, cells, daylight, season);
                frame.HazeColor = HomeScene::SceneOf(season).HazeColor(frame);
                const PropsKey key{ frame.Columns, frame.Rows, frame.Surface, frame.Shore, season };
                if (!(key == g_propsKey))
                {
                    g_propsKey = key;
                    g_props.clear();
                    HomeScene::SceneOf(season).PlaceProps(frame, g_props);
                    std::stable_sort(g_props.begin(), g_props.end(), [](const HomeScene::Prop& lhs, const HomeScene::Prop& rhs) { return lhs.Ground < rhs.Ground; });
                }
                std::vector<TerrariumCover> covers;
                for (const HomeScene::Prop& prop : g_props)
                {
                    const float scale = frame.ScaleAt(prop.Ground);
                    covers.push_back({ prop.X, prop.Ground, prop.Foot * scale, prop.Cover * scale });
                }
                box.SetCovers(std::move(covers));
            }
            box.SetPointer(hovered, (mouse.x - min.x) / pixel - cells.SpriteWidth * 0.5f, (mouse.y - min.y) / pixel - cells.SpriteHeight * 0.5f);
            void* const window = ImGui::GetMainViewport()->PlatformHandleRaw;
            const bool mayTug = home && model.Options().CreaturesTugMouse && hovered && !ImGui::IsAnyMouseDown() && !g_carrying && !g_ballInHand && !g_shellInHand &&
                IdleAnimation::Enabled() && Desktop::CanMovePointer() && Desktop::IsInFront(window);
            box.SetTugAllowed(mayTug);
            box.Update(guests, cells, elapsed, IdleAnimation::Enabled() && !(home && box.Residents().empty()));
            if (home)
                FollowTug(box, frame, cells, min, pixel, window, mayTug);
            for (const std::uint32_t seed : box.TakeFetches())
                model.FetchedBall(HomeCreature(seed));

            drawList->PushClipRect(min, max, true);
            if (home)
            {
                g_sceneCells = cells;
                g_sceneMax = max;
                HomeScene::SceneOf(season).DrawBack(frame);
            }
            else
            {
                DrawScene(Canvas{ drawList, min, pixel }, max, cells, home, daylight, season);
            }
            drawList->PopClipRect();

            std::vector<const TerrariumResident*> order;
            for (const TerrariumResident& resident : box.Residents())
                order.push_back(&resident);
            const auto layer = [](const TerrariumResident* resident) { return resident->State == ResidentState::Held ? 2 : resident->Habit == Habit::Floats && !resident->Asleep ? 1 : 0; };
            std::sort(order.begin(), order.end(), [&](const TerrariumResident* lhs, const TerrariumResident* rhs)
                {
                    return layer(lhs) != layer(rhs) ? layer(lhs) < layer(rhs) : lhs->Y < rhs->Y;
                });

            const TerrariumResident* underMouse = nullptr;
            const Palette palette = PaletteAt(daylight, season);
            const ImVec4 haze = home ? frame.HazeColor : Mix(palette.HillFar, palette.Sky, 0.5f);

            const float feetBack = cells.Height - cells.GroundDepth;
            const float feetFront = cells.Height - cells.LakeDepth - 1.0f;
            (void)feetBack;
            (void)feetFront;
            const auto depthAt = [&](float feet) { return home ? Depth{ frame.ScaleAt(feet), frame.HazeAt(feet) } : Depth{}; };
            const auto depthOf = [&](const TerrariumResident& resident)
                {
                    if (!home || resident.State == ResidentState::Held)
                        return Depth{};
                    if (resident.Habit == Habit::Swims)
                        return Depth{ kFrontScale, 0.0f };
                    const bool coming = resident.State == ResidentState::Settling || (resident.State == ResidentState::Entering && resident.Habit == Habit::Stays);
                    Depth depth = depthAt((coming ? resident.TargetY : resident.Y) + cells.SpriteHeight);
                    if (resident.Habit == Habit::Floats && !resident.Asleep)
                    {
                        depth.Scale = std::max(depth.Scale, kFlyingScale);
                        depth.Haze = 0.0f;
                    }
                    return depth;
                };

            drawList->PushClipRect(min, max, true);
            const Canvas ground{ drawList, min, pixel };
            const float landBack = std::floor(std::max(0.0f, cells.Height - cells.GroundDepth - 3.0f)) + 2.0f;
            const float water = std::floor(cells.Height - cells.LakeDepth) + 1.0f;
            for (const TerrariumMess& mess : box.Messes())
            {
                DrawMessGround(ground, mess, box.Clock(), daylight, landBack, water);
                if (box.Clock() - mess.Began < 1.5)
                    IdleAnimation::KeepFrames();
                else
                    IdleAnimation::RequestFrames();
            }
            if (home)
            {
                for (const HomeScene::Prop& prop : g_props)
                {
                    const float scale = frame.ScaleAt(prop.Ground);
                    HomeScene::DrawCastShadow(frame, prop.X, prop.Ground, prop.Foot * scale, prop.Height * scale, prop.Crown * scale);
                }
            }
            const float landTop = cells.Height - cells.SpriteHeight - cells.GroundDepth;
            const float landBottom = cells.Height - cells.SpriteHeight - cells.LakeDepth - 1.0f;
            for (const TerrariumResident* resident : order)
            {
                if (resident->Habit == Habit::Swims || resident->State == ResidentState::Held || resident->State == ResidentState::Settling || resident->State == ResidentState::Gone || resident->GroundLine >= 0.0f ||
                    resident->Y < landTop - 0.5f || resident->Y > landBottom + 0.5f)
                    continue;
                const CreatureId id = home ? HomeCreature(resident->Seed) : IdleAnimation::CreatureOf(resident->Seed);
                const int facing = IdleAnimation::FacingOf(id);
                const IdleAnimation::Footprint feet = IdleAnimation::FootprintOf(id, facing != 0 && resident->Direction != facing);
                const Depth depth = depthOf(*resident);
                const float half = cells.SpriteWidth * 0.5f;
                const float middle = std::round(resident->X + resident->ShiftX) + half;
                const float left = middle + (static_cast<float>(feet.Left) - half) * depth.Scale;
                const float right = middle + (static_cast<float>(feet.Right) + 1.0f - half) * depth.Scale;
                const float line = std::round(resident->Y) + cells.SpriteHeight + (static_cast<float>(feet.Ground) - cells.SpriteHeight) * depth.Scale;
                const float hop = std::round(resident->Lift) * depth.Scale;
                if (home)
                    HomeScene::DrawCastShadow(frame, (left + right) * 0.5f, line, (right - left) * 0.5f, cells.SpriteHeight * 0.7f * depth.Scale, (right - left) * 0.55f, std::max(0.3f, 1.0f - hop * 0.08f));
                else
                    DrawGroundShadow(drawList, min, pixel, left, right, line, hop);
            }
            const TerrariumBall& ball = box.Ball();
            const TerrariumShell& shell = box.Shell();
            const Depth ballDepth = depthAt(ball.Floor + ball.Radius);
            const Depth shellDepth = depthAt(shell.Floor + shell.Radius);
            if (home)
            {
                for (const TerrariumThing* thing : { static_cast<const TerrariumThing*>(&ball), static_cast<const TerrariumThing*>(&shell) })
                {
                    if (thing->State != BallState::Lying && thing->State != BallState::Flying)
                        continue;
                    const float scale = frame.ScaleAt(thing->Floor + thing->Radius);
                    const float up = std::max(0.0f, thing->Floor - thing->Y);
                    HomeScene::DrawCastShadow(frame, thing->X, thing->Floor + thing->Radius, thing->Radius * 0.8f * scale, thing->Radius * 1.6f * scale, thing->Radius * 0.9f * scale,
                        std::max(0.3f, 1.0f - up * 0.05f));
                }
                if (IdleAnimation::Enabled() && (shell.State != BallState::None || ball.State == BallState::Flying || ball.State == BallState::Carried || ball.Fetcher != 0))
                    IdleAnimation::KeepFrames();
            }
            if (home)
                HomeScene::SceneOf(season).DrawFront(frame);
            const auto thingScale = [](const TerrariumThing& thing, const Depth& depth) { return thing.State == BallState::Held ? 1.0f : depth.Scale; };
            const auto thingAt = [&](const TerrariumThing& thing, float scale)
                {
                    return ImVec2(min.x + thing.X * pixel, min.y + (thing.Y + thing.Radius) * pixel - thing.Radius * pixel * scale);
                };
            const auto drawBall = [&]()
                {
                    const float scale = thingScale(ball, ballDepth);
                    DrawBallAt(drawList, thingAt(ball, scale), pixel * scale, ball.Turn);
                };
            const auto drawShell = [&]()
                {
                    const float scale = thingScale(shell, shellDepth);
                    DrawLooseShell(drawList, thingAt(shell, scale), pixel * scale, shell);
                };

            bool ballDrawn = !home || ball.State == BallState::None;
            bool shellDrawn = !home || shell.State == BallState::None;
            const auto onTheLand = [](const TerrariumThing& thing) { return thing.State == BallState::Lying || thing.State == BallState::Flying; };
            std::size_t nextProp = 0;
            const auto drawProp = [&](const HomeScene::Prop& prop)
                {
                    const float scale = frame.ScaleAt(prop.Ground);
                    const HomeScene::Canvas paint{ drawList, ImVec2(std::round(min.x + prop.X * pixel), std::round(min.y + prop.Ground * pixel)), pixel * scale, frame.HazeAt(prop.Ground), frame.HazeColor };
                    HomeScene::SceneOf(season).DrawProp(frame, prop, paint);
                };
            const auto drawUpTo = [&](float limit)
                {
                    while (home)
                    {
                        float nearest = limit;
                        int which = -1;
                        if (nextProp < g_props.size() && g_props[nextProp].Ground <= nearest)
                        {
                            nearest = g_props[nextProp].Ground;
                            which = 0;
                        }
                        if (!ballDrawn && onTheLand(ball) && ball.Floor + ball.Radius <= nearest)
                        {
                            nearest = ball.Floor + ball.Radius;
                            which = 1;
                        }
                        if (!shellDrawn && onTheLand(shell) && shell.Floor + shell.Radius <= nearest)
                            which = 2;
                        if (which < 0)
                            return;
                        if (which == 0)
                            drawProp(g_props[nextProp++]);
                        else if (which == 1)
                        {
                            drawBall();
                            ballDrawn = true;
                        }
                        else
                        {
                            drawShell();
                            shellDrawn = true;
                        }
                    }
                };
            for (const TerrariumResident* resident : order)
            {
                if (resident->State == ResidentState::Gone)
                    continue;
                drawUpTo(layer(resident) > 0 ? std::numeric_limits<float>::max() : resident->Y + cells.SpriteHeight);
                const CreatureId id = home ? HomeCreature(resident->Seed) : IdleAnimation::CreatureOf(resident->Seed);
                const int facing = IdleAnimation::FacingOf(id);
                const bool mirrored = facing != 0 && resident->Direction != facing;
                const Depth depth = depthOf(*resident);
                const float scaled = pixel * depth.Scale;

                const bool walking = resident->Habit == Habit::Walks && resident->State != ResidentState::Resting && resident->State != ResidentState::Held && resident->State != ResidentState::Settling;
                const float bounce = walking && std::fmod(now * 5.0 + IdleAnimation::Moment(resident->Seed), 1.0) < 0.5 ? scaled : 0.0f;

                double crossLeft = 0.0;
                if (home)
                {
                    if ((resident->State == ResidentState::Held || resident->State == ResidentState::Settling) && ThrowMannerOf(id, resident->Bond) == ThrowManner::Hates)
                        g_crossUntil[resident->Seed] = now + kCrossSeconds;
                    if (const auto cross = g_crossUntil.find(resident->Seed); cross != g_crossUntil.end())
                        crossLeft = cross->second - now;
                }
                const float stamp = crossLeft > kCrossSeconds - kStampSeconds && resident->State == ResidentState::Resting && std::fmod(now * 12.0, 1.0) < 0.5 ? scaled : 0.0f;

                const float lift = scaled * std::round(resident->Lift);
                const ImVec2 picture = IdleAnimation::SizeAt(scaled);
                const ImVec2 feet(min.x + pixel * (std::round(resident->X + resident->ShiftX) + cells.SpriteWidth * 0.5f) + stamp,
                    min.y + pixel * (std::round(resident->Y) + cells.SpriteHeight) - bounce - lift);
                const ImVec2 at(std::round(feet.x - picture.x * 0.5f), std::round(feet.y - picture.y));
                const bool sleeping = resident->Asleep && !resident->Bare && resident->State == ResidentState::Resting && crossLeft <= 0.0 && resident->Reaction == TerrariumReaction::None;
                const IdleAnimation::Look look = !home && IsShiny(resident->Seed) ? IdleAnimation::Look::Shiny : IdleAnimation::Look::Normal;
                const float drawn = resident->Reaction == TerrariumReaction::Puffed ? scaled * 1.5f : scaled;
                const ImVec2 larger = IdleAnimation::SizeAt(drawn);
                const bool sinking = resident->GroundLine >= 0.0f;
                if (sinking)
                {
                    drawList->PushClipRect(min, ImVec2(max.x, std::min(max.y, min.y + pixel * std::round(resident->GroundLine))), true);
                    IdleAnimation::RequestFrames();
                }
                IdleAnimation::SetHaze(depth.Haze, haze);
                IdleAnimation::DrawCreature(drawList, ImVec2(at.x - (larger.x - picture.x) * 0.5f, at.y - (larger.y - picture.y)), id, sleeping ? IdleAnimation::Mood::Sleeping : IdleAnimation::Mood::Idling, resident->Seed, drawn, look, mirrored, resident->Squash,
                    resident->ShowsFirstForm ? IdleAnimation::Form::First : IdleAnimation::Form::Shown, resident->Bare ? IdleAnimation::Part::WithoutShell : IdleAnimation::Part::Whole);
                IdleAnimation::SetHaze(0.0f, haze);
                if (sinking)
                    drawList->PopClipRect();
                if (resident->Reaction == TerrariumReaction::Evolving)
                    IdleAnimation::RequestFrames();
                if (resident->Squash > 0.0f)
                    IdleAnimation::RequestFrames();
                if (crossLeft > 0.0)
                {
                    IdleAnimation::RequestFrames();
                    IdleAnimation::DrawAngerMark(drawList, ImVec2(at.x + picture.x * 0.92f, at.y + picture.y * 0.08f), scaled, std::fmod(now * 3.5, 1.0) < 0.5);
                }

                if (resident->Emote == TerrariumEmote::Heart)
                {
                    IdleAnimation::RequestFrames();
                    const float share = std::min(1.0f, resident->EmoteAge / 0.8f);
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x + picture.x * 0.5f - scaled * 1.5f, at.y - Px(2.0f) - Px(12.0f) * share), std::max(1.0f, std::round(scaled * 0.5f)), 1.0f - share);
                }
                else if (resident->Emote == TerrariumEmote::Alarm || resident->Emote == TerrariumEmote::Dots || resident->Emote == TerrariumEmote::Note || resident->Emote == TerrariumEmote::Question)
                {
                    IdleAnimation::RequestFrames();
                    const float markPixel = std::max(1.0f, std::round(scaled * 0.75f));
                    const float rise = resident->Emote == TerrariumEmote::Dots || std::fmod(resident->EmoteAge, 0.5f) >= 0.25f ? 0.0f : markPixel;
                    const IdleAnimation::Mark mark = resident->Emote == TerrariumEmote::Alarm ? IdleAnimation::Mark::Alarm : resident->Emote == TerrariumEmote::Dots ? IdleAnimation::Mark::Dots : resident->Emote == TerrariumEmote::Note ? IdleAnimation::Mark::Note : IdleAnimation::Mark::Question;
                    IdleAnimation::DrawMark(drawList, ImVec2(at.x + picture.x * 0.5f, at.y + scaled - rise), markPixel, mark);
                }
                else if (resident->Emote == TerrariumEmote::Stars)
                {
                    IdleAnimation::RequestFrames();
                    IdleAnimation::DrawDizzy(drawList, ImVec2(at.x + picture.x * 0.5f, at.y + scaled), std::max(1.0f, std::round(scaled * 0.5f)), resident->EmoteAge);
                }
                else if (resident->Emote == TerrariumEmote::Anger)
                {
                    IdleAnimation::RequestFrames();
                    IdleAnimation::DrawAngerMark(drawList, ImVec2(at.x + picture.x * 0.92f, at.y + picture.y * 0.08f), scaled, std::fmod(resident->EmoteAge, 0.3f) < 0.15f);
                }
                else if (resident->Emote == TerrariumEmote::Splash)
                {
                    IdleAnimation::RequestFrames();
                    DrawSplash(drawList, ImVec2(at.x + picture.x * 0.5f, min.y + pixel * (cells.Height - cells.LakeDepth)), pixel, resident->EmoteAge);
                }

                if (hovered && mouse.x >= at.x && mouse.x < at.x + picture.x && mouse.y >= at.y && mouse.y < at.y + picture.y)
                    underMouse = resident;
                if (!ballDrawn && ball.State == BallState::Carried && ball.Fetcher == resident->Seed)
                {
                    const ImVec2 motion = sleeping ? ImVec2(0.0f, 0.0f) : IdleAnimation::MotionOf(id, resident->Seed);
                    const float side = resident->Direction > 0 ? cells.SpriteWidth - 3.0f : 3.0f;
                    const ImVec2 mouth(at.x + (side + (mirrored ? -motion.x : motion.x)) * scaled, at.y + (cells.SpriteHeight * 0.6f - motion.y) * scaled);
                    DrawBallAt(drawList, mouth, scaled, ball.Turn);
                    ballDrawn = true;
                }
            }
            drawUpTo(std::numeric_limits<float>::max());
            if (!ballDrawn)
                drawBall();
            if (!shellDrawn)
                drawShell();
            const int columns = static_cast<int>(std::ceil(size.x / pixel));
            for (const TerrariumMess& mess : box.Messes())
                DrawMessAir(ground, mess, box.Clock(), daylight, cells.SpriteHeight, landBack);
            DrawWeather(Canvas{ drawList, min, pixel }, cells, columns, season, daylight);
            if (home)
                MaskCorners(drawList, min, max, Px(5.0f), ImGui::GetColorU32(ManifoldTheme::Page));
            drawList->PopClipRect();
            drawList->AddRect(min, max, ImGui::GetColorU32(ManifoldTheme::CardBorder), Px(5.0f));

            const bool tugging = home && box.Tug().Active;
            if (tugging)
                ForgetTip(home);
            if (home && (g_carrying || g_ballInHand || g_shellInHand))
            {
                ForgetTip(home);
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                return;
            }
            if (home && hovered && (OnThing(box.Ball(), hand) || OnThing(box.Shell(), hand)))
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (!tugging && TipDue(home, kThingTip, now))
                    ImGui::SetTooltip("Throw it");
                return;
            }
            if (!underMouse || tookBall)
            {
                ForgetTip(home);
                return;
            }
            const bool showTip = !tugging && TipDue(home, underMouse->Seed, now);

            if (home)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (pressed)
                {
                    g_grabbed = underMouse->Seed;
                    g_grabbedAt = ImVec2((mouse.x - min.x) / pixel - underMouse->X, (mouse.y - min.y) / pixel - underMouse->Y);
                }
                const CreatureInfo& info = InfoOf(HomeCreature(underMouse->Seed));
                const HomeWork& work = g_work[static_cast<std::size_t>(info.Id)];
                const char* bond = BondName(BondLevel(model.CreatureBondHours(info.Id)));
                if (showTip && work.Games > 1)
                {
                    ImGui::SetTooltip("%s, %s\n%s\nIdling %d games", info.Name, bond, info.Flavour, work.Games);
                }
                else if (showTip && work.Games == 1)
                {
                    const Game* game = model.FindGame(work.Game);
                    ImGui::SetTooltip("%s, %s\n%s\nIdling %s", info.Name, bond, info.Flavour, game ? game->Name.c_str() : "a game");
                }
                else if (showTip)
                {
                    ImGui::SetTooltip("%s, %s\n%s", info.Name, bond, info.Flavour);
                }
            }
            else
            {
                const Game* game = model.FindGame(underMouse->Seed);
                const std::string name = IdleAnimation::NameOf(underMouse->Seed);
                if (showTip)
                    ImGui::SetTooltip("%s\n%s", game ? game->Name.c_str() : "A game", name.c_str());
            }

            const auto petted = pettedAt.find(underMouse->Seed);
            if (clicked && !dropped && (petted == pettedAt.end() || now - petted->second >= IdleAnimation::kPetSeconds))
            {
                pettedAt[underMouse->Seed] = now;
                g_crossUntil.erase(underMouse->Seed);
                box.Pet(underMouse->Seed);
                model.PetCreature(home ? HomeCreature(underMouse->Seed) : IdleAnimation::CreatureOf(underMouse->Seed));
            }
        }
    }

    void DrawTerrarium(IdlerModel& model, ImVec2 size)
    {
        const float pixel = IdleAnimation::PixelSize();
        const ImVec2 sprite = IdleAnimation::SizeAt(1.0f);
        TerrariumSize cells{ size.x / pixel, size.y / pixel, sprite.x, sprite.y };
        cells.GroundDepth = std::round(cells.Height * 2.0f / 3.0f);
        cells.LakeDepth = std::max(kTerrariumLakeDepth, std::round(cells.Height / 3.0f));
        DrawBox(model, g_terrarium, g_pettedAt, GuestsOf(model), size, pixel, cells, false);
    }

    void SetHomeLightPreview(float daylight)
    {
        g_lightPreview = daylight;
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

        void DrawClassicBack(const Frame& frame, SceneSeason season)
        {
            DrawScene(UI::Canvas{ frame.Paint.List, frame.Paint.Min, frame.Paint.Pixel }, g_sceneMax, g_sceneCells, true, frame.Daylight, season);
        }

        void DrawClassicFront(const Frame& frame, SceneSeason season)
        {
            DrawFrontEdge(UI::Canvas{ frame.Paint.List, frame.Paint.Min, frame.Paint.Pixel }, PaletteAt(frame.Daylight, season), frame.Columns, frame.Shore, season, frame.Daylight);
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

    void DrawCreatureHome(IdlerModel& model, ImVec2 size)
    {
        const float pixel = std::clamp(std::round(size.y / 170.0f), IdleAnimation::PixelSize(), std::round(Px(6.0f)));
        const ImVec2 sprite = IdleAnimation::SizeAt(1.0f);
        TerrariumSize cells{ size.x / pixel, size.y / pixel, sprite.x, sprite.y };
        cells.GroundDepth = std::round(cells.Height * 0.42f);
        cells.LakeDepth = std::max(kTerrariumLakeDepth, std::round(cells.Height * 0.14f));
        if (IdleAnimation::Enabled())
            IdleAnimation::RequestFrames();
        g_home.SetBall(model.Options().HomeBall, cells);
        DrawBox(model, g_home, g_pettedAtHome, GuestsAtHome(model), size, pixel, cells, true);
    }

    namespace
    {
        struct StageLook
        {
            float Daylight = 1.0f;
            SceneSeason Season = SceneSeason::Summer;
            float Pixel = 2.0f;
            int Columns = 0;
            int Rows = 0;
            float Ground = 0.0f;
            bool Game = false;
        };
        StageLook g_stage;

        double StageTime()
        {
            return IdleAnimation::Enabled() ? ImGui::GetTime() : 0.0;
        }

        StageScene::Frame StageFrame(ImDrawList* drawList, ImVec2 min)
        {
            StageScene::Frame frame;
            frame.Paint = HomeScene::Canvas{ .List = drawList, .Min = min, .Pixel = g_stage.Pixel };
            frame.Columns = g_stage.Columns;
            frame.Rows = g_stage.Rows;
            frame.Ground = g_stage.Ground;
            frame.Daylight = g_stage.Daylight;
            frame.Season = g_stage.Season;
            frame.Time = StageTime();
            frame.Game = g_stage.Game;
            return frame;
        }

        void MaskCorners(ImDrawList* drawList, ImVec2 min, ImVec2 max, float radius, ImU32 color)
        {
            constexpr float kQuarter = 3.14159265f * 0.5f;
            const struct
            {
                ImVec2 Corner;
                ImVec2 Center;
                float From;
            } corners[] = {
                { min, ImVec2(min.x + radius, min.y + radius), kQuarter * 2.0f },
                { ImVec2(max.x, min.y), ImVec2(max.x - radius, min.y + radius), kQuarter * 3.0f },
                { max, ImVec2(max.x - radius, max.y - radius), 0.0f },
                { ImVec2(min.x, max.y), ImVec2(min.x + radius, max.y - radius), kQuarter },
            };
            for (const auto& corner : corners)
            {
                drawList->PathLineTo(corner.Corner);
                drawList->PathArcTo(corner.Center, radius, corner.From, corner.From + kQuarter, 8);
                drawList->PathFillConcave(color);
            }
        }
    }

    CreatureStage DrawCreatureStage(IdlerModel& model, ImVec2 min, ImVec2 max, Habit habit, bool game)
    {
        g_stage.Pixel = std::max(1.0f, std::round(Px(2.0f)));
        g_stage.Columns = static_cast<int>(std::ceil((max.x - min.x) / g_stage.Pixel));
        g_stage.Rows = static_cast<int>(std::ceil((max.y - min.y) / g_stage.Pixel));
        g_stage.Daylight = g_daylight >= 0.0f ? g_daylight : model.RunningCount() > 0 ? 1.0f : 0.0f;
        g_stage.Season = model.CurrentSeason();
        g_stage.Game = game;
        g_stage.Ground = std::round(static_cast<float>(g_stage.Rows) * (habit == Habit::Floats ? 0.78f : habit == Habit::Swims ? 0.84f : 0.9f));
        if (IdleAnimation::Enabled())
            IdleAnimation::RequestFrames();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(min, max, true);
        const StageScene::Frame frame = StageFrame(drawList, min);
        if (habit == Habit::Floats)
            StageScene::DrawCloudsBack(frame);
        else if (habit == Habit::Swims)
            StageScene::DrawUnderwaterBack(frame);
        else
            StageScene::DrawMeadowBack(frame);
        drawList->PopClipRect();
        return { min.y + g_stage.Pixel * g_stage.Ground };
    }

    void DrawCreatureStageFront(ImVec2 min, ImVec2 max, Habit habit, ImU32 corners)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(min, max, true);
        const StageScene::Frame frame = StageFrame(drawList, min);
        if (habit == Habit::Floats)
            StageScene::DrawCloudsFront(frame);
        else if (habit == Habit::Swims)
            StageScene::DrawUnderwaterFront(frame);
        else
            StageScene::DrawMeadowFront(frame);
        drawList->PopClipRect();
        MaskCorners(drawList, min, max, Px(4.0f), corners != 0 ? corners : ImGui::GetColorU32(ManifoldTheme::Card));
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
