#include "UI/StageScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Idler::UI::StageScene
{
    using namespace HomeScene;

    namespace
    {
        constexpr ImVec4 Colour(float red, float green, float blue, float alpha = 1.0f)
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

        ImVec4 Faded(ImVec4 color, float alpha)
        {
            color.w *= alpha;
            return color;
        }

        void Box(const Canvas& paint, float left, float top, float right, float bottom, ImVec4 color)
        {
            if (right <= left || bottom <= top || color.w < 0.01f)
                return;
            paint.Fill(left, top, right, bottom, color);
        }

        void Disc(const Canvas& paint, float x, float y, float radius, ImVec4 color)
        {
            const float reach = (radius + 0.4f) * (radius + 0.4f);
            for (float row = -radius; row <= radius; row += 1.0f)
            {
                const float half = std::floor(std::sqrt(std::max(0.0f, reach - row * row)));
                Box(paint, x - half, y + row, x + half + 1.0f, y + row + 1.0f, color);
            }
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
                        draw(F(x), F(y), mixed);
                }
            }
        }

        struct Column
        {
            float Top = 0.0f;
            float Bottom = 0.0f;
            int Tone = -1;
            bool operator==(const Column&) const = default;
        };

        template <typename Of>
        void ColumnRuns(const Canvas& paint, int columns, const ImVec4* colors, Of&& of)
        {
            int start = 0;
            Column run = of(0);
            for (int column = 1; column <= columns; ++column)
            {
                const Column here = column < columns ? of(column) : Column{};
                if (column < columns && here == run)
                    continue;
                if (run.Tone >= 0)
                    Box(paint, F(start), run.Top, F(column), run.Bottom, colors[run.Tone]);
                start = column;
                run = here;
            }
        }

        struct Palette
        {
            ImVec4 SkyTop, SkyMiddle, SkyLow, NightTop, NightLow, DuskTop, DuskLow, HazeDay, HazeNight;
            ImVec4 CloudLit, CloudBody, CloudShade;
            ImVec4 RangeLit, RangeShade, CapLit, CapShade;
            ImVec4 FarHill, FarHillShade, FarWood;
            ImVec4 Hill, HillShade, Hedge;
            ImVec4 Fields[4][2];
            ImVec4 Crowns[3];
            ImVec4 Accents[3];
            ImVec4 Trunk;
            ImVec4 MeadowBack, MeadowFront, MeadowRim, PatchDark, PatchLight;
            ImVec4 BladeDark, BladeMid, BladeLight, Seed;
            ImVec4 Petals[4];
            ImVec4 Hearts[4];
            float Moonlight;
        };

        constexpr int kSpring = 0;
        constexpr int kSummer = 1;
        constexpr int kAutumn = 2;
        constexpr int kWinter = 3;

        constexpr Palette kPalettes[] = {
            {
                Colour(0.280f, 0.510f, 0.790f), Colour(0.440f, 0.650f, 0.860f), Colour(0.640f, 0.810f, 0.920f), Colour(0.028f, 0.040f, 0.090f),
                Colour(0.070f, 0.105f, 0.180f), Colour(0.250f, 0.270f, 0.480f), Colour(0.990f, 0.640f, 0.480f), Colour(0.570f, 0.710f, 0.820f),
                Colour(0.075f, 0.105f, 0.175f),
                Colour(1.000f, 1.000f, 1.000f), Colour(0.920f, 0.950f, 0.990f), Colour(0.720f, 0.790f, 0.900f),
                Colour(0.500f, 0.550f, 0.750f), Colour(0.390f, 0.430f, 0.640f), Colour(0.950f, 0.970f, 1.000f), Colour(0.730f, 0.780f, 0.910f),
                Colour(0.400f, 0.580f, 0.590f), Colour(0.340f, 0.510f, 0.550f), Colour(0.250f, 0.410f, 0.440f),
                Colour(0.470f, 0.670f, 0.400f), Colour(0.400f, 0.590f, 0.380f), Colour(0.240f, 0.430f, 0.250f),
                { { Colour(0.580f, 0.770f, 0.390f), Colour(0.490f, 0.690f, 0.350f) }, { Colour(0.960f, 0.900f, 0.500f), Colour(0.880f, 0.800f, 0.340f) },
                    { Colour(0.380f, 0.590f, 0.320f), Colour(0.340f, 0.540f, 0.300f) }, { Colour(0.580f, 0.770f, 0.390f), Colour(0.970f, 0.800f, 0.860f) } },
                { Colour(0.250f, 0.450f, 0.250f), Colour(0.400f, 0.630f, 0.300f), Colour(0.560f, 0.770f, 0.370f) },
                { Colour(0.780f, 0.520f, 0.650f), Colour(0.940f, 0.700f, 0.800f), Colour(1.000f, 0.860f, 0.910f) },
                Colour(0.300f, 0.230f, 0.180f),
                Colour(0.440f, 0.650f, 0.340f), Colour(0.310f, 0.570f, 0.240f), Colour(0.570f, 0.780f, 0.390f), Colour(0.270f, 0.520f, 0.230f),
                Colour(0.430f, 0.680f, 0.290f),
                Colour(0.200f, 0.420f, 0.180f), Colour(0.330f, 0.600f, 0.240f), Colour(0.540f, 0.780f, 0.320f), Colour(0.930f, 0.940f, 0.920f),
                { Colour(0.970f, 0.970f, 0.940f), Colour(0.990f, 0.840f, 0.180f), Colour(0.950f, 0.560f, 0.720f), Colour(0.480f, 0.640f, 0.960f) },
                { Colour(0.980f, 0.820f, 0.220f), Colour(1.000f, 0.950f, 0.550f), Colour(1.000f, 0.800f, 0.880f), Colour(0.980f, 0.900f, 0.400f) },
                1.0f,
            },
            {
                Colour(0.160f, 0.340f, 0.600f), Colour(0.270f, 0.480f, 0.710f), Colour(0.520f, 0.690f, 0.800f), Colour(0.022f, 0.038f, 0.095f),
                Colour(0.090f, 0.125f, 0.210f), Colour(0.240f, 0.240f, 0.450f), Colour(0.930f, 0.620f, 0.440f), Colour(0.480f, 0.640f, 0.755f),
                Colour(0.080f, 0.110f, 0.190f),
                Colour(0.960f, 0.965f, 0.975f), Colour(0.850f, 0.885f, 0.935f), Colour(0.660f, 0.740f, 0.840f),
                Colour(0.480f, 0.620f, 0.730f), Colour(0.400f, 0.540f, 0.670f), Colour(0.520f, 0.660f, 0.760f), Colour(0.440f, 0.580f, 0.700f),
                Colour(0.400f, 0.560f, 0.500f), Colour(0.340f, 0.500f, 0.470f), Colour(0.180f, 0.360f, 0.300f),
                Colour(0.420f, 0.600f, 0.340f), Colour(0.340f, 0.520f, 0.300f), Colour(0.150f, 0.330f, 0.180f),
                { { Colour(0.500f, 0.660f, 0.380f), Colour(0.440f, 0.610f, 0.340f) }, { Colour(0.800f, 0.720f, 0.400f), Colour(0.740f, 0.660f, 0.360f) },
                    { Colour(0.360f, 0.540f, 0.320f), Colour(0.320f, 0.500f, 0.290f) }, { Colour(0.600f, 0.700f, 0.440f), Colour(0.540f, 0.650f, 0.400f) } },
                { Colour(0.090f, 0.230f, 0.150f), Colour(0.150f, 0.330f, 0.200f), Colour(0.270f, 0.470f, 0.240f) },
                { Colour(0.070f, 0.200f, 0.140f), Colour(0.130f, 0.300f, 0.190f), Colour(0.220f, 0.420f, 0.230f) },
                Colour(0.280f, 0.210f, 0.160f),
                Colour(0.330f, 0.550f, 0.280f), Colour(0.190f, 0.420f, 0.170f), Colour(0.470f, 0.660f, 0.340f), Colour(0.160f, 0.370f, 0.150f),
                Colour(0.330f, 0.560f, 0.250f),
                Colour(0.120f, 0.300f, 0.130f), Colour(0.220f, 0.450f, 0.190f), Colour(0.380f, 0.610f, 0.270f), Colour(0.760f, 0.700f, 0.420f),
                { Colour(0.880f, 0.200f, 0.140f), Colour(0.330f, 0.480f, 0.920f), Colour(0.960f, 0.960f, 0.900f), Colour(0.980f, 0.840f, 0.240f) },
                { Colour(0.500f, 0.070f, 0.080f), Colour(0.180f, 0.260f, 0.600f), Colour(0.980f, 0.840f, 0.240f), Colour(0.990f, 0.930f, 0.520f) },
                1.0f,
            },
            {
                Colour(0.300f, 0.450f, 0.630f), Colour(0.520f, 0.620f, 0.710f), Colour(0.830f, 0.780f, 0.660f), Colour(0.030f, 0.040f, 0.080f),
                Colour(0.110f, 0.140f, 0.215f), Colour(0.200f, 0.220f, 0.400f), Colour(0.960f, 0.580f, 0.400f), Colour(0.800f, 0.730f, 0.620f),
                Colour(0.105f, 0.125f, 0.185f),
                Colour(0.990f, 0.960f, 0.910f), Colour(0.880f, 0.860f, 0.840f), Colour(0.680f, 0.650f, 0.700f),
                Colour(0.520f, 0.500f, 0.620f), Colour(0.450f, 0.430f, 0.560f), Colour(0.600f, 0.580f, 0.680f), Colour(0.520f, 0.500f, 0.620f),
                Colour(0.600f, 0.470f, 0.420f), Colour(0.530f, 0.410f, 0.380f), Colour(0.420f, 0.300f, 0.260f),
                Colour(0.560f, 0.470f, 0.300f), Colour(0.480f, 0.390f, 0.250f), Colour(0.300f, 0.220f, 0.130f),
                { { Colour(0.470f, 0.320f, 0.200f), Colour(0.370f, 0.240f, 0.150f) }, { Colour(0.780f, 0.680f, 0.440f), Colour(0.690f, 0.580f, 0.360f) },
                    { Colour(0.460f, 0.540f, 0.290f), Colour(0.400f, 0.480f, 0.250f) }, { Colour(0.600f, 0.580f, 0.330f), Colour(0.540f, 0.520f, 0.290f) } },
                { Colour(0.480f, 0.260f, 0.150f), Colour(0.640f, 0.300f, 0.150f), Colour(0.760f, 0.450f, 0.180f) },
                { Colour(0.740f, 0.580f, 0.230f), Colour(0.440f, 0.420f, 0.200f), Colour(0.160f, 0.250f, 0.210f) },
                Colour(0.220f, 0.145f, 0.100f),
                Colour(0.640f, 0.600f, 0.370f), Colour(0.430f, 0.430f, 0.210f), Colour(0.780f, 0.690f, 0.400f), Colour(0.360f, 0.390f, 0.180f),
                Colour(0.700f, 0.610f, 0.350f),
                Colour(0.320f, 0.300f, 0.140f), Colour(0.520f, 0.480f, 0.250f), Colour(0.730f, 0.650f, 0.380f), Colour(0.820f, 0.720f, 0.480f),
                { Colour(0.600f, 0.460f, 0.820f), Colour(0.950f, 0.930f, 0.890f), Colour(0.880f, 0.460f, 0.640f), Colour(0.700f, 0.400f, 0.720f) },
                { Colour(0.980f, 0.820f, 0.300f), Colour(0.980f, 0.800f, 0.260f), Colour(0.700f, 0.260f, 0.420f), Colour(0.520f, 0.240f, 0.520f) },
                1.0f,
            },
            {
                Colour(0.290f, 0.450f, 0.650f), Colour(0.520f, 0.650f, 0.790f), Colour(0.760f, 0.835f, 0.905f), Colour(0.030f, 0.040f, 0.090f),
                Colour(0.100f, 0.135f, 0.230f), Colour(0.300f, 0.300f, 0.500f), Colour(0.900f, 0.640f, 0.560f), Colour(0.680f, 0.760f, 0.860f),
                Colour(0.105f, 0.140f, 0.240f),
                Colour(0.960f, 0.970f, 0.990f), Colour(0.860f, 0.890f, 0.940f), Colour(0.700f, 0.750f, 0.850f),
                Colour(0.430f, 0.510f, 0.650f), Colour(0.340f, 0.420f, 0.570f), Colour(0.930f, 0.955f, 0.985f), Colour(0.660f, 0.740f, 0.870f),
                Colour(0.860f, 0.900f, 0.950f), Colour(0.700f, 0.770f, 0.880f), Colour(0.340f, 0.450f, 0.520f),
                Colour(0.890f, 0.920f, 0.965f), Colour(0.750f, 0.810f, 0.900f), Colour(0.300f, 0.300f, 0.330f),
                { { Colour(0.900f, 0.930f, 0.970f), Colour(0.800f, 0.840f, 0.910f) }, { Colour(0.860f, 0.900f, 0.950f), Colour(0.760f, 0.800f, 0.870f) },
                    { Colour(0.900f, 0.930f, 0.970f), Colour(0.700f, 0.720f, 0.760f) }, { Colour(0.860f, 0.900f, 0.950f), Colour(0.800f, 0.840f, 0.910f) } },
                { Colour(0.065f, 0.175f, 0.190f), Colour(0.120f, 0.290f, 0.270f), Colour(0.210f, 0.420f, 0.350f) },
                { Colour(0.700f, 0.770f, 0.880f), Colour(0.880f, 0.920f, 0.970f), Colour(0.960f, 0.975f, 0.995f) },
                Colour(0.330f, 0.270f, 0.250f),
                Colour(0.790f, 0.840f, 0.915f), Colour(0.715f, 0.775f, 0.870f), Colour(0.940f, 0.960f, 0.990f), Colour(0.580f, 0.650f, 0.800f),
                Colour(0.450f, 0.530f, 0.715f),
                Colour(0.520f, 0.420f, 0.260f), Colour(0.740f, 0.630f, 0.400f), Colour(0.940f, 0.960f, 0.990f), Colour(1.000f, 1.000f, 1.000f),
                { Colour(0.960f, 0.970f, 0.990f), Colour(0.780f, 0.150f, 0.170f), Colour(0.960f, 0.970f, 0.990f), Colour(0.780f, 0.150f, 0.170f) },
                { Colour(0.500f, 0.700f, 0.400f), Colour(0.500f, 0.100f, 0.120f), Colour(0.500f, 0.700f, 0.400f), Colour(0.500f, 0.100f, 0.120f) },
                1.3f,
            },
        };

        int IndexOf(SceneSeason season)
        {
            switch (season)
            {
            case SceneSeason::Spring:
                return kSpring;
            case SceneSeason::Autumn:
                return kAutumn;
            case SceneSeason::Winter:
                return kWinter;
            default:
                return kSummer;
            }
        }

        struct Look
        {
            float Daylight = 1.0f;
            float Glow = 0.0f;
            float Moonlight = 1.0f;
            ImVec4 Haze;

            ImVec4 operator()(ImVec4 day) const
            {
                const ImVec4 night(day.x * 0.22f * Moonlight + 0.010f, day.y * 0.27f * Moonlight + 0.020f, day.z * 0.38f * Moonlight + 0.050f, day.w);
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

        struct Setting
        {
            Canvas Paint;
            int Columns = 0;
            int Rows = 0;
            float Width = 0.0f;
            float Horizon = 0.0f;
            float Ground = 0.0f;
            float Size = 1.0f;
            int Season = kSummer;
            bool Game = false;
            double Time = 0.0;
            const Palette* Colors = nullptr;
            Look Light;
            float SunX = 0.0f;
            float SunY = 0.0f;
            float MoonX = 0.0f;
            float MoonY = 0.0f;
            float SunRadius = 6.0f;
            float Elevation = 1.0f;
        };

        Setting SetUp(const Frame& frame)
        {
            Setting s;
            s.Paint = frame.Paint;
            s.Columns = frame.Columns;
            s.Rows = frame.Rows;
            s.Width = F(frame.Columns);
            s.Ground = frame.Ground;
            s.Size = std::max(0.3f, F(frame.Rows) / 100.0f);
            s.Horizon = std::min(std::round(F(frame.Rows) * 0.56f), std::max(4.0f, frame.Ground - 8.0f));
            s.Season = IndexOf(frame.Season);
            s.Colors = &kPalettes[s.Season];
            s.Game = frame.Game;
            s.Time = frame.Time;
            const Palette& colors = *s.Colors;
            const float daylight = std::clamp(frame.Daylight, 0.0f, 1.0f);
            const float glow = std::clamp(1.0f - std::fabs(daylight - 0.5f) * 2.6f, 0.0f, 1.0f);
            s.Light = { daylight, glow, colors.Moonlight, Mix(DayNight(colors.HazeDay, colors.HazeNight, daylight), colors.DuskLow, glow * 0.22f) };

            const float high = std::round(std::max(6.0f, s.Horizon * 0.22f));
            const float low = s.Horizon + 6.0f + 4.0f * s.Size;
            const float sunUp = std::sqrt(std::clamp((daylight - 0.3f) / 0.7f, 0.0f, 1.0f));
            const float moonUp = std::sqrt(std::clamp((0.7f - daylight) / 0.7f, 0.0f, 1.0f));
            s.SunX = std::round(s.Width * 0.8f);
            s.SunY = std::round(low + (high - low) * sunUp);
            s.MoonX = std::round(s.Width * 0.71f);
            s.MoonY = std::round(low + (high - low) * moonUp);
            s.SunRadius = std::clamp(std::round(3.0f + 2.6f * s.Size), 4.0f, 8.0f);
            s.Elevation = std::max(sunUp, moonUp);
            return s;
        }

        float SideOfLight(const Setting& s, float x)
        {
            const float lightX = s.Light.Daylight >= 0.5f ? s.SunX : s.MoonX;
            return lightX < x ? -1.0f : 1.0f;
        }

        bool FacesLight(const Setting& s, float slope, float x)
        {
            if (std::fabs(slope) < 0.05f)
                return false;
            return (slope < 0.0f ? -1.0f : 1.0f) == SideOfLight(s, x);
        }

        struct Toward
        {
            float X = 0.0f;
            float Y = -1.0f;
        };

        Toward TowardLight(const Setting& s, float x)
        {
            const float across = SideOfLight(s, x) * (0.5f + 0.45f * (1.0f - s.Elevation));
            const float up = -(0.3f + 0.6f * s.Elevation);
            const float length = std::sqrt(across * across + up * up);
            return { across / length, up / length };
        }

        float DepthOf(const Setting& s, float y)
        {
            return (y - s.Horizon) / std::max(1.0f, s.Ground - s.Horizon);
        }

        float HazeAt(const Setting& s, float y)
        {
            return 0.34f * std::max(0.0f, 1.0f - std::max(0.0f, DepthOf(s, y)) / 0.7f);
        }

        ImVec4 OnMeadow(const Setting& s, ImVec4 day, float y)
        {
            return Mix(s.Light(day), s.Light.Haze, HazeAt(s, y));
        }

        ImVec4 GroundAt(const Setting& s, float y)
        {
            const Palette& colors = *s.Colors;
            const float depth = std::clamp(DepthOf(s, y), 0.0f, 1.4f);
            ImVec4 day = Mix(colors.MeadowBack, colors.MeadowFront, std::pow(std::min(1.0f, depth), 0.85f));
            if (depth > 1.0f)
                day = Mix(day, ImVec4(day.x * 0.86f, day.y * 0.9f, day.z * 0.9f, 1.0f), (depth - 1.0f) * 2.0f);
            return OnMeadow(s, day, y);
        }

        ImVec4 SkyAt(const Setting& s, float down)
        {
            const Palette& colors = *s.Colors;
            down = std::clamp(down, 0.0f, 1.0f);
            const ImVec4 day = down < 0.5f ? Mix(colors.SkyTop, colors.SkyMiddle, down * 2.0f) : Mix(colors.SkyMiddle, colors.SkyLow, (down - 0.5f) * 2.0f);
            const ImVec4 night = Mix(colors.NightTop, colors.NightLow, down);
            return Mix(DayNight(day, night, s.Light.Daylight), Mix(colors.DuskTop, colors.DuskLow, down), s.Light.Glow * (0.12f + 0.55f * down * down));
        }

        class Sprite
        {
        public:
            static constexpr int kWidth = 150;
            static constexpr int kHeight = 110;
            static constexpr int kFootX = kWidth / 2;
            static constexpr int kFootY = kHeight - 8;

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

            void Snow(int lit, int shade, float toward, std::uint32_t seed)
            {
                for (int row = m_bottom; row > m_top; --row)
                {
                    for (int column = m_left; column <= m_right; ++column)
                    {
                        std::uint8_t& cell = m_cells[static_cast<std::size_t>(row * kWidth + column)];
                        const std::uint8_t above = m_cells[static_cast<std::size_t>((row - 1) * kWidth + column)];
                        if (cell == 0 || cell == lit || cell == shade || above != 0 || Hash(column * 131 + row, seed) % 5u == 0u)
                            continue;
                        cell = static_cast<std::uint8_t>(F(column - kFootX) * toward >= 0.0f ? lit : shade);
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
                            Box(paint, x + F(column - kFootX), top, x + F(end - kFootX), top + 1.0f, colors[tone]);
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

        void Clump(Sprite& sprite, const Ellipse& clump, const Ellipse& whole, Toward light, int ramp, int bias, std::uint32_t seed, int lowest = 8, float grain = 0.3f)
        {
            const auto top = static_cast<int>(std::floor(clump.Y - clump.Ry));
            const int bottom = std::min(lowest, static_cast<int>(std::ceil(clump.Y + clump.Ry)));
            const auto left = static_cast<int>(std::floor(clump.X - clump.Rx));
            const auto right = static_cast<int>(std::ceil(clump.X + clump.Rx));
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
                    const float rough = Roll(((x + 255) / (clump.Rx >= 6.0f ? 3 : 2)) * 131 + y, seed) - 0.5f;
                    const float facing = (nx * light.X + ny * light.Y) * 0.55f + (wx * light.X + wy * light.Y) * 0.55f + rough * grain;
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

        constexpr int kFresh = 1;
        constexpr int kPink = 5;
        constexpr int kDeep = 9;
        constexpr int kCrimson = 13;
        constexpr int kNeedle = 17;
        constexpr int kSnowy = 21;
        constexpr int kBark = 25;
        constexpr int kBirch = 28;
        constexpr int kWhite = 32;
        constexpr int kRose = 33;
        constexpr int kBerry = 34;
        constexpr int kCatkin = 35;
        constexpr int kLilac = 36;
        constexpr int kRobin = 39;
        constexpr int kCap = 41;
        constexpr int kGold = 44;
        constexpr int kOlive = 46;

        constexpr ImVec4 kTones[] = {
            Colour(0.0f, 0.0f, 0.0f, 0.0f),
            Colour(0.180f, 0.360f, 0.190f), Colour(0.310f, 0.540f, 0.240f), Colour(0.480f, 0.720f, 0.290f), Colour(0.680f, 0.860f, 0.400f),
            Colour(0.660f, 0.360f, 0.500f), Colour(0.870f, 0.540f, 0.670f), Colour(0.970f, 0.720f, 0.810f), Colour(1.000f, 0.890f, 0.920f),
            Colour(0.070f, 0.200f, 0.120f), Colour(0.130f, 0.320f, 0.160f), Colour(0.220f, 0.450f, 0.200f), Colour(0.360f, 0.580f, 0.260f),
            Colour(0.330f, 0.050f, 0.070f), Colour(0.530f, 0.090f, 0.090f), Colour(0.720f, 0.170f, 0.120f), Colour(0.880f, 0.300f, 0.160f),
            Colour(0.050f, 0.150f, 0.150f), Colour(0.100f, 0.250f, 0.230f), Colour(0.180f, 0.370f, 0.310f), Colour(0.280f, 0.480f, 0.380f),
            Colour(0.450f, 0.530f, 0.715f), Colour(0.580f, 0.650f, 0.800f), Colour(0.800f, 0.850f, 0.930f), Colour(0.940f, 0.960f, 0.990f),
            Colour(0.200f, 0.130f, 0.100f), Colour(0.350f, 0.240f, 0.170f), Colour(0.510f, 0.370f, 0.260f),
            Colour(0.130f, 0.120f, 0.130f), Colour(0.600f, 0.600f, 0.600f), Colour(0.820f, 0.810f, 0.780f), Colour(0.970f, 0.960f, 0.920f),
            Colour(0.990f, 0.970f, 0.960f),
            Colour(0.950f, 0.550f, 0.700f),
            Colour(0.780f, 0.150f, 0.170f),
            Colour(0.780f, 0.660f, 0.300f),
            Colour(0.400f, 0.280f, 0.540f), Colour(0.580f, 0.440f, 0.740f), Colour(0.740f, 0.600f, 0.880f),
            Colour(0.420f, 0.320f, 0.240f), Colour(0.900f, 0.420f, 0.220f),
            Colour(0.820f, 0.160f, 0.100f), Colour(0.980f, 0.960f, 0.900f), Colour(0.880f, 0.850f, 0.780f),
            Colour(0.920f, 0.740f, 0.150f), Colour(0.990f, 0.870f, 0.290f),
            Colour(0.200f, 0.230f, 0.100f), Colour(0.330f, 0.360f, 0.140f), Colour(0.480f, 0.480f, 0.200f), Colour(0.620f, 0.580f, 0.280f),
        };

        void RoundTree(Sprite& sprite, float k, Toward light, int leaf, std::uint32_t seed, float wide)
        {
            const int trunkTop = -std::max(4, static_cast<int>(std::round(14.0f * k)));
            Trunk(sprite, trunkTop, std::max(2.0f, 5.4f * k), std::max(1.5f, 3.6f * k), (Roll(0, seed) - 0.5f) * 3.0f * k, light, kBark, seed);
            const Ellipse crown{ 0.0f, F(trunkTop) - 11.0f * k, 19.0f * k * wide, 13.0f * k };
            for (int index = 0; index < 7; ++index)
            {
                const float angle = (F(index) + Roll(index, seed + 1u) * 0.7f) * (6.2832f / 7.0f);
                const float radius = (6.5f + Roll(index, seed + 2u) * 2.0f) * k;
                Clump(sprite, { crown.X + std::cos(angle) * crown.Rx * 0.5f, crown.Y - k + std::sin(angle) * crown.Ry * 0.4f, radius * 1.1f, radius * 0.85f }, crown, light,
                    leaf, -1, seed);
            }
            const float bough = std::max(1.0f, 2.4f * k);
            Branch(sprite, -0.5f, F(trunkTop) + 1.0f, -crown.Rx * 0.55f, crown.Y + 2.0f * k, bough, 1.0f, light, kBark);
            Branch(sprite, 0.5f, F(trunkTop) + 1.0f, crown.Rx * 0.5f, crown.Y + k, bough, 1.0f, light, kBark);
            Branch(sprite, 0.0f, F(trunkTop), k, crown.Y - crown.Ry * 0.5f, std::max(1.0f, 2.0f * k), 1.0f, light, kBark);
            for (int index = 0; index < 9; ++index)
            {
                const float angle = (F(index) + Roll(index, seed + 3u) * 0.8f) * (6.2832f / 9.0f) + 0.3f;
                const float radius = (5.0f + Roll(index, seed + 4u) * 1.8f) * k;
                Clump(sprite, { crown.X + std::cos(angle) * crown.Rx * 0.62f, crown.Y + std::sin(angle) * crown.Ry * 0.6f, radius * 1.05f, radius * 0.85f }, crown, light,
                    leaf, 0, seed + 5u);
            }
            Clump(sprite, { crown.X + light.X * 3.0f * k, crown.Y - 2.0f * k, 5.5f * k, 4.6f * k }, crown, light, leaf, 0, seed + 6u);
        }

        void Birch(Sprite& sprite, float k, Toward light, std::uint32_t seed)
        {
            const float lean = (Roll(0, seed) - 0.5f) * 4.0f * k;
            const int top = -std::max(12, static_cast<int>(std::round(47.0f * k)));
            const auto middleAt = [&](float y)
                {
                    const float share = std::clamp((-1.0f - y) / F(-1 - top), 0.0f, 1.0f);
                    return lean * share * share;
                };
            const Ellipse crown{ lean * 0.6f, -35.0f * k, 12.0f * k, 17.0f * k };
            for (int index = 0; index < 8; ++index)
            {
                const float height = (F(index) + Roll(index, seed + 13u)) / 8.0f;
                const float y = (-24.0f - height * 25.0f) * k;
                const float spread = (2.0f + 6.0f * std::sin(std::min(1.0f, (1.0f - height) * 1.3f) * 1.5708f)) * k;
                const float x = middleAt(y) + (index % 2 == 0 ? -spread : spread) * (0.4f + 0.6f * Roll(index, seed + 14u));
                const float radius = (3.0f + Roll(index, seed + 15u) * 1.4f) * k;
                Clump(sprite, { x, y, radius, radius * 0.9f }, crown, light, kFresh, 0, seed + 16u);
            }
            float tipX[7] = {};
            float tipY[7] = {};
            for (int index = 0; index < 7; ++index)
            {
                const float y = std::round((-19.0f - F(index) * 4.0f) * k);
                const float side = index % 2 == 0 ? -1.0f : 1.0f;
                const float reach = (5.0f + Roll(index, seed + 1u) * 4.0f) * (1.0f - F(index) * 0.08f) * k;
                tipX[index] = middleAt(y) + side * reach;
                tipY[index] = y - (3.0f + Roll(index, seed + 2u) * 3.0f) * k;
                Branch(sprite, middleAt(y), y, tipX[index], tipY[index], 1.0f, 1.0f, light, kBark);
            }
            for (int y = -1; y >= top; --y)
            {
                const float share = F(-1 - y) / F(-1 - top);
                const int base = std::max(2, static_cast<int>(std::round(4.0f * k)));
                const int width = y >= -2 ? base : share < 0.45f ? std::max(1, base - 1) : share < 0.8f ? std::max(1, base - 2) : 1;
                const auto left = static_cast<int>(std::round(middleAt(F(y)) - F(width) * 0.5f));
                const std::uint32_t mark = Hash(-y, seed + 5u);
                const int markFrom = mark % 3u == 0u ? static_cast<int>((mark >> 8) % 2u) : width;
                const int markTo = 1 + static_cast<int>((mark >> 9) % 2u);
                for (int cell = 0; cell < width; ++cell)
                {
                    const int fromLight = light.X < 0.0f ? cell : width - 1 - cell;
                    int level = width == 1 ? 2 : fromLight == 0 ? 3 : fromLight == width - 1 ? 1 : 2;
                    if (cell >= markFrom && cell <= markTo)
                        level = 0;
                    if (y >= -2 && (Hash(cell * 7 - y, seed + 6u) & 1u) == 0u)
                        level = 0;
                    sprite.Put(left + cell, y, kBirch + level);
                }
            }
            for (int index = 0; index < 9; ++index)
            {
                const bool twig = index < 7;
                const float x = twig ? tipX[index] - (tipX[index] - middleAt(tipY[index])) * 0.15f : middleAt(F(top)) + (F(index) - 7.5f) * 4.0f * k;
                const float y = twig ? tipY[index] + (Roll(index, seed + 8u) - 0.5f) * 2.0f * k : F(top) + 2.0f * k;
                const float radius = (2.8f + Roll(index, seed + 9u) * 1.4f) * k;
                Clump(sprite, { x, y, radius, radius * 0.9f }, crown, light, kFresh, 1, seed + 10u);
                if (index % 2 == 0)
                {
                    const auto cx = static_cast<int>(std::round(x + (Roll(index, seed + 11u) - 0.5f) * radius));
                    const auto cy = static_cast<int>(std::round(y + radius * 0.9f));
                    sprite.Put(cx, cy, kCatkin);
                    sprite.Put(cx, cy + 1, kCatkin);
                }
            }
        }

        void Spruce(Sprite& sprite, float k, Toward light, std::uint32_t seed, bool snowy)
        {
            const int height = std::max(14, static_cast<int>(std::round(52.0f * k)));
            const int foot = std::max(2, static_cast<int>(std::round(4.0f * k)));
            const int stem = std::max(1, static_cast<int>(std::round(2.0f * k)));
            for (int y = -1; y >= -foot - 1; --y)
            {
                for (int cell = 0; cell < stem; ++cell)
                    sprite.Put(cell - stem / 2, y, kBark + (cell == (light.X < 0.0f ? 0 : stem - 1) ? 1 : 0));
            }
            const int tiers = std::clamp(height / 9, 3, 7);
            const float widest = 15.0f * k;
            for (int row = 0; row < height - foot; ++row)
            {
                const float t = F(row) / F(std::max(1, height - foot - 1));
                const float tierAt = t * F(tiers);
                const int tier = std::min(tiers - 1, static_cast<int>(tierAt));
                const float local = tierAt - F(tier);
                const float reach = 1.0f + widest * (0.25f + 0.75f * t);
                float half = reach * (0.45f + 0.55f * local);
                if (local > 0.75f && (Hash(row, seed) & 1u) == 0u)
                    half += 1.0f;
                const auto halfCells = static_cast<int>(std::round(half));
                const int y = -foot - (height - foot) + row;
                for (int x = -halfCells; x <= halfCells; ++x)
                {
                    const float across = F(x) / std::max(1.0f, half);
                    const float grain = Roll(((x + 256) >> 1) * 37 + row, seed + 1u) - 0.5f;
                    const float facing = across * light.X + grain * 0.5f + (local - 0.5f) * light.Y * 0.4f;
                    int tone = kNeedle + (facing > 0.45f ? 3 : facing > 0.05f ? 2 : facing > -0.45f ? 1 : 0);
                    if (snowy && (local < 0.2f || (local < 0.4f && grain > 0.15f)))
                        tone = kSnowy + (facing > 0.0f ? 3 : 2);
                    sprite.Put(x, y, tone);
                }
            }
        }

        void BareTree(Sprite& sprite, float k, Toward light, std::uint32_t seed)
        {
            const int trunkTop = -std::max(6, static_cast<int>(std::round(20.0f * k)));
            Trunk(sprite, trunkTop, std::max(2.0f, 4.2f * k), std::max(1.0f, 2.6f * k), (Roll(0, seed) - 0.5f) * 2.0f * k, light, kBark, seed);
            struct Bough
            {
                float X, Y, Angle, Length, Width;
                int Depth;
            };
            std::array<Bough, 48> stack{};
            int count = 0;
            constexpr float kUp = -1.5708f;
            for (const float turn : { -0.75f, -0.15f, 0.55f })
                stack[static_cast<std::size_t>(count++)] = { 0.0f, F(trunkTop) + 1.0f, kUp + turn, 15.0f * k, std::max(1.0f, 2.2f * k), 3 };
            int index = 0;
            while (count > 0)
            {
                const Bough bough = stack[static_cast<std::size_t>(--count)];
                ++index;
                const float x1 = bough.X + std::cos(bough.Angle) * bough.Length;
                const float y1 = bough.Y + std::sin(bough.Angle) * bough.Length;
                Branch(sprite, bough.X, bough.Y, x1, y1, bough.Width, std::max(1.0f, bough.Width * 0.6f), light, kBark);
                if (bough.Depth <= 0 || count + 2 > static_cast<int>(stack.size()))
                    continue;
                const float spread = 0.35f + Roll(index, seed + 3u) * 0.3f;
                const float length = bough.Length * (0.62f + Roll(index, seed + 4u) * 0.12f);
                const float width = std::max(1.0f, bough.Width * 0.6f);
                stack[static_cast<std::size_t>(count++)] = { x1, y1, bough.Angle - spread, length, width, bough.Depth - 1 };
                stack[static_cast<std::size_t>(count++)] = { x1, y1, bough.Angle + spread * 0.8f, length * 0.9f, width, bough.Depth - 1 };
            }
            sprite.Snow(kSnowy + 3, kSnowy + 2, light.X, seed + 7u);
            const int side = light.X < 0.0f ? -1 : 1;
            for (int x = 3; x < static_cast<int>(16.0f * k); ++x)
            {
                const int column = side * x;
                for (int y = trunkTop - 1; y > trunkTop - static_cast<int>(14.0f * k); --y)
                {
                    if (sprite.At(column, y) == 0 || sprite.At(column, y - 1) != 0 || sprite.At(column - side, y - 1) != 0)
                        continue;
                    sprite.Put(column, y - 1, kRobin + 1);
                    sprite.Put(column - side, y - 1, kRobin);
                    sprite.Put(column, y - 2, kRobin);
                    sprite.Put(column + side, y - 2, kBark);
                    return;
                }
            }
        }

        void Bush(Sprite& sprite, float k, Toward light, int leaf, std::uint32_t seed)
        {
            const Ellipse mound{ 0.0f, -7.5f * k, 13.0f * k, 7.5f * k };
            for (int index = 0; index < 5; ++index)
            {
                const float radius = (4.2f + Roll(index, seed + 1u) * 0.6f) * k;
                const float x = (-9.0f + F(index) * 4.5f + (Roll(index, seed + 2u) - 0.5f)) * k;
                Clump(sprite, { x, -4.6f * k, radius, radius * 0.95f }, mound, light, leaf, 1, seed + 3u, -1);
            }
            for (int index = 0; index < 3; ++index)
            {
                const float radius = (4.4f + Roll(index, seed + 6u) * 0.6f) * k;
                const float x = (-5.0f + F(index) * 5.0f + (Roll(index, seed + 7u) - 0.5f)) * k;
                Clump(sprite, { x, -9.2f * k, radius, radius * 0.9f }, mound, light, leaf, 1, seed + 3u, -1);
            }
        }

        void PaintSky(const Setting& s)
        {
            constexpr int kBands = 12;
            const float horizon = s.Horizon;
            float from = 0.0f;
            ImVec4 above;
            for (int band = 0; band < kBands; ++band)
            {
                const float to = band == kBands - 1 ? horizon + 3.0f : std::round(horizon * std::pow(F(band + 1) / F(kBands), 0.7f));
                if (to <= from)
                    continue;
                const ImVec4 color = SkyAt(s, (from + std::min(to, horizon)) * 0.5f / horizon);
                Box(s.Paint, 0.0f, from, s.Width, to, color);
                if (band >= kBands / 2)
                {
                    int x = static_cast<int>(Hash(band, 17u) % 6u);
                    while (x < s.Columns)
                    {
                        const std::uint32_t mixed = Hash(x * 31 + band, 19u);
                        const int length = 2 + static_cast<int>(mixed % 6u);
                        Box(s.Paint, F(x), from, F(x + length), from + 1.0f, above);
                        x += length + 3 + static_cast<int>((mixed >> 8) % 7u);
                    }
                }
                above = color;
                from = to;
            }

            const float night = 1.0f - s.Light.Daylight;
            if (night > 0.0f)
            {
                const ImVec4 star = Colour(0.820f, 0.860f, 0.950f);
                Scatter(s.Columns - 2, static_cast<int>(horizon * 0.78f), 9, 7, 41u, 84u, [&](float x, float y, std::uint32_t mixed)
                    {
                        float bright = ((mixed >> 8) & 3u) == 0u ? 1.0f : 0.5f;
                        if (((mixed >> 10) & 7u) == 0u)
                            bright *= 0.6f + 0.4f * static_cast<float>(std::sin(s.Time * 1.7 + static_cast<double>(mixed & 255u)));
                        bright *= (1.0f - 0.6f * y / std::max(1.0f, horizon)) * night * night;
                        if (bright >= 0.12f)
                            Box(s.Paint, x + 1.0f, y + 1.0f, x + 2.0f, y + 2.0f, Faded(star, bright));
                    });
            }
        }

        void PaintLights(const Setting& s)
        {
            const Canvas& paint = s.Paint;
            const Look& look = s.Light;
            const float radius = s.SunRadius;
            const float moon = std::clamp(1.0f - look.Daylight * 1.6f, 0.0f, 1.0f);
            if (moon > 0.0f)
            {
                const float x = s.MoonX;
                const float y = s.MoonY;
                const float r = radius - 1.0f;
                Disc(paint, x, y, std::round(r * 2.4f), Colour(0.700f, 0.780f, 0.960f, 0.045f * moon));
                Disc(paint, x, y, std::round(r * 1.6f), Colour(0.700f, 0.780f, 0.960f, 0.05f * moon));
                Disc(paint, x, y, r, Colour(0.880f, 0.900f, 0.860f, moon));
                const ImVec4 sea = Colour(0.740f, 0.770f, 0.760f, moon);
                Box(paint, x - std::round(r * 0.55f), y - std::round(r * 0.45f), x - std::round(r * 0.55f) + std::max(1.0f, std::round(r * 0.4f)),
                    y - std::round(r * 0.45f) + std::max(1.0f, std::round(r * 0.3f)), sea);
                Box(paint, x + std::round(r * 0.15f), y + std::round(r * 0.2f), x + std::round(r * 0.15f) + std::max(1.0f, std::round(r * 0.45f)),
                    y + std::round(r * 0.2f) + std::max(1.0f, std::round(r * 0.35f)), sea);
                Box(paint, x - std::round(r * 0.4f), y + std::round(r * 0.5f), x - std::round(r * 0.4f) + 1.0f, y + std::round(r * 0.5f) + 1.0f, sea);
            }
            const float sun = std::clamp((look.Daylight - 0.3f) * 5.0f, 0.0f, 1.0f);
            if (sun > 0.0f)
            {
                const float x = s.SunX;
                const float y = s.SunY;
                const ImVec4 rim = Mix(Colour(1.000f, 0.900f, 0.600f), Colour(1.000f, 0.680f, 0.420f), look.Glow * 0.8f);
                const ImVec4 body = Mix(Colour(1.000f, 0.960f, 0.800f), Colour(1.000f, 0.820f, 0.600f), look.Glow * 0.8f);
                Disc(paint, x, y, std::round(radius * 2.8f), Colour(1.000f, 0.930f, 0.760f, (0.045f + 0.06f * look.Glow) * sun));
                Disc(paint, x, y, std::round(radius * 1.8f), Colour(1.000f, 0.930f, 0.760f, (0.06f + 0.08f * look.Glow) * sun));
                Disc(paint, x, y, radius, Faded(rim, sun));
                Disc(paint, x, y, radius - 1.0f, Faded(body, sun));
                Box(paint, x - std::round(radius * 0.5f), y - std::round(radius * 0.5f), x - std::round(radius * 0.5f) + 2.0f, y - std::round(radius * 0.5f) + 1.0f,
                    Colour(1.0f, 1.0f, 0.960f, sun));
            }
        }

        void Heap(Sprite& sprite, float width, float height, Toward light, std::uint32_t seed)
        {
            const int bumps = 3 + static_cast<int>(Hash(0, seed) % 3u);
            const Ellipse whole{ 0.0f, -height * 0.5f, width * 0.5f, height * 0.6f };
            for (int index = 0; index < bumps; ++index)
            {
                const float share = (F(index) + 0.5f) / F(bumps) - 0.5f;
                const float radius = height * (0.45f + 0.5f * (1.0f - std::fabs(share) * 1.6f)) * (0.85f + Roll(index, seed + 1u) * 0.3f);
                Clump(sprite, { share * width * 0.78f, -radius * 0.62f, radius * 1.2f, radius * 0.85f }, whole, light, 1, 0, seed + 2u, -1, 0.06f);
            }
            sprite.Sprinkle(4, 3, 256u, seed);
        }

        void PaintClouds(const Setting& s, bool banked)
        {
            const Palette& colors = *s.Colors;
            const Look& look = s.Light;
            const float rows = F(s.Rows);
            if (banked)
            {
                constexpr float kHeaps[] = { 0.9f, 1.1f, 0.6f, 0.4f };
                constexpr float kHeapSize[] = { 0.9f, 1.25f, 0.8f, 0.7f };
                const ImVec4 low = SkyAt(s, 0.85f);
                std::array<ImVec4, 5> heap{};
                const float pale = 0.25f + 0.4f * (1.0f - look.Daylight);
                heap[1] = Mix(look.Far(colors.CloudShade, 0.3f), low, pale);
                heap[2] = Mix(look.Far(colors.CloudBody, 0.3f), low, pale);
                heap[3] = Mix(look.Far(colors.CloudLit, 0.25f), low, pale * 0.8f);
                heap[4] = Mix(look.Far(colors.CloudLit, 0.15f), low, pale * 0.6f);
                constexpr float kSpacing = 70.0f;
                for (int index = 0; F(index) * kSpacing < s.Width + kSpacing; ++index)
                {
                    const auto salt = static_cast<std::uint32_t>(index) * 17u + 71u;
                    if (Roll(index, salt + 5u) > kHeaps[s.Season] * 0.75f)
                        continue;
                    const float x = std::round(F(index) * kSpacing + Roll(index, salt) * kSpacing * 0.6f);
                    const float size = kHeapSize[s.Season] * (0.7f + Roll(index, salt + 2u) * 0.6f);
                    const float height = std::max(4.0f, std::round(rows * 0.075f * size));
                    const float base = std::round(s.Horizon - rows * (0.07f + Roll(index, salt + 1u) * 0.04f));
                    Sprite sprite;
                    Heap(sprite, height * 3.2f, height, TowardLight(s, x), salt);
                    sprite.Flush(s.Paint, x, base, heap.data());
                }
                return;
            }

            const float strength = 0.35f + 0.25f * look.Daylight;
            const float thick = std::max(3.0f, std::round(2.0f + 1.6f * s.Size));
            for (int layer = 0; layer < 2; ++layer)
            {
                if (s.Season == kWinter && layer == 1)
                    break;
                const double travelled = s.Time * (layer == 0 ? 0.55 : 0.9);
                const double spacing = layer == 0 ? 170.0 : 230.0;
                const float y = std::round(s.Horizon * (layer == 0 ? 0.36f : 0.55f));
                const ImVec4 sky = SkyAt(s, y / s.Horizon);
                const ImVec4 tones[] = { Mix(sky, look(colors.CloudShade), strength * 0.7f), Mix(sky, look(colors.CloudBody), strength),
                    Mix(sky, look(colors.CloudLit), strength * 1.15f) };
                const auto first = static_cast<long long>(std::floor((-80.0 - travelled) / spacing));
                const auto last = static_cast<long long>(std::floor((static_cast<double>(s.Columns) - travelled) / spacing));
                for (long long slot = first; slot <= last; ++slot)
                {
                    const int index = static_cast<int>(slot) * 2 + layer;
                    const std::uint32_t mixed = Hash(index, 77u);
                    if (mixed % 4u == 0u)
                        continue;
                    const float length = std::round((24.0f + F(static_cast<int>((mixed >> 8) % 26u))) * (layer == 0 ? 1.0f : 0.75f) * std::min(1.0f, 0.55f + 0.3f * s.Size));
                    const float left = std::round(static_cast<float>(static_cast<double>(slot) * spacing + travelled) + F(static_cast<int>((mixed >> 16) % 60u)));
                    const float top = y + F(static_cast<int>((mixed >> 24) % 5u)) - 2.0f;
                    for (int puff = 0; puff < 3; ++puff)
                    {
                        const float share = puff == 1 ? 0.5f : puff == 0 ? 0.25f : 0.72f;
                        const float half = length * (puff == 1 ? 0.36f : 0.26f);
                        const float lift = puff == 1 ? 0.0f : 1.0f;
                        const float middle = left + length * share;
                        const int height = static_cast<int>(thick - lift);
                        for (int row = 0; row < height; ++row)
                        {
                            const float down = (F(row) + 0.5f) / F(height);
                            const float span = std::round(half * std::sqrt(std::max(0.0f, 1.0f - (1.0f - down) * (1.0f - down) * 0.85f)));
                            const int tone = row == height - 1 ? 0 : down < 0.4f ? 2 : 1;
                            Box(s.Paint, middle - span, top + lift + F(row), middle + span, top + lift + F(row) + 1.0f, tones[tone]);
                        }
                    }
                }
            }
        }

        struct Peak
        {
            float X = 0.0f;
            float Height = 0.0f;
            float Slope = 1.0f;
        };

        constexpr float kPeakSpacing = 46.0f;

        Peak PeakOf(const Setting& s, int index)
        {
            Peak peak;
            peak.X = F(index) * kPeakSpacing + Roll(index, 301u) * kPeakSpacing * 0.7f;
            const bool tall = Hash(index, 302u) % 3u == 0u;
            peak.Height = F(s.Rows) * (tall ? 0.14f + 0.05f * Roll(index, 303u) : 0.07f + 0.05f * Roll(index, 303u));
            peak.Slope = peak.Height / (kPeakSpacing * (0.5f + 0.3f * Roll(index, 304u)));
            return peak;
        }

        float RidgeAt(const Setting& s, int column, Peak& owner)
        {
            const int closest = static_cast<int>(std::floor(F(column) / kPeakSpacing));
            float best = F(s.Rows) * 0.03f;
            owner = PeakOf(s, closest);
            for (int index = closest - 2; index <= closest + 2; ++index)
            {
                const Peak peak = PeakOf(s, index);
                const float height = peak.Height - std::fabs(F(column) - peak.X) * peak.Slope;
                if (height > best)
                {
                    best = height;
                    owner = peak;
                }
            }
            return best + std::sin(F(column) * 0.45f + owner.X) * 0.7f;
        }

        void Mist(const Setting& s, float top, int rows, ImVec4 colour, float alpha)
        {
            for (int row = 0; row < rows; ++row)
            {
                const float middle = F(rows - 1) * 0.5f;
                const float share = 1.0f - std::fabs(F(row) - middle) / (middle + 1.0f);
                Box(s.Paint, 0.0f, top + F(row), s.Width, top + F(row) + 1.0f, Faded(colour, alpha * share));
            }
        }

        void PaintRange(const Setting& s)
        {
            const Palette& colors = *s.Colors;
            const Look& look = s.Light;
            const float horizon = s.Horizon;
            const float rows = F(s.Rows);
            const float bottom = horizon + 2.0f;
            const ImVec4 tones[] = { look.Far(colors.RangeLit, 0.5f), look.Far(colors.RangeShade, 0.5f), look.Far(colors.CapLit, 0.38f), look.Far(colors.CapShade, 0.38f) };
            if (s.Season == kSummer || s.Season == kAutumn)
            {
                const auto topAt = [&](float x)
                    {
                        return horizon - rows * (0.115f + 0.03f * std::sin(x * 0.021f + 0.4f) + 0.02f * std::sin(x * 0.057f + 2.0f)) - std::sin(x * 0.19f) * 0.6f;
                    };
                ColumnRuns(s.Paint, s.Columns, tones, [&](int column)
                    {
                        const float x = F(column);
                        return Column{ std::round(topAt(x)), bottom, FacesLight(s, topAt(x + 1.0f) - topAt(x - 1.0f), x) ? 2 : 0 };
                    });
                if (s.Season == kAutumn)
                    Mist(s, std::round(horizon - rows * 0.1f), std::max(3, static_cast<int>(rows * 0.04f)), look.Haze, 0.4f + 0.25f * (1.0f - look.Daylight));
                return;
            }
            const bool winter = s.Season == kWinter;
            ColumnRuns(s.Paint, s.Columns, tones, [&](int column)
                {
                    Peak peak;
                    const float top = std::round(horizon - RidgeAt(s, column, peak));
                    const bool lit = (F(column) < peak.X ? -1.0f : 1.0f) == SideOfLight(s, F(column));
                    return Column{ top, bottom, winter ? (lit ? 2 : 3) : (lit ? 0 : 1) };
                });
            const float line = horizon - rows * 0.12f;
            ColumnRuns(s.Paint, s.Columns, tones, [&](int column)
                {
                    Peak peak;
                    const float height = RidgeAt(s, column, peak);
                    const float top = std::round(horizon - height);
                    const bool lit = (F(column) < peak.X ? -1.0f : 1.0f) == SideOfLight(s, F(column));
                    const std::uint32_t mixed = Hash(column, 305u);
                    if (winter)
                    {
                        if (height < rows * 0.07f || mixed % (lit ? 7u : 3u) != 0u)
                            return Column{};
                        const float from = top + 2.0f + F(static_cast<int>((mixed >> 8) % 4u));
                        return Column{ from, std::min(bottom, std::round(from + 2.0f + height * 0.25f * Roll(column, 306u))), lit ? 0 : 1 };
                    }
                    const float edge = line + F(static_cast<int>(Hash(column / 2, 307u) % 3u)) - 1.0f;
                    if (top >= edge)
                        return Column{};
                    return Column{ top, edge, lit ? 2 : 3 };
                });
        }

        float FarHillLine(const Setting& s, float at)
        {
            return s.Horizon - F(s.Rows) * (0.072f + 0.02f * std::sin(at * 0.031f + 1.3f) + 0.012f * std::sin(at * 0.083f + 0.2f)) - std::sin(at * 0.17f) * 0.4f;
        }

        float HillLine(const Setting& s, float at)
        {
            return s.Horizon - F(s.Rows) * (0.042f + 0.016f * std::sin(at * 0.047f + 4.0f) + 0.008f * std::sin(at * 0.12f + 1.0f));
        }

        float FarHillTop(const Setting& s, int column)
        {
            return std::round(FarHillLine(s, F(column)));
        }

        float HillTop(const Setting& s, int column)
        {
            return std::round(HillLine(s, F(column)));
        }

        void Farm(Sprite& sprite, float toLight)
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

        void Windmill(Sprite& sprite, float toLight, int tall, double angle)
        {
            const bool fromLeft = toLight < 0.0f;
            for (int y = -1; y >= -tall; --y)
            {
                const int half = y >= -tall / 2 ? 2 : 1;
                for (int x = -half; x <= half; ++x)
                    sprite.Put(x, y, (fromLeft ? x == half : x == -half) ? 2 : 1);
            }
            sprite.Put(0, -1, 4);
            sprite.Put(0, -2, 4);
            sprite.Put(0, -tall + 3, 4);
            for (int x = -2; x <= 2; ++x)
                sprite.Put(x, -tall - 1, 3);
            for (int x = -1; x <= 1; ++x)
                sprite.Put(x, -tall - 2, 3);
            const int hub = -tall - 1;
            const int length = tall - 1;
            for (int sail = 0; sail < 4; ++sail)
            {
                const double turn = angle + static_cast<double>(sail) * 1.5707963;
                const auto cx = static_cast<float>(std::cos(turn));
                const auto cy = static_cast<float>(std::sin(turn));
                for (int step = 1; step <= length; ++step)
                {
                    const float along = F(step);
                    sprite.Put(static_cast<int>(std::round(cx * along)), hub + static_cast<int>(std::round(cy * along)), 6);
                    if (step * 3 >= length)
                    {
                        for (const float off : { 1.0f, 2.0f })
                        {
                            const auto ox = static_cast<int>(std::round(cx * along - cy * off));
                            const auto oy = hub + static_cast<int>(std::round(cy * along + cx * off));
                            if (sprite.At(ox, oy) == 0)
                                sprite.Put(ox, oy, 5);
                        }
                    }
                }
            }
            sprite.Put(0, hub, 4);
        }

        void PaintHills(const Setting& s, float farmX, float millX)
        {
            const Palette& colors = *s.Colors;
            const Look& look = s.Light;
            const Canvas& paint = s.Paint;
            const float bottom = s.Horizon + 3.0f;
            const bool winter = s.Season == kWinter;
            constexpr float kFarHaze = 0.42f;
            const ImVec4 far[] = { look.Far(colors.FarHill, kFarHaze), look.Far(colors.FarHillShade, kFarHaze), look.Far(colors.FarWood, kFarHaze) };
            ColumnRuns(paint, s.Columns, far, [&](int column)
                {
                    const float x = F(column);
                    return Column{ FarHillTop(s, column), bottom, FacesLight(s, FarHillLine(s, x - 1.0f) - FarHillLine(s, x + 1.0f), x) ? 1 : 0 };
                });
            ColumnRuns(paint, s.Columns, far, [&](int column)
                {
                    const float at = F(column);
                    if (std::sin(at * 0.043f + 2.0f) + 0.6f * std::sin(at * 0.11f) < (winter ? 0.2f : 0.5f))
                        return Column{};
                    const float top = FarHillTop(s, column) - F(static_cast<int>(Hash(column / 3, 311u) % 2u));
                    return Column{ top, top + 2.0f + F(static_cast<int>(Hash(column / 3, 312u) % 2u)) + std::round(s.Size), 2 };
                });
            if (s.Season == kAutumn)
                Mist(s, std::round(s.Horizon - F(s.Rows) * 0.06f), std::max(2, static_cast<int>(F(s.Rows) * 0.03f)), look.Haze, 0.35f + 0.25f * (1.0f - look.Daylight));

            constexpr float kHaze = 0.3f;
            const ImVec4 near[] = { look.Far(colors.Hill, kHaze), look.Far(colors.HillShade, kHaze) };
            ColumnRuns(paint, s.Columns, near, [&](int column)
                {
                    const float x = F(column);
                    return Column{ HillTop(s, column), bottom, FacesLight(s, HillLine(s, x - 1.0f) - HillLine(s, x + 1.0f), x) ? 1 : 0 };
                });

            constexpr float kFieldHaze = 0.36f;
            const ImVec4 hedge = look.Far(colors.Hedge, kHaze);
            ImVec4 fields[4][2];
            for (int kind = 0; kind < 4; ++kind)
            {
                fields[kind][0] = look.Far(colors.Fields[kind][0], kFieldHaze);
                fields[kind][1] = look.Far(colors.Fields[kind][1], kFieldHaze);
            }
            const float deep = std::max(2.0f, std::round(F(s.Rows) * 0.022f));
            int start = -static_cast<int>(Hash(0, 320u) % 20u);
            for (int index = 0; start < s.Columns; ++index)
            {
                const int length = 18 + static_cast<int>(Hash(index, 321u) % 24u);
                std::uint32_t kind = Hash(index, 322u) % 7u;
                if (kind == 1u && length > 24)
                    kind = 0u;
                const int end = std::min(s.Columns, start + length);
                const float skewFrom = F(1 + static_cast<int>(Hash(index, 323u) % 3u));
                const float skewTo = F(1 + static_cast<int>(Hash(index, 324u) % 3u));
                const float rows = deep + F(static_cast<int>(Hash(index, 325u) % 3u));
                const auto topAt = [&](int column)
                    {
                        return HillTop(s, column) + std::round(skewFrom + (skewTo - skewFrom) * F(column - start) / F(std::max(1, length)));
                    };
                if (kind < 4u && end > 0)
                {
                    float highest = bottom;
                    for (int column = std::max(0, start); column < end; ++column)
                        highest = std::min(highest, topAt(column));
                    for (float row = highest; row < highest + rows + 4.0f && row < bottom; row += 1.0f)
                    {
                        int run = std::max(0, start);
                        int runTone = -1;
                        for (int column = run; column <= end; ++column)
                        {
                            int tone = -1;
                            const float line = column < end ? row - topAt(column) : -1.0f;
                            if (line >= 0.0f && line < rows)
                                tone = kind == 3u ? (static_cast<int>(line) % 3 == 1 && (column + static_cast<int>(row)) % 3 == 0 ? 1 : 0) : static_cast<int>(line) % 2;
                            if (tone == runTone)
                                continue;
                            if (runTone >= 0)
                                Box(paint, F(run), row, F(column), row + 1.0f, fields[kind][runTone]);
                            run = column;
                            runTone = tone;
                        }
                    }
                    int run = std::max(0, start);
                    float runTop = topAt(run) + rows;
                    for (int column = run + 1; column <= end; ++column)
                    {
                        const float here = column < end ? topAt(column) + rows : -1.0f;
                        if (here == runTop)
                            continue;
                        Box(paint, F(run), runTop, F(column), runTop + 1.0f, hedge);
                        run = column;
                        runTop = here;
                    }
                    if (start > 0)
                        Box(paint, F(start), topAt(start) - 1.0f, F(start + 1), topAt(start) + rows + 1.0f, hedge);
                }
                start = end;
            }

            constexpr ImVec4 kFarmTones[4][12] = {
                { Colour(0.0f, 0.0f, 0.0f, 0.0f), Colour(0.940f, 0.910f, 0.840f), Colour(0.740f, 0.710f, 0.690f), Colour(0.800f, 0.340f, 0.270f), Colour(0.600f, 0.240f, 0.220f),
                    Colour(0.250f, 0.200f, 0.200f), Colour(0.640f, 0.290f, 0.220f), Colour(0.480f, 0.200f, 0.170f), Colour(0.960f, 0.760f, 0.840f), Colour(0.800f, 0.560f, 0.680f),
                    Colour(0.360f, 0.250f, 0.180f), Colour(0.300f, 0.280f, 0.300f) },
                { Colour(0.0f, 0.0f, 0.0f, 0.0f), Colour(0.940f, 0.910f, 0.840f), Colour(0.740f, 0.710f, 0.690f), Colour(0.780f, 0.330f, 0.260f), Colour(0.580f, 0.230f, 0.210f),
                    Colour(0.250f, 0.200f, 0.200f), Colour(0.640f, 0.290f, 0.220f), Colour(0.480f, 0.200f, 0.170f), Colour(0.300f, 0.500f, 0.250f), Colour(0.180f, 0.360f, 0.190f),
                    Colour(0.360f, 0.250f, 0.180f), Colour(0.300f, 0.280f, 0.300f) },
                { Colour(0.0f, 0.0f, 0.0f, 0.0f), Colour(0.900f, 0.860f, 0.780f), Colour(0.700f, 0.660f, 0.620f), Colour(0.720f, 0.300f, 0.230f), Colour(0.540f, 0.210f, 0.190f),
                    Colour(0.250f, 0.200f, 0.200f), Colour(0.600f, 0.270f, 0.200f), Colour(0.450f, 0.190f, 0.160f), Colour(0.700f, 0.300f, 0.160f), Colour(0.520f, 0.200f, 0.120f),
                    Colour(0.360f, 0.250f, 0.180f), Colour(0.300f, 0.280f, 0.300f) },
                { Colour(0.0f, 0.0f, 0.0f, 0.0f), Colour(0.880f, 0.860f, 0.820f), Colour(0.660f, 0.660f, 0.700f), Colour(0.940f, 0.960f, 0.990f), Colour(0.720f, 0.780f, 0.880f),
                    Colour(0.250f, 0.200f, 0.200f), Colour(0.600f, 0.270f, 0.220f), Colour(0.440f, 0.190f, 0.170f), Colour(0.880f, 0.920f, 0.970f), Colour(0.300f, 0.270f, 0.270f),
                    Colour(0.300f, 0.230f, 0.200f), Colour(0.300f, 0.280f, 0.300f) },
            };
            const ImVec4 lamplight = Colour(1.000f, 0.820f, 0.420f);
            const float lamps = std::clamp((0.7f - look.Daylight) * 2.0f, 0.0f, 1.0f);
            std::array<ImVec4, 12> farmColors{};
            for (std::size_t index = 1; index < farmColors.size(); ++index)
                farmColors[index] = look.Far(kFarmTones[s.Season][index], kHaze);
            farmColors[11] = Mix(farmColors[11], lamplight, lamps);
            Sprite farm;
            Farm(farm, SideOfLight(s, farmX));
            float ground = 0.0f;
            for (int column = static_cast<int>(farmX) - 1; column <= static_cast<int>(farmX) + 22; ++column)
                ground = std::max(ground, HillTop(s, column));
            farm.Flush(paint, farmX, ground + 2.0f, farmColors.data());

            const ImVec4 millTones[] = { Colour(0.0f, 0.0f, 0.0f, 0.0f), look.Far(Colour(0.930f, 0.910f, 0.860f), kFarHaze), look.Far(Colour(0.700f, 0.680f, 0.680f), kFarHaze),
                look.Far(winter ? Colour(0.920f, 0.940f, 0.980f) : Colour(0.360f, 0.260f, 0.210f), kFarHaze), Mix(look.Far(Colour(0.260f, 0.210f, 0.200f), kFarHaze), lamplight, lamps),
                look.Far(Colour(0.900f, 0.870f, 0.790f), kFarHaze), look.Far(Colour(0.420f, 0.320f, 0.250f), kFarHaze) };
            Sprite mill;
            const int tall = s.Size < 0.9f ? 8 : 11;
            Windmill(mill, SideOfLight(s, millX), tall, 0.7853982 + s.Time * 0.4);
            float millGround = 0.0f;
            for (int column = static_cast<int>(millX) - 2; column <= static_cast<int>(millX) + 2; ++column)
                millGround = std::max(millGround, FarHillTop(s, column));
            mill.Flush(paint, millX, millGround + 1.0f, millTones);
        }

        void FarFir(const Canvas& paint, float x, float base, int height, const ImVec4* tones, float toLight, bool snowy)
        {
            Box(paint, x, base - 2.0f, x + 1.0f, base, tones[3]);
            const int crown = height - 2;
            for (int row = 0; row < crown; ++row)
            {
                const float t = F(row) / F(std::max(1, crown - 1));
                const float half = std::round(0.4f + t * F(height) * 0.26f + F((row % 3 == 2) ? 1 : 0) * t);
                const float y = base - 2.0f - F(crown - row);
                const bool cap = snowy && row % 3 == 0;
                Box(paint, x - half, y, x + half + 1.0f, y + 1.0f, cap ? tones[4] : tones[0]);
                if (half >= 1.0f && !cap)
                {
                    if (toLight < 0.0f)
                        Box(paint, x - half, y, x, y + 1.0f, tones[1]);
                    else
                        Box(paint, x + 1.0f, y, x + half + 1.0f, y + 1.0f, tones[1]);
                }
            }
        }

        struct FarTree
        {
            float X = 0.0f;
            float Radius = 0.0f;
            float Tall = 1.0f;
            float Middle = 0.0f;
            int Kind = -1;
        };

        constexpr int kTreeSpacing = 6;

        FarTree FarTreeOf(const Setting& s, int index, float farmX, float millX)
        {
            FarTree tree;
            const std::uint32_t mixed = Hash(index, 41u);
            tree.X = F(index * kTreeSpacing + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(kTreeSpacing)));
            const float grove = std::sin(tree.X * 0.031f + 0.7f) + 0.6f * std::sin(tree.X * 0.087f + 2.0f);
            if (grove < -0.3f || mixed % 12u == 0u || (tree.X > farmX - 5.0f && tree.X < farmX + 25.0f) || std::fabs(tree.X - millX) < 9.0f)
                return tree;
            tree.Radius = 1.6f + F(static_cast<int>((mixed >> 12) % 3u)) * 0.7f + std::min(2.2f, s.Size * 1.2f) + std::min(1.0f, std::max(0.0f, grove));
            const std::uint32_t roll = (mixed >> 20) % 20u;
            switch (s.Season)
            {
            case kSpring:
                tree.Kind = roll < 8u ? 0 : roll < 15u ? 1 : roll < 18u ? 2 : 3;
                break;
            case kSummer:
                tree.Kind = roll < 9u ? 0 : roll < 16u ? 1 : 2;
                break;
            default:
                tree.Kind = roll < 6u ? 0 : roll < 11u ? 1 : roll < 17u ? 2 : 3;
                break;
            }
            const bool slim = (s.Season == kSummer && tree.Kind == 2) || (s.Season == kAutumn && tree.Kind == 3);
            if (slim)
            {
                tree.Radius = std::max(1.5f, tree.Radius * 0.55f);
                tree.Tall = 2.8f;
            }
            else
            {
                tree.Tall = 1.1f;
            }
            tree.Middle = std::round(s.Horizon - tree.Radius * tree.Tall * 0.45f - F(static_cast<int>((mixed >> 26) % 2u)));
            return tree;
        }

        void PaintTreeLine(const Setting& s, float farmX, float millX)
        {
            const Palette& colors = *s.Colors;
            const Look& look = s.Light;
            const float base = s.Horizon + 2.0f;
            if (s.Season == kWinter)
            {
                constexpr float kHaze = 0.36f;
                const ImVec4 firs[] = { look.Far(colors.Crowns[1], kHaze), look.Far(colors.Crowns[2], kHaze), look.Far(colors.Crowns[2], kHaze), look.Far(colors.Trunk, kHaze),
                    look.Far(colors.Accents[2], kHaze * 0.8f) };
                const ImVec4 bark = firs[3];
                constexpr int kSpacing = 8;
                for (int index = -1; index * kSpacing < s.Columns + kSpacing; ++index)
                {
                    const std::uint32_t mixed = Hash(index, 41u);
                    const float x = F(index * kSpacing + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(kSpacing)));
                    const float grove = std::sin(x * 0.031f + 0.7f) + 0.6f * std::sin(x * 0.087f + 2.0f);
                    if (grove < -0.25f || mixed % 10u < 2u || (x > farmX - 6.0f && x < farmX + 26.0f) || std::fabs(x - millX) < 12.0f)
                        continue;
                    const float radius = std::round(2.0f + F(static_cast<int>((mixed >> 12) % 3u)) + std::min(2.0f, s.Size * 1.1f));
                    if ((mixed >> 20) % 10u >= 7u)
                    {
                        const float tall = radius * 2.0f + 3.0f;
                        Box(s.Paint, x, base - tall, x + 1.0f, base, bark);
                        for (int twig = 0; twig < 3; ++twig)
                        {
                            const float y = base - tall + 2.0f + F(twig * 2);
                            const float reach = 1.0f + F(static_cast<int>((mixed >> (4 + twig * 2)) % 3u));
                            const bool left = ((mixed >> twig) & 1u) != 0u;
                            Box(s.Paint, left ? x - reach : x + 1.0f, y, left ? x : x + 1.0f + reach, y + 1.0f, bark);
                        }
                        continue;
                    }
                    FarFir(s.Paint, x, base, static_cast<int>(radius * 2.6f + 4.0f), firs, SideOfLight(s, x), true);
                }
                return;
            }

            const float haze = s.Season == kAutumn ? 0.42f : 0.36f;
            const ImVec4 sunlit = Colour(1.0f, 0.97f, 0.85f);
            const auto pair = [&](ImVec4 day) { return std::array<ImVec4, 2>{ look.Far(day, haze), look.Far(Mix(day, sunlit, 0.3f), haze) }; };
            std::array<std::array<ImVec4, 2>, 4> kinds{};
            if (s.Season == kAutumn)
                kinds = { pair(colors.Crowns[1]), pair(colors.Crowns[0]), pair(colors.Accents[1]), pair(colors.Accents[2]) };
            else
                kinds = { pair(colors.Crowns[0]), pair(colors.Crowns[1]), pair(colors.Accents[0]), pair(colors.Accents[1]) };
            std::array<ImVec4, 10> tones{};
            for (std::size_t kind = 0; kind < 4; ++kind)
            {
                tones[kind * 2] = kinds[kind][0];
                tones[kind * 2 + 1] = kinds[kind][1];
            }
            tones[8] = look.Far(Mix(colors.Crowns[0], colors.Trunk, 0.4f), haze);
            tones[9] = look.Far(colors.Trunk, haze);
            const auto ownerAt = [&](int column, float& top)
                {
                    FarTree best;
                    top = 1.0e6f;
                    const int slot = static_cast<int>(std::floor(F(column) / F(kTreeSpacing)));
                    for (int index = slot - 2; index <= slot + 1; ++index)
                    {
                        const FarTree tree = FarTreeOf(s, index, farmX, millX);
                        if (tree.Kind < 0)
                            continue;
                        const float across = (F(column) + 0.5f - tree.X) / (tree.Radius + 0.5f);
                        if (std::fabs(across) >= 1.0f)
                            continue;
                        const float here = std::round(tree.Middle - std::sqrt(1.0f - across * across) * tree.Radius * tree.Tall);
                        if (here < top)
                        {
                            top = here;
                            best = tree;
                        }
                    }
                    return best;
                };
            ColumnRuns(s.Paint, s.Columns, tones.data(), [&](int column)
                {
                    float top = 0.0f;
                    const FarTree tree = ownerAt(column, top);
                    if (tree.Kind < 0)
                        return Column{};
                    return Column{ top, tree.Middle + 1.0f, tree.Kind * 2 };
                });
            ColumnRuns(s.Paint, s.Columns, tones.data(), [&](int column)
                {
                    float top = 0.0f;
                    const FarTree tree = ownerAt(column, top);
                    if (tree.Kind < 0)
                        return Column{};
                    return Column{ tree.Middle + 1.0f, base, 8 };
                });
            ColumnRuns(s.Paint, s.Columns, tones.data(), [&](int column)
                {
                    float top = 0.0f;
                    const FarTree tree = ownerAt(column, top);
                    if (tree.Kind < 0)
                        return Column{};
                    const float across = (F(column) + 0.5f - tree.X) * SideOfLight(s, tree.X) / tree.Radius;
                    if (across < -0.15f)
                        return Column{};
                    const float deep = std::round(tree.Radius * tree.Tall * (0.9f - std::max(0.0f, across) * 0.5f));
                    return Column{ top, std::min(tree.Middle + 1.0f, top + std::max(1.0f, deep)), tree.Kind * 2 + 1 };
                });
            const float shade = std::max(1.0f, std::round(s.Size));
            for (int index = 0; index * kTreeSpacing < s.Columns; index += 3)
            {
                const FarTree tree = FarTreeOf(s, index, farmX, millX);
                if (tree.Kind >= 0 && tree.Tall < 1.0f)
                    Box(s.Paint, tree.X, base - 2.0f - shade, tree.X + 1.0f, base, tones[9]);
            }
        }

        void Flower(const Setting& s, float x, float y, int kind)
        {
            const Palette& colors = *s.Colors;
            const Canvas& paint = s.Paint;
            const float depth = DepthOf(s, y);
            const ImVec4 petal = OnMeadow(s, colors.Petals[kind], y);
            if (depth < 0.3f)
            {
                Box(paint, x, y - 1.0f, x + 1.0f, y, petal);
                return;
            }
            const ImVec4 stalk = OnMeadow(s, colors.BladeDark, y);
            if (depth < 0.62f)
            {
                Box(paint, x, y - 1.0f, x + 1.0f, y, stalk);
                Box(paint, x, y - 2.0f, x + (kind == 0 ? 2.0f : 1.0f), y - 1.0f, petal);
                return;
            }
            const ImVec4 heart = OnMeadow(s, colors.Hearts[kind], y);
            Box(paint, x, y - 2.0f, x + 1.0f, y, stalk);
            Box(paint, x - 1.0f, y - 3.0f, x + 2.0f, y - 2.0f, petal);
            if (kind % 2 == 0)
                Box(paint, x, y - 4.0f, x + 1.0f, y - 3.0f, petal);
            Box(paint, x, y - 3.0f, x + 1.0f, y - 2.0f, heart);
        }

        void PaintLand(const Setting& s)
        {
            const Palette& colors = *s.Colors;
            const Canvas& paint = s.Paint;
            const float horizon = s.Horizon;
            const float bottom = F(s.Rows);
            const auto edgeOf = [&](int column) { return std::round(horizon + 0.9f * std::sin(F(column) * 0.09f + 0.5f) + 0.6f * std::sin(F(column) * 0.23f + 1.7f)); };
            const ImVec4 edge[] = { GroundAt(s, horizon + 1.0f), OnMeadow(s, colors.MeadowRim, horizon) };
            ColumnRuns(paint, s.Columns, edge, [&](int column) { return Column{ edgeOf(column), horizon + 3.0f, 0 }; });
            ColumnRuns(paint, s.Columns, edge, [&](int column) { return Column{ edgeOf(column), edgeOf(column) + 1.0f, 1 }; });

            constexpr int kBands = 11;
            const float first = horizon + 3.0f;
            const float depth = bottom - first;
            for (int band = 0; band < kBands; ++band)
            {
                const float from = std::round(first + depth * std::pow(F(band) / F(kBands), 1.35f));
                const float to = band == kBands - 1 ? bottom : std::round(first + depth * std::pow(F(band + 1) / F(kBands), 1.35f));
                Box(paint, 0.0f, from, s.Width, to, GroundAt(s, (from + to) * 0.5f));
            }

            const int patches = std::max(4, s.Columns / 30);
            const float span = s.Ground - horizon;
            for (int index = 0; index < patches; ++index)
            {
                const float share = 0.06f + 0.9f * Roll(index, 511u);
                const float cy = std::round(horizon + 4.0f + share * (span - 4.0f));
                const float cx = std::round(Roll(index, 512u) * s.Width);
                const float rx = (8.0f + Roll(index, 513u) * 16.0f) * (0.45f + share) * std::min(1.4f, 0.5f + s.Size * 0.5f);
                const float ry = std::max(1.0f, std::round((1.0f + share * 2.5f) * std::min(1.5f, 0.5f + s.Size * 0.5f)));
                const ImVec4 color = Mix(GroundAt(s, cy), OnMeadow(s, index % 2 == 0 ? colors.PatchDark : colors.PatchLight, cy), s.Season == kWinter ? 0.35f : 0.45f);
                for (float row = -ry; row <= ry; row += 1.0f)
                {
                    const float part = row / (ry + 0.5f);
                    const float half = std::round(rx * std::sqrt(std::max(0.0f, 1.0f - part * part)));
                    if (half > 0.0f)
                        Box(paint, cx - half, cy + row, cx + half, cy + row + 1.0f, color);
                }
            }
        }

        void PaintGrass(const Setting& s)
        {
            const Palette& colors = *s.Colors;
            const Canvas& paint = s.Paint;
            const float bottom = F(s.Rows);
            const bool autumn = s.Season == kAutumn;
            constexpr ImVec4 kFallen[] = { Colour(0.760f, 0.230f, 0.100f), Colour(0.860f, 0.440f, 0.120f), Colour(0.820f, 0.620f, 0.200f), Colour(0.560f, 0.300f, 0.110f),
                Colour(0.680f, 0.160f, 0.090f) };
            float row = s.Horizon + 3.0f;
            for (int line = 0; row < bottom - 1.0f; ++line)
            {
                const float depth = std::min(DepthOf(s, row), 1.3f);
                const bool calm = depth > 0.24f && depth < 0.8f;
                const int step = depth < 0.28f ? 7 : 5 + static_cast<int>(std::round(depth * 6.0f));
                const ImVec4 ground = GroundAt(s, row);
                const ImVec4 dark = Mix(OnMeadow(s, colors.BladeDark, row), ground, calm ? 0.4f : 0.0f);
                const ImVec4 mid = Mix(OnMeadow(s, colors.BladeMid, row), ground, calm ? 0.3f : 0.0f);
                const ImVec4 light = Mix(OnMeadow(s, colors.BladeLight, row), ground, calm ? 0.45f : 0.0f);
                const ImVec4 seed = OnMeadow(s, colors.Seed, row);
                for (int index = 0; index * step < s.Columns; ++index)
                {
                    const std::uint32_t mixed = Hash(index * 131 + line * 7919, 521u);
                    if (mixed % 8u < (calm ? 5u : 3u))
                        continue;
                    const float x = F(index * step + static_cast<int>((mixed >> 8) % static_cast<std::uint32_t>(step)));
                    const float y = row + F(static_cast<int>((mixed >> 16) % 2u));
                    const std::uint32_t kind = (mixed >> 20) % 12u;
                    if (autumn && kind < 4u)
                    {
                        const ImVec4 leaf = Mix(OnMeadow(s, kFallen[(mixed >> 24) % std::size(kFallen)], y), ground, calm ? 0.35f : 0.1f);
                        Box(paint, x, y, x + (depth < 0.35f ? 1.0f : 2.0f), y + 1.0f, leaf);
                        continue;
                    }
                    const bool bright = kind % 3u == 0u;
                    if (depth < 0.28f)
                    {
                        Box(paint, x, y, x + 2.0f, y + 1.0f, bright ? light : dark);
                    }
                    else if (depth < 0.62f)
                    {
                        Box(paint, x, y - 1.0f, x + 1.0f, y + 1.0f, dark);
                        if (bright)
                            Box(paint, x + 1.0f, y, x + 2.0f, y + 1.0f, light);
                    }
                    else
                    {
                        const float tall = 2.0f + F(static_cast<int>((mixed >> 22) % 2u)) + (depth > 1.0f ? 1.0f : 0.0f);
                        Box(paint, x, y - tall, x + 1.0f, y + 1.0f, dark);
                        Box(paint, x + 1.0f, y - tall + 1.0f, x + 2.0f, y + 1.0f, bright ? light : mid);
                        if ((s.Season == kSummer || autumn) && kind == 11u && !calm)
                            Box(paint, x, y - tall - 2.0f, x + 1.0f, y - tall, seed);
                    }
                }
                row += F(depth < 0.28f ? 3 : 2 + static_cast<int>(std::round(depth * 3.0f)));
            }
        }

        void PaintFlowers(const Setting& s)
        {
            constexpr float kDrifts[] = { 3.6f, 3.0f, 1.2f, 0.0f };
            constexpr float kSingles[] = { 22.0f, 18.0f, 7.0f, 0.0f };
            if (kDrifts[s.Season] <= 0.0f)
                return;
            const float span = s.Ground - s.Horizon;
            const auto placeOf = [&](float roll)
                {
                    if (roll < 0.4f)
                        return 0.04f + roll * 0.65f;
                    return 0.7f + (roll - 0.4f) * 0.9f;
                };
            const int drifts = std::max(3, static_cast<int>(s.Width * kDrifts[s.Season] / 100.0f));
            for (int index = 0; index < drifts; ++index)
            {
                const float share = placeOf(Roll(index, 531u));
                const float cy = s.Horizon + 3.0f + share * (span - 3.0f);
                const float cx = Roll(index, 532u) * s.Width;
                const auto kind = static_cast<int>(Hash(index, 533u) % 4u);
                const int count = 4 + static_cast<int>(Hash(index, 534u) % 7u);
                const float rx = (5.0f + 8.0f * Roll(index, 535u)) * (0.5f + share);
                const float ry = (1.0f + 2.5f * share) * std::min(1.4f, 0.5f + s.Size * 0.5f);
                for (int each = 0; each < count; ++each)
                {
                    const float angle = Roll(index * 31 + each, 536u) * 6.2832f;
                    const float reach = std::sqrt(Roll(index * 31 + each, 537u));
                    Flower(s, std::round(cx + std::cos(angle) * rx * reach), std::round(cy + std::sin(angle) * ry * reach), kind);
                }
            }
            const int singles = static_cast<int>(s.Width * (F(s.Rows) - s.Horizon) * kSingles[s.Season] / 10000.0f);
            for (int index = 0; index < singles; ++index)
            {
                const float share = placeOf(Roll(index, 541u));
                const float y = std::round(s.Horizon + 3.0f + share * (span - 1.0f));
                Flower(s, std::round(Roll(index, 542u) * s.Width), y, static_cast<int>(Hash(index, 543u) % 4u));
            }
        }

        void PaintSnow(const Setting& s)
        {
            const Palette& colors = *s.Colors;
            const Canvas& paint = s.Paint;
            const float body = s.Horizon + 3.0f;
            const float bottom = F(s.Rows);
            const float away = SideOfLight(s, s.Width * 0.5f) < 0.0f ? 1.0f : -1.0f;
            const auto snow = [&](ImVec4 day, float y) { return OnMeadow(s, day, y); };
            const auto part = [&](float left, float length, float from, float to, float row, ImVec4 day)
                {
                    const float a = away > 0.0f ? from : 1.0f - to;
                    const float b = away > 0.0f ? to : 1.0f - from;
                    Box(paint, std::round(left + length * a), row, std::round(left + length * b), row + 1.0f, snow(day, row));
                };
            constexpr int kSpacing = 47;
            for (int place = 0; place * kSpacing < s.Columns; ++place)
            {
                const std::uint32_t mixed = Hash(place, 551u);
                float depth = 0.1f + F(static_cast<int>((mixed >> 4) % 100u)) * 0.011f;
                if (depth > 0.35f && depth < 0.75f)
                    depth = depth < 0.55f ? depth - 0.25f : depth + 0.2f;
                const float row = std::round(body + (s.Ground + 4.0f - body) * depth);
                const float scale = 0.5f + 0.7f * std::min(1.2f, DepthOf(s, row));
                const float length = std::round((16.0f + F(static_cast<int>((mixed >> 12) % 14u))) * scale * std::min(1.3f, 0.6f + 0.4f * s.Size));
                const float left = F(place * kSpacing + static_cast<int>(mixed % 20u));
                if ((mixed >> 20) % 3u != 0u)
                {
                    part(left, length, 0.3f, 0.7f, row, colors.MeadowRim);
                    part(left, length, 0.08f, 0.55f, row + 1.0f, colors.MeadowRim);
                    part(left, length, 0.55f, 0.92f, row + 1.0f, colors.PatchDark);
                    part(left, length, 0.6f, 1.0f, row + 2.0f, colors.PatchDark);
                }
                else
                {
                    part(left, length, 0.15f, 0.85f, row, colors.PatchDark);
                    part(left, length, 0.0f, 1.0f, row + 1.0f, colors.PatchDark);
                    part(left, length, 0.08f, 0.5f, row + 1.0f, colors.PatchLight);
                    part(left, length, 0.45f, 0.95f, row + 2.0f, colors.MeadowRim);
                }
            }
            const int land = static_cast<int>(bottom - body);
            Scatter(s.Columns - 8, land, 19, 6, 43u, 80u, [&](float x, float y, std::uint32_t mixed)
                {
                    const float row = body + y;
                    const float depth = DepthOf(s, row);
                    if (depth > 0.3f && depth < 0.8f && mixed % 3u != 0u)
                        return;
                    const float length = std::max(2.0f, std::round((3.0f + F(static_cast<int>(mixed % 5u))) * (0.4f + 0.7f * std::min(1.2f, depth))));
                    Box(paint, x + 4.0f, row, x + 4.0f + length, row + 1.0f, snow(colors.MeadowRim, row));
                    Box(paint, x + 4.0f + away, row + 1.0f, x + 4.0f + away + length, row + 2.0f, snow(colors.PatchDark, row));
                });
            Scatter(s.Columns - 4, land, 11, 5, 47u, 70u, [&](float x, float y, std::uint32_t mixed)
                {
                    const float row = body + y;
                    const float depth = DepthOf(s, row);
                    if (depth > 0.28f && depth < 0.82f && mixed % 4u != 0u)
                        return;
                    const ImVec4 stalk = snow((mixed >> 9) % 2u == 0u ? colors.BladeMid : colors.BladeDark, row);
                    if (depth < 0.4f)
                    {
                        Box(paint, x + 2.0f, row - 1.0f, x + 3.0f, row + 1.0f, stalk);
                        return;
                    }
                    const float tall = 2.0f + F(static_cast<int>((mixed >> 10) % 3u)) + (depth > 1.0f ? 1.0f : 0.0f);
                    Box(paint, x + 2.0f, row - tall, x + 3.0f, row + 1.0f, stalk);
                    Box(paint, x + 3.0f, row - tall + 2.0f, x + 4.0f, row + 1.0f, snow(colors.BladeDark, row));
                    Box(paint, x + 1.0f, row + 1.0f, x + 5.0f, row + 2.0f, snow(colors.MeadowRim, row));
                });
            const float from = std::round(s.Width * 0.2f);
            const float to = std::round(s.Width * 0.08f);
            int print = 0;
            for (float row = body + 2.0f; row < bottom - 2.0f; ++print)
            {
                const float share = (row - body) / std::max(1.0f, bottom - body);
                const float scale = 0.5f + 0.7f * std::min(1.2f, DepthOf(s, row));
                const float x = std::round(from + (to - from) * share + 3.0f * std::sin(share * 5.0f));
                const float size = std::max(1.0f, std::round(1.4f * scale));
                const float side = std::round((print % 2 == 0 ? -1.0f : 1.0f) * scale);
                Box(paint, x + side, row, x + side + size, row + 1.0f, snow(colors.PatchLight, row));
                Box(paint, x + side + (away > 0.0f ? size : -1.0f), row, x + side + (away > 0.0f ? size + 1.0f : 0.0f), row + 1.0f, snow(colors.MeadowRim, row));
                row += std::max(2.0f, std::round(3.0f * scale));
            }
            const ImVec4 glint = s.Light(colors.Seed);
            Scatter(s.Columns - 4, land, 13, 5, 59u, 70u, [&](float x, float y, std::uint32_t mixed)
                {
                    const double phase = static_cast<double>(mixed & 1023u) * 0.37;
                    if (std::sin(s.Time * 1.3 + phase) < 0.75)
                        return;
                    Box(paint, x + 2.0f, body + y, x + 3.0f, body + y + 1.0f, glint);
                });
        }

        void PaintSides(const Setting& s)
        {
            const Look& look = s.Light;
            std::array<ImVec4, std::size(kTones)> tones{};
            for (std::size_t index = 1; index < tones.size(); ++index)
                tones[index] = look(kTones[index]);
            const float span = s.Ground - s.Horizon;
            const float k = std::clamp(F(s.Rows) * 0.0068f, 0.32f, 1.5f);
            struct Place
            {
                float X;
                float Depth;
                bool Tree;
            };
            const Place places[] = { { std::round(s.Width * 0.05f), 0.32f, true }, { std::round(s.Width * 0.95f), 0.24f, true },
                { std::round(s.Width * 0.05f + 21.0f * k), 0.44f, false }, { std::round(s.Width * 0.95f - 23.0f * k), 0.36f, false } };
            int index = 0;
            for (const Place& place : places)
            {
                const bool left = index % 2 == 0;
                const float foot = std::round(s.Horizon + span * place.Depth);
                Canvas paint = s.Paint;
                paint.Haze = HazeAt(s, foot);
                paint.HazeColor = look.Haze;
                const Toward light = TowardLight(s, place.X);
                const auto seed = static_cast<std::uint32_t>(index) * 97u + 11u + static_cast<std::uint32_t>(s.Season) * 13u;
                Sprite sprite;
                if (place.Tree)
                {
                    switch (s.Season)
                    {
                    case kSpring:
                        if (left)
                        {
                            RoundTree(sprite, k, light, kPink, seed, 1.0f);
                            sprite.Sprinkle(kPink, kFresh + 1, 44u, seed + 7u);
                            sprite.Sprinkle(kPink + 1, kFresh + 2, 10u, seed + 8u);
                        }
                        else
                        {
                            Birch(sprite, k, light, seed);
                        }
                        break;
                    case kSummer:
                        RoundTree(sprite, k * (left ? 1.0f : 0.92f), light, kDeep, seed, left ? 1.12f : 0.9f);
                        break;
                    case kAutumn:
                        if (left)
                        {
                            RoundTree(sprite, k, light, kCrimson, seed, 1.0f);
                            sprite.Sprinkle(kCrimson + 1, kOlive + 1, 40u, seed + 7u);
                            sprite.Sprinkle(kCrimson, 0, 60u, seed + 8u);
                        }
                        else
                        {
                            Spruce(sprite, k, light, seed, false);
                        }
                        break;
                    default:
                        if (left)
                            Spruce(sprite, k, light, seed, true);
                        else
                            BareTree(sprite, k, light, seed);
                        break;
                    }
                }
                else
                {
                    const float small = k * 0.85f;
                    switch (s.Season)
                    {
                    case kSpring:
                        Bush(sprite, small, light, kFresh, seed);
                        if (left)
                        {
                            sprite.Sprinkle(kFresh + 3, kGold + 1, 80u, seed + 4u);
                            sprite.Sprinkle(kFresh + 2, kGold, 45u, seed + 5u);
                        }
                        else
                        {
                            sprite.Sprinkle(kFresh + 3, kLilac + 2, 110u, seed + 4u);
                            sprite.Sprinkle(kFresh + 2, kLilac + 1, 70u, seed + 5u);
                        }
                        break;
                    case kSummer:
                        Bush(sprite, small, light, kDeep, seed);
                        sprite.Sprinkle(kDeep + 3, left ? kRose : kWhite, 90u, seed + 4u);
                        sprite.Sprinkle(kDeep + 2, left ? kRose : kWhite, 45u, seed + 5u);
                        break;
                    case kAutumn:
                        Bush(sprite, small, light, kOlive, seed);
                        if (left)
                        {
                            sprite.Sprinkle(kOlive + 2, kBerry, 50u, seed + 4u);
                        }
                        else
                        {
                            sprite.Sprinkle(kOlive + 3, kCrimson + 3, 120u, seed + 4u);
                            sprite.Sprinkle(kOlive + 2, kCrimson + 2, 90u, seed + 5u);
                        }
                        break;
                    default:
                        Bush(sprite, small, light, kNeedle, seed);
                        sprite.Sprinkle(kNeedle + 2, kBerry, 40u, seed + 4u);
                        sprite.Snow(kSnowy + 3, kSnowy + 2, light.X, seed + 5u);
                        break;
                    }
                }
                {
                    const float away = -SideOfLight(s, place.X);
                    const float half = std::round((place.Tree ? 13.0f : 12.0f) * k) + 1.0f;
                    const float reach = std::round(half * (0.3f + 0.6f * (1.0f - s.Elevation)));
                    const ImVec4 shadow = s.Season == kWinter ? Colour(0.10f, 0.16f, 0.36f, 0.16f + 0.06f * look.Daylight)
                                                              : Colour(0.02f, 0.08f, 0.02f, 0.12f + 0.10f * look.Daylight);
                    const float middle = place.X + away * reach * 0.5f;
                    Box(s.Paint, middle - half - reach * 0.5f, foot - 1.0f, middle + half + reach * 0.5f, foot + 1.0f, shadow);
                    Box(s.Paint, middle - half * 0.7f, foot + 1.0f, middle + half * 0.7f, foot + 2.0f, shadow);
                }
                sprite.Flush(paint, place.X, foot, tones.data());

                if (s.Season != kWinter)
                {
                    const ImVec4 blade = OnMeadow(s, s.Colors->BladeDark, foot);
                    const ImVec4 bright = OnMeadow(s, s.Colors->BladeMid, foot);
                    const auto reach = static_cast<int>(std::round((place.Tree ? 6.0f : 10.0f) * k)) + 2;
                    for (int cell = -reach; cell <= reach; cell += 2)
                    {
                        const std::uint32_t mixed = Hash(cell + index * 64, 571u);
                        if (mixed % 3u == 0u)
                            continue;
                        const float x = place.X + F(cell);
                        const float tall = 1.0f + F(static_cast<int>((mixed >> 4) % 2u));
                        Box(s.Paint, x, foot - tall, x + 1.0f, foot + 1.0f, (mixed >> 6) % 2u == 0u ? blade : bright);
                    }
                }
                else
                {
                    const ImVec4 lit = OnMeadow(s, s.Colors->MeadowRim, foot);
                    const float half = std::round((place.Tree ? 7.0f : 12.0f) * k) + 2.0f;
                    Box(s.Paint, place.X - half, foot - 1.0f, place.X + half, foot + 1.0f, lit);
                    Box(s.Paint, place.X - half + 2.0f, foot - 2.0f, place.X + half - 2.0f, foot - 1.0f, lit);
                }
                ++index;
            }

            if (s.Season == kAutumn)
            {
                const float x = places[0].X;
                const float foot = std::round(s.Horizon + span * places[0].Depth);
                const float rx = 22.0f * k;
                const float ry = std::max(2.0f, rx * 0.2f);
                for (int leaf = 0; leaf < 48; ++leaf)
                {
                    const std::uint32_t mixed = Hash(leaf, 581u);
                    const float angle = Roll(leaf, 582u) * 6.2832f;
                    const float reach = std::pow(Roll(leaf, 583u), 0.8f);
                    const float lx = std::round(x + std::cos(angle) * reach * rx);
                    const float ly = std::round(foot + std::sin(angle) * reach * ry);
                    Box(s.Paint, lx, ly, lx + (mixed % 2u == 0u ? 1.0f : 2.0f), ly + 1.0f, OnMeadow(s, kTones[kCrimson + 1 + static_cast<int>((mixed >> 4) % 3u)], ly));
                }
                const float mx = x + std::round(13.0f * k) + 3.0f;
                const float my = foot + std::round(3.0f * k) + 2.0f;
                for (int shroom = 0; shroom < 3; ++shroom)
                {
                    const float sx = mx + F(shroom * 4) - F(shroom == 2 ? 1 : 0);
                    const float sy = my + F(shroom % 2);
                    const float tall = shroom == 1 ? 1.0f : 2.0f;
                    Box(s.Paint, sx + 1.0f, sy - tall, sx + 2.0f, sy, OnMeadow(s, kTones[kCap + 2], sy));
                    Box(s.Paint, sx, sy - tall - 2.0f, sx + 3.0f, sy - tall, OnMeadow(s, kTones[kCap], sy));
                    Box(s.Paint, sx + 1.0f, sy - tall - 2.0f, sx + 2.0f, sy - tall - 1.0f, OnMeadow(s, kTones[kCap + 1], sy));
                }
            }
        }

        void PaintCloudShadows(const Setting& s)
        {
            const float day = std::clamp((s.Light.Daylight - 0.55f) * 3.0f, 0.0f, 1.0f);
            if (day <= 0.0f)
                return;
            const ImVec4 shade = s.Season == kWinter ? Colour(0.10f, 0.16f, 0.40f, 0.075f * day) : Colour(0.0f, 0.05f, 0.10f, 0.08f * day);
            const float span = s.Ground - s.Horizon;
            const double travelled = s.Time * 0.55;
            constexpr double kSpacing = 190.0;
            const auto first = static_cast<long long>(std::floor((-160.0 - travelled) / kSpacing));
            const auto last = static_cast<long long>(std::floor((static_cast<double>(s.Columns) - travelled) / kSpacing));
            for (long long slot = first; slot <= last; ++slot)
            {
                const int index = static_cast<int>(slot);
                const std::uint32_t mixed = Hash(index, 91u);
                if (mixed % 3u == 0u)
                    continue;
                const float depth = 0.2f + 0.75f * Roll(index, 92u);
                const float cy = std::round(s.Horizon + 3.0f + depth * (span - 3.0f));
                const float scale = 0.5f + depth;
                const float rx = (26.0f + F(static_cast<int>((mixed >> 8) % 24u))) * scale;
                const float ry = std::max(1.0f, std::round((2.0f + F(static_cast<int>((mixed >> 16) % 3u))) * scale * std::min(1.4f, 0.5f + 0.5f * s.Size)));
                const float cx = std::round(static_cast<float>(static_cast<double>(slot) * kSpacing + travelled) + F(static_cast<int>((mixed >> 20) % 70u)));
                for (float row = -ry; row <= ry; row += 1.0f)
                {
                    const float part = row / (ry + 0.5f);
                    const float half = std::round(rx * std::sqrt(std::max(0.0f, 1.0f - part * part)));
                    const float shift = F(static_cast<int>(Hash(index * 17 + static_cast<int>(row), 93u) % 5u)) - 2.0f;
                    Box(s.Paint, cx - half + shift, cy + row, cx + half + shift, cy + row + 1.0f, shade);
                }
            }
        }

        void PaintLife(const Setting& s)
        {
            const Look& look = s.Light;
            const double time = s.Time;
            const float span = s.Ground - s.Horizon;
            if (s.Season == kWinter)
                return;
            if (look.Daylight > 0.45f)
            {
                constexpr float kAt[] = { 0.17f, 0.83f, 0.3f, 0.68f };
                constexpr ImVec4 kWings[] = { Colour(0.970f, 0.970f, 0.940f), Colour(0.980f, 0.930f, 0.560f), Colour(0.560f, 0.700f, 0.980f), Colour(0.960f, 0.640f, 0.780f) };
                const int count = s.Season == kAutumn ? 1 : s.Game ? 2 : 3;
                const float alpha = std::clamp((look.Daylight - 0.45f) * 4.0f, 0.0f, 1.0f);
                for (int index = 0; index < count; ++index)
                {
                    const double phase = static_cast<double>(index) * 2.4 + 0.7;
                    const float x = std::round(s.Width * kAt[index] + static_cast<float>(std::sin(time * 0.31 + phase) * 10.0 + std::sin(time * 1.3 + phase * 2.0) * 3.0));
                    const float y = std::round(s.Horizon + span * (0.5f + 0.12f * F(index % 2)) + static_cast<float>(std::sin(time * 0.73 + phase) * 3.0 - std::fabs(std::sin(time * 2.1 + phase)) * 2.0));
                    const bool open = time == 0.0 || std::fmod(time * 5.0 + phase, 1.0) < 0.5;
                    const ImVec4 wing = Faded(look(kWings[(index + s.Season) % 4]), alpha);
                    Box(s.Paint, x, y, x + 1.0f, y + 1.0f, Faded(look(Colour(0.200f, 0.160f, 0.140f)), alpha));
                    if (open)
                    {
                        Box(s.Paint, x - 1.0f, y - 1.0f, x, y + 1.0f, wing);
                        Box(s.Paint, x + 1.0f, y - 1.0f, x + 2.0f, y + 1.0f, wing);
                    }
                    else
                    {
                        Box(s.Paint, x, y - 1.0f, x + 1.0f, y, wing);
                    }
                }
            }
            if (look.Daylight < 0.45f)
            {
                constexpr std::uint32_t kShare[] = { 26u, 44u, 14u, 0u };
                const float glow = std::clamp((0.45f - look.Daylight) * 4.0f, 0.0f, 1.0f);
                const float top = s.Horizon + span * 0.3f;
                const ImVec4 core = Colour(0.900f, 0.980f, 0.450f);
                Scatter(s.Columns - 4, static_cast<int>(F(s.Rows) - top - 2.0f), 23, 9, 67u, kShare[s.Season] * (s.Game ? 2u : 3u), [&](float x, float y, std::uint32_t mixed)
                    {
                        const double phase = static_cast<double>(mixed & 255u) * 0.1;
                        const float on = time == 0.0 ? ((mixed >> 9) % 3u != 0u ? 1.0f : 0.0f) : static_cast<float>(std::max(0.0, std::sin(time * 1.1 + phase)));
                        if (on <= 0.05f)
                            return;
                        const float wander = std::round(static_cast<float>(std::sin(time * 0.4 + phase) * 2.0));
                        const float fx = x + 2.0f + wander;
                        const float fy = top + y + std::round(static_cast<float>(std::sin(time * 0.6 + phase * 1.7) * 1.5));
                        Box(s.Paint, fx - 1.0f, fy - 1.0f, fx + 2.0f, fy + 2.0f, Faded(core, 0.16f * on * glow));
                        Box(s.Paint, fx, fy, fx + 1.0f, fy + 1.0f, Faded(core, on * glow));
                    });
            }
        }
    }

    void DrawMeadowBack(const Frame& frame)
    {
        if (frame.Columns <= 0 || frame.Rows <= 0)
            return;
        const Setting s = SetUp(frame);
        const float farmX = std::round(s.Width * 0.15f);
        const float millX = std::round(s.Width * 0.8f);
        PaintSky(s);
        PaintClouds(s, true);
        PaintLights(s);
        PaintClouds(s, false);
        PaintRange(s);
        PaintHills(s, farmX, millX);
        PaintTreeLine(s, farmX, millX);
        PaintLand(s);
        if (s.Season == kWinter)
        {
            PaintSnow(s);
        }
        else
        {
            PaintGrass(s);
            PaintFlowers(s);
        }
        PaintSides(s);
        PaintCloudShadows(s);
        PaintLife(s);
    }

    void DrawMeadowFront(const Frame& frame)
    {
        if (frame.Columns <= 0 || frame.Rows <= 0)
            return;
        const Setting s = SetUp(frame);
        const Palette& colors = *s.Colors;
        const Look& look = s.Light;
        const Canvas& paint = s.Paint;
        const float bottom = F(s.Rows);
        const double time = s.Time;
        const bool winter = s.Season == kWinter;
        const ImVec4 dark = look(colors.BladeDark);
        const ImVec4 mid = look(colors.BladeMid);
        const ImVec4 light = look(colors.BladeLight);
        const ImVec4 snowLit = look(colors.MeadowRim);
        const ImVec4 snowShade = look(colors.PatchDark);
        for (int column = 1; column < s.Columns - 1; ++column)
        {
            const std::uint32_t mixed = Hash(column, 701u);
            const float across = std::fabs(F(column) / std::max(1.0f, s.Width) - 0.5f) * 2.0f;
            const bool side = across > (s.Game ? 0.66f : 0.5f);
            if (mixed % (side ? 4u : s.Game ? 13u : 7u) != 0u)
                continue;
            const float x = F(column);
            const int most = side ? 4 + static_cast<int>(std::round(std::min(2.0f, s.Size))) : 2;
            const int tall = std::max(1, most - static_cast<int>((mixed >> 8) % 3u));
            if (winter)
            {
                const float half = 1.0f + F(static_cast<int>((mixed >> 12) % 3u));
                Box(paint, x - half, bottom - 2.0f, x + half + 1.0f, bottom, snowShade);
                Box(paint, x - half + 1.0f, bottom - 3.0f, x + half, bottom - 1.0f, snowLit);
                if ((mixed >> 14) % 2u == 0u)
                    Box(paint, x, bottom - 3.0f - F(tall), x + 1.0f, bottom - 3.0f, look(colors.BladeMid));
                continue;
            }
            const std::uint32_t kind = (mixed >> 16) % 10u;
            const float sway = side && tall >= 4 ? std::round(static_cast<float>(std::sin(time * 1.4 + static_cast<double>((mixed >> 20) & 255u) * 0.1)) * 0.7f) : 0.0f;
            if (side && kind >= 8u && s.Season != kAutumn)
            {
                const auto flower = static_cast<int>((mixed >> 24) % 4u);
                const float top = bottom - F(tall) - 2.0f;
                Box(paint, x, top + 2.0f, x + 1.0f, bottom, dark);
                Box(paint, x + 1.0f, bottom - 2.0f, x + 2.0f, bottom, mid);
                Box(paint, x - 1.0f + sway, top + 1.0f, x + 2.0f + sway, top + 2.0f, look(colors.Petals[flower]));
                Box(paint, x + sway, top, x + 1.0f + sway, top + 3.0f, look(colors.Petals[flower]));
                Box(paint, x + sway, top + 1.0f, x + 1.0f + sway, top + 2.0f, look(colors.Hearts[flower]));
                continue;
            }
            const int blades = 2 + static_cast<int>((mixed >> 12) % 2u);
            for (int blade = 0; blade < blades; ++blade)
            {
                const float bx = x + F(blade);
                const float height = F(std::max(1, tall - (blade == 1 ? 0 : 1)));
                const float lean = blade == 0 ? -1.0f : blade == blades - 1 ? 1.0f : 0.0f;
                const ImVec4 color = blade == 1 ? light : blade == 0 ? mid : dark;
                Box(paint, bx, bottom - height + 1.0f, bx + 1.0f, bottom, color);
                Box(paint, bx + (height >= 3.0f ? lean + sway : 0.0f), bottom - height, bx + (height >= 3.0f ? lean + sway : 0.0f) + 1.0f, bottom - height + 1.0f, color);
            }
            if (s.Season == kAutumn && side && kind >= 7u)
            {
                Box(paint, x + 1.0f, bottom - F(tall) - 2.0f, x + 2.0f, bottom - F(tall), look(colors.Seed));
                Box(paint, x + 3.0f, bottom - 1.0f, x + 5.0f, bottom, look(Colour(0.700f, 0.200f, 0.100f)));
            }
        }
        if (!winter && look.Daylight < 0.45f)
        {
            const float glow = std::clamp((0.45f - look.Daylight) * 4.0f, 0.0f, 1.0f);
            const ImVec4 core = Colour(0.900f, 0.980f, 0.450f);
            for (int index = 0; index < 2; ++index)
            {
                const double phase = static_cast<double>(index) * 2.1 + 0.4;
                const float on = time == 0.0 ? 1.0f : static_cast<float>(std::max(0.0, std::sin(time * 0.9 + phase)));
                if (on <= 0.05f)
                    continue;
                const float x = std::round(s.Width * (index == 0 ? 0.06f : 0.93f) + static_cast<float>(std::sin(time * 0.35 + phase) * 4.0));
                const float y = std::round(bottom - 6.0f - 2.0f * s.Size + static_cast<float>(std::sin(time * 0.5 + phase) * 2.0));
                Box(paint, x - 1.0f, y - 1.0f, x + 2.0f, y + 2.0f, Faded(core, 0.18f * on * glow));
                Box(paint, x, y, x + 1.0f, y + 1.0f, Faded(core, on * glow));
            }
        }
    }
}
