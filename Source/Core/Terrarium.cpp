#include "Core/Terrarium.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace Idler
{
    namespace
    {
        constexpr float kWalkSpeed = 7.0f;
        constexpr float kFloatSpeed = 10.0f;
        constexpr float kFlyHomeSpeed = 18.0f;
        constexpr float kSwimSpeed = 6.0f;
        constexpr float kLeaveSpeed = 16.0f;
        constexpr float kGravity = 220.0f;
        constexpr float kSinkSpeed = 30.0f;
        constexpr float kSinkAwaySpeed = 12.0f;
        constexpr float kThrowMemorySeconds = 0.06f;
        constexpr float kMaxThrowSpeed = 320.0f;
        constexpr float kSettledSpeed = 6.0f;
        constexpr float kThrowDepthSeconds = 0.12f;
        constexpr float kWallBounce = 0.55f;
        constexpr float kGroundBounce = 0.3f;
        constexpr float kMinBounceSpeed = 45.0f;
        constexpr float kGroundFriction = 5.0f;
        constexpr float kAirDrag = 2.5f;
        constexpr float kWaterDrag = 6.0f;
        constexpr float kRiseShare = 0.7f;
        constexpr float kPaddleSpeed = 60.0f;
        constexpr float kRoam = 34.0f;
        constexpr float kMinRestSeconds = 1.5f;
        constexpr float kMaxRestSeconds = 6.0f;
        constexpr double kArrivalSeconds = 0.5;
        constexpr float kMinTurnSeconds = 18.0f;
        constexpr float kMaxTurnSeconds = 35.0f;
        constexpr float kMinEncounterSeconds = 6.0f;
        constexpr float kMaxEncounterSeconds = 14.0f;
        constexpr double kSeekSeconds = 14.0;
        constexpr double kGreetSeconds = 2.4;
        constexpr double kCaughtSeconds = 1.2;
        constexpr double kHopSeconds = 0.6;
        constexpr double kHeartEverySeconds = 0.8;
        constexpr float kHopHeight = 2.5f;
        constexpr float kMinChaseSeconds = 4.0f;
        constexpr float kMaxChaseSeconds = 8.0f;
        constexpr float kChaseSpeed = 15.0f;
        constexpr float kFleeSpeed = 11.0f;
        constexpr float kCaughtDistance = 9.0f;
        constexpr double kLeapSeconds = 1.1;
        constexpr double kSplashShare = 0.3;
        constexpr float kLeapMargin = 4.0f;
        constexpr float kBesideShare = 0.8f;
        constexpr float kHuddleShare = 0.65f;
        constexpr float kHuddleReach = 80.0f;
        constexpr double kPetStreakSeconds = 12.0;
        constexpr double kJoySeconds = 0.9;
        constexpr double kJoyHopSeconds = 0.36;
        constexpr float kJoyHopHeight = 3.0f;
        constexpr double kDodgeSeconds = 1.4;
        constexpr float kDodgeDistance = 14.0f;
        constexpr double kSnapSeconds = 1.2;
        constexpr double kPuffSeconds = 1.4;
        constexpr double kEagerSeconds = 8.0;
        constexpr double kFuriousSeconds = 7.0;
        constexpr double kScaredSeconds = 6.0;
        constexpr double kDizzySeconds = 3.5;
        constexpr double kSulkSeconds = 5.0;
        constexpr double kStareSeconds = 2.5;
        constexpr double kPuzzledSeconds = 1.4;
        constexpr double kSlipSeconds = 0.5;
        constexpr double kEvolveFlickerSeconds = 1.6;
        constexpr double kEvolveSeconds = kEvolveFlickerSeconds + kJoySeconds;
        constexpr float kEagerSpeed = 24.0f;
        constexpr float kBallAirDrag = 0.4f;
        constexpr float kBallBounce = 0.55f;
        constexpr float kBallRollFriction = 2.5f;
        constexpr float kBallRestSpeed = 3.0f;
        constexpr float kBallThrowSpeed = 25.0f;
        constexpr float kBallThrowDistance = 6.0f;
        constexpr float kFetchSpeed = 14.0f;
        constexpr double kFetchHesitation = 1.5;
        constexpr int kShyUntilBond = 2;
        constexpr double kBallWaitSeconds = 4.0;
        constexpr float kShellOffSpeed = 170.0f;
        constexpr float kSnailShellX = 11.0f;
        constexpr float kSnailShellY = 6.0f;
        constexpr float kCrawlSpeed = 5.0f;
        constexpr double kBareSulkSeconds = 1.6;
        constexpr float kBurstSpeed = 170.0f;
        constexpr double kGoneSeconds = 30.0;
        constexpr double kMessSeconds = 27.0;
        constexpr float kMessRadius = 30.0f;
        constexpr double kMinBurstSeconds = 120.0;
        constexpr double kMaxBurstSeconds = 360.0;
        constexpr double kStartledSeconds = 1.8;
        constexpr float kHideShare = 0.12f;
        constexpr float kShyHideShare = 0.4f;
        constexpr float kHidingRestFactor = 3.0f;
        constexpr float kTugCells = 34.0f;
        constexpr float kTugSpeed = 24.0f;
        constexpr double kTugSeconds = 1.8;
        constexpr float kFuriousSpeed = 20.0f;
        constexpr float kScaredSpeed = 18.0f;
        constexpr float kDodgeSpeed = 16.0f;
        constexpr float kPointerNear = 4.0f;
        constexpr float kScaredReach = 45.0f;
        constexpr float kLookReach = 50.0f;
        constexpr int kComesOverFromBond = 4;
        constexpr float kVisitReach = 70.0f;
        constexpr double kVisitSeconds = 6.0;
        constexpr float kMinVisitWait = 15.0f;
        constexpr float kMaxVisitWait = 30.0f;
        constexpr float kSquashPerSpeed = 1.0f / 300.0f;
        constexpr float kMinSquash = 0.25f;
        constexpr float kMaxSquash = 0.7f;
        constexpr float kSquashEase = 4.0f;
        constexpr float kSulkSquashEase = 0.25f;

        float GroundTop(const TerrariumSize& size)
        {
            return std::max(0.0f, size.Height - size.SpriteHeight - size.GroundDepth);
        }

        std::pair<float, float> BandOf(Habit habit, const TerrariumSize& size)
        {
            if (habit == Habit::Floats)
                return { 2.0f, std::max(2.0f, GroundTop(size) - 8.0f) };
            if (habit == Habit::Swims)
            {
                const float top = std::max(0.0f, size.Height - size.LakeDepth + 1.0f);
                return { top, std::max(top, size.Height - size.SpriteHeight - 1.0f) };
            }
            return { GroundTop(size), std::max(GroundTop(size), size.Height - size.SpriteHeight - size.LakeDepth - 1.0f) };
        }

        using BoxMarks = std::array<float, 6>;

        BoxMarks MarksOf(const TerrariumSize& size)
        {
            const auto [skyLow, skyHigh] = BandOf(Habit::Floats, size);
            const auto [landLow, landHigh] = BandOf(Habit::Walks, size);
            const auto [lakeLow, lakeHigh] = BandOf(Habit::Swims, size);
            BoxMarks marks = { skyLow, skyHigh, landLow, landHigh, lakeLow, lakeHigh };
            for (std::size_t index = 1; index < marks.size(); ++index)
                marks[index] = std::max(marks[index], marks[index - 1]);
            return marks;
        }

        float MapHeight(float y, const BoxMarks& from, const BoxMarks& to)
        {
            if (y <= from.front())
                return y + to.front() - from.front();
            for (std::size_t index = 1; index < from.size(); ++index)
            {
                if (y > from[index])
                    continue;
                const float span = from[index] - from[index - 1];
                const float share = span > 0.0f ? (y - from[index - 1]) / span : 1.0f;
                return to[index - 1] + (to[index] - to[index - 1]) * share;
            }
            return y + to.back() - from.back();
        }

        float MapAcross(float x, float fromRight, float toRight)
        {
            if (x <= 0.0f)
                return x;
            return x >= fromRight ? x - fromRight + toRight : x / fromRight * toRight;
        }

        Habit PlaceOf(const TerrariumResident& resident)
        {
            return resident.Habit == Habit::Floats && resident.Asleep ? Habit::Walks : resident.Habit;
        }

        bool SameLayer(Habit lhs, Habit rhs)
        {
            const auto layer = [](Habit habit) { return habit == Habit::Stays ? Habit::Walks : habit; };
            return layer(lhs) == layer(rhs);
        }

        float SpeedOf(const TerrariumResident& resident)
        {
            if (resident.Habit == Habit::Stays)
                return resident.State == ResidentState::Leaving ? kSinkAwaySpeed : kSinkSpeed;
            if (resident.State == ResidentState::Leaving)
                return kLeaveSpeed;
            return resident.Habit == Habit::Floats ? kFloatSpeed : resident.Habit == Habit::Swims ? kSwimSpeed : kWalkSpeed;
        }
    }

    Terrarium::Terrarium(std::uint32_t randomSeed, int capacity)
        : m_random(randomSeed), m_capacity(capacity)
    {
    }

    TerrariumResident* Terrarium::Find(std::uint32_t seed)
    {
        const auto found = std::find_if(m_residents.begin(), m_residents.end(), [&](const TerrariumResident& resident) { return resident.Seed == seed; });
        return found != m_residents.end() ? &*found : nullptr;
    }

    void Terrarium::Grab(std::uint32_t seed)
    {
        TerrariumResident* resident = Find(seed);
        if (!resident || resident->State == ResidentState::Leaving || resident->State == ResidentState::Gone)
            return;
        EndEncounter(*resident);
        resident->Huddling = false;
        resident->Reaction = TerrariumReaction::None;
        resident->Lift = 0.0f;
        resident->ShiftX = 0.0f;
        resident->Emote = TerrariumEmote::None;
        resident->HeldSince = m_clock;
        resident->State = ResidentState::Held;
        resident->FallSpeed = 0.0f;
        resident->VelocityX = 0.0f;
        resident->VelocityY = 0.0f;
    }

    void Terrarium::Carry(std::uint32_t seed, float x, float y, double elapsed)
    {
        TerrariumResident* resident = Find(seed);
        if (!resident || resident->State != ResidentState::Held)
            return;
        if (std::fabs(x - resident->X) > 0.5f)
            resident->Direction = x < resident->X ? -1 : 1;
        if (elapsed > 0.0)
        {
            const auto seconds = static_cast<float>(elapsed);
            const float blend = 1.0f - std::exp(-seconds / kThrowMemorySeconds);
            resident->VelocityX += ((x - resident->X) / seconds - resident->VelocityX) * blend;
            resident->VelocityY += ((y - resident->Y) / seconds - resident->VelocityY) * blend;
        }
        resident->X = resident->TargetX = x;
        resident->Y = resident->TargetY = y;
    }

    void Terrarium::Drop(std::uint32_t seed, const TerrariumSize& size)
    {
        TerrariumResident* resident = Find(seed);
        if (!resident || resident->State != ResidentState::Held)
            return;
        resident->VelocityX = std::clamp(resident->VelocityX, -kMaxThrowSpeed, kMaxThrowSpeed);
        resident->VelocityY = std::clamp(resident->VelocityY, -kMaxThrowSpeed, kMaxThrowSpeed);
        const bool slow = std::fabs(resident->VelocityX) + std::fabs(resident->VelocityY) < kSettledSpeed;
        if (slow)
        {
            resident->VelocityX = 0.0f;
            resident->VelocityY = 0.0f;
        }

        const auto [low, high] = BandOf(PlaceOf(*resident), size);
        resident->X = resident->TargetX = std::clamp(resident->X, 0.0f, std::max(0.0f, size.Width - size.SpriteWidth));
        resident->TargetY = resident->Y < low ? Between(low, low + (high - low) * 0.35f) : std::clamp(resident->Y + std::max(0.0f, resident->VelocityY) * kThrowDepthSeconds, low, high);
        resident->State = ResidentState::Settling;
        resident->WasThrown = true;
        resident->HasTugged = false;
        resident->BurstArmed = false;
        if (slow && resident->Y >= low && resident->Y <= high)
            ComeToRest(*resident);
    }

    void Terrarium::ComeToRest(TerrariumResident& resident)
    {
        resident.State = ResidentState::Resting;
        resident.VelocityX = 0.0f;
        resident.VelocityY = 0.0f;
        resident.TargetX = resident.X;
        resident.TargetY = resident.Y;
        resident.RestEnds = resident.Habit == Habit::Stays ? std::numeric_limits<double>::max() : m_clock + Between(kMinRestSeconds, kMaxRestSeconds);
        if (resident.WasThrown)
        {
            resident.WasThrown = false;
            ReactToThrow(resident);
        }
    }

    void Terrarium::Settle(TerrariumResident& resident, const TerrariumSize& size, float elapsed)
    {
        const auto [low, high] = BandOf(PlaceOf(resident), size);
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);

        resident.X += resident.VelocityX * elapsed;
        if (resident.X < 0.0f || resident.X > maxX)
        {
            resident.X = std::clamp(resident.X, 0.0f, maxX);
            if (HardHit(resident, std::fabs(resident.VelocityX), false, size))
                return;
            resident.VelocityX = -resident.VelocityX * kWallBounce;
        }
        if (std::fabs(resident.VelocityX) > 0.5f)
            resident.Direction = resident.VelocityX < 0.0f ? -1 : 1;
        const float speed = std::fabs(resident.VelocityX) + std::fabs(resident.VelocityY);

        if (PlaceOf(resident) == Habit::Floats)
        {
            const float brake = std::exp(-kAirDrag * elapsed);
            resident.VelocityX *= brake;
            resident.VelocityY *= brake;
            if (resident.Y > high)
                resident.VelocityY -= kGravity * kRiseShare * elapsed;
            resident.Y += resident.VelocityY * elapsed;
            if (resident.Y < low)
            {
                resident.Y = low;
                resident.VelocityY = -resident.VelocityY * kWallBounce;
            }
            if (resident.Y <= high && speed < kSettledSpeed)
                ComeToRest(resident);
            return;
        }

        if (resident.Habit == Habit::Swims && resident.Y >= low)
        {
            const float brake = std::exp(-kWaterDrag * elapsed);
            resident.VelocityX *= brake;
            resident.VelocityY *= brake;
            resident.Y += resident.VelocityY * elapsed;
            if (resident.Y < low || resident.Y > high)
            {
                resident.Y = std::clamp(resident.Y, low, high);
                resident.VelocityY = 0.0f;
            }
            if (speed < kSettledSpeed)
                ComeToRest(resident);
            return;
        }

        if (resident.Habit != Habit::Swims && resident.Y > resident.TargetY)
        {
            resident.Y = std::max(resident.TargetY, resident.Y - kPaddleSpeed * elapsed);
            resident.VelocityY = 0.0f;
            return;
        }

        const float ground = resident.Habit == Habit::Swims ? low : resident.TargetY;
        const bool onGround = resident.Y >= ground && resident.VelocityY == 0.0f;
        if (onGround)
        {
            if (resident.BurstArmed)
            {
                Burst(resident, size);
                return;
            }
            resident.VelocityX *= std::exp(-kGroundFriction * elapsed);
            if (speed < kSettledSpeed)
                ComeToRest(resident);
            return;
        }
        resident.VelocityY += kGravity * elapsed;
        resident.Y += resident.VelocityY * elapsed;
        if (resident.Y < 0.0f)
        {
            resident.Y = 0.0f;
            if (HardHit(resident, std::fabs(resident.VelocityY), false, size))
                return;
            resident.VelocityY = -resident.VelocityY * kWallBounce;
        }
        if (resident.Habit == Habit::Swims)
            resident.Y = std::min(resident.Y, high);
        if (resident.Y >= ground && resident.VelocityY > 0.0f && resident.Habit != Habit::Swims)
        {
            if (HardHit(resident, resident.VelocityY, true, size))
                return;
            resident.Squash = std::clamp(resident.VelocityY * kSquashPerSpeed, kMinSquash, kMaxSquash);
            resident.Y = ground;
            resident.VelocityX *= 0.8f;
            resident.VelocityY = resident.VelocityY > kMinBounceSpeed ? -resident.VelocityY * kGroundBounce : 0.0f;
        }
    }

    void Terrarium::Fit(const TerrariumSize& size)
    {
        if (size.Width == m_size.Width && size.Height == m_size.Height && size.GroundDepth == m_size.GroundDepth && size.LakeDepth == m_size.LakeDepth)
            return;

        const BoxMarks fromMarks = MarksOf(m_size);
        const BoxMarks toMarks = MarksOf(size);
        const float fromRight = std::max(0.0f, m_size.Width - m_size.SpriteWidth);
        const float toRight = std::max(0.0f, size.Width - size.SpriteWidth);
        for (TerrariumResident& resident : m_residents)
        {
            if (resident.State == ResidentState::Held)
                continue;

            resident.X = MapAcross(resident.X, fromRight, toRight);
            resident.TargetX = MapAcross(resident.TargetX, fromRight, toRight);
            resident.Y = MapHeight(resident.Y, fromMarks, toMarks);
            resident.TargetY = MapHeight(resident.TargetY, fromMarks, toMarks);
            if (resident.GroundLine >= 0.0f)
                resident.GroundLine = MapHeight(resident.GroundLine - m_size.SpriteHeight, fromMarks, toMarks) + size.SpriteHeight;
            if (resident.State == ResidentState::Leaving)
                continue;

            const auto [low, high] = BandOf(PlaceOf(resident), size);
            resident.TargetY = std::clamp(resident.TargetY, low, high);
            resident.TargetX = std::clamp(resident.TargetX, 0.0f, toRight);
            if (resident.State == ResidentState::Resting)
            {
                resident.Y = std::clamp(resident.Y, low, high);
                resident.X = std::clamp(resident.X, 0.0f, toRight);
            }
        }
        m_size = size;
    }

    float Terrarium::Between(float low, float high)
    {
        return low >= high ? low : std::uniform_real_distribution<float>(low, high)(m_random);
    }

    bool Terrarium::IsResident(std::uint32_t seed) const
    {
        return std::any_of(m_residents.begin(), m_residents.end(), [&](const TerrariumResident& resident) { return resident.Seed == seed; });
    }

    bool Terrarium::InFoot(float x, float ground, float margin) const
    {
        for (const TerrariumCover& cover : m_covers)
        {
            if (std::fabs(x - cover.X) < cover.Foot + margin && std::fabs(ground - cover.Ground) < 1.25f)
                return true;
        }
        return false;
    }

    void Terrarium::PlaceTarget(TerrariumResident& resident, const TerrariumSize& size, bool roam)
    {
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
        resident.Hiding = false;
        if (roam && resident.Habit == Habit::Walks && !resident.Asleep && !m_covers.empty())
        {
            const float chance = CharacterOf(resident.Creature).Pet == PetManner::Shy ? kShyHideShare : kHideShare;
            std::vector<const TerrariumCover*> hideouts;
            for (const TerrariumCover& cover : m_covers)
            {
                if (cover.Cover > 0.0f)
                    hideouts.push_back(&cover);
            }
            if (!hideouts.empty() && Between(0.0f, 1.0f) < chance)
            {
                const TerrariumCover& cover = *hideouts[static_cast<std::size_t>(Between(0.0f, static_cast<float>(hideouts.size()) - 0.001f))];
                const auto [lowY, highY] = BandOf(PlaceOf(resident), size);
                const float feet = cover.Ground - Between(2.0f, 3.5f);
                const float middle = cover.X + Between(-cover.Cover * 0.4f, cover.Cover * 0.4f);
                const float y = feet - size.SpriteHeight;
                if (y >= lowY && y <= highY && !InMess(middle, feet, size.SpriteWidth * 0.5f) && !InFoot(middle, feet, size.SpriteWidth * 0.3f))
                {
                    resident.TargetX = std::clamp(middle - size.SpriteWidth * 0.5f, 0.0f, maxX);
                    resident.TargetY = y;
                    resident.Hiding = true;
                    return;
                }
            }
        }
        const float lowX = roam ? std::max(0.0f, resident.X - kRoam) : 0.0f;
        const float highX = roam ? std::min(maxX, resident.X + kRoam) : maxX;
        const auto [lowY, highY] = BandOf(PlaceOf(resident), size);

        float best = -1.0f;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            const float x = Between(lowX, highX);
            const float y = Between(lowY, highY);
            float nearest = std::numeric_limits<float>::max();
            for (const TerrariumResident& other : m_residents)
            {
                if (other.Seed == resident.Seed || !SameLayer(other.Habit, resident.Habit) || other.State == ResidentState::Leaving || other.State == ResidentState::Gone)
                    continue;
                const float dx = other.TargetX - x;
                const float dy = other.TargetY - y;
                nearest = std::min(nearest, dx * dx + dy * dy);
            }
            if (!m_messes.empty() && OnTheLand(resident) && InMess(x + size.SpriteWidth * 0.5f, y + size.SpriteHeight, size.SpriteWidth * 0.5f))
                nearest = -0.5f;
            if (!m_covers.empty() && OnTheLand(resident) && InFoot(x + size.SpriteWidth * 0.5f, y + size.SpriteHeight, size.SpriteWidth * 0.3f))
                nearest = -0.5f;
            if (nearest > best)
            {
                best = nearest;
                resident.TargetX = x;
                resident.TargetY = y;
            }
        }
    }

    void Terrarium::Admit(const TerrariumGuest& guest, const TerrariumSize& size, bool animated)
    {
        TerrariumResident resident;
        resident.Seed = guest.Seed;
        resident.Habit = guest.Habit;
        resident.Asleep = guest.Asleep;
        resident.Creature = guest.Creature;
        resident.Bond = guest.Bond;
        resident.Evolutions = guest.Evolutions;
        PlaceTarget(resident, size, false);

        if (!animated)
        {
            resident.X = resident.TargetX;
            resident.Y = resident.TargetY;
            resident.State = ResidentState::Resting;
            resident.RestEnds = m_clock + Between(kMinRestSeconds, kMaxRestSeconds);
        }
        else if (guest.Habit == Habit::Stays)
        {
            resident.X = resident.TargetX;
            resident.Y = -size.SpriteHeight;
        }
        else
        {
            const bool fromLeft = Between(0.0f, 1.0f) < 0.5f;
            resident.X = fromLeft ? -size.SpriteWidth - 1.0f : size.Width + 1.0f;
            resident.Y = resident.TargetY;
            resident.Direction = fromLeft ? 1 : -1;
        }
        m_residents.push_back(resident);
    }

    const TerrariumGuest* Terrarium::NextWaiting(const std::vector<TerrariumGuest>& guests)
    {
        const TerrariumGuest* asleep = nullptr;
        for (std::size_t step = 0; step < guests.size(); ++step)
        {
            const std::size_t index = (m_nextGuest + step) % guests.size();
            const TerrariumGuest& guest = guests[index];
            if (IsResident(guest.Seed))
                continue;
            if (!guest.Asleep)
            {
                m_nextGuest = (index + 1) % guests.size();
                return &guest;
            }
            if (!asleep)
                asleep = &guest;
        }
        return asleep;
    }

    void Terrarium::SendAway(TerrariumResident& resident, const TerrariumSize& size)
    {
        EndEncounter(resident);
        resident.Huddling = false;
        resident.Reaction = TerrariumReaction::None;
        resident.Lift = 0.0f;
        resident.ShiftX = 0.0f;
        resident.Emote = TerrariumEmote::None;
        if (resident.Habit == Habit::Stays)
        {
            const auto [low, high] = BandOf(PlaceOf(resident), size);
            const float stands = std::clamp(resident.State == ResidentState::Entering ? resident.TargetY : resident.Y, low, high);
            resident.State = ResidentState::Leaving;
            resident.GroundLine = stands + size.SpriteHeight;
            resident.TargetX = resident.X;
            resident.TargetY = resident.GroundLine + 1.0f;
            resident.FallSpeed = 0.0f;
            return;
        }
        resident.State = ResidentState::Leaving;
        const bool left = resident.X + size.SpriteWidth * 0.5f < size.Width * 0.5f;
        resident.TargetX = left ? -size.SpriteWidth - 1.0f : size.Width + 1.0f;
        resident.TargetY = resident.Y;
    }

    void Terrarium::SetPointer(bool known, float x, float y)
    {
        m_pointerKnown = known;
        m_pointerX = x;
        m_pointerY = y;
    }

    void Terrarium::BeginReaction(TerrariumResident& resident, TerrariumReaction reaction, double seconds)
    {
        EndEncounter(resident);
        resident.Huddling = false;
        resident.Reaction = reaction;
        resident.ReactionBegan = m_clock;
        resident.ReactionEnds = m_clock + seconds;
        resident.Lift = 0.0f;
        resident.ShiftX = 0.0f;
        resident.Emote = TerrariumEmote::None;
        resident.EmoteAge = 0.0f;
        if (resident.State == ResidentState::Moving)
        {
            resident.State = ResidentState::Resting;
            resident.TargetX = resident.X;
            resident.TargetY = resident.Y;
        }
    }

    void Terrarium::StartTug(TerrariumResident& resident, const TerrariumSize& size)
    {
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
        const float direction = maxX - resident.X >= resident.X ? 1.0f : -1.0f;
        const float room = direction > 0.0f ? maxX - resident.X : resident.X;
        BeginReaction(resident, TerrariumReaction::Tugging, kTugSeconds);
        resident.TargetX = std::clamp(resident.X + direction * std::min(kTugCells, room), 0.0f, maxX);
        resident.TargetY = resident.Y;
        resident.Direction = direction > 0.0f ? 1 : -1;
        resident.State = ResidentState::Moving;
        resident.HasTugged = true;
        m_tugSeed = resident.Seed;
    }

    TerrariumTug Terrarium::Tug() const
    {
        TerrariumTug tug;
        if (m_tugSeed == 0)
            return tug;
        const auto resident = std::find_if(m_residents.begin(), m_residents.end(), [&](const TerrariumResident& each) { return each.Seed == m_tugSeed; });
        if (resident == m_residents.end() || resident->Reaction != TerrariumReaction::Tugging)
            return tug;
        tug.Active = true;
        tug.Seed = m_tugSeed;
        tug.FeetX = resident->X + m_size.SpriteWidth * 0.5f;
        tug.FeetY = resident->Y + m_size.SpriteHeight;
        tug.X = resident->X + (resident->Direction > 0 ? m_size.SpriteWidth - 2.0f : 2.0f);
        tug.Y = resident->Y - resident->Lift + m_size.SpriteHeight * 0.55f;
        return tug;
    }

    void Terrarium::LetGoOfHand(bool fought)
    {
        TerrariumResident* resident = m_tugSeed != 0 ? Find(m_tugSeed) : nullptr;
        m_tugSeed = 0;
        if (!resident || resident->Reaction != TerrariumReaction::Tugging)
            return;
        if (fought)
            BeginReaction(*resident, TerrariumReaction::Dizzy, kDizzySeconds);
        else
            EndReaction(*resident);
    }

    void Terrarium::EndReaction(TerrariumResident& resident)
    {
        if (resident.Seed == m_tugSeed)
            m_tugSeed = 0;
        resident.Reaction = TerrariumReaction::None;
        resident.ShiftX = 0.0f;
        Release(resident);
    }

    PetOutcome Terrarium::Pet(std::uint32_t seed)
    {
        TerrariumResident* resident = Find(seed);
        if (!resident || resident->State == ResidentState::Leaving || resident->State == ResidentState::Held || resident->State == ResidentState::Settling || resident->State == ResidentState::Gone)
            return PetOutcome::Joy;
        if (resident->Reaction == TerrariumReaction::Evolving)
            return PetOutcome::Joy;

        resident->PetStreak = m_clock - resident->LastPet < kPetStreakSeconds ? resident->PetStreak + 1 : 1;
        resident->LastPet = m_clock;
        const PetOutcome outcome = OutcomeOfPet(resident->Creature, resident->Bond, resident->PetStreak);
        switch (outcome)
        {
        case PetOutcome::Shrug:
            BeginReaction(*resident, TerrariumReaction::Shrug, kJoySeconds);
            break;
        case PetOutcome::Dodge:
            BeginReaction(*resident, TerrariumReaction::Dodging, kDodgeSeconds);
            if (resident->Habit != Habit::Stays)
            {
                const float maxX = std::max(0.0f, m_size.Width - m_size.SpriteWidth);
                const float side = !m_pointerKnown || m_pointerX <= resident->X ? 1.0f : -1.0f;
                float x = resident->X + side * kDodgeDistance;
                if (x < 0.0f || x > maxX)
                    x = resident->X - side * kDodgeDistance;
                resident->TargetX = std::clamp(x, 0.0f, maxX);
                resident->State = ResidentState::Moving;
            }
            break;
        case PetOutcome::Bite:
            BeginReaction(*resident, TerrariumReaction::Snapping, kSnapSeconds);
            break;
        case PetOutcome::Puff:
            BeginReaction(*resident, TerrariumReaction::Puffed, kPuffSeconds);
            break;
        default:
            BeginReaction(*resident, TerrariumReaction::Joy, kJoySeconds);
            break;
        }
        return outcome;
    }

    void Terrarium::ReactToThrow(TerrariumResident& resident)
    {
        switch (ThrowMannerOf(resident.Creature, resident.Bond))
        {
        case ThrowManner::Loves:
            BeginReaction(resident, TerrariumReaction::Eager, kEagerSeconds);
            break;
        case ThrowManner::Flees:
            BeginReaction(resident, TerrariumReaction::Scared, kScaredSeconds);
            break;
        case ThrowManner::Dizzy:
            BeginReaction(resident, TerrariumReaction::Dizzy, kDizzySeconds);
            break;
        case ThrowManner::Unmoved:
            BeginReaction(resident, TerrariumReaction::Sulking, kStareSeconds);
            break;
        case ThrowManner::Slips:
            BeginReaction(resident, TerrariumReaction::Puzzled, kPuzzledSeconds);
            break;
        case ThrowManner::Splats:
            BeginReaction(resident, TerrariumReaction::Sulking, kSulkSeconds);
            resident.Squash = 1.0f;
            break;
        case ThrowManner::Furious:
            BeginReaction(resident, TerrariumReaction::Furious, kFuriousSeconds);
            break;
        default:
            break;
        }
    }

    void Terrarium::StepReactions(const TerrariumSize& size, float elapsed)
    {
        constexpr float kPi = 3.14159265f;
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);

        for (TerrariumResident& resident : m_residents)
        {
            if (resident.Reaction != TerrariumReaction::Evolving)
                resident.ShowsFirstForm = false;
            if (resident.Squash > 0.0f)
                resident.Squash = std::max(0.0f, resident.Squash - (resident.Reaction == TerrariumReaction::Sulking ? kSulkSquashEase : kSquashEase) * elapsed);

            if (resident.State == ResidentState::Held)
            {
                const ThrowManner manner = ThrowMannerOf(resident.Creature, resident.Bond);
                resident.Emote = manner == ThrowManner::Loves ? TerrariumEmote::Note : manner == ThrowManner::Flees ? TerrariumEmote::Alarm : manner == ThrowManner::Slips ? TerrariumEmote::Question : TerrariumEmote::None;
                resident.EmoteAge = static_cast<float>(std::fmod(m_clock - resident.HeldSince, 0.8));
                if (manner == ThrowManner::Slips && m_clock - resident.HeldSince >= kSlipSeconds)
                {
                    resident.Emote = TerrariumEmote::None;
                    Drop(resident.Seed, size);
                }
                continue;
            }
            if (resident.Reaction == TerrariumReaction::None)
                continue;
            if (m_clock >= resident.ReactionEnds || resident.State == ResidentState::Leaving || resident.State == ResidentState::Settling)
            {
                EndReaction(resident);
                continue;
            }

            const auto since = static_cast<float>(m_clock - resident.ReactionBegan);
            const auto hop = [&]() { resident.Lift = kHopHeight * std::fabs(std::sin(since / static_cast<float>(kHopSeconds) * kPi)); };
            const auto towardsHand = [&]()
                {
                    if (resident.Habit == Habit::Stays || !m_pointerKnown)
                    {
                        hop();
                        return;
                    }
                    const auto [low, high] = BandOf(PlaceOf(resident), size);
                    const float x = std::clamp(m_pointerX, 0.0f, maxX);
                    const float y = std::clamp(m_pointerY, low, high);
                    const float dx = x - resident.X;
                    const float dy = y - resident.Y;
                    if (dx * dx + dy * dy > kPointerNear * kPointerNear)
                    {
                        resident.TargetX = x;
                        resident.TargetY = y;
                        resident.State = ResidentState::Moving;
                        resident.Lift = 0.0f;
                        return;
                    }
                    resident.TargetX = resident.X;
                    resident.TargetY = resident.Y;
                    resident.State = ResidentState::Resting;
                    hop();
                };

            switch (resident.Reaction)
            {
            case TerrariumReaction::Joy:
                resident.Lift = since < kJoyHopSeconds ? kJoyHopHeight * std::sin(since / static_cast<float>(kJoyHopSeconds) * kPi) : 0.0f;
                resident.Emote = TerrariumEmote::Heart;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Shrug:
                resident.Emote = TerrariumEmote::Heart;
                resident.EmoteAge = since + 0.35f;
                break;
            case TerrariumReaction::Dodging:
                resident.Emote = TerrariumEmote::Dots;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Snapping:
                resident.ShiftX = std::round(std::sin(since * 40.0f));
                resident.Lift = since < 0.2f ? 2.0f * std::sin(since / 0.2f * kPi) : 0.0f;
                resident.Emote = TerrariumEmote::Anger;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Puffed:
                resident.ShiftX = std::round(std::sin(since * 50.0f) * 0.6f);
                resident.Emote = TerrariumEmote::Alarm;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Eager:
            case TerrariumReaction::Visiting:
                towardsHand();
                resident.Emote = TerrariumEmote::Note;
                resident.EmoteAge = std::fmod(since, 0.8f);
                break;
            case TerrariumReaction::Furious:
                towardsHand();
                resident.Emote = TerrariumEmote::Anger;
                resident.EmoteAge = since;
                if (resident.State == ResidentState::Resting && m_pointerKnown && m_tugAllowed && m_tugSeed == 0 && !resident.HasTugged && resident.Habit != Habit::Stays)
                    StartTug(resident, size);
                break;
            case TerrariumReaction::Tugging:
                resident.Emote = TerrariumEmote::Anger;
                resident.EmoteAge = since;
                if (resident.State == ResidentState::Resting && since > 0.1f)
                {
                    m_tugSeed = 0;
                    BeginReaction(resident, TerrariumReaction::Joy, kJoySeconds);
                }
                break;
            case TerrariumReaction::Scared:
                resident.Emote = TerrariumEmote::Alarm;
                resident.EmoteAge = since;
                if (resident.State == ResidentState::Resting && resident.Habit != Habit::Stays)
                {
                    const float dx = m_pointerX - resident.X;
                    const float dy = m_pointerY - resident.Y;
                    if (m_pointerKnown && dx * dx + dy * dy < kScaredReach * kScaredReach)
                    {
                        float x = m_pointerX < resident.X ? maxX : 0.0f;
                        if (std::fabs(x - resident.X) < size.SpriteWidth)
                            x = x > 0.0f ? 0.0f : maxX;
                        const auto [low, high] = BandOf(PlaceOf(resident), size);
                        resident.TargetX = x;
                        resident.TargetY = Between(low, high);
                        resident.State = ResidentState::Moving;
                    }
                }
                resident.ShiftX = resident.State == ResidentState::Resting ? std::round(std::sin(since * 45.0f) * 0.6f) : 0.0f;
                break;
            case TerrariumReaction::Dizzy:
            {
                const float left = 1.0f - since / static_cast<float>(kDizzySeconds);
                resident.ShiftX = std::round(std::sin(since * 8.0f) * 2.0f * left);
                resident.Direction = std::fmod(since, 1.0f) < 0.5f ? 1 : -1;
                resident.Emote = TerrariumEmote::Stars;
                resident.EmoteAge = since;
                break;
            }
            case TerrariumReaction::Sulking:
                resident.Emote = TerrariumEmote::Dots;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Startled:
                resident.Emote = TerrariumEmote::Alarm;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Puzzled:
                resident.Emote = TerrariumEmote::Question;
                resident.EmoteAge = since;
                break;
            case TerrariumReaction::Evolving:
                if (since < kEvolveFlickerSeconds)
                {
                    const float phase = since * (2.5f + since * 3.5f);
                    resident.ShowsFirstForm = std::fmod(phase, 1.0f) < 0.5f;
                }
                else
                {
                    const float joy = since - static_cast<float>(kEvolveFlickerSeconds);
                    resident.ShowsFirstForm = false;
                    resident.Lift = joy < kJoyHopSeconds ? kJoyHopHeight * std::sin(joy / static_cast<float>(kJoyHopSeconds) * kPi) : 0.0f;
                    resident.Emote = TerrariumEmote::Heart;
                    resident.EmoteAge = joy;
                }
                break;
            default:
                break;
            }
        }

        if (!m_pointerKnown)
            return;
        if (m_nextVisit <= 0.0)
            m_nextVisit = m_clock + Between(kMinVisitWait, kMaxVisitWait) * 0.25f;
        if (m_clock < m_nextVisit)
            return;
        m_nextVisit = m_clock + Between(kMinVisitWait, kMaxVisitWait);
        TerrariumResident* pick = nullptr;
        float seen = 0.0f;
        for (TerrariumResident& resident : m_residents)
        {
            const float dx = m_pointerX - resident.X;
            const float dy = m_pointerY - resident.Y;
            if (resident.Bond < kComesOverFromBond || resident.Asleep || resident.Habit == Habit::Stays || resident.Reaction != TerrariumReaction::None || resident.Encounter != TerrariumEncounter::None || resident.Huddling ||
                resident.Fetch != FetchStep::None || resident.Bare ||
                (resident.State != ResidentState::Resting && resident.State != ResidentState::Moving) || dx * dx + dy * dy > kVisitReach * kVisitReach)
                continue;
            seen += 1.0f;
            if (Between(0.0f, seen) <= 1.0f)
                pick = &resident;
        }
        if (pick)
            BeginReaction(*pick, TerrariumReaction::Visiting, kVisitSeconds);
    }

    void Terrarium::Release(TerrariumResident& resident)
    {
        resident.Encounter = TerrariumEncounter::None;
        resident.Partner = 0;
        resident.Lift = 0.0f;
        resident.Emote = TerrariumEmote::None;
        resident.EmoteAge = 0.0f;
        if (resident.Habit == Habit::Stays || (resident.State != ResidentState::Resting && resident.State != ResidentState::Moving))
            return;

        const auto [low, high] = BandOf(PlaceOf(resident), m_size);
        if (resident.Y < low || resident.Y > high)
        {
            PlaceTarget(resident, m_size, true);
            resident.State = ResidentState::Moving;
            return;
        }
        resident.TargetX = resident.X;
        resident.TargetY = resident.Y;
        resident.State = ResidentState::Resting;
        resident.RestEnds = m_clock + Between(kMinRestSeconds, kMaxRestSeconds);
    }

    void Terrarium::EndEncounter(TerrariumResident& resident)
    {
        if (resident.Encounter == TerrariumEncounter::None)
            return;
        TerrariumResident* partner = resident.Partner != 0 ? Find(resident.Partner) : nullptr;
        const std::uint32_t seed = resident.Seed;
        Release(resident);
        if (partner && partner->Partner == seed)
            Release(*partner);
    }

    void Terrarium::PlaceBeside(TerrariumResident& resident, const TerrariumResident& other, const TerrariumSize& size, float gap)
    {
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
        const float side = resident.X < other.X ? -1.0f : 1.0f;
        float x = other.X + side * gap;
        if (x < 0.0f || x > maxX)
            x = other.X - side * gap;
        resident.TargetX = std::clamp(x, 0.0f, maxX);
        const auto [low, high] = BandOf(PlaceOf(other), size);
        resident.TargetY = std::clamp(other.Y, low, high);
        const float half = size.SpriteWidth * 0.5f;
        if (InFoot(resident.TargetX + half, resident.TargetY + size.SpriteHeight, size.SpriteWidth * 0.3f))
        {
            const float farther = std::clamp(resident.TargetX + side * gap, 0.0f, maxX);
            const float across = std::clamp(other.X - side * gap, 0.0f, maxX);
            resident.TargetX = !InFoot(farther + half, resident.TargetY + size.SpriteHeight, size.SpriteWidth * 0.3f) ? farther : across;
        }
    }

    void Terrarium::PlaceFlight(TerrariumResident& resident, const TerrariumResident& chaser, const TerrariumSize& size)
    {
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
        float x = chaser.X < resident.X ? maxX : 0.0f;
        if (std::fabs(x - resident.X) < size.SpriteWidth)
            x = x > 0.0f ? 0.0f : maxX;
        resident.TargetX = x;
        const auto [low, high] = BandOf(PlaceOf(resident), size);
        resident.TargetY = Between(low, high);
    }

    void Terrarium::HuddleUp(TerrariumResident& resident, const TerrariumSize& size)
    {
        const TerrariumResident* nearest = nullptr;
        float best = kHuddleReach * kHuddleReach;
        for (const TerrariumResident& other : m_residents)
        {
            if (other.Seed == resident.Seed || !other.Asleep || other.State != ResidentState::Resting || !SameLayer(PlaceOf(other), PlaceOf(resident)) || other.Habit == Habit::Stays)
                continue;
            const float dx = other.X - resident.X;
            const float dy = other.Y - resident.Y;
            if (dx * dx + dy * dy < best)
            {
                best = dx * dx + dy * dy;
                nearest = &other;
            }
        }
        if (!nearest)
            return;
        PlaceBeside(resident, *nearest, size, size.SpriteWidth * kHuddleShare);
        resident.State = ResidentState::Moving;
        resident.Huddling = true;
    }

    void Terrarium::BeginGreeting(TerrariumResident& first, TerrariumResident& second, double seconds, TerrariumEmote firstEmote, TerrariumEmote secondEmote)
    {
        const auto begin = [&](TerrariumResident& resident, const TerrariumResident& other, TerrariumEmote emote)
            {
                resident.Encounter = TerrariumEncounter::Greeting;
                resident.EncounterBegan = m_clock;
                resident.EncounterEnds = m_clock + seconds;
                resident.Emote = emote;
                resident.State = ResidentState::Resting;
                resident.TargetX = resident.X;
                resident.TargetY = resident.Y;
                if (std::fabs(other.X - resident.X) > 0.5f)
                    resident.Direction = other.X < resident.X ? -1 : 1;
            };
        begin(first, second, firstEmote);
        begin(second, first, secondEmote);
    }

    void Terrarium::StepEncounters(const TerrariumSize& size)
    {
        for (TerrariumResident& resident : m_residents)
        {
            if (resident.Encounter == TerrariumEncounter::None)
                continue;
            const double since = m_clock - resident.EncounterBegan;

            if (resident.Encounter == TerrariumEncounter::Leaping)
            {
                const double share = since / kLeapSeconds;
                if (share >= 1.0 || resident.Asleep)
                {
                    Release(resident);
                    continue;
                }
                const float surface = size.Height - size.LakeDepth;
                const float peak = std::max(kLeapMargin, resident.Y + size.SpriteHeight - surface + kLeapMargin);
                resident.Lift = peak * 4.0f * static_cast<float>(share * (1.0 - share));
                const bool out = share < kSplashShare;
                const bool in = share > 1.0 - kSplashShare;
                resident.Emote = out || in ? TerrariumEmote::Splash : TerrariumEmote::None;
                resident.EmoteAge = static_cast<float>(out ? since : since - (1.0 - kSplashShare) * kLeapSeconds);
                continue;
            }

            TerrariumResident* partner = Find(resident.Partner);
            const auto gone = [](const TerrariumResident& one) { return one.State == ResidentState::Leaving || one.State == ResidentState::Held || one.State == ResidentState::Settling; };
            if (!partner || partner->Partner != resident.Seed || gone(*partner) || gone(resident) || resident.Asleep || partner->Asleep)
            {
                EndEncounter(resident);
                continue;
            }

            switch (resident.Encounter)
            {
            case TerrariumEncounter::Seeking:
                if (resident.State == ResidentState::Resting)
                    BeginGreeting(resident, *partner, kGreetSeconds, TerrariumEmote::Heart, partner->Habit == Habit::Stays ? TerrariumEmote::None : TerrariumEmote::Heart);
                else if (m_clock >= resident.EncounterEnds)
                    EndEncounter(resident);
                break;
            case TerrariumEncounter::Greeting:
                if (m_clock >= resident.EncounterEnds)
                {
                    EndEncounter(resident);
                    break;
                }
                if (resident.Habit != Habit::Stays)
                {
                    const double phase = std::fmod(since + (resident.Seed & 1u) * kHopSeconds * 0.5, kHopSeconds) / kHopSeconds;
                    resident.Lift = kHopHeight * static_cast<float>(std::sin(phase * 3.14159265358979));
                }
                resident.EmoteAge = static_cast<float>(std::fmod(since, kHeartEverySeconds));
                break;
            case TerrariumEncounter::Chasing:
            {
                const float dx = partner->X - resident.X;
                const float dy = partner->Y - resident.Y;
                if (m_clock >= resident.EncounterEnds || dx * dx + dy * dy < kCaughtDistance * kCaughtDistance)
                {
                    BeginGreeting(resident, *partner, kCaughtSeconds, TerrariumEmote::Heart, TerrariumEmote::Alarm);
                    break;
                }
                resident.TargetX = partner->X;
                resident.TargetY = partner->Y;
                resident.State = ResidentState::Moving;
                break;
            }
            case TerrariumEncounter::Fleeing:
                resident.Emote = TerrariumEmote::Alarm;
                resident.EmoteAge = static_cast<float>(since);
                if (resident.State == ResidentState::Resting)
                {
                    PlaceFlight(resident, *partner, size);
                    resident.State = ResidentState::Moving;
                }
                break;
            default:
                break;
            }
        }
    }

    void Terrarium::BeginEncounter(const TerrariumSize& size)
    {
        const auto free = [](const TerrariumResident& resident)
            {
                return !resident.Asleep && !resident.Huddling && resident.Reaction == TerrariumReaction::None && resident.Encounter == TerrariumEncounter::None && resident.Fetch == FetchStep::None &&
                    !resident.Bare && (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving);
            };
        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);

        enum class Kind
        {
            Greet,
            Chase,
            Leap
        };
        TerrariumResident* first = nullptr;
        TerrariumResident* second = nullptr;
        Kind kind = Kind::Greet;
        float total = 0.0f;
        const auto consider = [&](TerrariumResident* one, TerrariumResident* other, Kind what, float weight)
            {
                total += weight;
                if (Between(0.0f, total) <= weight)
                {
                    first = one;
                    second = other;
                    kind = what;
                }
            };

        for (TerrariumResident& one : m_residents)
        {
            if (!free(one) || one.Habit == Habit::Stays || one.X < 0.0f || one.X > maxX)
                continue;
            if (one.Habit == Habit::Swims)
                consider(&one, nullptr, Kind::Leap, 1.5f);

            for (TerrariumResident& other : m_residents)
            {
                if (other.Seed == one.Seed || !free(other) || other.X < 0.0f || other.X > maxX)
                    continue;
                const Relation relation = RelationOf(one.Creature, other.Creature);
                const float dx = other.X - one.X;
                const float dy = other.Y - one.Y;
                const float close = 40.0f / (40.0f + std::sqrt(dx * dx + dy * dy));
                if (SameLayer(one.Habit, other.Habit))
                {
                    if (relation == Relation::Chases && other.Habit != Habit::Stays)
                        consider(&one, &other, Kind::Chase, 6.0f * close);
                    else
                        consider(&one, &other, Kind::Greet, (relation == Relation::Friends ? 6.0f : 1.0f) * close);
                }
                else if (one.Habit == Habit::Floats && other.Habit != Habit::Swims)
                {
                    consider(&one, &other, Kind::Greet, (relation == Relation::Friends ? 3.0f : 0.4f) * close);
                }
            }
        }
        if (!first)
            return;

        first->EncounterBegan = m_clock;
        if (kind == Kind::Leap)
        {
            first->Encounter = TerrariumEncounter::Leaping;
            first->State = ResidentState::Resting;
            first->TargetX = first->X;
            first->TargetY = first->Y;
            return;
        }

        first->Partner = second->Seed;
        second->Partner = first->Seed;
        second->EncounterBegan = m_clock;
        if (kind == Kind::Chase)
        {
            first->Encounter = TerrariumEncounter::Chasing;
            second->Encounter = TerrariumEncounter::Fleeing;
            first->EncounterEnds = m_clock + Between(kMinChaseSeconds, kMaxChaseSeconds);
            second->EncounterEnds = first->EncounterEnds;
            first->TargetX = second->X;
            first->TargetY = second->Y;
            first->State = ResidentState::Moving;
            PlaceFlight(*second, *first, size);
            second->State = ResidentState::Moving;
            return;
        }

        first->Encounter = TerrariumEncounter::Seeking;
        second->Encounter = TerrariumEncounter::Awaiting;
        first->EncounterEnds = m_clock + kSeekSeconds;
        second->EncounterEnds = first->EncounterEnds;
        second->State = ResidentState::Resting;
        second->TargetX = second->X;
        second->TargetY = second->Y;
        if (first->Habit == Habit::Floats && second->Habit != Habit::Floats)
        {
            first->TargetX = second->X;
            first->TargetY = std::max(0.0f, second->Y - size.SpriteHeight * 0.9f);
        }
        else
        {
            PlaceBeside(*first, *second, size, size.SpriteWidth * kBesideShare);
        }
        first->State = ResidentState::Moving;
    }

    std::pair<float, float> Terrarium::ThingFloors(const TerrariumSize& size, float radius) const
    {
        const auto [low, high] = BandOf(Habit::Walks, size);
        return { low + size.SpriteHeight - radius, high + size.SpriteHeight - radius };
    }

    bool Terrarium::TakeThing(TerrariumThing& thing, float x, float y)
    {
        if (thing.State == BallState::None || thing.State == BallState::Held)
            return false;
        const float reach = thing.Radius + 2.5f;
        const float dx = x - thing.X;
        const float dy = y - thing.Y;
        if (dx * dx + dy * dy > reach * reach)
            return false;
        thing.State = BallState::Held;
        thing.VelocityX = 0.0f;
        thing.VelocityY = 0.0f;
        thing.GrabbedX = thing.X;
        thing.GrabbedY = thing.Y;
        return true;
    }

    void Terrarium::MoveThing(TerrariumThing& thing, float x, float y, double elapsed)
    {
        if (thing.State != BallState::Held)
            return;
        if (elapsed > 0.0)
        {
            const float share = std::min(1.0f, static_cast<float>(elapsed) / kThrowMemorySeconds);
            thing.VelocityX += ((x - thing.X) / static_cast<float>(elapsed) - thing.VelocityX) * share;
            thing.VelocityY += ((y - thing.Y) / static_cast<float>(elapsed) - thing.VelocityY) * share;
        }
        thing.X = x;
        thing.Y = y;
    }

    bool Terrarium::ThrowThing(TerrariumThing& thing, const TerrariumSize& size)
    {
        const float speed = std::hypot(thing.VelocityX, thing.VelocityY);
        if (speed > kMaxThrowSpeed)
        {
            thing.VelocityX *= kMaxThrowSpeed / speed;
            thing.VelocityY *= kMaxThrowSpeed / speed;
        }
        const auto [back, front] = ThingFloors(size, thing.Radius);
        thing.Floor = std::clamp(thing.Y, back, front);
        thing.State = BallState::Flying;
        return speed > kBallThrowSpeed || std::hypot(thing.X - thing.GrabbedX, thing.Y - thing.GrabbedY) > kBallThrowDistance;
    }

    bool Terrarium::StepThing(TerrariumThing& thing, const TerrariumSize& size, float elapsed)
    {
        const auto [back, front] = ThingFloors(size, thing.Radius);
        const float right = std::max(thing.Radius, size.Width - thing.Radius);
        thing.Floor = std::clamp(thing.Floor, back, front);
        if (thing.State == BallState::Lying)
        {
            thing.X = std::clamp(thing.X, thing.Radius, right);
            thing.Y = thing.Floor;
            return false;
        }
        if (thing.State != BallState::Flying)
            return false;

        thing.VelocityY += kGravity * elapsed;
        thing.VelocityX *= std::max(0.0f, 1.0f - kBallAirDrag * elapsed);
        thing.X += thing.VelocityX * elapsed;
        thing.Y += thing.VelocityY * elapsed;
        thing.Turn += thing.VelocityX * elapsed;
        if (thing.X < thing.Radius || thing.X > right)
        {
            thing.X = std::clamp(thing.X, thing.Radius, right);
            thing.VelocityX = -thing.VelocityX * kWallBounce;
        }
        if (thing.Y < thing.Radius)
        {
            thing.Y = thing.Radius;
            thing.VelocityY = std::fabs(thing.VelocityY) * 0.5f;
        }
        if (thing.Y >= thing.Floor)
        {
            thing.Y = thing.Floor;
            thing.VelocityY = thing.VelocityY > kMinBounceSpeed ? -thing.VelocityY * kBallBounce : 0.0f;
            thing.VelocityX *= std::max(0.0f, 1.0f - kBallRollFriction * elapsed);
            if (thing.VelocityY == 0.0f && std::fabs(thing.VelocityX) < kBallRestSpeed)
            {
                thing.State = BallState::Lying;
                thing.VelocityX = 0.0f;
                return true;
            }
        }
        return false;
    }

    void Terrarium::SetBall(bool present, const TerrariumSize& size)
    {
        if (!present)
        {
            if (m_ball.State == BallState::None)
                return;
            for (TerrariumResident& resident : m_residents)
            {
                if (resident.Fetch != FetchStep::None)
                    EndFetch(resident);
            }
            m_ball = {};
            m_ballThrown = false;
            return;
        }
        if (m_ball.State != BallState::None)
            return;
        const auto [back, front] = ThingFloors(size, kBallRadius);
        m_ball.State = BallState::Lying;
        m_ball.X = std::round(size.Width * 0.5f);
        m_ball.Floor = std::round((back + front) * 0.5f);
        m_ball.Y = m_ball.Floor;
    }

    bool Terrarium::GrabBall(float x, float y)
    {
        if (!TakeThing(m_ball, x, y))
            return false;
        for (TerrariumResident& resident : m_residents)
        {
            if (resident.Fetch != FetchStep::None)
                EndFetch(resident);
        }
        m_ball.Fetcher = 0;
        m_ballThrown = false;
        return true;
    }

    void Terrarium::CarryBall(float x, float y, double elapsed)
    {
        MoveThing(m_ball, x, y, elapsed);
    }

    void Terrarium::DropBall(const TerrariumSize& size)
    {
        if (m_ball.State != BallState::Held)
            return;
        m_ballThrown = ThrowThing(m_ball, size);
        m_ball.ThrownFromX = m_ball.X;
    }

    bool Terrarium::GrabShell(float x, float y)
    {
        return TakeThing(m_shell, x, y);
    }

    void Terrarium::CarryShell(float x, float y, double elapsed)
    {
        MoveThing(m_shell, x, y, elapsed);
    }

    void Terrarium::DropShell(const TerrariumSize& size)
    {
        if (m_shell.State == BallState::Held)
            ThrowThing(m_shell, size);
    }

    void Terrarium::KnockShell(TerrariumResident& resident, float impact, const TerrariumSize& size)
    {
        if (resident.Creature != CreatureId::Snail || resident.Bare || impact < kShellOffSpeed || m_shell.State != BallState::None)
            return;
        const float slot = resident.Direction < 0 ? kSnailShellX : size.SpriteWidth - kSnailShellX;
        const float backwards = resident.Direction < 0 ? 1.0f : -1.0f;
        m_shell = {};
        m_shell.Radius = kShellRadius;
        m_shell.Owner = resident.Seed;
        m_shell.State = BallState::Flying;
        m_shell.X = std::clamp(resident.X + slot, kShellRadius, std::max(kShellRadius, size.Width - kShellRadius));
        m_shell.Y = std::max(kShellRadius, resident.Y + kSnailShellY);
        m_shell.VelocityX = resident.VelocityX * 0.5f + backwards * 40.0f;
        m_shell.VelocityY = -std::max(60.0f, impact * 0.4f);
        const auto [back, front] = ThingFloors(size, kShellRadius);
        m_shell.Floor = std::clamp(resident.TargetY + size.SpriteHeight - kShellRadius, back, front);
        resident.Bare = true;
        resident.BareNoticed = false;
    }

    void Terrarium::StepShell(const TerrariumSize& size)
    {
        if (m_shell.State == BallState::None)
            return;
        TerrariumResident* owner = Find(m_shell.Owner);
        if (!owner || !owner->Bare)
        {
            m_shell = {};
            return;
        }
        if (owner->State != ResidentState::Resting && owner->State != ResidentState::Moving)
            return;
        if (!owner->BareNoticed)
        {
            if (owner->State == ResidentState::Resting && owner->Reaction == TerrariumReaction::None)
            {
                BeginReaction(*owner, TerrariumReaction::Sulking, kBareSulkSeconds);
                owner->BareNoticed = true;
            }
            return;
        }
        if (owner->Reaction != TerrariumReaction::None || owner->Encounter != TerrariumEncounter::None)
            return;

        const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
        const float lookingLeft = std::clamp(m_shell.X - kSnailShellX, 0.0f, maxX);
        const float lookingRight = std::clamp(m_shell.X - (size.SpriteWidth - kSnailShellX), 0.0f, maxX);
        const auto [low, high] = BandOf(Habit::Walks, size);
        owner->TargetX = std::fabs(lookingLeft - owner->X) <= std::fabs(lookingRight - owner->X) ? lookingLeft : lookingRight;
        owner->TargetY = std::clamp(m_shell.Floor + m_shell.Radius - size.SpriteHeight, low, high);
        owner->State = ResidentState::Moving;
        owner->Huddling = false;
    }

    void Terrarium::ArriveAtShell(TerrariumResident& resident, const TerrariumSize& size)
    {
        if (m_shell.State != BallState::Lying || m_shell.Owner != resident.Seed)
            return;
        const float lookingLeft = resident.X + kSnailShellX;
        const float lookingRight = resident.X + size.SpriteWidth - kSnailShellX;
        constexpr float kNear = 3.0f;
        if (std::fabs(lookingLeft - m_shell.X) <= kNear)
            resident.Direction = -1;
        else if (std::fabs(lookingRight - m_shell.X) <= kNear)
            resident.Direction = 1;
        else
            return;
        resident.Bare = false;
        resident.BareNoticed = false;
        m_shell = {};
        resident.State = ResidentState::Resting;
        resident.RestEnds = m_clock + Between(kMinRestSeconds, kMaxRestSeconds);
        BeginReaction(resident, TerrariumReaction::Joy, kJoySeconds);
    }

    void Terrarium::EndFetch(TerrariumResident& resident)
    {
        if (m_ball.Fetcher == resident.Seed)
        {
            if (m_ball.State == BallState::Carried)
            {
                m_ball.State = BallState::Flying;
                m_ball.VelocityX = 0.0f;
                m_ball.VelocityY = 0.0f;
            }
            m_ball.Fetcher = 0;
        }
        if (resident.Emote == TerrariumEmote::Note && resident.Reaction == TerrariumReaction::None)
            resident.Emote = TerrariumEmote::None;
        resident.Fetch = FetchStep::None;
        resident.FetchEager = false;
    }

    void Terrarium::ChooseFetcher(const TerrariumSize& size)
    {
        if (InMess(m_ball.X, m_ball.Floor + m_ball.Radius))
            return;
        TerrariumResident* best = nullptr;
        float nearest = 0.0f;
        for (TerrariumResident& resident : m_residents)
        {
            if (resident.Asleep || resident.Huddling || resident.Encounter != TerrariumEncounter::None || resident.Reaction != TerrariumReaction::None ||
                (resident.State != ResidentState::Resting && resident.State != ResidentState::Moving) || (resident.Habit != Habit::Walks && resident.Habit != Habit::Floats))
                continue;
            const CreatureCharacter character = CharacterOf(resident.Creature);
            if (character.Pet == PetManner::Unmoved || ThrowMannerOf(resident.Creature, resident.Bond) == ThrowManner::Unmoved)
                continue;
            const float distance = std::fabs(resident.X + size.SpriteWidth * 0.5f - m_ball.X) + std::fabs(resident.Y + size.SpriteHeight - m_ball.Floor) * 0.5f;
            if (!best || distance < nearest)
            {
                best = &resident;
                nearest = distance;
            }
        }
        if (!best)
            return;
        const CreatureCharacter character = CharacterOf(best->Creature);
        best->FetchEager = character.Pet == PetManner::Loves || ThrowMannerOf(best->Creature, best->Bond) == ThrowManner::Loves;
        m_ball.Fetcher = best->Seed;
        if (character.Pet == PetManner::Shy && best->Bond < kShyUntilBond)
        {
            BeginReaction(*best, TerrariumReaction::Puzzled, kFetchHesitation);
            best->Fetch = FetchStep::Waiting;
            best->FetchStarts = m_clock + kFetchHesitation;
        }
        else
        {
            best->Fetch = FetchStep::ToBall;
        }
    }

    void Terrarium::StepBall(const TerrariumSize& size, float elapsed)
    {
        if (m_ball.State == BallState::None)
            return;

        if (m_ball.Fetcher != 0)
        {
            TerrariumResident* fetcher = Find(m_ball.Fetcher);
            const bool free = fetcher && fetcher->Fetch != FetchStep::None && !fetcher->Asleep && fetcher->Encounter == TerrariumEncounter::None &&
                (fetcher->State == ResidentState::Resting || fetcher->State == ResidentState::Moving) &&
                (fetcher->Reaction == TerrariumReaction::None || (fetcher->Fetch == FetchStep::Waiting && fetcher->Reaction == TerrariumReaction::Puzzled));
            if (!free)
            {
                if (fetcher)
                {
                    EndFetch(*fetcher);
                }
                else
                {
                    if (m_ball.State == BallState::Carried)
                        m_ball.State = BallState::Flying;
                    m_ball.Fetcher = 0;
                }
            }
        }

        if (StepThing(m_ball, size, elapsed))
            m_ballWaitsUntil = m_clock + kBallWaitSeconds;
        switch (m_ball.State)
        {
        case BallState::Carried:
            if (const TerrariumResident* fetcher = Find(m_ball.Fetcher))
            {
                m_ball.X = fetcher->X + (fetcher->Direction > 0 ? size.SpriteWidth - 3.0f : 3.0f);
                m_ball.Y = fetcher->Y - fetcher->Lift + size.SpriteHeight * 0.6f;
            }
            break;
        default:
            break;
        }

        if (m_ball.State == BallState::Lying && m_ballThrown && m_ball.Fetcher == 0)
        {
            ChooseFetcher(size);
            if (m_ball.Fetcher != 0 || m_clock >= m_ballWaitsUntil)
                m_ballThrown = false;
        }

        TerrariumResident* fetcher = m_ball.Fetcher != 0 ? Find(m_ball.Fetcher) : nullptr;
        if (!fetcher)
            return;
        if (fetcher->Fetch == FetchStep::Waiting && m_clock >= fetcher->FetchStarts)
        {
            if (fetcher->Reaction == TerrariumReaction::Puzzled)
                EndReaction(*fetcher);
            fetcher->Fetch = FetchStep::ToBall;
        }
        if (fetcher->Fetch == FetchStep::ToBall)
        {
            if (m_ball.State != BallState::Lying && m_ball.State != BallState::Flying)
            {
                EndFetch(*fetcher);
                return;
            }
            const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
            fetcher->TargetX = std::clamp(m_ball.X - size.SpriteWidth * 0.5f, 0.0f, maxX);
            fetcher->TargetY = std::max(0.0f, m_ball.Floor + kBallRadius - size.SpriteHeight);
            fetcher->State = ResidentState::Moving;
        }
        if (fetcher->FetchEager && fetcher->Fetch != FetchStep::Waiting)
        {
            fetcher->Emote = TerrariumEmote::Note;
            fetcher->EmoteAge = static_cast<float>(std::fmod(m_clock, 0.8));
        }
    }

    void Terrarium::ArriveFetching(TerrariumResident& resident, const TerrariumSize& size)
    {
        if (resident.Fetch == FetchStep::ToBall)
        {
            const float reach = size.SpriteWidth * 0.5f + kBallRadius;
            if (m_ball.State != BallState::Lying || std::fabs(resident.X + size.SpriteWidth * 0.5f - m_ball.X) > reach)
                return;
            m_ball.State = BallState::Carried;
            resident.Fetch = FetchStep::Back;
            const float maxX = std::max(0.0f, size.Width - size.SpriteWidth);
            resident.TargetX = std::clamp(m_ball.ThrownFromX - size.SpriteWidth * 0.5f, 0.0f, maxX);
            resident.TargetY = resident.Y;
            resident.Direction = resident.TargetX < resident.X ? -1 : 1;
            return;
        }

        m_ball.State = BallState::Lying;
        m_ball.X = std::clamp(resident.X + (resident.Direction > 0 ? size.SpriteWidth - 3.0f : 3.0f), kBallRadius, std::max(kBallRadius, size.Width - kBallRadius));
        m_ball.Y = m_ball.Floor;
        m_ball.VelocityX = 0.0f;
        m_ball.VelocityY = 0.0f;
        m_ball.Fetcher = 0;
        resident.Fetch = FetchStep::None;
        resident.FetchEager = false;
        resident.Emote = TerrariumEmote::None;
        resident.State = ResidentState::Resting;
        resident.RestEnds = m_clock + Between(kMinRestSeconds, kMaxRestSeconds);
        BeginReaction(resident, TerrariumReaction::Joy, kJoySeconds);
        m_fetches.push_back(resident.Seed);
    }

    bool Terrarium::InMess(float x, float ground, float margin) const
    {
        for (const TerrariumMess& mess : m_messes)
        {
            if (m_clock >= mess.Ends)
                continue;
            const float across = (x - mess.X) / (mess.Radius + margin);
            const float deep = (ground - mess.Y) / (mess.Radius * kMessDepthShare + margin * 0.5f);
            if (across * across + deep * deep <= 1.0f)
                return true;
        }
        return false;
    }

    bool Terrarium::OnTheLand(const TerrariumResident& resident) const
    {
        return resident.Habit == Habit::Walks || resident.Habit == Habit::Stays || (resident.Habit == Habit::Floats && resident.Asleep);
    }

    bool Terrarium::HardHit(TerrariumResident& resident, float impact, bool ground, const TerrariumSize& size)
    {
        if (resident.Creature == CreatureId::ParkaKid)
        {
            if (!ground)
            {
                resident.BurstArmed |= impact >= kBurstSpeed;
                return false;
            }
            if (resident.BurstArmed || impact >= kBurstSpeed)
            {
                Burst(resident, size);
                return true;
            }
            return false;
        }
        KnockShell(resident, impact, size);
        return false;
    }

    void Terrarium::Burst(TerrariumResident& resident, const TerrariumSize& size)
    {
        if (resident.Fetch != FetchStep::None)
            EndFetch(resident);
        EndEncounter(resident);
        resident.Huddling = false;

        const auto [low, high] = BandOf(Habit::Walks, size);
        TerrariumMess mess;
        mess.X = resident.X + size.SpriteWidth * 0.5f;
        mess.Y = std::clamp(resident.Y, low, high) + size.SpriteHeight;
        mess.Radius = std::max(6.0f, std::min(kMessRadius, size.Width * 0.25f));
        mess.X = std::clamp(mess.X, mess.Radius * 0.5f, std::max(mess.Radius * 0.5f, size.Width - mess.Radius * 0.5f));
        mess.Began = m_clock;
        mess.Ends = m_clock + kMessSeconds;
        mess.Seed = resident.Seed * 2654435761u + static_cast<std::uint32_t>(m_clock * 1000.0);
        m_messes.push_back(mess);

        resident.State = ResidentState::Gone;
        resident.BurstArmed = false;
        resident.GoneUntil = m_clock + kGoneSeconds;
        resident.NextBurst = 0.0;
        resident.VelocityX = resident.VelocityY = 0.0f;
        resident.Lift = resident.ShiftX = resident.Squash = 0.0f;
        resident.Reaction = TerrariumReaction::None;
        resident.Emote = TerrariumEmote::None;
        resident.WasThrown = false;

        for (TerrariumResident& other : m_residents)
        {
            if (other.Seed != resident.Seed && OnTheLand(other) && other.Habit != Habit::Stays &&
                (other.State == ResidentState::Resting || other.State == ResidentState::Moving) &&
                InMess(other.X + size.SpriteWidth * 0.5f, other.Y + size.SpriteHeight, size.SpriteWidth * 0.5f))
                Startle(other, size);
        }
    }

    void Terrarium::Startle(TerrariumResident& resident, const TerrariumSize& size)
    {
        if (resident.Fetch != FetchStep::None)
            EndFetch(resident);
        BeginReaction(resident, TerrariumReaction::Startled, kStartledSeconds);
        PlaceTarget(resident, size, false);
        resident.State = ResidentState::Moving;
    }

    void Terrarium::ComeBack(TerrariumResident& resident, const TerrariumSize& size)
    {
        PlaceTarget(resident, size, false);
        const bool fromLeft = Between(0.0f, 1.0f) < 0.5f;
        resident.X = fromLeft ? -size.SpriteWidth - 1.0f : size.Width + 1.0f;
        resident.Y = resident.TargetY;
        resident.Direction = fromLeft ? 1 : -1;
        resident.State = ResidentState::Entering;
        resident.GoneUntil = 0.0;
        resident.NextBurst = m_clock + Between(kMinBurstSeconds, kMaxBurstSeconds);
    }

    void Terrarium::StepBursts(const TerrariumSize& size)
    {
        std::erase_if(m_messes, [&](const TerrariumMess& mess) { return m_clock >= mess.Ends; });

        for (TerrariumResident& resident : m_residents)
        {
            if (resident.Creature == CreatureId::ParkaKid && !resident.Asleep && resident.Reaction == TerrariumReaction::None &&
                resident.Encounter == TerrariumEncounter::None && resident.Fetch == FetchStep::None &&
                (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving))
            {
                if (resident.NextBurst <= 0.0)
                    resident.NextBurst = m_clock + Between(kMinBurstSeconds, kMaxBurstSeconds);
                else if (m_clock >= resident.NextBurst)
                    Burst(resident, size);
            }
        }
        if (m_messes.empty())
            return;

        const bool ballInMess = (m_ball.State == BallState::Lying || m_ball.State == BallState::Flying) && InMess(m_ball.X, m_ball.Floor + m_ball.Radius);
        for (TerrariumResident& resident : m_residents)
        {
            if (!OnTheLand(resident) || resident.Habit == Habit::Stays || resident.Bare || resident.Encounter != TerrariumEncounter::None)
                continue;
            if (resident.Fetch != FetchStep::None)
            {
                if (ballInMess)
                    EndFetch(resident);
                continue;
            }
            const float middle = size.SpriteWidth * 0.5f;
            if (resident.State == ResidentState::Resting && resident.Reaction == TerrariumReaction::None && InMess(resident.X + middle, resident.Y + size.SpriteHeight, middle))
                Startle(resident, size);
            else if (resident.State == ResidentState::Moving && resident.Reaction == TerrariumReaction::None && InMess(resident.TargetX + middle, resident.TargetY + size.SpriteHeight, middle))
                PlaceTarget(resident, size, false);
        }
    }

    void Terrarium::Update(const std::vector<TerrariumGuest>& guests, const TerrariumSize& size, double elapsed, bool animated)
    {
        m_clock += elapsed;
        Fit(size);

        for (TerrariumResident& resident : m_residents)
        {
            const auto guest = std::find_if(guests.begin(), guests.end(), [&](const TerrariumGuest& known) { return known.Seed == resident.Seed; });
            if (guest == guests.end())
            {
                if (resident.State == ResidentState::Gone)
                    resident.Seed = 0;
                else if (resident.State != ResidentState::Leaving)
                    SendAway(resident, size);
                continue;
            }
            const bool wasAsleep = resident.Asleep;
            resident.Asleep = guest->Asleep;
            resident.Creature = guest->Creature;
            resident.Bond = guest->Bond;
            if (guest->Evolutions != resident.Evolutions)
            {
                resident.Evolutions = guest->Evolutions;
                if (animated && (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving))
                    BeginReaction(resident, TerrariumReaction::Evolving, kEvolveSeconds);
            }
            if (resident.Asleep != wasAsleep)
            {
                EndEncounter(resident);
                resident.Huddling = false;
            }
            if (animated && resident.Habit == Habit::Floats && wasAsleep != resident.Asleep && (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving || resident.State == ResidentState::Entering))
            {
                PlaceTarget(resident, size, true);
                resident.State = ResidentState::Moving;
            }
            if (animated && !wasAsleep && resident.Asleep && resident.Habit != Habit::Stays && resident.Habit != Habit::Swims && (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving))
                HuddleUp(resident, size);
            if (resident.State == ResidentState::Leaving && static_cast<int>(guests.size()) <= m_capacity)
            {
                PlaceTarget(resident, size, false);
                resident.State = ResidentState::Entering;
                resident.GroundLine = -1.0f;
                if (resident.Habit == Habit::Stays)
                {
                    resident.Y = -size.SpriteHeight;
                    resident.FallSpeed = 0.0f;
                }
            }
        }

        const auto staying = [&]()
            {
                return static_cast<int>(std::count_if(m_residents.begin(), m_residents.end(), [](const TerrariumResident& resident) { return resident.State != ResidentState::Leaving; }));
            };

        if (!animated)
        {
            for (TerrariumResident& resident : m_residents)
            {
                if (resident.Fetch != FetchStep::None)
                    EndFetch(resident);
            }
            m_ballThrown = false;
            if (m_ball.State == BallState::Flying)
            {
                m_ball.State = BallState::Lying;
                m_ball.Y = m_ball.Floor;
            }
            m_shell = {};
            m_messes.clear();
            for (TerrariumResident& resident : m_residents)
            {
                resident.Bare = resident.BareNoticed = false;
                if (resident.State == ResidentState::Gone)
                {
                    resident.State = ResidentState::Resting;
                    resident.GoneUntil = 0.0;
                    resident.NextBurst = 0.0;
                }
            }
            std::erase_if(m_residents, [](const TerrariumResident& resident) { return resident.State == ResidentState::Leaving; });
            for (TerrariumResident& resident : m_residents)
            {
                resident.Encounter = TerrariumEncounter::None;
                resident.Partner = 0;
                resident.Lift = 0.0f;
                resident.ShiftX = 0.0f;
                resident.Squash = 0.0f;
                resident.WasThrown = false;
                resident.Huddling = false;
                resident.ShowsFirstForm = false;
                if ((resident.Reaction == TerrariumReaction::Joy || resident.Reaction == TerrariumReaction::Shrug) && m_clock < resident.ReactionEnds)
                {
                    resident.Emote = TerrariumEmote::Heart;
                    resident.EmoteAge = static_cast<float>(m_clock - resident.ReactionBegan);
                }
                else
                {
                    resident.Reaction = TerrariumReaction::None;
                    resident.Emote = TerrariumEmote::None;
                }
                if (resident.State == ResidentState::Held)
                    continue;
                const auto [low, high] = BandOf(PlaceOf(resident), size);
                if (resident.State == ResidentState::Settling)
                {
                    resident.Y = std::clamp(resident.Y, low, high);
                    ComeToRest(resident);
                }
                else if (resident.State == ResidentState::Resting)
                {
                    resident.Y = std::clamp(resident.Y, low, high);
                    resident.TargetY = resident.Y;
                }
            }
            while (staying() < m_capacity)
            {
                const TerrariumGuest* guest = NextWaiting(guests);
                if (!guest)
                    break;
                Admit(*guest, size, false);
            }
            return;
        }

        if (m_clock >= m_nextArrival && staying() < m_capacity)
        {
            if (const TerrariumGuest* guest = NextWaiting(guests))
            {
                Admit(*guest, size, true);
                m_nextArrival = m_clock + kArrivalSeconds;
            }
        }

        if (static_cast<int>(guests.size()) > m_capacity && staying() >= m_capacity)
        {
            if (m_nextTurn <= 0.0)
                m_nextTurn = m_clock + Between(kMinTurnSeconds, kMaxTurnSeconds);
            if (m_clock >= m_nextTurn)
            {
                std::vector<TerrariumResident*> candidates;
                for (TerrariumResident& resident : m_residents)
                {
                    if (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving)
                        candidates.push_back(&resident);
                }
                if (!candidates.empty())
                {
                    const auto pick = static_cast<std::size_t>(Between(0.0f, static_cast<float>(candidates.size()) - 0.001f));
                    SendAway(*candidates[pick], size);
                }
                m_nextTurn = m_clock + Between(kMinTurnSeconds, kMaxTurnSeconds);
            }
        }

        StepReactions(size, static_cast<float>(elapsed));
        StepEncounters(size);
        StepBall(size, static_cast<float>(elapsed));
        StepThing(m_shell, size, static_cast<float>(elapsed));
        StepShell(size);
        StepBursts(size);
        if (m_clock >= m_nextEncounter)
        {
            const auto busy = std::count_if(m_residents.begin(), m_residents.end(), [](const TerrariumResident& resident) { return resident.Encounter != TerrariumEncounter::None; });
            const auto many = 1.0f + static_cast<float>(m_residents.size()) / 10.0f;
            if (static_cast<float>(busy) < many * 2.0f)
                BeginEncounter(size);
            m_nextEncounter = m_clock + Between(kMinEncounterSeconds, kMaxEncounterSeconds) / many;
        }

        for (TerrariumResident& resident : m_residents)
        {
            if (resident.State == ResidentState::Held)
                continue;
            if (resident.State == ResidentState::Gone)
            {
                if (m_clock >= resident.GoneUntil)
                    ComeBack(resident, size);
                continue;
            }
            if (resident.State == ResidentState::Settling)
            {
                Settle(resident, size, static_cast<float>(elapsed));
                continue;
            }
            if (resident.State == ResidentState::Resting)
            {
                if (!m_covers.empty() && resident.Habit == Habit::Walks && !resident.Bare && resident.Encounter == TerrariumEncounter::None && resident.Reaction == TerrariumReaction::None &&
                    InFoot(resident.X + size.SpriteWidth * 0.5f, resident.Y + size.SpriteHeight, size.SpriteWidth * 0.3f))
                {
                    PlaceTarget(resident, size, false);
                    resident.State = ResidentState::Moving;
                    continue;
                }
                if (!resident.Asleep && !resident.Bare && resident.Habit != Habit::Stays && resident.Encounter == TerrariumEncounter::None && resident.Reaction == TerrariumReaction::None)
                {
                    if (m_clock >= resident.RestEnds)
                    {
                        PlaceTarget(resident, size, true);
                        resident.State = ResidentState::Moving;
                    }
                    else if (m_pointerKnown && std::fabs(m_pointerX - resident.X) > 2.0f && std::fabs(m_pointerX - resident.X) < kLookReach)
                    {
                        resident.Direction = m_pointerX < resident.X ? -1 : 1;
                    }
                }
                continue;
            }
            if (resident.State == ResidentState::Moving && resident.Asleep && resident.Habit != Habit::Floats && !resident.Huddling && !resident.Bare && resident.Reaction == TerrariumReaction::None)
            {
                resident.State = ResidentState::Resting;
                continue;
            }

            const float dx = resident.TargetX - resident.X;
            const float dy = resident.TargetY - resident.Y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            float speed = SpeedOf(resident);
            if (resident.Habit == Habit::Floats && resident.State != ResidentState::Leaving)
            {
                const auto [low, high] = BandOf(PlaceOf(resident), size);
                if (resident.Asleep || resident.Y < low || resident.Y > high)
                    speed = kFlyHomeSpeed;
            }
            if (resident.Habit == Habit::Stays && (resident.State == ResidentState::Entering || (resident.State == ResidentState::Leaving && resident.Y < resident.GroundLine - size.SpriteHeight - 0.5f)))
            {
                resident.FallSpeed += kGravity * static_cast<float>(elapsed);
                speed = resident.FallSpeed;
            }
            if (resident.Bare)
                speed = kCrawlSpeed;
            else if (resident.Fetch == FetchStep::ToBall || resident.Fetch == FetchStep::Back)
                speed = resident.FetchEager ? kEagerSpeed : kFetchSpeed;
            else if (resident.Encounter == TerrariumEncounter::Chasing)
                speed = kChaseSpeed;
            else if (resident.Encounter == TerrariumEncounter::Fleeing)
                speed = kFleeSpeed;
            else if (resident.Reaction == TerrariumReaction::Eager || resident.Reaction == TerrariumReaction::Visiting)
                speed = kEagerSpeed;
            else if (resident.Reaction == TerrariumReaction::Furious)
                speed = kFuriousSpeed;
            else if (resident.Reaction == TerrariumReaction::Tugging)
                speed = kTugSpeed;
            else if (resident.Reaction == TerrariumReaction::Scared || resident.Reaction == TerrariumReaction::Startled)
                speed = kScaredSpeed;
            else if (resident.Reaction == TerrariumReaction::Dodging)
                speed = kDodgeSpeed;
            const float step = speed * static_cast<float>(elapsed);
            if (std::fabs(dx) > 0.5f)
                resident.Direction = dx < 0.0f ? -1 : 1;
            if (distance > step)
            {
                resident.X += dx / distance * step;
                resident.Y += dy / distance * step;
                continue;
            }

            resident.X = resident.TargetX;
            resident.Y = resident.TargetY;
            resident.Huddling = false;
            if (resident.State == ResidentState::Moving && (resident.Fetch == FetchStep::ToBall || resident.Fetch == FetchStep::Back))
            {
                ArriveFetching(resident, size);
                continue;
            }
            if (resident.State == ResidentState::Moving && resident.Bare)
            {
                ArriveAtShell(resident, size);
                continue;
            }
            if (resident.State != ResidentState::Leaving)
            {
                resident.State = ResidentState::Resting;
                resident.RestEnds = resident.Habit == Habit::Stays ? std::numeric_limits<double>::max()
                                                                   : m_clock + Between(kMinRestSeconds, kMaxRestSeconds) * (resident.Hiding ? kHidingRestFactor : 1.0f);
            }
            else
            {
                resident.Seed = 0;
            }
        }
        std::erase_if(m_residents, [](const TerrariumResident& resident) { return resident.Seed == 0; });

        if (static_cast<int>(guests.size()) <= m_capacity)
            m_nextTurn = 0.0;
    }
}
