#include "UI/HomeScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Idler::UI::HomeScene
{
    namespace
    {
        constexpr ImVec4 Colour(float red, float green, float blue)
        {
            return ImVec4(red, green, blue, 1.0f);
        }

        struct Pair
        {
            ImVec4 Day;
            ImVec4 Night;
        };

        constexpr Pair kSkyTop{ Colour(0.290f, 0.450f, 0.650f), Colour(0.030f, 0.040f, 0.090f) };
        constexpr Pair kSkyLow{ Colour(0.760f, 0.835f, 0.905f), Colour(0.100f, 0.135f, 0.230f) };
        constexpr ImVec4 kDuskLow = Colour(0.900f, 0.640f, 0.560f);
        constexpr ImVec4 kDuskTop = Colour(0.300f, 0.300f, 0.500f);
        constexpr Pair kHaze{ Colour(0.680f, 0.760f, 0.860f), Colour(0.105f, 0.140f, 0.240f) };
        constexpr Pair kRockLit{ Colour(0.430f, 0.510f, 0.650f), Colour(0.085f, 0.105f, 0.175f) };
        constexpr Pair kRockShade{ Colour(0.340f, 0.420f, 0.570f), Colour(0.060f, 0.075f, 0.135f) };
        constexpr Pair kPeakLit{ Colour(0.930f, 0.955f, 0.985f), Colour(0.300f, 0.360f, 0.500f) };
        constexpr Pair kPeakShade{ Colour(0.660f, 0.740f, 0.870f), Colour(0.130f, 0.165f, 0.270f) };
        constexpr Pair kHillLit{ Colour(0.860f, 0.900f, 0.950f), Colour(0.240f, 0.290f, 0.410f) };
        constexpr Pair kHill{ Colour(0.760f, 0.820f, 0.900f), Colour(0.155f, 0.195f, 0.300f) };
        constexpr Pair kHillShade{ Colour(0.620f, 0.690f, 0.820f), Colour(0.110f, 0.140f, 0.235f) };
        constexpr Pair kForest{ Colour(0.200f, 0.330f, 0.380f), Colour(0.035f, 0.060f, 0.090f) };
        constexpr Pair kSnow{ Colour(0.715f, 0.775f, 0.870f), Colour(0.165f, 0.210f, 0.320f) };
        constexpr Pair kSnowLit{ Colour(0.940f, 0.960f, 0.990f), Colour(0.320f, 0.385f, 0.530f) };
        constexpr Pair kSnowShade{ Colour(0.580f, 0.650f, 0.800f), Colour(0.115f, 0.150f, 0.255f) };
        constexpr Pair kSnowDeep{ Colour(0.450f, 0.530f, 0.715f), Colour(0.080f, 0.105f, 0.195f) };
        constexpr Pair kGlint{ Colour(1.000f, 1.000f, 1.000f), Colour(0.600f, 0.680f, 0.840f) };
        constexpr Pair kNeedle{ Colour(0.120f, 0.290f, 0.270f), Colour(0.050f, 0.100f, 0.120f) };
        constexpr Pair kNeedleLit{ Colour(0.210f, 0.420f, 0.350f), Colour(0.090f, 0.165f, 0.190f) };
        constexpr Pair kNeedleDark{ Colour(0.065f, 0.175f, 0.190f), Colour(0.030f, 0.060f, 0.085f) };
        constexpr Pair kBark{ Colour(0.330f, 0.270f, 0.250f), Colour(0.110f, 0.105f, 0.125f) };
        constexpr Pair kBarkLit{ Colour(0.480f, 0.400f, 0.350f), Colour(0.180f, 0.180f, 0.220f) };
        constexpr Pair kBarkDark{ Colour(0.200f, 0.160f, 0.170f), Colour(0.060f, 0.060f, 0.080f) };
        constexpr Pair kLeaf{ Colour(0.680f, 0.420f, 0.200f), Colour(0.170f, 0.120f, 0.100f) };
        constexpr Pair kWood{ Colour(0.440f, 0.360f, 0.300f), Colour(0.130f, 0.120f, 0.135f) };
        constexpr Pair kWoodLit{ Colour(0.580f, 0.480f, 0.380f), Colour(0.200f, 0.190f, 0.215f) };
        constexpr Pair kWoodDark{ Colour(0.270f, 0.215f, 0.200f), Colour(0.075f, 0.070f, 0.090f) };
        constexpr Pair kStone{ Colour(0.480f, 0.500f, 0.540f), Colour(0.140f, 0.160f, 0.215f) };
        constexpr Pair kStoneLit{ Colour(0.640f, 0.650f, 0.680f), Colour(0.230f, 0.260f, 0.340f) };
        constexpr Pair kStoneDark{ Colour(0.330f, 0.360f, 0.440f), Colour(0.085f, 0.095f, 0.150f) };
        constexpr Pair kLichen{ Colour(0.600f, 0.620f, 0.380f), Colour(0.130f, 0.140f, 0.130f) };
        constexpr Pair kStalk{ Colour(0.740f, 0.630f, 0.400f), Colour(0.190f, 0.180f, 0.170f) };
        constexpr Pair kStalkDark{ Colour(0.520f, 0.420f, 0.260f), Colour(0.120f, 0.110f, 0.110f) };
        constexpr Pair kBerry{ Colour(0.780f, 0.150f, 0.170f), Colour(0.220f, 0.060f, 0.090f) };
        constexpr Pair kWater{ Colour(0.170f, 0.360f, 0.490f), Colour(0.055f, 0.100f, 0.165f) };
        constexpr Pair kWaterMid{ Colour(0.125f, 0.295f, 0.425f), Colour(0.045f, 0.085f, 0.145f) };
        constexpr Pair kWaterDeep{ Colour(0.085f, 0.215f, 0.335f), Colour(0.032f, 0.065f, 0.115f) };
        constexpr Pair kRipple{ Colour(0.400f, 0.590f, 0.710f), Colour(0.110f, 0.170f, 0.260f) };
        constexpr Pair kIce{ Colour(0.760f, 0.880f, 0.945f), Colour(0.210f, 0.290f, 0.400f) };
        constexpr Pair kIceShade{ Colour(0.500f, 0.680f, 0.800f), Colour(0.120f, 0.185f, 0.280f) };

        struct Tones
        {
            float Twilight = 0.0f;
            ImVec4 SkyTop, SkyLow, Haze;
            ImVec4 RockLit, RockShade, PeakLit, PeakShade;
            ImVec4 HillLit, Hill, HillShade, Forest;
            ImVec4 Snow, SnowLit, SnowShade, SnowDeep, Glint;
            ImVec4 Needle, NeedleLit, NeedleDark;
            ImVec4 Bark, BarkLit, BarkDark, Leaf, LeafDark;
            ImVec4 Wood, WoodLit, WoodDark;
            ImVec4 Stone, StoneLit, StoneDark, Lichen;
            ImVec4 Stalk, StalkDark, Berry;
            ImVec4 Water, WaterMid, WaterDeep, Ripple, Ice, IceShade, Glade, Halo;
        };

        ImVec4 Warm(ImVec4 colour, float share)
        {
            return ImVec4(std::min(1.0f, colour.x * (1.0f + 0.22f * share)), colour.y * (1.0f - 0.03f * share), colour.z * (1.0f - 0.2f * share), colour.w);
        }

        Tones TonesOf(const Frame& frame)
        {
            const float day = std::clamp(frame.Daylight, 0.0f, 1.0f);
            Tones tones;
            tones.Twilight = std::clamp(1.0f - std::abs(day - 0.5f) * 2.8f, 0.0f, 1.0f);
            const float warm = frame.Sun.Moon ? tones.Twilight * 0.3f : tones.Twilight;
            const auto at = [&](const Pair& pair) { return DayNight(pair.Day, pair.Night, day); };
            const auto lit = [&](const Pair& pair) { return Warm(at(pair), warm); };
            tones.SkyTop = Mix(at(kSkyTop), kDuskTop, tones.Twilight * 0.5f);
            tones.SkyLow = Mix(at(kSkyLow), kDuskLow, tones.Twilight * 0.65f);
            tones.Haze = Mix(at(kHaze), tones.SkyLow, 0.35f);
            tones.RockLit = lit(kRockLit);
            tones.RockShade = at(kRockShade);
            tones.PeakLit = lit(kPeakLit);
            tones.PeakShade = at(kPeakShade);
            tones.HillLit = lit(kHillLit);
            tones.Hill = at(kHill);
            tones.HillShade = at(kHillShade);
            tones.Forest = at(kForest);
            tones.Snow = Warm(at(kSnow), warm * 0.45f);
            tones.SnowLit = lit(kSnowLit);
            tones.SnowShade = at(kSnowShade);
            tones.SnowDeep = at(kSnowDeep);
            tones.Glint = lit(kGlint);
            tones.Needle = at(kNeedle);
            tones.NeedleLit = lit(kNeedleLit);
            tones.NeedleDark = at(kNeedleDark);
            tones.Bark = at(kBark);
            tones.BarkLit = lit(kBarkLit);
            tones.BarkDark = at(kBarkDark);
            tones.Leaf = lit(kLeaf);
            tones.LeafDark = Mix(tones.Leaf, tones.BarkDark, 0.45f);
            tones.Wood = at(kWood);
            tones.WoodLit = lit(kWoodLit);
            tones.WoodDark = at(kWoodDark);
            tones.Stone = at(kStone);
            tones.StoneLit = lit(kStoneLit);
            tones.StoneDark = at(kStoneDark);
            tones.Lichen = at(kLichen);
            tones.Stalk = lit(kStalk);
            tones.StalkDark = at(kStalkDark);
            tones.Berry = at(kBerry);
            tones.Water = at(kWater);
            tones.WaterMid = at(kWaterMid);
            tones.WaterDeep = at(kWaterDeep);
            tones.Ripple = at(kRipple);
            tones.Ice = lit(kIce);
            tones.IceShade = at(kIceShade);
            tones.Glade = frame.Sun.Moon ? Colour(0.520f, 0.580f, 0.700f) : Warm(Colour(0.980f, 0.950f, 0.800f), tones.Twilight);
            tones.Halo = frame.Sun.Moon ? ImVec4(0.700f, 0.780f, 0.950f, 0.028f) : ImVec4(1.000f, 0.960f, 0.820f, 0.055f);
            return tones;
        }

        void Box(const Canvas& paint, float left, float top, float right, float bottom, ImVec4 colour)
        {
            if (right > left && bottom > top)
                paint.Fill(left, top, right, bottom, colour);
        }

        void SplitRow(const Canvas& paint, float left, float right, float y, float split, float toward, ImVec4 lit, ImVec4 shade)
        {
            split = std::clamp(std::round(split), left, right);
            Box(paint, left, y, split, y + 1.0f, toward < 0.0f ? lit : shade);
            Box(paint, split, y, right, y + 1.0f, toward < 0.0f ? shade : lit);
        }

        void ShadedRow(const Canvas& paint, float left, float right, float y, float toward, float litCells, float darkCells, ImVec4 lit, ImVec4 middle, ImVec4 dark)
        {
            litCells = std::min(litCells, right - left);
            darkCells = std::min(darkCells, right - left - litCells);
            const float litLeft = toward < 0.0f ? left : right - litCells;
            const float darkLeft = toward < 0.0f ? right - darkCells : left;
            const float middleLeft = toward < 0.0f ? left + litCells : left + darkCells;
            const float middleRight = toward < 0.0f ? right - darkCells : right - litCells;
            Box(paint, litLeft, y, litLeft + litCells, y + 1.0f, lit);
            Box(paint, middleLeft, y, middleRight, y + 1.0f, middle);
            Box(paint, darkLeft, y, darkLeft + darkCells, y + 1.0f, dark);
        }

        template <typename Draw>
        void ScatterCells(int columns, int rows, int cellWidth, int cellHeight, std::uint32_t salt, std::uint32_t share, Draw&& draw)
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

        ImVec4 SkyAt(const Tones& tones, float share)
        {
            return Mix(tones.SkyTop, tones.SkyLow, share * share);
        }

        void PaintSky(const Frame& frame, const Tones& tones)
        {
            constexpr std::array<float, 10> kEdges{ 0.0f, 0.26f, 0.44f, 0.57f, 0.67f, 0.75f, 0.82f, 0.88f, 0.93f, 0.97f };
            const Canvas& paint = frame.Paint;
            const auto width = static_cast<float>(frame.Columns);
            const float horizon = frame.Surface;
            for (std::size_t band = 0; band < kEdges.size(); ++band)
            {
                const bool last = band + 1 == kEdges.size();
                const float from = std::round(kEdges[band] * horizon);
                const float to = last ? horizon + 4.0f : std::round(kEdges[band + 1] * horizon);
                const float middle = last ? 1.0f : (kEdges[band] + kEdges[band + 1]) * 0.5f;
                Box(paint, 0.0f, from, width, to, SkyAt(tones, middle));
                if (band == 0)
                    continue;
                const ImVec4 above = SkyAt(tones, (kEdges[band - 1] + kEdges[band]) * 0.5f);
                for (int column = static_cast<int>(band % 2u) * 2; column < frame.Columns; column += 4)
                    Box(paint, static_cast<float>(column), from, static_cast<float>(column + 2), from + 1.0f, above);
            }
            DrawDisc(paint, frame.Sun.X, frame.Sun.Y, 19.0f, tones.Halo);
            DrawDisc(paint, frame.Sun.X, frame.Sun.Y, 13.0f, tones.Halo);
            DrawSky(frame, SkyAt(tones, 0.3f));
        }

        constexpr int kPeakSpacing = 34;

        struct Peak
        {
            float X = 0.0f;
            float Height = 0.0f;
            float Slope = 1.0f;
        };

        Peak PeakOf(int index)
        {
            const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 7121u + 977u);
            Peak peak;
            peak.X = static_cast<float>(index * kPeakSpacing) + static_cast<float>(mixed % 22u);
            peak.Height = (mixed >> 8) % 3u == 0u ? 30.0f + static_cast<float>((mixed >> 12) % 12u) : 17.0f + static_cast<float>((mixed >> 12) % 12u);
            peak.Slope = 0.68f + static_cast<float>((mixed >> 20) % 40u) * 0.01f;
            return peak;
        }

        float RidgeAt(int column, Peak& owner)
        {
            const int closest = column / kPeakSpacing;
            float best = 0.0f;
            for (int index = closest - 2; index <= closest + 2; ++index)
            {
                const Peak peak = PeakOf(index);
                const float height = peak.Height - std::abs(static_cast<float>(column) - peak.X) * peak.Slope;
                if (height > best)
                {
                    best = height;
                    owner = peak;
                }
            }
            return best;
        }

        void PaintRange(const Frame& frame, const Tones& tones, bool fromLeft)
        {
            Canvas distant = frame.Paint;
            distant.Haze = 0.32f;
            distant.HazeColor = tones.SkyLow;
            const float bottom = frame.Surface + 2.0f;
            for (int column = 0; column < frame.Columns; ++column)
            {
                Peak peak;
                const float height = std::max(3.0f, RidgeAt(column, peak));
                const auto x = static_cast<float>(column);
                const float top = std::round(frame.Surface - height);
                const bool lit = fromLeft ? x <= peak.X : x >= peak.X;
                Box(distant, x, top, x + 1.0f, bottom, lit ? tones.PeakLit : tones.PeakShade);
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(column) * 389u + 11u);
                if (height > 10.0f && mixed % (lit ? 7u : 3u) == 0u)
                {
                    const float from = top + 2.0f + static_cast<float>((mixed >> 8) % 4u);
                    const float length = 3.0f + static_cast<float>((mixed >> 12) % 3u) * height * 0.12f;
                    Box(distant, x, from, x + 1.0f, std::min(bottom, std::round(from + length)), lit ? tones.RockLit : tones.RockShade);
                }
            }
        }

        float HillAt(float x)
        {
            return 8.0f + 4.5f * std::sin(x * 0.029f + 1.3f) + 2.5f * std::sin(x * 0.071f + 0.4f) + 1.2f * std::sin(x * 0.17f);
        }

        void PaintHills(const Frame& frame, const Tones& tones, bool fromLeft)
        {
            Canvas distant = frame.Paint;
            distant.Haze = 0.3f;
            distant.HazeColor = tones.Haze;
            const float bottom = frame.Surface + 2.0f;
            const float away = fromLeft ? 1.0f : -1.0f;
            const std::array<ImVec4, 3> faces{ tones.HillLit, tones.Hill, tones.HillShade };
            const auto faceAt = [&](float x)
                {
                    const float rise = (HillAt(x + away) - HillAt(x - away)) * 0.5f;
                    return rise > 0.12f ? 0 : rise < -0.12f ? 2 : 1;
                };
            int start = 0;
            float runTop = std::round(frame.Surface - HillAt(0.0f));
            int runFace = faceAt(0.0f);
            for (int column = 1; column <= frame.Columns; ++column)
            {
                const auto x = static_cast<float>(column);
                const bool end = column == frame.Columns;
                const float top = end ? -1.0f : std::round(frame.Surface - HillAt(x));
                const int face = end ? -1 : faceAt(x);
                if (top == runTop && face == runFace)
                    continue;
                Box(distant, static_cast<float>(start), runTop, x, bottom, faces[static_cast<std::size_t>(runFace)]);
                start = column;
                runTop = top;
                runFace = face;
            }

            constexpr int kSpacing = 17;
            for (int patch = 0; patch * kSpacing < frame.Columns + kSpacing; ++patch)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(patch) * 389u + 71u);
                if (mixed % 3u == 0u)
                    continue;
                const int count = 2 + static_cast<int>((mixed >> 4) % 5u);
                const int first = patch * kSpacing + static_cast<int>((mixed >> 8) % 8u);
                for (int tree = 0; tree < count; ++tree)
                {
                    const int column = first + tree * 3 + static_cast<int>((mixed >> (12 + tree)) & 1u);
                    const auto x = static_cast<float>(column);
                    const float base = std::round(frame.Surface - HillAt(x)) + 2.0f + static_cast<float>((mixed >> (18 + tree)) & 3u);
                    const float tall = 4.0f + static_cast<float>(Noise(static_cast<std::uint32_t>(column) * 31u) % 4u);
                    Box(distant, x, base - tall, x + 1.0f, base, tones.Forest);
                    Box(distant, x - 1.0f, base - tall + 2.0f, x + 2.0f, base, tones.Forest);
                    if (tall >= 6.0f)
                        Box(distant, x - 2.0f, base - 2.0f, x + 3.0f, base, tones.Forest);
                    Box(distant, x, base - tall, x + 1.0f, base - tall + 1.0f, tones.HillLit);
                    Box(distant, fromLeft ? x - 1.0f : x + 1.0f, base - tall + 2.0f, fromLeft ? x : x + 2.0f, base - tall + 3.0f, tones.HillLit);
                }
            }
        }

        void PaintSmallFir(const Canvas& paint, const Tones& tones, float toward, float x, float base, int height)
        {
            Box(paint, x, base - 3.0f, x + 1.0f, base, tones.BarkDark);
            constexpr std::array<float, 4> kStarts{ 0.0f, 0.27f, 0.57f, 1.0f };
            constexpr std::array<float, 3> kTop{ 0.0f, 0.8f, 1.4f };
            constexpr std::array<float, 3> kBottom{ 1.2f, 2.3f, 3.4f };
            const int crown = height - 2;
            const float scale = static_cast<float>(height) / 12.0f;
            std::size_t tier = 0;
            for (int row = 0; row < crown; ++row)
            {
                while (tier < 2 && static_cast<float>(row) >= std::round(kStarts[tier + 1] * static_cast<float>(crown)))
                    ++tier;
                const float start = std::round(kStarts[tier] * static_cast<float>(crown));
                const float end = std::round(kStarts[tier + 1] * static_cast<float>(crown));
                const float local = (static_cast<float>(row) - start) / std::max(1.0f, end - start - 1.0f);
                const float half = std::round((kTop[tier] + (kBottom[tier] - kTop[tier]) * local) * scale);
                const float y = base - 2.0f - static_cast<float>(crown - row);
                const float left = x - half;
                const float right = x + half + 1.0f;
                if (static_cast<float>(row) == start || row == 1)
                    SplitRow(paint, left, right, y, toward < 0.0f ? x + 1.0f : x, toward, tones.SnowLit, tones.SnowShade);
                else
                    ShadedRow(paint, left, right, y, toward, 1.0f, half >= 2.0f ? 1.0f : 0.0f, tones.NeedleLit, tones.Needle, tones.NeedleDark);
            }
        }

        void PaintTwigTree(const Canvas& paint, const Tones& tones, float x, float base, int height, std::uint32_t seed)
        {
            const auto tall = static_cast<float>(height);
            Box(paint, x, base - tall, x + 1.0f, base, tones.Bark);
            for (int twig = 0; twig < 4; ++twig)
            {
                const float y = base - tall + 2.0f + static_cast<float>(twig * 2);
                const float side = ((seed >> twig) & 1u) != 0u ? 1.0f : -1.0f;
                const float reach = 2.0f + static_cast<float>((seed >> (8 + twig * 2)) % 3u);
                Box(paint, side < 0.0f ? x - reach : x + 1.0f, y, side < 0.0f ? x : x + 1.0f + reach, y + 1.0f, tones.Bark);
                Box(paint, side < 0.0f ? x - reach : x + reach, y - 1.0f, side < 0.0f ? x - reach + 1.0f : x + reach + 1.0f, y, tones.Bark);
                Box(paint, side < 0.0f ? x - reach + 1.0f : x + 1.0f, y - 1.0f, side < 0.0f ? x : x + reach, y, tones.SnowLit);
            }
        }

        void PaintTreeline(const Frame& frame, const Tones& tones, bool fromLeft)
        {
            Canvas edge = frame.Paint;
            edge.Haze = 0.42f;
            edge.HazeColor = tones.Haze;
            const float toward = fromLeft ? -1.0f : 1.0f;
            const float base = frame.Surface + 2.0f;
            constexpr int kSpacing = 9;
            for (int slot = 0; slot * kSpacing < frame.Columns + kSpacing; ++slot)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(slot) * 977u + 11u);
                if (mixed % 5u < 2u)
                    continue;
                const auto x = static_cast<float>(slot * kSpacing + static_cast<int>((mixed >> 4) % 7u));
                const int height = 9 + static_cast<int>((mixed >> 8) % 9u);
                if ((mixed >> 20) % 6u == 0u)
                    PaintTwigTree(edge, tones, x, base, height + 2, mixed);
                else
                    PaintSmallFir(edge, tones, toward, x, base, height);
            }
        }

        float RimAt(int column)
        {
            const auto x = static_cast<float>(column);
            return 1.3f * std::sin(x * 0.12f) + 0.7f * std::sin(x * 0.31f + 1.0f);
        }

        void PaintLand(const Frame& frame, const Tones& tones, bool fromLeft)
        {
            const Canvas& paint = frame.Paint;
            const float back = frame.Surface;
            const float shore = frame.Shore;
            const auto width = static_cast<float>(frame.Columns);
            const float body = back + 3.0f;
            const auto hazed = [&](ImVec4 colour, float y) { return Mix(colour, tones.Haze, frame.HazeAt(y)); };

            int start = 0;
            float runTop = std::round(back + RimAt(0));
            for (int column = 1; column <= frame.Columns; ++column)
            {
                const float top = column < frame.Columns ? std::round(back + RimAt(column)) : -1.0f;
                if (top == runTop)
                    continue;
                const auto from = static_cast<float>(start);
                const auto to = static_cast<float>(column);
                Box(paint, from, runTop, to, runTop + 1.0f, hazed(tones.SnowLit, runTop));
                Box(paint, from, runTop + 1.0f, to, body, hazed(tones.Snow, runTop + 1.0f));
                start = column;
                runTop = top;
            }
            for (float y = body; y < shore + 3.0f; y += 3.0f)
            {
                const bool clear = frame.HazeAt(y) <= 0.0f;
                Box(paint, 0.0f, y, width, clear ? shore + 3.0f : std::min(y + 3.0f, shore + 3.0f), hazed(tones.Snow, y + 1.0f));
                if (clear)
                    break;
            }

            const int land = static_cast<int>(shore - body) - 3;
            const float away = fromLeft ? 1.0f : -1.0f;
            const auto part = [&](float left, float length, float from, float to, float row, ImVec4 colour)
                {
                    const float a = away > 0.0f ? from : 1.0f - to;
                    const float b = away > 0.0f ? to : 1.0f - from;
                    Box(paint, std::round(left + length * a), row, std::round(left + length * b), row + 1.0f, hazed(colour, row));
                };
            constexpr int kSpacing = 47;
            for (int place = 0; place * kSpacing < frame.Columns; ++place)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(place) * 4243u + 101u);
                const float depth = 0.12f + static_cast<float>((mixed >> 4) % 80u) * 0.01f;
                const float row = std::round(body + (shore - 4.0f - body) * depth);
                const float scale = frame.ScaleAt(row);
                const float length = std::round((16.0f + static_cast<float>((mixed >> 12) % 14u)) * scale * scale);
                const float left = static_cast<float>(place * kSpacing) + static_cast<float>(mixed % 20u);
                if ((mixed >> 20) % 3u != 0u)
                {
                    part(left, length, 0.3f, 0.7f, row, tones.SnowLit);
                    part(left, length, 0.08f, 0.55f, row + 1.0f, tones.SnowLit);
                    part(left, length, 0.55f, 0.92f, row + 1.0f, tones.SnowShade);
                    part(left, length, 0.6f, 1.0f, row + 2.0f, tones.SnowShade);
                }
                else
                {
                    part(left, length, 0.15f, 0.85f, row, tones.SnowShade);
                    part(left, length, 0.0f, 1.0f, row + 1.0f, tones.SnowShade);
                    part(left, length, 0.08f, 0.5f, row + 1.0f, tones.SnowDeep);
                    part(left, length, 0.45f, 0.95f, row + 2.0f, tones.SnowLit);
                }
            }
            ScatterCells(frame.Columns - 8, land, 19, 7, 41u, 70u, [&](float x, float y, std::uint32_t mixed)
                {
                    const float row = body + y;
                    const float scale = frame.ScaleAt(row);
                    const float length = std::max(3.0f, std::round((4.0f + static_cast<float>(mixed % 6u)) * scale * scale));
                    const float left = x + 4.0f;
                    Box(paint, left, row, left + length, row + 1.0f, hazed(tones.SnowLit, row));
                    const float shadeLeft = left + (away > 0.0f ? 1.0f : -1.0f);
                    Box(paint, shadeLeft, row + 1.0f, shadeLeft + length, row + 2.0f, hazed(tones.SnowShade, row));
                });
            for (int trail = 0; trail < 2; ++trail)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(trail) * 5113u + 29u);
                const float from = std::round(width * (trail == 0 ? 0.49f : 0.17f));
                const float to = from + width * (trail == 0 ? -0.035f : 0.03f);
                const float wander = 2.0f + static_cast<float>(mixed % 3u);
                const float end = shore - 3.0f;
                int print = 0;
                for (float row = body + 2.0f; row < end; ++print)
                {
                    const float share = (row - body) / std::max(1.0f, end - body);
                    const float scale = frame.ScaleAt(row);
                    const float x = std::round(from + (to - from) * share + wander * std::sin(share * 5.0f + static_cast<float>(trail) * 2.0f));
                    const float side = std::round((print % 2 == 0 ? -1.2f : 1.2f) * scale);
                    const float size = std::max(1.0f, std::round(1.6f * scale));
                    Box(paint, x + side, row, x + side + size, row + 1.0f, hazed(tones.SnowDeep, row));
                    Box(paint, x + side + (away > 0.0f ? size : -1.0f), row, x + side + (away > 0.0f ? size + 1.0f : 0.0f), row + 1.0f, hazed(tones.SnowLit, row));
                    row += std::max(2.0f, std::round(2.6f * scale));
                }
            }
            ScatterCells(frame.Columns - 4, land, 13, 5, 59u, 90u, [&](float x, float y, std::uint32_t mixed)
                {
                    const double phase = static_cast<double>(mixed & 1023u) * 0.37;
                    if (std::sin(frame.Time * 1.3 + phase) < 0.8)
                        return;
                    const float row = body + y;
                    Box(paint, x + 2.0f, row, x + 3.0f, row + 1.0f, hazed(tones.Glint, row));
                });
        }

        float WaterTopAt(int column, float shore)
        {
            return shore + std::round(std::abs(std::sin(static_cast<float>(column) * 0.11f + 2.0f)) * 1.5f);
        }

        int IceAt(int column)
        {
            const std::uint32_t block = Noise(static_cast<std::uint32_t>(column / 6) * 131u + 7u);
            int thick = block % 7u == 0u ? 0 : 1 + static_cast<int>((block >> 4) % 3u);
            if (column % 6 == 5 && thick > 0 && (block >> 8) % 2u == 0u)
                thick -= 1;
            return thick;
        }

        void PaintLake(const Frame& frame, const Tones& tones)
        {
            const Canvas& paint = frame.Paint;
            const float shore = frame.Shore;
            const auto width = static_cast<float>(frame.Columns);
            const auto bottom = static_cast<float>(frame.Rows) + 1.0f;
            const float depth = bottom - shore;
            const float upper = shore + 4.0f;
            const float midFrom = std::round(shore + depth * 0.4f);
            const float deepFrom = std::round(shore + depth * 0.7f);

            int start = 0;
            float runTop = WaterTopAt(0, shore);
            int runIce = IceAt(0);
            for (int column = 1; column <= frame.Columns; ++column)
            {
                const bool end = column == frame.Columns;
                const float top = end ? -1.0f : WaterTopAt(column, shore);
                const int ice = end ? -1 : IceAt(column);
                if (top == runTop && ice == runIce)
                    continue;
                const auto from = static_cast<float>(start);
                const auto to = static_cast<float>(column);
                const auto thick = static_cast<float>(runIce);
                Box(paint, from, runTop - 1.0f, to, runTop, tones.SnowShade);
                Box(paint, from, runTop + thick, to, upper, tones.Water);
                if (runIce > 0)
                {
                    Box(paint, from, runTop, to, runTop + thick, tones.Ice);
                    Box(paint, from, runTop + thick, to, runTop + thick + 1.0f, tones.IceShade);
                }
                start = column;
                runTop = top;
                runIce = ice;
            }
            Box(paint, 0.0f, upper, width, midFrom, tones.Water);
            Box(paint, 0.0f, midFrom, width, deepFrom, tones.WaterMid);
            Box(paint, 0.0f, deepFrom, width, bottom, tones.WaterDeep);
            for (int column = 0; column < frame.Columns; column += 4)
            {
                const auto x = static_cast<float>(column);
                Box(paint, x, midFrom, x + 2.0f, midFrom + 1.0f, tones.Water);
                Box(paint, x + 2.0f, deepFrom, x + 4.0f, deepFrom + 1.0f, tones.WaterMid);
            }

            ScatterCells(frame.Columns - 12, 5, 23, 5, 83u, 100u, [&](float x, float y, std::uint32_t mixed)
                {
                    const float row = shore + 5.0f + y;
                    const float length = 3.0f + static_cast<float>(mixed % 5u);
                    Box(paint, x + 4.0f, row, x + 4.0f + length, row + 1.0f, tones.Ice);
                    Box(paint, x + 5.0f, row + 1.0f, x + 3.0f + length, row + 2.0f, tones.IceShade);
                });
            ScatterCells(frame.Columns - 8, static_cast<int>(bottom - shore) - 12, 11, 4, 29u, 110u, [&](float x, float y, std::uint32_t mixed)
                {
                    const float row = shore + 9.0f + y;
                    const float length = 2.0f + static_cast<float>(mixed % 4u);
                    Box(paint, x + 3.0f, row, x + 3.0f + length, row + 1.0f, row < midFrom ? tones.Ripple : Mix(tones.Ripple, tones.WaterMid, 0.45f));
                });
            const float under = std::round(frame.Sun.X);
            int index = 0;
            for (float row = shore + 6.0f; row < bottom - 1.0f; row += 2.0f, ++index)
            {
                const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(index) * 613u + 3u);
                const float share = (row - shore) / std::max(1.0f, depth);
                const float half = 1.0f + std::floor((row - shore) * 0.12f) + static_cast<float>(mixed % 2u);
                const float left = under + static_cast<float>((mixed >> 4) % 3u) - 1.0f - half;
                ImVec4 glade = tones.Glade;
                glade.w = 0.6f - 0.3f * share;
                Box(paint, left, row, left + half * 2.0f, row + 1.0f, glade);
            }
        }

        enum class Kind : std::uint32_t
        {
            Spruce,
            Oak,
            Bush,
            Fence,
            Boulder,
            Saplings,
        };

        void PaintDrift(const Canvas& paint, const Tones& tones, float toward, float x, float half, int height)
        {
            for (int row = 0; row < height; ++row)
            {
                const float up = (static_cast<float>(height - row) - 0.5f) / static_cast<float>(height);
                const float reach = std::max(1.0f, std::round(half * std::sqrt(std::max(0.0f, 1.0f - up * up))));
                const auto y = static_cast<float>(row - height);
                if (row == 0)
                {
                    Box(paint, x - reach, y, x + reach, y + 1.0f, tones.SnowLit);
                    continue;
                }
                SplitRow(paint, x - reach, x + reach, y, x - toward * reach * 0.1f, toward, row * 2 < height ? tones.SnowLit : tones.Snow, tones.SnowShade);
                const float edge = toward < 0.0f ? x + reach - 1.0f : x - reach;
                Box(paint, edge, y, edge + 1.0f, y + 1.0f, tones.SnowDeep);
            }
        }

        void PaintSpruce(const Canvas& paint, const Tones& tones, float toward, float x, int height, int halfWidth, int tiers, std::uint32_t seed)
        {
            Box(paint, x - 2.0f, -4.0f, x + 2.0f, 0.0f, tones.Bark);
            Box(paint, toward < 0.0f ? x - 2.0f : x + 1.0f, -4.0f, toward < 0.0f ? x - 1.0f : x + 2.0f, 0.0f, tones.BarkLit);
            Box(paint, toward < 0.0f ? x + 1.0f : x - 2.0f, -4.0f, toward < 0.0f ? x + 2.0f : x - 1.0f, 0.0f, tones.BarkDark);

            tiers = std::clamp(tiers, 2, 8);
            const int crown = height - 3;
            std::array<int, 8> rows{};
            std::array<int, 8> tops{};
            float weights = 0.0f;
            for (int tier = 0; tier < tiers; ++tier)
                weights += 1.0f + 0.3f * static_cast<float>(tier);
            int used = 0;
            for (int tier = 0; tier < tiers; ++tier)
            {
                const auto at = static_cast<std::size_t>(tier);
                rows[at] = tier + 1 < tiers ? static_cast<int>(std::round(static_cast<float>(crown) * (1.0f + 0.3f * static_cast<float>(tier)) / weights)) : crown - used;
                tops[at] = -height + used;
                used += rows[at];
            }

            const auto sideOf = [&](float cell, ImVec4 lit, ImVec4 shade) { return (cell < x) == (toward < 0.0f) ? lit : shade; };
            for (int tier = tiers - 1; tier >= 0; --tier)
            {
                const auto at = static_cast<std::size_t>(tier);
                const int count = rows[at];
                const int top = tops[at];
                const float grown = std::pow(static_cast<float>(tier + 1) / static_cast<float>(tiers), 0.85f);
                const float wide = std::round(2.0f + static_cast<float>(halfWidth - 2) * grown);
                const float narrow = tier == 0 ? 1.0f : std::max(2.0f, std::round(wide * 0.5f));
                const int snowRows = count >= 7 ? 3 : 2;
                float half = narrow;
                for (int row = 0; row < count; ++row)
                {
                    const auto y = static_cast<float>(top + row);
                    half = std::round(narrow + (wide - narrow) * static_cast<float>(row) / static_cast<float>(std::max(1, count - 1)));
                    if (row < snowRows)
                        SplitRow(paint, x - half, x + half, y, x - toward * half * 0.3f, toward, tones.SnowLit, tones.SnowShade);
                    else
                        ShadedRow(paint, x - half, x + half, y, toward, std::max(1.0f, std::round(half * 0.4f)), std::max(1.0f, std::round(half * 0.45f)), tones.NeedleLit, tones.Needle, tones.NeedleDark);
                }
                const auto bottom = static_cast<float>(top + count - 1);
                const auto firstNeedles = static_cast<float>(top + snowRows);
                const std::uint32_t own = Noise(seed + static_cast<std::uint32_t>(tier) * 7919u);
                const auto halfAt = [&](int row) { return std::round(narrow + (wide - narrow) * static_cast<float>(row) / static_cast<float>(std::max(1, count - 1))); };
                if (count > snowRows + 1)
                {
                    const float reach = halfAt(snowRows);
                    const auto span = static_cast<std::uint32_t>(std::max(1.0f, reach * 2.0f - 1.0f));
                    for (int drip = 0; drip < 3; ++drip)
                    {
                        const float cell = x - reach + static_cast<float>((own >> (drip * 6)) % span);
                        Box(paint, cell, firstNeedles, cell + 1.0f, firstNeedles + 1.0f + static_cast<float>(drip % 2), sideOf(cell, tones.SnowLit, tones.SnowShade));
                    }
                }
                if (count >= 6 && wide >= 5.0f)
                {
                    const float reach = halfAt(snowRows + 2);
                    const float cell = x - reach + 1.0f + static_cast<float>((own >> 20) % static_cast<std::uint32_t>(std::max(1.0f, reach * 2.0f - 3.0f)));
                    const float y = firstNeedles + 2.0f;
                    Box(paint, cell, y, cell + 2.0f, y + 1.0f, sideOf(cell, tones.SnowLit, tones.SnowShade));
                    Box(paint, cell, y + 1.0f, cell + 2.0f, y + 2.0f, tones.NeedleDark);
                }
                for (float cell = x - half + 1.0f; cell < x + half - 1.0f; cell += 3.0f)
                    Box(paint, cell, bottom + 1.0f, cell + 1.0f, bottom + 2.0f, sideOf(cell, tones.Needle, tones.NeedleDark));
                if (tier > 0)
                {
                    Box(paint, x - half - 1.0f, bottom, x - half, bottom + 1.0f, toward < 0.0f ? tones.NeedleLit : tones.NeedleDark);
                    Box(paint, x + half, bottom, x + half + 1.0f, bottom + 1.0f, toward < 0.0f ? tones.NeedleDark : tones.NeedleLit);
                    Box(paint, x - half - 1.0f, bottom - 1.0f, x - half + 1.0f, bottom, toward < 0.0f ? tones.SnowLit : tones.SnowShade);
                    Box(paint, x + half - 1.0f, bottom - 1.0f, x + half + 1.0f, bottom, toward < 0.0f ? tones.SnowShade : tones.SnowLit);
                }
            }
            Box(paint, toward < 0.0f ? x - 1.0f : x, static_cast<float>(-height), toward < 0.0f ? x : x + 1.0f, static_cast<float>(1 - height), tones.Glint);
        }

        struct Limb
        {
            float X0, Y0, X1, Y1;
            int Thick;
        };

        constexpr Limb kOak[] = {
            { -1.0f, -16.0f, -9.0f, -25.0f, 3 }, { 0.0f, -16.0f, 2.0f, -30.0f, 3 }, { 1.0f, -16.0f, 10.0f, -23.0f, 3 },
            { -9.0f, -25.0f, -16.0f, -31.0f, 2 }, { -16.0f, -31.0f, -21.0f, -34.0f, 1 }, { -16.0f, -31.0f, -17.0f, -39.0f, 1 },
            { -9.0f, -25.0f, -8.0f, -36.0f, 2 }, { -8.0f, -36.0f, -12.0f, -42.0f, 1 }, { -8.0f, -36.0f, -4.0f, -43.0f, 1 },
            { 2.0f, -30.0f, -1.0f, -40.0f, 2 }, { -1.0f, -40.0f, -3.0f, -46.0f, 1 }, { -1.0f, -40.0f, 3.0f, -45.0f, 1 },
            { 2.0f, -30.0f, 8.0f, -38.0f, 2 }, { 8.0f, -38.0f, 9.0f, -45.0f, 1 }, { 8.0f, -38.0f, 13.0f, -42.0f, 1 },
            { 10.0f, -23.0f, 17.0f, -27.0f, 2 }, { 17.0f, -27.0f, 22.0f, -28.0f, 1 }, { 17.0f, -27.0f, 19.0f, -34.0f, 1 },
            { 10.0f, -23.0f, 13.0f, -32.0f, 2 }, { 13.0f, -32.0f, 17.0f, -38.0f, 1 }, { 13.0f, -32.0f, 12.0f, -38.0f, 1 },
            { -21.0f, -34.0f, -24.0f, -33.0f, 1 }, { -17.0f, -39.0f, -19.0f, -42.0f, 1 }, { 22.0f, -28.0f, 25.0f, -30.0f, 1 },
            { 19.0f, -34.0f, 21.0f, -37.0f, 1 }, { -12.0f, -42.0f, -14.0f, -44.0f, 1 }, { 9.0f, -45.0f, 10.0f, -47.0f, 1 },
            { -5.0f, -21.0f, -8.0f, -20.0f, 1 }, { 6.0f, -20.0f, 8.0f, -18.0f, 1 },
        };
        constexpr float kOakHeight = 48.0f;
        constexpr float kOakCrown = 14.0f;

        void PaintBranch(const Canvas& paint, const Tones& tones, float toward, const Limb& limb)
        {
            const float dx = limb.X1 - limb.X0;
            const float dy = limb.Y1 - limb.Y0;
            const int steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy)));
            if (steps <= 0)
                return;
            const bool steep = std::abs(dy) >= std::abs(dx);
            const bool leaning = steep && std::abs(dx) >= std::abs(dy) * 0.3f;
            const auto thick = static_cast<float>(limb.Thick);
            const float half = std::floor(thick * 0.5f);
            const auto at = [&](int index)
                {
                    const float share = static_cast<float>(index) / static_cast<float>(steps);
                    return ImVec2(std::round(limb.X0 + dx * share), std::round(limb.Y0 + dy * share));
                };
            int step = 0;
            while (step <= steps)
            {
                const ImVec2 first = at(step);
                int last = step;
                while (last + 1 <= steps && (steep ? at(last + 1).x == first.x : at(last + 1).y == first.y))
                    ++last;
                const ImVec2 end = at(last);
                if (steep)
                {
                    const float left = first.x - half;
                    const float top = std::min(first.y, end.y);
                    const float bottom = std::max(first.y, end.y) + 1.0f;
                    Box(paint, left, top, left + thick, bottom, limb.Thick == 1 ? tones.BarkDark : tones.Bark);
                    if (limb.Thick >= 2)
                    {
                        const float litX = toward < 0.0f ? left : left + thick - 1.0f;
                        Box(paint, litX, top, litX + 1.0f, bottom, tones.BarkLit);
                    }
                    if (limb.Thick >= 3)
                    {
                        const float darkX = toward < 0.0f ? left + thick - 1.0f : left;
                        Box(paint, darkX, top, darkX + 1.0f, bottom, tones.BarkDark);
                    }
                    if (leaning)
                    {
                        const float snowX = dx < 0.0f ? left + thick : left - 1.0f;
                        const bool bright = (snowX < first.x) == (toward < 0.0f);
                        Box(paint, snowX, top, snowX + 1.0f, std::min(bottom, top + 2.0f), bright ? tones.SnowLit : tones.SnowShade);
                    }
                }
                else
                {
                    const float left = std::min(first.x, end.x);
                    const float right = std::max(first.x, end.x) + 1.0f;
                    const float top = first.y - half;
                    Box(paint, left, top, right, top + thick, limb.Thick == 1 ? tones.BarkDark : tones.Bark);
                    if (limb.Thick >= 2)
                        Box(paint, left, top + thick - 1.0f, right, top + thick, tones.BarkDark);
                    Box(paint, left, top - 1.0f, right, top, tones.SnowLit);
                }
                step = last + 1;
            }
        }

        void PaintOak(const Canvas& paint, const Tones& tones, float toward, std::uint32_t seed)
        {
            const bool litLeft = toward < 0.0f;
            struct Section
            {
                float Top, Bottom, Half;
            };
            constexpr Section kTrunk[] = { { -17.0f, -9.0f, 2.0f }, { -9.0f, -2.0f, 3.0f }, { -2.0f, 0.0f, 4.0f } };
            for (const Section& section : kTrunk)
            {
                Box(paint, -section.Half, section.Top, section.Half, section.Bottom, tones.Bark);
                Box(paint, litLeft ? -section.Half : section.Half - 1.0f, section.Top, litLeft ? 1.0f - section.Half : section.Half, section.Bottom, tones.BarkLit);
                Box(paint, litLeft ? section.Half - 1.0f : -section.Half, section.Top, litLeft ? section.Half : 1.0f - section.Half, section.Bottom, tones.BarkDark);
            }
            Box(paint, -6.0f, -1.0f, -4.0f, 0.0f, tones.Bark);
            Box(paint, 4.0f, -1.0f, 6.0f, 0.0f, tones.Bark);
            Box(paint, -1.0f, -8.0f, 1.0f, -5.0f, tones.BarkDark);
            Box(paint, -1.0f, -7.0f, 0.0f, -5.0f, Mix(tones.BarkDark, Colour(0.0f, 0.0f, 0.0f), 0.5f));
            Box(paint, 0.0f, -15.0f, 1.0f, -11.0f, tones.BarkDark);
            Box(paint, litLeft ? 1.0f : -2.0f, -4.0f, litLeft ? 2.0f : -1.0f, -1.0f, tones.BarkDark);

            std::uint32_t index = 0;
            for (const Limb& limb : kOak)
            {
                PaintBranch(paint, tones, toward, limb);
                if (limb.Thick >= 2)
                    Box(paint, limb.X1 - 1.0f, limb.Y1 - 1.0f, limb.X1 + 1.0f, limb.Y1, tones.SnowLit);
                else if (Noise(seed + index * 104729u) % 3u == 0u)
                {
                    Box(paint, limb.X1, limb.Y1 + 1.0f, limb.X1 + 1.0f, limb.Y1 + 2.0f, tones.Leaf);
                    Box(paint, limb.X1 - 1.0f, limb.Y1 + 2.0f, limb.X1, limb.Y1 + 3.0f, tones.LeafDark);
                }
                ++index;
            }
            Box(paint, -2.0f, -18.0f, 2.0f, -17.0f, tones.SnowLit);
            SplitRow(paint, -7.0f, -3.0f, -2.0f, -5.0f, toward, tones.SnowLit, tones.SnowShade);
            SplitRow(paint, 3.0f, 7.0f, -2.0f, 5.0f, toward, tones.SnowLit, tones.SnowShade);
            SplitRow(paint, -8.0f, 8.0f, -1.0f, -toward * 4.0f, toward, tones.SnowLit, tones.Snow);
            Box(paint, -4.0f, -1.0f, 4.0f, 0.0f, tones.Snow);
        }

        void PaintDome(const Canvas& paint, const Tones& tones, float toward, float x, float half, int height, std::uint32_t seed)
        {
            const auto sideOf = [&](float cell, ImVec4 lit, ImVec4 shade) { return (cell < x) == (toward < 0.0f) ? lit : shade; };
            const ImVec4 rim = Mix(tones.NeedleDark, Colour(0.0f, 0.0f, 0.0f), 0.35f);
            const int cap = std::max(2, static_cast<int>(std::round(static_cast<float>(height) * 0.38f)));
            for (int row = 0; row < height; ++row)
            {
                const float middle = static_cast<float>(height) * 0.55f;
                const float offset = static_cast<float>(row) + 0.5f - middle;
                const float bend = offset < 0.0f ? offset / middle : offset / (static_cast<float>(height) - middle) * 0.6f;
                const float reach = std::max(1.0f, std::round(half * std::sqrt(std::max(0.0f, 1.0f - bend * bend))));
                const auto y = static_cast<float>(row - height);
                const float edge = toward < 0.0f ? x + reach - 1.0f : x - reach;
                if (row == 0)
                {
                    Box(paint, x - reach, y, x + reach, y + 1.0f, tones.SnowLit);
                }
                else if (row < cap)
                {
                    SplitRow(paint, x - reach, x + reach, y, x - toward * reach * 0.3f, toward, tones.SnowLit, tones.SnowShade);
                    Box(paint, edge, y, edge + 1.0f, y + 1.0f, tones.SnowDeep);
                }
                else if (row == height - 1)
                {
                    Box(paint, x - reach, y, x + reach, y + 1.0f, rim);
                }
                else
                {
                    const std::uint32_t ragged = Noise(seed + static_cast<std::uint32_t>(row) * 2654435761u);
                    const float left = x - reach - static_cast<float>(ragged % 3u == 0u) + static_cast<float>((ragged >> 4) % 4u == 0u);
                    const float right = x + reach + static_cast<float>((ragged >> 8) % 3u == 0u) - static_cast<float>((ragged >> 12) % 4u == 0u);
                    ShadedRow(paint, left, right, y, toward, std::max(1.0f, std::round(reach * 0.35f)), std::max(1.0f, std::round(reach * 0.4f)), tones.NeedleLit, tones.Needle, tones.NeedleDark);
                    const float rimX = toward < 0.0f ? right - 1.0f : left;
                    Box(paint, rimX, y, rimX + 1.0f, y + 1.0f, rim);
                }
                if (row == cap)
                {
                    for (int lump = 0; lump < 4; ++lump)
                    {
                        const float cell = x - reach + 1.0f + static_cast<float>((Noise(seed + static_cast<std::uint32_t>(lump) * 31u) % static_cast<std::uint32_t>(std::max(1.0f, reach * 2.0f - 3.0f))));
                        Box(paint, cell, y, cell + 2.0f, y + 1.0f + static_cast<float>(lump % 2), sideOf(cell, tones.SnowLit, tones.SnowShade));
                    }
                }
            }
            const std::uint32_t mixed = Noise(seed * 3u + 17u);
            const float body = static_cast<float>(cap - height);
            for (int bit = 0; bit < 6; ++bit)
            {
                const std::uint32_t own = Noise(mixed + static_cast<std::uint32_t>(bit) * 977u);
                const float row = body + 1.0f + static_cast<float>(own % static_cast<std::uint32_t>(std::max(1, height - cap - 2)));
                const float cell = x - half * 0.7f + static_cast<float>((own >> 8) % static_cast<std::uint32_t>(std::max(1.0f, half * 1.4f)));
                const float rounded = std::round(cell);
                if (bit < 2)
                    Box(paint, rounded, row, rounded + 2.0f, row + 1.0f, sideOf(rounded, tones.SnowLit, tones.SnowShade));
                else if (bit < 3)
                    Box(paint, rounded, row, rounded + 1.0f, row + 1.0f, Mix(tones.NeedleDark, Colour(0.0f, 0.0f, 0.0f), 0.4f));
                else
                    Box(paint, rounded, row, rounded + 1.0f, row + 1.0f, tones.Berry);
            }
        }

        void PaintBush(const Canvas& paint, const Tones& tones, float toward, std::uint32_t seed)
        {
            PaintDome(paint, tones, toward, 3.0f, 12.0f, 14, seed * 7u + 3u);
            PaintDome(paint, tones, toward, -11.0f, 7.0f, 9, seed);
            const auto pillow = [&](float x, float top, float half)
                {
                    Box(paint, x - half + 1.0f, top, x + half - 1.0f, top + 1.0f, tones.SnowLit);
                    SplitRow(paint, x - half, x + half, top + 1.0f, x - toward, toward, tones.SnowLit, tones.SnowShade);
                };
            pillow(toward < 0.0f ? 0.0f : 6.0f, -15.0f, 3.0f);
            pillow(toward < 0.0f ? 8.0f : -2.0f, -14.0f, 3.0f);
            pillow(-11.0f, -10.0f, 2.0f);
            PaintDrift(paint, tones, toward, toward < 0.0f ? -16.0f : 15.0f, 5.0f, 2);
        }

        void PaintFence(const Canvas& paint, const Tones& tones, float toward)
        {
            const bool litLeft = toward < 0.0f;
            const auto rail = [&](float left, float right, float y)
                {
                    Box(paint, left, y - 1.0f, right, y, tones.SnowLit);
                    Box(paint, left, y, right, y + 1.0f, tones.Wood);
                    Box(paint, left, y + 1.0f, right, y + 2.0f, tones.WoodDark);
                };
            rail(-18.0f, 6.0f, -13.0f);
            rail(-18.0f, 15.0f, -8.0f);
            for (int step = 0; step < 7; ++step)
            {
                const auto x = static_cast<float>(5 + step);
                const float y = -13.0f + std::floor(static_cast<float>(step) * 0.75f);
                Box(paint, x, y, x + 1.0f, y + 2.0f, step % 2 == 0 ? tones.Wood : tones.WoodDark);
                if (step % 2 == 0)
                    Box(paint, x, y - 1.0f, x + 1.0f, y, tones.SnowShade);
            }

            struct Post
            {
                float X, Height;
            };
            constexpr Post kPosts[] = { { -16.0f, 15.0f }, { -7.0f, 14.0f }, { 2.0f, 16.0f }, { 11.0f, 13.0f } };
            for (const Post& post : kPosts)
            {
                const float top = -post.Height;
                Box(paint, post.X, top, post.X + 2.0f, 0.0f, tones.Wood);
                Box(paint, litLeft ? post.X : post.X + 1.0f, top, litLeft ? post.X + 1.0f : post.X + 2.0f, 0.0f, tones.WoodLit);
                Box(paint, litLeft ? post.X + 1.0f : post.X, top + 3.0f, litLeft ? post.X + 2.0f : post.X + 1.0f, top + 5.0f, tones.WoodDark);
                SplitRow(paint, post.X - 1.0f, post.X + 3.0f, top - 1.0f, post.X + 1.0f + (litLeft ? 1.0f : -1.0f), toward, tones.SnowLit, tones.SnowShade);
                Box(paint, post.X, top - 2.0f, post.X + 2.0f, top - 1.0f, tones.SnowLit);
            }

            constexpr float kPeak = -4.0f;
            constexpr int kHigh = 8;
            for (int row = 0; row < kHigh; ++row)
            {
                const float up = (static_cast<float>(kHigh - row) - 0.5f) / static_cast<float>(kHigh);
                const float reach = std::sqrt(std::max(0.0f, 1.0f - up * up));
                const float left = std::round(kPeak - 15.0f * reach);
                const float right = std::round(kPeak + 20.0f * reach);
                const auto y = static_cast<float>(row - kHigh);
                if (row == 0)
                    Box(paint, left, y, right, y + 1.0f, tones.SnowLit);
                else
                    SplitRow(paint, left, right, y, kPeak - toward * 3.0f, toward, row < 4 ? tones.SnowLit : tones.Snow, tones.SnowShade);
                if (row == 1)
                    Box(paint, litLeft ? kPeak + 2.0f : left + 1.0f, y, litLeft ? right - 1.0f : kPeak - 2.0f, y + 1.0f, tones.SnowDeep);
                if (row > 0)
                    Box(paint, litLeft ? right - 1.0f : left, y, litLeft ? right : left + 1.0f, y + 1.0f, tones.SnowDeep);
            }
            Box(paint, 13.0f, -6.0f, 14.0f, -2.0f, tones.Stalk);
            Box(paint, 15.0f, -5.0f, 16.0f, -2.0f, tones.StalkDark);
            Box(paint, 12.0f, -7.0f, 14.0f, -6.0f, tones.StalkDark);
        }

        void MirrorBox(const Canvas& paint, float toward, float left, float top, float right, float bottom, ImVec4 colour)
        {
            if (toward < 0.0f)
                Box(paint, left, top, right, bottom, colour);
            else
                Box(paint, -right, top, -left, bottom, colour);
        }

        struct Facets
        {
            float Left, LitEnd, DarkStart, Right;
        };
        constexpr Facets kBoulder[] = {
            { -6.0f, -2.0f, 1.0f, 3.0f }, { -9.0f, -4.0f, 3.0f, 6.0f }, { -10.0f, -5.0f, 4.0f, 8.0f }, { -11.0f, -5.0f, 5.0f, 9.0f },
            { -11.0f, -6.0f, 5.0f, 10.0f }, { -12.0f, -6.0f, 6.0f, 10.0f }, { -12.0f, -7.0f, 6.0f, 11.0f }, { -12.0f, -7.0f, 6.0f, 11.0f },
            { -12.0f, -8.0f, 7.0f, 11.0f }, { -12.0f, -8.0f, 7.0f, 11.0f }, { -11.0f, -8.0f, 7.0f, 11.0f }, { -10.0f, -10.0f, -10.0f, 10.0f },
        };
        constexpr Facets kPebble[] = {
            { 13.0f, 15.0f, 16.0f, 17.0f }, { 12.0f, 14.0f, 16.0f, 18.0f }, { 12.0f, 14.0f, 17.0f, 19.0f }, { 12.0f, 14.0f, 17.0f, 19.0f },
            { 12.0f, 12.0f, 12.0f, 18.0f },
        };

        template <std::size_t Rows>
        void PaintRock(const Canvas& paint, const Tones& tones, float toward, const Facets (&rows)[Rows])
        {
            for (std::size_t row = 0; row < Rows; ++row)
            {
                const Facets& facets = rows[row];
                const auto y = static_cast<float>(static_cast<int>(row) - static_cast<int>(Rows));
                MirrorBox(paint, toward, facets.Left, y, facets.LitEnd, y + 1.0f, tones.StoneLit);
                MirrorBox(paint, toward, facets.LitEnd, y, facets.DarkStart, y + 1.0f, tones.Stone);
                MirrorBox(paint, toward, facets.DarkStart, y, facets.Right, y + 1.0f, tones.StoneDark);
            }
        }

        void PaintBoulder(const Canvas& paint, const Tones& tones, float toward)
        {
            const auto box = [&](float left, float top, float right, float bottom, ImVec4 colour) { MirrorBox(paint, toward, left, top, right, bottom, colour); };
            const ImVec4 crack = Mix(tones.StoneDark, Colour(0.0f, 0.0f, 0.0f), 0.3f);
            PaintRock(paint, tones, toward, kBoulder);
            PaintRock(paint, tones, toward, kPebble);
            box(-8.0f, -11.0f, 6.0f, -10.0f, tones.StoneDark);
            box(1.0f, -9.0f, 2.0f, -7.0f, crack);
            box(2.0f, -7.0f, 3.0f, -5.0f, crack);
            box(3.0f, -5.0f, 4.0f, -3.0f, crack);
            box(-10.0f, -6.0f, -8.0f, -5.0f, tones.Lichen);
            box(-9.0f, -5.0f, -8.0f, -4.0f, tones.Lichen);
            box(-6.0f, -3.0f, -5.0f, -2.0f, tones.Lichen);
            box(-2.0f, -6.0f, -1.0f, -5.0f, tones.StoneLit);
            box(-4.0f, -9.0f, -3.0f, -8.0f, tones.Stone);
            box(8.0f, -5.0f, 9.0f, -4.0f, tones.Stone);
            box(5.0f, -3.0f, 6.0f, -2.0f, crack);
            box(-5.0f, -15.0f, 0.0f, -14.0f, tones.SnowLit);
            box(-8.0f, -14.0f, 1.0f, -13.0f, tones.SnowLit);
            box(1.0f, -14.0f, 3.0f, -13.0f, tones.SnowShade);
            box(-10.0f, -13.0f, -1.0f, -12.0f, tones.SnowLit);
            box(-1.0f, -13.0f, 5.0f, -12.0f, tones.SnowShade);
            box(5.0f, -13.0f, 6.0f, -12.0f, tones.SnowDeep);
            box(-11.0f, -12.0f, -3.0f, -11.0f, tones.SnowLit);
            box(-3.0f, -12.0f, 6.0f, -11.0f, tones.SnowShade);
            box(6.0f, -12.0f, 8.0f, -11.0f, tones.SnowDeep);
            box(-11.0f, -11.0f, -8.0f, -10.0f, tones.SnowLit);
            box(-11.0f, -10.0f, -10.0f, -9.0f, tones.SnowLit);
            box(-4.0f, -11.0f, -1.0f, -10.0f, tones.SnowLit);
            box(3.0f, -11.0f, 5.0f, -10.0f, tones.SnowShade);
            box(7.0f, -11.0f, 8.0f, -9.0f, tones.Ice);
            box(5.0f, -10.0f, 6.0f, -9.0f, tones.Ice);
            box(-4.0f, -15.0f, -3.0f, -14.0f, tones.Glint);
            box(13.0f, -7.0f, 17.0f, -6.0f, tones.SnowLit);
            box(12.0f, -6.0f, 15.0f, -5.0f, tones.SnowLit);
            box(15.0f, -6.0f, 18.0f, -5.0f, tones.SnowShade);
            PaintDrift(paint, tones, toward, toward < 0.0f ? -12.0f : 12.0f, 5.0f, 3);
            PaintDrift(paint, tones, toward, toward < 0.0f ? 20.0f : -20.0f, 3.0f, 2);
        }

        void PaintSaplings(const Canvas& paint, const Tones& tones, float toward, std::uint32_t seed)
        {
            PaintSpruce(paint, tones, toward, -5.0f, 25, 8, 4, seed);
            PaintSpruce(paint, tones, toward, 6.0f, 17, 6, 3, seed * 5u + 1u);
            PaintDrift(paint, tones, toward, 0.0f, 14.0f, 5);
            Box(paint, -11.0f, -7.0f, -10.0f, -4.0f, tones.StalkDark);
            Box(paint, 12.0f, -6.0f, 13.0f, -3.0f, tones.Stalk);
        }

        class WinterScene final : public Scene
        {
        public:
            void DrawBack(const Frame& frame) override
            {
                const Tones tones = TonesOf(frame);
                const bool fromLeft = frame.Sun.X < static_cast<float>(frame.Columns) * 0.5f;
                PaintSky(frame, tones);
                PaintRange(frame, tones, fromLeft);
                PaintHills(frame, tones, fromLeft);
                PaintTreeline(frame, tones, fromLeft);
                PaintLand(frame, tones, fromLeft);
                PaintLake(frame, tones);
            }

            void PlaceProps(const Frame& frame, std::vector<Prop>& props) override
            {
                struct Plan
                {
                    Kind What;
                    float Across;
                    float Depth;
                };
                constexpr Plan kPlans[] = {
                    { Kind::Spruce, 0.09f, 0.30f }, { Kind::Bush, 0.27f, 0.78f }, { Kind::Oak, 0.42f, 0.05f },
                    { Kind::Fence, 0.59f, 0.50f }, { Kind::Boulder, 0.76f, 0.86f }, { Kind::Saplings, 0.90f, 0.24f },
                };
                const auto width = static_cast<float>(frame.Columns);
                const float farthest = frame.FeetBack + 2.0f;
                const float nearest = frame.FeetFront - 6.0f;
                std::uint32_t index = 0;
                for (const Plan& plan : kPlans)
                {
                    const std::uint32_t mixed = Noise((static_cast<std::uint32_t>(frame.Columns) * 2654435761u) ^ (index * 40503u + 7u));
                    Prop prop;
                    prop.Kind = static_cast<std::uint32_t>(plan.What);
                    prop.Seed = mixed;
                    const float depth = std::clamp(plan.Depth + (static_cast<float>((mixed >> 8) % 9u) - 4.0f) * 0.01f, 0.0f, 1.0f);
                    prop.Ground = std::round(farthest + (nearest - farthest) * depth);
                    switch (plan.What)
                    {
                    case Kind::Spruce:
                        prop.Foot = 2.0f;
                        prop.Cover = 11.0f;
                        prop.CoverHeight = 12.0f;
                        prop.Height = 46.0f;
                        prop.Crown = 7.0f;
                        break;
                    case Kind::Oak:
                        prop.Foot = 3.0f;
                        prop.Height = kOakHeight;
                        prop.Crown = kOakCrown;
                        break;
                    case Kind::Bush:
                        prop.Foot = 7.0f;
                        prop.Cover = 12.0f;
                        prop.CoverHeight = 12.0f;
                        prop.Height = 15.0f;
                        prop.Crown = 10.0f;
                        break;
                    case Kind::Fence:
                        prop.Foot = 12.0f;
                        prop.Cover = 13.0f;
                        prop.CoverHeight = 10.0f;
                        prop.Height = 18.0f;
                        prop.Crown = 16.0f;
                        break;
                    case Kind::Boulder:
                        prop.Foot = 9.0f;
                        prop.Cover = 8.0f;
                        prop.CoverHeight = 10.0f;
                        prop.Height = 15.0f;
                        prop.Crown = 8.0f;
                        break;
                    case Kind::Saplings:
                        prop.Foot = 3.0f;
                        prop.Cover = 10.0f;
                        prop.CoverHeight = 10.0f;
                        prop.Height = 25.0f;
                        prop.Crown = 6.0f;
                        break;
                    }
                    const float reach = 8.0f + std::max(prop.Crown, prop.Cover) * frame.ScaleAt(prop.Ground);
                    prop.X = std::clamp(std::round(width * plan.Across + static_cast<float>(mixed % 9u) - 4.0f), reach, std::max(reach, width - reach));
                    props.push_back(prop);
                    ++index;
                }
            }

            void DrawProp(const Frame& frame, const Prop& prop, const Canvas& paint) override
            {
                const Tones tones = TonesOf(frame);
                const float toward = prop.X >= frame.Sun.X ? -1.0f : 1.0f;
                switch (static_cast<Kind>(prop.Kind))
                {
                case Kind::Spruce:
                    PaintSpruce(paint, tones, toward, 0.0f, 46, 14, 6, prop.Seed);
                    PaintDrift(paint, tones, toward, 0.0f, 7.0f, 2);
                    break;
                case Kind::Oak:
                    PaintOak(paint, tones, toward, prop.Seed);
                    break;
                case Kind::Bush:
                    PaintBush(paint, tones, toward, prop.Seed);
                    break;
                case Kind::Fence:
                    PaintFence(paint, tones, toward);
                    break;
                case Kind::Boulder:
                    PaintBoulder(paint, tones, toward);
                    break;
                case Kind::Saplings:
                    PaintSaplings(paint, tones, toward, prop.Seed);
                    break;
                }
            }

            void DrawFront(const Frame& frame) override
            {
                const Tones tones = TonesOf(frame);
                const Canvas& paint = frame.Paint;
                const float toward = frame.Sun.X < static_cast<float>(frame.Columns) * 0.5f ? -1.0f : 1.0f;
                const float edge = frame.Shore;
                constexpr int kSpacing = 13;
                for (int slot = 1; slot * kSpacing < frame.Columns - 10; ++slot)
                {
                    const std::uint32_t mixed = Noise(static_cast<std::uint32_t>(slot) * 2477u + 19u);
                    const auto x = static_cast<float>(slot * kSpacing + static_cast<int>(mixed % 6u));
                    switch ((mixed >> 8) % 7u)
                    {
                    case 0:
                    case 1:
                    {
                        const float half = 3.0f + static_cast<float>((mixed >> 12) % 3u);
                        const float middle = x + half;
                        Box(paint, middle - half + 2.0f, edge - 3.0f, middle + half - 2.0f, edge - 2.0f, tones.SnowLit);
                        SplitRow(paint, middle - half + 1.0f, middle + half - 1.0f, edge - 2.0f, middle - toward, toward, tones.SnowLit, tones.SnowShade);
                        SplitRow(paint, middle - half, middle + half, edge - 1.0f, middle - toward, toward, tones.Snow, tones.SnowShade);
                        break;
                    }
                    case 2:
                    case 3:
                    {
                        const int count = 3 + static_cast<int>((mixed >> 12) % 2u);
                        for (int stalk = 0; stalk < count; ++stalk)
                        {
                            const auto sx = x + static_cast<float>(stalk);
                            const float tall = 2.0f + static_cast<float>((mixed >> (14 + stalk * 2)) % 3u) + (stalk == 1 ? 2.0f : 0.0f);
                            const ImVec4 colour = stalk % 2 == 0 ? tones.Stalk : tones.StalkDark;
                            Box(paint, sx, edge - tall, sx + 1.0f, edge, colour);
                            if (stalk == 0)
                                Box(paint, sx - 1.0f, edge - tall - 1.0f, sx, edge - tall, colour);
                            else if (stalk == count - 1)
                                Box(paint, sx + 1.0f, edge - tall - 1.0f, sx + 2.0f, edge - tall, colour);
                            else if (stalk == 1)
                            {
                                Box(paint, sx, edge - tall - 2.0f, sx + 1.0f, edge - tall, tones.StalkDark);
                                Box(paint, sx, edge - tall - 3.0f, sx + 1.0f, edge - tall - 2.0f, tones.SnowLit);
                            }
                        }
                        Box(paint, x - 1.0f, edge - 1.0f, x + static_cast<float>(count) + 1.0f, edge, tones.SnowLit);
                        break;
                    }
                    case 4:
                    {
                        Box(paint, x, edge - 2.0f, x + 3.0f, edge, tones.Ice);
                        Box(paint, x + 1.0f, edge - 3.0f, x + 3.0f, edge - 2.0f, tones.Ice);
                        Box(paint, x + 3.0f, edge - 2.0f, x + 4.0f, edge, tones.IceShade);
                        Box(paint, x + 1.0f, edge - 3.0f, x + 2.0f, edge - 2.0f, tones.Glint);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            ImVec4 HazeColor(const Frame& frame) override
            {
                return TonesOf(frame).Haze;
            }
        };
    }

    std::unique_ptr<Scene> MakeWinter()
    {
        return std::make_unique<WinterScene>();
    }
}
