#pragma once

#include "Core/Creatures.h"

#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace Idler
{
    enum class Habit
    {
        Walks,
        Floats,
        Swims,
        Stays
    };

    struct TerrariumGuest
    {
        std::uint32_t Seed = 0;
        Idler::Habit Habit = Habit::Walks;
        bool Asleep = false;
        CreatureId Creature = CreatureId::Slime;
        int Bond = 0;
        std::uint32_t Evolutions = 0;
    };

    enum class ResidentState
    {
        Entering,
        Resting,
        Moving,
        Leaving,
        Held,
        Settling,
        Gone
    };

    enum class TerrariumEncounter
    {
        None,
        Seeking,
        Awaiting,
        Greeting,
        Chasing,
        Fleeing,
        Leaping
    };

    enum class TerrariumEmote
    {
        None,
        Heart,
        Alarm,
        Splash,
        Dots,
        Note,
        Question,
        Stars,
        Anger
    };

    enum class TerrariumReaction
    {
        None,
        Joy,
        Shrug,
        Dodging,
        Snapping,
        Puffed,
        Eager,
        Furious,
        Scared,
        Dizzy,
        Sulking,
        Puzzled,
        Visiting,
        Evolving,
        Startled,
        Tugging
    };

    enum class FetchStep : std::uint8_t
    {
        None,
        Waiting,
        ToBall,
        Back
    };

    struct TerrariumResident
    {
        std::uint32_t Seed = 0;
        Idler::Habit Habit = Habit::Walks;
        bool Asleep = false;
        CreatureId Creature = CreatureId::Slime;
        TerrariumEncounter Encounter = TerrariumEncounter::None;
        std::uint32_t Partner = 0;
        double EncounterBegan = 0.0;
        double EncounterEnds = 0.0;
        float Lift = 0.0f;
        TerrariumEmote Emote = TerrariumEmote::None;
        float EmoteAge = 0.0f;
        bool Huddling = false;
        int Bond = 0;
        TerrariumReaction Reaction = TerrariumReaction::None;
        double ReactionBegan = 0.0;
        double ReactionEnds = 0.0;
        float ShiftX = 0.0f;
        float Squash = 0.0f;
        int PetStreak = 0;
        double LastPet = -1000.0;
        bool WasThrown = false;
        double HeldSince = 0.0;
        std::uint32_t Evolutions = 0;
        bool ShowsFirstForm = false;
        ResidentState State = ResidentState::Entering;
        float X = 0.0f;
        float Y = 0.0f;
        float TargetX = 0.0f;
        float TargetY = 0.0f;
        int Direction = 1;
        double RestEnds = 0.0;
        float FallSpeed = 0.0f;
        float VelocityX = 0.0f;
        float VelocityY = 0.0f;
        float GroundLine = -1.0f;
        FetchStep Fetch = FetchStep::None;
        double FetchStarts = 0.0;
        bool FetchEager = false;
        bool Bare = false;
        bool BareNoticed = false;
        double GoneUntil = 0.0;
        double NextBurst = 0.0;
        bool Hiding = false;
        bool HasTugged = false;
        bool BurstArmed = false;
    };

    struct TerrariumTug
    {
        bool Active = false;
        std::uint32_t Seed = 0;
        float X = 0.0f;
        float Y = 0.0f;
        float FeetX = 0.0f;
        float FeetY = 0.0f;
    };

    struct TerrariumCover
    {
        float X = 0.0f;
        float Ground = 0.0f;
        float Foot = 0.0f;
        float Cover = 0.0f;
    };

    struct TerrariumMess
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Radius = 0.0f;
        double Began = 0.0;
        double Ends = 0.0;
        std::uint32_t Seed = 0;
    };
    inline constexpr float kMessDepthShare = 0.4f;

    enum class BallState : std::uint8_t
    {
        None,
        Lying,
        Held,
        Flying,
        Carried
    };

    inline constexpr float kBallRadius = 2.5f;
    inline constexpr float kShellRadius = 5.0f;

    struct TerrariumThing
    {
        BallState State = BallState::None;
        float X = 0.0f;
        float Y = 0.0f;
        float Floor = 0.0f;
        float VelocityX = 0.0f;
        float VelocityY = 0.0f;
        float Turn = 0.0f;
        float Radius = kBallRadius;
        float GrabbedX = 0.0f;
        float GrabbedY = 0.0f;
    };

    struct TerrariumBall : TerrariumThing
    {
        std::uint32_t Fetcher = 0;
        float ThrownFromX = 0.0f;
    };

    struct TerrariumShell : TerrariumThing
    {
        std::uint32_t Owner = 0;
    };

    inline constexpr int kMaxTerrariumResidents = 10;
    inline constexpr float kTerrariumGroundDepth = 58.0f;
    inline constexpr float kTerrariumLakeDepth = 20.0f;

    struct TerrariumSize
    {
        float Width = 0.0f;
        float Height = 0.0f;
        float SpriteWidth = 0.0f;
        float SpriteHeight = 0.0f;
        float GroundDepth = kTerrariumGroundDepth;
        float LakeDepth = kTerrariumLakeDepth;
    };

    class Terrarium
    {
    public:
        explicit Terrarium(std::uint32_t randomSeed = 1u, int capacity = kMaxTerrariumResidents);

        void Update(const std::vector<TerrariumGuest>& guests, const TerrariumSize& size, double elapsed, bool animated);
        const std::vector<TerrariumResident>& Residents() const { return m_residents; }

        void Grab(std::uint32_t seed);
        void Carry(std::uint32_t seed, float x, float y, double elapsed = 0.0);
        void Drop(std::uint32_t seed, const TerrariumSize& size);

        PetOutcome Pet(std::uint32_t seed);
        void SetPointer(bool known, float x, float y);
        void SetTugAllowed(bool allowed) { m_tugAllowed = allowed; }
        TerrariumTug Tug() const;
        void LetGoOfHand(bool fought);

        void SetBall(bool present, const TerrariumSize& size);
        const TerrariumBall& Ball() const { return m_ball; }
        bool GrabBall(float x, float y);
        void CarryBall(float x, float y, double elapsed);
        void DropBall(const TerrariumSize& size);
        std::vector<std::uint32_t> TakeFetches() { return std::exchange(m_fetches, {}); }
        const TerrariumShell& Shell() const { return m_shell; }
        const std::vector<TerrariumMess>& Messes() const { return m_messes; }
        double Clock() const { return m_clock; }
        bool InMess(float x, float ground, float margin = 0.0f) const;
        void SetCovers(std::vector<TerrariumCover> covers) { m_covers = std::move(covers); }
        const std::vector<TerrariumCover>& Covers() const { return m_covers; }
        bool InFoot(float x, float ground, float margin = 0.0f) const;
        bool GrabShell(float x, float y);
        void CarryShell(float x, float y, double elapsed);
        void DropShell(const TerrariumSize& size);

    private:
        void Fit(const TerrariumSize& size);
        float Between(float low, float high);
        void PlaceTarget(TerrariumResident& resident, const TerrariumSize& size, bool roam);
        void Admit(const TerrariumGuest& guest, const TerrariumSize& size, bool animated);
        const TerrariumGuest* NextWaiting(const std::vector<TerrariumGuest>& guests);
        void SendAway(TerrariumResident& resident, const TerrariumSize& size);
        TerrariumResident* Find(std::uint32_t seed);
        void Settle(TerrariumResident& resident, const TerrariumSize& size, float elapsed);
        void ComeToRest(TerrariumResident& resident);
        bool IsResident(std::uint32_t seed) const;
        void StepEncounters(const TerrariumSize& size);
        void BeginEncounter(const TerrariumSize& size);
        void BeginGreeting(TerrariumResident& first, TerrariumResident& second, double seconds, TerrariumEmote firstEmote, TerrariumEmote secondEmote);
        void PlaceBeside(TerrariumResident& resident, const TerrariumResident& other, const TerrariumSize& size, float gap);
        void PlaceFlight(TerrariumResident& resident, const TerrariumResident& chaser, const TerrariumSize& size);
        void HuddleUp(TerrariumResident& resident, const TerrariumSize& size);
        void EndEncounter(TerrariumResident& resident);
        void Release(TerrariumResident& resident);
        void BeginReaction(TerrariumResident& resident, TerrariumReaction reaction, double seconds);
        void EndReaction(TerrariumResident& resident);
        void ReactToThrow(TerrariumResident& resident);
        void StepReactions(const TerrariumSize& size, float elapsed);
        void StepBall(const TerrariumSize& size, float elapsed);
        void ChooseFetcher(const TerrariumSize& size);
        void EndFetch(TerrariumResident& resident);
        void ArriveFetching(TerrariumResident& resident, const TerrariumSize& size);
        std::pair<float, float> ThingFloors(const TerrariumSize& size, float radius) const;
        bool TakeThing(TerrariumThing& thing, float x, float y);
        void MoveThing(TerrariumThing& thing, float x, float y, double elapsed);
        bool ThrowThing(TerrariumThing& thing, const TerrariumSize& size);
        bool StepThing(TerrariumThing& thing, const TerrariumSize& size, float elapsed);
        void KnockShell(TerrariumResident& resident, float impact, const TerrariumSize& size);
        void StepShell(const TerrariumSize& size);
        void ArriveAtShell(TerrariumResident& resident, const TerrariumSize& size);
        bool HardHit(TerrariumResident& resident, float impact, bool ground, const TerrariumSize& size);
        void Burst(TerrariumResident& resident, const TerrariumSize& size);
        void ComeBack(TerrariumResident& resident, const TerrariumSize& size);
        void StepBursts(const TerrariumSize& size);
        void Startle(TerrariumResident& resident, const TerrariumSize& size);
        bool OnTheLand(const TerrariumResident& resident) const;
        void StartTug(TerrariumResident& resident, const TerrariumSize& size);

        std::minstd_rand m_random;
        int m_capacity = kMaxTerrariumResidents;
        TerrariumSize m_size;
        std::vector<TerrariumResident> m_residents;
        std::size_t m_nextGuest = 0;
        double m_clock = 0.0;
        double m_nextArrival = 0.0;
        double m_nextTurn = 0.0;
        double m_nextEncounter = 0.0;
        double m_nextVisit = 0.0;
        bool m_pointerKnown = false;
        float m_pointerX = 0.0f;
        float m_pointerY = 0.0f;
        TerrariumBall m_ball;
        bool m_ballThrown = false;
        double m_ballWaitsUntil = 0.0;
        std::vector<std::uint32_t> m_fetches;
        TerrariumShell m_shell;
        std::vector<TerrariumMess> m_messes;
        std::vector<TerrariumCover> m_covers;
        bool m_tugAllowed = false;
        std::uint32_t m_tugSeed = 0;
    };
}
