#include "UI/HomeScene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace Idler::UI::HomeScene
{
    namespace
    {
        constexpr ImVec4 Colour(float red, float green, float blue)
        {
            return ImVec4(red, green, blue, 1.0f);
        }

        constexpr ImVec4 WithAlpha(ImVec4 colour, float alpha)
        {
            return ImVec4(colour.x, colour.y, colour.z, alpha);
        }

        std::uint32_t Hash(int x, int y, std::uint32_t salt)
        {
            return Noise((static_cast<std::uint32_t>(x) * 73856093u) ^ (static_cast<std::uint32_t>(y) * 19349663u) ^ (salt * 83492791u));
        }

        float Unit(std::uint32_t value)
        {
            return static_cast<float>(value & 0xFFFFu) / 65535.0f;
        }

        constexpr ImVec4 kSkyDayTop = Colour(0.300f, 0.450f, 0.630f);
        constexpr ImVec4 kSkyDayMiddle = Colour(0.520f, 0.620f, 0.710f);
        constexpr ImVec4 kSkyDayLow = Colour(0.880f, 0.780f, 0.590f);
        constexpr ImVec4 kSkyNightTop = Colour(0.030f, 0.040f, 0.080f);
        constexpr ImVec4 kSkyNightLow = Colour(0.110f, 0.140f, 0.215f);
        constexpr ImVec4 kSkyDawnTop = Colour(0.200f, 0.220f, 0.400f);
        constexpr ImVec4 kSkyDawnLow = Colour(0.960f, 0.580f, 0.400f);
        constexpr ImVec4 kHazeDay = Colour(0.840f, 0.740f, 0.580f);
        constexpr ImVec4 kHazeNight = Colour(0.105f, 0.125f, 0.185f);
        constexpr ImVec4 kHazeDawn = Colour(0.820f, 0.560f, 0.480f);
        constexpr ImVec4 kNightAir = Colour(0.050f, 0.070f, 0.125f);
        constexpr ImVec4 kMoonRim = Colour(0.380f, 0.440f, 0.560f);
        constexpr ImVec4 kGoldenRim = Colour(1.000f, 0.620f, 0.320f);
        constexpr ImVec4 kDuskShade = Colour(0.200f, 0.120f, 0.280f);

        float GlowOf(const Frame& frame)
        {
            return std::clamp(1.0f - std::fabs(frame.Daylight - 0.5f) * 2.6f, 0.0f, 1.0f);
        }

        ImVec4 NightOf(ImVec4 day)
        {
            const float grey = day.x * 0.3f + day.y * 0.59f + day.z * 0.11f;
            const ImVec4 muted = Mix(day, ImVec4(grey, grey, grey, 1.0f), 0.45f);
            return Mix(ImVec4(muted.x * 0.30f, muted.y * 0.30f, muted.z * 0.30f, 1.0f), kNightAir, 0.35f);
        }

        ImVec4 Lighted(ImVec4 day, const Frame& frame)
        {
            return DayNight(day, NightOf(day), frame.Daylight);
        }

        ImVec4 SkyAt(const Frame& frame, float down)
        {
            const ImVec4 day = down < 0.5f ? Mix(kSkyDayTop, kSkyDayMiddle, down * 2.0f) : Mix(kSkyDayMiddle, kSkyDayLow, (down - 0.5f) * 2.0f);
            const ImVec4 night = Mix(kSkyNightTop, kSkyNightLow, down);
            return Mix(DayNight(day, night, frame.Daylight), Mix(kSkyDawnTop, kSkyDawnLow, down), GlowOf(frame) * (0.2f + 0.65f * down));
        }

        ImVec4 HazeOf(const Frame& frame)
        {
            return Mix(DayNight(kHazeDay, kHazeNight, frame.Daylight), kHazeDawn, 0.55f * GlowOf(frame));
        }

        float SkyHigh(const Frame& frame)
        {
            return std::round(std::max(16.0f, frame.Surface * 0.2f));
        }

        float SunY(const Frame& frame)
        {
            const float below = frame.Surface + 15.0f;
            return std::round(below + (SkyHigh(frame) - below) * frame.Daylight);
        }

        float MoonY(const Frame& frame)
        {
            const float below = frame.Surface + 15.0f;
            return std::round(SkyHigh(frame) + (below - SkyHigh(frame)) * frame.Daylight);
        }

        enum class Stuff : std::uint8_t
        {
            Blank,
            Bark,
            Scarlet,
            Ember,
            Gold,
            Russet,
            Lemon,
            BirchBark,
            Crimson,
            Cap,
            Stem,
            Pumpkin,
            Vine,
            Wood,
            Moss,
            Litter,
            Count
        };

        constexpr ImVec4 kRamps[static_cast<int>(Stuff::Count)][5] = {
            { Colour(0.0f, 0.0f, 0.0f), Colour(0.0f, 0.0f, 0.0f), Colour(0.0f, 0.0f, 0.0f), Colour(0.0f, 0.0f, 0.0f), Colour(0.0f, 0.0f, 0.0f) },
            { Colour(0.130f, 0.085f, 0.070f), Colour(0.220f, 0.145f, 0.100f), Colour(0.320f, 0.215f, 0.140f), Colour(0.440f, 0.310f, 0.200f), Colour(0.560f, 0.420f, 0.280f) },
            { Colour(0.330f, 0.060f, 0.060f), Colour(0.530f, 0.110f, 0.080f), Colour(0.730f, 0.190f, 0.100f), Colour(0.880f, 0.330f, 0.130f), Colour(0.980f, 0.540f, 0.220f) },
            { Colour(0.450f, 0.150f, 0.050f), Colour(0.670f, 0.260f, 0.060f), Colour(0.850f, 0.410f, 0.090f), Colour(0.950f, 0.570f, 0.150f), Colour(1.000f, 0.750f, 0.320f) },
            { Colour(0.390f, 0.240f, 0.060f), Colour(0.570f, 0.370f, 0.080f), Colour(0.750f, 0.540f, 0.130f), Colour(0.890f, 0.700f, 0.220f), Colour(0.990f, 0.860f, 0.420f) },
            { Colour(0.260f, 0.120f, 0.050f), Colour(0.410f, 0.190f, 0.070f), Colour(0.570f, 0.290f, 0.100f), Colour(0.710f, 0.400f, 0.150f), Colour(0.850f, 0.560f, 0.250f) },
            { Colour(0.470f, 0.380f, 0.060f), Colour(0.670f, 0.560f, 0.090f), Colour(0.840f, 0.730f, 0.150f), Colour(0.950f, 0.860f, 0.300f), Colour(1.000f, 0.960f, 0.580f) },
            { Colour(0.140f, 0.130f, 0.130f), Colour(0.500f, 0.480f, 0.460f), Colour(0.690f, 0.670f, 0.640f), Colour(0.840f, 0.820f, 0.780f), Colour(0.960f, 0.940f, 0.900f) },
            { Colour(0.250f, 0.040f, 0.070f), Colour(0.430f, 0.060f, 0.090f), Colour(0.610f, 0.100f, 0.110f), Colour(0.780f, 0.180f, 0.140f), Colour(0.920f, 0.340f, 0.220f) },
            { Colour(0.370f, 0.050f, 0.040f), Colour(0.570f, 0.090f, 0.060f), Colour(0.750f, 0.150f, 0.090f), Colour(0.880f, 0.260f, 0.150f), Colour(0.980f, 0.480f, 0.340f) },
            { Colour(0.450f, 0.410f, 0.360f), Colour(0.610f, 0.570f, 0.500f), Colour(0.760f, 0.720f, 0.640f), Colour(0.880f, 0.850f, 0.780f), Colour(0.980f, 0.960f, 0.900f) },
            { Colour(0.430f, 0.160f, 0.040f), Colour(0.640f, 0.260f, 0.050f), Colour(0.830f, 0.400f, 0.080f), Colour(0.950f, 0.540f, 0.130f), Colour(1.000f, 0.710f, 0.300f) },
            { Colour(0.120f, 0.150f, 0.060f), Colour(0.200f, 0.250f, 0.080f), Colour(0.300f, 0.360f, 0.110f), Colour(0.420f, 0.470f, 0.170f), Colour(0.560f, 0.580f, 0.250f) },
            { Colour(0.350f, 0.220f, 0.110f), Colour(0.490f, 0.340f, 0.180f), Colour(0.630f, 0.480f, 0.280f), Colour(0.760f, 0.620f, 0.400f), Colour(0.870f, 0.760f, 0.540f) },
            { Colour(0.120f, 0.170f, 0.070f), Colour(0.200f, 0.270f, 0.090f), Colour(0.300f, 0.380f, 0.120f), Colour(0.420f, 0.490f, 0.170f), Colour(0.540f, 0.600f, 0.240f) },
            { Colour(0.270f, 0.150f, 0.070f), Colour(0.410f, 0.230f, 0.090f), Colour(0.550f, 0.320f, 0.120f), Colour(0.680f, 0.430f, 0.170f), Colour(0.800f, 0.570f, 0.260f) },
        };

        constexpr std::uint8_t Ink(Stuff stuff, int tone)
        {
            return static_cast<std::uint8_t>(static_cast<int>(stuff) * 8 + tone);
        }

        constexpr int kInks = static_cast<int>(Stuff::Count) * 8;

        ImVec4 ToneAt(int stuff, int tone, const Frame& frame)
        {
            const ImVec4 day = kRamps[stuff][tone];
            ImVec4 night = NightOf(Mix(day, kRamps[stuff][2], 0.3f));
            if (tone >= 3)
                night = Mix(night, kMoonRim, 0.10f * static_cast<float>(tone - 2));
            ImVec4 colour = DayNight(day, night, frame.Daylight);
            const float glow = GlowOf(frame);
            if (!frame.Sun.Moon && tone >= 3)
                colour = Mix(colour, kGoldenRim, 0.2f * glow * static_cast<float>(tone - 2));
            else if (tone <= 1)
                colour = Mix(colour, kDuskShade, 0.18f * glow);
            return colour;
        }

        struct Block
        {
            std::int16_t Left;
            std::int16_t Top;
            std::int16_t Right;
            std::int16_t Bottom;
            std::uint8_t Ink;
        };

        class Sketch
        {
        public:
            Sketch(int left, int top, int right, int bottom)
                : m_left(left), m_top(top), m_width(right - left), m_height(bottom - top), m_cells(static_cast<std::size_t>((right - left) * (bottom - top)), std::uint8_t{ 0 })
            {
            }

            std::uint8_t At(int x, int y) const
            {
                return Inside(x, y) ? m_cells[Index(x, y)] : std::uint8_t{ 0 };
            }

            void Put(int x, int y, std::uint8_t ink)
            {
                if (Inside(x, y))
                    m_cells[Index(x, y)] = ink;
            }

            void Cut(int row)
            {
                for (int y = std::max(row, m_top); y < m_top + m_height; ++y)
                {
                    for (int x = m_left; x < m_left + m_width; ++x)
                        m_cells[Index(x, y)] = 0;
                }
            }

            std::vector<Block> Blocks() const
            {
                struct Open
                {
                    int Left;
                    int Right;
                    std::uint8_t Ink;
                    std::size_t Index;
                };
                std::vector<Block> blocks;
                std::vector<Open> above;
                std::vector<Open> here;
                std::vector<Span> strokes;
                std::vector<Span> spans;
                std::vector<std::uint8_t> inks;
                std::vector<int> fewest;
                std::vector<int> choice;
                for (int row = 0; row < m_height; ++row)
                {
                    const std::uint8_t* cells = &m_cells[static_cast<std::size_t>(row * m_width)];
                    strokes.clear();
                    int column = 0;
                    while (column < m_width)
                    {
                        if (cells[column] == 0)
                        {
                            ++column;
                            continue;
                        }
                        spans.clear();
                        inks.clear();
                        while (column < m_width && cells[column] != 0)
                        {
                            int end = column + 1;
                            while (end < m_width && cells[end] == cells[column])
                                ++end;
                            spans.push_back({ column, end, cells[column] });
                            inks.push_back(cells[column]);
                            column = end;
                        }
                        Layer(inks, fewest, choice);
                        const auto count = static_cast<int>(inks.size());
                        const std::size_t first = strokes.size();
                        Emit(inks, choice, count, 0, count - 1, 0, strokes);
                        for (std::size_t index = first; index < strokes.size(); ++index)
                        {
                            Span& stroke = strokes[index];
                            stroke = { spans[static_cast<std::size_t>(stroke.From)].From, spans[static_cast<std::size_t>(stroke.To)].To, stroke.Ink };
                        }
                    }
                    here.clear();
                    for (std::size_t index = 0; index < strokes.size(); ++index)
                    {
                        const Span& stroke = strokes[index];
                        std::size_t under = 0;
                        bool covers = false;
                        for (std::size_t before = 0; before < index; ++before)
                        {
                            if (strokes[before].To > stroke.From && strokes[before].From < stroke.To)
                            {
                                under = std::max(under, here[before].Index);
                                covers = true;
                            }
                        }
                        std::size_t at = blocks.size();
                        for (const Open& open : above)
                        {
                            if (open.Left == stroke.From && open.Right == stroke.To && open.Ink == stroke.Ink && (!covers || open.Index > under))
                                at = open.Index;
                        }
                        if (at < blocks.size())
                            ++blocks[at].Bottom;
                        else
                        {
                            blocks.push_back({ static_cast<std::int16_t>(stroke.From + m_left), static_cast<std::int16_t>(row + m_top), static_cast<std::int16_t>(stroke.To + m_left),
                                static_cast<std::int16_t>(row + m_top + 1), stroke.Ink });
                        }
                        here.push_back({ stroke.From, stroke.To, stroke.Ink, at });
                    }
                    std::swap(above, here);
                }
                return blocks;
            }

        private:
            struct Span
            {
                int From;
                int To;
                std::uint8_t Ink;
            };

            static void Layer(const std::vector<std::uint8_t>& inks, std::vector<int>& fewest, std::vector<int>& choice)
            {
                const auto count = static_cast<int>(inks.size());
                fewest.assign(static_cast<std::size_t>(count * count), 0);
                choice.assign(static_cast<std::size_t>(count * count), -1);
                const auto at = [count](int from, int to) { return static_cast<std::size_t>(from * count + to); };
                for (int length = 1; length <= count; ++length)
                {
                    for (int from = 0; from + length <= count; ++from)
                    {
                        const int to = from + length - 1;
                        int best = 1 + (from < to ? fewest[at(from + 1, to)] : 0);
                        int pick = -1;
                        for (int shared = from + 1; shared <= to; ++shared)
                        {
                            if (inks[static_cast<std::size_t>(shared)] != inks[static_cast<std::size_t>(from)])
                                continue;
                            const int cost = (shared - 1 >= from + 1 ? fewest[at(from + 1, shared - 1)] : 0) + fewest[at(shared, to)];
                            if (cost < best)
                            {
                                best = cost;
                                pick = shared;
                            }
                        }
                        fewest[at(from, to)] = best;
                        choice[at(from, to)] = pick;
                    }
                }
            }

            static void Emit(const std::vector<std::uint8_t>& inks, const std::vector<int>& choice, int count, int from, int to, int reach, std::vector<Span>& strokes)
            {
                if (from > to)
                    return;
                const int shared = choice[static_cast<std::size_t>(from * count + to)];
                if (shared < 0)
                {
                    strokes.push_back({ reach, from, inks[static_cast<std::size_t>(from)] });
                    Emit(inks, choice, count, from + 1, to, from + 1, strokes);
                }
                else
                {
                    Emit(inks, choice, count, shared, to, reach, strokes);
                    Emit(inks, choice, count, from + 1, shared - 1, from + 1, strokes);
                }
            }

            bool Inside(int x, int y) const
            {
                return x >= m_left && x < m_left + m_width && y >= m_top && y < m_top + m_height;
            }

            std::size_t Index(int x, int y) const
            {
                return static_cast<std::size_t>((y - m_top) * m_width + (x - m_left));
            }

            int m_left;
            int m_top;
            int m_width;
            int m_height;
            std::vector<std::uint8_t> m_cells;
        };

        struct Lamp
        {
            float X = 0.0f;
            float Y = -0.7f;
            float Z = 0.7f;
            float Side = 0.0f;
        };

        Lamp LampOf(float side, float height)
        {
            const float x = side * (0.5f + 0.5f * (1.0f - height));
            const float y = -(0.35f + 0.5f * height);
            const float z = 0.7f;
            const float length = std::sqrt(x * x + y * y + z * z);
            return { x / length, y / length, z / length, side };
        }

        int ToneOf(float light)
        {
            return light < 0.2f ? 0 : light < 0.42f ? 1 : light < 0.62f ? 2 : light < 0.82f ? 3 : 4;
        }

        float LightOn(const Lamp& lamp, float nx, float ny, float nz)
        {
            return 0.5f + 0.5f * (nx * lamp.X + ny * lamp.Y + nz * lamp.Z);
        }

        void Clump(Sketch& sketch, float cx, float cy, float rx, float ry, Stuff stuff, const Lamp& lamp, float shade, std::uint32_t seed, float ragged, int lowest = 0)
        {
            const auto top = static_cast<int>(std::floor(cy - ry));
            const auto bottom = static_cast<int>(std::ceil(cy + ry));
            const auto left = static_cast<int>(std::floor(cx - rx));
            const auto right = static_cast<int>(std::ceil(cx + rx));
            for (int y = top; y <= bottom; ++y)
            {
                for (int x = left; x <= right; ++x)
                {
                    const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
                    const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
                    const float reach = nx * nx + ny * ny;
                    if (reach > 1.0f)
                        continue;
                    const std::uint32_t mixed = Hash(x, y, seed);
                    if (reach > 0.6f && Unit(mixed) < ragged)
                        continue;
                    const float light = LightOn(lamp, nx, ny, std::sqrt(1.0f - reach)) + shade + (Unit(mixed >> 16) - 0.5f) * 0.08f;
                    sketch.Put(x, y, Ink(stuff, std::max(lowest, ToneOf(light))));
                }
            }
        }

        bool ShowsThrough(std::uint8_t ink)
        {
            return ink == 0 || (ink & 7u) <= 1u;
        }

        void Limb(Sketch& sketch, float x0, float y0, float x1, float y1, float from, float to, Stuff stuff, const Lamp& lamp, bool behind)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const bool upright = std::fabs(dy) >= std::fabs(dx);
            const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy)) * 1.5f)));
            for (int step = 0; step <= steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                const float x = x0 + dx * t;
                const float y = y0 + dy * t;
                const int count = std::max(1, static_cast<int>(std::round(from + (to - from) * t)));
                for (int index = 0; index < count; ++index)
                {
                    const float offset = static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f;
                    const auto cx = static_cast<int>(std::floor(upright ? x + offset : x));
                    const auto cy = static_cast<int>(std::floor(upright ? y : y + offset));
                    if (behind && !ShowsThrough(sketch.At(cx, cy)))
                        continue;
                    int tone = 1;
                    if (count > 1)
                    {
                        const float toward = (upright ? offset * lamp.Side : -offset) / (static_cast<float>(count) * 0.5f);
                        tone = toward > 0.3f ? 3 : toward < -0.3f ? 1 : 2;
                    }
                    sketch.Put(cx, cy, Ink(stuff, tone));
                }
            }
        }

        void Trunk(Sketch& sketch, float height, float base, float top, float lean, Stuff stuff, const Lamp& lamp, std::uint32_t seed)
        {
            const auto rows = static_cast<int>(std::ceil(height));
            for (int row = 0; row < rows; ++row)
            {
                const float up = static_cast<float>(row) / std::max(1.0f, height);
                const float flare = row < 3 ? static_cast<float>(3 - row) * 0.9f : 0.0f;
                const float width = base + (top - base) * std::sqrt(up) + flare;
                const float middle = lean * up * up;
                const auto left = static_cast<int>(std::round(middle - width * 0.5f));
                const auto right = std::max(left + 1, static_cast<int>(std::round(middle + width * 0.5f)));
                const int y = -1 - row;
                for (int x = left; x < right; ++x)
                {
                    const float across = right - left > 1 ? (static_cast<float>(x - left) + 0.5f) / static_cast<float>(right - left) * 2.0f - 1.0f : 0.0f;
                    int tone = 2;
                    if (std::fabs(lamp.Side) < 0.25f)
                        tone = std::fabs(across) > 0.7f ? 1 : std::fabs(across) < 0.3f ? 3 : 2;
                    else
                    {
                        const float toward = across * (lamp.Side > 0.0f ? 1.0f : -1.0f);
                        tone = toward > 0.4f ? 3 : toward > -0.25f ? 2 : toward > -0.75f ? 1 : 0;
                    }
                    const std::uint32_t mixed = Hash(x, (y + 4000 + static_cast<int>(Hash(x, 0, seed) % 7u)) / 7, seed + 1u);
                    if (mixed % 5u == 0u)
                        tone = std::max(0, tone - 1);
                    else if (tone == 3 && mixed % 7u == 1u)
                        tone = 4;
                    sketch.Put(x, y, Ink(stuff, tone));
                }
            }
        }

        void Stroke(Sketch& sketch, float x0, float y0, float x1, float y1, std::uint8_t ink)
        {
            const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)))));
            for (int step = 0; step <= steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                sketch.Put(static_cast<int>(std::floor(x0 + (x1 - x0) * t)), static_cast<int>(std::floor(y0 + (y1 - y0) * t)), ink);
            }
        }

        struct Lobe
        {
            float X;
            float Y;
            float RadiusX;
            float RadiusY;
        };

        struct Leafage
        {
            const Lobe* Lobes = nullptr;
            int Count = 0;
            Stuff Main = Stuff::Scarlet;
            Stuff Second = Stuff::Ember;
            float SecondShare = 0.0f;
            float ClumpSize = 5.0f;
            float Body = 0.85f;
            float Ragged = 0.45f;
            int Holes = 3;
        };

        struct Mass
        {
            float X = 0.0f;
            float Y = 0.0f;
            float RadiusX = 1.0f;
            float RadiusY = 1.0f;
        };

        Mass MassOf(const Leafage& leafage)
        {
            float left = 1.0e6f;
            float right = -1.0e6f;
            float top = 1.0e6f;
            float bottom = -1.0e6f;
            for (int index = 0; index < leafage.Count; ++index)
            {
                const Lobe& lobe = leafage.Lobes[index];
                left = std::min(left, lobe.X - lobe.RadiusX);
                right = std::max(right, lobe.X + lobe.RadiusX);
                top = std::min(top, lobe.Y - lobe.RadiusY);
                bottom = std::max(bottom, lobe.Y + lobe.RadiusY);
            }
            return { (left + right) * 0.5f, (top + bottom) * 0.5f, std::max(1.0f, (right - left) * 0.5f), std::max(1.0f, (bottom - top) * 0.5f) };
        }

        float Overall(const Mass& mass, const Lamp& lamp, float x, float y)
        {
            return -0.2f * (y - mass.Y) / mass.RadiusY + 0.1f * (x - mass.X) / mass.RadiusX * lamp.Side;
        }

        Stuff StuffAt(const Leafage& leafage, const Mass& mass, float y, std::uint32_t mixed)
        {
            const float share = leafage.SecondShare * std::clamp(1.0f - 0.8f * (y - mass.Y) / mass.RadiusY, 0.2f, 1.8f);
            return Unit(mixed >> 4) < share ? leafage.Second : leafage.Main;
        }

        void LeafageBack(Sketch& sketch, const Leafage& leafage, const Lamp& lamp, std::uint32_t seed)
        {
            const Mass mass = MassOf(leafage);
            for (int index = 0; index < leafage.Count; ++index)
            {
                const Lobe& lobe = leafage.Lobes[index];
                Clump(sketch, lobe.X, lobe.Y + lobe.RadiusY * 0.06f, lobe.RadiusX * leafage.Body, lobe.RadiusY * leafage.Body, leafage.Main, lamp,
                    -0.36f + Overall(mass, lamp, lobe.X, lobe.Y), seed + static_cast<std::uint32_t>(index) * 7u, 0.0f);
            }
            for (int index = 0; index < leafage.Count; ++index)
            {
                const Lobe& lobe = leafage.Lobes[index];
                const float around = 6.2831853f * std::sqrt((lobe.RadiusX * lobe.RadiusX + lobe.RadiusY * lobe.RadiusY) * 0.5f);
                const int count = std::max(4, static_cast<int>(std::round(around / (leafage.ClumpSize * 1.8f))));
                for (int clump = 0; clump < count; ++clump)
                {
                    const std::uint32_t mixed = Noise(seed * 31u + static_cast<std::uint32_t>(index) * 7919u + static_cast<std::uint32_t>(clump) * 977u);
                    const float angle = (static_cast<float>(clump) + Unit(mixed) * 0.7f) / static_cast<float>(count) * 6.2831853f;
                    const float reach = 0.66f + 0.26f * Unit(mixed >> 16);
                    const float size = leafage.ClumpSize * (0.8f + 0.4f * Unit(mixed >> 8));
                    const float cx = lobe.X + std::cos(angle) * reach * std::max(1.0f, lobe.RadiusX - size * 0.5f);
                    const float cy = lobe.Y + std::sin(angle) * reach * std::max(1.0f, lobe.RadiusY - size * 0.5f);
                    Clump(sketch, cx, cy, size, size * 0.85f, StuffAt(leafage, mass, cy, mixed), lamp, -0.08f + Overall(mass, lamp, cx, cy), mixed, leafage.Ragged);
                }
            }
        }

        void LeafageFront(Sketch& sketch, const Leafage& leafage, const Lamp& lamp, std::uint32_t seed)
        {
            const Mass mass = MassOf(leafage);
            struct Tuft
            {
                float X;
                float Y;
                float Size;
                std::uint32_t Mixed;
            };
            std::vector<Tuft> tufts;
            for (int index = 0; index < leafage.Count; ++index)
            {
                const Lobe& lobe = leafage.Lobes[index];
                const int count = std::max(2, static_cast<int>(std::round(lobe.RadiusX * lobe.RadiusY / (leafage.ClumpSize * leafage.ClumpSize * 1.5f))));
                for (int clump = 0; clump < count; ++clump)
                {
                    const std::uint32_t mixed = Noise(seed * 57u + static_cast<std::uint32_t>(index) * 6151u + static_cast<std::uint32_t>(clump) * 613u + 3u);
                    const float reach = std::sqrt((static_cast<float>(clump) + 0.5f) / static_cast<float>(count)) * 0.62f;
                    const float angle = static_cast<float>(clump) * 2.39996f + Unit(mixed) * 0.8f + static_cast<float>(index);
                    tufts.push_back({ lobe.X + std::cos(angle) * reach * lobe.RadiusX, lobe.Y + std::sin(angle) * reach * lobe.RadiusY,
                        leafage.ClumpSize * (0.75f + 0.35f * Unit(mixed >> 8)), mixed });
                }
            }
            std::sort(tufts.begin(), tufts.end(), [](const Tuft& lhs, const Tuft& rhs) { return lhs.Y < rhs.Y; });
            for (const Tuft& tuft : tufts)
            {
                Clump(sketch, tuft.X, tuft.Y, tuft.Size, tuft.Size * 0.85f, StuffAt(leafage, mass, tuft.Y, tuft.Mixed), lamp, 0.04f + Overall(mass, lamp, tuft.X, tuft.Y), tuft.Mixed,
                    leafage.Ragged * 0.75f, 1);
            }
            for (int index = 0; index < leafage.Holes; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 71u + static_cast<std::uint32_t>(index) * 337u + 11u);
                const Lobe& lobe = leafage.Lobes[mixed % static_cast<std::uint32_t>(leafage.Count)];
                const float angle = Unit(mixed >> 8) * 6.2831853f;
                const float reach = 0.3f + 0.35f * Unit(mixed >> 16);
                const auto x = static_cast<int>(std::floor(lobe.X + std::cos(angle) * reach * lobe.RadiusX));
                const auto y = static_cast<int>(std::floor(lobe.Y + std::sin(angle) * reach * lobe.RadiusY));
                const int wide = 2 + static_cast<int>((mixed >> 4) % 2u);
                for (int row = 0; row < 2; ++row)
                {
                    for (int cell = row; cell < wide + row; ++cell)
                    {
                        if (sketch.At(x + cell, y + row) != 0)
                            sketch.Put(x + cell, y + row, Ink(leafage.Main, 0));
                    }
                }
            }
        }

        void Twigs(Sketch& sketch, const Leafage& leafage, const Lamp& lamp, std::uint32_t seed, int count)
        {
            const Mass mass = MassOf(leafage);
            for (int index = 0; index < count; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 13u + static_cast<std::uint32_t>(index) * 331u + 9u);
                const float angle = -3.1415927f * (0.08f + 0.84f * (static_cast<float>(index) + 0.2f + 0.6f * Unit(mixed)) / static_cast<float>(count));
                const float cosine = std::cos(angle);
                const float sine = std::sin(angle);
                float reach = 0.4f;
                while (reach < 1.6f &&
                    sketch.At(static_cast<int>(std::floor(mass.X + cosine * reach * mass.RadiusX)), static_cast<int>(std::floor(mass.Y + sine * reach * mass.RadiusY))) != 0)
                    reach += 0.03f;
                const float length = 2.0f + 4.5f * Unit(mixed >> 16);
                const float edgeX = mass.X + cosine * reach * mass.RadiusX;
                const float edgeY = mass.Y + sine * reach * mass.RadiusY;
                Limb(sketch, edgeX - cosine * 3.0f, edgeY - sine * 3.0f, edgeX + cosine * length, edgeY + sine * length, 1.0f, 1.0f, Stuff::Bark, lamp, true);
                if (length > 3.5f)
                {
                    const float fork = angle + ((mixed >> 3) & 1u ? 0.75f : -0.75f);
                    const float forkX = edgeX + cosine * length * 0.5f;
                    const float forkY = edgeY + sine * length * 0.5f;
                    Limb(sketch, forkX, forkY, forkX + std::cos(fork) * 2.5f, forkY + std::sin(fork) * 2.5f, 1.0f, 1.0f, Stuff::Bark, lamp, true);
                }
            }
        }

        enum class Thing : std::uint32_t
        {
            Oak,
            Maple,
            Birch,
            Bush,
            Pile,
            Stump,
            Pumpkins
        };

        constexpr Lobe kMapleLobes[] = {
            { 0.0f, -43.0f, 16.0f, 14.0f }, { -11.0f, -36.0f, 10.0f, 9.0f }, { 11.0f, -38.0f, 10.0f, 9.0f },
            { -6.0f, -54.0f, 10.0f, 8.0f }, { 6.0f, -55.0f, 9.0f, 8.0f }, { 1.0f, -31.0f, 9.0f, 5.0f },
        };
        constexpr Lobe kOakLobes[] = {
            { 0.0f, -39.0f, 17.0f, 11.0f }, { -16.0f, -34.0f, 11.0f, 8.0f }, { 16.0f, -36.0f, 11.0f, 8.0f }, { -7.0f, -48.0f, 11.0f, 7.0f },
            { 9.0f, -49.0f, 10.0f, 6.0f }, { -25.0f, -29.0f, 6.0f, 5.0f }, { 24.0f, -30.0f, 6.0f, 5.0f },
        };
        constexpr Lobe kBrackenLobes[] = {
            { -8.0f, -4.0f, 7.0f, 4.0f }, { 7.0f, -4.0f, 7.0f, 4.0f }, { 0.0f, -6.0f, 6.0f, 4.0f }, { -14.0f, -2.5f, 4.0f, 2.5f }, { 14.0f, -2.5f, 4.0f, 2.5f },
        };
        constexpr Lobe kBirchLobes[] = {
            { -1.0f, -45.0f, 8.0f, 12.0f }, { -6.0f, -35.0f, 6.0f, 7.0f }, { 5.0f, -37.0f, 6.0f, 8.0f }, { -2.0f, -56.0f, 6.0f, 5.0f }, { 3.0f, -28.0f, 4.0f, 4.0f },
        };
        constexpr Lobe kBushLobes[] = {
            { -7.0f, -6.0f, 8.0f, 6.0f }, { 6.0f, -7.0f, 9.0f, 7.0f }, { -1.0f, -11.0f, 8.0f, 6.0f }, { -13.0f, -4.0f, 4.5f, 4.0f }, { 13.0f, -4.0f, 4.5f, 4.0f },
        };

        std::vector<Block> DrawMaple(const Lamp& lamp, std::uint32_t seed)
        {
            Sketch sketch(-30, -70, 31, 1);
            Trunk(sketch, 36.0f, 6.0f, 3.5f, 1.0f, Stuff::Bark, lamp, seed);
            Leafage leafage;
            leafage.Lobes = kMapleLobes;
            leafage.Count = static_cast<int>(std::size(kMapleLobes));
            leafage.Main = Stuff::Scarlet;
            leafage.Second = Stuff::Ember;
            leafage.SecondShare = 0.35f;
            leafage.ClumpSize = 5.0f;
            leafage.Body = 0.85f;
            leafage.Ragged = 0.45f;
            leafage.Holes = 4;
            LeafageBack(sketch, leafage, lamp, seed);
            LeafageFront(sketch, leafage, lamp, seed);
            Limb(sketch, 1.0f, -27.0f, -11.0f, -40.0f, 2.5f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 1.0f, -28.0f, 11.0f, -42.0f, 2.5f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 1.0f, -30.0f, -2.0f, -56.0f, 2.0f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 1.0f, -31.0f, 6.0f, -55.0f, 2.0f, 1.0f, Stuff::Bark, lamp, true);
            Twigs(sketch, leafage, lamp, seed, 5);
            return sketch.Blocks();
        }

        std::vector<Block> DrawOak(const Lamp& lamp, std::uint32_t seed)
        {
            Sketch sketch(-36, -62, 37, 1);
            Trunk(sketch, 30.0f, 8.0f, 5.5f, -0.5f, Stuff::Bark, lamp, seed);
            const int hole = lamp.Side > 0.0f ? -2 : 1;
            sketch.Put(hole, -17, Ink(Stuff::Bark, 0));
            sketch.Put(hole + 1, -17, Ink(Stuff::Bark, 0));
            sketch.Put(hole, -16, Ink(Stuff::Bark, 0));
            sketch.Put(hole + 1, -16, Ink(Stuff::Bark, 1));
            sketch.Put(hole, -18, Ink(Stuff::Bark, 3));
            sketch.Put(hole + 1, -18, Ink(Stuff::Bark, 3));
            Leafage leafage;
            leafage.Lobes = kOakLobes;
            leafage.Count = static_cast<int>(std::size(kOakLobes));
            leafage.Main = Stuff::Gold;
            leafage.Second = Stuff::Russet;
            leafage.SecondShare = 0.4f;
            leafage.ClumpSize = 6.5f;
            leafage.Body = 0.75f;
            leafage.Ragged = 0.42f;
            leafage.Holes = 4;
            LeafageBack(sketch, leafage, lamp, seed);
            LeafageFront(sketch, leafage, lamp, seed);
            Limb(sketch, -1.0f, -22.0f, -17.0f, -33.0f, 4.0f, 1.5f, Stuff::Bark, lamp, true);
            Limb(sketch, 1.0f, -23.0f, 17.0f, -35.0f, 4.0f, 1.5f, Stuff::Bark, lamp, true);
            Limb(sketch, -10.0f, -29.0f, -25.0f, -29.0f, 2.0f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 10.0f, -30.0f, 24.0f, -31.0f, 2.0f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 0.0f, -27.0f, -6.0f, -48.0f, 3.0f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 1.0f, -27.0f, 8.0f, -49.0f, 2.5f, 1.0f, Stuff::Bark, lamp, true);
            Twigs(sketch, leafage, lamp, seed, 6);
            Leafage bracken;
            bracken.Lobes = kBrackenLobes;
            bracken.Count = static_cast<int>(std::size(kBrackenLobes));
            bracken.Main = Stuff::Russet;
            bracken.Second = Stuff::Litter;
            bracken.SecondShare = 0.4f;
            bracken.ClumpSize = 4.0f;
            bracken.Body = 0.85f;
            bracken.Ragged = 0.35f;
            bracken.Holes = 0;
            LeafageBack(sketch, bracken, lamp, seed + 17u);
            for (int index = 0; index < 6; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 47u + static_cast<std::uint32_t>(index) * 389u);
                const float baseX = -13.0f + 26.0f * (static_cast<float>(index) + 0.5f) / 6.0f + (Unit(mixed) - 0.5f) * 2.0f;
                const float lean = baseX < 0.0f ? -1.0f : 1.0f;
                const float high = 12.5f + 3.0f * Unit(mixed >> 8) - std::fabs(baseX) * 0.3f;
                const float topX = baseX + lean * 1.5f;
                const float tipX = topX + lean * (3.0f + 2.0f * Unit(mixed >> 16));
                const bool lit = lean * lamp.Side > 0.0f;
                Stroke(sketch, baseX, -4.0f, topX, -high, Ink(Stuff::Russet, lit ? 3 : 2));
                Stroke(sketch, topX, -high, tipX, -high + 2.5f, Ink(Stuff::Russet, lit ? 4 : 3));
                for (int step = 1; step < 4; ++step)
                {
                    const float y = -4.0f - (high - 4.0f) * static_cast<float>(step) / 4.0f;
                    const float x = baseX + lean * 1.5f * static_cast<float>(step) / 4.0f;
                    const int side = step % 2 == 0 ? -1 : 1;
                    sketch.Put(static_cast<int>(std::floor(x)) + side, static_cast<int>(std::floor(y)), Ink(Stuff::Russet, side * lamp.Side > 0.0f ? 3 : 1));
                }
            }
            sketch.Cut(0);
            return sketch.Blocks();
        }

        std::vector<Block> DrawBirch(const Lamp& lamp, std::uint32_t seed)
        {
            Sketch sketch(-22, -66, 23, 1);
            const float lean = -2.0f;
            const float height = 58.0f;
            const auto middleAt = [&](float y)
                {
                    const float up = -y / height;
                    return lean * up * up;
                };
            const auto rows = static_cast<int>(height);
            for (int row = 0; row < rows; ++row)
            {
                const int y = -1 - row;
                const float up = static_cast<float>(row) / height;
                const float width = 3.6f - 1.8f * up + (row < 2 ? 1.0f : 0.0f);
                const float middle = middleAt(static_cast<float>(y));
                const auto left = static_cast<int>(std::round(middle - width * 0.5f));
                const auto right = std::max(left + 1, static_cast<int>(std::round(middle + width * 0.5f)));
                const std::uint32_t mark = Hash(0, y, seed + 3u);
                const bool marked = mark % 4u == 0u;
                const int markFrom = left + static_cast<int>((mark >> 8) % 2u);
                for (int x = left; x < right; ++x)
                {
                    const float across = right - left > 1 ? (static_cast<float>(x - left) + 0.5f) / static_cast<float>(right - left) * 2.0f - 1.0f : 0.0f;
                    const float toward = across * (lamp.Side > 0.0f ? 1.0f : lamp.Side < 0.0f ? -1.0f : 0.0f);
                    int tone = toward > 0.3f ? 4 : toward > -0.4f ? 3 : 2;
                    if (marked && x >= markFrom && x < markFrom + 2)
                        tone = 0;
                    sketch.Put(x, y, Ink(Stuff::BirchBark, tone));
                }
            }
            Leafage leafage;
            leafage.Lobes = kBirchLobes;
            leafage.Count = static_cast<int>(std::size(kBirchLobes));
            leafage.Main = Stuff::Lemon;
            leafage.Second = Stuff::Gold;
            leafage.SecondShare = 0.25f;
            leafage.ClumpSize = 3.2f;
            leafage.Body = 0.55f;
            leafage.Ragged = 0.55f;
            leafage.Holes = 4;
            LeafageBack(sketch, leafage, lamp, seed);
            for (int index = 0; index < leafage.Count; ++index)
            {
                const Lobe& lobe = kBirchLobes[index];
                Limb(sketch, middleAt(lobe.Y + 6.0f), lobe.Y + 6.0f, lobe.X, lobe.Y, 1.0f, 1.0f, Stuff::Bark, lamp, true);
            }
            LeafageFront(sketch, leafage, lamp, seed);
            for (int index = 0; index < 7; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 43u + static_cast<std::uint32_t>(index) * 271u);
                const float x = -9.0f + 20.0f * (static_cast<float>(index) + Unit(mixed) * 0.6f) / 7.0f;
                float y = -60.0f;
                while (y < -22.0f && sketch.At(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))) == 0)
                    y += 1.0f;
                while (y < -20.0f && sketch.At(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))) != 0)
                    y += 1.0f;
                if (y >= -20.0f)
                    continue;
                const float hang = 2.0f + static_cast<float>((mixed >> 8) % 4u);
                Limb(sketch, x, y - 1.0f, x, y + hang, 1.0f, 1.0f, Stuff::Bark, lamp, true);
                sketch.Put(static_cast<int>(std::floor(x)) + ((mixed >> 12) & 1u ? 1 : -1), static_cast<int>(y + hang), Ink(Stuff::Lemon, 2));
            }
            return sketch.Blocks();
        }

        std::vector<Block> DrawBush(const Lamp& lamp, std::uint32_t seed)
        {
            Sketch sketch(-20, -20, 21, 1);
            Leafage leafage;
            leafage.Lobes = kBushLobes;
            leafage.Count = static_cast<int>(std::size(kBushLobes));
            leafage.Main = Stuff::Crimson;
            leafage.Second = Stuff::Scarlet;
            leafage.SecondShare = 0.25f;
            leafage.ClumpSize = 3.4f;
            leafage.Body = 0.9f;
            leafage.Ragged = 0.45f;
            leafage.Holes = 2;
            LeafageBack(sketch, leafage, lamp, seed);
            Limb(sketch, -1.0f, -1.0f, -5.0f, -7.0f, 1.0f, 1.0f, Stuff::Bark, lamp, true);
            Limb(sketch, 1.0f, -1.0f, 4.0f, -8.0f, 1.0f, 1.0f, Stuff::Bark, lamp, true);
            LeafageFront(sketch, leafage, lamp, seed);
            sketch.Cut(0);
            for (int x = -18; x <= 18; ++x)
            {
                if (sketch.At(x, -1) != 0)
                    sketch.Put(x, -1, Ink(Stuff::Crimson, 0));
            }
            for (int index = 0; index < 8; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 29u + static_cast<std::uint32_t>(index) * 197u);
                const int x = -13 + static_cast<int>(mixed % 27u);
                const int y = -3 - static_cast<int>((mixed >> 8) % 11u);
                if ((sketch.At(x, y) & 7u) >= 2u)
                    sketch.Put(x, y, Ink(Stuff::Pumpkin, 3));
            }
            return sketch.Blocks();
        }

        std::vector<Block> DrawPile(const Lamp& lamp, std::uint32_t seed)
        {
            Sketch sketch(-22, -16, 23, 1);
            constexpr Stuff kLeaves[] = { Stuff::Russet, Stuff::Litter, Stuff::Ember, Stuff::Scarlet, Stuff::Gold, Stuff::Litter, Stuff::Russet, Stuff::Ember };
            constexpr float kWidth = 14.5f;
            constexpr float kHeight = 11.0f;
            for (int x = -15; x < 15; ++x)
            {
                const float across = (static_cast<float>(x) + 0.5f) / kWidth;
                if (std::fabs(across) >= 1.0f)
                    continue;
                const float top = kHeight * std::sqrt(1.0f - across * across) * (0.88f + 0.12f * Unit(Hash((x + 100) / 2, 0, seed)));
                for (int y = -1; static_cast<float>(-y) <= top + 0.5f; --y)
                {
                    const float up = (static_cast<float>(y) + 0.5f) / kHeight;
                    const float nz = std::sqrt(std::max(0.0f, 1.0f - across * across - up * up));
                    const int shifted = x + 300 + (y * 2 + 300) % 3;
                    const std::uint32_t flake = Hash(shifted / 3, y, seed + 5u);
                    int tone = ToneOf(LightOn(lamp, across, up, nz) + (Unit(flake >> 8) - 0.5f) * 0.3f);
                    if (shifted % 3 == 0 && flake % 3u == 0u)
                        tone = std::max(0, tone - 2);
                    sketch.Put(x, y, Ink(kLeaves[flake % 8u], tone));
                }
            }
            for (int index = 0; index < 9; ++index)
            {
                const std::uint32_t mixed = Noise(seed * 37u + static_cast<std::uint32_t>(index) * 151u);
                const int side = index % 2 == 0 ? -1 : 1;
                const int x = side * (13 + static_cast<int>(mixed % 6u));
                const int y = -static_cast<int>((mixed >> 8) % 2u);
                sketch.Put(x, y, Ink(kLeaves[(mixed >> 12) % 8u], 2 + static_cast<int>((mixed >> 16) % 2u)));
                if ((mixed >> 20) % 2u == 0u)
                    sketch.Put(x + 1, y, Ink(kLeaves[(mixed >> 12) % 8u], 2));
            }
            return sketch.Blocks();
        }

        void Mushroom(Sketch& sketch, int x, int high, float wide, Stuff cap, bool spotted, const Lamp& lamp)
        {
            for (int y = -1; y > -high; --y)
            {
                sketch.Put(x, y, Ink(Stuff::Stem, lamp.Side < 0.0f ? 3 : 2));
                if (wide > 3.0f)
                    sketch.Put(x + 1, y, Ink(Stuff::Stem, lamp.Side < 0.0f ? 2 : 3));
            }
            const float cx = static_cast<float>(x) + (wide > 3.0f ? 1.0f : 0.5f);
            const float cy = -static_cast<float>(high) + 0.5f;
            const float rx = wide * 0.5f;
            const float ry = std::max(1.0f, wide * 0.32f);
            for (int y = static_cast<int>(std::floor(cy - ry)); y <= static_cast<int>(std::floor(cy)); ++y)
            {
                for (int cell = static_cast<int>(std::floor(cx - rx)); cell <= static_cast<int>(std::ceil(cx + rx)); ++cell)
                {
                    const float nx = (static_cast<float>(cell) + 0.5f - cx) / rx;
                    const float ny = (static_cast<float>(y) + 0.5f - cy) / (ry + 0.5f);
                    if (nx * nx + ny * ny > 1.0f)
                        continue;
                    const float light = LightOn(lamp, nx, ny, std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny)));
                    sketch.Put(cell, y, Ink(cap, std::clamp(ToneOf(light), 1, 4)));
                }
            }
            if (spotted)
            {
                sketch.Put(static_cast<int>(std::floor(cx - rx * 0.4f)), static_cast<int>(std::floor(cy - ry * 0.5f)), Ink(Stuff::Stem, 4));
                sketch.Put(static_cast<int>(std::floor(cx + rx * 0.35f)), static_cast<int>(std::floor(cy)), Ink(Stuff::Stem, 4));
            }
        }

        std::vector<Block> DrawStump(const Lamp& lamp, std::uint32_t seed)
        {
            Sketch sketch(-13, -13, 14, 1);
            Trunk(sketch, 7.0f, 10.0f, 9.5f, 0.0f, Stuff::Bark, lamp, seed);
            for (const int side : { -1, 1 })
            {
                for (int step = 0; step < 3; ++step)
                    sketch.Put(side * (6 + step) - (side < 0 ? 1 : 0), -1, Ink(Stuff::Bark, step == 2 ? 1 : 2));
                sketch.Put(side * 6 - (side < 0 ? 1 : 0), -2, Ink(Stuff::Bark, 1));
            }
            const float cx = 0.0f;
            const float cy = -7.5f;
            for (int y = -11; y <= -5; ++y)
            {
                for (int x = -6; x <= 6; ++x)
                {
                    const float nx = (static_cast<float>(x) + 0.5f - cx) / 5.4f;
                    const float ny = (static_cast<float>(y) + 0.5f - cy) / 2.3f;
                    const float reach = std::sqrt(nx * nx + ny * ny);
                    if (reach > 1.0f)
                        continue;
                    int tone = static_cast<int>(reach * 4.5f) % 2 == 0 ? 3 : 2;
                    if (reach < 0.18f)
                        tone = 1;
                    Stuff stuff = Stuff::Wood;
                    if (reach > 0.8f)
                    {
                        stuff = Stuff::Bark;
                        tone = ny < 0.0f ? (nx * lamp.Side > 0.2f ? 4 : 3) : 2;
                    }
                    sketch.Put(x, y, Ink(stuff, tone));
                }
            }
            sketch.Put(1, -8, Ink(Stuff::Wood, 1));
            sketch.Put(2, -8, Ink(Stuff::Wood, 1));
            sketch.Put(3, -7, Ink(Stuff::Wood, 1));
            const int shaded = lamp.Side > 0.0f ? -1 : 1;
            for (int y = -1; y >= -4; --y)
            {
                const int edge = shaded < 0 ? -5 : 4;
                sketch.Put(edge, y, Ink(Stuff::Moss, y == -4 ? 3 : 2));
                if (y > -3)
                    sketch.Put(edge - shaded, y, Ink(Stuff::Moss, 1));
            }
            Mushroom(sketch, 7, 4, 5.0f, Stuff::Cap, true, lamp);
            Mushroom(sketch, 10, 2, 3.0f, Stuff::Cap, false, lamp);
            Mushroom(sketch, -9, 3, 3.0f, Stuff::Wood, false, lamp);
            Mushroom(sketch, -7, 2, 2.0f, Stuff::Wood, false, lamp);
            return sketch.Blocks();
        }

        void Pumpkin(Sketch& sketch, float cx, float cy, float rx, float ry, const Lamp& lamp)
        {
            for (int y = static_cast<int>(std::floor(cy - ry)); y <= static_cast<int>(std::ceil(cy + ry)); ++y)
            {
                for (int x = static_cast<int>(std::floor(cx - rx)); x <= static_cast<int>(std::ceil(cx + rx)); ++x)
                {
                    const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
                    const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
                    const float reach = nx * nx + ny * ny;
                    if (reach > 1.0f || y > -1)
                        continue;
                    int tone = ToneOf(LightOn(lamp, nx, ny, std::sqrt(1.0f - reach)) + 0.05f);
                    const float rib = std::fabs(std::sin(std::asin(std::clamp(nx, -1.0f, 1.0f)) * 2.6f));
                    if (rib < 0.22f && reach < 0.8f)
                        tone = std::max(0, tone - 1);
                    sketch.Put(x, y, Ink(Stuff::Pumpkin, tone));
                }
            }
            const auto stemX = static_cast<int>(std::floor(cx));
            const auto stemY = static_cast<int>(std::floor(cy - ry));
            sketch.Put(stemX, stemY, Ink(Stuff::Vine, 1));
            sketch.Put(stemX, stemY - 1, Ink(Stuff::Vine, 2));
            sketch.Put(stemX + 1, stemY - 2, Ink(Stuff::Vine, 3));
        }

        std::vector<Block> DrawPumpkins(const Lamp& lamp, std::uint32_t)
        {
            Sketch sketch(-14, -14, 15, 1);
            for (int x = -9; x <= 9; ++x)
            {
                const int y = x < -5 || x > 6 ? -1 : 0;
                if (y < 0)
                    sketch.Put(x, y, Ink(Stuff::Vine, 2));
            }
            sketch.Put(-11, -2, Ink(Stuff::Vine, 2));
            sketch.Put(-12, -2, Ink(Stuff::Vine, 3));
            sketch.Put(-12, -3, Ink(Stuff::Vine, 3));
            sketch.Put(-11, -3, Ink(Stuff::Vine, 4));
            sketch.Put(-13, -2, Ink(Stuff::Vine, 1));
            sketch.Put(10, -2, Ink(Stuff::Vine, 2));
            sketch.Put(11, -3, Ink(Stuff::Vine, 1));
            Pumpkin(sketch, 4.5f, -3.5f, 4.3f, 3.4f, lamp);
            Pumpkin(sketch, -3.0f, -4.6f, 5.6f, 4.5f, lamp);
            Pumpkin(sketch, 2.5f, -1.8f, 2.8f, 2.1f, lamp);
            return sketch.Blocks();
        }

        std::vector<Block> DrawThing(Thing thing, const Lamp& lamp, std::uint32_t seed)
        {
            switch (thing)
            {
            case Thing::Oak:
                return DrawOak(lamp, seed);
            case Thing::Maple:
                return DrawMaple(lamp, seed);
            case Thing::Birch:
                return DrawBirch(lamp, seed);
            case Thing::Bush:
                return DrawBush(lamp, seed);
            case Thing::Pile:
                return DrawPile(lamp, seed);
            case Thing::Stump:
                return DrawStump(lamp, seed);
            default:
                return DrawPumpkins(lamp, seed);
            }
        }

        struct Measure
        {
            float Foot;
            float Cover;
            float CoverHeight;
            float Height;
            float Crown;
            float Reach;
        };

        Measure MeasureOf(Thing thing)
        {
            switch (thing)
            {
            case Thing::Oak:
                return { 3.0f, 13.0f, 10.0f, 56.0f, 30.0f, 31.0f };
            case Thing::Maple:
                return { 2.5f, 0.0f, 0.0f, 64.0f, 21.0f, 22.0f };
            case Thing::Birch:
                return { 1.5f, 0.0f, 0.0f, 62.0f, 11.0f, 12.0f };
            case Thing::Bush:
                return { 4.0f, 14.0f, 13.0f, 17.0f, 16.0f, 18.0f };
            case Thing::Pile:
                return { 4.0f, 12.0f, 10.0f, 11.0f, 14.0f, 17.0f };
            case Thing::Stump:
                return { 5.0f, 0.0f, 0.0f, 9.0f, 6.0f, 11.0f };
            default:
                return { 6.0f, 0.0f, 0.0f, 10.0f, 7.0f, 12.0f };
            }
        }

        struct Spot
        {
            Thing What;
            float Across;
            float Depth;
        };

        constexpr Spot kSpots[] = {
            { Thing::Oak, 0.04f, 0.2f },
            { Thing::Stump, 0.17f, 0.84f },
            { Thing::Bush, 0.30f, 0.50f },
            { Thing::Pumpkins, 0.44f, 0.94f },
            { Thing::Maple, 0.57f, 0.1f },
            { Thing::Pile, 0.74f, 0.68f },
            { Thing::Birch, 0.93f, 0.3f },
        };

        constexpr Spot kNarrowSpots[] = {
            { Thing::Maple, 0.0f, 0.22f },
            { Thing::Pumpkins, 1.0f, 0.78f },
        };

        void Arrange(const Frame& frame, std::vector<Prop>& props)
        {
            const auto columns = static_cast<float>(frame.Columns);
            const float back = frame.FeetBack + 2.0f;
            const float front = std::max(back, frame.FeetFront - 6.0f);
            for (const Spot& spot : frame.Narrow ? std::span<const Spot>(kNarrowSpots) : std::span<const Spot>(kSpots))
            {
                if (!frame.Narrow && frame.Columns < 200 && (spot.What == Thing::Stump || spot.What == Thing::Pumpkins))
                    continue;
                const Measure measure = MeasureOf(spot.What);
                const float ground = std::round(back + (front - back) * spot.Depth);
                const float margin = 8.0f + measure.Reach * frame.ScaleAt(ground);
                if (columns - 2.0f * margin < 8.0f)
                    continue;
                Prop prop;
                prop.X = std::round(margin + (columns - 2.0f * margin) * spot.Across);
                prop.Ground = ground;
                prop.Foot = measure.Foot;
                prop.Cover = measure.Cover;
                prop.CoverHeight = measure.CoverHeight;
                prop.Height = measure.Height;
                prop.Crown = measure.Crown;
                prop.Kind = static_cast<std::uint32_t>(spot.What);
                prop.Seed = Noise(static_cast<std::uint32_t>(spot.What) * 7919u + 101u);
                props.push_back(prop);
            }
        }

        template <typename Outline>
        void Ridge(const Frame& frame, float bottom, ImVec4 colour, ImVec4 rim, Outline&& outline)
        {
            int start = 0;
            float top = std::round(outline(0.0f));
            for (int column = 1; column <= frame.Columns; ++column)
            {
                const float next = column < frame.Columns ? std::round(outline(static_cast<float>(column))) : -1.0e6f;
                if (next == top)
                    continue;
                frame.Paint.Fill(static_cast<float>(start), top, static_cast<float>(column), bottom, colour);
                if (rim.w > 0.0f)
                    frame.Paint.Fill(static_cast<float>(start), top, static_cast<float>(column), top + 1.0f, rim);
                start = column;
                top = next;
            }
        }

        void Mist(const Frame& frame, float top, int rows, ImVec4 colour, float alpha)
        {
            for (int row = 0; row < rows; ++row)
            {
                const float middle = static_cast<float>(rows - 1) * 0.5f;
                const float share = 1.0f - std::fabs(static_cast<float>(row) - middle) / (middle + 1.0f);
                frame.Paint.Fill(0.0f, top + static_cast<float>(row), static_cast<float>(frame.Columns), top + static_cast<float>(row) + 1.0f, WithAlpha(colour, alpha * share));
            }
        }

        constexpr ImVec4 kForest[] = {
            Colour(0.660f, 0.300f, 0.140f), Colour(0.800f, 0.460f, 0.140f), Colour(0.800f, 0.620f, 0.200f), Colour(0.520f, 0.300f, 0.140f),
            Colour(0.460f, 0.420f, 0.180f), Colour(0.720f, 0.240f, 0.120f),
        };
        constexpr ImVec4 kConifer = Colour(0.160f, 0.250f, 0.210f);

        constexpr ImVec4 kGroundFar = Colour(0.600f, 0.470f, 0.250f);
        constexpr ImVec4 kGroundNear = Colour(0.460f, 0.300f, 0.140f);
        constexpr ImVec4 kGroundRim = Colour(0.780f, 0.640f, 0.330f);
        constexpr ImVec4 kDryGrass = Colour(0.720f, 0.600f, 0.340f);
        constexpr ImVec4 kDarkGrass = Colour(0.360f, 0.250f, 0.120f);
        constexpr ImVec4 kSoil = Colour(0.300f, 0.180f, 0.080f);
        constexpr ImVec4 kFallen[] = {
            Colour(0.760f, 0.230f, 0.100f), Colour(0.880f, 0.450f, 0.120f), Colour(0.880f, 0.660f, 0.200f), Colour(0.560f, 0.300f, 0.110f), Colour(0.680f, 0.160f, 0.090f),
        };

        constexpr ImVec4 kWaterTop = Colour(0.140f, 0.300f, 0.340f);
        constexpr ImVec4 kWaterDeep = Colour(0.050f, 0.150f, 0.210f);
        constexpr ImVec4 kWaterEdge = Colour(0.720f, 0.740f, 0.620f);
        constexpr ImVec4 kWaterBank = Colour(0.100f, 0.180f, 0.190f);

        class AutumnScene final : public Scene
        {
        public:
            void DrawBack(const Frame& frame) override
            {
                Settle(frame);
                const ImVec4 haze = HazeOf(frame);
                PaintSky(frame);
                DrawSky(frame, WithAlpha(SkyAt(frame, 0.3f), 0.0f));
                PaintGeese(frame);
                PaintDistance(frame, haze);
                PaintLand(frame, haze);
                PaintLake(frame);
            }

            void PlaceProps(const Frame& frame, std::vector<Prop>& props) override
            {
                m_props.clear();
                Arrange(frame, m_props);
                m_layout = { frame.Columns, frame.Rows, frame.Surface, frame.Shore };
                m_drawings.clear();
                props.insert(props.end(), m_props.begin(), m_props.end());
            }

            void DrawProp(const Frame& frame, const Prop& prop, const Canvas& paint) override
            {
                UpdateTones(frame);
                const float side = std::clamp(std::round((frame.Sun.X - prop.X) / 20.0f) * 0.5f, -1.0f, 1.0f);
                const int high = frame.Sun.Height < 0.45f ? 0 : frame.Sun.Height < 0.8f ? 1 : 2;
                const int key = static_cast<int>((side + 1.0f) * 2.0f) * 3 + high;
                Drawing* drawing = nullptr;
                for (Drawing& each : m_drawings)
                {
                    if (each.Seed == prop.Seed)
                        drawing = &each;
                }
                if (drawing == nullptr)
                {
                    m_drawings.push_back({ prop.Seed, -1, {} });
                    drawing = &m_drawings.back();
                }
                if (drawing->Key != key)
                {
                    drawing->Key = key;
                    drawing->Blocks = DrawThing(static_cast<Thing>(prop.Kind), LampOf(side, std::clamp(frame.Sun.Height, 0.0f, 1.0f)), prop.Seed);
                }
                for (const Block& block : drawing->Blocks)
                    paint.Fill(static_cast<float>(block.Left), static_cast<float>(block.Top), static_cast<float>(block.Right), static_cast<float>(block.Bottom), m_tones[block.Ink]);
            }

            void DrawFront(const Frame& frame) override
            {
                const Canvas& paint = frame.Paint;
                const float shore = frame.Shore;
                const ImVec4 blades[] = { Lighted(Colour(0.700f, 0.600f, 0.370f), frame), Lighted(Colour(0.560f, 0.480f, 0.260f), frame), Lighted(Colour(0.380f, 0.300f, 0.150f), frame) };
                const ImVec4 reed = Lighted(Colour(0.520f, 0.470f, 0.270f), frame);
                const ImVec4 reedDark = Lighted(Colour(0.360f, 0.330f, 0.170f), frame);
                const ImVec4 head = Lighted(Colour(0.330f, 0.190f, 0.090f), frame);
                const ImVec4 headLit = Lighted(Colour(0.460f, 0.290f, 0.140f), frame);
                const ImVec4 stone = Lighted(Colour(0.430f, 0.410f, 0.390f), frame);
                const ImVec4 stoneLit = Lighted(Colour(0.640f, 0.620f, 0.580f), frame);
                const bool lightLeft = frame.Sun.X < static_cast<float>(frame.Columns) * 0.5f;

                for (int spot = 0; spot < 3; ++spot)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(spot) * 389u + 77u);
                    const float x = std::round(static_cast<float>(frame.Columns) * (0.1f + 0.38f * static_cast<float>(spot)) + static_cast<float>(mixed % 12u));
                    constexpr int kHeights[] = { 5, 6, 4, 6, 3 };
                    for (int stalk = 0; stalk < 5; ++stalk)
                    {
                        const float sx = x + static_cast<float>(stalk) + (stalk >= 3 ? 1.0f : 0.0f);
                        const auto high = static_cast<float>(kHeights[(stalk + spot) % 5]);
                        paint.Fill(sx, shore - high, sx + 1.0f, shore, stalk % 2 == 0 ? reed : reedDark);
                        if (high >= 5.0f)
                        {
                            paint.Fill(sx, shore - high, sx + 1.0f, shore - high + 2.0f, head);
                            paint.Fill(sx, shore - high, sx + 1.0f, shore - high + 1.0f, headLit);
                            paint.Fill(sx, shore - high - 1.0f, sx + 1.0f, shore - high, reed);
                        }
                    }
                    paint.Fill(x - 1.0f, shore - 3.0f, x, shore - 1.0f, reed);
                    paint.Fill(x - 2.0f, shore - 4.0f, x - 1.0f, shore - 3.0f, reed);
                    paint.Fill(x + 6.0f, shore - 2.0f, x + 7.0f, shore, reedDark);
                    paint.Fill(x + 7.0f, shore - 3.0f, x + 8.0f, shore - 2.0f, reedDark);
                }

                for (int column = 3; column < frame.Columns - 3; ++column)
                {
                    const std::uint32_t mixed = Hash(column, 5, 211u);
                    if (mixed % 5u != 0u)
                        continue;
                    const auto x = static_cast<float>(column);
                    const std::uint32_t kind = (mixed >> 8) % 16u;
                    if (kind < 9u)
                    {
                        const int count = 2 + static_cast<int>((mixed >> 12) % 2u);
                        const int tall = 3 + static_cast<int>((mixed >> 14) % 3u);
                        for (int blade = 0; blade < count; ++blade)
                        {
                            const float bx = x + static_cast<float>(blade);
                            const auto high = static_cast<float>(std::max(2, tall - (blade == 1 ? 0 : blade + 1)));
                            const ImVec4 colour = blades[(blade + static_cast<int>(mixed >> 20)) % 3];
                            const bool bends = ((mixed >> (22 + blade)) & 1u) != 0u;
                            paint.Fill(bx, shore - high + (bends ? 1.0f : 0.0f), bx + 1.0f, shore, colour);
                            if (bends)
                            {
                                const float lean = blade == 0 ? -1.0f : 1.0f;
                                paint.Fill(bx + lean, shore - high, bx + lean + 1.0f, shore - high + 1.0f, colour);
                            }
                        }
                    }
                    else if (kind < 13u)
                    {
                        const ImVec4 leaf = Lighted(kFallen[(mixed >> 20) % std::size(kFallen)], frame);
                        paint.Fill(x, shore - 1.0f, x + 2.0f, shore, leaf);
                        if ((mixed >> 24) % 2u == 0u)
                            paint.Fill(x + 2.0f, shore - 2.0f, x + 3.0f, shore - 1.0f, leaf);
                    }
                    else if (kind < 14u)
                    {
                        paint.Fill(x, shore - 2.0f, x + 4.0f, shore, stone);
                        paint.Fill(x + 1.0f, shore - 3.0f, x + 3.0f, shore - 2.0f, stone);
                        paint.Fill(lightLeft ? x + 1.0f : x + 2.0f, shore - 3.0f, lightLeft ? x + 2.0f : x + 3.0f, shore - 2.0f, stoneLit);
                        paint.Fill(lightLeft ? x : x + 3.0f, shore - 2.0f, lightLeft ? x + 1.0f : x + 4.0f, shore - 1.0f, stoneLit);
                    }
                }
            }

            ImVec4 HazeColor(const Frame& frame) override
            {
                return HazeOf(frame);
            }

        private:
            struct Layout
            {
                int Columns = -1;
                int Rows = -1;
                float Surface = -1.0f;
                float Shore = -1.0f;

                bool operator==(const Layout&) const = default;
            };

            struct Drawing
            {
                std::uint32_t Seed = 0;
                int Key = -1;
                std::vector<Block> Blocks;
            };

            void Settle(const Frame& frame)
            {
                const Layout layout{ frame.Columns, frame.Rows, frame.Surface, frame.Shore };
                if (layout == m_layout)
                    return;
                m_layout = layout;
                m_props.clear();
                Arrange(frame, m_props);
                m_drawings.clear();
            }

            void UpdateTones(const Frame& frame)
            {
                if (frame.Daylight == m_tonesDaylight && frame.Sun.Moon == m_tonesMoon)
                    return;
                m_tonesDaylight = frame.Daylight;
                m_tonesMoon = frame.Sun.Moon;
                for (int stuff = 0; stuff < static_cast<int>(Stuff::Count); ++stuff)
                {
                    for (int tone = 0; tone < 5; ++tone)
                        m_tones[static_cast<std::size_t>(stuff * 8 + tone)] = ToneAt(stuff, tone, frame);
                }
            }

            void PaintSky(const Frame& frame)
            {
                const Canvas& paint = frame.Paint;
                const auto columns = static_cast<float>(frame.Columns);
                const float horizon = frame.Surface;
                constexpr int kBands = 12;
                ImVec4 above;
                for (int band = 0; band < kBands; ++band)
                {
                    const float from = std::round(horizon * static_cast<float>(band) / kBands);
                    const float to = band == kBands - 1 ? horizon + 8.0f : std::round(horizon * static_cast<float>(band + 1) / kBands);
                    const ImVec4 colour = SkyAt(frame, std::pow((static_cast<float>(band) + 0.5f) / kBands, 1.3f));
                    paint.Fill(0.0f, from, columns, to, colour);
                    if (band > 0)
                    {
                        int x = static_cast<int>(Hash(band, 0, 17u) % 5u);
                        while (x < frame.Columns)
                        {
                            const std::uint32_t mixed = Hash(x, band, 19u);
                            const auto length = static_cast<float>(2u + mixed % 6u);
                            paint.Fill(static_cast<float>(x), from, static_cast<float>(x) + length, from + 1.0f, above);
                            x += static_cast<int>(length) + 2 + static_cast<int>((mixed >> 8) % 6u);
                        }
                    }
                    above = colour;
                }
                if (frame.Daylight > 0.0f)
                {
                    const float x = std::round(columns * 0.2f);
                    const float y = SunY(frame);
                    DrawDisc(paint, x, y, 22.0f, ImVec4(1.0f, 0.93f, 0.72f, 0.07f * frame.Daylight));
                    DrawDisc(paint, x, y, 15.0f, ImVec4(1.0f, 0.90f, 0.66f, 0.10f * frame.Daylight));
                }
                if (frame.Daylight < 1.0f)
                {
                    const float x = std::round(columns * 0.8f);
                    const float y = MoonY(frame);
                    DrawDisc(paint, x, y, 19.0f, ImVec4(0.62f, 0.70f, 0.86f, 0.05f * (1.0f - frame.Daylight)));
                    DrawDisc(paint, x, y, 13.0f, ImVec4(0.62f, 0.70f, 0.86f, 0.07f * (1.0f - frame.Daylight)));
                }
            }

            void PaintGeese(const Frame& frame)
            {
                if (frame.Daylight < 0.05f)
                    return;
                const ImVec4 colour = WithAlpha(Colour(0.200f, 0.180f, 0.200f), 0.7f * frame.Daylight);
                const double span = static_cast<double>(frame.Columns) + 90.0;
                float leadX = std::round(static_cast<float>(frame.Columns) * 0.66f - static_cast<float>(std::fmod(frame.Time * 1.1, span)));
                if (leadX < -50.0f)
                    leadX += static_cast<float>(span);
                const float leadY = std::round(frame.Surface * 0.34f);
                constexpr int kArms[] = { 3, 4 };
                for (int arm = -1; arm < 2; ++arm)
                {
                    const int count = arm == 0 ? 1 : kArms[arm < 0 ? 0 : 1];
                    for (int rank = arm == 0 ? 0 : 1; rank <= (arm == 0 ? 0 : count); ++rank)
                    {
                        const float x = leadX + static_cast<float>(rank) * 4.0f;
                        const float y = leadY + static_cast<float>(arm * rank) * 2.0f;
                        frame.Paint.Fill(x - 1.0f, y - 1.0f, x, y, colour);
                        frame.Paint.Fill(x + 1.0f, y - 1.0f, x + 2.0f, y, colour);
                        frame.Paint.Fill(x, y, x + 1.0f, y + 1.0f, colour);
                    }
                }
            }

            void PaintDistance(const Frame& frame, ImVec4 haze)
            {
                const float s = frame.Surface;
                const float bottom = s + 6.0f;
                const float misty = 1.0f + 0.6f * (1.0f - frame.Daylight);
                const auto layer = [&](ImVec4 day, float share, ImVec4 night) { return DayNight(Mix(day, haze, share), night, frame.Daylight); };

                Ridge(frame, bottom, layer(Colour(0.500f, 0.480f, 0.600f), 0.55f, Colour(0.095f, 0.112f, 0.165f)), ImVec4(), [&](float x)
                    {
                        return s - (24.0f + 6.5f * std::sin(x * 0.021f + 0.4f) + 4.0f * std::sin(x * 0.067f + 2.0f) + 1.5f * std::sin(x * 0.19f));
                    });
                Mist(frame, s - 17.0f, 6, haze, 0.45f * misty);

                Ridge(frame, bottom, layer(Colour(0.580f, 0.450f, 0.400f), 0.45f, Colour(0.082f, 0.097f, 0.142f)),
                    layer(Colour(0.740f, 0.600f, 0.470f), 0.45f, Colour(0.098f, 0.114f, 0.160f)), [&](float x)
                    {
                        return s - (14.0f + 4.0f * std::sin(x * 0.033f + 1.3f) + 2.5f * std::sin(x * 0.087f));
                    });
                Mist(frame, s - 11.0f, 5, haze, 0.4f * misty);

                const auto nearHill = [&](float x) { return s - (6.0f + 3.0f * std::sin(x * 0.052f + 4.0f) + 1.5f * std::sin(x * 0.15f + 1.0f)); };
                const ImVec4 farNight = Colour(0.070f, 0.082f, 0.120f);
                Ridge(frame, bottom, layer(Colour(0.520f, 0.370f, 0.220f), 0.38f, farNight), ImVec4(), nearHill);
                for (int index = 0; index * 4 < frame.Columns + 4; ++index)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 613u + 29u);
                    const float x = static_cast<float>(index * 4) + static_cast<float>(mixed % 3u);
                    const float top = std::round(nearHill(x));
                    const auto high = static_cast<float>(2u + (mixed >> 8) % 3u);
                    const auto half = static_cast<float>(2u + (mixed >> 12) % 2u);
                    const ImVec4 colour = layer((mixed >> 16) % 7u == 0u ? kConifer : kForest[(mixed >> 20) % std::size(kForest)], 0.42f, farNight);
                    frame.Paint.Fill(x - half + 1.0f, top - high, x + half - 1.0f, top - high + 1.0f, colour);
                    frame.Paint.Fill(x - half, top - high + 1.0f, x + half, top + 1.0f, colour);
                }
                Mist(frame, s - 5.0f, 4, haze, 0.32f * misty);

                for (int row = 0; row < 2; ++row)
                {
                    const int spacing = row == 0 ? 7 : 9;
                    const float hazed = row == 0 ? 0.5f : 0.26f;
                    const ImVec4 night = row == 0 ? Colour(0.062f, 0.072f, 0.104f) : Colour(0.052f, 0.060f, 0.088f);
                    const ImVec4 moonlit = row == 0 ? Colour(0.074f, 0.086f, 0.122f) : Colour(0.074f, 0.086f, 0.124f);
                    for (int index = -1; index * spacing < frame.Columns + spacing; ++index)
                    {
                        const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index + 50) * 977u + static_cast<std::uint32_t>(row) * 7919u + 13u);
                        const float x = std::round(static_cast<float>(index * spacing) + Unit(mixed) * static_cast<float>(spacing));
                        const float grove = std::sin(x * 0.029f + 0.7f + static_cast<float>(row) * 1.7f) + 0.6f * std::sin(x * 0.083f + 2.0f);
                        if (grove < -0.35f)
                            continue;
                        const bool conifer = (mixed >> 16) % 6u == 0u;
                        const ImVec4 base = conifer ? kConifer : kForest[(mixed >> 20) % std::size(kForest)];
                        const ImVec4 shade = layer(ImVec4(base.x * 0.72f, base.y * 0.72f, base.z * 0.72f, 1.0f), hazed, night);
                        const ImVec4 lit = layer(Mix(base, Colour(1.0f, 0.85f, 0.55f), 0.15f), hazed, moonlit);
                        const float side = frame.Sun.X < x ? -1.0f : 1.0f;
                        const float rx = (row == 0 ? 3.5f : 4.0f) + Unit(mixed >> 4) * 2.5f;
                        const float ry = (row == 0 ? 4.0f : 5.0f) + Unit(mixed >> 8) * 2.5f;
                        const float baseY = s + static_cast<float>(row) + 1.0f;
                        frame.Paint.Fill(x, baseY - 4.0f, x + 1.0f, baseY, layer(kRamps[static_cast<int>(Stuff::Bark)][1], hazed, night));
                        const float top = std::round(baseY - 2.0f - ry * 2.0f - (conifer ? 3.0f : 0.0f));
                        const auto rows = static_cast<int>(std::round(ry * 2.0f + (conifer ? 3.0f : 0.0f)));
                        for (int line = 0; line < rows; ++line)
                        {
                            const float down = (static_cast<float>(line) + 0.5f) / static_cast<float>(rows);
                            float half = 0.0f;
                            if (conifer)
                                half = std::round(0.5f + static_cast<float>(line % 4) * 0.5f + static_cast<float>(line) * 0.25f);
                            else
                                half = std::round(rx * std::sqrt(std::max(0.0f, 1.0f - (down * 2.0f - 1.0f) * (down * 2.0f - 1.0f))));
                            if (half < 1.0f)
                                half = 1.0f;
                            const float y = top + static_cast<float>(line);
                            frame.Paint.Fill(x + 0.5f - half, y, x + 0.5f + half, y + 1.0f, row == 0 ? Mix(shade, lit, 0.5f) : shade);
                            if (row == 1)
                            {
                                const float litWidth = std::round(half * 1.1f * (1.0f - down));
                                if (litWidth > 0.0f)
                                {
                                    if (side < 0.0f)
                                        frame.Paint.Fill(x + 0.5f - half, y, x + 0.5f - half + litWidth, y + 1.0f, lit);
                                    else
                                        frame.Paint.Fill(x + 0.5f + half - litWidth, y, x + 0.5f + half, y + 1.0f, lit);
                                }
                            }
                        }
                    }
                }
            }

            void PaintLand(const Frame& frame, ImVec4 haze)
            {
                const Canvas& paint = frame.Paint;
                const float s = frame.Surface;
                const float shore = frame.Shore;
                const auto columns = static_cast<float>(frame.Columns);
                const float span = std::max(1.0f, frame.FeetFront - frame.FeetBack);
                const auto depthAt = [&](float y) { return std::clamp((y - frame.FeetBack) / span, 0.0f, 1.0f); };
                const auto groundAt = [&](float y)
                    {
                        return Mix(Lighted(Mix(kGroundFar, kGroundNear, std::pow(depthAt(y), 0.7f)), frame), haze, frame.HazeAt(y));
                    };
                const auto hazed = [&](ImVec4 colour, float y) { return Mix(Lighted(colour, frame), haze, frame.HazeAt(y)); };

                const ImVec4 back = groundAt(s + 1.0f);
                const ImVec4 rim = hazed(kGroundRim, s);
                const auto edgeAt = [&](int column)
                    {
                        const auto x = static_cast<float>(column);
                        return std::round(s + std::sin(x * 0.13f) * 1.1f + std::sin(x * 0.37f + 1.0f) * 0.7f);
                    };
                int start = 0;
                float top = edgeAt(0);
                for (int column = 1; column <= frame.Columns; ++column)
                {
                    const float next = column < frame.Columns ? edgeAt(column) : -1.0e6f;
                    if (next == top)
                        continue;
                    paint.Fill(static_cast<float>(start), top, static_cast<float>(column), s + 4.0f, back);
                    paint.Fill(static_cast<float>(start), top, static_cast<float>(column), top + 1.0f, rim);
                    start = column;
                    top = next;
                }
                for (int column = 0; column < frame.Columns; ++column)
                {
                    const std::uint32_t mixed = Hash(column, 1, 43u);
                    if (mixed % 6u == 0u)
                    {
                        const auto x = static_cast<float>(column);
                        const float edge = edgeAt(column);
                        paint.Fill(x, edge - static_cast<float>(1u + (mixed >> 8) % 2u), x + 1.0f, edge, (mixed >> 12) % 3u == 0u ? hazed(kDarkGrass, s) : rim);
                    }
                }

                constexpr int kBands = 12;
                const float first = s + 3.0f;
                const float depth = shore + 3.0f - first;
                for (int band = 0; band < kBands; ++band)
                {
                    const float from = std::round(first + depth * std::pow(static_cast<float>(band) / kBands, 1.4f));
                    const float to = std::round(first + depth * std::pow(static_cast<float>(band + 1) / kBands, 1.4f));
                    if (to > from)
                        paint.Fill(0.0f, from, columns, to, groundAt((from + to) * 0.5f));
                }

                for (int index = 0; index < frame.Columns / 10; ++index)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 1009u + 61u);
                    const float cy = std::round(first + 1.0f + (shore - first - 3.0f) * Unit(mixed >> 16));
                    const float scale = frame.ScaleAt(cy);
                    const float rx = (5.0f + 9.0f * Unit(mixed >> 4)) * scale;
                    const float ry = std::max(1.0f, std::round(rx * 0.16f));
                    const float cx = std::round(Unit(mixed) * columns);
                    const bool bare = (mixed >> 28) % 3u == 0u;
                    for (float y = cy - ry; y <= cy + ry; y += 1.0f)
                    {
                        const float down = (y - cy) / (ry + 0.5f);
                        const float half = std::round(rx * std::sqrt(std::max(0.0f, 1.0f - down * down)));
                        if (half >= 1.0f)
                            paint.Fill(cx - half, y, cx + half, y + 1.0f, Mix(groundAt(y), hazed(bare ? kSoil : kDryGrass, y), bare ? 0.3f : 0.28f));
                    }
                }

                for (int row = static_cast<int>(first); row < static_cast<int>(shore); ++row)
                {
                    const auto y = static_cast<float>(row);
                    const float nearness = depthAt(y);
                    const auto leafRoll = static_cast<std::uint32_t>(70.0f - 30.0f * nearness);
                    for (int cell = 0; cell * 3 < frame.Columns; ++cell)
                    {
                        const std::uint32_t mixed = Hash(cell, row, 71u);
                        const std::uint32_t roll = mixed & 1023u;
                        const auto x = static_cast<float>(cell * 3 + static_cast<int>((mixed >> 10) % 3u));
                        if (roll < leafRoll)
                        {
                            const ImVec4 leaf = hazed(kFallen[(mixed >> 13) % std::size(kFallen)], y);
                            const float width = nearness < 0.3f ? 1.0f : 2.0f;
                            paint.Fill(x, y, x + width, y + 1.0f, leaf);
                            if (nearness > 0.65f && (mixed >> 17) % 3u == 0u)
                                paint.Fill(x + width, y - 1.0f, x + width + 1.0f, y, leaf);
                        }
                        else if (roll < leafRoll + 40u)
                        {
                            const ImVec4 blade = Mix(groundAt(y), hazed(kDryGrass, y), 0.7f);
                            paint.Fill(x, y, x + 1.0f, y + 1.0f, blade);
                            paint.Fill(x + 1.0f, y, x + 2.0f, y + 1.0f, Mix(groundAt(y), hazed(kDarkGrass, y), 0.6f));
                            if (nearness > 0.45f && (mixed >> 17) % 2u == 0u)
                                paint.Fill(x - 1.0f, y - 1.0f, x, y, blade);
                        }
                        else if (roll < leafRoll + 56u)
                        {
                            paint.Fill(x, y, x + (nearness < 0.4f ? 1.0f : 2.0f), y + 1.0f, Mix(groundAt(y), hazed(kSoil, y), 0.7f));
                        }
                    }
                }

                const float mistiness = 0.32f * std::max(1.0f - frame.Daylight, GlowOf(frame));
                if (mistiness > 0.01f)
                {
                    constexpr int kRows = 7;
                    for (int row = 0; row < kRows; ++row)
                    {
                        const float y = s - 1.0f + static_cast<float>(row);
                        paint.Fill(0.0f, y, columns, y + 1.0f, WithAlpha(haze, mistiness * (1.0f - static_cast<float>(row) / kRows)));
                    }
                }

                for (const Prop& prop : m_props)
                {
                    const auto thing = static_cast<Thing>(prop.Kind);
                    Stuff fallen = Stuff::Blank;
                    Stuff second = Stuff::Blank;
                    if (thing == Thing::Maple)
                    {
                        fallen = Stuff::Scarlet;
                        second = Stuff::Ember;
                    }
                    else if (thing == Thing::Oak)
                    {
                        fallen = Stuff::Gold;
                        second = Stuff::Russet;
                    }
                    else if (thing == Thing::Birch)
                    {
                        fallen = Stuff::Lemon;
                        second = Stuff::Lemon;
                    }
                    else if (thing == Thing::Bush)
                    {
                        fallen = Stuff::Crimson;
                        second = Stuff::Scarlet;
                    }
                    else
                        continue;
                    const float scale = frame.ScaleAt(prop.Ground);
                    const float rx = prop.Crown * scale * 1.1f;
                    const float ry = std::max(2.0f, rx * 0.22f);
                    for (int index = 0; index < 64; ++index)
                    {
                        const std::uint32_t mixed = Noise(prop.Seed * 7u + static_cast<std::uint32_t>(index) * 131u);
                        const float angle = Unit(mixed) * 6.2831853f;
                        const float reach = std::pow(Unit(mixed >> 16), 0.8f);
                        const float x = std::round(prop.X + std::cos(angle) * reach * rx);
                        const float y = std::round(prop.Ground + std::sin(angle) * reach * ry);
                        if (y < first || y >= shore)
                            continue;
                        const Stuff stuff = (mixed >> 8) % 3u == 0u ? second : fallen;
                        const ImVec4 leaf = hazed(kRamps[static_cast<int>(stuff)][1 + static_cast<int>((mixed >> 10) % 2u)], y);
                        paint.Fill(x, y, x + (depthAt(y) < 0.35f ? 1.0f : 2.0f), y + 1.0f, leaf);
                    }
                }
            }

            void PaintLake(const Frame& frame)
            {
                const Canvas& paint = frame.Paint;
                const float shore = frame.Shore;
                const auto bottom = static_cast<float>(frame.Rows);
                const auto columns = static_cast<float>(frame.Columns);
                const ImVec4 edge = Lighted(kWaterEdge, frame);
                const ImVec4 bank = Lighted(kWaterBank, frame);
                const ImVec4 mud = Lighted(Colour(0.280f, 0.170f, 0.080f), frame);
                const auto topAt = [&](int column) { return std::round(shore + std::fabs(std::sin(static_cast<float>(column) * 0.11f + 2.0f)) * 1.5f); };
                int start = 0;
                float top = topAt(0);
                for (int column = 1; column <= frame.Columns; ++column)
                {
                    const float next = column < frame.Columns ? topAt(column) : -1.0e6f;
                    if (next == top)
                        continue;
                    paint.Fill(static_cast<float>(start), top, static_cast<float>(column), shore + 5.0f, bank);
                    paint.Fill(static_cast<float>(start), top, static_cast<float>(column), top + 1.0f, mud);
                    paint.Fill(static_cast<float>(start), top + 1.0f, static_cast<float>(column), top + 2.0f, edge);
                    start = column;
                    top = next;
                }
                constexpr int kBands = 5;
                const float first = shore + 5.0f;
                for (int band = 0; band < kBands; ++band)
                {
                    const float from = std::round(first + (bottom - first) * static_cast<float>(band) / kBands);
                    const float to = band == kBands - 1 ? bottom : std::round(first + (bottom - first) * static_cast<float>(band + 1) / kBands);
                    paint.Fill(0.0f, from, columns, to, Lighted(Mix(kWaterTop, kWaterDeep, static_cast<float>(band) / (kBands - 1)), frame));
                }
                const ImVec4 streak = Mix(Lighted(kWaterTop, frame), SkyAt(frame, 0.8f), 0.3f);
                const auto rows = static_cast<int>(bottom - first) - 2;
                for (int index = 0; rows > 0 && index < frame.Columns / 6; ++index)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 701u + 19u);
                    const auto x = static_cast<float>(mixed % static_cast<std::uint32_t>(frame.Columns));
                    const float y = first + static_cast<float>((mixed >> 12) % static_cast<std::uint32_t>(rows));
                    const auto length = static_cast<float>(3u + (mixed >> 20) % 7u);
                    paint.Fill(x, y, x + length, y + 1.0f, streak);
                }
                const auto glitter = [&](float x, ImVec4 colour, std::uint32_t salt)
                    {
                        if (colour.w <= 0.01f)
                            return;
                        for (int row = static_cast<int>(shore) + 4; row < frame.Rows; row += 2)
                        {
                            const std::uint32_t mixed = Hash(row, 3, salt);
                            const float nearness = (static_cast<float>(row) - shore) / std::max(1.0f, bottom - shore);
                            const float width = std::round(1.0f + 4.0f * nearness * Unit(mixed) + static_cast<float>(mixed % 2u));
                            const auto beat = static_cast<int>(std::floor(frame.Time * 1.2 + static_cast<double>((mixed >> 8) % 7u)));
                            const float shift = static_cast<float>((static_cast<int>((mixed >> 12) % 5u) + beat) % 5 - 2);
                            paint.Fill(x + shift - std::floor(width * 0.5f), static_cast<float>(row), x + shift + std::ceil(width * 0.5f), static_cast<float>(row) + 1.0f, colour);
                        }
                    };
                glitter(std::round(columns * 0.2f), ImVec4(1.0f, 0.86f, 0.52f, 0.7f * frame.Daylight), 5u);
                glitter(std::round(columns * 0.8f), ImVec4(0.72f, 0.78f, 0.88f, 0.55f * (1.0f - frame.Daylight)), 9u);
                for (int index = 0; rows > 0 && index < frame.Columns / 14; ++index)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 911u + 3u);
                    const auto x = static_cast<float>(4u + mixed % static_cast<std::uint32_t>(std::max(1, frame.Columns - 8)));
                    const float y = first + static_cast<float>((mixed >> 12) % static_cast<std::uint32_t>(rows));
                    const ImVec4 leaf = Lighted(kFallen[(mixed >> 24) % std::size(kFallen)], frame);
                    paint.Fill(x, y, x + 2.0f, y + 1.0f, leaf);
                    if ((mixed >> 8) % 3u == 0u)
                        paint.Fill(x + 2.0f, y - 1.0f, x + 3.0f, y, leaf);
                }
            }

            std::vector<Prop> m_props;
            Layout m_layout;
            std::vector<Drawing> m_drawings;
            ImVec4 m_tones[kInks] = {};
            float m_tonesDaylight = -1.0f;
            bool m_tonesMoon = false;
        };
    }

    std::unique_ptr<Scene> MakeAutumn()
    {
        return std::make_unique<AutumnScene>();
    }
}
