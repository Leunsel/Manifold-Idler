#include "UI/HomeScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace Idler::UI::HomeScene
{
    namespace
    {
        constexpr ImVec4 Col(float red, float green, float blue)
        {
            return ImVec4(red, green, blue, 1.0f);
        }

        struct AirColor
        {
            ImVec4 Day;
            ImVec4 Night;
            ImVec4 Low;
        };

        constexpr AirColor kZenith{ Col(0.160f, 0.340f, 0.600f), Col(0.022f, 0.038f, 0.095f), Col(0.170f, 0.200f, 0.400f) };
        constexpr AirColor kMidSky{ Col(0.270f, 0.480f, 0.710f), Col(0.045f, 0.070f, 0.145f), Col(0.420f, 0.360f, 0.550f) };
        constexpr AirColor kHorizon{ Col(0.520f, 0.690f, 0.800f), Col(0.090f, 0.125f, 0.210f), Col(0.930f, 0.620f, 0.440f) };
        constexpr AirColor kRidge{ Col(0.400f, 0.540f, 0.670f), Col(0.060f, 0.085f, 0.150f), Col(0.580f, 0.460f, 0.560f) };
        constexpr AirColor kRidgeLit{ Col(0.480f, 0.620f, 0.730f), Col(0.085f, 0.115f, 0.190f), Col(0.720f, 0.540f, 0.520f) };

        constexpr ImVec4 kGrassBack = Col(0.300f, 0.520f, 0.270f);
        constexpr ImVec4 kGrassFront = Col(0.170f, 0.390f, 0.160f);
        constexpr ImVec4 kGrassRim = Col(0.360f, 0.570f, 0.290f);
        constexpr ImVec4 kGrassRimLit = Col(0.470f, 0.660f, 0.340f);
        constexpr ImVec4 kGrassLight = Col(0.300f, 0.540f, 0.230f);
        constexpr ImVec4 kGrassDark = Col(0.120f, 0.300f, 0.130f);
        constexpr ImVec4 kGrassSeed = Col(0.720f, 0.680f, 0.400f);
        constexpr ImVec4 kHedge = Col(0.150f, 0.330f, 0.180f);
        constexpr ImVec4 kHedgeLit = Col(0.280f, 0.490f, 0.240f);
        constexpr ImVec4 kHedgeDark = Col(0.090f, 0.230f, 0.140f);
        constexpr ImVec4 kEarth = Col(0.300f, 0.210f, 0.120f);
        constexpr ImVec4 kEarthDark = Col(0.200f, 0.140f, 0.090f);
        constexpr ImVec4 kFieldLight = Col(0.500f, 0.660f, 0.380f);
        constexpr ImVec4 kFieldMid = Col(0.360f, 0.540f, 0.320f);
        constexpr ImVec4 kFieldGold = Col(0.760f, 0.690f, 0.380f);
        constexpr ImVec4 kFieldPale = Col(0.600f, 0.700f, 0.440f);
        constexpr ImVec4 kFieldHedge = Col(0.220f, 0.400f, 0.250f);
        constexpr ImVec4 kFieldRimLit = Col(0.620f, 0.760f, 0.460f);
        constexpr ImVec4 kWoods = Col(0.150f, 0.330f, 0.200f);
        constexpr ImVec4 kWoodsLit = Col(0.270f, 0.470f, 0.240f);
        constexpr ImVec4 kWoodsDark = Col(0.090f, 0.230f, 0.150f);
        constexpr ImVec4 kWaterEdge = Col(0.600f, 0.820f, 0.820f);
        constexpr ImVec4 kWaterNear = Col(0.110f, 0.380f, 0.480f);
        constexpr ImVec4 kWaterMid = Col(0.080f, 0.310f, 0.430f);
        constexpr ImVec4 kWaterDeep = Col(0.050f, 0.230f, 0.350f);
        constexpr ImVec4 kWaterShade = Col(0.070f, 0.250f, 0.240f);
        constexpr ImVec4 kRipple = Col(0.300f, 0.580f, 0.640f);
        constexpr ImVec4 kPad = Col(0.200f, 0.440f, 0.190f);
        constexpr ImVec4 kPadLit = Col(0.340f, 0.600f, 0.260f);
        constexpr ImVec4 kPadDark = Col(0.040f, 0.180f, 0.170f);
        constexpr ImVec4 kLily = Col(0.980f, 0.800f, 0.880f);
        constexpr ImVec4 kPoppy = Col(0.880f, 0.200f, 0.140f);
        constexpr ImVec4 kPoppyDark = Col(0.500f, 0.070f, 0.080f);
        constexpr ImVec4 kCornflower = Col(0.330f, 0.480f, 0.920f);
        constexpr ImVec4 kCornflowerDark = Col(0.180f, 0.260f, 0.600f);
        constexpr ImVec4 kChamomile = Col(0.960f, 0.960f, 0.900f);
        constexpr ImVec4 kButtercup = Col(0.980f, 0.840f, 0.240f);
        constexpr ImVec4 kStem = Col(0.160f, 0.360f, 0.140f);
        constexpr ImVec4 kReed = Col(0.250f, 0.420f, 0.170f);
        constexpr ImVec4 kReedLit = Col(0.420f, 0.580f, 0.260f);
        constexpr ImVec4 kCattail = Col(0.380f, 0.230f, 0.120f);
        constexpr ImVec4 kPebble = Col(0.470f, 0.460f, 0.440f);
        constexpr ImVec4 kPebbleLit = Col(0.640f, 0.630f, 0.590f);
        constexpr ImVec4 kPebbleDark = Col(0.300f, 0.300f, 0.290f);

        struct Mood
        {
            float Day = 1.0f;
            float Low = 0.0f;

            ImVec4 Of(ImVec4 day, float warmth = 0.45f) const
            {
                const ImVec4 night = warmth >= 0.6f ? ImVec4(day.x * 0.22f + 0.030f, day.y * 0.27f + 0.050f, day.z * 0.34f + 0.100f, 1.0f)
                                                    : ImVec4(day.x * 0.18f + 0.014f, day.y * 0.23f + 0.026f, day.z * 0.30f + 0.060f, 1.0f);
                const ImVec4 now = DayNight(day, night, Day);
                const ImVec4 warm(std::min(1.0f, now.x * 1.25f + 0.03f), now.y * 0.95f, now.z * 0.75f, 1.0f);
                return Mix(now, warm, Low * warmth);
            }

            ImVec4 Air(const AirColor& color) const
            {
                return Mix(DayNight(color.Day, color.Night, Day), color.Low, Low * 0.8f);
            }
        };

        Mood MoodOf(const Frame& frame)
        {
            Mood mood;
            mood.Day = std::clamp(frame.Daylight, 0.0f, 1.0f);
            mood.Low = std::clamp(1.0f - std::fabs(frame.Daylight - 0.5f) / 0.4f, 0.0f, 1.0f);
            return mood;
        }

        ImVec4 SkyAt(const Mood& mood, float share)
        {
            const ImVec4 zenith = mood.Air(kZenith);
            const ImVec4 middle = mood.Air(kMidSky);
            const ImVec4 horizon = mood.Air(kHorizon);
            return share < 0.55f ? Mix(zenith, middle, share / 0.55f) : Mix(middle, horizon, (share - 0.55f) / 0.45f);
        }

        ImVec4 HazeOf(const Mood& mood)
        {
            return Mix(SkyAt(mood, 1.0f), mood.Air(kRidge), 0.35f);
        }

        float BackHaze(float share)
        {
            return 0.4f * std::max(0.0f, 1.0f - share / 0.72f);
        }

        float SideOf(const Frame& frame, float x)
        {
            return frame.Sun.X < x ? -1.0f : 1.0f;
        }

        bool TowardLight(float offset, float side)
        {
            return offset * side > 0.0f;
        }

        bool Faces(int rise, float side)
        {
            return rise != 0 && (rise > 0) == (side < 0.0f);
        }

        int RiseOf(double slope)
        {
            return slope > 0.02 ? 1 : slope < -0.02 ? -1 : 0;
        }

        void Box(const Canvas& paint, int left, int top, int right, int bottom, ImVec4 color)
        {
            paint.Fill(static_cast<float>(left), static_cast<float>(top), static_cast<float>(right), static_cast<float>(bottom), color);
        }

        template <typename Cell>
        void PaintRows(const Canvas& paint, int columns, int top, int bottom, const ImVec4* colors, Cell&& cell)
        {
            for (int y = top; y < bottom; ++y)
            {
                int start = 0;
                int current = cell(0, y);
                for (int x = 1; x <= columns; ++x)
                {
                    const int next = x < columns ? cell(x, y) : -1;
                    if (x < columns && next == current)
                        continue;
                    if (current >= 0)
                        Box(paint, start, y, x, y + 1, colors[current]);
                    start = x;
                    current = next;
                }
            }
        }

        enum Tone : int
        {
            kEmpty,
            kLeafDeep,
            kLeafDark,
            kLeafMid,
            kLeafLight,
            kLeafSun,
            kBushDeep,
            kBushDark,
            kBushMid,
            kBushLight,
            kBushSun,
            kBarkCrack,
            kBarkDark,
            kBark,
            kBarkLit,
            kBloom,
            kBloomLit,
            kBloomEye,
            kHayDeep,
            kHayDark,
            kHay,
            kHayLit,
            kStoneDark,
            kStone,
            kStoneLit,
            kMoss,
            kMossLit,
            kPetalDark,
            kPetal,
            kPetalLit,
            kSeed,
            kSeedDark,
            kStalk,
            kBlade,
            kBladeLit,
            kRed,
            kRedDark,
            kToneCount
        };

        class Grid
        {
        public:
            static constexpr int kSize = 96;
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

            void Emit(const Canvas& paint, const ImVec4* colors, int atX = 0, int atY = 0) const
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

        void Clump(Grid& grid, float middleX, float middleY, float radius, float side, int back, int first, std::uint32_t seed)
        {
            const float lightX = side * 0.6f;
            const float lightY = -0.8f;
            const int top = static_cast<int>(std::floor(middleY - radius - 1.0f));
            const int bottom = static_cast<int>(std::ceil(middleY + radius));
            const int left = static_cast<int>(std::floor(middleX - radius - 1.0f));
            const int right = static_cast<int>(std::ceil(middleX + radius));
            for (int y = top; y <= bottom; ++y)
            {
                for (int x = left; x <= right; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - middleX;
                    const float dy = static_cast<float>(y) + 0.5f - middleY;
                    const std::uint32_t mixed = Noise(seed ^ (static_cast<std::uint32_t>(x + 512) * 73856093u) ^ (static_cast<std::uint32_t>(y + 512) * 19349663u));
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    float rim = radius;
                    if ((mixed & 15u) == 0u)
                        rim -= 0.9f;
                    else if (((mixed >> 3) & 7u) == 0u && dy < -radius * 0.3f)
                        rim += 0.9f;
                    if (distance > rim)
                        continue;
                    const float toward = (dx * lightX + dy * lightY) / std::max(0.5f, distance) + (static_cast<float>((mixed >> 8) & 15u) / 15.0f - 0.5f) * 0.06f;
                    const float rimShare = distance / radius;
                    int level = 2;
                    if (toward > 0.45f && rimShare > 0.5f)
                        level = 4;
                    else if (toward > -0.1f)
                        level = 3;
                    else if (toward < -0.55f && rimShare > 0.55f)
                        level = rimShare > 0.8f ? 0 : 1;
                    if (dy > radius * 0.6f)
                        level = std::min(level, 1);
                    if (back > 0)
                        level = level >= 3 ? 2 : 1;
                    grid.Set(x, y, first + std::max(0, level - back));
                }
            }
        }

        void Trunk(Grid& grid, int top, int half, float side, std::uint32_t seed, bool furrowed)
        {
            for (int y = -1; y >= top; --y)
            {
                const int flare = y >= -2 ? 2 : y >= -4 ? 1 : 0;
                const int left = -half - flare;
                const int right = half - 1 + flare;
                for (int x = left; x <= right; ++x)
                {
                    const int fromLight = side < 0.0f ? x - left : right - x;
                    const int fromShade = right - left - fromLight;
                    int tone = fromLight == 0 ? kBarkLit : fromShade <= (half >= 3 ? 1 : 0) ? kBarkDark : kBark;
                    const auto column = static_cast<std::uint32_t>(x + 64);
                    const int run = (-y + static_cast<int>(Noise(seed + column) % 4u)) / (furrowed ? 4 : 3);
                    const std::uint32_t mark = Noise(seed ^ (column * 977u + static_cast<std::uint32_t>(run) * 7919u));
                    if (furrowed && mark % 3u == 0u)
                        tone = fromLight == 0 ? kBark : tone == kBarkDark ? kBarkCrack : kBarkDark;
                    else if (!furrowed && mark % 7u == 0u && (-y) % 3 == 0 && fromLight > 0 && fromShade > 0)
                        tone = kBarkDark;
                    grid.Set(x, y, tone);
                }
            }
        }

        void Limb(Grid& grid, float fromX, float fromY, float toX, float toY, float width, float side)
        {
            const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(toX - fromX), std::fabs(toY - fromY)))));
            for (int step = 0; step <= steps; ++step)
            {
                const float share = static_cast<float>(step) / static_cast<float>(steps);
                const float x = fromX + (toX - fromX) * share;
                const float y = fromY + (toY - fromY) * share;
                const int thick = std::max(1, static_cast<int>(std::round(width * (1.0f - 0.4f * share))));
                const int left = static_cast<int>(std::floor(x - static_cast<float>(thick) * 0.5f + 0.5f));
                for (int cell = 0; cell < thick; ++cell)
                {
                    const bool lit = side < 0.0f ? cell == 0 : cell == thick - 1;
                    const bool dark = thick > 1 && (side < 0.0f ? cell == thick - 1 : cell == 0);
                    grid.Set(left + cell, static_cast<int>(std::floor(y)), lit ? kBarkLit : dark ? kBarkDark : kBark);
                }
            }
        }

        enum Kind : std::uint32_t
        {
            kOak,
            kLinden,
            kRose,
            kHayBales,
            kSunflowers,
            kBoulder,
        };

        struct PropSize
        {
            float Foot;
            float Cover;
            float CoverHeight;
            float Height;
            float Crown;
        };

        constexpr PropSize kSizes[] = {
            { 3.0f, 12.0f, 11.0f, 53.0f, 21.0f },
            { 2.0f, 0.0f, 0.0f, 57.0f, 19.0f },
            { 3.0f, 13.0f, 12.0f, 19.0f, 16.0f },
            { 7.0f, 11.0f, 10.0f, 13.0f, 12.0f },
            { 4.0f, 0.0f, 0.0f, 29.0f, 11.0f },
            { 5.0f, 0.0f, 0.0f, 9.0f, 7.0f },
        };

        struct PropPlace
        {
            Kind What;
            float Across;
            float Forward;
        };

        constexpr PropPlace kPlaces[] = {
            { kOak, 0.13f, 0.30f },
            { kHayBales, 0.31f, 0.72f },
            { kRose, 0.47f, 0.06f },
            { kSunflowers, 0.62f, 0.50f },
            { kLinden, 0.80f, 0.18f },
            { kBoulder, 0.92f, 0.86f },
        };

        constexpr PropPlace kNarrowPlaces[] = {
            { kOak, 0.10f, 0.22f },
            { kRose, 0.86f, 0.72f },
        };

        struct Leaves
        {
            float X;
            float Y;
            float Radius;
        };

        constexpr Leaves kOakBack[] = { { -12.0f, -36.0f, 7.0f }, { 11.0f, -37.0f, 7.0f }, { 0.0f, -44.0f, 7.0f }, { -5.0f, -47.0f, 5.0f }, { 6.0f, -46.0f, 5.0f },
            { -17.0f, -30.0f, 5.0f }, { 17.0f, -31.0f, 5.0f } };
        constexpr Leaves kOakFront[] = { { -2.0f, -41.0f, 6.0f }, { 6.0f, -42.0f, 5.0f }, { -9.0f, -41.0f, 5.0f }, { -7.0f, -33.0f, 7.0f }, { 7.0f, -35.0f, 7.0f },
            { -14.0f, -29.0f, 6.0f }, { 14.0f, -30.0f, 6.0f }, { 0.0f, -30.0f, 5.0f } };
        constexpr Leaves kOakBush[] = { { -4.0f, -9.0f, 5.0f }, { 4.0f, -9.0f, 5.0f }, { -8.0f, -6.0f, 6.0f }, { 8.0f, -6.0f, 6.0f }, { -13.0f, -4.0f, 4.0f },
            { 13.0f, -4.0f, 4.0f }, { 0.0f, -6.0f, 6.0f } };
        constexpr Leaves kLindenBack[] = { { -9.0f, -34.0f, 8.0f }, { 9.0f, -34.0f, 8.0f }, { 0.0f, -45.0f, 8.0f }, { -4.0f, -51.0f, 5.0f }, { 5.0f, -50.0f, 5.0f },
            { -14.0f, -27.0f, 5.0f }, { 14.0f, -27.0f, 5.0f } };
        constexpr Leaves kLindenFront[] = { { 0.0f, -50.0f, 5.0f }, { -6.0f, -43.0f, 6.0f }, { 6.0f, -44.0f, 6.0f }, { 0.0f, -38.0f, 7.0f }, { -6.0f, -31.0f, 7.0f },
            { 6.0f, -32.0f, 7.0f }, { -12.0f, -26.0f, 6.0f }, { 12.0f, -27.0f, 6.0f }, { 0.0f, -27.0f, 5.0f } };
        constexpr Leaves kRoseBack[] = { { -8.0f, -10.0f, 6.0f }, { 8.0f, -10.0f, 6.0f }, { 0.0f, -12.0f, 6.0f } };
        constexpr Leaves kRoseFront[] = { { -4.0f, -9.0f, 6.0f }, { 5.0f, -9.0f, 6.0f }, { -13.0f, -4.0f, 4.0f }, { 13.0f, -4.0f, 4.0f }, { -8.0f, -4.0f, 5.0f },
            { 8.0f, -4.0f, 5.0f }, { 0.0f, -5.0f, 6.0f } };
        constexpr int kRoses[][2] = { { -10, -10 }, { -3, -15 }, { 4, -12 }, { 10, -7 }, { -13, -4 }, { 13, -3 }, { -1, -6 }, { 7, -15 }, { -7, -4 } };

        struct Stalk
        {
            int X;
            int Height;
            int Head;
        };

        constexpr Stalk kStalks[] = { { -7, 15, 2 }, { -3, 21, 3 }, { 1, 24, 3 }, { 5, 18, 3 }, { 8, 12, 2 } };

        std::array<ImVec4, kToneCount> PropColors(const Mood& mood, std::uint32_t kind)
        {
            std::array<ImVec4, kToneCount> colors{};
            const auto set = [&](int tone, ImVec4 day, float warmth = 0.45f) { colors[static_cast<std::size_t>(tone)] = mood.Of(day, warmth); };
            if (kind == kLinden)
            {
                set(kLeafDeep, Col(0.080f, 0.210f, 0.100f));
                set(kLeafDark, Col(0.130f, 0.310f, 0.130f));
                set(kLeafMid, Col(0.220f, 0.440f, 0.160f));
                set(kLeafLight, Col(0.340f, 0.570f, 0.200f), 0.6f);
                set(kLeafSun, Col(0.540f, 0.720f, 0.280f), 0.8f);
                set(kBarkCrack, Col(0.200f, 0.200f, 0.190f));
                set(kBarkDark, Col(0.320f, 0.310f, 0.290f));
                set(kBark, Col(0.470f, 0.460f, 0.420f));
                set(kBarkLit, Col(0.640f, 0.630f, 0.570f), 0.7f);
                set(kBloom, Col(0.900f, 0.880f, 0.560f), 0.6f);
            }
            else if (kind == kSunflowers)
            {
                set(kLeafDeep, Col(0.090f, 0.230f, 0.090f));
                set(kLeafDark, Col(0.140f, 0.320f, 0.120f));
                set(kLeafMid, Col(0.220f, 0.450f, 0.160f));
                set(kLeafLight, Col(0.330f, 0.580f, 0.210f), 0.6f);
                set(kLeafSun, Col(0.480f, 0.690f, 0.270f), 0.8f);
            }
            else
            {
                set(kLeafDeep, Col(0.060f, 0.180f, 0.100f));
                set(kLeafDark, Col(0.100f, 0.270f, 0.130f));
                set(kLeafMid, Col(0.160f, 0.370f, 0.150f));
                set(kLeafLight, Col(0.260f, 0.490f, 0.190f), 0.6f);
                set(kLeafSun, Col(0.430f, 0.640f, 0.250f), 0.8f);
                set(kBarkCrack, Col(0.150f, 0.100f, 0.070f));
                set(kBarkDark, Col(0.230f, 0.160f, 0.100f));
                set(kBark, Col(0.360f, 0.260f, 0.160f));
                set(kBarkLit, Col(0.520f, 0.380f, 0.240f), 0.7f);
                set(kBloom, Col(0.950f, 0.550f, 0.700f));
            }
            set(kBushDeep, Col(0.050f, 0.160f, 0.110f));
            set(kBushDark, Col(0.090f, 0.250f, 0.150f));
            set(kBushMid, Col(0.150f, 0.350f, 0.180f));
            set(kBushLight, Col(0.240f, 0.470f, 0.210f), 0.6f);
            set(kBushSun, Col(0.380f, 0.600f, 0.270f), 0.8f);
            set(kBloomLit, Col(1.000f, 0.780f, 0.860f), 0.7f);
            set(kBloomEye, Col(0.980f, 0.850f, 0.300f));
            set(kHayDeep, Col(0.400f, 0.280f, 0.120f));
            set(kHayDark, Col(0.580f, 0.440f, 0.200f));
            set(kHay, Col(0.770f, 0.620f, 0.300f));
            set(kHayLit, Col(0.920f, 0.790f, 0.440f), 0.8f);
            set(kStoneDark, Col(0.320f, 0.320f, 0.310f));
            set(kStone, Col(0.500f, 0.490f, 0.470f));
            set(kStoneLit, Col(0.670f, 0.660f, 0.620f), 0.8f);
            set(kMoss, Col(0.280f, 0.440f, 0.180f));
            set(kMossLit, Col(0.440f, 0.600f, 0.240f), 0.7f);
            set(kPetalDark, Col(0.800f, 0.500f, 0.060f));
            set(kPetal, Col(0.960f, 0.740f, 0.100f));
            set(kPetalLit, Col(1.000f, 0.880f, 0.320f), 0.7f);
            set(kSeed, Col(0.380f, 0.230f, 0.100f));
            set(kSeedDark, Col(0.220f, 0.130f, 0.060f));
            set(kStalk, Col(0.240f, 0.440f, 0.150f));
            set(kBlade, Col(0.240f, 0.480f, 0.180f));
            set(kBladeLit, Col(0.400f, 0.640f, 0.270f), 0.6f);
            set(kRed, kPoppy);
            set(kRedDark, kPoppyDark);
            return colors;
        }

        template <std::size_t Count>
        void Clumps(Grid& grid, const Leaves (&leaves)[Count], float side, int back, int first, std::uint32_t seed)
        {
            std::uint32_t index = 0;
            for (const Leaves& clump : leaves)
                Clump(grid, clump.X, clump.Y, clump.Radius, side, back, first, seed + ++index * 101u);
        }

        void GrassAt(Grid& grid, int from, int to, float side, std::uint32_t seed)
        {
            for (int x = from; x <= to; ++x)
            {
                const std::uint32_t mixed = Noise(seed + static_cast<std::uint32_t>(x + 64) * 31u);
                if (mixed % 3u == 0u)
                    continue;
                const int tall = 1 + static_cast<int>((mixed >> 4) % 3u);
                for (int y = -1; y >= -tall; --y)
                {
                    if (grid.At(x, y) == kEmpty)
                        grid.Set(x, y, TowardLight(static_cast<float>(x), side) && y == -tall ? kBladeLit : kBlade);
                }
            }
        }

        void DrawOak(Grid& grid, float side, std::uint32_t seed)
        {
            Clumps(grid, kOakBack, side, 1, kLeafDeep, seed);
            Trunk(grid, -26, 3, side, seed, true);
            Limb(grid, -1.0f, -24.0f, -10.0f, -33.0f, 3.0f, side);
            Limb(grid, 1.0f, -24.0f, 10.0f, -34.0f, 3.0f, side);
            Limb(grid, 0.0f, -25.0f, 1.0f, -40.0f, 2.0f, side);
            Clumps(grid, kOakFront, side, 0, kLeafDeep, seed + 7u);
            Clumps(grid, kOakBush, side, 0, kBushDeep, seed + 13u);
        }

        void DrawLinden(Grid& grid, float side, std::uint32_t seed)
        {
            Clumps(grid, kLindenBack, side, 1, kLeafDeep, seed);
            Trunk(grid, -24, 2, side, seed, false);
            Limb(grid, -1.0f, -22.0f, -7.0f, -30.0f, 2.0f, side);
            Limb(grid, 1.0f, -22.0f, 7.0f, -31.0f, 2.0f, side);
            Limb(grid, 0.0f, -23.0f, 0.0f, -38.0f, 2.0f, side);
            Clumps(grid, kLindenFront, side, 0, kLeafDeep, seed + 7u);
            for (int y = -57; y <= -20; ++y)
            {
                for (int x = -20; x <= 20; ++x)
                {
                    const int tone = grid.At(x, y);
                    if ((tone == kLeafLight || tone == kLeafSun) && Noise(seed ^ (static_cast<std::uint32_t>(x + 64) * 2246822519u) ^ (static_cast<std::uint32_t>(y + 64) * 3266489917u)) % 15u == 0u)
                        grid.Set(x, y, kBloom);
                }
            }
            GrassAt(grid, -6, 5, side, seed);
        }

        void DrawRose(Grid& grid, float side, std::uint32_t seed)
        {
            Clumps(grid, kRoseBack, side, 1, kBushDeep, seed);
            Clumps(grid, kRoseFront, side, 0, kBushDeep, seed + 7u);
            for (const auto& rose : kRoses)
            {
                const int x = rose[0];
                const int y = rose[1];
                if (grid.At(x, y) == kEmpty)
                    continue;
                grid.Set(x, y, kBloomEye);
                grid.Set(x - 1, y, side < 0.0f ? kBloomLit : kBloom);
                grid.Set(x + 1, y, side < 0.0f ? kBloom : kBloomLit);
                grid.Set(x, y - 1, kBloomLit);
                grid.Set(x, y + 1, kBloom);
            }
        }

        void DrawHay(Grid& grid, float side, std::uint32_t seed)
        {
            for (int y = -12; y <= -1; ++y)
            {
                for (int x = 0; x <= 12; ++x)
                {
                    const bool corner = (y == -12 && (x <= 1 || x >= 11)) || ((y == -11 || y == -1) && (x == 0 || x == 12));
                    if (corner)
                        continue;
                    const int fromTop = y + 12;
                    int tone = fromTop <= 2 ? kHayLit : fromTop >= 9 ? kHayDark : kHay;
                    if (side < 0.0f ? x >= 11 : x <= 1)
                        tone = tone == kHayLit ? kHay : kHayDark;
                    if (fromTop % 3 == 1 && Noise(seed + static_cast<std::uint32_t>(x) * 31u + static_cast<std::uint32_t>(y + 64) * 977u) % 3u == 0u)
                        tone = tone == kHayLit ? kHay : kHayDark;
                    grid.Set(x, y, tone);
                }
            }
            constexpr float kMiddleX = -5.0f;
            constexpr float kMiddleY = -6.5f;
            constexpr float kRadius = 6.4f;
            for (int y = -13; y <= -1; ++y)
            {
                for (int x = -12; x <= 2; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - kMiddleX;
                    const float dy = static_cast<float>(y) + 0.5f - kMiddleY;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance > kRadius)
                        continue;
                    const float turn = distance + std::atan2(dy, dx) / 6.2832f * 1.8f;
                    const int ring = static_cast<int>(std::floor(turn / 1.8f));
                    int tone = ((ring % 2) + 2) % 2 == 0 ? kHay : kHayDark;
                    if (distance < 1.2f)
                        tone = kHayDeep;
                    if (distance > kRadius - 1.3f)
                    {
                        const float light = (dx * side * 0.6f - dy * 0.8f) / kRadius;
                        tone = light > 0.25f ? kHayLit : dy > 2.5f ? kHayDeep : kHay;
                    }
                    grid.Set(x, y, tone);
                }
            }
            for (int x = -14; x <= 14; ++x)
            {
                const std::uint32_t mixed = Noise(seed + static_cast<std::uint32_t>(x + 64) * 7u);
                if (grid.At(x, -1) == kEmpty && mixed % 3u == 0u)
                    grid.Set(x, -1, mixed % 2u == 0u ? kHay : kHayLit);
            }
        }

        void DrawSunflowers(Grid& grid, float side, std::uint32_t seed)
        {
            bool left = (seed & 1u) != 0u;
            for (const Stalk& stalk : kStalks)
            {
                for (int y = -1; y >= -stalk.Height; --y)
                    grid.Set(stalk.X, y, kStalk);
                for (int y = -3; y > -stalk.Height + 4; y -= 4)
                {
                    const int way = left ? -1 : 1;
                    const bool lit = (way < 0) == (side < 0.0f);
                    grid.Set(stalk.X + way, y - 1, lit ? kLeafSun : kLeafMid);
                    grid.Set(stalk.X + 2 * way, y - 1, lit ? kLeafLight : kLeafMid);
                    grid.Set(stalk.X + way, y, kLeafDark);
                    grid.Set(stalk.X + 2 * way, y, lit ? kLeafLight : kLeafDark);
                    grid.Set(stalk.X + 3 * way, y, lit ? kLeafMid : kLeafDeep);
                    left = !left;
                }
                const float middleX = static_cast<float>(stalk.X) + 0.5f;
                const float middleY = static_cast<float>(-stalk.Height - 1) + 0.5f;
                const float petals = static_cast<float>(stalk.Head) + 1.4f;
                const float seedsX = middleX + side * 0.8f;
                const float seeds = static_cast<float>(stalk.Head) - 0.5f;
                for (int y = static_cast<int>(std::floor(middleY - petals)); y <= static_cast<int>(std::ceil(middleY + petals)); ++y)
                {
                    for (int x = static_cast<int>(std::floor(middleX - petals)); x <= static_cast<int>(std::ceil(middleX + petals)); ++x)
                    {
                        const float dx = static_cast<float>(x) + 0.5f - middleX;
                        const float dy = static_cast<float>(y) + 0.5f - middleY;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance > petals)
                            continue;
                        const std::uint32_t mixed = Noise(seed ^ (static_cast<std::uint32_t>(x + 64) * 73856093u) ^ (static_cast<std::uint32_t>(y + 64) * 19349663u));
                        const float sx = static_cast<float>(x) + 0.5f - seedsX;
                        if (std::sqrt(sx * sx + dy * dy) <= seeds)
                        {
                            grid.Set(x, y, mixed % 3u == 0u ? kSeedDark : kSeed);
                            continue;
                        }
                        if (distance > petals - 0.8f && mixed % 4u == 0u)
                            continue;
                        const float light = (dx * side * 0.6f - dy * 0.8f) / petals;
                        grid.Set(x, y, light > 0.3f ? kPetalLit : light < -0.45f ? kPetalDark : kPetal);
                    }
                }
            }
            GrassAt(grid, -9, 10, side, seed);
        }

        void DrawStone(Grid& grid, float side, std::uint32_t seed)
        {
            constexpr int kHalf[] = { 6, 7, 7, 7, 6, 6, 5, 4, 2 };
            constexpr int kLean[] = { 0, 0, 0, 0, 1, 1, 1, 0, 0 };
            constexpr int kRows = static_cast<int>(std::size(kHalf));
            for (int row = 0; row < kRows; ++row)
            {
                const int y = -1 - row;
                const int left = -kHalf[row] + kLean[row];
                const int right = kHalf[row] - 1 + kLean[row];
                for (int x = left; x <= right; ++x)
                {
                    const float offset = (static_cast<float>(x) + 0.5f) / 7.0f;
                    const float light = offset * side * 0.7f + static_cast<float>(row) / static_cast<float>(kRows - 1) * 0.8f - 0.35f;
                    int tone = light > 0.3f ? kStoneLit : light > -0.2f ? kStone : kStoneDark;
                    if (row == 0)
                        tone = kStoneDark;
                    const std::uint32_t mixed = Noise(seed ^ (static_cast<std::uint32_t>(x + 64) * 73856093u) ^ (static_cast<std::uint32_t>(row) * 19349663u));
                    if (row >= 6 && mixed % 4u != 0u)
                        tone = TowardLight(offset, side) ? kMossLit : kMoss;
                    else if (row == 5 && mixed % 3u == 0u)
                        tone = kMoss;
                    else if (row >= 2 && mixed % 17u == 0u)
                        tone = kMossLit;
                    grid.Set(x, y, tone);
                }
            }
            grid.Set(1, -3, kStoneDark);
            grid.Set(2, -4, kStoneDark);
            grid.Set(2, -5, kStoneDark);
            const int poppy = side < 0.0f ? 8 : -9;
            for (int y = -1; y >= -4; --y)
                grid.Set(poppy, y, kStalk);
            grid.Set(poppy, -5, kRed);
            grid.Set(poppy + 1, -5, kRed);
            grid.Set(poppy, -6, kRed);
            grid.Set(poppy + 1, -6, kRedDark);
            GrassAt(grid, -9, 9, side, seed);
        }

        class SummerScene final : public Scene
        {
        public:
            void DrawBack(const Frame& frame) override
            {
                if (frame.Columns <= 0 || frame.Rows <= 0)
                    return;
                Measure(frame);
                const Mood mood = MoodOf(frame);
                const ImVec4 haze = HazeOf(mood);
                DrawAir(frame, mood);
                DrawDistance(frame, mood, haze);
                DrawLand(frame, mood, haze);
                DrawLake(frame, mood);
                DrawBackTrees(frame, mood, haze);
                DrawMeadow(frame, mood, haze);
                DrawFlowers(frame, mood, haze);
                DrawWater(frame, mood);
            }

            void PlaceProps(const Frame& frame, std::vector<Prop>& props) override
            {
                const auto width = static_cast<float>(frame.Columns);
                const float back = std::ceil(frame.FeetBack + 2.0f);
                const float front = std::max(back, std::floor(frame.FeetFront - 6.0f));
                const std::uint32_t salt = Noise(static_cast<std::uint32_t>(frame.Columns) * 2654435761u ^ static_cast<std::uint32_t>(frame.Rows) * 40503u);
                std::uint32_t index = 0;
                for (const PropPlace& place : frame.Narrow ? std::span<const PropPlace>(kNarrowPlaces) : std::span<const PropPlace>(kPlaces))
                {
                    const std::uint32_t mixed = Noise(salt + ++index * 7919u);
                    const PropSize& size = kSizes[place.What];
                    const float across = place.Across + (static_cast<float>(mixed & 255u) / 255.0f - 0.5f) * 0.03f;
                    const float forward = std::clamp(place.Forward + (static_cast<float>((mixed >> 8) & 255u) / 255.0f - 0.5f) * 0.08f, 0.0f, 1.0f);
                    Prop prop;
                    prop.X = std::clamp(std::round(width * across), 8.0f, std::max(8.0f, width - 9.0f));
                    prop.Ground = std::clamp(std::round(back + (front - back) * forward), back, front);
                    prop.Foot = size.Foot;
                    prop.Cover = size.Cover;
                    prop.CoverHeight = size.CoverHeight;
                    prop.Height = size.Height;
                    prop.Crown = size.Crown;
                    prop.Kind = place.What;
                    prop.Seed = Noise(mixed + 11u);
                    props.push_back(prop);
                }
            }

            void DrawProp(const Frame& frame, const Prop& prop, const Canvas& paint) override
            {
                const Mood mood = MoodOf(frame);
                const float side = SideOf(frame, prop.X);
                const std::array<ImVec4, kToneCount> colors = PropColors(mood, prop.Kind);
                Grid grid;
                switch (prop.Kind)
                {
                case kOak:
                    DrawOak(grid, side, prop.Seed);
                    break;
                case kLinden:
                    DrawLinden(grid, side, prop.Seed);
                    break;
                case kRose:
                    DrawRose(grid, side, prop.Seed);
                    break;
                case kHayBales:
                    DrawHay(grid, side, prop.Seed);
                    break;
                case kSunflowers:
                    DrawSunflowers(grid, side, prop.Seed);
                    break;
                default:
                    DrawStone(grid, side, prop.Seed);
                    break;
                }
                grid.Emit(paint, colors.data());
            }

            void DrawFront(const Frame& frame) override
            {
                const Mood mood = MoodOf(frame);
                const Canvas& paint = frame.Paint;
                const int shore = static_cast<int>(frame.Shore);
                const ImVec4 blades[3] = { mood.Of(Col(0.120f, 0.300f, 0.120f)), mood.Of(Col(0.240f, 0.490f, 0.180f)), mood.Of(Col(0.400f, 0.640f, 0.270f), 0.6f) };
                const ImVec4 ripe = mood.Of(kGrassSeed, 0.6f);
                const ImVec4 stem = mood.Of(kStem);
                const ImVec4 petals[3] = { mood.Of(kPoppy), mood.Of(kCornflower), mood.Of(kChamomile) };
                const ImVec4 hearts[3] = { mood.Of(kPoppyDark), mood.Of(kCornflowerDark), mood.Of(kButtercup) };
                const ImVec4 pebble = mood.Of(kPebble);
                const ImVec4 pebbleLit = mood.Of(kPebbleLit, 0.7f);
                const ImVec4 pebbleDark = mood.Of(kPebbleDark);
                for (int column = 3; column < frame.Columns - 3;)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(column) * 131u + 977u);
                    const std::uint32_t kind = (mixed >> 4) % 16u;
                    const float side = SideOf(frame, static_cast<float>(column));
                    if (kind < 9u)
                    {
                        const int count = 2 + static_cast<int>((mixed >> 8) % 3u);
                        for (int blade = 0; blade < count; ++blade)
                        {
                            const std::uint32_t own = Noise(mixed + static_cast<std::uint32_t>(blade) * 31u);
                            const int x = column + blade;
                            const int tall = 2 + static_cast<int>(own % 4u);
                            const int lean = tall >= 4 ? (blade == 0 ? -1 : blade == count - 1 ? 1 : 0) : 0;
                            const ImVec4 color = blade == 1 ? blades[0] : ((blade == 0) == (side < 0.0f)) ? blades[2] : blades[1];
                            Box(paint, x, shore - tall + (lean != 0 ? 2 : 0), x + 1, shore, color);
                            if (lean != 0)
                                Box(paint, x + lean, shore - tall, x + lean + 1, shore - tall + 2, color);
                            if ((own >> 20) % 7u == 0u && lean == 0)
                                Box(paint, x, shore - tall - 1, x + 1, shore - tall + 1, ripe);
                        }
                    }
                    else if (kind < 11u)
                    {
                        const std::uint32_t which = (mixed >> 12) % 3u;
                        const int x = column;
                        const int top = shore - 3 - static_cast<int>((mixed >> 16) % 3u);
                        Box(paint, x, top + 1, x + 1, shore, stem);
                        Box(paint, x + 1, shore - 2, x + 2, shore, blades[1]);
                        if (which == 0u)
                        {
                            Box(paint, x - 1, top - 1, x + 1, top + 1, petals[0]);
                            Box(paint, x, top, x + 1, top + 1, hearts[0]);
                        }
                        else
                        {
                            Box(paint, x - 1, top, x + 2, top + 1, petals[which]);
                            Box(paint, x, top - 1, x + 1, top, petals[which]);
                            Box(paint, x, top, x + 1, top + 1, hearts[which]);
                        }
                    }
                    else if (kind == 11u)
                    {
                        const int x = column;
                        Box(paint, x, shore - 2, x + 4, shore + 1, pebble);
                        Box(paint, x + 1, shore - 3, x + 3, shore - 2, pebble);
                        Box(paint, side < 0.0f ? x + 1 : x + 2, shore - 3, side < 0.0f ? x + 2 : x + 3, shore - 2, pebbleLit);
                        Box(paint, side < 0.0f ? x : x + 3, shore - 2, side < 0.0f ? x + 1 : x + 4, shore - 1, pebbleLit);
                        Box(paint, x, shore, x + 4, shore + 1, pebbleDark);
                    }
                    column += 4 + static_cast<int>(mixed % 6u);
                }

                const ImVec4 reed = mood.Of(kReed);
                const ImVec4 reedLit = mood.Of(kReedLit, 0.6f);
                const ImVec4 cattail = mood.Of(kCattail);
                for (int end = 0; end < 2; ++end)
                {
                    const int from = end == 0 ? 2 : frame.Columns - 15;
                    for (int stalk = 0; stalk < 6; ++stalk)
                    {
                        const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(stalk) * 97u + static_cast<std::uint32_t>(end) * 7u + 5u);
                        const int x = from + stalk * 2 + static_cast<int>(mixed % 2u);
                        const int tall = 4 + static_cast<int>((mixed >> 4) % 3u);
                        const int base = shore + 3;
                        Box(paint, x, shore - tall, x + 1, base, reed);
                        if ((mixed >> 8) % 2u == 0u)
                            Box(paint, x, shore - tall + 1, x + 1, shore - tall + 4, cattail);
                        else
                            Box(paint, x + 1, shore - tall + 2, x + 2, base - 1, reedLit);
                    }
                }
            }

            ImVec4 HazeColor(const Frame& frame) override
            {
                return HazeOf(MoodOf(frame));
            }

        private:
            std::vector<int> m_ridge;
            std::vector<int> m_fields;
            std::vector<int> m_woods;
            std::vector<int> m_woodsMiddle;
            std::vector<int> m_poplar;
            std::vector<int> m_hedge;
            std::vector<int> m_hedgeMiddle;
            std::vector<int> m_land;
            std::vector<int> m_water;
            std::vector<int> m_ridgeRise;
            std::vector<int> m_fieldsRise;
            std::vector<int> m_landRise;
            int m_columns = -1;
            float m_surface = -1.0f;
            float m_shore = -1.0f;

            void Measure(const Frame& frame)
            {
                if (frame.Columns == m_columns && frame.Surface == m_surface && frame.Shore == m_shore)
                    return;
                m_columns = frame.Columns;
                m_surface = frame.Surface;
                m_shore = frame.Shore;
                const int columns = frame.Columns;
                const auto count = static_cast<std::size_t>(columns);
                const int surface = static_cast<int>(frame.Surface);
                const int shore = static_cast<int>(frame.Shore);
                m_ridge.assign(count, 0);
                m_fields.assign(count, 0);
                m_woods.assign(count, surface + 8);
                m_woodsMiddle.assign(count, 0);
                m_poplar.assign(count, 0);
                m_hedge.assign(count, surface + 8);
                m_hedgeMiddle.assign(count, 0);
                m_land.assign(count, surface);
                m_water.assign(count, shore + 1);
                m_ridgeRise.assign(count, 0);
                m_fieldsRise.assign(count, 0);
                m_landRise.assign(count, 0);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto at = static_cast<double>(i);
                    m_ridge[i] = surface - static_cast<int>(std::lround(16.0 + 5.0 * std::sin(at * 0.018 + 0.4) + 3.0 * std::sin(at * 0.051 + 1.9) + 1.0 * std::sin(at * 0.14 + 0.3)));
                    m_fields[i] = surface - static_cast<int>(std::lround(9.0 + 4.5 * std::sin(at * 0.029 + 1.3) + 2.5 * std::sin(at * 0.077 + 0.2) + 0.8 * std::sin(at * 0.21)));
                    m_land[i] = surface + static_cast<int>(std::lround(1.6 * std::sin(at * 0.17) + 0.8 * std::sin(at * 0.43 + 1.0)));
                    m_water[i] = shore + 1 + static_cast<int>(std::lround(std::fabs(std::sin(at * 0.11 + 2.0))));
                    m_ridgeRise[i] = RiseOf(0.09 * std::cos(at * 0.018 + 0.4) + 0.153 * std::cos(at * 0.051 + 1.9) + 0.14 * std::cos(at * 0.14 + 0.3));
                    m_fieldsRise[i] = RiseOf(0.1305 * std::cos(at * 0.029 + 1.3) + 0.1925 * std::cos(at * 0.077 + 0.2) + 0.168 * std::cos(at * 0.21));
                    m_landRise[i] = RiseOf(-(0.272 * std::cos(at * 0.17) + 0.344 * std::cos(at * 0.43 + 1.0)));
                    m_woodsMiddle[i] = static_cast<int>(i);
                    m_hedgeMiddle[i] = static_cast<int>(i);
                }
                const auto raise = [&](std::vector<int>& line, std::vector<int>& middles, int x, int top, int middle, int poplar)
                    {
                        if (x < 0 || x >= columns)
                            return;
                        const auto i = static_cast<std::size_t>(x);
                        if (top < line[i])
                        {
                            line[i] = top;
                            middles[i] = middle;
                            if (&line == &m_woods)
                                m_poplar[i] = poplar;
                        }
                    };
                for (int crown = -2; crown * 4 < columns + 8; ++crown)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(crown + 64) * 2654435761u + 17u);
                    const bool tall = mixed % 7u == 0u;
                    const float radius = tall ? 5.0f : 2.5f + static_cast<float>((mixed >> 4) % 3u);
                    const int middle = crown * 4 + static_cast<int>((mixed >> 8) % 3u);
                    const float rise = tall ? 5.0f : 2.0f + static_cast<float>((mixed >> 12) % 4u);
                    for (int x = middle - static_cast<int>(radius); x <= middle + static_cast<int>(radius); ++x)
                    {
                        const auto dx = static_cast<float>(x - middle);
                        raise(m_woods, m_woodsMiddle, x, surface - static_cast<int>(std::lround(rise + std::sqrt(std::max(0.0f, radius * radius - dx * dx)))), middle, 0);
                    }
                }
                for (int group = 0; group * 53 < columns; ++group)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(group) * 7717u + 3u);
                    if (mixed % 3u == 0u)
                        continue;
                    const int trees = 1 + static_cast<int>((mixed >> 4) % 3u);
                    for (int tree = 0; tree < trees; ++tree)
                    {
                        const int middle = group * 53 + static_cast<int>((mixed >> 8) % 36u) + tree * 4;
                        const int height = 11 + static_cast<int>(Noise(mixed + static_cast<std::uint32_t>(tree)) % 5u);
                        for (int dx = -2; dx <= 2; ++dx)
                            raise(m_woods, m_woodsMiddle, middle + dx, surface - height + (dx == 0 ? 0 : std::abs(dx) == 1 ? 2 : 7), middle, 1);
                    }
                }
                for (int bush = -1; bush * 26 < columns + 26; ++bush)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(bush + 16) * 4099u + 101u);
                    if (mixed % 5u < 2u)
                        continue;
                    const int middle = bush * 26 + static_cast<int>((mixed >> 4) % 14u);
                    const float half = 4.0f + static_cast<float>((mixed >> 8) % 5u);
                    const float high = 3.0f + static_cast<float>((mixed >> 12) % 4u);
                    for (int x = middle - static_cast<int>(half); x <= middle + static_cast<int>(half); ++x)
                    {
                        const float share = static_cast<float>(x - middle) / (half + 0.5f);
                        raise(m_hedge, m_hedgeMiddle, x, surface + 2 - static_cast<int>(std::lround(high * std::sqrt(std::max(0.0f, 1.0f - share * share)))), middle, 0);
                    }
                }
            }

            void DrawAir(const Frame& frame, const Mood& mood) const
            {
                constexpr int kBands = 16;
                std::array<ImVec4, kBands> colors{};
                for (int band = 0; band < kBands; ++band)
                    colors[static_cast<std::size_t>(band)] = SkyAt(mood, static_cast<float>(band) / static_cast<float>(kBands - 1));
                const float horizon = std::max(8.0f, frame.Surface - 6.0f);
                PaintRows(frame.Paint, frame.Columns, 0, static_cast<int>(frame.Surface) + 4, colors.data(), [&](int, int y) -> int
                    {
                        const float share = std::clamp(static_cast<float>(y) / horizon, 0.0f, 1.0f);
                        const float at = std::pow(share, 1.6f) * static_cast<float>(kBands - 1);
                        int band = static_cast<int>(at);
                        if (at - static_cast<float>(band) > 0.6f && (y & 1) == 0)
                            ++band;
                        return std::min(band, kBands - 1);
                    });
                const ImVec4 sky = colors[3];
                DrawSky(frame, ImVec4(sky.x, sky.y, sky.z, 0.0f));
                DrawCloudBank(frame, mood, 0.50f, 1.0f, 3u);
                DrawCloudBank(frame, mood, 0.94f, 0.6f, 9u);
            }

            void DrawCloudBank(const Frame& frame, const Mood& mood, float across, float size, std::uint32_t seed) const
            {
                struct Puff
                {
                    float X;
                    float Y;
                    float Radius;
                };
                constexpr Puff kPuffs[] = { { -2.0f, -16.0f, 11.0f }, { -14.0f, -11.0f, 9.0f }, { 11.0f, -12.0f, 9.0f }, { -24.0f, -6.0f, 7.0f }, { 21.0f, -7.0f, 7.0f },
                    { -31.0f, -3.0f, 5.0f }, { 30.0f, -3.0f, 5.0f }, { -8.0f, -5.0f, 7.0f }, { 6.0f, -5.0f, 8.0f } };
                constexpr AirColor kCloudLit{ Col(0.940f, 0.950f, 0.960f), Col(0.210f, 0.240f, 0.330f), Col(1.000f, 0.800f, 0.650f) };
                constexpr AirColor kCloudBody{ Col(0.820f, 0.865f, 0.920f), Col(0.120f, 0.150f, 0.235f), Col(0.880f, 0.640f, 0.620f) };
                constexpr AirColor kCloudShade{ Col(0.660f, 0.740f, 0.840f), Col(0.075f, 0.100f, 0.175f), Col(0.620f, 0.480f, 0.600f) };
                const ImVec4 horizon = SkyAt(mood, 0.9f);
                std::array<ImVec4, 4> colors{};
                colors[1] = Mix(mood.Air(kCloudShade), horizon, 0.3f);
                colors[2] = Mix(mood.Air(kCloudBody), horizon, 0.25f);
                colors[3] = Mix(mood.Air(kCloudLit), horizon, 0.2f);
                const int middle = static_cast<int>(std::round(static_cast<float>(frame.Columns) * across));
                const float side = SideOf(frame, static_cast<float>(middle));
                const float lightX = side * 0.6f;
                const float lightY = -0.8f;
                Grid grid;
                std::uint32_t index = 0;
                for (const Puff& puff : kPuffs)
                {
                    const float puffX = puff.X * size;
                    const float puffY = puff.Y * size;
                    const float radius = puff.Radius * size;
                    for (int y = static_cast<int>(std::floor(puffY - radius)); y <= -1; ++y)
                    {
                        for (int x = static_cast<int>(std::floor(puffX - radius)); x <= static_cast<int>(std::ceil(puffX + radius)); ++x)
                        {
                            const float dx = static_cast<float>(x) + 0.5f - puffX;
                            const float dy = static_cast<float>(y) + 0.5f - puffY;
                            const float distance = std::sqrt(dx * dx + dy * dy);
                            const std::uint32_t mixed = Noise(seed ^ (++index * 2654435761u));
                            if (distance > radius - ((mixed & 7u) == 0u ? 0.8f : 0.0f))
                                continue;
                            const float toward = (dx * lightX + dy * lightY) / std::max(0.5f, distance);
                            int tone = 2;
                            if (toward > 0.35f && distance > radius * 0.55f)
                                tone = 3;
                            else if (dy > radius * 0.3f || y >= -2)
                                tone = 1;
                            grid.Set(x, y, tone);
                        }
                    }
                }
                grid.Emit(frame.Paint, colors.data(), middle, static_cast<int>(frame.Surface) - 11);
            }

            void DrawDistance(const Frame& frame, const Mood& mood, const ImVec4& haze) const
            {
                enum : int
                {
                    kFar,
                    kFarLit,
                    kFarMist,
                    kHillTop,
                    kHillTopLit,
                    kHillLight,
                    kHillMid,
                    kHillGold,
                    kHillPale,
                    kHillHedge,
                    kWood,
                    kWoodLit,
                    kWoodDark,
                    kDistanceColors
                };
                std::array<ImVec4, kDistanceColors> colors{};
                colors[kFar] = mood.Air(kRidge);
                colors[kFarLit] = mood.Air(kRidgeLit);
                colors[kFarMist] = Mix(colors[kFar], SkyAt(mood, 1.0f), 0.45f);
                const auto field = [&](ImVec4 day) { return Mix(mood.Of(day), haze, 0.5f); };
                colors[kHillTop] = field(kFieldMid);
                colors[kHillTopLit] = field(kFieldRimLit);
                colors[kHillLight] = field(kFieldLight);
                colors[kHillMid] = field(kFieldMid);
                colors[kHillGold] = field(kFieldGold);
                colors[kHillPale] = field(kFieldPale);
                colors[kHillHedge] = field(kFieldHedge);
                const auto wood = [&](ImVec4 day) { return Mix(mood.Of(day), haze, 0.3f); };
                colors[kWood] = wood(kWoods);
                colors[kWoodLit] = wood(kWoodsLit);
                colors[kWoodDark] = wood(kWoodsDark);
                constexpr int kFieldKinds[] = { kHillLight, kHillMid, kHillGold, kHillLight, kHillMid, kHillPale };

                const int surface = static_cast<int>(frame.Surface);
                const int top = *std::min_element(m_ridge.begin(), m_ridge.end());
                PaintRows(frame.Paint, frame.Columns, top, surface + 6, colors.data(), [&](int x, int y) -> int
                    {
                        const auto i = static_cast<std::size_t>(x);
                        const float side = SideOf(frame, static_cast<float>(x));
                        if (y >= m_woods[i])
                        {
                            const auto offset = static_cast<float>(x - m_woodsMiddle[i]);
                            if (m_poplar[i] != 0)
                                return offset == 0.0f ? kWood : TowardLight(offset, side) ? kWoodLit : kWoodDark;
                            if (y >= surface - 1)
                                return kWoodDark;
                            const int down = y - m_woods[i];
                            if (x > 0 && m_woodsMiddle[i - 1] != m_woodsMiddle[i] && down <= 2)
                                return kWoodDark;
                            if (down <= 1 && TowardLight(offset, side))
                                return kWoodLit;
                            return kWood;
                        }
                        if (y >= m_fields[i])
                        {
                            const int down = y - m_fields[i];
                            if (down == 0)
                                return Faces(m_fieldsRise[i], side) ? kHillTopLit : kHillTop;
                            int strip = 0;
                            int from = 1;
                            while (down >= from + 2 + strip)
                            {
                                from += 2 + strip;
                                ++strip;
                            }
                            const auto band = static_cast<std::uint32_t>(strip);
                            const int length = 13 + static_cast<int>((band * 7u) % 12u);
                            const int segment = (x + static_cast<int>(Noise(band * 31u + 7u) % 23u)) / length;
                            const std::uint32_t kind = Noise(static_cast<std::uint32_t>(segment) * 131u + band * 977u + 5u) % 6u;
                            if (down == from && strip > 0 && kind % 2u == 0u)
                                return kHillHedge;
                            return kFieldKinds[kind];
                        }
                        if (y >= m_ridge[i])
                        {
                            if (y - m_ridge[i] <= 3 && Faces(m_ridgeRise[i], side))
                                return kFarLit;
                            return y >= surface - 7 ? kFarMist : kFar;
                        }
                        return -1;
                    });
            }

            void DrawLand(const Frame& frame, const Mood& mood, const ImVec4& haze) const
            {
                constexpr int kBands = 7;
                enum : int
                {
                    kRim = kBands,
                    kRimLit,
                    kBush,
                    kBushLit,
                    kBushShade,
                    kBank,
                    kBankDark,
                    kLandColors
                };
                std::array<ImVec4, kLandColors> colors{};
                for (int band = 0; band < kBands; ++band)
                {
                    const float share = (static_cast<float>(band) + 0.5f) / static_cast<float>(kBands);
                    colors[static_cast<std::size_t>(band)] = Mix(mood.Of(Mix(kGrassBack, kGrassFront, share)), haze, BackHaze(share));
                }
                colors[kRim] = Mix(mood.Of(kGrassRim), haze, BackHaze(0.0f));
                colors[kRimLit] = Mix(mood.Of(kGrassRimLit, 0.7f), haze, BackHaze(0.0f));
                colors[kBush] = Mix(mood.Of(kHedge), haze, 0.34f);
                colors[kBushLit] = Mix(mood.Of(kHedgeLit, 0.7f), haze, 0.34f);
                colors[kBushShade] = Mix(mood.Of(kHedgeDark), haze, 0.34f);
                colors[kBank] = mood.Of(kEarth);
                colors[kBankDark] = mood.Of(kEarthDark);

                const int surface = static_cast<int>(frame.Surface);
                const int shore = static_cast<int>(frame.Shore);
                const auto depth = static_cast<float>(std::max(1, shore - surface));
                const int top = std::min(*std::min_element(m_hedge.begin(), m_hedge.end()), *std::min_element(m_land.begin(), m_land.end()));
                const int bottom = *std::max_element(m_water.begin(), m_water.end());
                PaintRows(frame.Paint, frame.Columns, top, bottom, colors.data(), [&](int x, int y) -> int
                    {
                        const auto i = static_cast<std::size_t>(x);
                        if (y >= m_water[i])
                            return -1;
                        if (y >= m_hedge[i] && y <= surface + 2)
                        {
                            const float side = SideOf(frame, static_cast<float>(x));
                            const int down = y - m_hedge[i];
                            if (y == surface + 2 || (x > 0 && m_hedgeMiddle[i - 1] != m_hedgeMiddle[i] && down <= 1))
                                return kBushShade;
                            if (down <= 1 && TowardLight(static_cast<float>(x - m_hedgeMiddle[i]), side))
                                return kBushLit;
                            return kBush;
                        }
                        if (y < m_land[i])
                            return -1;
                        if (y >= shore)
                            return y == shore ? kBank : kBankDark;
                        if (y == m_land[i])
                            return Faces(m_landRise[i], SideOf(frame, static_cast<float>(x))) ? kRimLit : kRim;
                        const float at = std::clamp(static_cast<float>(y - surface) / depth, 0.0f, 0.999f) * static_cast<float>(kBands);
                        int band = static_cast<int>(at);
                        if (at - static_cast<float>(band) > 0.65f && band + 1 < kBands && (y & 1) == 0)
                            ++band;
                        return band;
                    });
            }

            void DrawLake(const Frame& frame, const Mood& mood) const
            {
                enum : int
                {
                    kEdge,
                    kShade,
                    kNear,
                    kMiddle,
                    kDeep,
                    kLakeColors
                };
                std::array<ImVec4, kLakeColors> colors{};
                colors[kEdge] = mood.Of(kWaterEdge, 0.6f);
                colors[kShade] = mood.Of(kWaterShade);
                colors[kNear] = mood.Of(kWaterNear, 0.25f);
                colors[kMiddle] = mood.Of(kWaterMid, 0.2f);
                colors[kDeep] = mood.Of(kWaterDeep, 0.15f);
                const int shore = static_cast<int>(frame.Shore);
                const auto depth = static_cast<float>(std::max(1, frame.Rows - shore));
                PaintRows(frame.Paint, frame.Columns, shore + 1, frame.Rows, colors.data(), [&](int x, int y) -> int
                    {
                        const int top = m_water[static_cast<std::size_t>(x)];
                        if (y < top)
                            return -1;
                        if (y == top)
                            return kEdge;
                        if (y == top + 1)
                            return kShade;
                        if (y == top + 2)
                            return Noise(static_cast<std::uint32_t>(x / 3) * 977u + 3u) % 3u == 0u ? kNear : kShade;
                        const float at = static_cast<float>(y - shore) / depth * 3.0f;
                        int band = static_cast<int>(at);
                        if (at - static_cast<float>(band) > 0.7f && (y & 1) == 0)
                            ++band;
                        return kNear + std::min(band, 2);
                    });
            }

            void DrawBackTrees(const Frame& frame, const Mood& mood, const ImVec4& haze) const
            {
                Canvas paint = frame.Paint;
                paint.Haze = 0.4f;
                paint.HazeColor = haze;
                const std::array<ImVec4, kToneCount> colors = PropColors(mood, kOak);
                constexpr float kAcross[] = { 0.25f, 0.56f, 0.95f };
                std::uint32_t index = 0;
                for (const float across : kAcross)
                {
                    const int x = static_cast<int>(std::round(static_cast<float>(frame.Columns) * across));
                    const float side = SideOf(frame, static_cast<float>(x));
                    const std::uint32_t seed = Noise(++index * 313u + 5u);
                    const float lift = static_cast<float>(seed % 3u);
                    Grid grid;
                    Clump(grid, -4.0f, -12.0f - lift, 5.0f, side, 1, kLeafDeep, seed);
                    Clump(grid, 4.0f, -13.0f - lift, 5.0f, side, 1, kLeafDeep, seed + 1u);
                    Trunk(grid, -8, 1, side, seed, true);
                    Clump(grid, 0.0f, -16.0f - lift, 5.0f, side, 0, kLeafDeep, seed + 2u);
                    Clump(grid, -2.0f, -10.0f - lift, 4.0f, side, 0, kLeafDeep, seed + 3u);
                    Clump(grid, 3.0f, -10.0f - lift, 4.0f, side, 0, kLeafDeep, seed + 4u);
                    grid.Emit(paint, colors.data(), x, static_cast<int>(frame.Surface) + 3);
                }
            }

            void DrawMeadow(const Frame& frame, const Mood& mood, const ImVec4& haze) const
            {
                const Canvas& paint = frame.Paint;
                const int surface = static_cast<int>(frame.Surface);
                const int shore = static_cast<int>(frame.Shore);
                const auto depth = static_cast<float>(std::max(1, shore - surface));
                const ImVec4 light = mood.Of(kGrassLight, 0.6f);
                const ImVec4 dark = mood.Of(kGrassDark);
                const ImVec4 ripe = mood.Of(kGrassSeed, 0.6f);
                for (int y = surface + 5; y < shore - 1;)
                {
                    const float share = static_cast<float>(y - surface) / depth;
                    const float hazed = BackHaze(share);
                    const int stepY = share < 0.3f ? 2 : share < 0.65f ? 3 : 4;
                    const int stepX = share < 0.3f ? 6 : share < 0.65f ? 8 : 10;
                    const int stroke = share < 0.3f ? 2 : share < 0.65f ? 3 : 4;
                    const ImVec4 lightNow = Mix(light, haze, hazed);
                    const ImVec4 darkNow = Mix(dark, haze, hazed);
                    const ImVec4 ripeNow = Mix(ripe, haze, hazed);
                    for (int cell = 0; cell * stepX < frame.Columns; ++cell)
                    {
                        const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(y) * 7919u + static_cast<std::uint32_t>(cell) * 104729u + 13u);
                        const int x = cell * stepX + static_cast<int>(mixed % static_cast<std::uint32_t>(stepX));
                        const std::uint32_t kind = (mixed >> 12) % 16u;
                        if (kind < 4u)
                        {
                            Box(paint, x, y, x + stroke, y + 1, lightNow);
                        }
                        else if (kind < 7u)
                        {
                            Box(paint, x, y, x + stroke, y + 1, darkNow);
                        }
                        else if (kind < 10u && share >= 0.3f)
                        {
                            const int tall = share < 0.65f ? 2 : 3;
                            Box(paint, x, y - tall + 1, x + 1, y + 1, darkNow);
                            Box(paint, x + 1, y - tall + 2, x + 2, y + 1, lightNow);
                        }
                        else if (kind == 10u && share >= 0.4f)
                        {
                            const int tall = share < 0.65f ? 3 : 4;
                            Box(paint, x, y - tall + 2, x + 1, y + 1, darkNow);
                            Box(paint, x, y - tall, x + 1, y - tall + 2, ripeNow);
                        }
                    }
                    y += stepY;
                }
            }

            void DrawFlowers(const Frame& frame, const Mood& mood, const ImVec4& haze) const
            {
                const Canvas& paint = frame.Paint;
                const int columns = frame.Columns;
                const int surface = static_cast<int>(frame.Surface);
                const int shore = static_cast<int>(frame.Shore);
                const auto depth = static_cast<float>(std::max(1, shore - surface));
                const ImVec4 petals[4] = { mood.Of(kPoppy), mood.Of(kCornflower), mood.Of(kChamomile), mood.Of(kButtercup) };
                const ImVec4 hearts[4] = { mood.Of(kPoppyDark), mood.Of(kCornflowerDark), mood.Of(kButtercup), mood.Of(kButtercup) };
                const ImVec4 stem = mood.Of(kStem);

                struct Drift
                {
                    float X;
                    float Y;
                    float Wide;
                    float Deep;
                    std::uint32_t Kind;
                };
                std::array<Drift, 8> drifts{};
                const int count = std::clamp(columns / 45, 3, static_cast<int>(drifts.size()));
                for (int index = 0; index < count; ++index)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 7717u + static_cast<std::uint32_t>(columns) * 31u + 41u);
                    Drift& drift = drifts[static_cast<std::size_t>(index)];
                    drift.Y = static_cast<float>(surface) + 7.0f + (depth - 13.0f) * static_cast<float>(mixed & 255u) / 255.0f;
                    const float share = (drift.Y - static_cast<float>(surface)) / depth;
                    drift.X = (static_cast<float>(index) + 0.2f + 0.6f * static_cast<float>((mixed >> 8) & 255u) / 255.0f) * static_cast<float>(columns) / static_cast<float>(count);
                    drift.Wide = (16.0f + static_cast<float>((mixed >> 16) % 14u)) * (0.6f + share);
                    drift.Deep = (3.0f + static_cast<float>((mixed >> 20) % 3u)) * (0.6f + share);
                    drift.Kind = (mixed >> 24) % 4u;
                }

                for (int y = surface + 5; y < shore - 2;)
                {
                    const float share = static_cast<float>(y - surface) / depth;
                    const float hazed = BackHaze(share);
                    const int stepY = share < 0.3f ? 2 : 3;
                    const int stepX = share < 0.3f ? 4 : share < 0.65f ? 5 : 7;
                    for (int cell = 0; cell * stepX < columns; ++cell)
                    {
                        const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(y) * 31337u + static_cast<std::uint32_t>(cell) * 7919u + 77u);
                        const int x = cell * stepX + static_cast<int>(mixed % static_cast<std::uint32_t>(stepX));
                        int inside = -1;
                        for (int index = 0; index < count; ++index)
                        {
                            const Drift& drift = drifts[static_cast<std::size_t>(index)];
                            const float dx = (static_cast<float>(x) - drift.X) / drift.Wide;
                            const float dy = (static_cast<float>(y) - drift.Y) / drift.Deep;
                            if (dx * dx + dy * dy < 1.0f)
                                inside = index;
                        }
                        const std::uint32_t chance = (mixed >> 12) % 100u;
                        if (inside >= 0 ? chance >= 42u : chance >= 4u)
                            continue;
                        std::uint32_t kind = (mixed >> 20) % 4u;
                        if (inside >= 0 && (mixed >> 24) % 5u != 0u)
                            kind = drifts[static_cast<std::size_t>(inside)].Kind;
                        const ImVec4 petal = Mix(petals[kind], haze, hazed);
                        const ImVec4 heart = Mix(hearts[kind], haze, hazed);
                        if (share < 0.3f)
                        {
                            Box(paint, x, y, x + 1, y + 1, petal);
                        }
                        else if (share < 0.65f)
                        {
                            Box(paint, x, y + 1, x + 1, y + 2, Mix(stem, haze, hazed));
                            Box(paint, x, y, x + (kind == 2u ? 2 : 1), y + 1, petal);
                        }
                        else
                        {
                            Box(paint, x, y + 1, x + 1, y + 3, stem);
                            Box(paint, x - 1, y, x + 2, y + 1, petal);
                            if (kind == 0u)
                                Box(paint, x, y - 1, x + 2, y, petal);
                            else if (kind == 1u)
                            {
                                Box(paint, x - 1, y - 1, x, y, petal);
                                Box(paint, x + 1, y - 1, x + 2, y, petal);
                            }
                            else if (kind == 3u)
                                Box(paint, x, y - 1, x + 1, y, petal);
                            if (kind != 3u)
                                Box(paint, x, y, x + 1, y + 1, heart);
                        }
                    }
                    y += stepY;
                }
            }

            void DrawWater(const Frame& frame, const Mood& mood) const
            {
                const Canvas& paint = frame.Paint;
                const int columns = frame.Columns;
                const int rows = frame.Rows;
                const int shore = static_cast<int>(frame.Shore);
                const auto depth = static_cast<float>(std::max(1, rows - shore));
                const ImVec4 ripple = mood.Of(kRipple, 0.3f);
                const ImVec4 rippleDeep = Mix(ripple, mood.Of(kWaterDeep), 0.45f);
                for (int y = shore + 5; y < rows - 1; y += 3)
                {
                    const bool near = static_cast<float>(y - shore) / depth < 0.45f;
                    for (int cell = 0; cell * 14 < columns; ++cell)
                    {
                        const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(y) * 4111u + static_cast<std::uint32_t>(cell) * 9173u + 3u);
                        if (mixed % 3u == 0u)
                            continue;
                        const int x = cell * 14 + static_cast<int>((mixed >> 4) % 14u);
                        Box(paint, x, y, x + 2 + static_cast<int>((mixed >> 9) % 4u), y + 1, near ? ripple : rippleDeep);
                    }
                }

                const ImVec4 glint = frame.Sun.Moon ? Mix(mood.Of(kWaterNear), Col(0.720f, 0.780f, 0.820f), 0.75f) : Mix(Col(1.000f, 0.960f, 0.780f), Col(1.000f, 0.700f, 0.400f), mood.Low);
                const ImVec4 glintSoft = Mix(glint, mood.Of(kWaterNear), 0.5f);
                const int lightX = static_cast<int>(std::round(frame.Sun.X));
                const auto beat = static_cast<std::uint32_t>(std::max(0.0, std::floor(frame.Time * 1.3)));
                std::uint32_t index = 0;
                for (int y = shore + 4; y < rows - 1; y += 2)
                {
                    const std::uint32_t mixed = Noise(++index * 2654435761u + 7u);
                    if ((mixed + beat) % 5u == 0u)
                        continue;
                    const int spread = 1 + (y - shore) / 6;
                    const int x = lightX + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(2 * spread + 1)) - spread;
                    const int width = 1 + static_cast<int>((mixed >> 4) % 3u) + spread / 2;
                    Box(paint, x - width / 2, y, x - width / 2 + width, y + 1, (mixed >> 12) % 3u == 0u ? glintSoft : glint);
                }

                const ImVec4 pad = mood.Of(kPad);
                const ImVec4 padLit = mood.Of(kPadLit, 0.6f);
                const ImVec4 padDark = mood.Of(kPadDark);
                const ImVec4 lily = mood.Of(kLily, 0.6f);
                const ImVec4 lilyEye = mood.Of(kButtercup);
                constexpr float kGroups[] = { 0.40f, 0.66f };
                constexpr int kPads[][3] = { { 0, 0, 1 }, { 7, 2, 0 }, { 3, 5, 0 }, { -7, 3, 0 }, { 13, 6, 1 } };
                std::uint32_t group = 0;
                for (const float across : kGroups)
                {
                    ++group;
                    const int baseX = static_cast<int>(std::round(static_cast<float>(columns) * across));
                    for (const auto& place : kPads)
                    {
                        if (Noise(group * 131u + static_cast<std::uint32_t>(place[0] + 32) * 7u) % 4u == 0u)
                            continue;
                        const int x = baseX + place[0];
                        const int y = shore + 4 + place[1];
                        if (y + 3 >= rows)
                            continue;
                        const float side = SideOf(frame, static_cast<float>(x + 3));
                        Box(paint, x + 1, y, x + 2, y + 1, side < 0.0f ? padLit : pad);
                        Box(paint, x + 3, y, x + 5, y + 1, side < 0.0f ? pad : padLit);
                        Box(paint, x, y + 1, x + 6, y + 2, pad);
                        Box(paint, side < 0.0f ? x : x + 5, y + 1, side < 0.0f ? x + 1 : x + 6, y + 2, padLit);
                        Box(paint, x + 1, y + 2, x + 5, y + 3, padDark);
                        if (place[2] != 0)
                        {
                            Box(paint, x + 2, y - 1, x + 5, y, lily);
                            Box(paint, x + 3, y - 2, x + 4, y - 1, lily);
                            Box(paint, x + 3, y - 1, x + 4, y, lilyEye);
                        }
                    }
                }
            }
        };
    }

    std::unique_ptr<Scene> MakeSummer()
    {
        return std::make_unique<SummerScene>();
    }
}
