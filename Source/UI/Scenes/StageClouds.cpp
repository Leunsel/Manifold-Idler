#include "UI/StageScene.h"

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

        ImVec4 Faded(ImVec4 color, float alpha)
        {
            color.w *= alpha;
            return color;
        }

        struct AirColor
        {
            ImVec4 Day;
            ImVec4 Night;
            ImVec4 Low;
        };

        constexpr AirColor kZenith{ Col(0.120f, 0.290f, 0.560f), Col(0.018f, 0.028f, 0.075f), Col(0.150f, 0.170f, 0.370f) };
        constexpr AirColor kMidSky{ Col(0.250f, 0.470f, 0.740f), Col(0.040f, 0.060f, 0.135f), Col(0.420f, 0.350f, 0.570f) };
        constexpr AirColor kHorizon{ Col(0.640f, 0.790f, 0.910f), Col(0.105f, 0.140f, 0.240f), Col(0.920f, 0.600f, 0.580f) };

        constexpr AirColor kCloudRim{ Col(1.000f, 1.000f, 1.000f), Col(0.420f, 0.480f, 0.640f), Col(1.000f, 0.870f, 0.660f) };
        constexpr AirColor kCloudLit{ Col(0.945f, 0.965f, 0.995f), Col(0.320f, 0.370f, 0.520f), Col(0.990f, 0.770f, 0.700f) };
        constexpr AirColor kCloudBody{ Col(0.860f, 0.905f, 0.965f), Col(0.235f, 0.275f, 0.400f), Col(0.870f, 0.660f, 0.710f) };
        constexpr AirColor kCloudShade{ Col(0.745f, 0.810f, 0.910f), Col(0.180f, 0.215f, 0.330f), Col(0.700f, 0.570f, 0.690f) };
        constexpr AirColor kCloudDeep{ Col(0.640f, 0.720f, 0.860f), Col(0.140f, 0.170f, 0.275f), Col(0.570f, 0.470f, 0.640f) };

        constexpr AirColor kRock{ Col(0.400f, 0.465f, 0.590f), Col(0.055f, 0.070f, 0.125f), Col(0.440f, 0.350f, 0.500f) };
        constexpr AirColor kRockLit{ Col(0.560f, 0.615f, 0.720f), Col(0.105f, 0.125f, 0.195f), Col(0.780f, 0.520f, 0.500f) };
        constexpr AirColor kSnow{ Col(0.740f, 0.800f, 0.900f), Col(0.160f, 0.190f, 0.285f), Col(0.700f, 0.570f, 0.700f) };
        constexpr AirColor kSnowLit{ Col(0.975f, 0.985f, 1.000f), Col(0.360f, 0.410f, 0.540f), Col(1.000f, 0.820f, 0.690f) };

        struct Mood
        {
            float Day = 1.0f;
            float Low = 0.0f;

            ImVec4 Air(const AirColor& color) const
            {
                return Mix(DayNight(color.Day, color.Night, Day), color.Low, Low * 0.8f);
            }
        };

        struct Look
        {
            Mood Now;
            int Columns = 0;
            int Rows = 0;
            int Ground = 0;
            float Side = -1.0f;
            float LightX = -0.6f;
            float LightY = -0.8f;
            float Size = 1.0f;
            double Time = 0.0;
            bool Game = false;
            SceneSeason Season = SceneSeason::Summer;
        };

        Look LookOf(const Frame& frame)
        {
            Look look;
            look.Now.Day = std::clamp(frame.Daylight, 0.0f, 1.0f);
            look.Now.Low = std::clamp(1.0f - std::fabs(frame.Daylight - 0.5f) / 0.4f, 0.0f, 1.0f);
            look.Columns = frame.Columns;
            look.Rows = frame.Rows;
            look.Ground = static_cast<int>(frame.Ground);
            look.Side = frame.Daylight >= 0.3f ? -1.0f : 1.0f;
            look.LightY = -0.8f + 0.35f * look.Now.Low;
            look.LightX = look.Side * std::sqrt(1.0f - look.LightY * look.LightY);
            look.Size = std::clamp(frame.Ground / 144.0f, 0.3f, 1.5f);
            look.Time = frame.Time;
            look.Game = frame.Game;
            look.Season = frame.Season;
            return look;
        }

        void Box(const Canvas& paint, int left, int top, int right, int bottom, ImVec4 color)
        {
            paint.Fill(static_cast<float>(left), static_cast<float>(top), static_cast<float>(right), static_cast<float>(bottom), color);
        }

        template <typename Cell>
        void PaintRuns(const Canvas& paint, int left, int right, int top, int bottom, const ImVec4* colors, Cell&& cell)
        {
            for (int y = top; y < bottom; ++y)
            {
                int start = left;
                int current = -1;
                for (int x = left; x <= right; ++x)
                {
                    const int next = x < right ? cell(x, y) : -1;
                    if (next == current)
                        continue;
                    if (current >= 0)
                        Box(paint, start, y, x, y + 1, colors[current]);
                    start = x;
                    current = next;
                }
            }
        }

        int Drift(const Look& look, float start, float margin, double speed)
        {
            const double span = static_cast<double>(look.Columns) + static_cast<double>(margin) * 2.0;
            const double at = std::fmod(static_cast<double>(start + margin) + look.Time * speed, span);
            return static_cast<int>(std::floor(at - static_cast<double>(margin)));
        }

        ImVec4 SkyAt(const Mood& mood, float share)
        {
            const ImVec4 zenith = mood.Air(kZenith);
            const ImVec4 middle = mood.Air(kMidSky);
            const ImVec4 horizon = mood.Air(kHorizon);
            return share < 0.55f ? Mix(zenith, middle, share / 0.55f) : Mix(middle, horizon, (share - 0.55f) / 0.45f);
        }

        float SkyShare(const Look& look, float y)
        {
            const float share = std::clamp(y / std::max(8.0f, static_cast<float>(look.Ground)), 0.0f, 1.0f);
            return std::pow(share, 1.5f);
        }

        ImVec4 SkyRow(const Look& look, float y)
        {
            return SkyAt(look.Now, SkyShare(look, y));
        }

        void PaintSky(const Canvas& paint, const Look& look)
        {
            constexpr int kBands = 14;
            std::array<ImVec4, kBands> colors{};
            for (int band = 0; band < kBands; ++band)
                colors[static_cast<std::size_t>(band)] = SkyAt(look.Now, static_cast<float>(band) / static_cast<float>(kBands - 1));
            const int bottom = look.Ground + 4;
            int start = 0;
            int current = -1;
            for (int y = 0; y <= bottom; ++y)
            {
                int band = -1;
                if (y < bottom)
                {
                    const float at = SkyShare(look, static_cast<float>(y)) * static_cast<float>(kBands - 1);
                    band = static_cast<int>(at);
                    if (at - static_cast<float>(band) > 0.7f && (y & 1) == 0)
                        ++band;
                    band = std::min(band, kBands - 1);
                }
                if (band == current)
                    continue;
                if (current >= 0)
                    Box(paint, 0, start, look.Columns, y, colors[static_cast<std::size_t>(current)]);
                start = y;
                current = band;
            }
        }

        void PaintStars(const Canvas& paint, const Look& look)
        {
            const float night = 1.0f - look.Now.Day;
            if (night <= 0.0f)
                return;
            constexpr int kCellWidth = 14;
            constexpr int kCellHeight = 10;
            const int high = static_cast<int>(static_cast<float>(look.Ground) * 0.8f);
            for (int row = 0; row * kCellHeight < high; ++row)
            {
                for (int column = 0; column * kCellWidth < look.Columns; ++column)
                {
                    const std::uint32_t mixed = Noise((static_cast<std::uint32_t>(column) * 73856093u) ^ (static_cast<std::uint32_t>(row) * 19349663u) ^ 0x5A17C3u);
                    if ((mixed >> 24) >= 135u)
                        continue;
                    const int x = column * kCellWidth + static_cast<int>(mixed % static_cast<std::uint32_t>(kCellWidth));
                    const int y = row * kCellHeight + static_cast<int>((mixed >> 12) % static_cast<std::uint32_t>(kCellHeight));
                    if (x >= look.Columns || y >= high)
                        continue;
                    const float fade = std::min(1.0f, 1.6f * (1.0f - static_cast<float>(y) / static_cast<float>(high)));
                    const float bright = ((mixed >> 8) & 7u) == 0u ? 0.7f : 0.32f;
                    Box(paint, x, y, x + 1, y + 1, ImVec4(0.740f, 0.800f, 0.950f, night * fade * bright));
                }
            }
        }

        void Round(const Canvas& paint, float x, float y, float radius, ImVec4 color)
        {
            const float outer = radius + 0.35f;
            for (float row = -radius; row <= radius; row += 1.0f)
            {
                const float half = std::floor(std::sqrt(std::max(0.0f, outer * outer - row * row)));
                paint.Fill(x - half, y + row, x + half + 1.0f, y + row + 1.0f, color);
            }
        }

        float LightRadius(const Look& look)
        {
            return std::clamp(std::round(static_cast<float>(look.Ground) * 0.05f), 5.0f, 8.0f);
        }

        float LightRow(const Look& look, float radius, float up)
        {
            const float high = std::max(radius + 6.0f, std::round(static_cast<float>(look.Ground) * 0.2f));
            const float low = static_cast<float>(look.Ground) + radius;
            return std::round(low + (high - low) * up);
        }

        float SunUp(const Look& look)
        {
            return std::clamp((look.Now.Day - 0.3f) / 0.7f, 0.0f, 1.0f);
        }

        float SunColumn(const Look& look)
        {
            return std::round(static_cast<float>(look.Columns) * 0.2f);
        }

        void PaintGlow(const Canvas& paint, const Look& look)
        {
            if (look.Now.Low <= 0.0f || SunUp(look) <= 0.0f)
                return;
            const float x = SunColumn(look);
            const ImVec4 warm = Col(1.000f, 0.760f, 0.500f);
            constexpr float kSizes[] = { 0.36f, 0.23f, 0.12f };
            for (const float size : kSizes)
            {
                const float tall = std::max(2.0f, std::round(static_cast<float>(look.Ground) * size * 0.75f));
                const float wide = std::max(4.0f, std::round(static_cast<float>(look.Columns) * size * 1.3f));
                const ImVec4 color = Faded(warm, 0.09f * look.Now.Low);
                const int rows = static_cast<int>(tall);
                int startRow = 0;
                float current = -1.0f;
                for (int row = 0; row <= rows; ++row)
                {
                    const float share = static_cast<float>(row) / tall;
                    const float half = row < rows ? std::round(wide * std::sqrt(std::max(0.0f, 1.0f - share * share))) : -1.0f;
                    if (half == current)
                        continue;
                    if (current > 0.0f)
                        paint.Fill(x - current, static_cast<float>(look.Ground - row), x + current, static_cast<float>(look.Ground - startRow), color);
                    startRow = row;
                    current = half;
                }
            }
        }

        void PaintSun(const Canvas& paint, const Look& look)
        {
            const float up = SunUp(look);
            if (up <= 0.0f)
                return;
            const float radius = LightRadius(look);
            const float x = SunColumn(look);
            const float y = LightRow(look, radius, up);
            const float low = look.Now.Low;
            const ImVec4 glow = Mix(Col(1.000f, 0.980f, 0.900f), Col(1.000f, 0.760f, 0.500f), low);
            Round(paint, x, y, radius + 11.0f, Faded(glow, 0.05f));
            Round(paint, x, y, radius + 6.0f, Faded(glow, 0.08f));
            Round(paint, x, y, radius + 3.0f, Faded(glow, 0.12f));
            Round(paint, x, y, radius, Mix(Col(1.000f, 0.930f, 0.740f), Col(1.000f, 0.640f, 0.400f), low));
            Round(paint, x, y, radius - 1.0f, Mix(Col(1.000f, 0.970f, 0.860f), Col(1.000f, 0.790f, 0.560f), low));
            const float spot = std::round(radius * 0.3f);
            Round(paint, x - spot, y - spot, std::round(radius * 0.4f), Mix(Col(1.000f, 1.000f, 0.960f), Col(1.000f, 0.900f, 0.740f), low));
        }

        void PaintMoon(const Canvas& paint, const Look& look)
        {
            const float up = std::clamp((0.42f - look.Now.Day) / 0.42f, 0.0f, 1.0f);
            if (up <= 0.0f)
                return;
            const float radius = std::max(5.0f, LightRadius(look) - 1.0f);
            const float x = std::round(static_cast<float>(look.Columns) * 0.8f);
            const float y = LightRow(look, radius, up);
            const float bright = 0.35f + 0.65f * up;
            const ImVec4 sky = SkyRow(look, y);
            Round(paint, x, y, radius + 8.0f, ImVec4(0.700f, 0.780f, 0.950f, 0.035f * up));
            Round(paint, x, y, radius + 3.0f, ImVec4(0.700f, 0.780f, 0.950f, 0.07f * up));
            Round(paint, x, y, radius, Mix(sky, Col(0.830f, 0.860f, 0.920f), bright));
            const ImVec4 sea = Mix(sky, Col(0.660f, 0.700f, 0.790f), bright);
            const auto at = [&](float share) { return std::round(radius * share); };
            paint.Fill(x - at(0.6f), y - at(0.5f), x - at(0.6f) + std::max(2.0f, at(0.45f)), y - at(0.5f) + std::max(1.0f, at(0.35f)), sea);
            paint.Fill(x + at(0.1f), y + at(0.15f), x + at(0.1f) + std::max(2.0f, at(0.45f)), y + at(0.15f) + std::max(1.0f, at(0.3f)), sea);
            paint.Fill(x - at(0.35f), y + at(0.55f), x - at(0.35f) + std::max(1.0f, at(0.25f)), y + at(0.55f) + 1.0f, sea);
            paint.Fill(x + at(0.45f), y - at(0.6f), x + at(0.45f) + 1.0f, y - at(0.6f) + 1.0f, sea);
        }

        struct Streak
        {
            float Row;
            float Length;
            float Speed;
            float Start;
        };
        constexpr Streak kStreaks[] = {
            { 0.25f, 74.0f, 0.10f, 0.56f },
            { 0.33f, 36.0f, 0.15f, 0.08f },
            { 0.47f, 92.0f, 0.07f, 0.26f },
            { 0.41f, 28.0f, 0.12f, 0.90f },
        };

        void PaintStreaks(const Canvas& paint, const Look& look)
        {
            const ImVec4 lit = look.Now.Air(kCloudLit);
            const ImVec4 shade = look.Now.Air(kCloudShade);
            for (const Streak& streak : kStreaks)
            {
                const int length = std::max(10, static_cast<int>(std::round(streak.Length * std::max(0.5f, look.Size))));
                const int y = static_cast<int>(std::round(static_cast<float>(look.Ground) * streak.Row));
                const int x = Drift(look, static_cast<float>(look.Columns) * streak.Start, static_cast<float>(length), streak.Speed);
                const ImVec4 sky = SkyRow(look, static_cast<float>(y));
                Box(paint, x + length * 3 / 10, y - 1, x + length * 2 / 3, y, Mix(sky, lit, 0.26f));
                Box(paint, x, y, x + length, y + 1, Mix(sky, lit, 0.18f));
                Box(paint, x + length / 8, y + 1, x + length * 4 / 5, y + 2, Mix(sky, shade, 0.12f));
            }
        }

        struct Puff
        {
            float X;
            float Y;
            float Radius;
        };

        constexpr Puff kBank[] = { { -2.0f, -16.0f, 11.0f }, { -14.0f, -11.0f, 9.0f }, { 11.0f, -12.0f, 9.0f }, { -24.0f, -6.0f, 7.0f }, { 21.0f, -7.0f, 7.0f },
            { -31.0f, -3.0f, 5.0f }, { 30.0f, -3.0f, 5.0f }, { -8.0f, -5.0f, 7.0f }, { 6.0f, -5.0f, 8.0f } };
        constexpr Puff kCumulus[] = { { 1.0f, -21.0f, 11.0f }, { -13.0f, -15.0f, 9.0f }, { 14.0f, -15.0f, 9.5f }, { -25.0f, -9.0f, 7.0f }, { 26.0f, -8.0f, 7.0f },
            { -35.0f, -4.0f, 5.0f }, { 35.0f, -4.0f, 4.5f }, { -5.0f, -10.0f, 9.0f }, { 9.0f, -8.0f, 8.0f }, { -17.0f, -5.0f, 6.0f }, { 20.0f, -4.0f, 5.5f } };
        constexpr Puff kSmallCumulus[] = { { -3.0f, -11.0f, 7.5f }, { 8.0f, -10.0f, 6.5f }, { -13.0f, -6.0f, 5.5f }, { 17.0f, -5.0f, 4.5f },
            { 1.0f, -5.0f, 6.0f }, { -21.0f, -3.0f, 3.5f }, { 24.0f, -3.0f, 3.0f } };

        template <std::size_t Count>
        void PaintHeap(const Canvas& paint, const Look& look, const Puff (&puffs)[Count], int middle, int base, float scale, bool mirrored, const ImVec4* tones,
            std::uint32_t seed)
        {
            std::array<Puff, Count> placed{};
            float left = static_cast<float>(middle);
            float right = left;
            float top = static_cast<float>(base);
            for (std::size_t index = 0; index < Count; ++index)
            {
                const Puff& puff = puffs[index];
                const float radius = std::max(1.5f, puff.Radius * scale);
                placed[index] = { static_cast<float>(middle) + (mirrored ? -puff.X : puff.X) * scale, static_cast<float>(base) + puff.Y * scale, radius };
                left = std::min(left, placed[index].X - radius - 1.0f);
                right = std::max(right, placed[index].X + radius + 1.0f);
                top = std::min(top, placed[index].Y - radius - 1.0f);
            }
            const int from = std::max(0, static_cast<int>(std::floor(left)));
            const int to = std::min(look.Columns, static_cast<int>(std::ceil(right)));
            if (from >= to)
                return;
            const float belly = std::max(1.0f, std::round(2.0f * scale));
            PaintRuns(paint, from, to, std::max(0, static_cast<int>(std::floor(top))), base, tones, [&](int x, int y) -> int
                {
                    const float cellX = static_cast<float>(x) + 0.5f;
                    const float cellY = static_cast<float>(y) + 0.5f;
                    const std::uint32_t mixed = Noise(seed ^ (static_cast<std::uint32_t>(x - middle + 512) * 73856093u) ^ (static_cast<std::uint32_t>(y - base + 512) * 19349663u));
                    const float ragged = (mixed & 7u) == 0u ? 0.9f : 0.0f;
                    int owner = -1;
                    float dx = 0.0f;
                    float dy = 0.0f;
                    float distance = 0.0f;
                    for (std::size_t index = 0; index < Count; ++index)
                    {
                        const float offsetX = cellX - placed[index].X;
                        const float offsetY = cellY - placed[index].Y;
                        const float length = std::sqrt(offsetX * offsetX + offsetY * offsetY);
                        if (length <= placed[index].Radius - ragged)
                        {
                            owner = static_cast<int>(index);
                            dx = offsetX;
                            dy = offsetY;
                            distance = length;
                        }
                    }
                    if (owner < 0)
                        return -1;
                    const float up = static_cast<float>(base) - cellY;
                    if (up < 1.0f)
                        return 4;
                    const float radius = placed[static_cast<std::size_t>(owner)].Radius;
                    const float toward = (dx * look.LightX + dy * look.LightY) / std::max(0.5f, distance);
                    if (up < 1.0f + belly || dy > radius * 0.45f)
                        return 3;
                    if (toward > 0.5f && distance > radius - 1.6f)
                        return 0;
                    if (toward > 0.15f && distance > radius * 0.45f)
                        return 1;
                    if (toward < -0.45f && distance > radius * 0.6f)
                        return 3;
                    return 2;
                });
        }

        std::array<ImVec4, 5> CloudTones(const Look& look, float y, float haze)
        {
            const ImVec4 sky = SkyRow(look, y);
            return { Mix(look.Now.Air(kCloudRim), sky, haze), Mix(look.Now.Air(kCloudLit), sky, haze), Mix(look.Now.Air(kCloudBody), sky, haze),
                Mix(look.Now.Air(kCloudShade), sky, haze), Mix(look.Now.Air(kCloudDeep), sky, haze) };
        }

        void PaintBanks(const Canvas& paint, const Look& look)
        {
            const float scale = 0.85f * look.Size;
            const int base = look.Ground + 1;
            const auto tones = CloudTones(look, static_cast<float>(base) - 14.0f * scale, 0.5f);
            const float half = 36.0f * scale;
            PaintHeap(paint, look, kBank, Drift(look, static_cast<float>(look.Columns) * 0.44f, half, 0.05), base, scale, false, tones.data(), 3u);
            PaintHeap(paint, look, kBank, Drift(look, static_cast<float>(look.Columns) * 0.99f, half, 0.05), base, scale * 0.7f, true, tones.data(), 9u);
        }

        void PaintCumulus(const Canvas& paint, const Look& look)
        {
            const float ground = static_cast<float>(look.Ground);
            const float night = 1.0f - look.Now.Day;
            {
                const float scale = 0.8f * look.Size;
                const int base = static_cast<int>(std::round(ground * 0.37f));
                const auto tones = CloudTones(look, static_cast<float>(base) - 6.0f * scale, (look.Game ? 0.45f : 0.3f) + 0.3f * night);
                PaintHeap(paint, look, kSmallCumulus, Drift(look, static_cast<float>(look.Columns) * 0.52f, 26.0f * scale, 0.22), base, scale, false, tones.data(), 21u);
            }
            {
                const float scale = 0.85f * look.Size;
                const int base = static_cast<int>(std::round(ground * 0.63f));
                const auto tones = CloudTones(look, static_cast<float>(base) - 12.0f * scale, (look.Game ? 0.3f : 0.12f) + 0.3f * night);
                PaintHeap(paint, look, kCumulus, Drift(look, static_cast<float>(look.Columns) * 0.79f, 41.0f * scale, 0.4), base, scale, false, tones.data(), 27u);
            }
        }

        struct Peak
        {
            float X;
            float Height;
            float Half;
            std::uint32_t Seed;
        };
        constexpr Peak kMassif[] = { { 0.700f, 0.58f, 1.35f, 3u }, { 0.905f, 0.70f, 1.20f, 5u }, { 0.795f, 1.00f, 1.20f, 7u } };
        constexpr Peak kFarPeaks[] = { { 0.150f, 0.30f, 1.70f, 13u }, { 0.055f, 0.50f, 1.45f, 11u } };

        template <std::size_t Count>
        void PaintPeaks(const Canvas& paint, const Look& look, const Peak (&peaks)[Count], float haze)
        {
            const float ground = static_cast<float>(look.Ground);
            const float tallest = std::clamp(std::round(ground * 0.2f), 6.0f, 40.0f);
            float snowShare = 0.42f;
            if (look.Season == SceneSeason::Winter)
                snowShare = 0.7f;
            else if (look.Season == SceneSeason::Summer)
                snowShare = 0.3f;
            else if (look.Season == SceneSeason::Autumn)
                snowShare = 0.36f;

            struct Slope
            {
                int Top = 1 << 20;
                int Snow = 0;
                float Summit = 0.0f;
                float Peak = 0.0f;
                float RibNear = 0.0f;
                float RibFar = 0.0f;
                std::uint32_t Seed = 0;
            };
            int from = look.Columns;
            int to = 0;
            for (const Peak& peak : peaks)
            {
                const float half = peak.Half * peak.Height * tallest;
                from = std::min(from, static_cast<int>(std::floor(peak.X * static_cast<float>(look.Columns) - half)));
                to = std::max(to, static_cast<int>(std::ceil(peak.X * static_cast<float>(look.Columns) + half)) + 1);
            }
            from = std::max(0, from);
            to = std::min(look.Columns, to);
            if (from >= to)
                return;
            std::vector<Slope> slopes(static_cast<std::size_t>(to - from));
            int highest = look.Ground;
            for (const Peak& peak : peaks)
            {
                const float summit = std::round(peak.X * static_cast<float>(look.Columns)) + 0.5f;
                const float height = std::round(peak.Height * tallest);
                const float half = peak.Half * height;
                for (int x = std::max(from, static_cast<int>(summit - half)); x < std::min(to, static_cast<int>(summit + half) + 1); ++x)
                {
                    const float away = std::fabs(static_cast<float>(x) + 0.5f - summit) / half;
                    if (away >= 1.0f)
                        continue;
                    const std::uint32_t mixed = Noise(peak.Seed * 7919u + static_cast<std::uint32_t>(x / 2 + 4096));
                    const float ragged = away > 0.06f ? static_cast<float>(mixed % 3u) - 1.0f : 0.0f;
                    const int top = static_cast<int>(std::round(ground - height * std::pow(1.0f - away, 1.25f) + ragged));
                    Slope& slope = slopes[static_cast<std::size_t>(x - from)];
                    if (top >= slope.Top)
                        continue;
                    slope.Top = top;
                    slope.Summit = summit;
                    slope.Peak = ground - height;
                    slope.Seed = peak.Seed;
                    const std::uint32_t ribs = Noise(peak.Seed * 6151u);
                    slope.RibNear = peak.Half * (0.28f + 0.14f * static_cast<float>(ribs & 255u) / 255.0f);
                    slope.RibFar = height >= 14.0f ? peak.Half * (0.6f + 0.14f * static_cast<float>((ribs >> 8) & 255u) / 255.0f) : -1.0f;
                    const std::uint32_t line = Noise(peak.Seed * 104729u + static_cast<std::uint32_t>(x / 2 + 8192));
                    const float gully = (line >> 8) % 4u == 0u ? std::round(height * 0.18f) : 0.0f;
                    slope.Snow = static_cast<int>(std::round(ground - height + height * snowShare + static_cast<float>(line % 3u) + gully));
                    highest = std::min(highest, top);
                }
            }

            const ImVec4 air = SkyAt(look.Now, 1.0f);
            const ImVec4 rock = Mix(look.Now.Air(kRock), air, haze);
            const ImVec4 rockLit = Mix(look.Now.Air(kRockLit), air, haze);
            const ImVec4 snow = Mix(look.Now.Air(kSnow), air, haze);
            const ImVec4 snowLit = Mix(look.Now.Air(kSnowLit), air, haze);
            const std::array<ImVec4, 6> tones = { rock, rockLit, snow, snowLit, Mix(rock, rockLit, 0.45f), Mix(snow, snowLit, 0.45f) };
            PaintRuns(paint, from, to, highest, look.Ground, tones.data(), [&](int x, int y) -> int
                {
                    const Slope& slope = slopes[static_cast<std::size_t>(x - from)];
                    if (y < slope.Top)
                        return -1;
                    const std::uint32_t mixed = Noise(slope.Seed * 31u + static_cast<std::uint32_t>(y / 2 + 256));
                    const float ridge = slope.Summit + static_cast<float>(mixed % 3u) - 1.0f;
                    const bool left = static_cast<float>(x) + 0.5f < ridge;
                    const bool lit = left == (look.Side < 0.0f);
                    const bool snowy = y < slope.Snow;
                    if (!lit)
                    {
                        const float across = std::fabs(static_cast<float>(x) + 0.5f - slope.Summit);
                        const float down = static_cast<float>(y) + 0.5f - slope.Peak;
                        const bool rib = down > 3.0f && (std::fabs(across - down * slope.RibNear - 0.5f) < 1.0f || std::fabs(across - down * slope.RibFar - 0.5f) < 1.0f);
                        if (rib)
                            return snowy ? 5 : 4;
                    }
                    return (snowy ? 2 : 0) + (lit ? 1 : 0);
                });
        }

        void PaintCollars(const Canvas& paint, const Look& look)
        {
            struct Collar
            {
                float From;
                float To;
                float Height;
            };
            constexpr Collar kCollars[] = { { 0.660f, 0.775f, 0.40f }, { 0.850f, 0.980f, 0.36f }, { 0.745f, 0.885f, 0.46f }, { 0.005f, 0.110f, 0.26f },
                { 0.085f, 0.195f, 0.20f } };
            const float tallest = std::clamp(std::round(static_cast<float>(look.Ground) * 0.2f), 6.0f, 40.0f);
            for (const Collar& collar : kCollars)
            {
                const int y = look.Ground - static_cast<int>(std::round(tallest * collar.Height));
                const auto tones = CloudTones(look, static_cast<float>(y), 0.4f);
                const int left = static_cast<int>(std::round(static_cast<float>(look.Columns) * collar.From));
                const int right = static_cast<int>(std::round(static_cast<float>(look.Columns) * collar.To));
                const int width = right - left;
                Box(paint, left + width / 4, y - 1, left + width * 3 / 5, y, tones[1]);
                Box(paint, left, y, right, y + 1, tones[2]);
                Box(paint, left + width / 8, y + 1, right - width / 5, y + 2, tones[3]);
            }
        }

        void PaintMist(const Canvas& paint, const Look& look)
        {
            const int tall = static_cast<int>(std::round(4.0f + 6.0f * look.Size));
            const ImVec4 air = SkyAt(look.Now, 1.0f);
            for (int row = 0; row < tall; ++row)
            {
                const float share = static_cast<float>(row + 1) / static_cast<float>(tall);
                const int y = look.Ground - tall + row;
                Box(paint, 0, y, look.Columns, y + 1, Faded(air, 0.8f * std::pow(share, 1.6f)));
            }
            Box(paint, 0, look.Ground, look.Columns, look.Ground + 4, Faded(air, 0.85f));
        }

        struct Billow
        {
            int Top = 0;
            float Across = 1.0f;
        };

        void PaintSea(const Canvas& paint, const Look& look)
        {
            struct Row
            {
                float Radius;
                float Base;
            };
            constexpr int kMostRows = 9;
            std::array<Row, kMostRows> rows{};
            int count = 0;
            float top = static_cast<float>(look.Ground);
            const bool shallow = look.Rows - look.Ground < 26;
            float radius = shallow ? 2.8f : 2.4f * std::max(1.0f, look.Size);
            while (count < kMostRows && (count == 0 || top < static_cast<float>(look.Rows)))
            {
                rows[static_cast<std::size_t>(count)] = { radius, top + radius };
                top += radius * 1.05f;
                radius *= shallow ? 1.65f : 1.42f;
                ++count;
            }

            const int columns = look.Columns;
            std::vector<Billow> billows(static_cast<std::size_t>(count * columns));
            for (int index = 0; index < count; ++index)
            {
                const Row& row = rows[static_cast<std::size_t>(index)];
                const float near = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 1.0f;
                const float stretch = 1.0f + 1.2f * (1.0f - near);
                Billow* line = &billows[static_cast<std::size_t>(index * columns)];
                for (int x = 0; x < columns; ++x)
                    line[x].Top = static_cast<int>(std::round(row.Base));
                const float spacing = row.Radius * 2.3f * stretch;
                const double offset = look.Time * (0.12 + 0.18 * static_cast<double>(index));
                const auto first = static_cast<long long>(std::floor(-offset / spacing)) - 3;
                const auto last = static_cast<long long>(std::floor((static_cast<double>(columns) - offset) / spacing)) + 3;
                for (long long billow = first; billow <= last; ++billow)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(billow) * 2654435761u + static_cast<std::uint32_t>(index) * 40503u + 17u);
                    const auto share = [&](int shift) { return static_cast<float>((mixed >> shift) & 255u) / 255.0f; };
                    const float middle = static_cast<float>(static_cast<double>(billow) * spacing + offset) + (share(0) - 0.5f) * row.Radius * stretch;
                    const float half = row.Radius * (1.6f + 0.7f * share(8)) * stretch;
                    float height = row.Radius * (0.7f + 0.55f * share(16));
                    if (index == 0)
                        height = std::min(height, row.Radius + 1.0f);
                    const std::uint32_t heads = Noise(mixed + 0x9E37u);
                    const bool headed = row.Radius >= 3.0f;
                    const float headHalf = half * (0.3f + 0.15f * static_cast<float>(heads & 255u) / 255.0f);
                    const float headX = middle + half * (static_cast<float>((heads >> 8) & 255u) / 255.0f - 0.5f) * 0.8f;
                    const float headHeight = headed ? height * (0.22f + 0.2f * static_cast<float>((heads >> 16) & 255u) / 255.0f) : 0.0f;
                    const int left = std::max(0, static_cast<int>(std::floor(middle - half)));
                    const int right = std::min(columns - 1, static_cast<int>(std::ceil(middle + half)));
                    for (int x = left; x <= right; ++x)
                    {
                        const float across = (static_cast<float>(x) + 0.5f - middle) / half;
                        if (std::fabs(across) >= 1.0f)
                            continue;
                        float rise = height * std::sqrt(1.0f - across * across);
                        const float onHead = (static_cast<float>(x) + 0.5f - headX) / headHalf;
                        if (std::fabs(onHead) < 1.0f)
                            rise += headHeight * std::sqrt(1.0f - onHead * onHead);
                        const int at = static_cast<int>(std::round(row.Base - rise));
                        if (at < line[x].Top)
                        {
                            line[x].Top = at;
                            line[x].Across = across;
                        }
                    }
                }
            }

            constexpr int kTones = 5;
            std::array<ImVec4, kMostRows * kTones> colors{};
            const ImVec4 air = SkyAt(look.Now, 1.0f);
            const AirColor* kinds[kTones] = { &kCloudRim, &kCloudLit, &kCloudBody, &kCloudShade, &kCloudDeep };
            for (int index = 0; index < count; ++index)
            {
                const float near = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 1.0f;
                const float haze = 0.5f * std::pow(1.0f - near, 1.3f);
                for (int tone = 0; tone < kTones; ++tone)
                    colors[static_cast<std::size_t>(index * kTones + tone)] = Mix(look.Now.Air(*kinds[tone]), air, haze);
            }

            PaintRuns(paint, 0, columns, look.Ground - 3, look.Rows, colors.data(), [&](int x, int y) -> int
                {
                    for (int index = count - 1; index >= 0; --index)
                    {
                        const Billow& here = billows[static_cast<std::size_t>(index * columns + x)];
                        if (y < here.Top)
                            continue;
                        const Row& row = rows[static_cast<std::size_t>(index)];
                        const int depth = y - here.Top;
                        const bool round = index >= count - 2 && index >= 2;
                        const float facing = here.Across * -look.Side;
                        const int gap = index + 1 < count ? billows[static_cast<std::size_t>((index + 1) * columns + x)].Top - y : 1 << 20;
                        int tone = 2;
                        if (gap <= 1 && index >= 1)
                            tone = index >= 2 ? 4 : 3;
                        else if (depth == 0)
                            tone = facing > -0.45f ? 0 : 1;
                        else if (facing > 0.25f && index >= 2 && (depth == 1 || (round && static_cast<float>(depth) <= row.Radius * 0.3f)))
                            tone = 1;
                        else if (static_cast<float>(y) >= row.Base - row.Radius * 0.05f)
                            tone = 3;
                        else if (round && facing < -0.6f && static_cast<float>(depth) <= row.Radius * 0.5f)
                            tone = 3;
                        return index * kTones + tone;
                    }
                    return -1;
                });
        }

        void PaintSheen(const Canvas& paint, const Look& look)
        {
            const float sun = SunUp(look);
            const bool moon = sun <= 0.0f;
            const float x = moon ? std::round(static_cast<float>(look.Columns) * 0.8f) : SunColumn(look);
            const ImVec4 color = moon ? Col(0.700f, 0.780f, 0.950f) : Mix(Col(1.000f, 0.980f, 0.920f), Col(1.000f, 0.780f, 0.480f), look.Now.Low);
            const float strength = moon ? 0.10f * std::clamp((0.42f - look.Now.Day) / 0.42f, 0.0f, 1.0f) : 0.07f + 0.17f * look.Now.Low;
            if (strength <= 0.0f)
                return;
            const int deep = static_cast<int>(std::round(static_cast<float>(look.Rows - look.Ground) * 0.7f));
            constexpr int kSteps = 6;
            for (int step = 0; step < kSteps; ++step)
            {
                const int from = look.Ground + deep * step / kSteps;
                const int to = look.Ground + deep * (step + 1) / kSteps;
                const float share = static_cast<float>(step) / static_cast<float>(kSteps);
                const float half = std::round(static_cast<float>(look.Columns) * (0.07f + 0.1f * share));
                paint.Fill(x - half, static_cast<float>(from), x + half, static_cast<float>(to), Faded(color, strength * (1.0f - share) * (1.0f - share)));
                paint.Fill(x - half * 2.0f, static_cast<float>(from), x + half * 2.0f, static_cast<float>(to), Faded(color, 0.5f * strength * (1.0f - share) * (1.0f - share)));
            }
        }
    }

    void DrawCloudsBack(const Frame& frame)
    {
        if (frame.Columns <= 0 || frame.Rows <= 0)
            return;
        const Canvas& canvas = frame.Paint;
        const Look look = LookOf(frame);
        PaintSky(canvas, look);
        PaintStars(canvas, look);
        PaintGlow(canvas, look);
        PaintSun(canvas, look);
        PaintMoon(canvas, look);
        PaintStreaks(canvas, look);
        PaintBanks(canvas, look);
        PaintPeaks(canvas, look, kFarPeaks, 0.55f);
        PaintPeaks(canvas, look, kMassif, 0.32f);
        PaintCollars(canvas, look);
        PaintMist(canvas, look);
        PaintCumulus(canvas, look);
        PaintSea(canvas, look);
        PaintSheen(canvas, look);
    }

    void DrawCloudsFront(const Frame& frame)
    {
        const Look look = LookOf(frame);
        const int sea = look.Rows - look.Ground;
        if (sea < 6 || look.Columns <= 0)
            return;
        const Canvas& canvas = frame.Paint;
        const ImVec4 mist = look.Now.Air(kCloudLit);
        struct Wisp
        {
            float Row;
            float Length;
            float Speed;
            float Start;
            float Alpha;
        };
        constexpr Wisp kWisps[] = {
            { 0.55f, 0.40f, 0.9f, 0.10f, 0.20f },
            { 0.76f, 0.55f, 1.4f, 0.58f, 0.26f },
            { 0.92f, 0.32f, 1.9f, 0.86f, 0.30f },
        };
        for (const Wisp& wisp : kWisps)
        {
            const int length = static_cast<int>(static_cast<float>(look.Columns) * wisp.Length);
            const int y = look.Ground + static_cast<int>(std::round(static_cast<float>(sea) * wisp.Row));
            const int x = Drift(look, static_cast<float>(look.Columns) * wisp.Start, static_cast<float>(length), wisp.Speed);
            Box(canvas, x, y, x + length, y + 1, Faded(mist, wisp.Alpha));
            Box(canvas, x + length / 6, y - 1, x + length * 3 / 5, y, Faded(mist, wisp.Alpha * 0.6f));
        }
        if (!frame.Game)
        {
            const int length = std::max(24, look.Columns * 3 / 10);
            const int x = Drift(look, static_cast<float>(look.Columns) * 0.62f, static_cast<float>(length + look.Columns), 1.3);
            const int y = look.Ground - 5;
            Box(canvas, x, y, x + length, y + 1, Faded(mist, 0.3f));
            Box(canvas, x + length / 5, y - 1, x + length * 3 / 4, y, Faded(mist, 0.2f));
            Box(canvas, x + length / 3, y + 1, x + length * 9 / 10, y + 2, Faded(look.Now.Air(kCloudShade), 0.18f));
        }
    }
}
