#include "UI/SeasonFrame.h"

#include "UI/HomeScene.h"
#include "UI/IdleAnimation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace Idler::UI
{
    namespace
    {
        using HomeScene::Canvas;
        using HomeScene::Lit;
        using HomeScene::Mix;
        using HomeScene::Noise;
        using HomeScene::Rgb;

        constexpr double kEventSeconds = 3.2;
        constexpr double kEventEvery = 11.0;
        constexpr float kPi = 3.14159265f;

        double g_hoverStarted = -100.0;
        bool g_wasHovered = false;
        float g_push = 0.0f;
        float g_pushSpeed = 0.0f;
        float g_mouse = 0.0f;

        float F(int value)
        {
            return static_cast<float>(value);
        }

        int Round(float value)
        {
            return static_cast<int>(std::floor(value + 0.5f));
        }

        std::uint32_t Hash(int value, std::uint32_t salt)
        {
            return Noise(static_cast<std::uint32_t>(value + 1000) * 2654435761u + salt);
        }

        float EventShare(double now, double offset)
        {
            if (!IdleAnimation::Enabled())
                return -1.0f;
            const double hovered = now - g_hoverStarted;
            if (hovered >= 0.0 && hovered < kEventSeconds)
                return static_cast<float>(hovered / kEventSeconds);
            const double cycle = std::fmod(now + offset, kEventEvery);
            return cycle < kEventSeconds ? static_cast<float>(cycle / kEventSeconds) : -1.0f;
        }

        void UpdatePush(bool hovered)
        {
            const float target = hovered ? 1.0f : 0.0f;
            if (!IdleAnimation::Enabled())
            {
                g_push = target;
                g_pushSpeed = 0.0f;
                return;
            }
            const float step = std::min(ImGui::GetIO().DeltaTime, 0.05f) / 4.0f;
            for (int pass = 0; pass < 4; ++pass)
            {
                g_pushSpeed += (90.0f * (target - g_push) - 11.0f * g_pushSpeed) * step;
                g_push += g_pushSpeed * step;
            }
            if (std::fabs(target - g_push) < 0.002f && std::fabs(g_pushSpeed) < 0.01f)
            {
                g_push = target;
                g_pushSpeed = 0.0f;
                return;
            }
            IdleAnimation::RequestFrames();
        }

        enum Material : std::uint8_t
        {
            kEmpty = 0,
            kStem,
            kLeafA,
            kLeafB,
            kLeafC,
            kLeafD,
            kSnow,
            kIce,
            kDried,
            kMaterialCount
        };

        constexpr std::uint8_t kVein = 0x40;
        constexpr std::uint8_t kKind = 0x3F;
        constexpr std::uint8_t kShadow = 0xFF;
        constexpr int kTones = 1 + 4 * (kMaterialCount - 1);

        using Palette = std::array<ImVec4, kTones>;

        int ToneOf(std::uint8_t material, int level)
        {
            return 1 + (static_cast<int>(material) - 1) * 4 + level;
        }

        struct Cell
        {
            int X = 0;
            int Y = 0;
        };

        class Cells
        {
        public:
            static constexpr int kWidth = 256;
            static constexpr int kHeight = 56;
            static constexpr int kLeft = 12;
            static constexpr int kTop = 20;

            void Clear()
            {
                if (m_right >= m_left)
                {
                    for (int row = m_top; row <= m_bottom; ++row)
                    {
                        std::fill_n(m_material.begin() + row * kWidth + m_left, m_right - m_left + 1, std::uint8_t{ 0 });
                        std::fill_n(m_tone.begin() + row * kWidth + m_left, m_right - m_left + 1, std::uint8_t{ 0 });
                    }
                }
                m_left = kWidth;
                m_right = -1;
                m_top = kHeight;
                m_bottom = -1;
            }

            void Put(int x, int y, std::uint8_t material)
            {
                const int column = x + kLeft;
                const int row = y + kTop;
                if (column < 1 || column >= kWidth - 1 || row < 1 || row >= kHeight - 1)
                    return;
                m_material[static_cast<std::size_t>(row * kWidth + column)] = material;
                m_left = std::min(m_left, column - 1);
                m_right = std::max(m_right, column + 1);
                m_top = std::min(m_top, row - 1);
                m_bottom = std::max(m_bottom, row + 1);
            }

            std::uint8_t At(int x, int y) const
            {
                const int column = x + kLeft;
                const int row = y + kTop;
                if (column < 0 || column >= kWidth || row < 0 || row >= kHeight)
                    return 0;
                return m_material[static_cast<std::size_t>(row * kWidth + column)];
            }

            void Shade()
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    for (int column = m_left; column <= m_right; ++column)
                    {
                        const std::uint8_t raw = m_material[static_cast<std::size_t>(row * kWidth + column)];
                        const std::uint8_t kind = raw & kKind;
                        if (kind == kEmpty)
                            continue;
                        const auto other = [&](int dx, int dy)
                            {
                                return (m_material[static_cast<std::size_t>((row + dy) * kWidth + column + dx)] & kKind) != kind;
                            };
                        const bool up = other(0, -1);
                        const bool left = other(-1, 0);
                        const bool down = other(0, 1);
                        const bool right = other(1, 0);
                        int level = 1 + (up || left ? 1 : 0) - (down || right ? 1 : 0);
                        if (up && left && !down && !right)
                            level = 3;
                        if ((raw & kVein) != 0)
                            level = std::max(0, level - 1);
                        m_tone[static_cast<std::size_t>(row * kWidth + column)] = static_cast<std::uint8_t>(ToneOf(kind, level));
                    }
                }
                for (int row = m_bottom; row > m_top; --row)
                {
                    for (int column = m_right; column > m_left; --column)
                    {
                        const auto here = static_cast<std::size_t>(row * kWidth + column);
                        const auto from = static_cast<std::size_t>((row - 1) * kWidth + column - 1);
                        if (m_material[here] == kEmpty && m_material[from] != kEmpty)
                            m_tone[here] = kShadow;
                    }
                }
            }

            void Brighten(int fromX, int toX, std::uint8_t material)
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    for (int x = fromX; x <= toX; ++x)
                    {
                        const int column = x + kLeft;
                        if (column <= m_left || column >= m_right)
                            continue;
                        const auto index = static_cast<std::size_t>(row * kWidth + column);
                        if ((m_material[index] & kKind) == material)
                            m_tone[index] = static_cast<std::uint8_t>(ToneOf(material, 3));
                    }
                }
            }

            void Flush(const Canvas& paint, const Palette& colors, ImVec4 shadow) const
            {
                for (int row = m_top; row <= m_bottom; ++row)
                {
                    const std::uint8_t* line = m_tone.data() + static_cast<std::size_t>(row * kWidth);
                    int column = m_left;
                    while (column <= m_right)
                    {
                        const std::uint8_t tone = line[column];
                        int end = column + 1;
                        while (end <= m_right && line[end] == tone)
                            ++end;
                        if (tone != 0)
                        {
                            const float top = F(row - kTop);
                            paint.Fill(F(column - kLeft), top, F(end - kLeft), top + 1.0f, tone == kShadow ? shadow : colors[tone]);
                        }
                        column = end;
                    }
                }
            }

        private:
            std::array<std::uint8_t, static_cast<std::size_t>(kWidth * kHeight)> m_material{};
            std::array<std::uint8_t, static_cast<std::size_t>(kWidth * kHeight)> m_tone{};
            int m_left = 0;
            int m_right = kWidth - 1;
            int m_top = 0;
            int m_bottom = kHeight - 1;
        };

        Cells g_cells;

        class Path
        {
        public:
            static constexpr int kCapacity = 512;

            void Start(float x, float y)
            {
                m_count = 0;
                m_x = x;
                m_y = y;
                Add(Round(x), Round(y));
            }

            void LineTo(float x, float y)
            {
                const float span = std::max(std::fabs(x - m_x), std::fabs(y - m_y));
                const int steps = std::max(1, static_cast<int>(std::ceil(span * 2.0f)));
                for (int step = 1; step <= steps; ++step)
                {
                    const float share = F(step) / F(steps);
                    Add(Round(m_x + (x - m_x) * share), Round(m_y + (y - m_y) * share));
                }
                m_x = x;
                m_y = y;
            }

            int Count() const { return m_count; }

            Cell At(int index) const { return m_cells[static_cast<std::size_t>(std::clamp(index, 0, std::max(0, m_count - 1)))]; }

        private:
            void Add(int x, int y)
            {
                if (m_count > 0)
                {
                    const Cell& last = m_cells[static_cast<std::size_t>(m_count - 1)];
                    if (last.X == x && last.Y == y)
                        return;
                }
                if (m_count >= 2)
                {
                    const Cell& before = m_cells[static_cast<std::size_t>(m_count - 2)];
                    const Cell& last = m_cells[static_cast<std::size_t>(m_count - 1)];
                    if (std::abs(x - before.X) == 1 && std::abs(y - before.Y) == 1 && (last.X == before.X || last.Y == before.Y))
                    {
                        m_cells[static_cast<std::size_t>(m_count - 1)] = Cell{ x, y };
                        return;
                    }
                }
                if (m_count < kCapacity)
                    m_cells[static_cast<std::size_t>(m_count++)] = Cell{ x, y };
            }

            std::array<Cell, kCapacity> m_cells{};
            int m_count = 0;
            float m_x = 0.0f;
            float m_y = 0.0f;
        };

        struct Anchor
        {
            int X = 0;
            int Y = 0;
            int Tx = 0;
            int Ty = 0;
            int Ox = 0;
            int Oy = 0;
        };

        struct Mask
        {
            const char* const* Rows = nullptr;
            int Height = 0;
        };

        constexpr const char* kTinyRows[] = { "X ", "XX", "s " };
        constexpr const char* kSmallRows[] = { "  XX", " XXX", "XvX ", "s   " };
        constexpr const char* kBigRows[] = { "   XX", " XXXX", "XXvX ", "XvX  ", "s    " };
        constexpr const char* kCurlRows[] = { " XX ", "X  X", "X X ", "s   " };

        constexpr Mask kTiny{ kTinyRows, 3 };
        constexpr Mask kSmall{ kSmallRows, 4 };
        constexpr Mask kBig{ kBigRows, 5 };
        constexpr Mask kCurl{ kCurlRows, 4 };

        void PutMask(Cells& cells, const Anchor& at, const Mask& mask, int mirror, std::uint8_t leaf, std::uint8_t stem)
        {
            int attach = 0;
            const char* bottom = mask.Rows[mask.Height - 1];
            for (int column = 0; bottom[column] != '\0'; ++column)
            {
                if (bottom[column] == 's')
                    attach = column;
            }
            for (int row = 0; row < mask.Height; ++row)
            {
                const char* line = mask.Rows[row];
                const int out = mask.Height - row;
                for (int column = 0; line[column] != '\0'; ++column)
                {
                    const char ink = line[column];
                    if (ink == ' ')
                        continue;
                    const int along = (column - attach) * mirror;
                    const std::uint8_t material = ink == 's' ? stem : ink == 'v' ? static_cast<std::uint8_t>(leaf | kVein) : leaf;
                    cells.Put(at.X + at.Tx * along + at.Ox * out, at.Y + at.Ty * along + at.Oy * out, material);
                }
            }
        }

        struct Style
        {
            int Leaves = 1;
            int Spacing = 4;
            bool Big = false;
            int BareShare = 0;
            bool Winter = false;
        };

        bool Bare(const Style& style, std::uint32_t mixed)
        {
            return style.BareShare > 0 && static_cast<int>((mixed >> 8) % 100u) < style.BareShare;
        }

        std::uint8_t LeafOf(const Style& style, std::uint32_t mixed)
        {
            return static_cast<std::uint8_t>(kLeafA + (mixed >> 12) % static_cast<std::uint32_t>(std::max(1, style.Leaves)));
        }

        struct Motion
        {
            float Push = 0.0f;
            float Mouse = 0.0f;
            float Sway = 0.0f;
            float Time = 0.0f;
            float Right = 0.0f;
            float Reach = 1.0f;
        };

        ImVec2 Moved(ImVec2 rest, const Motion& motion)
        {
            const float depth = std::max(0.0f, rest.y + 1.0f);
            const float share = std::min(1.0f, depth / motion.Reach);
            const float away = rest.x >= motion.Mouse ? 1.0f : -1.0f;
            const float swing = motion.Sway * share * std::sin(motion.Time * 5.0f - depth * 0.35f);
            const float x = std::clamp(rest.x + swing + motion.Push * away * depth * 0.6f, -1.0f, motion.Right);
            return ImVec2(x, rest.y - motion.Push * depth * 0.75f);
        }

        struct Strand
        {
            bool Swag = false;
            float From = 0.0f;
            float To = 0.0f;
            float Depth = 0.0f;
            std::uint32_t Seed = 0;

            ImVec2 Rest(float share) const
            {
                if (Swag)
                    return ImVec2(From + (To - From) * share, -1.0f + Depth * 4.0f * share * (1.0f - share));
                return ImVec2(From + 1.2f * std::sin(share * 4.4f + static_cast<float>(Seed)), -1.0f + Depth * share);
            }

            float Length() const { return Swag ? To - From : Depth; }
        };

        struct Strands
        {
            std::array<Strand, 3> List{};
            int Count = 0;
            float FreeFrom = 0.0f;
            float ClearFrom = 1.0e6f;
            float ClearTo = -1.0e6f;
        };

        int CreatureEnd(int width)
        {
            return std::min(width / 3, 17);
        }

        Strands PlanStrands(int columns, int rows, float textEnd)
        {
            Strands plan;
            const float width = F(columns);
            const float height = F(rows);
            const float creature = F(CreatureEnd(columns)) + 2.0f;
            const float swagTo = std::min(width - 8.0f, creature + std::max(14.0f, width * 0.4f));
            if (swagTo - creature >= 12.0f)
            {
                plan.List[static_cast<std::size_t>(plan.Count++)] = Strand{ true, creature, swagTo, 3.0f, 1u };
                plan.ClearFrom = creature - 2.0f;
                plan.ClearTo = swagTo + 2.0f;
            }
            const float from = std::max(textEnd + 3.0f, creature);
            plan.FreeFrom = from;
            const float right = width - 3.0f;
            if (right - from >= 6.0f)
                plan.List[static_cast<std::size_t>(plan.Count++)] = Strand{ false, right - 1.0f, 0.0f, height - 4.0f, 2u };
            if (right - from >= 15.0f)
                plan.List[static_cast<std::size_t>(plan.Count++)] = Strand{ false, from + 3.0f, 0.0f, height - 7.0f, 3u };
            return plan;
        }

        void DrawStrand(Cells& cells, const Strand& strand, const Motion& motion, const Style& style)
        {
            const int samples = std::max(6, static_cast<int>(strand.Length() * 1.2f));
            const int every = std::max(2, static_cast<int>(F(samples) * (strand.Swag ? 4.0f : 3.0f) / strand.Length()));
            Path path;
            const ImVec2 first = Moved(strand.Rest(0.0f), motion);
            path.Start(first.x, first.y);
            std::array<int, 48> leafAt{};
            int leaves = 0;
            for (int sample = 1; sample <= samples; ++sample)
            {
                const ImVec2 at = Moved(strand.Rest(F(sample) / F(samples)), motion);
                path.LineTo(at.x, at.y);
                if (sample % every == 0 && sample < samples - 1 && leaves < static_cast<int>(leafAt.size()))
                    leafAt[static_cast<std::size_t>(leaves++)] = path.Count() - 1;
            }
            for (int index = 0; index < path.Count(); ++index)
            {
                const Cell cell = path.At(index);
                cells.Put(cell.X, cell.Y, kStem);
            }

            if (style.Winter && strand.Swag)
            {
                for (int index = 1; index < path.Count() - 1; ++index)
                {
                    const Cell cell = path.At(index);
                    const std::uint32_t mixed = Hash(cell.X, strand.Seed * 977u);
                    if (mixed % 10u < 6u)
                        cells.Put(cell.X, cell.Y - 1, kSnow);
                    if (mixed % 7u == 0u)
                        cells.Put(cell.X, cell.Y + 1, kIce);
                }
            }

            if (!strand.Swag)
            {
                const Cell tip = path.At(path.Count() - 1);
                const int side = strand.Seed % 2u == 0u ? 1 : -1;
                PutMask(cells, Anchor{ tip.X, tip.Y, side, 0, 0, 1 }, kCurl, 1, kStem, kStem);
            }

            if (style.Winter)
                return;
            for (int leaf = 0; leaf < leaves; ++leaf)
            {
                const std::uint32_t mixed = Hash(leaf, strand.Seed * 131u);
                if (Bare(style, mixed))
                    continue;
                const Cell cell = path.At(leafAt[static_cast<std::size_t>(leaf)]);
                const int side = leaf % 2 == 0 ? 1 : -1;
                const std::uint8_t material = LeafOf(style, mixed);
                if (strand.Swag)
                {
                    const int out = cell.Y <= 0 ? side : -1;
                    PutMask(cells, Anchor{ cell.X, cell.Y, 1, 0, 0, out }, kSmall, (mixed >> 4) % 2u == 0u ? 1 : -1, material, kStem);
                }
                else
                    PutMask(cells, Anchor{ cell.X, cell.Y, 0, 1, side, 0 }, kSmall, 1, material, kStem);
            }
        }

        Cell OutOf(Cell cell, int width)
        {
            if (cell.Y >= 0 && cell.X <= 0)
                return Cell{ -1, 0 };
            if (cell.Y >= 0 && cell.X >= width)
                return Cell{ 1, 0 };
            return Cell{ 0, -1 };
        }

        Cell AlongOf(Cell out)
        {
            if (out.X < 0)
                return Cell{ 0, -1 };
            if (out.X > 0)
                return Cell{ 0, 1 };
            return Cell{ 1, 0 };
        }

        struct Frame
        {
            Path Line;
            int TopFrom = 0;
            int TopTo = 0;
            int Curls[2] = { 0, 0 };
        };

        void BuildFrame(Frame& frame, int width, int height)
        {
            const auto leftX = [](int y) { return -0.5f + 0.45f * std::sin(F(y) * 0.55f + 0.3f); };
            const auto topY = [](int x) { return -1.5f + 0.55f * std::sin(F(x) * 0.19f + 0.4f) + 0.25f * std::sin(F(x) * 0.47f + 1.3f); };
            Path& path = frame.Line;
            path.Start(leftX(height), F(height));
            for (int y = height - 1; y >= 1; --y)
                path.LineTo(leftX(y), F(y));
            path.LineTo(0.4f, -0.8f);
            frame.TopFrom = path.Count();
            for (int x = 1; x < width; ++x)
                path.LineTo(F(x), topY(x));
            frame.TopTo = path.Count();
            path.LineTo(F(width), -0.4f);
            const int right = std::max(2, height / 3);
            for (int y = 0; y <= right; ++y)
                path.LineTo(F(width) + 0.2f * std::sin(F(y) * 0.9f), F(y));

            const int targets[2] = { width * 28 / 100, width * 66 / 100 };
            for (int curl = 0; curl < 2; ++curl)
            {
                int best = frame.TopFrom;
                for (int index = frame.TopFrom; index < frame.TopTo; ++index)
                {
                    if (std::abs(path.At(index).X - targets[curl]) < std::abs(path.At(best).X - targets[curl]))
                        best = index;
                }
                frame.Curls[curl] = best;
            }
        }

        bool NearCurl(const Frame& frame, int index)
        {
            return std::abs(index - frame.Curls[0]) <= 2 || std::abs(index - frame.Curls[1]) <= 2;
        }

        void DrawFrame(Cells& cells, const Frame& frame, int width, const Style& style, const Strands& strands, float front)
        {
            const Path& path = frame.Line;
            for (int index = 0; index < path.Count(); ++index)
            {
                const Cell cell = path.At(index);
                const Cell out = OutOf(cell, width);
                cells.Put(cell.X, cell.Y, kStem);
                cells.Put(cell.X + out.X, cell.Y + out.Y, kStem);
            }

            for (int curl = 0; curl < 2; ++curl)
            {
                const Cell cell = path.At(frame.Curls[curl]);
                PutMask(cells, Anchor{ cell.X, cell.Y - 1, 1, 0, 0, -1 }, kCurl, curl == 0 ? 1 : -1, kStem, kStem);
            }
            const Cell end = path.At(path.Count() - 1);
            PutMask(cells, Anchor{ end.X, end.Y, 1, 0, 0, 1 }, kCurl, 1, kStem, kStem);

            if (style.Winter)
                return;
            const int creature = CreatureEnd(width);
            int number = 0;
            for (int base = 2; base < frame.TopTo - 1; base += style.Spacing, ++number)
            {
                const std::uint32_t mixed = Hash(base, 71u);
                const int index = base + static_cast<int>(mixed % 3u) - 1;
                const Cell cell = path.At(index);
                const Cell out = OutOf(cell, width);
                const bool top = out.Y < 0;
                if ((top && NearCurl(frame, index)) || (!top && index > frame.TopFrom - 3) || Bare(style, mixed))
                    continue;
                const Cell along = AlongOf(out);
                const std::uint8_t material = LeafOf(style, mixed);
                const bool gust = std::fabs(F(index) - front) < 2.5f;
                const int mirror = gust ? 1 : (mixed >> 4) % 2u == 0u ? 1 : -1;
                if (!top)
                {
                    PutMask(cells, Anchor{ cell.X + out.X, cell.Y + out.Y, along.X, along.Y, out.X, out.Y }, kTiny, mirror, material, kStem);
                    continue;
                }
                const bool hanging = number % 2 == 1 && cell.X > creature && (F(cell.X) < strands.ClearFrom || F(cell.X) > strands.ClearTo);
                if (hanging)
                    PutMask(cells, Anchor{ cell.X, cell.Y, 1, 0, 0, 1 }, kSmall, mirror, material, kStem);
                else
                    PutMask(cells, Anchor{ cell.X, cell.Y - 1, 1, 0, 0, -1 }, style.Big ? kBig : kSmall, mirror, material, kStem);
            }
        }

        void DrawWinterDetail(Cells& cells, const Frame& frame, int width)
        {
            const Path& path = frame.Line;
            const int creature = CreatureEnd(width);
            for (int index = frame.TopFrom; index < frame.TopTo; ++index)
            {
                const Cell cell = path.At(index);
                const std::uint32_t mixed = Hash(cell.X, 313u);
                if (mixed % 10u < 7u)
                {
                    cells.Put(cell.X, cell.Y - 2, kSnow);
                    if (mixed % 4u == 0u)
                        cells.Put(cell.X, cell.Y - 3, kSnow);
                }
                if (cell.X > creature && mixed % 9u == 0u)
                {
                    cells.Put(cell.X, cell.Y + 1, kIce);
                    if ((mixed >> 3) % 2u == 0u)
                        cells.Put(cell.X, cell.Y + 2, kIce);
                }
            }
            for (const int share : { 14, 52, 84 })
            {
                int best = frame.TopFrom;
                for (int index = frame.TopFrom; index < frame.TopTo; ++index)
                {
                    if (std::abs(path.At(index).X - width * share / 100) < std::abs(path.At(best).X - width * share / 100))
                        best = index;
                }
                if (NearCurl(frame, best))
                    continue;
                const Cell cell = path.At(best);
                PutMask(cells, Anchor{ cell.X, cell.Y - 1, 1, 0, 0, -1 }, kSmall, share % 2 == 0 ? 1 : -1, kDried, kStem);
            }
        }

        void SetRamp(Palette& palette, Material material, const ImVec4 (&tones)[4], float light)
        {
            for (int level = 0; level < 4; ++level)
                palette[static_cast<std::size_t>(ToneOf(material, level))] = Lit(tones[level], light);
        }

        void SetCold(Palette& palette, Material material, const ImVec4 (&tones)[4], float dark)
        {
            for (int level = 0; level < 4; ++level)
            {
                const ImVec4 tone = tones[level];
                palette[static_cast<std::size_t>(ToneOf(material, level))] = Mix(tone, Rgb(tone.x * 0.62f, tone.y * 0.68f, tone.z * 0.84f), dark * 0.8f);
            }
        }

        Style SeasonStyle(SceneSeason season, Palette& palette, float daylight)
        {
            const float light = 0.45f + 0.55f * daylight;
            Style style;
            switch (season)
            {
            case SceneSeason::Spring:
                SetRamp(palette, kStem, { Rgb(0.22f, 0.38f, 0.18f), Rgb(0.33f, 0.52f, 0.24f), Rgb(0.46f, 0.66f, 0.30f), Rgb(0.60f, 0.78f, 0.40f) }, light);
                SetRamp(palette, kLeafA, { Rgb(0.30f, 0.54f, 0.22f), Rgb(0.46f, 0.72f, 0.30f), Rgb(0.62f, 0.86f, 0.40f), Rgb(0.80f, 0.95f, 0.58f) }, light);
                SetRamp(palette, kLeafB, { Rgb(0.26f, 0.48f, 0.22f), Rgb(0.40f, 0.66f, 0.28f), Rgb(0.56f, 0.80f, 0.36f), Rgb(0.74f, 0.92f, 0.52f) }, light);
                style.Leaves = 2;
                style.Spacing = 5;
                break;
            case SceneSeason::Summer:
                SetRamp(palette, kStem, { Rgb(0.16f, 0.26f, 0.12f), Rgb(0.24f, 0.38f, 0.17f), Rgb(0.34f, 0.50f, 0.22f), Rgb(0.46f, 0.62f, 0.30f) }, light);
                SetRamp(palette, kLeafA, { Rgb(0.12f, 0.28f, 0.17f), Rgb(0.20f, 0.40f, 0.22f), Rgb(0.32f, 0.54f, 0.26f), Rgb(0.46f, 0.66f, 0.32f) }, light);
                SetRamp(palette, kLeafB, { Rgb(0.18f, 0.36f, 0.19f), Rgb(0.31f, 0.54f, 0.24f), Rgb(0.48f, 0.72f, 0.29f), Rgb(0.68f, 0.86f, 0.40f) }, light);
                style.Leaves = 2;
                style.Spacing = 4;
                style.Big = true;
                break;
            case SceneSeason::Autumn:
                SetRamp(palette, kStem, { Rgb(0.24f, 0.15f, 0.10f), Rgb(0.36f, 0.24f, 0.16f), Rgb(0.50f, 0.35f, 0.24f), Rgb(0.62f, 0.46f, 0.32f) }, light);
                SetRamp(palette, kLeafA, { Rgb(0.48f, 0.10f, 0.08f), Rgb(0.70f, 0.18f, 0.12f), Rgb(0.86f, 0.32f, 0.18f), Rgb(0.96f, 0.50f, 0.30f) }, light);
                SetRamp(palette, kLeafB, { Rgb(0.62f, 0.28f, 0.06f), Rgb(0.84f, 0.46f, 0.12f), Rgb(0.95f, 0.62f, 0.22f), Rgb(1.00f, 0.78f, 0.40f) }, light);
                SetRamp(palette, kLeafC, { Rgb(0.66f, 0.50f, 0.10f), Rgb(0.86f, 0.68f, 0.18f), Rgb(0.96f, 0.82f, 0.32f), Rgb(1.00f, 0.93f, 0.55f) }, light);
                SetRamp(palette, kLeafD, { Rgb(0.36f, 0.08f, 0.10f), Rgb(0.54f, 0.12f, 0.14f), Rgb(0.70f, 0.20f, 0.20f), Rgb(0.84f, 0.32f, 0.30f) }, light);
                style.Leaves = 4;
                style.Spacing = 4;
                style.Big = true;
                style.BareShare = 30;
                break;
            default:
                SetRamp(palette, kStem, { Rgb(0.20f, 0.16f, 0.15f), Rgb(0.32f, 0.26f, 0.24f), Rgb(0.44f, 0.37f, 0.33f), Rgb(0.56f, 0.48f, 0.43f) }, light);
                SetRamp(palette, kDried, { Rgb(0.36f, 0.26f, 0.17f), Rgb(0.50f, 0.38f, 0.26f), Rgb(0.62f, 0.49f, 0.34f), Rgb(0.72f, 0.60f, 0.44f) }, light);
                SetCold(palette, kSnow, { Rgb(0.62f, 0.70f, 0.84f), Rgb(0.80f, 0.86f, 0.95f), Rgb(0.93f, 0.96f, 1.00f), Rgb(1.00f, 1.00f, 1.00f) }, 1.0f - daylight);
                SetCold(palette, kIce, { Rgb(0.50f, 0.66f, 0.82f), Rgb(0.66f, 0.80f, 0.94f), Rgb(0.82f, 0.92f, 1.00f), Rgb(0.95f, 0.99f, 1.00f) }, 1.0f - daylight);
                style.Winter = true;
                break;
            }
            return style;
        }

        void DrawParticles(Cells& cells, SceneSeason season, const Strands& strands, int width, int height, float share)
        {
            if (share < 0.0f)
                return;
            if (season == SceneSeason::Autumn)
            {
                float origin = F(width) * 0.6f;
                for (int index = 0; index < strands.Count; ++index)
                {
                    if (!strands.List[static_cast<std::size_t>(index)].Swag)
                        origin = strands.List[static_cast<std::size_t>(index)].From - 2.0f;
                }
                const int x = Round(origin + std::sin(share * 6.0f) * 3.0f - share * 4.0f);
                const int y = Round(1.0f + share * (F(height) + 1.0f));
                const bool turned = std::fmod(share * 8.0f, 1.0f) < 0.5f;
                cells.Put(x, y, kLeafB);
                cells.Put(x + 1, y, kLeafB);
                cells.Put(turned ? x + 1 : x, y + 1, kLeafB);
                cells.Put(turned ? x + 2 : x - 1, y + 1, kLeafA);
                return;
            }
            if (season != SceneSeason::Winter)
                return;
            const float right = F(width) - 3.0f;
            const float room = right - strands.FreeFrom;
            const float bottom = room >= 4.0f ? F(height) + 1.0f : 0.0f;
            for (int flake = 0; flake < 3; ++flake)
            {
                const float fall = share * 1.4f - 0.2f * F(flake);
                if (fall < 0.0f || fall > 1.0f)
                    continue;
                const float lane = room >= 4.0f ? strands.FreeFrom + room * (0.2f + 0.3f * F(flake)) : F(width) * (0.24f + 0.3f * F(flake));
                const int x = Round(lane + std::sin(fall * 7.0f + F(flake)) * 1.5f);
                cells.Put(x, Round(-5.0f + fall * (bottom + 5.0f)), kSnow);
            }
        }
    }

    void DrawSeasonFrame(ImDrawList* drawList, ImVec2 min, ImVec2 max, float textEnd, SceneSeason season, float daylight, bool hovered)
    {
        const double now = ImGui::GetTime();
        if (hovered && !g_wasHovered)
            g_hoverStarted = now;
        g_wasHovered = hovered;

        const float pixel = IdleAnimation::PixelSize();
        const ImVec2 origin(std::round(min.x), std::round(min.y));
        const int width = static_cast<int>(std::floor((max.x - min.x) / pixel));
        const int height = Round((max.y - min.y) / pixel);
        if (width < 24 || height < 8)
            return;
        if (hovered)
            g_mouse = (ImGui::GetIO().MousePos.x - origin.x) / pixel;
        UpdatePush(hovered);

        const float share = EventShare(now, static_cast<double>(season) * 2.7);
        if (share >= 0.0f)
            IdleAnimation::RequestFrames();

        Palette palette{};
        const Style style = SeasonStyle(season, palette, daylight);
        const Strands strands = PlanStrands(width, height, std::ceil((textEnd - origin.x) / pixel));
        static Frame frame;
        BuildFrame(frame, width, height);

        Motion motion;
        motion.Push = g_push;
        motion.Mouse = g_mouse;
        motion.Time = static_cast<float>(now);
        motion.Right = F(width) + 1.0f;
        motion.Reach = std::max(1.0f, F(height) - 3.0f);
        motion.Sway = share >= 0.0f ? 1.4f * std::sin(share * kPi) : 0.0f;
        const float front = share >= 0.0f ? share * F(frame.Line.Count() + 8) - 4.0f : -1000.0f;

        Cells& cells = g_cells;
        cells.Clear();
        for (int index = 0; index < strands.Count; ++index)
            DrawStrand(cells, strands.List[static_cast<std::size_t>(index)], motion, style);
        DrawFrame(cells, frame, width, style, strands, front);
        if (style.Winter)
            DrawWinterDetail(cells, frame, width);
        DrawParticles(cells, season, strands, width, height, share);
        cells.Shade();
        if (style.Winter && share >= 0.0f)
        {
            const int glint = Round(share * F(width));
            cells.Brighten(glint - 1, glint + 1, kSnow);
        }
        cells.Flush(Canvas{ drawList, origin, pixel, 0.0f, ImVec4() }, palette, ImVec4(0.0f, 0.0f, 0.0f, 0.34f));
    }
}
