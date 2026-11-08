#include "Core/MiniGames.h"

#include <algorithm>
#include <cmath>

namespace Idler
{
    namespace
    {
        constexpr float kPi = 3.14159265f;

        float Mix(float from, float to, float share)
        {
            return from + (to - from) * share;
        }
    }

    float PlayGroundOf(MiniGame game)
    {
        switch (game)
        {
        case MiniGame::StarChase:
            return std::round(kPlayHeight * 0.78f);
        case MiniGame::PearlDive:
            return std::round(kPlayHeight * 0.84f);
        default:
            return std::round(kPlayHeight * 0.9f);
        }
    }

    namespace
    {
        constexpr int kStreakLength = 10;
        constexpr int kStreakBonus = 5;
    }

    MiniGame TerrainGameOf(Habit habit)
    {
        switch (habit)
        {
        case Habit::Floats:
            return MiniGame::StarChase;
        case Habit::Swims:
            return MiniGame::PearlDive;
        default:
            return MiniGame::TreatCatch;
        }
    }

    const char* MiniGameName(MiniGame game)
    {
        switch (game)
        {
        case MiniGame::TreatCatch:
            return "Treat catch";
        case MiniGame::StarChase:
            return "Star chase";
        case MiniGame::PearlDive:
            return "Pearl dive";
        case MiniGame::ShellGame:
            return "Shell game";
        }
        return "";
    }

    const char* MiniGameRules(MiniGame game)
    {
        switch (game)
        {
        case MiniGame::TreatCatch:
            return "Lead it with the mouse to catch the treats. Golden ones count three, pebbles make it dizzy.";
        case MiniGame::StarChase:
            return "Lead it with the mouse to the stars before they fade. Shooting stars count three, the storm cloud makes it dizzy.";
        case MiniGame::PearlDive:
            return "Lead it with the mouse to the pearls: two in an open clam, one in a bubble. Jellyfish make it dizzy.";
        case MiniGame::ShellGame:
            return "Watch the cup it hides under, then click that cup. Every round shuffles faster.";
        }
        return "";
    }

    std::uint32_t PlayBondOf(MiniGame game, int score)
    {
        constexpr std::uint32_t kForPlaying = 3;
        constexpr int kMostForScore = 7;
        int perMinute = 1;
        switch (game)
        {
        case MiniGame::TreatCatch:
            perMinute = 5;
            break;
        case MiniGame::StarChase:
        case MiniGame::PearlDive:
            perMinute = 7;
            break;
        case MiniGame::ShellGame:
            perMinute = 1;
            break;
        }
        return kForPlaying + static_cast<std::uint32_t>(std::clamp(score / perMinute, 0, kMostForScore));
    }

    std::uint32_t PlayBondLeft(const PlayRecord& record, std::int32_t today)
    {
        if (record.Day != today)
            return kPlayBondPerDay;
        return record.MinutesThatDay >= kPlayBondPerDay ? 0u : kPlayBondPerDay - record.MinutesThatDay;
    }

    std::uint32_t GrantPlayBond(PlayRecord& record, std::int32_t today, std::uint32_t minutes)
    {
        const std::uint32_t granted = std::min(minutes, PlayBondLeft(record, today));
        if (record.Day != today)
        {
            record.Day = today;
            record.MinutesThatDay = 0;
        }
        record.MinutesThatDay += granted;
        record.Minutes += granted;
        return granted;
    }

    TerrainGame::TerrainGame(MiniGame game, std::uint32_t seed)
        : m_game(game == MiniGame::ShellGame ? MiniGame::TreatCatch : game), m_random(seed == 0 ? 1u : seed)
    {
        m_ground = PlayGroundOf(m_game);
        m_x = std::round((kPlayWidth - kPlaySpriteWidth) * 0.5f);
        switch (m_game)
        {
        case MiniGame::StarChase:
            m_y = std::round(m_ground * 0.4f);
            m_nextSpecial = 5.0f;
            break;
        case MiniGame::PearlDive:
            m_y = std::round(m_ground * 0.45f);
            m_nextSpecial = 8.0f;
            for (const float share : { 0.14f, 0.38f, 0.62f, 0.86f })
            {
                PlayItem clam;
                clam.Kind = PlayItemKind::Clam;
                clam.X = std::round(kPlayWidth * share);
                clam.Y = m_ground - 2.0f;
                clam.Closed = Between(0.8f, 4.5f);
                m_items.push_back(clam);
            }
            break;
        default:
            m_y = m_ground - kPlaySpriteHeight;
            break;
        }
    }

    float TerrainGame::Between(float low, float high)
    {
        const auto share = static_cast<float>(m_random() - std::minstd_rand::min()) / static_cast<float>(std::minstd_rand::max() - std::minstd_rand::min());
        return low + (high - low) * share;
    }

    void TerrainGame::Step(double elapsed, float pointerX, float pointerY)
    {
        double left = std::clamp(elapsed, 0.0, 0.1);
        constexpr double kLongest = 1.0 / 60.0;
        while (left > 0.0 && !Over())
        {
            const double step = std::min(left, kLongest);
            StepOnce(static_cast<float>(step), pointerX, pointerY);
            left -= step;
        }
    }

    void TerrainGame::StepOnce(float elapsed, float pointerX, float pointerY)
    {
        m_time = std::min(kPlayRoundSeconds, m_time + elapsed);
        const auto progress = static_cast<float>(m_time / kPlayRoundSeconds);
        m_dizzy = std::max(0.0f, m_dizzy - elapsed);
        m_safe = std::max(0.0f, m_safe - elapsed);
        Move(elapsed, pointerX, pointerY);
        Spawn(elapsed, progress);

        for (PlayItem& item : m_items)
        {
            item.Age += elapsed;
            switch (item.Kind)
            {
            case PlayItemKind::Treat:
            case PlayItemKind::GoldenTreat:
            case PlayItemKind::Pebble:
                item.Y += item.VelocityY * elapsed;
                if (item.Kind == PlayItemKind::Pebble && Touches(item, 2.2f))
                {
                    Hurt(item, 1.1f);
                    item.Gone = true;
                }
                else if (item.Kind != PlayItemKind::Pebble && Touches(item, 2.5f))
                {
                    Catch(item, item.Kind == PlayItemKind::GoldenTreat ? 3 : 1);
                }
                else if (item.Y - 2.0f > m_ground)
                {
                    if (item.Kind != PlayItemKind::Pebble)
                        Missed();
                    item.Gone = true;
                }
                break;
            case PlayItemKind::Star:
                if (Touches(item, 3.0f))
                {
                    Catch(item, 1);
                }
                else if (item.Age >= item.Life)
                {
                    Missed();
                    item.Gone = true;
                }
                break;
            case PlayItemKind::ShootingStar:
            case PlayItemKind::StormCloud:
            case PlayItemKind::Jellyfish:
            case PlayItemKind::Pearl:
                item.X += item.VelocityX * elapsed;
                if (item.Kind == PlayItemKind::Jellyfish)
                    item.Y = item.BaseY + std::sin(item.Age * 1.7f) * 5.0f;
                else
                    item.Y += item.VelocityY * elapsed;
                if (item.Kind == PlayItemKind::ShootingStar && Touches(item, 3.0f))
                    Catch(item, 3);
                else if (item.Kind == PlayItemKind::Pearl && Touches(item, 2.5f))
                    Catch(item, 1);
                else if (item.Kind == PlayItemKind::StormCloud && Touches(item, 6.0f))
                    Hurt(item, 1.3f);
                else if (item.Kind == PlayItemKind::Jellyfish && Touches(item, 4.0f))
                    Hurt(item, 1.0f);
                if (item.X < -10.0f || item.X > kPlayWidth + 10.0f || item.Y < 2.0f || item.Y > m_ground + 4.0f)
                    item.Gone = true;
                break;
            case PlayItemKind::Clam:
                if (item.Open > 0.0f)
                {
                    PlayItem pearl = item;
                    pearl.Y -= 2.0f;
                    item.Open -= elapsed;
                    if (Touches(pearl, 3.0f))
                    {
                        Catch(item, 2);
                        item.Open = 0.0f;
                    }
                    if (item.Open <= 0.0f)
                    {
                        item.Open = 0.0f;
                        item.Closed = Between(3.0f, 6.0f);
                    }
                }
                else
                {
                    item.Closed -= elapsed;
                    if (item.Closed <= 0.0f)
                        item.Open = Mix(1.6f, 1.1f, progress);
                }
                break;
            }
        }
        std::erase_if(m_items, [](const PlayItem& item) { return item.Gone; });
    }

    void TerrainGame::Move(float elapsed, float pointerX, float pointerY)
    {
        const float targetX = std::clamp(pointerX - kPlaySpriteWidth * 0.5f, 0.0f, kPlayWidth - kPlaySpriteWidth);
        if (m_game == MiniGame::TreatCatch)
        {
            m_y = m_ground - kPlaySpriteHeight;
            if (m_dizzy > 0.0f)
                return;
            const float reach = 105.0f * elapsed;
            const float moved = std::clamp(targetX - m_x, -reach, reach);
            m_x += moved;
            if (std::fabs(moved) > 0.02f)
                m_direction = moved > 0.0f ? 1 : -1;
            return;
        }

        const bool flies = m_game == MiniGame::StarChase;
        const float top = flies ? 1.0f : 5.0f;
        const float bottom = m_ground - kPlaySpriteHeight + (flies ? 3.0f : 1.0f);
        const float targetY = std::clamp(pointerY - kPlaySpriteHeight * 0.5f, top, bottom);
        const float fastest = flies ? 115.0f : 80.0f;
        const float grip = flies ? 9.0f : 5.0f;
        float wantX = (targetX - m_x) * 7.0f;
        float wantY = (targetY - m_y) * 7.0f;
        if (m_dizzy > 0.0f)
            wantX = wantY = 0.0f;
        if (const float speed = std::hypot(wantX, wantY); speed > fastest)
        {
            wantX *= fastest / speed;
            wantY *= fastest / speed;
        }
        const float follow = std::min(1.0f, grip * elapsed);
        m_velocityX += (wantX - m_velocityX) * follow;
        m_velocityY += (wantY - m_velocityY) * follow;
        m_x += m_velocityX * elapsed;
        m_y += m_velocityY * elapsed;
        if (m_x < 0.0f || m_x > kPlayWidth - kPlaySpriteWidth)
        {
            m_x = std::clamp(m_x, 0.0f, kPlayWidth - kPlaySpriteWidth);
            m_velocityX = 0.0f;
        }
        if (m_y < top || m_y > bottom)
        {
            m_y = std::clamp(m_y, top, bottom);
            m_velocityY = 0.0f;
        }
        if (std::fabs(m_velocityX) > 4.0f)
            m_direction = m_velocityX > 0.0f ? 1 : -1;
    }

    void TerrainGame::Spawn(float elapsed, float progress)
    {
        m_nextItem -= elapsed;
        m_nextSpecial -= elapsed;
        const auto count = [&](PlayItemKind kind) { return std::count_if(m_items.begin(), m_items.end(), [&](const PlayItem& item) { return item.Kind == kind; }); };
        const auto fromSide = [&](PlayItemKind kind, float margin, float y, float speed)
            {
                const bool fromLeft = Between(0.0f, 1.0f) < 0.5f;
                PlayItem item;
                item.Kind = kind;
                item.X = fromLeft ? -margin : kPlayWidth + margin;
                item.Y = item.BaseY = y;
                item.VelocityX = fromLeft ? speed : -speed;
                m_items.push_back(item);
                return &m_items.back();
            };

        switch (m_game)
        {
        case MiniGame::TreatCatch:
        {
            if (m_nextItem > 0.0f)
                break;
            m_nextItem = Mix(0.95f, 0.42f, progress) * Between(0.8f, 1.2f);
            const float pebbles = Mix(0.04f, 0.24f, progress);
            const float roll = Between(0.0f, 1.0f);
            PlayItem item;
            item.Kind = roll < pebbles ? PlayItemKind::Pebble : roll < pebbles + 0.1f ? PlayItemKind::GoldenTreat : PlayItemKind::Treat;
            item.X = Between(6.0f, kPlayWidth - 6.0f);
            item.Y = -3.0f;
            item.VelocityY = Mix(24.0f, 44.0f, progress) * (item.Kind == PlayItemKind::GoldenTreat ? 1.3f : 1.0f) * Between(0.9f, 1.1f);
            m_items.push_back(item);
            break;
        }
        case MiniGame::StarChase:
        {
            if (m_nextItem <= 0.0f && count(PlayItemKind::Star) < 3)
            {
                m_nextItem = Mix(1.1f, 0.7f, progress);
                PlayItem star;
                star.Kind = PlayItemKind::Star;
                star.Life = Mix(3.6f, 2.3f, progress);
                const float middleX = m_x + kPlaySpriteWidth * 0.5f;
                const float middleY = m_y + kPlaySpriteHeight * 0.5f;
                for (int attempt = 0; attempt < 6; ++attempt)
                {
                    star.X = Between(8.0f, kPlayWidth - 8.0f);
                    star.Y = Between(6.0f, m_ground - 8.0f);
                    if (std::hypot(star.X - middleX, star.Y - middleY) > 26.0f)
                        break;
                }
                m_items.push_back(star);
            }
            if (m_nextSpecial <= 0.0f)
            {
                m_nextSpecial = Between(4.5f, 7.5f);
                if (m_time > 12.0 && count(PlayItemKind::StormCloud) == 0 && Between(0.0f, 1.0f) < 0.5f)
                    fromSide(PlayItemKind::StormCloud, 8.0f, Between(12.0f, m_ground - 16.0f), 10.0f + 10.0f * progress);
                else
                    fromSide(PlayItemKind::ShootingStar, 4.0f, Between(6.0f, 26.0f), Between(60.0f, 75.0f))->VelocityY = Between(8.0f, 16.0f);
            }
            break;
        }
        case MiniGame::PearlDive:
        {
            if (m_nextItem <= 0.0f)
            {
                m_nextItem = Mix(2.0f, 1.3f, progress);
                PlayItem pearl;
                pearl.Kind = PlayItemKind::Pearl;
                pearl.X = Between(8.0f, kPlayWidth - 8.0f);
                pearl.Y = m_ground - 3.0f;
                pearl.VelocityY = -Between(14.0f, 19.0f);
                m_items.push_back(pearl);
            }
            if (m_nextSpecial <= 0.0f)
            {
                m_nextSpecial = Between(5.0f, 9.0f);
                if (count(PlayItemKind::Jellyfish) < (progress > 0.5f ? 2 : 1))
                    fromSide(PlayItemKind::Jellyfish, 6.0f, Between(14.0f, m_ground - 18.0f), 8.0f + 7.0f * progress);
            }
            break;
        }
        case MiniGame::ShellGame:
            break;
        }
    }

    bool TerrainGame::Touches(const PlayItem& item, float radius) const
    {
        const float left = m_x + 2.0f;
        const float right = m_x + kPlaySpriteWidth - 2.0f;
        const float top = m_y + 1.0f;
        const float bottom = m_y + kPlaySpriteHeight - 1.0f;
        const float nearestX = std::clamp(item.X, left, right);
        const float nearestY = std::clamp(item.Y, top, bottom);
        return std::hypot(item.X - nearestX, item.Y - nearestY) <= radius;
    }

    void TerrainGame::Catch(PlayItem& item, int points)
    {
        ++m_streak;
        if (m_streak % kStreakLength == 0)
            points += kStreakBonus;
        m_score += points;
        m_catches.push_back({ item.Kind, item.X, item.Y, points });
        if (item.Kind != PlayItemKind::Clam)
            item.Gone = true;
    }

    void TerrainGame::Hurt(const PlayItem& item, float seconds)
    {
        if (m_dizzy > 0.0f || m_safe > 0.0f)
            return;
        m_dizzy = seconds;
        m_safe = seconds + 1.0f;
        m_streak = 0;
        m_velocityX = m_velocityY = 0.0f;
        m_catches.push_back({ item.Kind, item.X, item.Y, 0 });
    }

    void TerrainGame::Missed()
    {
        m_streak = 0;
    }

    namespace
    {
        constexpr double kCoverSeconds = 0.35;
        constexpr double kRevealSeconds = 1.4;

        float Ease(float share)
        {
            return share * share * (3.0f - 2.0f * share);
        }
    }

    ShellGame::ShellGame(std::uint32_t seed) : m_random(seed == 0 ? 1u : seed)
    {
        m_hiding = static_cast<int>(m_random() % kCups);
        BeginRound();
    }

    void ShellGame::BeginRound()
    {
        m_phase = Phase::Show;
        m_phaseTime = 0.0;
        m_picked = -1;
        m_swap = 0;
        const int swaps = std::min(12, 2 + m_round);
        m_swapSeconds = std::max(0.17f, 0.6f - 0.045f * static_cast<float>(m_round - 1));
        m_swaps.clear();
        for (int index = 0; index < swaps; ++index)
        {
            const int first = static_cast<int>(m_random() % kCups);
            const int second = (first + 1 + static_cast<int>(m_random() % (kCups - 1))) % kCups;
            m_swaps.emplace_back(first, second);
        }
    }

    float ShellGame::Share() const
    {
        double length = 1.0;
        switch (m_phase)
        {
        case Phase::Show:
            length = m_round == 1 ? 1.4 : 0.9;
            break;
        case Phase::Cover:
            length = kCoverSeconds;
            break;
        case Phase::Shuffle:
            length = m_swapSeconds;
            break;
        case Phase::Reveal:
            length = kRevealSeconds;
            break;
        default:
            return 1.0f;
        }
        return static_cast<float>(std::clamp(m_phaseTime / length, 0.0, 1.0));
    }

    void ShellGame::Step(double elapsed)
    {
        m_phaseTime += std::clamp(elapsed, 0.0, 0.25);
        for (int guard = 0; guard < 64 && Share() >= 1.0f && m_phase != Phase::Pick && m_phase != Phase::Over; ++guard)
        {
            switch (m_phase)
            {
            case Phase::Show:
                m_phaseTime -= m_round == 1 ? 1.4 : 0.9;
                m_phase = Phase::Cover;
                break;
            case Phase::Cover:
                m_phaseTime -= kCoverSeconds;
                m_phase = Phase::Shuffle;
                break;
            case Phase::Shuffle:
            {
                m_phaseTime -= m_swapSeconds;
                const auto [first, second] = m_swaps[m_swap];
                for (int& slot : m_slot)
                {
                    if (slot == first)
                        slot = second;
                    else if (slot == second)
                        slot = first;
                }
                if (++m_swap >= m_swaps.size())
                {
                    m_phase = Phase::Pick;
                    m_phaseTime = 0.0;
                }
                break;
            }
            case Phase::Reveal:
                if (m_picked == m_hiding)
                {
                    ++m_round;
                    BeginRound();
                }
                else
                {
                    m_phase = Phase::Over;
                    m_phaseTime = 0.0;
                }
                break;
            default:
                break;
            }
        }
    }

    bool ShellGame::PickSlot(int slot)
    {
        if (m_phase != Phase::Pick || slot < 0 || slot >= kCups)
            return false;
        for (int cup = 0; cup < kCups; ++cup)
        {
            if (m_slot[static_cast<std::size_t>(cup)] == slot)
                m_picked = cup;
        }
        m_phase = Phase::Reveal;
        m_phaseTime = 0.0;
        return true;
    }

    float ShellGame::CupPosition(int cup) const
    {
        const auto slot = static_cast<float>(m_slot[static_cast<std::size_t>(cup)]);
        if (m_phase != Phase::Shuffle || m_swap >= m_swaps.size())
            return slot;
        const auto [first, second] = m_swaps[m_swap];
        const float share = Ease(Share());
        if (m_slot[static_cast<std::size_t>(cup)] == first)
            return Mix(static_cast<float>(first), static_cast<float>(second), share);
        if (m_slot[static_cast<std::size_t>(cup)] == second)
            return Mix(static_cast<float>(second), static_cast<float>(first), share);
        return slot;
    }

    float ShellGame::CupDepth(int cup) const
    {
        if (m_phase != Phase::Shuffle || m_swap >= m_swaps.size())
            return 0.0f;
        const auto [first, second] = m_swaps[m_swap];
        const int from = m_slot[static_cast<std::size_t>(cup)];
        if (from != first && from != second)
            return 0.0f;
        const int to = from == first ? second : first;
        return (to > from ? 1.0f : -1.0f) * std::sin(Share() * kPi);
    }

    float ShellGame::CupLift(int cup) const
    {
        const float share = Share();
        switch (m_phase)
        {
        case Phase::Show:
            return cup == m_hiding ? 1.0f : 0.0f;
        case Phase::Cover:
            return cup == m_hiding ? 1.0f - Ease(share) : 0.0f;
        case Phase::Reveal:
            if (cup == m_picked)
                return std::min(1.0f, share * 3.0f);
            if (cup == m_hiding)
                return std::clamp((share - 0.35f) * 3.0f, 0.0f, 1.0f);
            return 0.0f;
        case Phase::Over:
            return cup == m_picked || cup == m_hiding ? 1.0f : 0.0f;
        default:
            return 0.0f;
        }
    }
}
