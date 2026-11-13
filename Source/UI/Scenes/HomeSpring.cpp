#include "UI/HomeScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Idler::UI::HomeScene
{
    namespace
    {
        constexpr ImVec4 Color(float red, float green, float blue, float alpha = 1.0f)
        {
            return ImVec4(red, green, blue, alpha);
        }

        float F(int value)
        {
            return static_cast<float>(value);
        }

        std::uint32_t Hash(int value, std::uint32_t salt)
        {
            return Noise(static_cast<std::uint32_t>(value) * 0x9E3779B1u ^ (salt * 0x85EBCA77u + 0x632BE5ABu));
        }

        float Roll(int value, std::uint32_t salt)
        {
            return static_cast<float>(Hash(value, salt) & 0xFFFFu) / 65536.0f;
        }

        constexpr ImVec4 kSkyTop = Color(0.280f, 0.510f, 0.790f);
        constexpr ImVec4 kSkyLow = Color(0.640f, 0.810f, 0.920f);
        constexpr ImVec4 kSkyTopNight = Color(0.028f, 0.040f, 0.090f);
        constexpr ImVec4 kSkyLowNight = Color(0.070f, 0.105f, 0.180f);
        constexpr ImVec4 kGlow = Color(0.990f, 0.640f, 0.480f);
        constexpr ImVec4 kHazeDay = Color(0.570f, 0.710f, 0.820f);
        constexpr ImVec4 kHazeNight = Color(0.075f, 0.105f, 0.175f);

        constexpr ImVec4 kHeapTones[] = { Color(0.0f, 0.0f, 0.0f, 0.0f), Color(0.700f, 0.770f, 0.880f), Color(0.830f, 0.880f, 0.950f), Color(0.930f, 0.955f, 0.990f),
            Color(1.000f, 1.000f, 1.000f) };

        constexpr ImVec4 kRock = Color(0.500f, 0.550f, 0.750f);
        constexpr ImVec4 kRockShade = Color(0.390f, 0.430f, 0.640f);
        constexpr ImVec4 kSnow = Color(0.950f, 0.970f, 1.000f);
        constexpr ImVec4 kSnowShade = Color(0.730f, 0.780f, 0.910f);
        constexpr ImVec4 kFarHill = Color(0.400f, 0.580f, 0.590f);
        constexpr ImVec4 kFarHillShade = Color(0.340f, 0.510f, 0.550f);
        constexpr ImVec4 kFarWood = Color(0.250f, 0.410f, 0.440f);
        constexpr ImVec4 kHill = Color(0.470f, 0.670f, 0.400f);
        constexpr ImVec4 kHillShade = Color(0.400f, 0.590f, 0.380f);
        constexpr ImVec4 kHedge = Color(0.240f, 0.430f, 0.250f);
        constexpr ImVec4 kSprouts = Color(0.580f, 0.770f, 0.390f);
        constexpr ImVec4 kSproutsRow = Color(0.490f, 0.690f, 0.350f);
        constexpr ImVec4 kRapeseed = Color(0.900f, 0.820f, 0.330f);
        constexpr ImVec4 kRapeseedTop = Color(0.980f, 0.920f, 0.500f);
        constexpr ImVec4 kWheat = Color(0.380f, 0.590f, 0.320f);
        constexpr ImVec4 kWheatRow = Color(0.340f, 0.540f, 0.300f);
        constexpr ImVec4 kOrchard = Color(0.970f, 0.800f, 0.860f);

        constexpr ImVec4 kFarmTones[] = { Color(0.0f, 0.0f, 0.0f, 0.0f), Color(0.940f, 0.910f, 0.840f), Color(0.740f, 0.710f, 0.690f), Color(0.800f, 0.340f, 0.270f),
            Color(0.600f, 0.240f, 0.220f), Color(0.250f, 0.200f, 0.200f), Color(0.640f, 0.290f, 0.220f), Color(0.480f, 0.200f, 0.170f), Color(0.960f, 0.760f, 0.840f),
            Color(0.800f, 0.560f, 0.680f), Color(0.360f, 0.250f, 0.180f), Color(0.300f, 0.280f, 0.300f) };
        constexpr ImVec4 kLamplight = Color(1.000f, 0.820f, 0.420f);

        constexpr ImVec4 kTreeShade = Color(0.250f, 0.450f, 0.250f);
        constexpr ImVec4 kTreeLit = Color(0.400f, 0.630f, 0.300f);
        constexpr ImVec4 kTreeBright = Color(0.560f, 0.770f, 0.370f);
        constexpr ImVec4 kPinkShade = Color(0.780f, 0.520f, 0.650f);
        constexpr ImVec4 kPinkLit = Color(0.940f, 0.700f, 0.800f);
        constexpr ImVec4 kPinkBright = Color(1.000f, 0.860f, 0.910f);
        constexpr ImVec4 kTrunkFar = Color(0.300f, 0.230f, 0.180f);

        constexpr ImVec4 kMeadowBack = Color(0.420f, 0.640f, 0.330f);
        constexpr ImVec4 kMeadowFront = Color(0.300f, 0.560f, 0.230f);
        constexpr ImVec4 kMeadowRim = Color(0.560f, 0.780f, 0.380f);
        constexpr ImVec4 kClover = Color(0.250f, 0.500f, 0.230f);
        constexpr ImVec4 kFreshGrass = Color(0.430f, 0.680f, 0.290f);
        constexpr ImVec4 kBladeDark = Color(0.200f, 0.420f, 0.180f);
        constexpr ImVec4 kBladeMid = Color(0.330f, 0.600f, 0.240f);
        constexpr ImVec4 kBladeLight = Color(0.540f, 0.780f, 0.320f);
        constexpr ImVec4 kStalk = Color(0.260f, 0.520f, 0.200f);
        constexpr ImVec4 kBank = Color(0.200f, 0.330f, 0.160f);

        constexpr ImVec4 kPetals[] = { Color(0.970f, 0.970f, 0.940f), Color(0.990f, 0.840f, 0.180f), Color(0.900f, 0.240f, 0.260f), Color(0.950f, 0.560f, 0.720f),
            Color(0.580f, 0.440f, 0.860f), Color(0.480f, 0.640f, 0.960f) };
        constexpr ImVec4 kHearts[] = { Color(0.980f, 0.820f, 0.220f), Color(1.000f, 0.950f, 0.550f), Color(0.990f, 0.520f, 0.460f), Color(1.000f, 0.800f, 0.880f),
            Color(0.980f, 0.860f, 0.300f), Color(0.980f, 0.900f, 0.400f) };

        constexpr ImVec4 kWaterNear = Color(0.130f, 0.360f, 0.370f);
        constexpr ImVec4 kWater = Color(0.150f, 0.450f, 0.550f);
        constexpr ImVec4 kWaterDeep = Color(0.100f, 0.330f, 0.470f);
        constexpr ImVec4 kWaterEdge = Color(0.640f, 0.870f, 0.890f);
        constexpr ImVec4 kRipple = Color(0.360f, 0.660f, 0.740f);
        constexpr ImVec4 kRippleDark = Color(0.080f, 0.280f, 0.400f);
        constexpr ImVec4 kPad = Color(0.320f, 0.580f, 0.260f);
        constexpr ImVec4 kPadShade = Color(0.190f, 0.410f, 0.210f);

        float GlowOf(float daylight)
        {
            return std::clamp(1.0f - std::fabs(daylight - 0.5f) * 2.6f, 0.0f, 1.0f);
        }

        ImVec4 HazeOf(float daylight)
        {
            const ImVec4 haze = DayNight(kHazeDay, kHazeNight, daylight);
            return Mix(haze, kGlow, GlowOf(daylight) * 0.22f);
        }

        struct Look
        {
            float Daylight = 1.0f;
            float Glow = 0.0f;
            ImVec4 Haze;

            ImVec4 operator()(ImVec4 day) const
            {
                const ImVec4 night(day.x * 0.22f + 0.010f, day.y * 0.27f + 0.020f, day.z * 0.38f + 0.050f, day.w);
                ImVec4 color = Mix(night, day, Daylight);
                color.x = std::min(1.0f, color.x * (1.0f + 0.20f * Glow));
                color.y = std::min(1.0f, color.y * (1.0f + 0.03f * Glow));
                color.z = color.z * (1.0f - 0.12f * Glow);
                return color;
            }

            ImVec4 Far(ImVec4 day, float haze) const
            {
                return Mix((*this)(day), Haze, haze);
            }
        };

        Look LookOf(const Frame& frame)
        {
            return { frame.Daylight, GlowOf(frame.Daylight), HazeOf(frame.Daylight) };
        }

        float SideOfLight(const Frame& frame, float x)
        {
            return frame.Sun.X < x ? -1.0f : 1.0f;
        }

        struct Toward
        {
            float X = 0.0f;
            float Y = -1.0f;
        };

        Toward TowardLight(const Frame& frame, float x)
        {
            const float height = std::clamp(frame.Sun.Height, 0.0f, 1.0f);
            const float across = SideOfLight(frame, x) * (0.5f + 0.45f * (1.0f - height));
            const float up = -(0.3f + 0.6f * height);
            const float length = std::sqrt(across * across + up * up);
            return { across / length, up / length };
        }

        struct Column
        {
            float Top = 0.0f;
            float Bottom = 0.0f;
            int Tone = -1;
            bool operator==(const Column&) const = default;
        };

        template <typename Of>
        void Columns(const Canvas& paint, int columns, const ImVec4* colors, Of&& of)
        {
            int start = 0;
            Column run = of(0);
            for (int column = 1; column <= columns; ++column)
            {
                const Column here = column < columns ? of(column) : Column{};
                if (column < columns && here == run)
                    continue;
                if (run.Tone >= 0 && run.Bottom > run.Top)
                    paint.Fill(F(start), run.Top, F(column), run.Bottom, colors[run.Tone]);
                start = column;
                run = here;
            }
        }

        class Sprite
        {
        public:
            static constexpr int kWidth = 112;
            static constexpr int kHeight = 80;
            static constexpr int kFootX = kWidth / 2;
            static constexpr int kFootY = kHeight - 6;

            void Put(int x, int y, int tone)
            {
                const int column = x + kFootX;
                const int row = y + kFootY;
                if (column < 0 || column >= kWidth || row < 0 || row >= kHeight)
                    return;
                m_cells[static_cast<std::size_t>(row * kWidth + column)] = static_cast<std::uint8_t>(tone);
                m_left = std::min(m_left, column);
                m_right = std::max(m_right, column);
                m_top = std::min(m_top, row);
                m_bottom = std::max(m_bottom, row);
            }

            int At(int x, int y) const
            {
                const int column = x + kFootX;
                const int row = y + kFootY;
                if (column < 0 || column >= kWidth || row < 0 || row >= kHeight)
                    return 0;
                return m_cells[static_cast<std::size_t>(row * kWidth + column)];
            }

            int TopOf(int x) const
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    if (At(x, row - kFootY) != 0)
                        return row - kFootY;
                }
                return 1;
            }

            void Sprinkle(int from, int to, std::uint32_t share, std::uint32_t seed)
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    for (int column = m_left; column <= m_right; ++column)
                    {
                        std::uint8_t& cell = m_cells[static_cast<std::size_t>(row * kWidth + column)];
                        if (cell == from && Hash(column * 257 + row, seed) % 256u < share)
                            cell = static_cast<std::uint8_t>(to);
                    }
                }
            }

            void Flush(const Canvas& paint, float x, float y, const ImVec4* colors) const
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    const std::uint8_t* line = m_cells.data() + static_cast<std::size_t>(row * kWidth);
                    int column = m_left;
                    while (column <= m_right)
                    {
                        const std::uint8_t tone = line[column];
                        int end = column + 1;
                        while (end <= m_right && line[end] == tone)
                            ++end;
                        if (tone != 0)
                        {
                            const float top = y + F(row - kFootY);
                            paint.Fill(x + F(column - kFootX), top, x + F(end - kFootX), top + 1.0f, colors[tone]);
                        }
                        column = end;
                    }
                }
            }

        private:
            std::array<std::uint8_t, static_cast<std::size_t>(kWidth * kHeight)> m_cells{};
            int m_left = kWidth;
            int m_right = -1;
            int m_top = kHeight;
            int m_bottom = -1;
        };

        struct Ellipse
        {
            float X = 0.0f;
            float Y = 0.0f;
            float Rx = 1.0f;
            float Ry = 1.0f;
        };

        void Clump(Sprite& sprite, const Ellipse& clump, const Ellipse& whole, Toward light, int ramp, int bias, std::uint32_t seed, int lowest = 8)
        {
            const int top = static_cast<int>(std::floor(clump.Y - clump.Ry));
            const int bottom = std::min(lowest, static_cast<int>(std::ceil(clump.Y + clump.Ry)));
            const int left = static_cast<int>(std::floor(clump.X - clump.Rx));
            const int right = static_cast<int>(std::ceil(clump.X + clump.Rx));
            for (int y = top; y <= bottom; ++y)
            {
                for (int x = left; x <= right; ++x)
                {
                    const float nx = (F(x) + 0.5f - clump.X) / clump.Rx;
                    const float ny = (F(y) + 0.5f - clump.Y) / clump.Ry;
                    if (nx * nx + ny * ny > 1.0f)
                        continue;
                    const float wx = (F(x) + 0.5f - whole.X) / whole.Rx;
                    const float wy = (F(y) + 0.5f - whole.Y) / whole.Ry;
                    const float grain = Roll(((x + 256) >> 1) * 131 + y, seed) - 0.5f;
                    const float facing = (nx * light.X + ny * light.Y) * 0.55f + (wx * light.X + wy * light.Y) * 0.55f + grain * 0.3f;
                    const int level = std::clamp((facing > 0.6f ? 3 : facing > 0.1f ? 2 : facing > -0.4f ? 1 : 0) + bias, 0, 3);
                    sprite.Put(x, y, ramp + level);
                }
            }
        }

        void Branch(Sprite& sprite, float x0, float y0, float x1, float y1, float width, float end, Toward light, int ramp)
        {
            const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)) * 1.5f)));
            for (int step = 0; step <= steps; ++step)
            {
                const float share = F(step) / F(steps);
                const float x = x0 + (x1 - x0) * share;
                const auto y = static_cast<int>(std::round(y0 + (y1 - y0) * share));
                const int thick = std::max(1, static_cast<int>(std::round(width + (end - width) * share)));
                const auto left = static_cast<int>(std::round(x - F(thick) * 0.5f));
                for (int cell = 0; cell < thick; ++cell)
                {
                    const int fromLight = light.X < 0.0f ? cell : thick - 1 - cell;
                    const int level = thick == 1 ? 1 : fromLight == 0 ? 2 : fromLight == thick - 1 ? 0 : 1;
                    sprite.Put(left + cell, y, ramp + level);
                }
            }
        }

        void Trunk(Sprite& sprite, int top, float base, float upper, float lean, Toward light, int ramp, std::uint32_t seed)
        {
            for (int y = -1; y >= top; --y)
            {
                const float share = F(-1 - y) / F(std::max(1, -1 - top));
                const float flare = y == -1 ? 2.0f : y == -2 ? 1.0f : 0.0f;
                const int width = std::max(1, static_cast<int>(std::round(base + (upper - base) * share + flare)));
                const float middle = lean * share * share;
                const auto left = static_cast<int>(std::round(middle - F(width) * 0.5f));
                for (int cell = 0; cell < width; ++cell)
                {
                    const int fromLight = light.X < 0.0f ? cell : width - 1 - cell;
                    int level = fromLight == 0 ? 2 : fromLight >= width - (width > 3 ? 2 : 1) ? 0 : 1;
                    if (level == 1 && (Hash(left + cell + 64, seed + static_cast<std::uint32_t>((-y) / 3)) & 7u) == 0u)
                        level = 0;
                    sprite.Put(left + cell, y, ramp + level);
                }
            }
        }

        enum class Kind : std::uint32_t
        {
            Cherry,
            Birch,
            Forsythia,
            Apple,
            Lilac,
            Rock,
            Stump,
        };

        struct KindShape
        {
            float Foot;
            float Cover;
            float CoverHeight;
            float Height;
            float Crown;
            float DepthFrom;
            float DepthTo;
        };

        constexpr KindShape kShapes[] = {
            { 2.5f, 13.0f, 11.0f, 45.0f, 21.0f, 0.10f, 0.30f },
            { 1.5f, 0.0f, 0.0f, 53.0f, 12.0f, 0.28f, 0.42f },
            { 3.0f, 14.0f, 11.0f, 13.0f, 16.0f, 0.45f, 0.65f },
            { 2.5f, 0.0f, 0.0f, 38.0f, 23.0f, 0.12f, 0.28f },
            { 3.0f, 12.0f, 12.0f, 16.0f, 13.0f, 0.35f, 0.55f },
            { 6.0f, 0.0f, 0.0f, 8.0f, 7.0f, 0.60f, 0.90f },
            { 4.5f, 0.0f, 0.0f, 7.0f, 6.0f, 0.50f, 0.80f },
        };

        constexpr Kind kOrders[4][7] = {
            { Kind::Cherry, Kind::Forsythia, Kind::Birch, Kind::Apple },
            { Kind::Cherry, Kind::Forsythia, Kind::Birch, Kind::Stump, Kind::Apple },
            { Kind::Cherry, Kind::Forsythia, Kind::Birch, Kind::Stump, Kind::Apple, Kind::Lilac },
            { Kind::Cherry, Kind::Lilac, Kind::Birch, Kind::Forsythia, Kind::Stump, Kind::Apple, Kind::Rock },
        };

        constexpr int kLeaf = 1;
        constexpr int kPink = 5;
        constexpr int kWhite = 9;
        constexpr int kGold = 13;
        constexpr int kLilac = 17;
        constexpr int kBark = 21;
        constexpr int kBirch = 24;
        constexpr int kStone = 28;
        constexpr int kMoss = 32;
        constexpr int kCatkin = 34;
        constexpr int kHeart = 35;
        constexpr int kTrumpet = 36;
        constexpr int kRing = 37;
        constexpr int kFungus = 39;
        constexpr int kViolet = 40;
        constexpr int kPetal = 41;
        constexpr int kDeepLeaf = 42;

        constexpr ImVec4 kTones[] = {
            Color(0.0f, 0.0f, 0.0f, 0.0f),
            Color(0.180f, 0.360f, 0.190f), Color(0.310f, 0.540f, 0.240f), Color(0.480f, 0.720f, 0.290f), Color(0.680f, 0.860f, 0.400f),
            Color(0.660f, 0.360f, 0.500f), Color(0.870f, 0.540f, 0.670f), Color(0.970f, 0.720f, 0.810f), Color(1.000f, 0.890f, 0.920f),
            Color(0.800f, 0.720f, 0.780f), Color(0.930f, 0.890f, 0.900f), Color(0.990f, 0.970f, 0.960f), Color(1.000f, 1.000f, 1.000f),
            Color(0.700f, 0.560f, 0.120f), Color(0.920f, 0.740f, 0.150f), Color(0.990f, 0.870f, 0.290f), Color(1.000f, 0.960f, 0.600f),
            Color(0.400f, 0.280f, 0.540f), Color(0.580f, 0.440f, 0.740f), Color(0.740f, 0.600f, 0.880f), Color(0.880f, 0.790f, 0.970f),
            Color(0.200f, 0.130f, 0.100f), Color(0.350f, 0.240f, 0.170f), Color(0.510f, 0.370f, 0.260f),
            Color(0.130f, 0.120f, 0.130f), Color(0.600f, 0.600f, 0.600f), Color(0.820f, 0.810f, 0.780f), Color(0.970f, 0.960f, 0.920f),
            Color(0.320f, 0.330f, 0.360f), Color(0.480f, 0.490f, 0.510f), Color(0.640f, 0.640f, 0.640f), Color(0.790f, 0.790f, 0.770f),
            Color(0.280f, 0.440f, 0.160f), Color(0.450f, 0.620f, 0.200f),
            Color(0.780f, 0.660f, 0.300f),
            Color(0.990f, 0.840f, 0.220f),
            Color(0.960f, 0.600f, 0.150f),
            Color(0.760f, 0.600f, 0.420f), Color(0.560f, 0.420f, 0.290f),
            Color(0.880f, 0.760f, 0.560f),
            Color(0.560f, 0.420f, 0.860f),
            Color(0.970f, 0.970f, 0.950f),
            Color(0.120f, 0.280f, 0.170f), Color(0.200f, 0.400f, 0.220f), Color(0.320f, 0.540f, 0.260f), Color(0.460f, 0.660f, 0.320f),
        };

        void DrawCherry(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            Trunk(sprite, -18, 4.6f, 3.0f, 0.0f, light, kBark, seed);
            const Ellipse crown{ 0.0f, -31.0f, 20.0f, 12.5f };
            for (int index = 0; index < 7; ++index)
            {
                const float angle = (F(index) + Roll(index, seed + 1u) * 0.7f) * (6.2832f / 7.0f);
                const float radius = 7.0f + Roll(index, seed + 2u) * 2.0f;
                Clump(sprite, { crown.X + std::cos(angle) * 9.5f, crown.Y - 1.0f + std::sin(angle) * 5.0f, radius, radius * 0.85f }, crown, light, kPink, -1, seed);
            }
            Branch(sprite, -0.5f, -17.0f, -11.0f, -28.0f, 2.4f, 1.0f, light, kBark);
            Branch(sprite, 0.5f, -17.0f, 10.0f, -30.0f, 2.4f, 1.0f, light, kBark);
            Branch(sprite, 0.0f, -18.0f, 1.0f, -37.0f, 2.0f, 1.0f, light, kBark);
            Branch(sprite, -6.0f, -22.0f, -16.0f, -24.0f, 1.4f, 1.0f, light, kBark);
            Branch(sprite, 5.0f, -24.0f, 15.0f, -21.0f, 1.4f, 1.0f, light, kBark);
            for (int index = 0; index < 9; ++index)
            {
                const float angle = (F(index) + Roll(index, seed + 3u) * 0.8f) * (6.2832f / 9.0f) + 0.3f;
                const float radius = 5.0f + Roll(index, seed + 4u) * 1.8f;
                Clump(sprite, { crown.X + std::cos(angle) * crown.Rx * 0.62f, crown.Y + std::sin(angle) * crown.Ry * 0.62f, radius, radius * 0.85f }, crown, light, kPink, 0, seed + 5u);
            }
            Clump(sprite, { crown.X - 3.0f + light.X * 3.0f, crown.Y - 2.0f, 5.5f, 4.6f }, crown, light, kPink, 0, seed + 6u);
            sprite.Sprinkle(kPink, kLeaf + 1, 70u, seed + 7u);
            sprite.Sprinkle(kPink + 1, kLeaf + 2, 18u, seed + 8u);

            const Ellipse bush{ 0.0f, -4.5f, 14.5f, 7.0f };
            constexpr float kBushX[] = { -11.0f, -7.0f, -2.5f, 2.5f, 7.0f, 11.0f, 0.0f };
            constexpr float kBushR[] = { 3.6f, 4.6f, 5.0f, 5.0f, 4.6f, 3.6f, 4.0f };
            for (int index = 0; index < 7; ++index)
            {
                const float radius = kBushR[index];
                const float x = kBushX[index] + (Roll(index, seed + 9u) - 0.5f) * 1.5f;
                const float y = index == 6 ? -8.0f : -4.0f - (radius - 3.6f) * 1.1f - Roll(index, seed + 12u);
                Clump(sprite, { x, y, radius, radius * 0.95f }, bush, light, kLeaf, 0, seed + 10u, -1);
            }
            sprite.Sprinkle(kLeaf + 2, kPetal, 26u, seed + 11u);
        }

        void DrawBirch(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            const float lean = (Roll(0, seed) - 0.5f) * 4.0f;
            constexpr int kTop = -47;
            const auto middleAt = [&](int y)
                {
                    const float share = F(-1 - y) / F(-1 - kTop);
                    return lean * share * share;
                };
            const Ellipse crown{ lean * 0.6f, -35.0f, 12.0f, 17.0f };
            for (int index = 0; index < 8; ++index)
            {
                const float height = (F(index) + Roll(index, seed + 13u)) / 8.0f;
                const float y = -24.0f - height * 25.0f;
                const float spread = 2.0f + 6.0f * std::sin(std::min(1.0f, (1.0f - height) * 1.3f) * 1.5708f);
                const float x = middleAt(static_cast<int>(y)) + (index % 2 == 0 ? -spread : spread) * (0.4f + 0.6f * Roll(index, seed + 14u));
                const float radius = 3.0f + Roll(index, seed + 15u) * 1.4f;
                Clump(sprite, { x, y, radius, radius * 0.9f }, crown, light, kLeaf, 0, seed + 16u);
            }
            float tipX[7] = {};
            float tipY[7] = {};
            for (int index = 0; index < 7; ++index)
            {
                const int y = -19 - index * 4;
                const float side = index % 2 == 0 ? -1.0f : 1.0f;
                const float reach = (5.0f + Roll(index, seed + 1u) * 4.0f) * (1.0f - F(index) * 0.08f);
                const float x = middleAt(y);
                tipX[index] = x + side * reach;
                tipY[index] = F(y) - 3.0f - Roll(index, seed + 2u) * 3.0f;
                Branch(sprite, x, F(y), tipX[index], tipY[index], 1.0f, 1.0f, light, kBark);
            }
            for (int y = -1; y >= kTop; --y)
            {
                const float share = F(-1 - y) / F(-1 - kTop);
                const int width = y >= -2 ? 4 : share < 0.45f ? 3 : share < 0.8f ? 2 : 1;
                const auto left = static_cast<int>(std::round(middleAt(y) - F(width) * 0.5f));
                const std::uint32_t mark = Hash(-y, seed + 5u);
                const int markFrom = mark % 3u == 0u ? static_cast<int>((mark >> 8) % 2u) : width;
                const int markTo = 1 + static_cast<int>((mark >> 9) % 2u);
                for (int cell = 0; cell < width; ++cell)
                {
                    const int fromLight = light.X < 0.0f ? cell : width - 1 - cell;
                    int level = width == 1 ? 2 : fromLight == 0 ? 3 : fromLight == width - 1 ? 1 : 2;
                    if (cell >= markFrom && cell <= markTo)
                        level = 0;
                    if (y >= -3 && (Hash(cell * 7 - y, seed + 6u) & 1u) == 0u)
                        level = 0;
                    sprite.Put(left + cell, y, kBirch + level);
                }
            }
            const auto front = [&](int index, float x, float y, float radius)
                {
                    Clump(sprite, { x, y, radius, radius * 0.9f }, crown, light, kLeaf, 1, seed + 10u);
                    if (index % 2 == 0)
                    {
                        const auto cx = static_cast<int>(std::round(x + (Roll(index, seed + 11u) - 0.5f) * radius));
                        const auto cy = static_cast<int>(std::round(y + radius * 0.9f));
                        sprite.Put(cx, cy, kCatkin);
                        sprite.Put(cx, cy + 1, kCatkin);
                    }
                };
            for (int index = 0; index < 7; ++index)
            {
                const float x = tipX[index] - (tipX[index] - middleAt(-19 - index * 4)) * 0.15f;
                front(index, x, tipY[index] + (Roll(index, seed + 8u) - 0.5f) * 2.0f, 3.0f + Roll(index, seed + 9u) * 1.4f);
            }
            for (int index = 7; index < 11; ++index)
            {
                const float y = -47.0f - Roll(index, seed + 7u) * 3.0f + std::fabs(F(index) - 8.5f);
                front(index, middleAt(kTop) + (F(index) - 8.5f) * 2.6f, y, 2.6f + Roll(index, seed + 9u) * 1.4f);
            }
            for (int index = 0; index < 4; ++index)
            {
                const int x = (index < 2 ? -3 : 2) - index % 2;
                const int tall = 2 + static_cast<int>(Hash(index, seed + 12u) % 2u);
                for (int y = -1; y >= -tall; --y)
                    sprite.Put(x, y, kLeaf + 1 + index % 2);
            }
            sprite.Put(4, -3, kViolet);
            sprite.Put(4, -2, kLeaf + 1);
            sprite.Put(4, -1, kLeaf + 1);
        }

        void DrawForsythia(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            const Ellipse mound{ 0.0f, -6.0f, 13.0f, 6.0f };
            for (int index = 0; index < 7; ++index)
            {
                const float radius = 3.8f + (3.0f - std::fabs(F(index) - 3.0f)) * 0.45f + Roll(index, seed + 4u) * 0.6f;
                const float x = -10.5f + F(index) * 3.5f + (Roll(index, seed + 5u) - 0.5f) * 1.2f;
                Clump(sprite, { x, -4.2f - (radius - 3.8f) * 1.2f, radius, radius * 0.92f }, mound, light, kLeaf, 0, seed + 6u, -1);
            }
            sprite.Sprinkle(kLeaf + 3, kGold + 3, 150u, seed + 7u);
            sprite.Sprinkle(kLeaf + 2, kGold + 2, 120u, seed + 8u);
            sprite.Sprinkle(kLeaf + 1, kGold + 1, 70u, seed + 9u);

            constexpr int kSprays = 11;
            for (int index = 0; index < kSprays; ++index)
            {
                const float spread = (F(index) - 5.0f) / 5.0f;
                const float peak = 11.2f - std::fabs(spread) * 4.0f + Roll(index, seed + 1u) * 1.6f;
                const float startX = spread * 6.0f;
                const float startY = -5.0f - (1.0f - std::fabs(spread)) * 3.0f;
                const float bendX = spread * 11.0f;
                const float bendY = -peak * 1.3f;
                const float endX = spread * 16.5f + (Roll(index, seed + 2u) - 0.5f) * 2.0f;
                const float endY = std::min(-1.0f, -peak * 0.3f + std::fabs(spread) * 2.0f);
                for (int step = 0; step <= 16; ++step)
                {
                    const float t = F(step) / 16.0f;
                    const float x = (1.0f - t) * (1.0f - t) * startX + 2.0f * (1.0f - t) * t * bendX + t * t * endX;
                    const float y = (1.0f - t) * (1.0f - t) * startY + 2.0f * (1.0f - t) * t * bendY + t * t * endY;
                    const auto cx = static_cast<int>(std::round(x));
                    const auto cy = static_cast<int>(std::round(y));
                    const std::uint32_t mixed = Hash(index * 37 + step, seed + 3u);
                    if (mixed % 3u == 0u)
                    {
                        if (sprite.At(cx, cy) == 0)
                            sprite.Put(cx, cy, kBark + 2);
                        continue;
                    }
                    const int lit = (x - startX) * light.X > 0.0f || y < startY - 4.0f ? 1 : 0;
                    sprite.Put(cx, cy, kGold + 1 + lit);
                    sprite.Put(cx + ((mixed >> 4) % 2u == 0u ? 1 : -1), cy - static_cast<int>((mixed >> 5) % 2u), kGold + 2 + lit);
                }
            }
        }

        void DrawApple(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            Trunk(sprite, -13, 4.4f, 3.4f, -2.0f, light, kBark, seed);
            const Ellipse crown{ -1.0f, -26.0f, 22.0f, 11.0f };
            for (int index = 0; index < 7; ++index)
            {
                const float angle = (F(index) + Roll(index, seed + 1u) * 0.7f) * (6.2832f / 7.0f);
                const float radius = 6.5f + Roll(index, seed + 2u) * 1.5f;
                Clump(sprite, { crown.X + std::cos(angle) * 10.5f, crown.Y - 1.0f + std::sin(angle) * 4.0f, radius, radius * 0.85f }, crown, light, kLeaf, -1, seed);
            }
            Branch(sprite, -2.0f, -12.0f, -13.0f, -22.0f, 2.2f, 1.0f, light, kBark);
            Branch(sprite, -2.0f, -12.0f, 9.0f, -24.0f, 2.2f, 1.0f, light, kBark);
            Branch(sprite, -2.0f, -13.0f, -1.0f, -31.0f, 2.0f, 1.0f, light, kBark);
            Branch(sprite, -8.0f, -18.0f, -18.0f, -20.0f, 1.2f, 1.0f, light, kBark);
            Branch(sprite, 4.0f, -19.0f, 17.0f, -21.0f, 1.2f, 1.0f, light, kBark);
            for (int index = 0; index < 6; ++index)
            {
                const float angle = (F(index) + Roll(index, seed + 3u) * 0.8f) * (6.2832f / 6.0f) + 0.2f;
                const float radius = 4.8f + Roll(index, seed + 4u) * 1.5f;
                Clump(sprite, { crown.X + std::cos(angle) * crown.Rx * 0.6f, crown.Y + std::sin(angle) * crown.Ry * 0.55f, radius, radius * 0.85f }, crown, light, kLeaf, 0, seed + 5u);
            }
            for (int index = 0; index < 20; ++index)
            {
                const float angle = Roll(index, seed + 6u) * 6.2832f;
                const float reach = 0.25f + 0.62f * std::sqrt(Roll(index, seed + 7u));
                const float radius = 2.2f + Roll(index, seed + 8u) * 1.2f;
                Clump(sprite, { crown.X + std::cos(angle) * crown.Rx * reach * 0.86f, crown.Y + std::sin(angle) * crown.Ry * reach * 0.8f, radius, radius * 0.9f }, crown, light, kWhite, 0, seed + 9u);
            }
            sprite.Sprinkle(kWhite, kPink + 1, 150u, seed + 10u);
            sprite.Sprinkle(kWhite + 1, kPink + 2, 40u, seed + 11u);
            sprite.Sprinkle(kLeaf + 2, kWhite + 2, 30u, seed + 12u);
            for (int index = 0; index < 5; ++index)
            {
                const int x = -4 + index * 2 + static_cast<int>(Hash(index, seed + 10u) % 2u);
                const int tall = 1 + static_cast<int>(Hash(index, seed + 11u) % 3u);
                for (int y = -1; y >= -tall; --y)
                    sprite.Put(x, y, kLeaf + 1 + index % 2);
            }
        }

        void DrawLilac(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            const Ellipse mound{ 0.0f, -7.5f, 13.0f, 7.5f };
            for (int index = 0; index < 5; ++index)
            {
                const float radius = 4.2f + Roll(index, seed + 1u) * 0.6f;
                const float x = -9.0f + F(index) * 4.5f + (Roll(index, seed + 2u) - 0.5f) * 1.0f;
                Clump(sprite, { x, -4.6f, radius, radius * 0.95f }, mound, light, kDeepLeaf, 1, seed + 3u, -1);
            }
            for (int index = 0; index < 3; ++index)
            {
                const float radius = 4.4f + Roll(index, seed + 6u) * 0.6f;
                const float x = -5.0f + F(index) * 5.0f + (Roll(index, seed + 7u) - 0.5f) * 1.0f;
                Clump(sprite, { x, -9.2f - (index == 1 ? 0.6f : 0.0f), radius, radius * 0.9f }, mound, light, kDeepLeaf, 1, seed + 3u, -1);
            }
            for (int index = 0; index < 9; ++index)
            {
                const auto x = static_cast<int>(std::round(-11.0f + F(index) * 2.75f + (Roll(index, seed + 4u) - 0.5f) * 1.6f));
                const int base = std::max(sprite.TopOf(x) + 2 + static_cast<int>(Hash(index, seed + 8u) % 5u), -12);
                const int tall = 3 + static_cast<int>(Hash(index, seed + 5u) % 2u);
                for (int step = 0; step < tall; ++step)
                {
                    const int y = base - step;
                    if (y < -16)
                        break;
                    const int width = step < 2 ? 3 : step < tall - 1 ? 2 : 1;
                    const int left = width == 3 ? x - 1 : width == 2 ? (light.X < 0.0f ? x - 1 : x) : x;
                    for (int cell = 0; cell < width; ++cell)
                    {
                        const int fromLight = light.X < 0.0f ? cell : width - 1 - cell;
                        const int level = step == tall - 1 ? 3 : step == 0 ? (fromLight == 0 ? 1 : 0) : fromLight == 0 ? 2 : 1;
                        sprite.Put(left + cell, y, kLilac + level);
                    }
                }
            }
        }

        void DrawRock(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            constexpr float kHalf[] = { 7.5f, 7.5f, 7.0f, 6.5f, 5.5f, 4.5f, 2.5f };
            for (int step = 0; step < 7; ++step)
            {
                const int y = -1 - step;
                const float half = kHalf[step];
                for (auto x = static_cast<int>(std::floor(-half)); x < static_cast<int>(std::ceil(half)); ++x)
                {
                    const float nx = (F(x) + 0.5f) / half;
                    const float ny = (F(y) + 4.5f) / 4.0f;
                    const float grain = Roll(((x + 64) >> 1) * 31 + y, seed) - 0.5f;
                    const float facing = nx * light.X * 0.8f + ny * light.Y * 0.8f + grain * 0.35f;
                    int tone = kStone + (facing > 0.55f ? 3 : facing > 0.1f ? 2 : facing > -0.35f ? 1 : 0);
                    if ((ny < -0.15f && grain > -0.3f) || (facing < -0.2f && grain > 0.15f))
                        tone = facing > 0.2f ? kMoss + 1 : kMoss;
                    if (y == -1 && std::fabs(nx) > 0.75f)
                        tone = kStone;
                    sprite.Put(x, y, tone);
                }
            }
            for (int step = 0; step < 4; ++step)
                sprite.Put(-1 + step / 2, -6 + step, kStone);
            const int side = light.X < 0.0f ? 1 : -1;
            for (int index = 0; index < 3; ++index)
            {
                const int x = side * (9 + index * 2);
                const int tall = 5 + static_cast<int>(Hash(index, seed + 1u) % 3u);
                for (int y = -1; y >= -tall + 1; --y)
                    sprite.Put(x, y, kLeaf + 1);
                sprite.Put(x, -tall, kHeart);
                sprite.Put(x + side, -tall, kTrumpet);
                sprite.Put(x - side, -2 - index, kLeaf + 2);
            }
            sprite.Put(-side * 9, -1, kLeaf + 1);
            sprite.Put(-side * 10, -1, kLeaf + 2);
            sprite.Put(-side * 9, -2, kGold + 3);
            sprite.Put(-side * 10, -2, kGold + 2);
        }

        void DrawStump(Sprite& sprite, Toward light, std::uint32_t seed)
        {
            Trunk(sprite, -5, 8.0f, 8.0f, 0.0f, light, kBark, seed);
            for (int x = -4; x <= 3; ++x)
                sprite.Put(x, -6, x == -4 || x == 3 ? kRing + 1 : kRing);
            for (int x = -3; x <= 2; ++x)
                sprite.Put(x, -7, x == -3 || x == 2 ? kRing + 1 : kRing);
            sprite.Put(-1, -6, kRing + 1);
            sprite.Put(0, -6, kRing + 1);
            const int shade = light.X < 0.0f ? 1 : -1;
            for (int y = -1; y >= -4; --y)
            {
                sprite.Put(shade > 0 ? 3 : -4, y, kMoss);
                if (y > -3)
                    sprite.Put(shade > 0 ? 2 : -3, y, kMoss + 1);
            }
            sprite.Put(-shade * 5, -3, kFungus);
            sprite.Put(-shade * 6, -3, kFungus);
            sprite.Put(-shade * 5, -4, kFungus);
            for (int index = 0; index < 3; ++index)
            {
                const int x = shade * (7 + index * 2);
                sprite.Put(x, -1, kLeaf + 1);
                sprite.Put(x, -2, kViolet);
                if (index != 1)
                    sprite.Put(x + 1, -1, kLeaf + 2);
            }
        }

        void DrawHeap(Sprite& sprite, float width, Toward light, std::uint32_t seed)
        {
            const int bumps = 4 + static_cast<int>(Hash(0, seed) % 3u);
            const Ellipse whole{ 0.0f, -width * 0.18f, width * 0.5f, width * 0.22f };
            for (int index = 0; index < bumps; ++index)
            {
                const float share = (F(index) + 0.5f) / F(bumps) - 0.5f;
                const float radius = (3.5f + width * 0.16f * (1.0f - std::fabs(share) * 1.3f)) * (0.8f + Roll(index, seed + 1u) * 0.4f);
                Clump(sprite, { share * width * 0.8f, -radius * 0.55f, radius, radius * 0.85f }, whole, light, 1, 0, seed + 2u, -1);
            }
        }

        void PaintSky(const Frame& frame, const Look& look)
        {
            const Canvas& paint = frame.Paint;
            const float width = F(frame.Columns);
            const ImVec4 top = DayNight(kSkyTop, kSkyTopNight, look.Daylight);
            const ImVec4 low = DayNight(kSkyLow, kSkyLowNight, look.Daylight);
            constexpr int kBands = 16;
            float from = 0.0f;
            ImVec4 above;
            for (int band = 0; band < kBands; ++band)
            {
                const float share = (F(band) + 0.5f) / F(kBands);
                const ImVec4 color = Mix(Mix(top, low, share), kGlow, look.Glow * 0.75f * share * share);
                const float to = band == kBands - 1 ? frame.Surface + 4.0f : std::round(frame.Surface * F(band + 1) / F(kBands));
                paint.Fill(0.0f, from, width, to, color);
                if (band >= kBands / 2)
                {
                    for (int column = (band & 1) * 3; column < frame.Columns; column += 6)
                        paint.Fill(F(column), from, F(column + 3), from + 1.0f, above);
                }
                above = color;
                from = to;
            }

            const Light& sun = frame.Sun;
            if (!sun.Moon)
            {
                const ImVec4 ring = Color(1.0f, 0.92f, 0.74f, 0.05f + 0.07f * look.Glow);
                for (const float radius : { 30.0f, 22.0f, 16.0f })
                    DrawDisc(paint, sun.X, sun.Y, radius, ring);
            }
            else
            {
                const ImVec4 ring = Color(0.75f, 0.82f, 1.0f, 0.04f);
                for (const float radius : { 22.0f, 15.0f })
                    DrawDisc(paint, sun.X, sun.Y, radius, ring);
            }

            std::array<ImVec4, std::size(kHeapTones)> heap{};
            for (std::size_t index = 1; index < heap.size(); ++index)
                heap[index] = Mix(look.Far(kHeapTones[index], 0.25f), low, 0.12f + 0.4f * (1.0f - look.Daylight));
            constexpr float kHeaps[] = { 0.40f, 0.61f, 0.95f, 0.03f };
            for (int index = 0; index < static_cast<int>(std::size(kHeaps)); ++index)
            {
                const auto salt = static_cast<std::uint32_t>(index) * 17u + 71u;
                const float x = std::round(width * kHeaps[index] + (Roll(index, salt) - 0.5f) * 14.0f);
                const float base = std::round(frame.Surface - 21.0f - Roll(index, salt + 1u) * 8.0f);
                Sprite sprite;
                DrawHeap(sprite, 26.0f + Roll(index, salt + 2u) * 18.0f, TowardLight(frame, x), salt);
                sprite.Flush(paint, x, base, heap.data());
            }

            const ImVec4 middle = Mix(top, low, 0.35f);
            DrawSky(frame, ImVec4(middle.x, middle.y, middle.z, 0.0f));
        }

        void PaintMountains(const Frame& frame, const Look& look)
        {
            constexpr float kHaze = 0.5f;
            const ImVec4 colors[] = { look.Far(kRock, kHaze), look.Far(kRockShade, kHaze), look.Far(kSnow, kHaze * 0.7f), look.Far(kSnowShade, kHaze * 0.7f) };
            constexpr int kSpacing = 34;
            const float surface = frame.Surface;
            struct Peak
            {
                float Top = 0.0f;
                float X = 0.0f;
            };
            const auto peakAt = [&](int column)
                {
                    Peak best{ -1.0e9f, 0.0f };
                    for (int index = column / kSpacing - 3; index <= column / kSpacing + 3; ++index)
                    {
                        const float x = F(index * kSpacing) + Roll(index, 301u) * F(kSpacing);
                        const float height = 17.0f + Roll(index, 302u) * 21.0f;
                        const float slope = 0.55f + Roll(index, 303u) * 0.4f;
                        const float here = height - slope * std::fabs(F(column) - x);
                        if (here > best.Top)
                            best = { here, x };
                    }
                    best.Top = std::round(surface - best.Top + std::sin(F(column) * 0.45f + best.X) * 0.8f);
                    return best;
                };
            const auto litAt = [&](int column, const Peak& peak)
                {
                    const float facing = F(column) < peak.X ? -1.0f : 1.0f;
                    return facing == SideOfLight(frame, F(column));
                };
            Columns(frame.Paint, frame.Columns, colors, [&](int column)
                {
                    const Peak peak = peakAt(column);
                    return Column{ peak.Top, surface + 2.0f, litAt(column, peak) ? 0 : 1 };
                });
            Columns(frame.Paint, frame.Columns, colors, [&](int column)
                {
                    const Peak peak = peakAt(column);
                    const float line = surface - 29.0f + F(static_cast<int>(Hash(column / 2, 304u) % 3u));
                    if (peak.Top >= line)
                        return Column{};
                    return Column{ peak.Top, line, litAt(column, peak) ? 2 : 3 };
                });
        }

        float FarHillTop(float surface, int column)
        {
            const float at = F(column);
            return std::round(surface - 15.0f - 4.5f * std::sin(at * 0.031f + 1.3f) - 2.5f * std::sin(at * 0.083f + 0.2f) - std::sin(at * 0.21f));
        }

        float HillTop(float surface, int column)
        {
            const float at = F(column);
            return std::round(surface - 9.0f - 3.5f * std::sin(at * 0.047f + 4.0f) - 1.8f * std::sin(at * 0.12f + 1.0f));
        }

        void DrawFarm(Sprite& sprite, float toLight)
        {
            const bool fromLeft = toLight < 0.0f;
            for (int y = -4; y <= -1; ++y)
            {
                for (int x = 0; x <= 8; ++x)
                    sprite.Put(x, y, (fromLeft ? x == 8 : x == 0) ? 2 : 1);
            }
            sprite.Put(2, -3, 11);
            sprite.Put(6, -3, 11);
            sprite.Put(4, -2, 5);
            sprite.Put(4, -1, 5);
            for (int x = -1; x <= 9; ++x)
                sprite.Put(x, -5, 4);
            for (int x = 0; x <= 8; ++x)
                sprite.Put(x, -6, 3);
            for (int x = 1; x <= 7; ++x)
                sprite.Put(x, -7, 3);
            sprite.Put(6, -8, 4);
            sprite.Put(6, -9, 4);
            for (int y = -5; y <= -1; ++y)
            {
                for (int x = 11; x <= 17; ++x)
                    sprite.Put(x, y, (fromLeft ? x >= 16 : x <= 12) ? 7 : 6);
            }
            for (int y = -3; y <= -1; ++y)
            {
                for (int x = 13; x <= 15; ++x)
                    sprite.Put(x, y, 5);
            }
            for (int x = 11; x <= 17; ++x)
                sprite.Put(x, -6, 4);
            for (int x = 12; x <= 16; ++x)
                sprite.Put(x, -7, 4);
            for (int x = 13; x <= 15; ++x)
                sprite.Put(x, -8, 4);
            sprite.Put(21, -1, 10);
            sprite.Put(21, -2, 10);
            for (int y = -6; y <= -3; ++y)
            {
                const int half = y == -6 || y == -3 ? 1 : 2;
                for (int x = 21 - half; x <= 21 + half; ++x)
                    sprite.Put(x, y, (fromLeft ? x < 21 : x > 21) || y == -6 ? 8 : 9);
            }
        }

        void PaintHills(const Frame& frame, const Look& look, float farmX)
        {
            const Canvas& paint = frame.Paint;
            const float surface = frame.Surface;
            constexpr float kFarHaze = 0.42f;
            const ImVec4 far[] = { look.Far(kFarHill, kFarHaze), look.Far(kFarHillShade, kFarHaze), look.Far(kFarWood, kFarHaze) };
            const auto litOn = [&](float before, float after, int column)
                {
                    const float slope = after - before;
                    return slope == 0.0f || (slope < 0.0f ? -1.0f : 1.0f) == SideOfLight(frame, F(column));
                };
            Columns(paint, frame.Columns, far, [&](int column)
                {
                    return Column{ FarHillTop(surface, column), surface + 3.0f, litOn(FarHillTop(surface, column - 1), FarHillTop(surface, column + 1), column) ? 0 : 1 };
                });
            Columns(paint, frame.Columns, far, [&](int column)
                {
                    const float at = F(column);
                    if (std::sin(at * 0.043f + 2.0f) + 0.6f * std::sin(at * 0.11f) < 0.5f)
                        return Column{};
                    const float top = FarHillTop(surface, column) - F(static_cast<int>(Hash(column, 311u) % 2u));
                    return Column{ top, top + 3.0f + F(static_cast<int>(Hash(column / 3, 312u) % 2u)), 2 };
                });

            constexpr float kHaze = 0.3f;
            const ImVec4 near[] = { look.Far(kHill, kHaze), look.Far(kHillShade, kHaze) };
            Columns(paint, frame.Columns, near, [&](int column)
                {
                    return Column{ HillTop(surface, column), surface + 3.0f, litOn(HillTop(surface, column - 1), HillTop(surface, column + 1), column) ? 0 : 1 };
                });

            constexpr float kFieldHaze = 0.36f;
            const ImVec4 hedge = look.Far(kHedge, kHaze);
            const ImVec4 fields[4][2] = {
                { look.Far(kSprouts, kFieldHaze), look.Far(kSproutsRow, kFieldHaze) },
                { look.Far(kRapeseedTop, kFieldHaze), look.Far(kRapeseed, kFieldHaze) },
                { look.Far(kWheat, kFieldHaze), look.Far(kWheatRow, kFieldHaze) },
                { look.Far(kSprouts, kFieldHaze), look.Far(kOrchard, kFieldHaze) },
            };
            int start = -static_cast<int>(Hash(0, 320u) % 20u);
            for (int index = 0; start < frame.Columns; ++index)
            {
                const int length = 14 + static_cast<int>(Hash(index, 321u) % 22u);
                std::uint32_t kind = Hash(index, 322u) % 7u;
                if (kind == 1u && length > 24)
                    kind = 0u;
                const int end = std::min(frame.Columns, start + length);
                const float skewFrom = F(1 + static_cast<int>(Hash(index, 323u) % 4u));
                const float skewTo = F(1 + static_cast<int>(Hash(index, 324u) % 4u));
                const float rows = F(3 + static_cast<int>(Hash(index, 325u) % 4u));
                const auto topAt = [&](int column)
                    {
                        return HillTop(surface, column) + std::round(skewFrom + (skewTo - skewFrom) * F(column - start) / F(std::max(1, length)));
                    };
                if (kind < 4u)
                {
                    float highest = surface;
                    for (int column = std::max(0, start); column < end; ++column)
                        highest = std::min(highest, topAt(column));
                    for (float row = highest; row < highest + rows + 4.0f && row < surface + 3.0f; row += 1.0f)
                    {
                        int run = std::max(0, start);
                        int runTone = -1;
                        for (int column = run; column <= end; ++column)
                        {
                            int tone = -1;
                            const float line = column < end ? row - topAt(column) : -1.0f;
                            if (line >= 0.0f && line < rows)
                                tone = kind == 3u ? 0 : static_cast<int>(line) % 2;
                            if (tone == runTone)
                                continue;
                            if (runTone >= 0)
                                paint.Fill(F(run), row, F(column), row + 1.0f, fields[kind][runTone]);
                            run = column;
                            runTone = tone;
                        }
                        if (kind == 3u)
                        {
                            for (int column = std::max(0, start) + 1 + static_cast<int>(row) % 2; column < end - 1; column += 3)
                            {
                                const float top = topAt(column);
                                if (top + 1.0f <= row && row < top + rows && static_cast<int>(row) % 2 == 0)
                                    paint.Fill(F(column), row, F(column + 1), row + 1.0f, fields[3][1]);
                            }
                        }
                    }
                    int run = std::max(0, start);
                    float runTop = topAt(run) + rows;
                    for (int column = run + 1; column <= end; ++column)
                    {
                        const float here = column < end ? topAt(column) + rows : -1.0f;
                        if (here == runTop)
                            continue;
                        paint.Fill(F(run), runTop, F(column), runTop + 1.0f, hedge);
                        run = column;
                        runTop = here;
                    }
                    if (start > 0)
                        paint.Fill(F(start), topAt(start) - 1.0f, F(start + 1), topAt(start) + rows + 1.0f, hedge);
                }
                start = end;
            }

            Sprite farm;
            DrawFarm(farm, SideOfLight(frame, farmX));
            float ground = 0.0f;
            for (int column = static_cast<int>(farmX) - 1; column <= static_cast<int>(farmX) + 22; ++column)
                ground = std::max(ground, HillTop(surface, column));
            std::array<ImVec4, std::size(kFarmTones)> farmColors{};
            for (std::size_t index = 1; index < farmColors.size(); ++index)
                farmColors[index] = look.Far(kFarmTones[index], kHaze);
            farmColors[11] = Mix(farmColors[11], kLamplight, std::clamp((0.7f - look.Daylight) * 2.0f, 0.0f, 1.0f));
            farm.Flush(paint, farmX, ground + 2.0f, farmColors.data());
        }

        void FarCrown(const Canvas& paint, float x, float y, float radius, const ImVec4* tones, float toLight)
        {
            const auto rows = static_cast<int>(radius);
            for (int row = -rows; row <= rows; ++row)
            {
                const float share = F(row) / (radius + 0.5f);
                const float half = std::round(radius * 1.1f * std::sqrt(std::max(0.0f, 1.0f - share * share)));
                if (half <= 0.0f)
                    continue;
                const float top = y + F(row);
                paint.Fill(x - half, top, x + half + 1.0f, top + 1.0f, tones[0]);
                const float lit = std::min(half * 2.0f + 1.0f, std::round(half * (1.15f - share * 0.65f)));
                if (toLight < 0.0f)
                    paint.Fill(x - half, top, x - half + lit, top + 1.0f, tones[1]);
                else
                    paint.Fill(x + half + 1.0f - lit, top, x + half + 1.0f, top + 1.0f, tones[1]);
            }
            const float spot = x + toLight * std::round(radius * 0.45f);
            paint.Fill(spot - 1.0f, y - F(rows) + 1.0f, spot + 1.0f, y - F(rows) + 2.0f, tones[2]);
        }

        void PaintTreeLine(const Frame& frame, const Look& look, float farmX)
        {
            constexpr float kHaze = 0.36f;
            const ImVec4 greens[] = { look.Far(kTreeShade, kHaze), look.Far(kTreeLit, kHaze), look.Far(kTreeBright, kHaze) };
            const ImVec4 pinks[] = { look.Far(kPinkShade, kHaze), look.Far(kPinkLit, kHaze), look.Far(kPinkBright, kHaze) };
            const ImVec4 bark = look.Far(kTrunkFar, kHaze);
            constexpr int kSpacing = 8;
            for (int index = -1; index * kSpacing < frame.Columns + kSpacing; ++index)
            {
                const std::uint32_t mixed = Hash(index, 41u);
                if (mixed % 10u < 3u)
                    continue;
                const float x = F(index * kSpacing + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(kSpacing)));
                if (x > farmX - 6.0f && x < farmX + 28.0f)
                    continue;
                const float radius = 3.0f + F(static_cast<int>((mixed >> 12) % 4u));
                const float trunk = 1.0f + F(static_cast<int>((mixed >> 16) % 2u));
                const float base = frame.Surface + 2.0f;
                const float middle = base - trunk - radius;
                const std::uint32_t kind = (mixed >> 20) % 10u;
                frame.Paint.Fill(x, middle, x + 1.0f, base, bark);
                FarCrown(frame.Paint, x, middle, radius, kind < 2u ? pinks : greens, SideOfLight(frame, x));
            }
        }

        void DrawFlower(const Frame& frame, const Look& look, float x, float y, int kind)
        {
            const Canvas& paint = frame.Paint;
            const float share = std::clamp((y - frame.Surface) / std::max(1.0f, frame.Shore - frame.Surface), 0.0f, 1.0f);
            const float haze = frame.HazeAt(y);
            const ImVec4 petal = Mix(look(kPetals[kind]), look.Haze, haze);
            if (share < 0.3f)
            {
                paint.Fill(x, y - 1.0f, x + 1.0f, y, petal);
                return;
            }
            const ImVec4 stalk = Mix(look(kStalk), look.Haze, haze);
            if (share < 0.62f)
            {
                paint.Fill(x, y - 1.0f, x + 1.0f, y, stalk);
                paint.Fill(x, y - 2.0f, x + (kind == 0 ? 2.0f : 1.0f), y - 1.0f, petal);
                return;
            }
            const ImVec4 heart = Mix(look(kHearts[kind]), look.Haze, haze);
            paint.Fill(x, y - 2.0f, x + 1.0f, y, stalk);
            switch (kind)
            {
            case 0:
                paint.Fill(x - 1.0f, y - 3.0f, x + 2.0f, y - 2.0f, petal);
                paint.Fill(x, y - 4.0f, x + 1.0f, y - 3.0f, petal);
                paint.Fill(x, y - 3.0f, x + 1.0f, y - 2.0f, heart);
                break;
            case 1:
                paint.Fill(x - 1.0f, y - 4.0f, x + 1.0f, y - 2.0f, petal);
                paint.Fill(x - 1.0f, y - 4.0f, x, y - 3.0f, heart);
                break;
            case 2:
            case 3:
                paint.Fill(x - 1.0f, y - 4.0f, x + 1.0f, y - 2.0f, petal);
                paint.Fill(x, y - 4.0f, x + 1.0f, y - 3.0f, heart);
                paint.Fill(x + 1.0f, y - 2.0f, x + 2.0f, y - 1.0f, stalk);
                break;
            default:
                paint.Fill(x - 1.0f, y - 3.0f, x + 1.0f, y - 2.0f, petal);
                paint.Fill(x, y - 4.0f, x + 1.0f, y - 3.0f, petal);
                paint.Fill(x - 1.0f, y - 3.0f, x, y - 2.0f, heart);
                break;
            }
        }

        void PaintMeadow(const Frame& frame, const Look& look)
        {
            const Canvas& paint = frame.Paint;
            const float surface = frame.Surface;
            const float shore = frame.Shore;
            const float depth = std::max(1.0f, shore - surface);
            const float width = F(frame.Columns);
            const auto at = [&](ImVec4 color, float row) { return Mix(look(color), look.Haze, frame.HazeAt(row)); };
            const auto groundAt = [&](float row) { return at(Mix(kMeadowBack, kMeadowFront, std::clamp((row - surface) / depth, 0.0f, 1.0f)), row); };

            const auto edgeOf = [&](int column) { return std::round(surface + 0.9f * std::sin(F(column) * 0.11f + 0.5f) + 0.7f * std::sin(F(column) * 0.27f + 1.7f)); };
            const ImVec4 edge[] = { groundAt(surface), at(kMeadowRim, surface) };
            Columns(paint, frame.Columns, edge, [&](int column) { return Column{ edgeOf(column), surface + 3.0f, 0 }; });
            Columns(paint, frame.Columns, edge, [&](int column) { return Column{ edgeOf(column), edgeOf(column) + 1.0f, 1 }; });
            for (int column = 0; column < frame.Columns; ++column)
            {
                const std::uint32_t mixed = Hash(column, 501u);
                if (mixed % 4u == 0u)
                {
                    const float top = edgeOf(column);
                    paint.Fill(F(column), top - 1.0f - F(static_cast<int>((mixed >> 8) % 2u)), F(column + 1), top, edge[1]);
                }
            }

            constexpr int kBands = 9;
            for (int band = 0; band < kBands; ++band)
            {
                const float from = std::round(surface + 3.0f + (shore - surface - 1.0f) * F(band) / F(kBands));
                const float to = std::round(surface + 3.0f + (shore - surface - 1.0f) * F(band + 1) / F(kBands));
                paint.Fill(0.0f, from, width, to, groundAt((from + to) * 0.5f));
            }

            const int patches = std::max(4, frame.Columns / 28);
            for (int index = 0; index < patches; ++index)
            {
                const float share = 0.08f + 0.84f * Roll(index, 511u);
                const float cy = std::round(surface + 4.0f + share * (depth - 7.0f));
                const float cx = std::round(Roll(index, 512u) * width);
                const float scale = frame.ScaleAt(cy);
                const float rx = (9.0f + Roll(index, 513u) * 14.0f) * scale;
                const float ry = std::max(1.0f, std::round((1.5f + share * 2.5f) * scale));
                const ImVec4 color = Mix(groundAt(cy), at(index % 2 == 0 ? kClover : kFreshGrass, cy), 0.6f);
                for (float row = -ry; row <= ry; row += 1.0f)
                {
                    const float part = row / (ry + 0.5f);
                    const float half = std::round(rx * std::sqrt(std::max(0.0f, 1.0f - part * part)));
                    if (half > 0.0f)
                        paint.Fill(cx - half, cy + row, cx + half, cy + row + 1.0f, color);
                }
            }

            float row = surface + 3.0f;
            for (int line = 0; row < shore - 1.0f; ++line)
            {
                const float share = (row - surface) / depth;
                const int step = 4 + static_cast<int>(std::round(share * 7.0f));
                const ImVec4 dark = at(kBladeDark, row);
                const ImVec4 mid = at(kBladeMid, row);
                const ImVec4 light = at(kBladeLight, row);
                for (int index = 0; index * step < frame.Columns; ++index)
                {
                    const std::uint32_t mixed = Hash(index * 131 + line * 7919, 521u);
                    if (mixed % 8u < 3u)
                        continue;
                    const float x = F(index * step + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(step)));
                    const float y = row + F(static_cast<int>((mixed >> 16) % 2u));
                    const bool bright = (mixed >> 20) % 3u == 0u;
                    if (share < 0.28f)
                    {
                        paint.Fill(x, y, x + 1.0f, y + 1.0f, bright ? light : dark);
                    }
                    else if (share < 0.6f)
                    {
                        paint.Fill(x, y - 1.0f, x + 1.0f, y + 1.0f, dark);
                        if (bright)
                            paint.Fill(x + 1.0f, y, x + 2.0f, y + 1.0f, light);
                    }
                    else
                    {
                        const float tall = 2.0f + F(static_cast<int>((mixed >> 22) % 2u));
                        paint.Fill(x, y - tall, x + 1.0f, y + 1.0f, dark);
                        paint.Fill(x + 1.0f, y - tall + 1.0f, x + 2.0f, y + 1.0f, bright ? light : mid);
                        paint.Fill(x - 1.0f, y - 1.0f, x, y + 1.0f, mid);
                    }
                }
                row += F(2 + static_cast<int>(std::round(share * 3.0f)));
            }

            const int clusters = std::max(6, frame.Columns / 16);
            for (int index = 0; index < clusters; ++index)
            {
                const float share = 0.04f + 0.92f * Roll(index, 531u);
                const float cy = surface + 4.0f + share * (depth - 7.0f);
                const float cx = Roll(index, 532u) * width;
                const float scale = frame.ScaleAt(cy);
                const auto kind = static_cast<int>(Hash(index, 533u) % 6u);
                const int count = 4 + static_cast<int>(Hash(index, 534u) % 7u);
                const float rx = (6.0f + 8.0f * Roll(index, 535u)) * scale;
                const float ry = (1.5f + 2.5f * share) * scale;
                for (int each = 0; each < count; ++each)
                {
                    const float angle = Roll(index * 31 + each, 536u) * 6.2832f;
                    const float reach = std::sqrt(Roll(index * 31 + each, 537u));
                    DrawFlower(frame, look, std::round(cx + std::cos(angle) * rx * reach), std::round(cy + std::sin(angle) * ry * reach), kind);
                }
            }
            const int singles = frame.Columns * static_cast<int>(depth) / 110;
            for (int index = 0; index < singles; ++index)
            {
                const float share = Roll(index, 541u);
                const float y = std::round(surface + 4.0f + share * (depth - 6.0f));
                DrawFlower(frame, look, std::round(Roll(index, 542u) * width), y, Hash(index, 543u) % 3u == 0u ? 1 : 0);
            }
        }

        void PaintLake(const Frame& frame, const Look& look)
        {
            const Canvas& paint = frame.Paint;
            const float shore = frame.Shore;
            const float bottom = F(frame.Rows);
            const float width = F(frame.Columns);
            const ImVec4 colors[] = { look(kWaterNear), look(kWater), look(kWaterDeep), look(kWaterEdge), look(kBank) };
            const auto edgeOf = [&](int column) { return shore + (std::sin(F(column) * 0.11f + 2.0f) > 0.55f ? 1.0f : 0.0f); };
            Columns(paint, frame.Columns, colors, [&](int column) { return Column{ edgeOf(column) - 1.0f, edgeOf(column), 4 }; });
            Columns(paint, frame.Columns, colors, [&](int column) { return Column{ edgeOf(column), shore + 3.0f, 0 }; });
            const float deepFrom = shore + std::floor((bottom - shore) * 0.5f);
            paint.Fill(0.0f, shore + 3.0f, width, deepFrom + 2.0f, colors[1]);
            Columns(paint, frame.Columns, colors, [&](int column) { return Column{ deepFrom + std::round(std::sin(F(column) * 0.07f + 1.0f) * 1.5f), bottom, 2 }; });
            Columns(paint, frame.Columns, colors, [&](int column) { return Hash(column / 3, 551u) % 4u == 0u ? Column{} : Column{ edgeOf(column), edgeOf(column) + 1.0f, 3 }; });

            const ImVec4 ripple = look(kRipple);
            const ImVec4 rippleDark = look(kRippleDark);
            int line = 0;
            for (float row = shore + 5.0f; row < bottom - 1.0f; row += 3.0f, ++line)
            {
                const float share = (row - shore) / std::max(1.0f, bottom - shore);
                const int cell = 11 + static_cast<int>(std::round(share * 8.0f));
                for (int index = 0; index * cell < frame.Columns; ++index)
                {
                    const std::uint32_t mixed = Hash(index * 977 + line, 552u);
                    if (mixed % 3u != 0u)
                        continue;
                    const float x = F(index * cell + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(cell)));
                    const float length = 2.0f + std::round(share * 5.0f) + F(static_cast<int>((mixed >> 16) % 3u));
                    const float y = row + F(static_cast<int>((mixed >> 20) % 2u));
                    paint.Fill(x, y, x + length, y + 1.0f, (mixed >> 24) % 3u == 0u ? rippleDark : ripple);
                }
            }

            const ImVec4 pad = look(kPad);
            const ImVec4 padShade = look(kPadShade);
            const ImVec4 lily = look(kPetals[3]);
            const ImVec4 lilyHeart = look(kHearts[0]);
            constexpr float kPads[] = { 0.035f, 0.075f, 0.93f, 0.965f, 0.11f };
            for (int index = 0; index < static_cast<int>(std::size(kPads)); ++index)
            {
                const float x = std::round(width * kPads[index] + (Roll(index, 561u) - 0.5f) * 4.0f);
                const float y = std::round(shore + 5.0f + Roll(index, 562u) * std::min(7.0f, (bottom - shore) * 0.3f));
                paint.Fill(x - 2.0f, y, x + 3.0f, y + 1.0f, pad);
                paint.Fill(x - 3.0f, y + 1.0f, x + 4.0f, y + 2.0f, padShade);
                paint.Fill(x + 1.0f, y, x + 2.0f, y + 1.0f, colors[1]);
                if (index % 2 == 0)
                {
                    paint.Fill(x - 2.0f, y - 1.0f, x + 1.0f, y, lily);
                    paint.Fill(x - 1.0f, y - 2.0f, x, y - 1.0f, lily);
                    paint.Fill(x - 1.0f, y - 1.0f, x, y, lilyHeart);
                }
            }

            const Light& sun = frame.Sun;
            const auto beat = static_cast<int>(frame.Time * 1.5);
            const ImVec4 glint = sun.Moon ? Color(0.80f, 0.86f, 1.0f, 0.32f) : Color(1.0f, 0.97f, 0.84f, 0.48f + 0.2f * look.Glow);
            int streak = 0;
            for (float row = shore + 3.0f; row < bottom; row += 2.0f, ++streak)
            {
                const float share = (row - shore) / std::max(1.0f, bottom - shore);
                const float half = 1.0f + std::round(share * 6.0f);
                const std::uint32_t mixed = Hash(streak + beat * 37, 571u);
                const float shift = F(static_cast<int>(mixed % 3u)) - 1.0f;
                const float gap = F(static_cast<int>((mixed >> 4) % 3u));
                paint.Fill(sun.X - half + shift, row, sun.X - gap + shift, row + 1.0f, glint);
                paint.Fill(sun.X + gap + 1.0f + shift, row, sun.X + half + 1.0f + shift, row + 1.0f, glint);
            }
        }

        class SpringScene final : public Scene
        {
        public:
            void DrawBack(const Frame& frame) override
            {
                const Look look = LookOf(frame);
                const float farmX = std::round(F(frame.Columns) * 0.6f);
                PaintSky(frame, look);
                PaintMountains(frame, look);
                PaintHills(frame, look, farmX);
                PaintTreeLine(frame, look, farmX);
                PaintMeadow(frame, look);
                PaintLake(frame, look);
            }

            void PlaceProps(const Frame& frame, std::vector<Prop>& props) override
            {
                if (frame.Narrow)
                {
                    constexpr Kind kNarrowKinds[] = { Kind::Cherry, Kind::Forsythia };
                    constexpr float kNarrowAcross[] = { 0.12f, 0.86f };
                    const float nearBack = frame.FeetBack + 2.0f;
                    const float nearFront = std::max(nearBack, frame.FeetFront - 6.0f);
                    for (int index = 0; index < 2; ++index)
                    {
                        const Kind kind = kNarrowKinds[index];
                        const KindShape& shape = kShapes[static_cast<std::size_t>(kind)];
                        Prop prop;
                        prop.X = std::round(F(frame.Columns) * kNarrowAcross[index]);
                        prop.Ground = std::round(nearBack + (nearFront - nearBack) * (shape.DepthFrom + shape.DepthTo) * 0.5f);
                        prop.Foot = shape.Foot;
                        prop.Cover = shape.Cover;
                        prop.CoverHeight = shape.CoverHeight;
                        prop.Height = shape.Height;
                        prop.Crown = shape.Crown;
                        prop.Kind = static_cast<std::uint32_t>(kind);
                        prop.Seed = Hash(index, 603u + static_cast<std::uint32_t>(kind));
                        props.push_back(prop);
                    }
                    return;
                }
                constexpr float kEdge = 10.0f;
                const float right = F(frame.Columns) - kEdge;
                const float back = frame.FeetBack + 2.0f;
                const float front = frame.FeetFront - 6.0f;
                if (right - kEdge < 80.0f || front <= back)
                    return;
                const int count = std::clamp(static_cast<int>(std::round((right - kEdge) / 48.0f)), 4, 7);
                for (int index = 0; index < count; ++index)
                {
                    const Kind kind = kOrders[count - 4][index];
                    const KindShape& shape = kShapes[static_cast<std::size_t>(kind)];
                    const float slot = (F(index) + 0.5f + (Roll(index, 601u) - 0.5f) * 0.36f) / F(count);
                    const float depth = shape.DepthFrom + (shape.DepthTo - shape.DepthFrom) * Roll(index, 602u);
                    Prop prop;
                    prop.X = std::round(kEdge + (right - kEdge) * slot);
                    prop.Ground = std::round(back + (front - back) * depth);
                    prop.Foot = shape.Foot;
                    prop.Cover = shape.Cover;
                    prop.CoverHeight = shape.CoverHeight;
                    prop.Height = shape.Height;
                    prop.Crown = shape.Crown;
                    prop.Kind = static_cast<std::uint32_t>(kind);
                    prop.Seed = Hash(index, 603u + static_cast<std::uint32_t>(kind));
                    props.push_back(prop);
                }
            }

            void DrawProp(const Frame& frame, const Prop& prop, const Canvas& paint) override
            {
                const Look look = LookOf(frame);
                std::array<ImVec4, std::size(kTones)> colors{};
                for (std::size_t index = 1; index < colors.size(); ++index)
                    colors[index] = look(kTones[index]);
                const Toward light = TowardLight(frame, prop.X);
                Sprite sprite;
                switch (static_cast<Kind>(prop.Kind))
                {
                case Kind::Cherry:
                    DrawCherry(sprite, light, prop.Seed);
                    break;
                case Kind::Birch:
                    DrawBirch(sprite, light, prop.Seed);
                    break;
                case Kind::Forsythia:
                    DrawForsythia(sprite, light, prop.Seed);
                    break;
                case Kind::Apple:
                    DrawApple(sprite, light, prop.Seed);
                    break;
                case Kind::Lilac:
                    DrawLilac(sprite, light, prop.Seed);
                    break;
                case Kind::Rock:
                    DrawRock(sprite, light, prop.Seed);
                    break;
                case Kind::Stump:
                    DrawStump(sprite, light, prop.Seed);
                    break;
                }
                sprite.Flush(paint, 0.0f, 0.0f, colors.data());
            }

            void DrawFront(const Frame& frame) override
            {
                const Look look = LookOf(frame);
                const Canvas& paint = frame.Paint;
                const float shore = frame.Shore;
                const ImVec4 dark = look(kBladeDark);
                const ImVec4 mid = look(kBladeMid);
                const ImVec4 light = look(kBladeLight);
                const ImVec4 stalk = look(kStalk);
                for (int column = 4; column < frame.Columns - 4; ++column)
                {
                    const std::uint32_t mixed = Hash(column, 701u);
                    if (mixed % 5u != 0u)
                        continue;
                    const float x = F(column);
                    const float side = SideOfLight(frame, x);
                    const std::uint32_t kind = (mixed >> 8) % 20u;
                    if (kind < 10u)
                    {
                        const int blades = 2 + static_cast<int>((mixed >> 13) % 3u);
                        const int tall = 3 + static_cast<int>((mixed >> 16) % 3u);
                        for (int blade = 0; blade < blades; ++blade)
                        {
                            const float bx = x + F(blade);
                            const float height = F(tall - std::abs(blade * 2 - blades + 1) / 2);
                            const float lean = blade * 2 < blades - 1 ? -1.0f : blade * 2 > blades - 1 ? 1.0f : 0.0f;
                            const ImVec4 color = blade % 2 == 0 ? (lean == side ? light : mid) : dark;
                            paint.Fill(bx, shore - height + 1.0f, bx + 1.0f, shore, color);
                            paint.Fill(bx + lean, shore - height, bx + lean + 1.0f, shore - height + 1.0f, color);
                        }
                        continue;
                    }
                    switch (kind)
                    {
                    case 10u:
                    case 11u:
                    {
                        const ImVec4 petal = look(kPetals[0]);
                        paint.Fill(x, shore - 3.0f, x + 1.0f, shore, stalk);
                        paint.Fill(x + 1.0f, shore - 1.0f, x + 2.0f, shore, mid);
                        paint.Fill(x - 1.0f, shore - 5.0f, x + 2.0f, shore - 4.0f, petal);
                        paint.Fill(x, shore - 6.0f, x + 1.0f, shore - 3.0f, petal);
                        paint.Fill(x, shore - 5.0f, x + 1.0f, shore - 4.0f, look(kHearts[0]));
                        break;
                    }
                    case 12u:
                    {
                        paint.Fill(x, shore - 3.0f, x + 1.0f, shore, stalk);
                        paint.Fill(x - 1.0f, shore - 1.0f, x, shore, dark);
                        paint.Fill(x - 1.0f, shore - 5.0f, x + 1.0f, shore - 3.0f, look(kPetals[1]));
                        paint.Fill(side < 0.0f ? x - 1.0f : x, shore - 5.0f, side < 0.0f ? x : x + 1.0f, shore - 4.0f, look(kHearts[1]));
                        break;
                    }
                    case 13u:
                    {
                        const ImVec4 seed = look(Color(0.930f, 0.940f, 0.920f));
                        paint.Fill(x, shore - 4.0f, x + 1.0f, shore, stalk);
                        paint.Fill(x - 1.0f, shore - 6.0f, x + 2.0f, shore - 5.0f, seed);
                        paint.Fill(x, shore - 6.0f, x + 1.0f, shore - 4.0f, seed);
                        paint.Fill(x, shore - 6.0f, x + 1.0f, shore - 5.0f, look(Color(0.700f, 0.700f, 0.640f)));
                        break;
                    }
                    case 14u:
                    {
                        const ImVec4 bell = look(kPetals[5]);
                        paint.Fill(x, shore - 4.0f, x + 1.0f, shore, stalk);
                        paint.Fill(x + 1.0f, shore - 5.0f, x + 2.0f, shore - 4.0f, stalk);
                        paint.Fill(x + 2.0f, shore - 4.0f, x + 3.0f, shore - 3.0f, bell);
                        paint.Fill(x - 1.0f, shore - 3.0f, x, shore - 2.0f, bell);
                        paint.Fill(x + 1.0f, shore - 3.0f, x + 2.0f, shore - 2.0f, bell);
                        break;
                    }
                    case 15u:
                    {
                        paint.Fill(x, shore - 3.0f, x + 1.0f, shore, stalk);
                        paint.Fill(x + 1.0f, shore - 2.0f, x + 2.0f, shore, mid);
                        paint.Fill(x - 1.0f, shore - 5.0f, x + 1.0f, shore - 3.0f, look(kPetals[2]));
                        paint.Fill(side < 0.0f ? x - 1.0f : x, shore - 5.0f, side < 0.0f ? x : x + 1.0f, shore - 4.0f, look(kHearts[2]));
                        break;
                    }
                    case 16u:
                    {
                        paint.Fill(x, shore - 2.0f, x + 4.0f, shore, look(kTones[kStone + 1]));
                        paint.Fill(x + 1.0f, shore - 3.0f, x + 3.0f, shore - 2.0f, look(kTones[kStone + 1]));
                        paint.Fill(side < 0.0f ? x + 1.0f : x + 2.0f, shore - 3.0f, side < 0.0f ? x + 2.0f : x + 3.0f, shore - 2.0f, look(kTones[kStone + 3]));
                        paint.Fill(side < 0.0f ? x + 3.0f : x, shore - 2.0f, side < 0.0f ? x + 4.0f : x + 1.0f, shore, look(kTones[kStone]));
                        break;
                    }
                    default:
                    {
                        paint.Fill(x - 1.0f, shore - 2.0f, x + 2.0f, shore, dark);
                        paint.Fill(x, shore - 3.0f, x + 1.0f, shore - 2.0f, mid);
                        paint.Fill(x + (side < 0.0f ? -1.0f : 1.0f), shore - 2.0f, x + (side < 0.0f ? 0.0f : 2.0f), shore - 1.0f, light);
                        break;
                    }
                    }
                }

                const ImVec4 reed = look(Color(0.420f, 0.560f, 0.260f));
                const ImVec4 reedDark = look(Color(0.280f, 0.400f, 0.200f));
                const ImVec4 cattail = look(Color(0.420f, 0.260f, 0.150f));
                for (const float at : { 0.025f, 0.965f })
                {
                    const float left = std::round(F(frame.Columns) * at);
                    for (int stalkIndex = 0; stalkIndex < 6; ++stalkIndex)
                    {
                        const float x = left + F(stalkIndex) + F(static_cast<int>(Hash(stalkIndex, 711u) % 2u));
                        const float tall = 4.0f + F(static_cast<int>(Hash(stalkIndex, 712u) % 3u));
                        paint.Fill(x, shore + 1.0f - tall, x + 1.0f, shore + 1.0f, stalkIndex % 2 == 0 ? reed : reedDark);
                        if (stalkIndex % 3 == 1)
                            paint.Fill(x, shore + 1.0f - tall, x + 1.0f, shore + 3.0f - tall, cattail);
                    }
                }
            }

            ImVec4 HazeColor(const Frame& frame) override
            {
                return HazeOf(frame.Daylight);
            }
        };
    }

    std::unique_ptr<Scene> MakeSpring()
    {
        return std::make_unique<SpringScene>();
    }
}
