#include "TestKit.h"

#include "Core/Clock.h"
#include "Core/Creatures.h"
#include "Core/MiniGames.h"
#include "Core/Terrarium.h"

using namespace Idler;

namespace
{
    TerrainGame PlayWell(MiniGame game, std::uint32_t seed, int& dizzyMoments, bool chasePebbles = false)
    {
        TerrainGame play(game, seed);
        dizzyMoments = 0;
        bool wasDizzy = false;
        while (!play.Over())
        {
            const float middleX = play.X() + kPlaySpriteWidth * 0.5f;
            const float middleY = play.Y() + kPlaySpriteHeight * 0.5f;
            float pointerX = middleX;
            float pointerY = middleY;
            float nearest = 1e9f;
            for (const PlayItem& item : play.Items())
            {
                const bool wanted = chasePebbles ? item.Kind == PlayItemKind::Pebble
                    : item.Kind == PlayItemKind::Treat || item.Kind == PlayItemKind::GoldenTreat || item.Kind == PlayItemKind::Star || item.Kind == PlayItemKind::ShootingStar ||
                    item.Kind == PlayItemKind::Pearl || (item.Kind == PlayItemKind::Clam && item.Open > 0.3f);
                const float distance = std::hypot(item.X - middleX, item.Y - middleY);
                if (wanted && distance < nearest)
                {
                    nearest = distance;
                    pointerX = item.X;
                    pointerY = item.Kind == PlayItemKind::Clam ? item.Y - 4.0f : item.Y;
                }
            }
            play.Step(1.0 / 60.0, pointerX, pointerY);
            if (play.Dizzy() > 0.0f && !wasDizzy)
                ++dizzyMoments;
            wasDizzy = play.Dizzy() > 0.0f;
            CHECK(play.X() >= 0.0f && play.X() <= kPlayWidth - kPlaySpriteWidth && play.Y() >= 0.0f && play.Y() + kPlaySpriteHeight <= play.Ground() + 3.5f);
        }
        return play;
    }
}

namespace Tests
{
    void TestMiniGames()
    {
        CHECK(TerrainGameOf(Habit::Walks) == MiniGame::TreatCatch && TerrainGameOf(Habit::Stays) == MiniGame::TreatCatch);
        CHECK(TerrainGameOf(Habit::Floats) == MiniGame::StarChase && TerrainGameOf(Habit::Swims) == MiniGame::PearlDive);
        for (std::size_t game = 0; game < kMiniGameCount; ++game)
        {
            CHECK(std::string_view(MiniGameName(static_cast<MiniGame>(game))).size() > 0 && std::string_view(MiniGameRules(static_cast<MiniGame>(game))).size() > 0);
            std::uint32_t before = 0;
            for (int score = 0; score <= 200; score += 5)
            {
                const std::uint32_t bond = PlayBondOf(static_cast<MiniGame>(game), score);
                CHECK(bond >= 3 && bond <= 10 && bond >= before);
                before = bond;
            }
            CHECK(PlayBondOf(static_cast<MiniGame>(game), -4) == 3);
        }

        PlayRecord record;
        CHECK(PlayBondLeft(record, 100) == kPlayBondPerDay);
        CHECK(GrantPlayBond(record, 100, 10) == 10 && GrantPlayBond(record, 100, 10) == 10);
        CHECK(GrantPlayBond(record, 100, 10) == kPlayBondPerDay - 20);
        CHECK(GrantPlayBond(record, 100, 10) == 0 && PlayBondLeft(record, 100) == 0);
        CHECK(record.Minutes == kPlayBondPerDay && record.MinutesThatDay == kPlayBondPerDay && record.Day == 100);
        CHECK(PlayBondLeft(record, 101) == kPlayBondPerDay);
        CHECK(GrantPlayBond(record, 101, 4) == 4 && record.MinutesThatDay == 4 && record.Minutes == kPlayBondPerDay + 4 && record.Day == 101);
        CHECK(std::fabs(BondHours(3600.0, 0, 30) - 1.5) < 1e-9 && BondHours(0.0, 0) == 0.0);

        for (const MiniGame game : { MiniGame::TreatCatch, MiniGame::StarChase, MiniGame::PearlDive })
        {
            int dizzy = 0;
            const TerrainGame good = PlayWell(game, 7u, dizzy);
            CHECK(good.Over() && good.TimeLeft() == 0.0);
            CHECK(good.Score() >= 15);
            int again = 0;
            CHECK(PlayWell(game, 7u, again).Score() == good.Score() && again == dizzy);

            TerrainGame idle(game, 7u);
            for (int step = 0; step < 60 * 50; ++step)
                idle.Step(1.0 / 60.0, 0.0f, 0.0f);
            CHECK(idle.Over() && idle.Score() < good.Score());
            const int ended = idle.Score();
            idle.TakeCatches();
            idle.Step(1.0, 50.0f, 50.0f);
            CHECK(idle.Score() == ended && idle.TakeCatches().empty());
        }
        {
            int dizzy = 0;
            const TerrainGame pebbles = PlayWell(MiniGame::TreatCatch, 11u, dizzy, true);
            CHECK(dizzy >= 2);
        }
        {
            TerrainGame slow(MiniGame::TreatCatch, 3u);
            TerrainGame fast(MiniGame::TreatCatch, 3u);
            for (int step = 0; step < 600; ++step)
                slow.Step(1.0 / 60.0, 88.0f, 0.0f);
            for (int step = 0; step < 100; ++step)
                fast.Step(0.1, 88.0f, 0.0f);
            CHECK(slow.Score() > 0 && std::abs(slow.Score() - fast.Score()) <= 1);
        }

        ShellGame shells(5u);
        CHECK(shells.CurrentPhase() == ShellGame::Phase::Show && shells.Round() == 1 && shells.Score() == 0);
        CHECK(shells.CupLift(shells.HidingCup()) == 1.0f);
        CHECK(!shells.PickSlot(0));
        int previousSwaps = 0;
        float previousSeconds = 10.0f;
        for (int round = 1; round <= 6; ++round)
        {
            CHECK(shells.Round() == round);
            CHECK(shells.SwapsThisRound() > previousSwaps && shells.SwapSeconds() < previousSeconds);
            previousSwaps = shells.SwapsThisRound();
            previousSeconds = shells.SwapSeconds();
            bool sawMove = false;
            while (shells.CurrentPhase() != ShellGame::Phase::Pick)
            {
                for (int cup = 0; cup < ShellGame::kCups; ++cup)
                {
                    const float position = shells.CupPosition(cup);
                    CHECK(position >= 0.0f && position <= 2.0f);
                    sawMove |= position != std::round(position);
                }
                shells.Step(1.0 / 60.0);
            }
            CHECK(sawMove);
            CHECK(shells.SlotOf(0) != shells.SlotOf(1) && shells.SlotOf(1) != shells.SlotOf(2) && shells.SlotOf(0) != shells.SlotOf(2));
            CHECK(shells.CupLift(0) == 0.0f && shells.CupLift(1) == 0.0f && shells.CupLift(2) == 0.0f);
            CHECK(shells.PickSlot(shells.SlotOf(shells.HidingCup())));
            CHECK(shells.CurrentPhase() == ShellGame::Phase::Reveal && shells.PickedCup() == shells.HidingCup());
            CHECK(!shells.PickSlot(0));
            for (int step = 0; step < 120; ++step)
                shells.Step(1.0 / 60.0);
            CHECK(shells.Score() == round);
        }
        while (shells.CurrentPhase() != ShellGame::Phase::Pick)
            shells.Step(0.05);
        const int wrongSlot = (shells.SlotOf(shells.HidingCup()) + 1) % ShellGame::kCups;
        CHECK(shells.PickSlot(wrongSlot));
        CHECK(shells.PickedCup() != shells.HidingCup());
        shells.Step(2.0);
        CHECK(shells.CurrentPhase() == ShellGame::Phase::Reveal);
        for (int step = 0; step < 8; ++step)
            shells.Step(0.25);
        CHECK(shells.Over() && shells.Score() == 6);
        CHECK(shells.CupLift(shells.HidingCup()) == 1.0f && shells.CupLift(shells.PickedCup()) == 1.0f);
        ShellGame jumpy(9u);
        jumpy.Step(30.0);
        for (int step = 0; step < 200 && jumpy.CurrentPhase() != ShellGame::Phase::Pick; ++step)
            jumpy.Step(0.25);
        CHECK(jumpy.CurrentPhase() == ShellGame::Phase::Pick);
        for (int cup = 0; cup < ShellGame::kCups; ++cup)
            CHECK(jumpy.CupPosition(cup) == static_cast<float>(jumpy.SlotOf(cup)));
    }

    void TestFetch()
    {
        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
            {
                for (double passed = 0.0; passed < seconds; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };
        const auto walker = [](CreatureId id, int bond = 0) { return std::vector<TerrariumGuest>{ TerrariumGuest{ 1, Habit::Walks, false, id, bond } }; };
        const auto throwBall = [&](Terrarium& box, float distance)
            {
                const TerrariumBall before = box.Ball();
                CHECK(box.GrabBall(before.X + 1.0f, before.Y - 1.0f));
                CHECK(box.Ball().State == BallState::Held);
                for (int step = 1; step <= 6; ++step)
                    box.CarryBall(before.X + distance * static_cast<float>(step) / 6.0f, before.Y - 2.0f * static_cast<float>(step), 1.0 / 60.0);
                box.DropBall(size);
            };
        const auto setUp = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests)
            {
                box.Update(guests, size, kStep, false);
                run(box, guests, 2.0);
                box.SetBall(true, size);
            };

        {
            Terrarium box(3u);
            const auto guests = walker(CreatureId::Crab);
            setUp(box, guests);
            CHECK(box.Ball().State == BallState::Lying);
            CHECK(box.Ball().Y > 130.0f - kTerrariumGroundDepth - 3.0f && box.Ball().Y < 130.0f - kTerrariumLakeDepth);
            CHECK(!box.GrabBall(box.Ball().X + 20.0f, box.Ball().Y));
            box.SetBall(false, size);
            CHECK(box.Ball().State == BallState::None);
        }

        {
            Terrarium box(5u);
            const auto guests = walker(CreatureId::Crab);
            setUp(box, guests);
            throwBall(box, 40.0f);
            CHECK(box.Ball().State == BallState::Flying);
            const float thrownFrom = box.Ball().ThrownFromX;
            bool carried = false;
            bool wentUp = false;
            std::vector<std::uint32_t> fetched;
            for (double passed = 0.0; passed < 25.0 && fetched.empty(); passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                carried |= box.Ball().State == BallState::Carried;
                wentUp |= box.Ball().Y < box.Ball().Floor - 5.0f;
                for (const std::uint32_t seed : box.TakeFetches())
                    fetched.push_back(seed);
                CHECK(box.Ball().X >= kBallRadius && box.Ball().X <= size.Width - kBallRadius);
            }
            CHECK(carried && wentUp);
            CHECK(fetched.size() == 1 && fetched[0] == 1);
            CHECK(box.Ball().State == BallState::Lying && std::fabs(box.Ball().X - thrownFrom) < 12.0f);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Joy && box.Residents()[0].Fetch == FetchStep::None);
            run(box, guests, 10.0);
            CHECK(box.TakeFetches().empty() && box.Ball().State == BallState::Lying);
        }

        {
            Terrarium box(5u);
            const auto guests = walker(CreatureId::Crab);
            setUp(box, guests);
            CHECK(box.GrabBall(box.Ball().X, box.Ball().Y));
            box.DropBall(size);
            run(box, guests, 8.0);
            CHECK(box.TakeFetches().empty() && box.Residents()[0].Fetch == FetchStep::None && box.Ball().State == BallState::Lying);
        }

        {
            Terrarium box(6u);
            const auto guests = walker(CreatureId::Capybara);
            setUp(box, guests);
            throwBall(box, 40.0f);
            run(box, guests, 12.0);
            CHECK(box.TakeFetches().empty() && box.Ball().Fetcher == 0 && box.Ball().State == BallState::Lying);
        }

        {
            Terrarium box(6u);
            const auto guests = walker(CreatureId::Bunny);
            setUp(box, guests);
            throwBall(box, -40.0f);
            bool waited = false;
            std::size_t fetched = 0;
            for (double passed = 0.0; passed < 30.0 && fetched == 0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                waited |= box.Residents()[0].Fetch == FetchStep::Waiting && box.Residents()[0].Reaction == TerrariumReaction::Puzzled;
                fetched += box.TakeFetches().size();
            }
            CHECK(waited && fetched == 1);
        }

        {
            Terrarium box(5u);
            const auto guests = walker(CreatureId::Crab);
            setUp(box, guests);
            throwBall(box, 40.0f);
            for (double passed = 0.0; passed < 25.0 && box.Ball().State != BallState::Carried; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Ball().State == BallState::Carried);
            box.Grab(1);
            box.Update(guests, size, kStep, true);
            CHECK(box.Ball().State == BallState::Flying || box.Ball().State == BallState::Lying);
            CHECK(box.Ball().Fetcher == 0 && box.Residents()[0].Fetch == FetchStep::None);
            run(box, guests, 5.0);
            CHECK(box.TakeFetches().empty() && box.Ball().State == BallState::Lying);
        }

        {
            Terrarium box(5u);
            const auto guests = walker(CreatureId::Crab);
            setUp(box, guests);
            throwBall(box, 40.0f);
            box.Update(guests, size, kStep, false);
            CHECK(box.Ball().State == BallState::Lying && box.Ball().Fetcher == 0);
            run(box, guests, 8.0);
            CHECK(box.TakeFetches().empty());
        }
    }

    void TestSnailShell()
    {
        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto one = [](CreatureId id) { return std::vector<TerrariumGuest>{ TerrariumGuest{ 1, Habit::Walks, false, id, 0 } }; };
        const auto hurl = [&](Terrarium& box, float speed)
            {
                box.Grab(1);
                box.Carry(1, 100.0f, 40.0f, 0.0);
                for (int step = 1; step <= 6; ++step)
                    box.Carry(1, 100.0f + speed * static_cast<float>(step) / 60.0f, 40.0f, 1.0 / 60.0);
                box.Drop(1, size);
            };
        const auto fresh = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests)
            {
                box.Update(guests, size, kStep, false);
                for (double passed = 0.0; passed < 1.0; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };
        struct Seen
        {
            bool CameOff = false;
            bool Sulked = false;
            bool Crawled = false;
        };
        const auto watch = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
            {
                Seen seen;
                for (double passed = 0.0; passed < seconds; passed += kStep)
                {
                    box.Update(guests, size, kStep, true);
                    const TerrariumResident& snail = box.Residents()[0];
                    seen.CameOff |= snail.Bare && box.Shell().State != BallState::None;
                    seen.Sulked |= snail.Bare && snail.Reaction == TerrariumReaction::Sulking;
                    seen.Crawled |= snail.Bare && snail.State == ResidentState::Moving;
                    if (seen.CameOff && !snail.Bare)
                        break;
                }
                return seen;
            };

        {
            Terrarium box(7u);
            const auto guests = one(CreatureId::Snail);
            fresh(box, guests);
            hurl(box, 400.0f);
            const Seen seen = watch(box, guests, 90.0);
            CHECK(seen.CameOff && seen.Sulked && seen.Crawled);
            CHECK(!box.Residents()[0].Bare && box.Shell().State == BallState::None);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Joy);
        }

        {
            Terrarium box(7u);
            const auto guests = one(CreatureId::Snail);
            fresh(box, guests);
            hurl(box, 60.0f);
            CHECK(!watch(box, guests, 6.0).CameOff && box.Shell().State == BallState::None);
        }
        {
            Terrarium box(7u);
            const auto guests = one(CreatureId::Crab);
            fresh(box, guests);
            hurl(box, 400.0f);
            CHECK(!watch(box, guests, 6.0).CameOff && box.Shell().State == BallState::None && !box.Residents()[0].Bare);
        }

        {
            Terrarium box(7u);
            const auto guests = one(CreatureId::Snail);
            fresh(box, guests);
            hurl(box, 400.0f);
            for (double passed = 0.0; passed < 10.0 && box.Shell().State != BallState::Lying; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Shell().State == BallState::Lying && box.Residents()[0].Bare);
            const TerrariumShell before = box.Shell();
            CHECK(!box.GrabShell(before.X + 20.0f, before.Y));
            CHECK(box.GrabShell(before.X, before.Y));
            const float to = before.X > 100.0f ? 30.0f : 170.0f;
            for (int step = 1; step <= 10; ++step)
                box.CarryShell(before.X + (to - before.X) * static_cast<float>(step) / 10.0f, before.Y - 10.0f, 1.0 / 60.0);
            box.DropShell(size);
            CHECK(box.Shell().State == BallState::Flying);
            const Seen seen = watch(box, guests, 120.0);
            CHECK(seen.Crawled && !box.Residents()[0].Bare && box.Shell().State == BallState::None);
        }

        {
            Terrarium box(7u);
            const auto guests = one(CreatureId::Snail);
            fresh(box, guests);
            hurl(box, 400.0f);
            for (double passed = 0.0; passed < 3.0 && !box.Residents()[0].Bare; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].Bare);
            box.Update(guests, size, kStep, false);
            CHECK(!box.Residents()[0].Bare && box.Shell().State == BallState::None);
        }
    }

    void TestParkaKidBurst()
    {
        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
            {
                for (double passed = 0.0; passed < seconds; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };
        const auto kid = [&](Terrarium& box) -> const TerrariumResident&
            {
                return *std::find_if(box.Residents().begin(), box.Residents().end(), [](const TerrariumResident& resident) { return resident.Seed == 1; });
            };
        const auto feetInMess = [&](const Terrarium& box, const TerrariumResident& resident)
            {
                return box.InMess(resident.X + size.SpriteWidth * 0.5f, resident.Y + size.SpriteHeight);
            };

        {
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
            Terrarium box(9u);
            box.Update(guests, size, kStep, false);
            run(box, guests, 1.0);
            box.Grab(1);
            box.Carry(1, 100.0f, 40.0f, 0.0);
            for (int step = 1; step <= 6; ++step)
                box.Carry(1, 100.0f + 400.0f * static_cast<float>(step) / 60.0f, 40.0f, 1.0 / 60.0);
            box.Drop(1, size);
            const float wall = size.Width - size.SpriteWidth;
            bool atWall = false;
            bool bouncedBack = false;
            float lastX = 0.0f;
            float lastY = 0.0f;
            float lastTargetY = 0.0f;
            for (double passed = 0.0; passed < 3.0 && kid(box).State != ResidentState::Gone; passed += kStep)
            {
                lastX = kid(box).X;
                lastY = kid(box).Y;
                lastTargetY = kid(box).TargetY;
                box.Update(guests, size, kStep, true);
                if (kid(box).State == ResidentState::Gone)
                    break;
                atWall |= kid(box).X >= wall - 0.5f;
                bouncedBack |= atWall && kid(box).VelocityX < 0.0f && kid(box).State == ResidentState::Settling;
            }
            CHECK(atWall && bouncedBack);
            CHECK(kid(box).State == ResidentState::Gone);
            CHECK(lastY >= lastTargetY - 6.0f);
            CHECK(lastX < wall - 4.0f);
            CHECK(box.Messes().size() == 1);
            if (box.Messes().size() == 1)
            {
                const TerrariumMess mess = box.Messes()[0];
                CHECK(std::fabs(mess.X - (lastX + size.SpriteWidth * 0.5f)) < 10.0f);
                CHECK(mess.Y >= 130.0f - kTerrariumGroundDepth && mess.Y <= 130.0f - kTerrariumLakeDepth);
                CHECK(box.InMess(mess.X, mess.Y) && !box.InMess(mess.X + mess.Radius + 1.0f, mess.Y) && !box.InMess(mess.X, mess.Y + mess.Radius * kMessDepthShare + 1.0f));
            }
            box.Grab(1);
            CHECK(kid(box).State == ResidentState::Gone);
            run(box, guests, 25.0);
            CHECK(kid(box).State == ResidentState::Gone && box.Messes().size() == 1);
            run(box, guests, 6.0);
            CHECK(kid(box).State != ResidentState::Gone && box.Messes().empty());
            CHECK(kid(box).State == ResidentState::Entering || kid(box).State == ResidentState::Moving || kid(box).State == ResidentState::Resting);
            CHECK(kid(box).NextBurst > box.Clock() + 100.0);
        }

        {
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
            Terrarium box(9u);
            box.Update(guests, size, kStep, false);
            run(box, guests, 1.0);
            box.Grab(1);
            box.Carry(1, 100.0f, 40.0f, 0.0);
            for (int step = 1; step <= 6; ++step)
                box.Carry(1, 100.0f + 400.0f * static_cast<float>(step) / 60.0f, 40.0f, 1.0 / 60.0);
            box.Drop(1, size);
            const float wall = size.Width - size.SpriteWidth;
            for (double passed = 0.0; passed < 1.0 && kid(box).X < wall - 0.5f; passed += 1.0 / 120.0)
                box.Update(guests, size, 1.0 / 120.0, true);
            CHECK(kid(box).State == ResidentState::Settling && kid(box).X >= wall - 0.5f);
            box.Grab(1);
            box.Carry(1, 120.0f, 60.0f, 0.0);
            box.Carry(1, 120.0f, 60.0f, 0.5);
            box.Drop(1, size);
            run(box, guests, 3.0);
            CHECK(kid(box).State != ResidentState::Gone && box.Messes().empty());
        }

        {
            std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
            for (std::uint32_t seed = 2; seed <= 7; ++seed)
                guests.push_back(TerrariumGuest{ seed, Habit::Walks, false, CreatureId::Crab, 0 });
            Terrarium box(11u);
            box.Update(guests, size, kStep, false);
            bool burst = false;
            bool startled = false;
            int insideLate = 0;
            for (double passed = 0.0; passed < 420.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                burst |= kid(box).State == ResidentState::Gone;
                if (box.Messes().empty())
                    continue;
                const double age = box.Clock() - box.Messes()[0].Began;
                for (const TerrariumResident& other : box.Residents())
                {
                    if (other.Seed == 1)
                        continue;
                    startled |= other.Reaction == TerrariumReaction::Startled;
                    if (age > 4.0 && other.State == ResidentState::Resting && feetInMess(box, other))
                        ++insideLate;
                }
            }
            CHECK(burst);
            CHECK(insideLate == 0);
            (void)startled;
        }

        {
            const std::vector<TerrariumGuest> asleep{ TerrariumGuest{ 1, Habit::Walks, true, CreatureId::ParkaKid, 0 } };
            Terrarium box(13u);
            box.Update(asleep, size, kStep, false);
            run(box, asleep, 400.0);
            CHECK(kid(box).State != ResidentState::Gone && box.Messes().empty());

            const std::vector<TerrariumGuest> awake{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::ParkaKid, 0 } };
            for (double passed = 0.0; passed < 420.0 && kid(box).State != ResidentState::Gone; passed += kStep)
                box.Update(awake, size, kStep, true);
            CHECK(kid(box).State == ResidentState::Gone);
            box.Update(awake, size, kStep, false);
            CHECK(kid(box).State == ResidentState::Resting && box.Messes().empty());
        }
    }

    void TestTug()
    {
        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, CreatureId::Demon, 0 } };
        const auto demon = [](const Terrarium& box) -> const TerrariumResident& { return box.Residents()[0]; };
        const auto throwIt = [&](Terrarium& box)
            {
                box.Grab(1);
                box.Carry(1, 100.0f, 20.0f, 0.0);
                for (int step = 1; step <= 4; ++step)
                    box.Carry(1, 100.0f + 0.02f * static_cast<float>(step), 20.0f, 1.0 / 60.0);
                box.Drop(1, size);
            };
        const auto fresh = [&](Terrarium& box)
            {
                box.Update(guests, size, kStep, false);
                for (double passed = 0.0; passed < 1.0; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };

        {
            Terrarium box(21u);
            fresh(box);
            box.SetPointer(true, 40.0f, 70.0f);
            box.SetTugAllowed(true);
            throwIt(box);
            bool tugged = false;
            bool moved = false;
            float startX = 0.0f;
            for (double passed = 0.0; passed < 8.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                const TerrariumTug tug = box.Tug();
                if (tug.Active && !tugged)
                    startX = tug.X;
                if (tug.Active)
                {
                    moved |= std::fabs(tug.X - startX) > 8.0f;
                    box.SetPointer(true, tug.X - size.SpriteWidth * 0.5f, tug.Y - size.SpriteHeight * 0.5f);
                }
                tugged |= tug.Active;
            }
            CHECK(tugged && moved);
            CHECK(!box.Tug().Active && demon(box).HasTugged);
            for (double passed = 0.0; passed < 6.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                CHECK(!box.Tug().Active);
            }
        }

        {
            Terrarium box(21u);
            fresh(box);
            box.SetPointer(true, 40.0f, 70.0f);
            box.SetTugAllowed(true);
            throwIt(box);
            for (double passed = 0.0; passed < 8.0 && !box.Tug().Active; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Tug().Active);
            box.LetGoOfHand(true);
            CHECK(!box.Tug().Active && demon(box).Reaction == TerrariumReaction::Dizzy);
        }

        {
            Terrarium box(21u);
            fresh(box);
            box.SetPointer(true, 40.0f, 70.0f);
            box.SetTugAllowed(false);
            throwIt(box);
            bool tugged = false;
            bool furious = false;
            for (double passed = 0.0; passed < 8.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                tugged |= box.Tug().Active;
                furious |= demon(box).Reaction == TerrariumReaction::Furious;
            }
            CHECK(furious && !tugged);
        }
    }
}
