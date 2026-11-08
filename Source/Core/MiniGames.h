#pragma once

#include "Core/Creatures.h"
#include "Core/Terrarium.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace Idler
{
    enum class MiniGame : std::uint8_t
    {
        TreatCatch,
        StarChase,
        PearlDive,
        ShellGame
    };
    inline constexpr std::size_t kMiniGameCount = 4;

    MiniGame TerrainGameOf(Habit habit);
    const char* MiniGameName(MiniGame game);
    const char* MiniGameRules(MiniGame game);

    inline constexpr std::uint32_t kPlayBondPerDay = 30;
    inline constexpr std::uint32_t kFetchBond = 1;
    std::uint32_t PlayBondOf(MiniGame game, int score);

    struct PlayRecord
    {
        std::uint32_t Minutes = 0;
        std::int32_t Day = 0;
        std::uint32_t MinutesThatDay = 0;
        std::uint32_t Games = 0;
        std::uint32_t Fetches = 0;
        std::array<std::uint32_t, kMiniGameCount> Best{};
    };
    std::uint32_t PlayBondLeft(const PlayRecord& record, std::int32_t today);
    std::uint32_t GrantPlayBond(PlayRecord& record, std::int32_t today, std::uint32_t minutes);

    inline constexpr float kPlayWidth = 176.0f;
    inline constexpr float kPlayHeight = 92.0f;
    inline constexpr float kPlaySpriteWidth = 16.0f;
    inline constexpr float kPlaySpriteHeight = 14.0f;
    inline constexpr double kPlayRoundSeconds = 45.0;
    float PlayGroundOf(MiniGame game);

    enum class PlayItemKind : std::uint8_t
    {
        Treat,
        GoldenTreat,
        Pebble,
        Star,
        ShootingStar,
        StormCloud,
        Pearl,
        Clam,
        Jellyfish
    };

    struct PlayItem
    {
        PlayItemKind Kind = PlayItemKind::Treat;
        float X = 0.0f;
        float Y = 0.0f;
        float VelocityX = 0.0f;
        float VelocityY = 0.0f;
        float Age = 0.0f;
        float Life = 0.0f;
        float Open = 0.0f;
        float Closed = 0.0f;
        float BaseY = 0.0f;
        bool Gone = false;
    };

    struct PlayCatch
    {
        PlayItemKind Kind = PlayItemKind::Treat;
        float X = 0.0f;
        float Y = 0.0f;
        int Points = 0;
    };

    class TerrainGame
    {
    public:
        TerrainGame(MiniGame game, std::uint32_t seed);

        void Step(double elapsed, float pointerX, float pointerY);

        MiniGame Kind() const { return m_game; }
        bool Over() const { return m_time >= kPlayRoundSeconds; }
        double TimeLeft() const { return m_time >= kPlayRoundSeconds ? 0.0 : kPlayRoundSeconds - m_time; }
        int Score() const { return m_score; }
        int Streak() const { return m_streak; }
        float X() const { return m_x; }
        float Y() const { return m_y; }
        int Direction() const { return m_direction; }
        float Dizzy() const { return m_dizzy; }
        float Ground() const { return m_ground; }
        const std::vector<PlayItem>& Items() const { return m_items; }
        std::vector<PlayCatch> TakeCatches() { return std::exchange(m_catches, {}); }

    private:
        void StepOnce(float elapsed, float pointerX, float pointerY);
        void Move(float elapsed, float pointerX, float pointerY);
        void Spawn(float elapsed, float progress);
        bool Touches(const PlayItem& item, float radius) const;
        void Catch(PlayItem& item, int points);
        void Hurt(const PlayItem& item, float seconds);
        void Missed();
        float Between(float low, float high);

        MiniGame m_game;
        std::minstd_rand m_random;
        double m_time = 0.0;
        int m_score = 0;
        int m_streak = 0;
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_velocityX = 0.0f;
        float m_velocityY = 0.0f;
        int m_direction = 1;
        float m_dizzy = 0.0f;
        float m_safe = 0.0f;
        float m_ground = 0.0f;
        float m_nextItem = 0.6f;
        float m_nextSpecial = 0.0f;
        std::vector<PlayItem> m_items;
        std::vector<PlayCatch> m_catches;
    };

    class ShellGame
    {
    public:
        enum class Phase : std::uint8_t
        {
            Show,
            Cover,
            Shuffle,
            Pick,
            Reveal,
            Over
        };
        static constexpr int kCups = 3;

        explicit ShellGame(std::uint32_t seed);

        void Step(double elapsed);
        bool PickSlot(int slot);

        Phase CurrentPhase() const { return m_phase; }
        bool Over() const { return m_phase == Phase::Over; }
        int Round() const { return m_round; }
        int Score() const { return m_round - 1; }
        int HidingCup() const { return m_hiding; }
        int PickedCup() const { return m_picked; }
        int SlotOf(int cup) const { return m_slot[static_cast<std::size_t>(cup)]; }
        float CupPosition(int cup) const;
        float CupLift(int cup) const;
        float CupDepth(int cup) const;
        int SwapsThisRound() const { return static_cast<int>(m_swaps.size()); }
        float SwapSeconds() const { return m_swapSeconds; }

    private:
        void BeginRound();
        float Share() const;

        std::minstd_rand m_random;
        Phase m_phase = Phase::Show;
        double m_phaseTime = 0.0;
        int m_round = 1;
        int m_hiding = 0;
        int m_picked = -1;
        std::array<int, kCups> m_slot{ 0, 1, 2 };
        std::vector<std::pair<int, int>> m_swaps;
        std::size_t m_swap = 0;
        float m_swapSeconds = 0.6f;
    };
}
