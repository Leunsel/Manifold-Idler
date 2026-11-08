#include "UI/StageScene.h"

#include "UI/Theme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Idler::UI::StageScene
{
    using namespace HomeScene;

    namespace
    {
        constexpr ImVec4 Col(float red, float green, float blue)
        {
            return ImVec4(red, green, blue, 1.0f);
        }

        struct WaterColor
        {
            ImVec4 Day;
            ImVec4 Night;
            ImVec4 Low;
        };

        constexpr WaterColor kShallow{ Col(0.235f, 0.545f, 0.610f), Col(0.110f, 0.225f, 0.340f), Col(0.400f, 0.400f, 0.470f) };
        constexpr WaterColor kOpen{ Col(0.105f, 0.375f, 0.490f), Col(0.068f, 0.158f, 0.262f), Col(0.170f, 0.250f, 0.370f) };
        constexpr WaterColor kDeep{ Col(0.045f, 0.230f, 0.350f), Col(0.038f, 0.100f, 0.185f), Col(0.070f, 0.140f, 0.240f) };
        constexpr WaterColor kSurface{ Col(0.560f, 0.830f, 0.840f), Col(0.300f, 0.420f, 0.520f), Col(1.000f, 0.780f, 0.540f) };
        constexpr WaterColor kShaft{ Col(0.820f, 0.970f, 0.930f), Col(0.560f, 0.680f, 0.800f), Col(1.000f, 0.820f, 0.560f) };
        constexpr ImVec4 kGlow = Col(0.300f, 0.900f, 0.800f);

        constexpr ImVec4 kCliff = Col(0.110f, 0.190f, 0.230f);
        constexpr ImVec4 kCliffLit = Col(0.420f, 0.560f, 0.560f);
        constexpr ImVec4 kCliffShade = Col(0.050f, 0.110f, 0.160f);
        constexpr ImVec4 kReefRock = Col(0.230f, 0.290f, 0.300f);
        constexpr ImVec4 kReefRockLit = Col(0.460f, 0.540f, 0.500f);
        constexpr ImVec4 kReefRockShade = Col(0.120f, 0.170f, 0.200f);
        constexpr ImVec4 kReefAlgae = Col(0.300f, 0.420f, 0.220f);
        constexpr ImVec4 kSandDay = Col(0.640f, 0.590f, 0.440f);
        constexpr ImVec4 kSandCrest = Col(0.760f, 0.720f, 0.560f);
        constexpr ImVec4 kSandTrough = Col(0.500f, 0.460f, 0.350f);
        constexpr ImVec4 kSandEdge = Col(0.720f, 0.700f, 0.560f);
        constexpr ImVec4 kPebble = Col(0.420f, 0.400f, 0.360f);
        constexpr ImVec4 kPebbleLit = Col(0.600f, 0.580f, 0.520f);
        constexpr ImVec4 kKelpStalk = Col(0.270f, 0.310f, 0.120f);
        constexpr ImVec4 kKelpLeaf = Col(0.350f, 0.430f, 0.150f);
        constexpr ImVec4 kKelpLeafLit = Col(0.510f, 0.590f, 0.230f);
        constexpr ImVec4 kKelpFloat = Col(0.600f, 0.530f, 0.210f);
        constexpr ImVec4 kGrassDark = Col(0.120f, 0.310f, 0.190f);
        constexpr ImVec4 kGrassMid = Col(0.200f, 0.450f, 0.250f);
        constexpr ImVec4 kGrassLit = Col(0.360f, 0.600f, 0.330f);
        constexpr ImVec4 kFish = Col(0.060f, 0.130f, 0.180f);
        constexpr ImVec4 kFrontRock = Col(0.100f, 0.140f, 0.160f);
        constexpr ImVec4 kFrontRockLit = Col(0.300f, 0.380f, 0.360f);

        struct Mood
        {
            float Day = 1.0f;
            float Low = 0.0f;

            ImVec4 Of(ImVec4 day, float warmth = 0.3f) const
            {
                const ImVec4 night = warmth >= 0.6f ? ImVec4(day.x * 0.30f + 0.032f, day.y * 0.37f + 0.066f, day.z * 0.45f + 0.122f, 1.0f)
                                                    : ImVec4(day.x * 0.26f + 0.022f, day.y * 0.33f + 0.048f, day.z * 0.42f + 0.096f, 1.0f);
                const ImVec4 now = DayNight(day, night, Day);
                const ImVec4 warm(std::min(1.0f, now.x * 1.22f + 0.03f), now.y * 0.96f, now.z * 0.80f, 1.0f);
                return Mix(now, warm, Low * warmth);
            }

            ImVec4 Water(const WaterColor& color) const
            {
                return Mix(DayNight(color.Day, color.Night, Day), color.Low, Low * 0.75f);
            }

            float Shine() const
            {
                return std::max(Day, 0.45f) * (1.0f - 0.25f * Low);
            }
        };

        ImVec4 SeasonTint(SceneSeason season)
        {
            switch (season)
            {
            case SceneSeason::Summer:
                return Col(0.080f, 0.440f, 0.640f);
            case SceneSeason::Autumn:
                return Col(0.260f, 0.420f, 0.330f);
            case SceneSeason::Winter:
                return Col(0.220f, 0.340f, 0.460f);
            default:
                return Col(0.180f, 0.520f, 0.460f);
            }
        }

        void Box(const Canvas& paint, int left, int top, int right, int bottom, ImVec4 color)
        {
            paint.Fill(static_cast<float>(left), static_cast<float>(top), static_cast<float>(right), static_cast<float>(bottom), color);
        }

        std::uint32_t Hash(int x, int y, std::uint32_t salt)
        {
            return Noise((static_cast<std::uint32_t>(x + 4096) * 73856093u) ^ (static_cast<std::uint32_t>(y + 4096) * 19349663u) ^ (salt * 83492791u));
        }

        float Swell(float x, float length, std::uint32_t salt)
        {
            const float at = x / length;
            const float knot = std::floor(at);
            const float share = at - knot;
            const int index = static_cast<int>(knot);
            const float from = static_cast<float>(Hash(index, 0, salt) & 1023u) / 1023.0f;
            const float to = static_cast<float>(Hash(index + 1, 0, salt) & 1023u) / 1023.0f;
            return from + (to - from) * share * share * (3.0f - 2.0f * share);
        }

        float Sway(double time, double speed, double phase)
        {
            return static_cast<float>(std::sin(time * speed + phase));
        }

        int Round(float value)
        {
            return static_cast<int>(std::lround(value));
        }

        template <typename Draw>
        void Scatter(int columns, int rows, int cellWidth, int cellHeight, std::uint32_t salt, std::uint32_t share, Draw&& draw)
        {
            for (int row = 0; row * cellHeight < rows; ++row)
            {
                for (int column = 0; column * cellWidth < columns; ++column)
                {
                    const std::uint32_t mixed = Hash(column, row, salt);
                    if ((mixed >> 24) >= share)
                        continue;
                    const int x = column * cellWidth + static_cast<int>(mixed % static_cast<std::uint32_t>(cellWidth));
                    const int y = row * cellHeight + static_cast<int>((mixed >> 12) % static_cast<std::uint32_t>(cellHeight));
                    if (x < columns && y < rows)
                        draw(x, y, mixed);
                }
            }
        }

        template <std::size_t Count, typename Palette, typename Cell>
        void PaintRows(const Canvas& paint, int columns, int top, int bottom, Palette&& palette, Cell&& cell)
        {
            std::array<ImVec4, Count> colors{};
            for (int y = top; y < bottom; ++y)
            {
                palette(y, colors);
                int start = 0;
                int current = cell(0, y);
                for (int x = 1; x <= columns; ++x)
                {
                    const int next = x < columns ? cell(x, y) : -1;
                    if (x < columns && next == current)
                        continue;
                    if (current >= 0)
                        Box(paint, start, y, x, y + 1, colors[static_cast<std::size_t>(current)]);
                    start = x;
                    current = next;
                }
            }
        }

        enum Tone : int
        {
            kEmpty,
            kRockDeep,
            kRockDark,
            kRockMid,
            kRockLight,
            kRockLit,
            kAlgae,
            kAlgaeLit,
            kBrainDark,
            kBrain,
            kBrainLit,
            kFanDark,
            kFan,
            kFanLit,
            kSpongeHole,
            kSpongeDark,
            kSponge,
            kSpongeLit,
            kAnemoneFoot,
            kTentacle,
            kTentacleTip,
            kStagDark,
            kStag,
            kStagLit,
            kClayDeep,
            kClayDark,
            kClay,
            kClayLit,
            kIronDark,
            kIron,
            kIronLit,
            kRust,
            kWoodDeep,
            kWoodDark,
            kWood,
            kWoodLit,
            kStarDark,
            kStar,
            kStarLit,
            kShellDark,
            kShell,
            kSandShadow,
            kMound,
            kMoundLit,
            kToneCount
        };

        class Grid
        {
        public:
            static constexpr int kSize = 112;
            static constexpr int kOriginX = kSize / 2;
            static constexpr int kOriginY = kSize - 8;

            void Set(int x, int y, int tone)
            {
                const int column = x + kOriginX;
                const int row = y + kOriginY;
                if (column < 0 || row < 0 || column >= kSize || row >= kSize)
                    return;
                m_cells[static_cast<std::size_t>(row * kSize + column)] = static_cast<std::uint8_t>(tone);
                m_left = std::min(m_left, column);
                m_right = std::max(m_right, column);
                m_top = std::min(m_top, row);
                m_bottom = std::max(m_bottom, row);
            }

            int At(int x, int y) const
            {
                const int column = x + kOriginX;
                const int row = y + kOriginY;
                if (column < 0 || row < 0 || column >= kSize || row >= kSize)
                    return kEmpty;
                return m_cells[static_cast<std::size_t>(row * kSize + column)];
            }

            int TopOf(int x) const
            {
                for (int y = -kOriginY; y < 0; ++y)
                {
                    if (At(x, y) != kEmpty)
                        return y;
                }
                return 0;
            }

            void Emit(const Canvas& paint, const ImVec4* colors, int atX, int atY) const
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    const std::uint8_t* line = &m_cells[static_cast<std::size_t>(row * kSize)];
                    int start = m_left;
                    for (int column = m_left + 1; column <= m_right + 1; ++column)
                    {
                        if (column <= m_right && line[column] == line[start])
                            continue;
                        if (line[start] != kEmpty)
                        {
                            const int y = atY + row - kOriginY;
                            Box(paint, atX + start - kOriginX, y, atX + column - kOriginX, y + 1, colors[line[start]]);
                        }
                        start = column;
                    }
                }
            }

        private:
            std::array<std::uint8_t, kSize * kSize> m_cells{};
            int m_left = kSize;
            int m_right = -1;
            int m_top = kSize;
            int m_bottom = -1;
        };

        std::array<ImVec4, kToneCount> ToneColors(const Mood& mood)
        {
            std::array<ImVec4, kToneCount> colors{};
            const auto set = [&](int tone, ImVec4 day, float warmth = 0.3f) { colors[static_cast<std::size_t>(tone)] = mood.Of(day, warmth); };
            set(kRockDeep, Col(0.100f, 0.120f, 0.150f));
            set(kRockDark, Col(0.190f, 0.220f, 0.250f));
            set(kRockMid, Col(0.310f, 0.340f, 0.350f));
            set(kRockLight, Col(0.420f, 0.460f, 0.450f), 0.6f);
            set(kRockLit, Col(0.570f, 0.620f, 0.580f), 0.8f);
            set(kAlgae, Col(0.290f, 0.410f, 0.210f));
            set(kAlgaeLit, Col(0.450f, 0.580f, 0.290f), 0.7f);
            set(kBrainDark, Col(0.450f, 0.370f, 0.170f));
            set(kBrain, Col(0.680f, 0.580f, 0.290f));
            set(kBrainLit, Col(0.850f, 0.760f, 0.440f), 0.7f);
            set(kFanDark, Col(0.400f, 0.210f, 0.150f));
            set(kFan, Col(0.600f, 0.350f, 0.210f));
            set(kFanLit, Col(0.770f, 0.500f, 0.290f), 0.7f);
            set(kSpongeHole, Col(0.060f, 0.130f, 0.140f));
            set(kSpongeDark, Col(0.150f, 0.340f, 0.330f));
            set(kSponge, Col(0.250f, 0.500f, 0.460f));
            set(kSpongeLit, Col(0.410f, 0.670f, 0.570f), 0.7f);
            set(kAnemoneFoot, Col(0.480f, 0.420f, 0.300f));
            set(kTentacle, Col(0.330f, 0.600f, 0.450f));
            set(kTentacleTip, Col(0.560f, 0.810f, 0.580f), 0.7f);
            set(kStagDark, Col(0.480f, 0.380f, 0.220f));
            set(kStag, Col(0.720f, 0.600f, 0.360f));
            set(kStagLit, Col(0.880f, 0.780f, 0.500f), 0.7f);
            set(kClayDeep, Col(0.150f, 0.090f, 0.070f));
            set(kClayDark, Col(0.370f, 0.210f, 0.140f));
            set(kClay, Col(0.570f, 0.340f, 0.210f));
            set(kClayLit, Col(0.730f, 0.490f, 0.310f), 0.7f);
            set(kIronDark, Col(0.170f, 0.170f, 0.190f));
            set(kIron, Col(0.340f, 0.340f, 0.340f));
            set(kIronLit, Col(0.540f, 0.540f, 0.510f), 0.7f);
            set(kRust, Col(0.560f, 0.340f, 0.180f));
            set(kWoodDeep, Col(0.090f, 0.070f, 0.060f));
            set(kWoodDark, Col(0.190f, 0.150f, 0.110f));
            set(kWood, Col(0.310f, 0.250f, 0.170f));
            set(kWoodLit, Col(0.460f, 0.380f, 0.260f), 0.7f);
            set(kStarDark, Col(0.580f, 0.310f, 0.150f));
            set(kStar, Col(0.820f, 0.530f, 0.250f));
            set(kStarLit, Col(0.930f, 0.690f, 0.390f), 0.7f);
            set(kShellDark, Col(0.540f, 0.450f, 0.350f));
            set(kShell, Col(0.820f, 0.760f, 0.630f), 0.6f);
            set(kSandShadow, kSandTrough);
            set(kMound, kSandDay);
            set(kMoundLit, kSandCrest, 0.6f);
            colors[kSandShadow] = Mix(colors[kSandShadow], Col(0.0f, 0.0f, 0.0f), 0.18f);
            colors[kTentacleTip] = Mix(colors[kTentacleTip], kGlow, (1.0f - mood.Day) * 0.85f);
            return colors;
        }

        constexpr int kBands = 24;

        struct Sea
        {
            Canvas Paint;
            int Columns = 0;
            int Rows = 0;
            int Ground = 0;
            int Surface = 0;
            double Time = 0.0;
            Mood Light;
            bool Game = false;
            float SourceX = 0.0f;
            float Slant = 1.0f;
            float Side = -1.0f;
            std::array<ImVec4, kBands> Water{};
            ImVec4 Haze;
        };

        ImVec4 WaterAt(const Mood& mood, SceneSeason season, float share)
        {
            const ImVec4 shallow = mood.Water(kShallow);
            const ImVec4 open = mood.Water(kOpen);
            const ImVec4 deep = mood.Water(kDeep);
            const ImVec4 water = share < 0.3f ? Mix(shallow, open, share / 0.3f) : Mix(open, deep, std::min(1.0f, (share - 0.3f) / 0.7f));
            return Mix(water, mood.Of(SeasonTint(season), 0.0f), 0.08f);
        }

        Sea SeaOf(const Frame& frame)
        {
            Sea sea;
            sea.Paint = frame.Paint;
            sea.Columns = frame.Columns;
            sea.Rows = frame.Rows;
            sea.Ground = std::clamp(Round(frame.Ground), 1, frame.Rows);
            sea.Surface = std::clamp(Round(static_cast<float>(sea.Ground) * 0.05f), 2, 8);
            sea.Time = frame.Time;
            sea.Light.Day = std::clamp(frame.Daylight, 0.0f, 1.0f);
            sea.Light.Low = std::clamp(1.0f - std::fabs(frame.Daylight - 0.5f) / 0.4f, 0.0f, 1.0f);
            sea.Game = frame.Game;
            sea.Slant = frame.Daylight >= 0.5f ? 1.0f : -1.0f;
            sea.Side = -sea.Slant;
            sea.SourceX = std::round(static_cast<float>(frame.Columns) * (sea.Slant > 0.0f ? 0.34f : 0.70f));
            for (int band = 0; band < kBands; ++band)
                sea.Water[static_cast<std::size_t>(band)] = WaterAt(sea.Light, frame.Season, static_cast<float>(band) / static_cast<float>(kBands - 1));
            sea.Haze = sea.Water[kBands - 1];
            return sea;
        }

        int BandAt(const Sea& sea, int y)
        {
            const float at = std::clamp(static_cast<float>(y) / static_cast<float>(sea.Ground), 0.0f, 1.0f) * static_cast<float>(kBands - 1);
            int band = static_cast<int>(at);
            if (at - static_cast<float>(band) > 0.55f && (y & 1) == 0)
                ++band;
            return std::min(band, kBands - 1);
        }

        const ImVec4& WaterIn(const Sea& sea, int y)
        {
            return sea.Water[static_cast<std::size_t>(BandAt(sea, y))];
        }

        struct Wreck
        {
            int Middle = 0;
            int Length = 0;
        };

        Wreck WreckOf(const Sea& sea)
        {
            Wreck wreck;
            if (sea.Columns < 110 || sea.Ground < 30)
                return wreck;
            wreck.Middle = Round(static_cast<float>(sea.Columns) * 0.80f);
            wreck.Length = std::clamp(Round(static_cast<float>(sea.Ground) * 0.48f), 26, 76);
            return wreck;
        }

        std::vector<int> CliffTops(const Sea& sea, const Wreck& wreck)
        {
            std::vector<int> tops(static_cast<std::size_t>(sea.Columns));
            const auto ground = static_cast<float>(sea.Ground);
            for (int x = 0; x < sea.Columns; ++x)
            {
                const auto at = static_cast<float>(x);
                const float shape = 0.6f * Swell(at, 58.0f, 7u) + 0.32f * Swell(at, 19.0f, 19u) + 0.08f * Swell(at, 5.0f, 29u);
                float high = 0.07f + 0.5f * shape * shape;
                if (wreck.Length > 0)
                {
                    const float apart = std::fabs(at - static_cast<float>(wreck.Middle)) / (static_cast<float>(wreck.Length) * 0.9f);
                    if (apart < 1.0f)
                        high *= 0.3f + 0.7f * apart * apart;
                }
                const int rise = Round(ground * high);
                tops[static_cast<std::size_t>(x)] = sea.Ground - (rise - rise % 2);
            }
            return tops;
        }

        void DrawWater(const Sea& sea, const std::vector<int>& cliffs)
        {
            enum : int
            {
                kWater,
                kGlowing,
                kSurfaceLight,
                kSurfaceSoft,
                kCliffBody,
                kCliffRim,
                kCliffDark,
                kCliffLedge,
                kWaterColors
            };
            const Mood& mood = sea.Light;
            const ImVec4 surface = mood.Water(kSurface);
            const ImVec4 shaft = mood.Water(kShaft);
            const ImVec4 rock = mood.Of(kCliff);
            const ImVec4 rim = mood.Of(kCliffLit, 0.7f);
            const ImVec4 shade = mood.Of(kCliffShade);
            const auto ground = static_cast<float>(sea.Ground);
            const float cone = ground * 0.45f;
            const float slope = sea.Slant * (0.42f + 0.22f * mood.Low);
            const float glow = 0.11f * mood.Shine() + 0.14f * mood.Low;
            const int columns = sea.Columns;
            const auto calm = static_cast<float>(columns) * 0.16f;
            PaintRows<kWaterColors>(sea.Paint, columns, 0, sea.Ground,
                [&](int y, std::array<ImVec4, kWaterColors>& colors)
                {
                    const ImVec4& water = WaterIn(sea, y);
                    colors[kWater] = water;
                    colors[kGlowing] = Mix(water, shaft, glow * std::max(0.0f, 1.0f - static_cast<float>(y) / cone));
                    colors[kSurfaceLight] = Mix(water, surface, y == 0 ? 0.55f : 0.34f);
                    colors[kSurfaceSoft] = Mix(water, surface, 0.17f);
                    colors[kCliffBody] = Mix(rock, water, 0.72f);
                    colors[kCliffRim] = Mix(rim, water, 0.70f);
                    colors[kCliffDark] = Mix(shade, water, 0.66f);
                    colors[kCliffLedge] = Mix(shade, water, 0.58f);
                },
                [&](int x, int y) -> int
                {
                    const int top = cliffs[static_cast<std::size_t>(x)];
                    if (y >= top)
                    {
                        if (y == top)
                            return kCliffRim;
                        const int toward = std::clamp(x + (sea.Side < 0.0f ? -1 : 1), 0, columns - 1);
                        const int away = std::clamp(x - (sea.Side < 0.0f ? -1 : 1), 0, columns - 1);
                        if (cliffs[static_cast<std::size_t>(toward)] > y)
                            return kCliffRim;
                        if (cliffs[static_cast<std::size_t>(away)] > y)
                            return kCliffDark;
                        if ((y + 3) % 11 == 0 && Swell(static_cast<float>(x), 9.0f, static_cast<std::uint32_t>(y)) > 0.42f)
                            return kCliffLedge;
                        return kCliffBody;
                    }
                    const auto at = static_cast<float>(x);
                    const float blot = ground * 0.07f + 3.0f - static_cast<float>(y) * 1.2f;
                    if (y < sea.Surface + 4 && std::fabs(at - sea.SourceX - slope * static_cast<float>(y)) < blot)
                        return y < sea.Surface ? kSurfaceLight : kSurfaceSoft;
                    if (y < sea.Surface)
                    {
                        if (y == 0)
                            return kSurfaceLight;
                        const float wave = Sway(sea.Time, 0.55, static_cast<double>(x) * 0.19 + y * 1.9) + 0.75f * Sway(sea.Time, -0.4, static_cast<double>(x) * 0.071 + y * 0.8);
                        float need = 1.1f + 0.5f * static_cast<float>(y) / static_cast<float>(sea.Surface);
                        if (sea.Game && at < calm)
                            need += 0.3f;
                        if (wave > need + 0.3f)
                            return kSurfaceLight;
                        if (wave > need)
                            return kSurfaceSoft;
                        return kWater;
                    }
                    if (static_cast<float>(y) < cone)
                    {
                        const float half = ground * 0.10f + static_cast<float>(y) * 0.32f + ((y & 1) != 0 ? 1.5f : -1.5f);
                        if (std::fabs(at - sea.SourceX - slope * static_cast<float>(y)) < half)
                            return kGlowing;
                    }
                    return kWater;
                });
        }

        void DrawWreck(const Sea& sea, const Wreck& wreck, const std::array<ImVec4, kToneCount>& colors)
        {
            if (wreck.Length <= 0)
                return;
            Grid grid;
            const int length = wreck.Length;
            const int half = length / 2;
            const int tall = std::max(5, Round(static_cast<float>(length) * 0.22f));
            const int broken = -half + Round(static_cast<float>(length) * 0.26f);
            const auto set = [&](int u, int v, int tone) { grid.Set(u, v + Round(static_cast<float>(u) * 0.09f), tone); };
            const auto railAt = [&](int u)
            {
                const float along = std::max(0.0f, static_cast<float>(u) / static_cast<float>(half));
                return -tall - Round(0.45f * static_cast<float>(tall) * along * along);
            };
            const auto bowTop = static_cast<float>(railAt(half));
            const int holeU = Round(static_cast<float>(length) * 0.12f);
            const int holeV = -tall / 2;
            for (int u = -half; u <= half; ++u)
            {
                const int top = railAt(u);
                for (int v = top; v <= 2; ++v)
                {
                    const float down = std::clamp((static_cast<float>(v) - bowTop) / -bowTop, 0.0f, 1.0f);
                    if (u > half - Round(0.8f * static_cast<float>(tall) * down * down))
                        continue;
                    const int jag = static_cast<int>(Hash(v, 0, 401u) % 3u);
                    if (u < broken + jag)
                    {
                        if (v >= -1 || ((u - broken) % 4 == 0 && v > top + (broken - u) / 2 + 1))
                            set(u, v, kWoodDark);
                        continue;
                    }
                    int tone = kWood;
                    if (v == top)
                        tone = kWoodLit;
                    else if (v == top + 1)
                        tone = kWoodDark;
                    else if ((v - top) % 3 == 1)
                        tone = kWoodDark;
                    if (tall >= 7 && v >= top + 3 && v <= top + 4 && (u - broken + 3) % 7 < 2 && u < half - 6)
                        tone = kWoodDeep;
                    const float holeX = static_cast<float>(u - holeU) / 3.5f;
                    const float holeY = static_cast<float>(v - holeV) / 2.2f;
                    if (holeX * holeX + holeY * holeY < 1.0f - 0.25f * static_cast<float>(Hash(u, v, 405u) % 3u))
                        tone = kWoodDeep;
                    if (v >= -1)
                        tone = kWoodDark;
                    if (u < broken + jag + 1)
                        tone = kWoodDeep;
                    set(u, v, tone);
                }
            }
            const int mastX = Round(static_cast<float>(length) * 0.04f);
            const int mastFoot = railAt(mastX);
            const int mastLength = Round(static_cast<float>(tall) * 2.0f);
            for (int step = 0; step < mastLength; ++step)
            {
                const int u = mastX - Round(static_cast<float>(step) * 0.5f);
                set(u, mastFoot - step, kWoodDark);
                if (step < mastLength - 2)
                    set(u + 1, mastFoot - step, kWood);
            }
            const int yardStep = Round(static_cast<float>(mastLength) * 0.66f);
            const int yardX = mastX - Round(static_cast<float>(yardStep) * 0.5f);
            const int yardHalf = std::max(3, tall * 2 / 3);
            for (int along = -yardHalf; along <= yardHalf; ++along)
                set(yardX + along, mastFoot - yardStep + Round(static_cast<float>(along) * 0.45f), kWoodDark);
            const int foreX = Round(static_cast<float>(length) * 0.3f);
            for (int step = 0; step < Round(static_cast<float>(tall) * 0.7f); ++step)
                set(foreX, railAt(foreX) - step, step % 4 == 3 ? kWood : kWoodDark);
            const int bowRow = railAt(half);
            for (int step = 0; step < std::max(3, tall * 3 / 4); ++step)
                set(half - 1 + step, bowRow - step / 2, kWoodDark);

            Canvas paint = sea.Paint;
            paint.Haze = 0.5f;
            paint.HazeColor = WaterIn(sea, sea.Ground - tall);
            grid.Emit(paint, colors.data(), wreck.Middle, sea.Ground);
        }

        void DrawFish(const Sea& sea)
        {
            const auto ground = static_cast<float>(sea.Ground);
            const auto span = static_cast<double>(sea.Columns + 80);
            for (int school = 0; school < 3; ++school)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(school) * 7919u + 23u);
                const int heading = school % 2 == 0 ? 1 : -1;
                const double travelled = std::fmod(sea.Time * (1.3 + 0.45 * school) + static_cast<double>(mixed % 997u), span);
                const double lead = heading > 0 ? travelled - 40.0 : static_cast<double>(sea.Columns) + 40.0 - travelled;
                const float leadY = ground * (0.34f + 0.15f * static_cast<float>(school)) + Sway(sea.Time, 0.21, school * 2.1) * 3.0f;
                const ImVec4 body = Mix(sea.Light.Of(kFish), WaterIn(sea, Round(leadY)), 0.5f + 0.06f * static_cast<float>(school));
                const int count = 7 + static_cast<int>(mixed % 5u);
                for (int fish = 0; fish < count; ++fish)
                {
                    const std::uint32_t own = Noise(mixed + static_cast<std::uint32_t>(fish) * 131u);
                    const int x = static_cast<int>(std::lround(lead)) - heading * static_cast<int>(own % 26u);
                    const int y = Round(leadY + static_cast<float>(static_cast<int>((own >> 8) % 9u) - 4) + Sway(sea.Time, 0.9, fish * 1.7) * 0.6f);
                    if (x < -4 || x > sea.Columns + 4)
                        continue;
                    const int tail = heading > 0 ? x - 1 : x + 3;
                    Box(sea.Paint, x, y, x + 3, y + 1, body);
                    Box(sea.Paint, tail, y - 1, tail + 1, y, body);
                    Box(sea.Paint, tail, y + 1, tail + 1, y + 2, body);
                }
            }
        }

        struct ReefWall
        {
            std::vector<int> Top;
            std::vector<float> Toward;
            std::vector<int> Middles;
            int Highest = 0;
        };

        ReefWall ReefWallOf(const Sea& sea, const Wreck& wreck)
        {
            ReefWall wall;
            const auto columns = static_cast<std::size_t>(sea.Columns);
            wall.Top.assign(columns, sea.Ground);
            wall.Toward.assign(columns, 2.0f);
            wall.Highest = sea.Ground;
            const auto ground = static_cast<float>(sea.Ground);
            const float size = std::clamp(ground / 140.0f, 0.45f, 1.1f);
            const auto dip = [&](float at)
            {
                if (wreck.Length <= 0)
                    return 1.0f;
                const float apart = std::fabs(at - static_cast<float>(wreck.Middle)) / (static_cast<float>(wreck.Length) * 0.75f);
                return apart < 1.0f ? 0.15f + 0.85f * apart * apart : 1.0f;
            };
            for (int x = 0; x < sea.Columns; ++x)
            {
                const auto at = static_cast<float>(x);
                const float rise = ground * (0.025f + 0.085f * Swell(at, 37.0f, 71u) + 0.025f * Swell(at, 8.0f, 73u)) * dip(at);
                wall.Top[static_cast<std::size_t>(x)] = sea.Ground - Round(rise);
            }
            constexpr int kSpacing = 29;
            for (int index = 0; index * kSpacing < sea.Columns + kSpacing; ++index)
            {
                const std::uint32_t mixed = Hash(index, 1, 33u);
                if ((mixed & 255u) >= 165u)
                    continue;
                const int middle = index * kSpacing + static_cast<int>(mixed % 22u);
                if (middle < 0 || middle >= sea.Columns || dip(static_cast<float>(middle)) < 0.9f)
                    continue;
                const int half = Round(static_cast<float>(4 + static_cast<int>((mixed >> 8) % 8u)) * size);
                const float high = static_cast<float>(half) * (0.6f + 0.5f * static_cast<float>((mixed >> 16) & 255u) / 255.0f);
                const int seat = wall.Top[static_cast<std::size_t>(middle)] + 1;
                wall.Middles.push_back(middle);
                for (int x = std::max(0, middle - half); x <= std::min(sea.Columns - 1, middle + half); ++x)
                {
                    const float across = (static_cast<float>(x - middle) + 0.5f) / (static_cast<float>(half) + 0.5f);
                    const int top = seat - Round(high * std::sqrt(std::max(0.0f, 1.0f - across * across)));
                    const auto i = static_cast<std::size_t>(x);
                    if (top < wall.Top[i])
                    {
                        wall.Top[i] = top;
                        wall.Toward[i] = across * sea.Side;
                    }
                }
            }
            for (std::size_t i = 0; i < columns; ++i)
            {
                wall.Highest = std::min(wall.Highest, wall.Top[i]);
                if (wall.Toward[i] < 1.5f)
                    continue;
                const int before = wall.Top[i < 3 ? 0 : i - 3];
                const int after = wall.Top[std::min(columns - 1, i + 3)];
                wall.Toward[i] = std::clamp(static_cast<float>(after - before) * 0.2f, -1.0f, 1.0f) * sea.Side;
            }
            return wall;
        }

        void DrawReefWall(const Sea& sea, const ReefWall& wall)
        {
            enum : int
            {
                kBody,
                kRim,
                kShade,
                kHole,
                kGreen,
                kWallColors
            };
            const Mood& mood = sea.Light;
            const ImVec4 body = mood.Of(kReefRock);
            const ImVec4 rim = mood.Of(kReefRockLit, 0.7f);
            const ImVec4 shade = mood.Of(kReefRockShade);
            const ImVec4 algae = mood.Of(kReefAlgae, 0.5f);
            PaintRows<kWallColors>(sea.Paint, sea.Columns, wall.Highest, sea.Ground,
                [&](int y, std::array<ImVec4, kWallColors>& colors)
                {
                    const ImVec4& water = WaterIn(sea, y);
                    colors[kBody] = Mix(body, water, 0.5f);
                    colors[kRim] = Mix(rim, water, 0.46f);
                    colors[kShade] = Mix(shade, water, 0.5f);
                    colors[kHole] = Mix(shade, water, 0.4f);
                    colors[kGreen] = Mix(algae, water, 0.48f);
                },
                [&](int x, int y) -> int
                {
                    const auto i = static_cast<std::size_t>(x);
                    const int top = wall.Top[i];
                    if (y < top || top >= sea.Ground)
                        return -1;
                    const int down = y - top;
                    const float toward = wall.Toward[i];
                    if (down <= 1 && Swell(static_cast<float>(x), 6.0f, 91u) > 0.64f)
                        return kGreen;
                    if (down == 0)
                        return toward > -0.4f ? kRim : kBody;
                    if (down <= 2 && toward > 0.35f)
                        return kRim;
                    if (toward < -0.55f || y >= sea.Ground - 2)
                        return kShade;
                    if (down > 2 && (y + x / 11) % 6 == 0 && Swell(static_cast<float>(x), 5.0f, static_cast<std::uint32_t>(y) + 57u) > 0.7f)
                        return kHole;
                    return kBody;
                });
        }

        struct KelpTones
        {
            ImVec4 Stalk;
            ImVec4 Leaf;
            ImVec4 LeafLit;
            ImVec4 Float;
        };

        void DrawKelp(const Canvas& paint, int x, int root, int height, int width, const Sea& sea, double phase, const KelpTones& tones, bool close)
        {
            const auto offsetAt = [&](int row)
            {
                const float up = static_cast<float>(row) / static_cast<float>(std::max(1, height));
                const float bend = Sway(sea.Time, 0.42, phase) * 3.2f * up * up + Sway(sea.Time, 0.9, phase * 1.7 + row * 0.16) * 0.7f * up;
                return Round(bend + up * 1.5f * std::sin(static_cast<float>(phase)));
            };
            int start = 0;
            int current = offsetAt(0);
            for (int row = 1; row <= height; ++row)
            {
                const int next = row < height ? offsetAt(row) : current + 1000;
                if (next == current)
                    continue;
                Box(paint, x + current, root - row, x + current + width, root - start, tones.Stalk);
                start = row;
                current = next;
            }
            int blade = 0;
            int row = 3 + static_cast<int>(Hash(x, root, 5u) % 3u);
            while (row < height - 2)
            {
                const std::uint32_t own = Hash(x, row, 9u);
                const int at = offsetAt(row);
                const int out = ((blade % 2 == 0) != ((own & 7u) == 0u)) ? -1 : 1;
                const int y = root - row;
                const ImVec4& leaf = static_cast<float>(out) * sea.Side > 0.0f ? tones.LeafLit : tones.Leaf;
                const int foot = out < 0 ? x + at - 1 : x + at + width;
                const bool carries = close && blade % 2 == 0;
                if (carries)
                    Box(paint, foot, y, foot + 1, y + 1, tones.Float);
                const int from = carries ? foot + out : foot;
                const int reach = (close ? 2 : 1) + static_cast<int>((own >> 4) % 3u);
                const int lower = (reach + 1) / 2;
                if (out < 0)
                {
                    Box(paint, from - lower + 1, y, from + 1, y + 1, leaf);
                    Box(paint, from - reach + 1, y - 1, from - lower + 1, y, leaf);
                }
                else
                {
                    Box(paint, from, y, from + lower, y + 1, leaf);
                    Box(paint, from + lower, y - 1, from + reach, y, leaf);
                }
                ++blade;
                row += (close ? 3 : 4) + static_cast<int>((own >> 8) % 3u);
            }
            const int tip = offsetAt(height);
            const int lean = Sway(sea.Time, 0.42, phase) >= 0.0f ? 1 : -1;
            Box(paint, x + tip + (lean > 0 ? width : -2), root - height - 1, x + tip + (lean > 0 ? width + 2 : 0), root - height, tones.Leaf);
        }

        void DrawKelpForest(const Sea& sea, const ReefWall& wall, bool close)
        {
            const Mood& mood = sea.Light;
            const auto ground = static_cast<float>(sea.Ground);
            const int spacing = close ? 116 : 57;
            for (int index = 0; index * spacing < sea.Columns; ++index)
            {
                const std::uint32_t mixed = Hash(index, close ? 5 : 6, 37u);
                if ((mixed & 255u) >= (close ? 215u : 150u))
                    continue;
                const int first = index * spacing + 6 + static_cast<int>(mixed % static_cast<std::uint32_t>(spacing - 26));
                const int stalks = close ? 2 + static_cast<int>((mixed >> 8) % 2u) : 1;
                for (int stalk = 0; stalk < stalks; ++stalk)
                {
                    const std::uint32_t own = Noise(mixed + static_cast<std::uint32_t>(stalk) * 131u);
                    const int x = first + stalk * 6 + static_cast<int>(own % 3u);
                    if (x < 1 || x >= sea.Columns - 2)
                        continue;
                    const float share = static_cast<float>((own >> 8) & 255u) / 255.0f;
                    const int height = Round(ground * (close ? 0.24f + 0.30f * share : 0.38f + 0.30f * share));
                    const int root = close ? std::min(sea.Ground + 1, wall.Top[static_cast<std::size_t>(x)] + 2) : sea.Ground + 1;
                    const float haze = close ? 0.30f : 0.62f;
                    const ImVec4& water = WaterIn(sea, sea.Ground - height / 2);
                    const KelpTones tones{ Mix(mood.Of(kKelpStalk), water, haze), Mix(mood.Of(kKelpLeaf), water, haze),
                        Mix(mood.Of(kKelpLeafLit, 0.7f), water, haze), Mix(mood.Of(kKelpFloat, 0.6f), water, haze) };
                    const int width = close && sea.Ground > 90 ? 2 : 1;
                    DrawKelp(sea.Paint, x, root, height, width, sea, static_cast<double>((own >> 16) % 628u) / 100.0, tones, close);
                }
            }
        }

        void DrawRays(const Sea& sea)
        {
            const Mood& mood = sea.Light;
            const float strength = 0.10f * mood.Shine() + 0.05f * mood.Low;
            const ImVec4 light = mood.Water(kShaft);
            const auto ground = static_cast<float>(sea.Ground);
            const float slope = sea.Slant * (0.42f + 0.22f * mood.Low);
            const auto columns = static_cast<float>(sea.Columns);
            constexpr int kSpacing = 34;
            constexpr int kSteps = 4;
            for (int index = 0; index * kSpacing < sea.Columns + sea.Ground / 2; ++index)
            {
                const std::uint32_t mixed = Hash(index, 3, 61u);
                if ((mixed & 255u) >= 175u)
                    continue;
                const float start = static_cast<float>(index * kSpacing + static_cast<int>(mixed % 20u)) - (sea.Slant < 0.0f ? 0.0f : ground * 0.15f);
                if (start < 44.0f)
                    continue;
                const float weight = std::max(0.0f, 1.0f - std::fabs(start - sea.SourceX) / (columns * 0.55f + 20.0f));
                const double phase = static_cast<double>((mixed >> 8) % 628u) / 100.0;
                const float alpha = strength * weight * (0.65f + 0.35f * Sway(sea.Time, 0.23 + 0.02 * ((mixed >> 4) & 7u), phase));
                if (alpha < 0.022f)
                    continue;
                const float wander = Sway(sea.Time, 0.11, index * 1.3) * 2.5f;
                const int width = 2 + static_cast<int>((mixed >> 12) % 5u);
                const float length = ground * (0.55f + 0.35f * static_cast<float>((mixed >> 16) & 255u) / 255.0f);
                const int rows = Round(length);
                int from = 1;
                int fromX = Round(start + wander + slope);
                int fromStep = 0;
                for (int row = 2; row <= rows; ++row)
                {
                    const int x = Round(start + wander + slope * static_cast<float>(row));
                    const int step = std::min(kSteps - 1, row * kSteps / std::max(1, rows));
                    if (row < rows && x == fromX && step == fromStep)
                        continue;
                    const float fade = 1.0f - static_cast<float>(fromStep) / static_cast<float>(kSteps);
                    Box(sea.Paint, fromX, from, fromX + width, row, ManifoldTheme::WithAlpha(light, alpha * fade));
                    from = row;
                    fromX = x;
                    fromStep = step;
                }
            }
        }

        float ForwardOf(const Sea& sea, int y)
        {
            return std::clamp(static_cast<float>(y - sea.Ground) / static_cast<float>(std::max(1, sea.Rows - sea.Ground - 1)), 0.0f, 1.0f);
        }

        float SandHaze(float forward)
        {
            return 0.5f * std::pow(1.0f - forward, 1.3f);
        }

        void DrawSand(const Sea& sea)
        {
            enum : int
            {
                kSand,
                kCrest,
                kTrough,
                kEdge,
                kSandColors
            };
            const Mood& mood = sea.Light;
            const ImVec4 sand = mood.Of(kSandDay);
            const ImVec4 crest = mood.Of(kSandCrest, 0.6f);
            const ImVec4 trough = mood.Of(kSandTrough);
            const ImVec4 edge = mood.Of(kSandEdge, 0.6f);
            constexpr int kSandBands = 5;

            constexpr int kMostRipples = 12;
            std::array<int, kMostRipples> ripples{};
            int count = 0;
            for (int y = sea.Ground + 2, gap = 2; y < sea.Rows - 1 && count < kMostRipples; y += gap, gap += count % 2)
                ripples[static_cast<std::size_t>(count++)] = y;
            std::vector<std::int8_t> bend(static_cast<std::size_t>(count * sea.Columns));
            for (int ripple = 0; ripple < count; ++ripple)
            {
                for (int x = 0; x < sea.Columns; ++x)
                {
                    const auto at = static_cast<float>(x);
                    const float wind = std::sin(at * (0.05f + 0.008f * static_cast<float>(ripple)) + static_cast<float>(ripple) * 1.9f);
                    const bool gone = Swell(at, 9.0f + 2.0f * static_cast<float>(ripple), static_cast<std::uint32_t>(ripple) + 300u) < 0.42f;
                    bend[static_cast<std::size_t>(ripple * sea.Columns + x)] = static_cast<std::int8_t>(gone ? 99 : Round(wind));
                }
            }

            PaintRows<kSandColors>(sea.Paint, sea.Columns, sea.Ground, sea.Rows,
                [&](int y, std::array<ImVec4, kSandColors>& colors)
                {
                    const float at = ForwardOf(sea, y) * static_cast<float>(kSandBands - 1);
                    int band = static_cast<int>(at);
                    if (at - static_cast<float>(band) > 0.55f && (y & 1) == 0)
                        ++band;
                    const float haze = SandHaze(static_cast<float>(std::min(band, kSandBands - 1)) / static_cast<float>(kSandBands - 1));
                    colors[kSand] = Mix(sand, sea.Haze, haze);
                    colors[kCrest] = Mix(Mix(sand, crest, 0.7f), sea.Haze, haze);
                    colors[kTrough] = Mix(trough, sea.Haze, haze);
                    colors[kEdge] = Mix(edge, sea.Haze, 0.45f);
                },
                [&](int x, int y) -> int
                {
                    if (y == sea.Ground)
                        return kEdge;
                    for (int ripple = 0; ripple < count; ++ripple)
                    {
                        const int row = ripples[static_cast<std::size_t>(ripple)];
                        if (row > y + 1)
                            break;
                        if (row < y - 2)
                            continue;
                        const int shift = bend[static_cast<std::size_t>(ripple * sea.Columns + x)];
                        if (shift == 99)
                            continue;
                        if (y == row + shift)
                            return kCrest;
                        if (y == row + shift + 1 && ripple >= count / 2)
                            return kTrough;
                    }
                    return kSand;
                });
        }

        void DrawCaustics(const Sea& sea)
        {
            const Mood& mood = sea.Light;
            const float strength = std::clamp((mood.Day - 0.2f) / 0.6f, 0.0f, 1.0f) * (1.0f - 0.4f * mood.Low);
            if (strength <= 0.0f)
                return;
            const ImVec4 light = mood.Water(kShaft);
            for (int y = sea.Ground + 2; y < sea.Rows - 1;)
            {
                const float forward = ForwardOf(sea, y);
                const int width = Round(8.0f + 10.0f * forward);
                for (int cell = 0; cell * width < sea.Columns; ++cell)
                {
                    const std::uint32_t own = Hash(cell, y, 71u);
                    if (own % 5u < 3u)
                        continue;
                    const double phase = static_cast<double>((own >> 8) & 1023u) / 162.0;
                    const int x = cell * width + static_cast<int>((own >> 4) % static_cast<std::uint32_t>(width)) + Round(Sway(sea.Time, 0.35, phase) * 2.0f);
                    const int length = std::max(1, Round((1.5f + 2.0f * forward) * (1.0f + 0.6f * Sway(sea.Time, 0.6, phase * 1.3))));
                    const float alpha = (0.17f + 0.2f * forward) * strength * (0.6f + 0.4f * Sway(sea.Time, 0.5, phase * 0.7));
                    const ImVec4 glint = ManifoldTheme::WithAlpha(light, alpha);
                    Box(sea.Paint, x, y, x + length, y + 1, glint);
                    if (forward > 0.5f && ((own >> 20) & 1u) != 0u)
                        Box(sea.Paint, x + length, y - 1, x + length + 1 + length / 2, y, glint);
                }
                y += forward < 0.4f ? 2 : 3;
            }
        }

        void DrawSandBits(const Sea& sea)
        {
            const Mood& mood = sea.Light;
            const ImVec4 pebble = mood.Of(kPebble);
            const ImVec4 pebbleLit = mood.Of(kPebbleLit, 0.6f);
            const int depth = sea.Rows - sea.Ground - 2;
            Scatter(sea.Columns - 3, depth, 11, 4, 83u, 70u, [&](int x, int row, std::uint32_t mixed)
                {
                    const int y = sea.Ground + 2 + row;
                    const float haze = SandHaze(ForwardOf(sea, y));
                    const int wide = 1 + static_cast<int>((mixed >> 20) % 3u);
                    Box(sea.Paint, x, y, x + wide, y + 1, Mix(pebble, sea.Haze, haze));
                    if (wide >= 2)
                        Box(sea.Paint, sea.Side < 0.0f ? x : x + wide - 1, y - 1, sea.Side < 0.0f ? x + 1 : x + wide, y, Mix(pebbleLit, sea.Haze, haze));
                });
        }

        struct GrassTones
        {
            ImVec4 Dark;
            ImVec4 Mid;
            ImVec4 Lit;
        };

        GrassTones GrassOf(const Mood& mood, const ImVec4& haze, float hazed)
        {
            return { Mix(mood.Of(kGrassDark), haze, hazed), Mix(mood.Of(kGrassMid), haze, hazed), Mix(mood.Of(kGrassLit, 0.6f), haze, hazed) };
        }

        void DrawTuft(const Canvas& paint, int x, int foot, int blades, int tall, const Sea& sea, std::uint32_t seed, const GrassTones& tones)
        {
            for (int blade = 0; blade < blades; ++blade)
            {
                const std::uint32_t own = Noise(seed + static_cast<std::uint32_t>(blade) * 31u);
                const bool outer = blade == 0 || blade == blades - 1;
                const int height = std::max(2, tall - static_cast<int>(own % 4u) - (outer ? 2 : 0));
                const int lean = outer ? (blade == 0 ? -1 : 1) : 0;
                const int bend = Round(Sway(sea.Time, 0.75, static_cast<double>(seed % 97u) * 0.13 + blade * 0.35) * 1.2f) + lean;
                const int straight = height / 2;
                const int bx = x + blade;
                const bool towardLight = static_cast<float>(blade * 2 - blades + 1) * sea.Side > 0.0f;
                const ImVec4& tone = towardLight ? tones.Lit : blade % 2 == 0 ? tones.Mid : tones.Dark;
                Box(paint, bx, foot - straight, bx + 1, foot, tone);
                Box(paint, bx + bend, foot - height, bx + bend + 1, foot - straight, tone);
            }
        }

        constexpr float kClams[] = { 0.14f, 0.38f, 0.62f, 0.86f };

        bool BehindClam(const Sea& sea, int x)
        {
            for (const float share : kClams)
            {
                if (std::abs(x - Round(static_cast<float>(sea.Columns) * share)) < 10)
                    return true;
            }
            return false;
        }

        void DrawSeaGrass(const Sea& sea)
        {
            const GrassTones back = GrassOf(sea.Light, sea.Haze, 0.42f);
            const GrassTones nearer = GrassOf(sea.Light, sea.Haze, 0.15f);
            constexpr int kSpacing = 17;
            for (int index = 0; index * kSpacing < sea.Columns; ++index)
            {
                const std::uint32_t mixed = Hash(index, 7, 29u);
                if ((mixed & 255u) >= 150u)
                    continue;
                const int x = index * kSpacing + static_cast<int>(mixed % 12u);
                if (BehindClam(sea, x))
                    continue;
                const int blades = 3 + static_cast<int>((mixed >> 8) % 3u);
                const int tall = 4 + static_cast<int>((mixed >> 12) % 4u) + (sea.Ground > 90 ? 2 : 0);
                DrawTuft(sea.Paint, x, sea.Ground + 2, blades, tall, sea, mixed, back);
            }
            const int depth = sea.Rows - sea.Ground;
            if (depth < 8)
                return;
            constexpr int kNearSpacing = 47;
            for (int index = 0; index * kNearSpacing < sea.Columns; ++index)
            {
                const std::uint32_t mixed = Hash(index, 8, 31u);
                if ((mixed & 255u) >= 120u)
                    continue;
                const int x = index * kNearSpacing + static_cast<int>(mixed % 30u);
                const int foot = sea.Ground + depth / 2 + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(std::max(1, depth / 2 - 1)));
                DrawTuft(sea.Paint, x, foot, 3 + static_cast<int>((mixed >> 16) % 3u), 5 + static_cast<int>((mixed >> 20) % 4u), sea, mixed, nearer);
            }
        }

        void Rock(Grid& grid, int middle, int half, int tall, float side, std::uint32_t seed)
        {
            const float lightX = side * 0.55f;
            constexpr float kLightUp = 0.83f;
            for (int x = -half; x <= half; ++x)
            {
                const float across = (static_cast<float>(x) + 0.5f) / (static_cast<float>(half) + 0.5f);
                const float lump = 0.14f * (Swell(static_cast<float>(x), 3.0f, seed) - 0.5f);
                for (int y = -tall - 1; y <= -1; ++y)
                {
                    const float up = (-static_cast<float>(y) - 0.5f) / static_cast<float>(tall);
                    const std::uint32_t mixed = Hash(x, y, seed);
                    const float reach = 1.0f + lump - ((mixed & 7u) == 0u ? 0.1f : 0.0f);
                    const float distance = across * across + up * up;
                    if (distance > reach)
                        continue;
                    const float length = std::sqrt(std::max(0.0001f, distance));
                    const float toward = (across * lightX + up * kLightUp) / length;
                    const bool rim = distance > reach * 0.7f;
                    int tone = kRockMid;
                    if (toward > 0.8f && rim)
                        tone = kRockLit;
                    else if (toward > 0.5f)
                        tone = kRockLight;
                    else if (toward < -0.3f)
                        tone = rim ? kRockDeep : kRockDark;
                    else if (toward < 0.1f)
                        tone = kRockDark;
                    if (y >= -1 && tone > kRockDark)
                        tone = kRockDark;
                    if (!rim && Hash(x / 2, y, seed + 1u) % 31u == 0u)
                        tone = kRockDeep;
                    if (up > 0.55f && toward > 0.55f && Swell(static_cast<float>(x), 5.0f, seed + 2u) > 0.58f)
                        tone = toward > 0.85f && rim ? kAlgaeLit : kAlgae;
                    grid.Set(middle + x, y, tone);
                }
            }
            const int from = side < 0.0f ? -half + 1 : -half - 2;
            const int to = side < 0.0f ? half + 2 : half - 1;
            for (int x = from; x <= to; ++x)
                grid.Set(middle + x, 0, kSandShadow);
        }

        void BrainCoral(Grid& grid, int middle, int base, int radius, float side)
        {
            const int tall = std::max(2, radius * 3 / 4 + 1);
            for (int x = -radius; x <= radius; ++x)
            {
                for (int y = -tall; y <= -1; ++y)
                {
                    const float across = (static_cast<float>(x) + 0.5f) / (static_cast<float>(radius) + 0.5f);
                    const float up = (-static_cast<float>(y) - 0.5f) / static_cast<float>(tall);
                    if (across * across + up * up > 1.0f)
                        continue;
                    const float toward = across * side * 0.6f + up * 0.8f;
                    int tone = toward > 0.7f ? kBrainLit : toward > 0.05f ? kBrain : kBrainDark;
                    const bool groove = std::sin(static_cast<float>(x) * 1.25f + std::sin(static_cast<float>(y) * 1.1f) * 1.6f) > 0.55f;
                    if (groove && tone != kBrainDark)
                        tone = tone == kBrainLit ? kBrain : kBrainDark;
                    grid.Set(middle + x, base + y, tone);
                }
            }
        }

        void SeaFan(Grid& grid, int middle, int base, int wide, int tall, float side, std::uint32_t seed)
        {
            const int foot = base - 2;
            grid.Set(middle, base - 1, kFanDark);
            grid.Set(middle, foot, kFanDark);
            constexpr int kRibs = 7;
            for (int rib = 0; rib < kRibs; ++rib)
            {
                const float turn = static_cast<float>(rib) / static_cast<float>(kRibs - 1) * 2.0f - 1.0f;
                const float reachX = turn * static_cast<float>(wide);
                const float reachY = static_cast<float>(tall) * (1.0f - 0.4f * turn * turn);
                const int steps = std::max(2, Round(std::sqrt(reachX * reachX + reachY * reachY)));
                const int tone = turn * side > 0.2f ? kFanLit : kFan;
                const int out = turn < 0.0f ? -1 : 1;
                for (int step = 1; step <= steps; ++step)
                {
                    const float share = static_cast<float>(step) / static_cast<float>(steps);
                    const int x = middle + Round(reachX * share * (0.7f + 0.3f * share));
                    const int y = foot - Round(reachY * share);
                    grid.Set(x, y, tone);
                    if (share > 0.35f && step % 3 == 0 && Hash(rib, step, seed) % 3u != 0u)
                        grid.Set(x + out, y, kFanDark);
                }
            }
        }
        void Sponges(Grid& grid, int middle, int base, int count, float side, std::uint32_t seed)
        {
            for (int tube = 0; tube < count; ++tube)
            {
                const std::uint32_t mixed = Noise(seed + static_cast<std::uint32_t>(tube) * 97u);
                const int x = middle + (tube - count / 2) * 4 + static_cast<int>(mixed % 2u);
                const int out = tube < count / 2 ? count / 2 - tube : tube - count / 2;
                const int tall = std::max(3, 6 + static_cast<int>((mixed >> 4) % 5u) - 2 * out);
                const int lit = side < 0.0f ? -1 : 1;
                for (int y = base - tall; y < base; ++y)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        int tone = dx == lit ? kSpongeLit : dx == -lit ? kSpongeDark : kSponge;
                        if (y == base - tall)
                            tone = dx == 0 ? kSpongeLit : kSponge;
                        else if (y == base - tall + 1 && dx == 0)
                            tone = kSpongeHole;
                        grid.Set(x + dx, y, tone);
                    }
                }
            }
        }

        void Anemone(Grid& grid, int middle, int base, const Sea& sea, std::uint32_t seed)
        {
            for (int x = -1; x <= 1; ++x)
            {
                grid.Set(middle + x, base - 1, kAnemoneFoot);
                grid.Set(middle + x, base - 2, kAnemoneFoot);
            }
            const int sway = Round(Sway(sea.Time, 0.8, static_cast<double>(seed % 100u) * 0.1) * 1.2f);
            constexpr int kTalls[] = { 3, 4, 4, 5, 4, 4, 3 };
            for (int index = 0; index < 7; ++index)
            {
                const int strand = index - 3;
                const int tall = kTalls[index] + static_cast<int>(Hash(index, 0, seed) % 2u);
                for (int step = 0; step < tall; ++step)
                {
                    const int lean = strand * (step + 1) / (2 * tall) + (step >= tall - 2 ? sway : 0);
                    grid.Set(middle + strand + lean, base - 3 - step, step == tall - 1 ? kTentacleTip : kTentacle);
                }
            }
        }

        void Branch(Grid& grid, float x, float y, float lean, int length, int depth, float side)
        {
            const int lit = side < 0.0f ? -1 : 1;
            for (int step = 0; step < length; ++step)
            {
                const int cx = Round(x + lean * static_cast<float>(step));
                const int cy = Round(y) - step;
                if (depth > 0)
                {
                    grid.Set(cx, cy, depth >= 2 ? kStagDark : kStag);
                    grid.Set(cx + lit, cy, kStagLit);
                }
                else
                {
                    const bool tip = step >= length - 1;
                    grid.Set(cx, cy, tip ? kStagLit : kStag);
                    if (tip)
                        grid.Set(cx + lit, cy, kStagLit);
                }
            }
            if (depth <= 0)
                return;
            const float endX = x + lean * static_cast<float>(length - 1);
            const float endY = y - static_cast<float>(length);
            const int next = std::max(2, length * 2 / 3);
            Branch(grid, endX, endY, lean - 0.6f, next, depth - 1, side);
            Branch(grid, endX + 1.0f, endY, lean + 0.6f, next, depth - 1, side);
        }
        void Amphora(Grid& grid, int facing)
        {
            const auto set = [&](int x, int y, int tone) { grid.Set(x * facing, y, tone); };
            for (int x = -7; x <= 4; ++x)
            {
                const float across = (static_cast<float>(x) + 1.5f) / 6.0f;
                const float half = 3.4f * std::sqrt(std::max(0.0f, 1.0f - across * across));
                if (half < 0.6f)
                    continue;
                const int top = Round(-3.5f - half);
                const int bottom = std::min(-1, Round(-3.5f + half));
                for (int y = top; y <= bottom; ++y)
                {
                    const float share = static_cast<float>(y - top) / static_cast<float>(std::max(1, bottom - top));
                    int tone = share < 0.25f ? kClayLit : share < 0.7f ? kClay : kClayDark;
                    if ((x == -3 || x == 0) && tone == kClay)
                        tone = kClayDark;
                    set(x, y, tone);
                }
            }
            for (int x = 5; x <= 6; ++x)
            {
                set(x, -5, kClayLit);
                set(x, -4, kClay);
                set(x, -3, kClayDark);
            }
            set(7, -6, kClayLit);
            set(7, -5, kClayDeep);
            set(7, -4, kClayDeep);
            set(7, -3, kClayDeep);
            set(7, -2, kClayDark);
            set(2, -7, kClayDark);
            set(3, -8, kClayDark);
            set(4, -8, kClayDark);
            set(5, -7, kClayDark);
            set(5, -6, kClayDark);
            set(-8, -4, kClayDark);
            set(-8, -3, kClayDark);
            for (int x = -6; x <= 3; ++x)
                set(x, -1, x <= -4 || x >= 2 ? kMoundLit : kMound);
            set(-5, -2, kMoundLit);
            set(-7, -1, kMound);
            set(4, -1, kMound);
            set(-11, -1, kClayDark);
            set(-12, -1, kClay);
            set(-11, -2, kClayLit);
        }

        void Anchor(Grid& grid, float side, std::uint32_t seed)
        {
            constexpr const char* kRows[] = {
                ".......LIID........",
                "......L....D.......",
                ".......LIID........",
                "...DIIIILIDIIIID...",
                "...D....LID....D...",
                "........LID........",
                "........LID........",
                "........LRD........",
                "........LID........",
                "........LID........",
                "........LID........",
                "........RID........",
                "........LID........",
                ".L......LID......D.",
                ".LL.....LID.....DD.",
                "..LI....LID....ID..",
                "...LI...LID...IR...",
                "....LII.LID.IID....",
                ".....LIILIDIID.....",
                "......sLIIIDSs.....",
                "....sSSSSSSSSSSs...",
            };
            constexpr int kCount = static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
            const bool fromLeft = side < 0.0f;
            int ringX = 0;
            for (int row = 0; row < kCount; ++row)
            {
                const int lean = (kCount - 1 - row) / 8;
                for (int column = 0; kRows[row][column] != '\0'; ++column)
                {
                    int tone = kEmpty;
                    switch (kRows[row][column])
                    {
                    case 'L':
                        tone = fromLeft ? kIronLit : kIronDark;
                        break;
                    case 'D':
                        tone = fromLeft ? kIronDark : kIronLit;
                        break;
                    case 'I':
                        tone = Hash(column, row, seed) % 5u == 0u ? kRust : kIron;
                        break;
                    case 'R':
                        tone = kRust;
                        break;
                    case 'S':
                        tone = kMound;
                        break;
                    case 's':
                        tone = kMoundLit;
                        break;
                    default:
                        break;
                    }
                    if (tone == kEmpty)
                        continue;
                    grid.Set(column - 9 + lean, row - kCount, tone);
                    if (row == 1)
                        ringX = column - 9 + lean;
                }
            }
            const auto chainAt = [](int along)
            {
                if (along > 9)
                    return (along / 6) % 3 == 0 ? -2 : -1;
                const float share = 1.0f - static_cast<float>(along) / 9.0f;
                return Round(-19.0f + 18.0f * (1.0f - share * share));
            };
            int link = 0;
            for (int along = 0; along <= 26; ++along)
            {
                const int y = chainAt(along);
                const int above = along == 0 ? y : chainAt(along - 1) + 1;
                for (int row = std::min(y, above); row <= y; ++row)
                    grid.Set(ringX + 1 + along, row, link++ % 3 == 2 ? kIron : kIronDark);
            }
        }

        void Starfish(Grid& grid, float side)
        {
            constexpr const char* kRows[] = { "..a..", "abcba", ".bcb.", ".b.b." };
            int y = -4;
            for (const char* row : kRows)
            {
                for (int x = 0; row[x] != '\0'; ++x)
                {
                    if (row[x] == '.')
                        continue;
                    int tone = row[x] == 'a' ? kStarLit : row[x] == 'b' ? kStar : kStarDark;
                    if (tone == kStarLit && static_cast<float>(x - 2) * side < 0.0f && y > -4)
                        tone = kStar;
                    grid.Set(x - 2, y, tone);
                }
                ++y;
            }
        }

        void Shell(Grid& grid, int x, int y, int facing)
        {
            grid.Set(x, y - 1, kShell);
            grid.Set(x + facing, y - 1, kShellDark);
            grid.Set(x - facing, y, kShell);
            grid.Set(x, y, kShell);
            grid.Set(x + facing, y, kShellDark);
            grid.Set(x + 2 * facing, y, kShell);
        }

        enum Piece : int
        {
            kReefLeft,
            kAmphoraPiece,
            kStaghorn,
            kBrainPiece,
            kStarPiece,
            kAnchorPiece,
            kReefRight,
            kShells,
        };

        struct Place
        {
            Piece What;
            float Across;
            float Forward;
        };

        constexpr Place kPlaces[] = {
            { kReefLeft, 0.035f, 0.55f },
            { kAnchorPiece, 0.245f, 0.80f },
            { kStaghorn, 0.455f, 0.10f },
            { kBrainPiece, 0.500f, 0.20f },
            { kShells, 0.405f, 0.85f },
            { kStarPiece, 0.565f, 0.75f },
            { kAmphoraPiece, 0.735f, 0.45f },
            { kShells, 0.905f, 0.85f },
            { kReefRight, 0.965f, 0.50f },
        };

        void DrawWallCorals(const Sea& sea, const ReefWall& wall, const std::array<ImVec4, kToneCount>& colors)
        {
            std::uint32_t index = 0;
            for (const int middle : wall.Middles)
            {
                const std::uint32_t mixed = Noise(++index * 4111u + static_cast<std::uint32_t>(middle) * 31u);
                const int x = middle + static_cast<int>(mixed % 9u) - 4;
                if (x < 0 || x >= sea.Columns)
                    continue;
                const int top = wall.Top[static_cast<std::size_t>(x)];
                if (top >= sea.Ground - 3)
                    continue;
                Grid grid;
                if ((mixed >> 8) % 3u == 0u)
                {
                    SeaFan(grid, 0, 0, 5, 8, sea.Side, mixed);
                }
                else
                {
                    Branch(grid, 0.0f, -1.0f, -0.3f, 4, 2, sea.Side);
                    Branch(grid, 3.0f, -1.0f, 0.4f, 3, 1, sea.Side);
                }
                Canvas paint = sea.Paint;
                paint.Haze = 0.42f;
                paint.HazeColor = WaterIn(sea, top - 4);
                grid.Emit(paint, colors.data(), x, top + 1);
            }
        }

        void DrawReef(const Sea& sea)
        {
            const std::array<ImVec4, kToneCount> colors = ToneColors(sea.Light);
            const int depth = sea.Rows - sea.Ground;
            const float side = sea.Side;
            const float scale = std::clamp(static_cast<float>(sea.Columns) / 340.0f, 0.62f, 1.0f);
            const auto sized = [&](int cells) { return Round(static_cast<float>(cells) * scale); };
            std::uint32_t index = 0;
            for (const Place& place : kPlaces)
            {
                const std::uint32_t seed = Noise(++index * 7919u + 3u);
                const int x = Round(static_cast<float>(sea.Columns) * place.Across);
                const int foot = sea.Ground + 2 + Round(place.Forward * static_cast<float>(std::max(0, depth - 5)));
                Grid grid;
                switch (place.What)
                {
                case kReefLeft:
                {
                    Rock(grid, 0, sized(18), sized(15), side, seed);
                    Rock(grid, sized(18), sized(9), sized(9), side, seed + 5u);
                    SeaFan(grid, sized(-6), grid.TopOf(sized(-6)) + 2, sized(8), sized(13), side, seed);
                    BrainCoral(grid, sized(7), grid.TopOf(sized(7)) + 2, sized(5), side);
                    Branch(grid, static_cast<float>(sized(16)), static_cast<float>(grid.TopOf(sized(16)) + 1), -0.25f, sized(5), 2, side);
                    Branch(grid, static_cast<float>(sized(21)), static_cast<float>(grid.TopOf(sized(21)) + 1), 0.4f, sized(4), 1, side);
                    Anemone(grid, sized(-12), 0, sea, seed);
                    break;
                }
                case kAmphoraPiece:
                    Amphora(grid, (seed & 1u) != 0u ? 1 : -1);
                    break;
                case kStaghorn:
                    Branch(grid, 0.0f, -1.0f, -0.25f, 5, 2, side);
                    Branch(grid, 4.0f, -1.0f, 0.45f, 4, 2, side);
                    break;
                case kBrainPiece:
                    Rock(grid, 0, 6, 4, side, seed);
                    BrainCoral(grid, 1, grid.TopOf(1) + 2, 4, side);
                    break;
                case kStarPiece:
                    Starfish(grid, side);
                    break;
                case kAnchorPiece:
                    Anchor(grid, side, seed);
                    break;
                case kReefRight:
                {
                    Rock(grid, 0, sized(17), sized(13), side, seed);
                    Rock(grid, sized(-17), sized(8), sized(8), side, seed + 7u);
                    Sponges(grid, sized(3), grid.TopOf(sized(3)) + 2, scale > 0.8f ? 4 : 3, side, seed);
                    BrainCoral(grid, sized(13), grid.TopOf(sized(13)) + 2, sized(4), side);
                    Anemone(grid, sized(-17), grid.TopOf(sized(-17)) + 1, sea, seed + 1u);
                    break;
                }
                default:
                    Shell(grid, 0, -1, (seed & 1u) != 0u ? 1 : -1);
                    Shell(grid, 5 + static_cast<int>(seed % 4u), -2, (seed & 2u) != 0u ? 1 : -1);
                    break;
                }
                Canvas paint = sea.Paint;
                paint.Haze = SandHaze(ForwardOf(sea, foot));
                paint.HazeColor = sea.Haze;
                grid.Emit(paint, colors.data(), x, foot);
            }
        }

        void DrawReefFish(const Sea& sea)
        {
            const Mood& mood = sea.Light;
            if (mood.Day < 0.25f)
                return;
            const float shown = std::min(1.0f, (mood.Day - 0.25f) / 0.25f);
            constexpr ImVec4 kBodies[] = { ImVec4(0.950f, 0.780f, 0.250f, 1.0f), ImVec4(0.320f, 0.580f, 0.880f, 1.0f) };
            constexpr ImVec4 kTails[] = { ImVec4(0.420f, 0.300f, 0.120f, 1.0f), ImVec4(0.950f, 0.800f, 0.300f, 1.0f) };
            constexpr float kHomes[] = { 0.075f, 0.925f };
            const float scale = std::clamp(static_cast<float>(sea.Columns) / 340.0f, 0.62f, 1.0f);
            for (int group = 0; group < 2; ++group)
            {
                const ImVec4 body = ManifoldTheme::WithAlpha(mood.Of(kBodies[group], 0.6f), shown);
                const ImVec4 tail = ManifoldTheme::WithAlpha(mood.Of(kTails[group]), shown);
                const float home = static_cast<float>(sea.Columns) * kHomes[group];
                for (int fish = 0; fish < 3; ++fish)
                {
                    const double speed = 0.16 + 0.03 * fish;
                    const double phase = group * 2.3 + fish * 1.7;
                    const int facing = std::cos(sea.Time * speed + phase) >= 0.0 ? 1 : -1;
                    const int x = Round(home + (Sway(sea.Time, speed, phase) * 9.0f + static_cast<float>(fish * 5 - 5)) * scale);
                    const int y = sea.Ground - Round((7.0f + static_cast<float>(fish) * 5.0f + static_cast<float>(group) * 2.0f) * scale + Sway(sea.Time, 0.5, phase) * 1.2f);
                    Box(sea.Paint, x, y, x + 3, y + 1, body);
                    Box(sea.Paint, x + 1, y - 1, x + 2, y, body);
                    const int back = facing > 0 ? x - 1 : x + 3;
                    Box(sea.Paint, back, y, back + 1, y + 1, tail);
                }
            }
        }

        void DrawNightGlow(const Sea& sea)
        {
            const float night = std::clamp(1.0f - sea.Light.Day * 1.8f, 0.0f, 1.0f);
            if (night <= 0.0f)
                return;
            const int top = sea.Ground - std::max(8, sea.Ground / 4);
            const auto edge = static_cast<int>(static_cast<float>(sea.Columns) * 0.1f);
            Scatter(sea.Columns, sea.Rows - top, 13, 6, 97u, 80u, [&](int x, int row, std::uint32_t mixed)
                {
                    const int y = top + row;
                    if (y < sea.Ground - 6 && x > edge && x < sea.Columns - edge)
                        return;
                    const double phase = static_cast<double>(mixed & 1023u) / 100.0;
                    const float glow = 0.5f + 0.5f * Sway(sea.Time, 0.9 + 0.1 * ((mixed >> 10) & 3u), phase);
                    const ImVec4 color = ((mixed >> 14) & 3u) == 0u ? Col(0.500f, 0.950f, 0.550f) : kGlow;
                    Box(sea.Paint, x, y, x + 1, y + 1, ManifoldTheme::WithAlpha(color, night * (0.25f + 0.6f * glow)));
                });
        }

        void FrontRock(const Sea& sea, int from, int to, int tall, std::uint32_t seed)
        {
            const Mood& mood = sea.Light;
            const ImVec4 body = mood.Of(kFrontRock);
            const ImVec4 rim = mood.Of(kFrontRockLit, 0.6f);
            const float middle = static_cast<float>(from + to) * 0.5f;
            const float half = static_cast<float>(to - from) * 0.5f;
            int start = from;
            int startTop = 0;
            for (int x = from; x <= to + 1; ++x)
            {
                int top = sea.Rows + 1;
                if (x <= to)
                {
                    const float across = (static_cast<float>(x) + 0.5f - middle) / half;
                    top = sea.Rows - Round(static_cast<float>(tall) * std::sqrt(std::max(0.0f, 1.0f - across * across)) + (Swell(static_cast<float>(x), 3.0f, seed) - 0.5f) * 2.0f);
                }
                if (x == from)
                {
                    startTop = top;
                    continue;
                }
                if (top == startTop)
                    continue;
                if (startTop < sea.Rows)
                {
                    Box(sea.Paint, start, startTop + 1, x, sea.Rows, body);
                    Box(sea.Paint, start, startTop, x, startTop + 1, rim);
                }
                start = x;
                startTop = top;
            }
        }
    }

    void DrawUnderwaterBack(const Frame& frame)
    {
        if (frame.Columns <= 0 || frame.Rows <= 0)
            return;
        const Sea sea = SeaOf(frame);
        const std::array<ImVec4, kToneCount> colors = ToneColors(sea.Light);
        const Wreck wreck = WreckOf(sea);
        DrawWater(sea, CliffTops(sea, wreck));
        DrawWreck(sea, wreck, colors);
        const ReefWall wall = ReefWallOf(sea, wreck);
        DrawKelpForest(sea, wall, false);
        DrawFish(sea);
        DrawReefWall(sea, wall);
        DrawWallCorals(sea, wall, colors);
        DrawKelpForest(sea, wall, true);
        DrawRays(sea);
        if (sea.Ground >= sea.Rows)
            return;
        DrawSand(sea);
        DrawCaustics(sea);
        DrawSandBits(sea);
        DrawSeaGrass(sea);
        DrawReef(sea);
        DrawReefFish(sea);
        DrawNightGlow(sea);
    }

    void DrawUnderwaterFront(const Frame& frame)
    {
        if (frame.Columns <= 0 || frame.Rows <= 0)
            return;
        const Sea sea = SeaOf(frame);
        const Mood& mood = sea.Light;
        const int columns = sea.Columns;
        const int wide = std::clamp(columns / 14, 10, 26);
        const int tall = sea.Game ? 4 : std::max(5, (sea.Rows - sea.Ground) / 2);
        FrontRock(sea, -3, wide, tall, 11u);
        FrontRock(sea, columns - wide * 2 / 3, columns + 3, tall - 1, 23u);
        const GrassTones grass{ mood.Of(Col(0.070f, 0.200f, 0.140f)), mood.Of(Col(0.130f, 0.320f, 0.190f)), mood.Of(Col(0.260f, 0.480f, 0.270f), 0.6f) };
        const int grassTall = sea.Game ? 6 : 9;
        DrawTuft(sea.Paint, wide / 2, sea.Rows - tall + 1, 4, grassTall, sea, 5u, grass);
        DrawTuft(sea.Paint, columns - wide / 2 - 4, sea.Rows - tall + 2, 3, grassTall - 1, sea, 9u, grass);
        if (!sea.Game)
        {
            const KelpTones kelp{ mood.Of(Col(0.170f, 0.200f, 0.080f)), mood.Of(Col(0.230f, 0.290f, 0.100f)), mood.Of(Col(0.360f, 0.440f, 0.160f), 0.6f),
                mood.Of(Col(0.420f, 0.380f, 0.150f), 0.6f) };
            DrawKelp(sea.Paint, 3, sea.Rows - tall + 2, Round(static_cast<float>(sea.Rows) * 0.62f), 1, sea, 0.4, kelp, true);
            DrawKelp(sea.Paint, columns - 5, sea.Rows - tall + 2, Round(static_cast<float>(sea.Rows) * 0.48f), 1, sea, 2.9, kelp, true);
        }
    }
}
