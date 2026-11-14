#include "TestKit.h"

#include "Core/Creatures.h"
#include "Core/Library.h"
#include "Core/Settings.h"
#include "Core/Terrarium.h"

using namespace Idler;

namespace Tests
{
    void TestCreatures()
    {
        CHECK(CreatureOfGame(2169200) == CreatureId::Ghillie);
        CHECK(CreatureOfGame(238090) == CreatureId::Ghillie);
        CHECK(CreatureOfGame(782330) == CreatureId::Demon);
        CreatureFacts lateAtNight;
        lateAtNight.RunningApps = { 620 };
        lateAtNight.Hour = 22;
        CHECK(IsEarned(InfoOf(CreatureId::Firefly), lateAtNight));
        CHECK(!IsEarned(InfoOf(CreatureId::Owl), lateAtNight));
        CreatureFacts valentine;
        valentine.RunningApps = { 620 };
        valentine.Month = 2;
        valentine.Day = 14;
        CHECK(IsEarned(InfoOf(CreatureId::LoveLetter), valentine));
        CHECK(!IsEarned(InfoOf(CreatureId::Parrot), valentine));
        CreatureFacts petted;
        petted.Pets = 999;
        CHECK(!IsEarned(InfoOf(CreatureId::Pufferfish), petted));
        petted.Pets = 1000;
        CHECK(IsEarned(InfoOf(CreatureId::Pufferfish), petted));
        CHECK(CreatureWithKey("coffee") == CreatureId::CoffeeMug);

        const auto all = AllCreatures();
        CHECK(all.size() == kCreatureCount);
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            CHECK(static_cast<std::size_t>(all[index].Id) == index);
            CHECK(CreatureWithKey(all[index].Key) == all[index].Id);
            CHECK(std::string_view(all[index].Name).size() > 0 && std::string_view(all[index].How).size() > 0 && std::string_view(all[index].Flavour).size() > 0);
            for (std::size_t other = index + 1; other < all.size(); ++other)
                CHECK(std::string_view(all[index].Key) != all[other].Key);
            for (const std::uint32_t appId : all[index].Games)
            {
                if (appId != 0)
                    CHECK(CreatureOfGame(appId) == all[index].Id);
            }
        }
        CHECK(!CreatureWithKey("nothing"));

        CreatureFacts facts;
        CHECK(IsEarned(InfoOf(CreatureId::Slime), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Frog), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Bug), facts));

        facts.TotalHours = 98.4;
        facts.GamesIdled = 73;
        CHECK(IsEarned(InfoOf(CreatureId::Frog), facts) && IsEarned(InfoOf(CreatureId::Whale), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Bunny), facts));
        CHECK(IsEarned(InfoOf(CreatureId::Turtle), facts) && !IsEarned(InfoOf(CreatureId::Octopus), facts));
        CHECK(ProgressText(InfoOf(CreatureId::Bunny), facts) == "98 of 100 hours");
        CHECK(ProgressShare(InfoOf(CreatureId::Octopus), facts) == 0.73f);
        CHECK(ProgressText(InfoOf(CreatureId::Frog), facts) == "10 of 10 hours");
        CreatureSet someFound;
        someFound.set(static_cast<std::size_t>(CreatureId::Frog));
        someFound.set(static_cast<std::size_t>(CreatureId::Whale));
        const std::vector<CreatureId> next = NextFinds(someFound, facts, 3);
        CHECK(next.size() == 3);
        CHECK(next[0] == CreatureId::Snail);
        CHECK(std::find(next.begin(), next.end(), CreatureId::Frog) == next.end());
        CHECK(std::find(next.begin(), next.end(), CreatureId::Pumpkin) == next.end());
        CHECK(NextFinds(someFound, facts, 0).empty());
        CHECK(NextFinds(CreatureSet().set(), facts, 3).empty());

        CreatureFacts almost;
        almost.TotalHours = 99.9;
        CHECK(!IsEarned(InfoOf(CreatureId::Bunny), almost) && ProgressText(InfoOf(CreatureId::Bunny), almost) == "99 of 100 hours");
        CHECK(ProgressShare(InfoOf(CreatureId::Frog), facts) == 1.0f);
        CHECK(ProgressShare(InfoOf(CreatureId::Pumpkin), facts) < 0.0f && ProgressText(InfoOf(CreatureId::Pumpkin), facts).empty());

        facts.Month = 10;
        facts.Day = 31;
        CHECK(!IsEarned(InfoOf(CreatureId::Pumpkin), facts));
        facts.RunningApps = { 10 };
        CHECK(IsEarned(InfoOf(CreatureId::Pumpkin), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Snowman), facts));
        facts.Month = 12;
        for (const int day : { 23, 24, 26, 27 })
        {
            facts.Day = day;
            CHECK(IsEarned(InfoOf(CreatureId::Snowman), facts) == (day == 24 || day == 26));
        }
        facts.Day = 13;
        facts.Weekday = 4;
        CHECK(!IsEarned(InfoOf(CreatureId::BlackCat), facts));
        facts.Weekday = 5;
        CHECK(IsEarned(InfoOf(CreatureId::BlackCat), facts));
        facts.Hour = 2;
        CHECK(!IsEarned(InfoOf(CreatureId::Owl), facts));
        facts.Hour = 3;
        CHECK(IsEarned(InfoOf(CreatureId::Owl), facts));
        facts.RunningApps.clear();
        CHECK(!IsEarned(InfoOf(CreatureId::Owl), facts));

        CHECK(!IsEarned(InfoOf(CreatureId::Crab), facts));
        const int gamesBefore = facts.GamesIdled;
        NoteGameTotal(facts, 774801, kGameCountsAfterSeconds - 1.0);
        CHECK(!IsEarned(InfoOf(CreatureId::Crab), facts));
        CHECK(facts.GamesIdled == gamesBefore);
        NoteGameTotal(facts, 774801, kGameCountsAfterSeconds);
        CHECK(IsEarned(InfoOf(CreatureId::Crab), facts));
        CHECK(facts.GamesIdled == gamesBefore + 1);
        facts.RunningApps = { 414700 };
        CHECK(!IsEarned(InfoOf(CreatureId::NightCam), facts));
        NoteGameTotal(facts, 414700, kGameCountsAfterSeconds + 60.0);
        CHECK(IsEarned(InfoOf(CreatureId::NightCam), facts));
        CHECK(!IsEarned(InfoOf(CreatureId::Centipede), facts));
        facts.RunningApps.assign(32, 1u);
        CHECK(IsEarned(InfoOf(CreatureId::Centipede), facts));

        CHECK(CountDaysInARow({}) == 0);
        CHECK(CountDaysInARow({ 5.0, 0.0, 5.0, 5.0, 5.0 }) == 3);
        CHECK(CountDaysInARow({ 5.0, 5.0, 0.0 }) == 2);
        CHECK(CountDaysInARow({ 5.0, 0.0, 0.0 }) == 0);

        CreatureSet found;
        for (int index = 0; index < 4; ++index)
            found.set(static_cast<std::size_t>(index));
        std::vector<CreatureId> before;
        for (std::uint32_t appId = 1000; appId < 3000; ++appId)
            before.push_back(PickCreature(appId, found));
        CHECK(std::count(before.begin(), before.end(), CreatureId::Slime) > 300);
        CHECK(std::count(before.begin(), before.end(), CreatureId::Robot) > 300);
        found.set(static_cast<std::size_t>(CreatureId::Frog));
        int moved = 0;
        for (std::uint32_t appId = 1000; appId < 3000; ++appId)
        {
            const CreatureId now = PickCreature(appId, found);
            if (now != before[appId - 1000])
            {
                CHECK(now == CreatureId::Frog);
                ++moved;
            }
        }
        CHECK(moved > 200 && moved < 600);

        std::array<int, static_cast<std::size_t>(Rarity::Count)> perRarity{};
        for (const CreatureInfo& info : AllCreatures())
        {
            ++perRarity[static_cast<std::size_t>(RarityOf(info.Id))];
            if (info.Kind == UnlockKind::Game)
                CHECK(RarityOf(info.Id) == Rarity::Uncommon);
            if (info.Kind == UnlockKind::Start)
                CHECK(RarityOf(info.Id) == Rarity::Common);
            if (info.Kind == UnlockKind::Date)
                CHECK(RarityOf(info.Id) == Rarity::Rare);
        }
        CHECK(perRarity[0] == 10 && perRarity[1] == 28 && perRarity[2] == 13 && perRarity[3] == 9 && perRarity[4] == 8);
        for (int rarity = 0; rarity < static_cast<int>(Rarity::Count); ++rarity)
        {
            CHECK(std::string_view(RarityName(static_cast<Rarity>(rarity))).size() > 0);
            CHECK(RarityWeight(static_cast<Rarity>(rarity)) > 0);
            if (rarity > 0)
                CHECK(RarityWeight(static_cast<Rarity>(rarity)) < RarityWeight(static_cast<Rarity>(rarity - 1)));
        }

        CreatureSet everyone;
        everyone.set();
        CreatureSet withoutOne = everyone;
        withoutOne.reset(static_cast<std::size_t>(CreatureId::Sloth));
        std::array<int, kCreatureCount> shownOn{};
        int movedToSloth = 0;
        constexpr int kDrawn = 60000;
        for (std::uint32_t appId = 5000000; appId < 5000000 + kDrawn; ++appId)
        {
            const CreatureId withAll = PickCreature(appId, everyone);
            ++shownOn[static_cast<std::size_t>(withAll)];
            const CreatureId without = PickCreature(appId, withoutOne);
            if (without != withAll)
            {
                CHECK(withAll == CreatureId::Sloth);
                ++movedToSloth;
            }
        }
        int weights = 0;
        for (const CreatureInfo& info : AllCreatures())
            weights += info.Kind == UnlockKind::Game ? 0 : RarityWeight(RarityOf(info.Id));
        for (const CreatureInfo& info : AllCreatures())
        {
            const int shown = shownOn[static_cast<std::size_t>(info.Id)];
            if (info.Kind == UnlockKind::Game)
            {
                CHECK(shown == 0);
                continue;
            }
            const double expected = static_cast<double>(kDrawn) * RarityWeight(RarityOf(info.Id)) / weights;
            CHECK(shown > expected * 0.75 && shown < expected * 1.25);
        }
        CHECK(shownOn[static_cast<std::size_t>(CreatureId::Slime)] > shownOn[static_cast<std::size_t>(CreatureId::Sloth)] * 5);
        CHECK(movedToSloth == shownOn[static_cast<std::size_t>(CreatureId::Sloth)]);

        CHECK(PickCreature(774801, found) != CreatureId::Crab);
        found.set(static_cast<std::size_t>(CreatureId::Crab));
        CHECK(PickCreature(774801, found) == CreatureId::Crab);
        for (std::uint32_t appId = 1000; appId < 3000; ++appId)
            CHECK(PickCreature(appId, found) != CreatureId::Crab);
        CHECK(PickCreature(5, CreatureSet{}) == CreatureId::Slime);

        int shiny = 0;
        for (std::uint32_t appId = 0; appId < 128000; ++appId)
            shiny += IsShiny(appId) ? 1 : 0;
        CHECK(shiny > 800 && shiny < 1200);
        std::uint32_t shinyApp = 0;
        while (!IsShiny(shinyApp))
            ++shinyApp;
        CHECK(!IsEarned(InfoOf(CreatureId::Star), {}));
        CreatureFacts shinyFacts;
        shinyFacts.RunningApps = { shinyApp };
        CHECK(IsEarned(InfoOf(CreatureId::Star), shinyFacts));
    }

    void TestTerrarium()
    {
        const TerrariumSize size{ 78.0f, 130.0f, 16.0f, 14.0f };
        const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds, bool animated = true)
            {
                for (double passed = 0.0; passed < seconds; passed += 0.05)
                    box.Update(guests, size, 0.05, animated);
            };
        const auto inside = [&](const TerrariumResident& resident)
            {
                return resident.X >= 0.0f && resident.X <= size.Width - size.SpriteWidth && resident.Y >= 0.0f && resident.Y <= size.Height - size.SpriteHeight;
            };

        Terrarium box;
        const std::vector<TerrariumGuest> three = { { 1, Habit::Walks, false }, { 2, Habit::Floats, false }, { 3, Habit::Stays, false } };
        box.Update(three, size, 0.05, true);
        CHECK(box.Residents().size() == 1);
        run(box, three, 10.0);
        CHECK(box.Residents().size() == 3);
        float stayX = -1.0f;
        for (const TerrariumResident& resident : box.Residents())
        {
            if (resident.Habit == Habit::Stays)
                stayX = resident.X;
        }
        bool allInside = true;
        for (int second = 0; second < 300; ++second)
        {
            run(box, three, 1.0);
            for (const TerrariumResident& resident : box.Residents())
            {
                allInside = allInside && (inside(resident) || resident.State == ResidentState::Entering);
                if (resident.Habit == Habit::Stays)
                    CHECK(resident.X == stayX);
                if (resident.Habit == Habit::Walks)
                    CHECK(resident.Y >= size.Height - size.SpriteHeight - kTerrariumGroundDepth - 0.01f);
                if (resident.Habit == Habit::Floats && resident.Encounter == TerrariumEncounter::None && resident.State != ResidentState::Moving)
                    CHECK(resident.Y <= size.Height - size.SpriteHeight - kTerrariumGroundDepth);
            }
        }
        CHECK(allInside);

        {
            const TerrariumSize deep{ 120.0f, 400.0f, 16.0f, 14.0f, 140.0f, 120.0f };
            Terrarium sinking;
            const std::vector<TerrariumGuest> trophy = { TerrariumGuest{ 7, Habit::Stays, false, CreatureId::Trophy } };
            for (double passed = 0.0; passed < 10.0; passed += 0.05)
                sinking.Update(trophy, deep, 0.05, true);
            CHECK(sinking.Residents().size() == 1 && sinking.Residents()[0].State == ResidentState::Resting);
            const float stood = sinking.Residents().empty() ? 0.0f : sinking.Residents()[0].Y;
            const float lakeTop = deep.Height - deep.LakeDepth;
            bool keptOutOfLake = true;
            bool hiddenBelowWhereItStood = true;
            double gone = -1.0;
            for (double passed = 0.05; passed < 5.0; passed += 0.05)
            {
                sinking.Update({}, deep, 0.05, true);
                for (const TerrariumResident& resident : sinking.Residents())
                {
                    keptOutOfLake = keptOutOfLake && resident.Y <= lakeTop + 0.01f;
                    hiddenBelowWhereItStood = hiddenBelowWhereItStood && std::fabs(resident.GroundLine - (stood + deep.SpriteHeight)) < 0.01f;
                }
                if (sinking.Residents().empty() && gone < 0.0)
                    gone = passed;
            }
            CHECK(keptOutOfLake);
            CHECK(hiddenBelowWhereItStood);
            CHECK(gone > 0.5 && gone < 2.0);

            Terrarium dropping;
            dropping.Update(trophy, deep, 0.05, true);
            dropping.Update(trophy, deep, 0.05, true);
            const float landsAt = dropping.Residents().empty() ? 0.0f : dropping.Residents()[0].TargetY;
            CHECK(!dropping.Residents().empty() && dropping.Residents()[0].State == ResidentState::Entering && dropping.Residents()[0].Y < landsAt);
            dropping.Update({}, deep, 0.05, true);
            CHECK(!dropping.Residents().empty() && std::fabs(dropping.Residents()[0].GroundLine - (landsAt + deep.SpriteHeight)) < 0.01f);
        }

        std::vector<TerrariumGuest> sleepers = three;
        sleepers[0].Asleep = true;
        run(box, sleepers, 0.1);
        const auto walker = [&]() { return *std::find_if(box.Residents().begin(), box.Residents().end(), [](const TerrariumResident& r) { return r.Seed == 1; }); };
        run(box, sleepers, 1.0);
        const float sleptAt = walker().X;
        CHECK(walker().Asleep && walker().State == ResidentState::Resting);
        run(box, sleepers, 20.0);
        CHECK(walker().X == sleptAt);
        run(box, three, 20.0);
        CHECK(!walker().Asleep);

        run(box, { three[1], three[2] }, 20.0);
        CHECK(box.Residents().size() == 2);
        run(box, {}, 20.0);
        CHECK(box.Residents().empty());

        std::vector<TerrariumGuest> crowd;
        for (std::uint32_t seed = 1; seed <= 32; ++seed)
            crowd.push_back({ seed, Habit::Walks, seed % 2 == 0 });
        Terrarium busy;
        run(busy, crowd, 8.0);
        CHECK(static_cast<int>(busy.Residents().size()) == kMaxTerrariumResidents);
        CHECK(std::none_of(busy.Residents().begin(), busy.Residents().end(), [](const TerrariumResident& r) { return r.Asleep; }));
        std::vector<std::uint32_t> seen;
        for (int second = 0; second < 1200; ++second)
        {
            run(busy, crowd, 1.0);
            int staying = 0;
            for (const TerrariumResident& resident : busy.Residents())
            {
                staying += resident.State != ResidentState::Leaving ? 1 : 0;
                if (std::find(seen.begin(), seen.end(), resident.Seed) == seen.end())
                    seen.push_back(resident.Seed);
            }
            CHECK(staying <= kMaxTerrariumResidents);
        }
        CHECK(seen.size() > kMaxTerrariumResidents);

        Terrarium again;
        run(again, three, 10.0);
        run(again, { three[1], three[2] }, 0.1);
        run(again, three, 20.0);
        CHECK(again.Residents().size() == 3);

        Terrarium still;
        still.Update(three, size, 0.05, false);
        CHECK(still.Residents().size() == 3);
        for (const TerrariumResident& resident : still.Residents())
            CHECK(inside(resident) && resident.State == ResidentState::Resting);
        const float stillX = still.Residents()[0].X;
        run(still, three, 10.0, false);
        CHECK(still.Residents()[0].X == stillX);
        still.Update({}, size, 0.05, false);
        CHECK(still.Residents().empty());

        Terrarium lake;
        const std::vector<TerrariumGuest> shore = { { 1, Habit::Walks, false }, { 2, Habit::Swims, false } };
        for (int second = 0; second < 200; ++second)
        {
            run(lake, shore, 1.0);
            for (const TerrariumResident& resident : lake.Residents())
            {
                if (resident.Habit == Habit::Swims)
                    CHECK(resident.Y >= size.Height - kTerrariumLakeDepth);
                else
                    CHECK(resident.Y + size.SpriteHeight <= size.Height - kTerrariumLakeDepth);
            }
        }

        const auto onLand = [](const TerrariumResident& resident, const TerrariumSize& box)
            {
                return resident.Y + box.SpriteHeight >= box.Height - box.GroundDepth - 0.01f && resident.Y + box.SpriteHeight <= box.Height - box.LakeDepth;
            };
        const TerrariumSize tall{ 300.0f, 700.0f, 16.0f, 14.0f };
        Terrarium falling;
        const std::vector<TerrariumGuest> cube = { { 3, Habit::Stays, false } };
        for (int step = 0; step < 60; ++step)
            falling.Update(cube, tall, 0.05, true);
        CHECK(falling.Residents().size() == 1 && falling.Residents()[0].State == ResidentState::Resting && onLand(falling.Residents()[0], tall));

        const std::vector<TerrariumGuest> all = { { 1, Habit::Walks, false }, { 2, Habit::Floats, false }, { 3, Habit::Stays, false }, { 4, Habit::Swims, false } };
        for (const bool animated : { false, true })
        {
            Terrarium growing;
            growing.Update(all, size, 0.05, false);
            growing.Update(all, tall, 0.05, animated);
            float stays = -1.0f;
            for (const TerrariumResident& resident : growing.Residents())
            {
                if (resident.Habit == Habit::Walks || resident.Habit == Habit::Stays)
                    CHECK(onLand(resident, tall));
                if (resident.Habit == Habit::Swims)
                    CHECK(resident.Y >= tall.Height - tall.LakeDepth);
                if (resident.Habit == Habit::Floats)
                    CHECK(resident.Y + tall.SpriteHeight <= tall.Height - tall.GroundDepth);
                if (resident.Habit == Habit::Stays)
                    stays = resident.Y;
            }
            growing.Update(all, size, 0.05, animated);
            for (const TerrariumResident& resident : growing.Residents())
            {
                CHECK(resident.X <= size.Width - size.SpriteWidth);
                if (resident.Habit == Habit::Stays)
                    CHECK(onLand(resident, size) && stays != resident.Y);
            }
        }

        const TerrariumSize roomy{ 400.0f, 300.0f, 16.0f, 14.0f, 100.0f, 40.0f };
        const TerrariumSize narrow{ 200.0f, 120.0f, 16.0f, 14.0f, 50.0f, 20.0f };
        std::vector<TerrariumGuest> crowded;
        for (std::uint32_t seed = 1; seed <= 24; ++seed)
            crowded.push_back({ seed, seed % 3 == 0 ? Habit::Floats : seed % 3 == 1 ? Habit::Walks : Habit::Swims, false });
        Terrarium keeping(5u, 24);
        keeping.Update(crowded, roomy, 0.0, false);
        const std::vector<TerrariumResident> atFirst = keeping.Residents();
        const auto landShare = [](const TerrariumResident& resident, const TerrariumSize& box)
            {
                const float low = box.Height - box.SpriteHeight - box.GroundDepth;
                const float high = box.Height - box.SpriteHeight - box.LakeDepth - 1.0f;
                return (resident.Y - low) / (high - low);
            };
        keeping.Update(crowded, narrow, 0.0, false);
        CHECK(keeping.Residents().size() == atFirst.size());
        float lowestShare = 1.0f;
        float highestShare = 0.0f;
        for (std::size_t index = 0; index < atFirst.size(); ++index)
        {
            const TerrariumResident& resident = keeping.Residents()[index];
            CHECK(std::fabs(resident.X / (narrow.Width - narrow.SpriteWidth) - atFirst[index].X / (roomy.Width - roomy.SpriteWidth)) < 0.001f);
            if (resident.Habit != Habit::Walks)
                continue;
            CHECK(std::fabs(landShare(resident, narrow) - landShare(atFirst[index], roomy)) < 0.001f);
            lowestShare = std::min(lowestShare, landShare(resident, narrow));
            highestShare = std::max(highestShare, landShare(resident, narrow));
        }
        CHECK(highestShare - lowestShare > 0.4f);
        keeping.Update(crowded, roomy, 0.0, false);
        for (std::size_t index = 0; index < atFirst.size(); ++index)
        {
            CHECK(std::fabs(keeping.Residents()[index].X - atFirst[index].X) < 0.01f);
            CHECK(std::fabs(keeping.Residents()[index].Y - atFirst[index].Y) < 0.01f);
        }

        Terrarium passing;
        const std::vector<TerrariumGuest> oneWalker = { { 1, Habit::Walks, false } };
        passing.Update(oneWalker, roomy, 0.05, true);
        CHECK(passing.Residents().size() == 1 && passing.Residents()[0].State == ResidentState::Entering);
        const float outside = passing.Residents()[0].X < 0.0f ? passing.Residents()[0].X : passing.Residents()[0].X - roomy.Width;
        passing.Update(oneWalker, narrow, 0.0, true);
        CHECK(std::fabs((passing.Residents()[0].X < 0.0f ? passing.Residents()[0].X : passing.Residents()[0].X - narrow.Width) - outside) < 0.01f);
        for (int step = 0; step < 200; ++step)
            passing.Update(oneWalker, narrow, 0.05, true);
        passing.Update({}, narrow, 0.05, true);
        CHECK(passing.Residents().size() == 1 && passing.Residents()[0].State == ResidentState::Leaving);
        const bool leftWall = passing.Residents()[0].TargetX < 0.0f;
        passing.Update({}, roomy, 0.0, true);
        CHECK(leftWall ? passing.Residents()[0].TargetX < 0.0f : passing.Residents()[0].TargetX > roomy.Width);

        const auto residentOf = [](const Terrarium& of, std::uint32_t seed) { return *std::find_if(of.Residents().begin(), of.Residents().end(), [&](const TerrariumResident& r) { return r.Seed == seed; }); };
        const auto carry = [&](Terrarium& hand, std::uint32_t seed, float x, float y)
            {
                hand.Grab(seed);
                hand.Carry(seed, x, y);
                for (int step = 0; step < 40; ++step)
                    hand.Update(all, tall, 0.05, true);
                CHECK(residentOf(hand, seed).X == x && residentOf(hand, seed).Y == y && residentOf(hand, seed).State == ResidentState::Held);
                hand.Drop(seed, tall);
            };
        const auto settle = [&](Terrarium& hand) { for (int step = 0; step < 160; ++step) hand.Update(all, tall, 0.05, true); };
        Terrarium hand;
        hand.Update(all, tall, 0.05, false);
        carry(hand, 1, 50.0f, 10.0f);
        CHECK(residentOf(hand, 1).State == ResidentState::Settling);
        settle(hand);
        CHECK(residentOf(hand, 1).State != ResidentState::Settling && onLand(residentOf(hand, 1), tall));
        carry(hand, 1, 50.0f, tall.Height - tall.SpriteHeight);
        settle(hand);
        CHECK(onLand(residentOf(hand, 1), tall));
        carry(hand, 2, 60.0f, tall.Height - tall.SpriteHeight);
        settle(hand);
        CHECK(residentOf(hand, 2).Y + tall.SpriteHeight <= tall.Height - tall.GroundDepth);
        carry(hand, 4, 70.0f, 10.0f);
        settle(hand);
        CHECK(residentOf(hand, 4).Y >= tall.Height - tall.LakeDepth);
        carry(hand, 3, 80.0f, 10.0f);
        settle(hand);
        CHECK(residentOf(hand, 3).X == 80.0f && residentOf(hand, 3).State == ResidentState::Resting && onLand(residentOf(hand, 3), tall));
        for (int step = 0; step < 400; ++step)
            hand.Update(all, tall, 0.05, true);
        CHECK(residentOf(hand, 3).X == 80.0f);
        Terrarium night;
        std::vector<TerrariumGuest> flyer = { { 2, Habit::Floats, false } };
        run(night, flyer, 10.0);
        CHECK(residentOf(night, 2).Y + size.SpriteHeight <= size.Height - size.GroundDepth);
        flyer[0].Asleep = true;
        run(night, flyer, 30.0);
        CHECK(residentOf(night, 2).State == ResidentState::Resting);
        CHECK(residentOf(night, 2).Y + size.SpriteHeight >= size.Height - size.GroundDepth);
        const float sleptAtX = residentOf(night, 2).X;
        run(night, flyer, 30.0);
        CHECK(residentOf(night, 2).X == sleptAtX);
        flyer[0].Asleep = false;
        run(night, flyer, 30.0);
        CHECK(residentOf(night, 2).Y + size.SpriteHeight <= size.Height - size.GroundDepth);
        Terrarium dark;
        const std::vector<TerrariumGuest> sleepingFlyer = { { 2, Habit::Floats, true } };
        dark.Update(sleepingFlyer, size, 0.05, false);
        CHECK(residentOf(dark, 2).Y + size.SpriteHeight >= size.Height - size.GroundDepth);

        const auto thrown = [&](Terrarium& box, std::uint32_t seed, float fromX, float fromY, float speedX, float speedY)
            {
                box.Grab(seed);
                float x = fromX;
                float y = fromY;
                box.Carry(seed, x, y);
                for (int step = 0; step < 10; ++step)
                {
                    x += speedX * 0.016f;
                    y += speedY * 0.016f;
                    box.Carry(seed, x, y, 0.016);
                }
                box.Drop(seed, tall);
                const auto at = residentOf(box, seed);
                CHECK(at.State == ResidentState::Settling);
                bool stayedInside = true;
                float farthest = at.X;
                for (int step = 0; step < 400; ++step)
                {
                    box.Update(all, tall, 0.05, true);
                    const auto now = residentOf(box, seed);
                    stayedInside = stayedInside && now.X >= 0.0f && now.X <= tall.Width - tall.SpriteWidth && now.Y >= 0.0f && now.Y <= tall.Height - tall.SpriteHeight;
                    if (now.State == ResidentState::Settling)
                        farthest = speedX > 0.0f ? std::max(farthest, now.X) : std::min(farthest, now.X);
                }
                CHECK(stayedInside);
                CHECK(residentOf(box, seed).State != ResidentState::Settling);
                return farthest - at.X;
            };
        Terrarium throwing;
        throwing.Update(all, tall, 0.05, false);
        CHECK(thrown(throwing, 1, 40.0f, 300.0f, 120.0f, 0.0f) > 10.0f);
        CHECK(onLand(residentOf(throwing, 1), tall) || residentOf(throwing, 1).State == ResidentState::Moving);
        CHECK(thrown(throwing, 2, 200.0f, 300.0f, -120.0f, 60.0f) < -10.0f);
        CHECK(thrown(throwing, 3, 100.0f, 400.0f, 90.0f, -200.0f) > 10.0f);
        CHECK(onLand(residentOf(throwing, 3), tall));
        CHECK(thrown(throwing, 4, 100.0f, 500.0f, 60.0f, 100.0f) > 1.0f);
        CHECK(residentOf(throwing, 4).Y >= tall.Height - tall.LakeDepth);
        Terrarium wall;
        wall.Update(all, tall, 0.05, false);
        thrown(wall, 1, 260.0f, 620.0f, 300.0f, 0.0f);
        CHECK(residentOf(wall, 1).X < tall.Width - tall.SpriteWidth);
        Terrarium steady;
        steady.Update(all, tall, 0.05, false);
        const auto before = residentOf(steady, 3);
        steady.Grab(3);
        for (int step = 0; step < 30; ++step)
            steady.Carry(3, before.X, before.Y, 0.016);
        steady.Drop(3, tall);
        CHECK(residentOf(steady, 3).State == ResidentState::Resting && residentOf(steady, 3).X == before.X);

        Terrarium plain;
        plain.Update(all, tall, 0.05, false);
        plain.Grab(1);
        plain.Carry(1, 5.0f, 5.0f);
        plain.Drop(1, tall);
        plain.Update(all, tall, 0.05, false);
        CHECK(residentOf(plain, 1).State == ResidentState::Resting && onLand(residentOf(plain, 1), tall));
        plain.Grab(99);
        plain.Carry(99, 1.0f, 1.0f);
        plain.Drop(99, tall);

        Terrarium sinking;
        sinking.Update(cube, size, 0.05, false);
        const float restedAt = sinking.Residents()[0].Y;
        sinking.Update({}, size, 0.05, true);
        sinking.Update({}, size, 0.05, true);
        CHECK(sinking.Residents()[0].State == ResidentState::Leaving && sinking.Residents()[0].Y > restedAt);

        Terrarium home(2u, 52);
        std::vector<TerrariumGuest> everyone;
        for (std::uint32_t seed = 1; seed <= 52; ++seed)
            everyone.push_back({ seed, Habit::Walks, false });
        home.Update(everyone, tall, 0.05, false);
        CHECK(home.Residents().size() == 52);
        for (int step = 0; step < 2000; ++step)
            home.Update(everyone, tall, 0.05, true);
        CHECK(home.Residents().size() == 52);
        CHECK(std::none_of(home.Residents().begin(), home.Residents().end(), [](const TerrariumResident& r) { return r.State == ResidentState::Leaving; }));

        Terrarium tiny;
        const TerrariumSize small{ 8.0f, 6.0f, 16.0f, 14.0f };
        for (int step = 0; step < 200; ++step)
            tiny.Update(three, small, 0.05, true);
        CHECK(tiny.Residents().size() == 3);
    }

    void TestBondsAndSets()
    {
        CHECK(BondLevel(0.0) == 0);
        CHECK(BondLevel(0.99) == 0);
        CHECK(BondLevel(1.0) == 1);
        CHECK(BondLevel(9.99) == 1);
        CHECK(BondLevel(10.0) == 2);
        CHECK(BondLevel(1000.0) == kMaxBondLevel);
        CHECK(BondLevel(100000.0) == kMaxBondLevel);
        CHECK(BondHours(3600.0, 0) == 1.0);
        CHECK(BondHours(0.0, 10) == 1.0);
        CHECK(BondHoursOfLevel(0) == 0.0);
        CHECK(BondHoursOfLevel(kMaxBondLevel) == 1000.0);
        CHECK(BondHoursOfLevel(kMaxBondLevel + 3) == 1000.0);
        CHECK(BondShare(0.0) == 0.0f);
        CHECK(BondShare(0.5) == 0.5f);
        CHECK(BondShare(1000.0) == 1.0f);
        for (int level = 0; level <= kMaxBondLevel; ++level)
        {
            CHECK(BondName(level)[0] != '\0');
            CHECK(BondLevel(BondHoursOfLevel(level)) == level);
        }

        std::array<int, kCreatureCount> sets{};
        for (const CreatureFamily& family : AllFamilies())
        {
            CHECK(family.Members.size() >= 2);
            for (std::size_t first = 0; first < family.Members.size(); ++first)
            {
                ++sets[static_cast<std::size_t>(family.Members[first])];
                for (std::size_t second = first + 1; second < family.Members.size(); ++second)
                    CHECK(family.Members[first] != family.Members[second]);
            }
        }
        for (const int count : sets)
            CHECK(count <= 2);

        CreatureSet found;
        const CreatureFamily& cats = AllFamilies()[1];
        CHECK(std::string_view(cats.Name) == "Cats");
        CHECK(FoundInFamily(cats, found) == 0);
        found.set(static_cast<std::size_t>(CreatureId::Cat));
        found.set(static_cast<std::size_t>(CreatureId::Slime));
        CHECK(FoundInFamily(cats, found) == 1);
        CHECK(IsInFamily(cats, CreatureId::BlackCat));
        CHECK(!IsInFamily(cats, CreatureId::Slime));

        CHECK(RelationOf(CreatureId::Cat, CreatureId::Cat) == Relation::None);
        CHECK(RelationOf(CreatureId::Cat, CreatureId::Hamster) == Relation::Chases);
        CHECK(RelationOf(CreatureId::Hamster, CreatureId::Cat) == Relation::None);
        CHECK(RelationOf(CreatureId::Cat, CreatureId::LeetCat) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Penguin, CreatureId::Snowman) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Snowman, CreatureId::Penguin) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Capybara, CreatureId::Shark) == Relation::Friends);
        CHECK(RelationOf(CreatureId::Slime, CreatureId::Trophy) == Relation::None);
        CHECK(RelationOf(CreatureId::Shark, CreatureId::Fish) == Relation::Chases);
        CHECK(RelationOf(CreatureId::Fish, CreatureId::Shark) == Relation::None);
        for (const CreatureInfo& one : AllCreatures())
        {
            for (const CreatureInfo& other : AllCreatures())
                CHECK(!(RelationOf(one.Id, other.Id) == Relation::Chases && RelationOf(other.Id, one.Id) == Relation::Chases));
        }
    }

    void TestEncounters()
    {
        const TerrariumSize size{ 78.0f, 130.0f, 16.0f, 14.0f };
        const auto inside = [&](const TerrariumResident& resident)
            {
                return resident.X >= 0.0f && resident.X <= size.Width - size.SpriteWidth && resident.Y >= 0.0f && resident.Y <= size.Height - size.SpriteHeight;
            };
        const auto walker = [](std::uint32_t seed, CreatureId id, bool asleep = false) { return TerrariumGuest{ seed, Habit::Walks, asleep, id }; };
        constexpr double kStep = 0.05;

        {
            Terrarium box(11u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::LeetCat) };
            bool sought = false;
            bool greeted = false;
            bool hopped = false;
            bool hearts = false;
            bool over = false;
            for (double passed = 0.0; passed < 120.0 && !over; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    sought |= resident.Encounter == TerrariumEncounter::Seeking;
                    if (resident.Encounter == TerrariumEncounter::Greeting)
                    {
                        greeted = true;
                        hopped |= resident.Lift > 0.5f;
                        hearts |= resident.Emote == TerrariumEmote::Heart;
                        const TerrariumResident& first = box.Residents()[0];
                        const TerrariumResident& second = box.Residents()[1];
                        CHECK(std::fabs(first.X - second.X) <= size.SpriteWidth + 1.0f);
                    }
                    else if (greeted && box.Residents()[0].Encounter == TerrariumEncounter::None && box.Residents()[1].Encounter == TerrariumEncounter::None)
                    {
                        over = true;
                    }
                }
            }
            CHECK(sought && greeted && hopped && hearts && over);
            for (const TerrariumResident& resident : box.Residents())
                CHECK(resident.Lift == 0.0f && resident.Emote == TerrariumEmote::None && resident.Partner == 0);
        }

        {
            Terrarium box(5u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::Hamster) };
            bool chased = false;
            bool alarm = false;
            for (double passed = 0.0; passed < 120.0 && !chased; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    if (resident.Encounter == TerrariumEncounter::Chasing)
                    {
                        chased = true;
                        CHECK(resident.Creature == CreatureId::Cat);
                    }
                }
            }
            CHECK(chased);
            for (double passed = 0.0; passed < 20.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    alarm |= resident.Emote == TerrariumEmote::Alarm;
                    if (resident.State != ResidentState::Entering)
                        CHECK(inside(resident));
                }
            }
            CHECK(alarm);
        }

        {
            Terrarium box(3u);
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Swims, false, CreatureId::Fish } };
            bool leapt = false;
            bool splash = false;
            float highest = 0.0f;
            for (double passed = 0.0; passed < 120.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    if (resident.Encounter != TerrariumEncounter::Leaping)
                        continue;
                    leapt = true;
                    splash |= resident.Emote == TerrariumEmote::Splash;
                    highest = std::max(highest, resident.Y + size.SpriteHeight - resident.Lift);
                    if (resident.Lift > 1.0f)
                        CHECK(resident.TargetY == resident.Y);
                }
            }
            CHECK(leapt && splash);
            CHECK(!box.Residents().empty() && box.Residents()[0].Lift == 0.0f);
        }

        {
            Terrarium box(9u);
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Floats, false, CreatureId::Firefly }, TerrariumGuest{ 2, Habit::Stays, false, CreatureId::Bonfire } };
            bool visited = false;
            for (double passed = 0.0; passed < 300.0 && !visited; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                    visited |= resident.Habit == Habit::Floats && resident.Encounter == TerrariumEncounter::Greeting;
            }
            CHECK(visited);
            for (double passed = 0.0; passed < 30.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Residents().size() == 2);
        }

        int huddled = 0;
        for (std::uint32_t seed = 1; seed <= 12; ++seed)
        {
            Terrarium box(seed);
            std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Slime, true), walker(2, CreatureId::Robot, true) };
            box.Update(guests, size, kStep, false);
            for (double passed = 0.0; passed < 30.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                    CHECK(resident.Encounter == TerrariumEncounter::None);
            }
            const float dx = box.Residents()[0].X - box.Residents()[1].X;
            const float dy = box.Residents()[0].Y - box.Residents()[1].Y;
            guests[1].Asleep = false;
            box.Update(guests, size, kStep, true);
            guests[1].Asleep = true;
            for (double passed = 0.0; passed < 15.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            const TerrariumResident& first = box.Residents()[0];
            const TerrariumResident& second = box.Residents()[1];
            CHECK(!first.Huddling && !second.Huddling);
            CHECK(first.State == ResidentState::Resting && second.State == ResidentState::Resting);
            if (std::sqrt(dx * dx + dy * dy) < 75.0f)
            {
                CHECK(std::fabs(first.X - second.X) <= size.SpriteWidth);
                ++huddled;
            }
        }
        CHECK(huddled > 0);

        {
            Terrarium box(11u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::LeetCat) };
            bool met = false;
            for (double passed = 0.0; passed < 120.0 && !met; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                met = box.Residents().size() == 2 && box.Residents()[0].Encounter != TerrariumEncounter::None;
            }
            CHECK(met);
            const std::uint32_t taken = box.Residents()[0].Seed;
            box.Grab(taken);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            for (const TerrariumResident& resident : box.Residents())
                CHECK(resident.Encounter == TerrariumEncounter::None && resident.Partner == 0);
            box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            box.Drop(taken, size);
        }

        {
            Terrarium box(11u);
            const std::vector<TerrariumGuest> guests{ walker(1, CreatureId::Cat), walker(2, CreatureId::LeetCat) };
            for (double passed = 0.0; passed < 30.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            box.Update(guests, size, kStep, false);
            for (const TerrariumResident& resident : box.Residents())
                CHECK(resident.Encounter == TerrariumEncounter::None && resident.Lift == 0.0f && resident.Emote == TerrariumEmote::None);
        }

        {
            Terrarium box(21u);
            std::vector<TerrariumGuest> guests;
            std::uint32_t seed = 1;
            for (const CreatureId id : { CreatureId::Cat, CreatureId::Hamster, CreatureId::Slime, CreatureId::KingSlime, CreatureId::Zombie, CreatureId::Bunny, CreatureId::Dwarf, CreatureId::Bug })
                guests.push_back(walker(seed++, id));
            guests.push_back(TerrariumGuest{ seed++, Habit::Swims, false, CreatureId::Shark });
            guests.push_back(TerrariumGuest{ seed++, Habit::Swims, false, CreatureId::Fish });
            guests.push_back(TerrariumGuest{ seed++, Habit::Floats, false, CreatureId::Bat });
            guests.push_back(TerrariumGuest{ seed++, Habit::Floats, false, CreatureId::Firefly });
            guests.push_back(TerrariumGuest{ seed++, Habit::Stays, false, CreatureId::Bonfire });
            int encounters = 0;
            for (double passed = 0.0; passed < 600.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                for (const TerrariumResident& resident : box.Residents())
                {
                    encounters += resident.Encounter == TerrariumEncounter::Greeting || resident.Encounter == TerrariumEncounter::Chasing ? 1 : 0;
                    if (resident.State == ResidentState::Resting || resident.State == ResidentState::Moving)
                    {
                        CHECK(resident.X >= -1.0f && resident.X <= size.Width - size.SpriteWidth + 1.0f);
                        CHECK(resident.Y >= -1.0f && resident.Y <= size.Height - size.SpriteHeight + 1.0f);
                    }
                    if (resident.Partner != 0)
                    {
                        const auto partner = std::find_if(box.Residents().begin(), box.Residents().end(), [&](const TerrariumResident& one) { return one.Seed == resident.Partner; });
                        CHECK(partner != box.Residents().end());
                    }
                }
            }
            CHECK(encounters > 0);
            guests.clear();
            for (double passed = 0.0; passed < 60.0; passed += kStep)
                box.Update(guests, size, kStep, true);
            CHECK(box.Residents().empty());
        }
    }

    void TestEvolutions()
    {
        CHECK(EvolutionOf(CreatureId::Slime) != nullptr);
        CHECK(std::size(kEvolutions) == kCreatureCount);
        CHECK(!EvolvesAt(CreatureId::Slime, kEvolveAtBond - 1));
        CHECK(EvolvesAt(CreatureId::Slime, kEvolveAtBond));
        CHECK(EvolvesAt(CreatureId::Robot, kMaxBondLevel));
        CHECK(!EvolvesAt(CreatureId::Crab, kEvolveAtBond - 1));
        for (std::size_t index = 0; index < std::size(kEvolutions); ++index)
        {
            const CreatureEvolution& evolution = kEvolutions[index];
            CHECK(index == 0 || kEvolutions[index - 1].Id < evolution.Id);
            CHECK(std::string_view(evolution.Name).find(InfoOf(evolution.Id).Name) != std::string_view::npos);
            CHECK(std::string_view(evolution.Flavour).size() > 0);
        }

        {
            const auto file = TestFile("-evolutions.ini");
            Settings saved;
            saved.Creatures.push_back({ "slime", 0u, 30u, true, false });
            saved.Creatures.push_back({ "cat", 1730000000u, 2u, true, true });
            saved.Creatures.push_back({ "frog", 1730000000u, 5u });
            CHECK(saved.Save(file));
            Settings loaded;
            loaded.Load(file);
            CHECK(loaded.Creatures.size() == 3);
            CHECK(loaded.Creatures.size() == 3 && loaded.Creatures[0].Evolved && !loaded.Creatures[0].FirstForm && loaded.Creatures[0].Pets == 30);
            CHECK(loaded.Creatures.size() == 3 && loaded.Creatures[1].Evolved && loaded.Creatures[1].FirstForm);
            CHECK(loaded.Creatures.size() == 3 && !loaded.Creatures[2].Evolved && !loaded.Creatures[2].FirstForm && loaded.Creatures[2].Pets == 5);
            std::filesystem::remove(file);
        }

        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto guest = [](std::uint32_t evolutions) { return TerrariumGuest{ 1, Habit::Walks, false, CreatureId::Slime, kEvolveAtBond, evolutions }; };

        {
            Terrarium box(5u);
            box.Update({ guest(1) }, size, kStep, true);
            box.Update({ guest(1) }, size, kStep, true);
            CHECK(box.Residents().size() == 1 && box.Residents()[0].Reaction == TerrariumReaction::None);
        }

        {
            Terrarium box(6u);
            box.Update({ guest(0) }, size, kStep, false);
            const TerrariumResident& slime = box.Residents()[0];
            box.Update({ guest(1) }, size, kStep, true);
            CHECK(slime.Reaction == TerrariumReaction::Evolving);
            int first = 0;
            int shown = 0;
            int turns = 0;
            bool last = true;
            bool petted = false;
            for (double passed = 0.0; passed < 1.65; passed += kStep)
            {
                box.Update({ guest(1) }, size, kStep, true);
                (slime.ShowsFirstForm ? first : shown) += 1;
                turns += slime.ShowsFirstForm != last ? 1 : 0;
                last = slime.ShowsFirstForm;
                if (!petted && passed > 0.5)
                {
                    box.Pet(1);
                    petted = true;
                }
            }
            CHECK(first > 0 && shown > 0);
            CHECK(turns >= 6);
            CHECK(slime.Reaction == TerrariumReaction::Evolving);
            bool hopped = false;
            bool heart = false;
            for (double passed = 0.0; passed < 0.6; passed += kStep)
            {
                box.Update({ guest(1) }, size, kStep, true);
                hopped = hopped || slime.Lift > 0.5f;
                heart = heart || slime.Emote == TerrariumEmote::Heart;
                CHECK(!slime.ShowsFirstForm);
            }
            CHECK(hopped && heart);
            for (double passed = 0.0; passed < 1.0; passed += kStep)
                box.Update({ guest(1) }, size, kStep, true);
            CHECK(slime.Reaction == TerrariumReaction::None && !slime.ShowsFirstForm && slime.Lift == 0.0f);
        }

        {
            Terrarium box(7u);
            box.Update({ guest(0) }, size, kStep, false);
            box.Update({ guest(1) }, size, kStep, false);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None && !box.Residents()[0].ShowsFirstForm);

            Terrarium held(8u);
            held.Update({ guest(0) }, size, kStep, false);
            held.Update({ guest(1) }, size, kStep, true);
            CHECK(held.Residents()[0].Reaction == TerrariumReaction::Evolving);
            held.Grab(1);
            held.Update({ guest(1) }, size, kStep, true);
            CHECK(!held.Residents()[0].ShowsFirstForm);
        }
    }

    void TestCharacters()
    {
        CHECK(CharacterOf(CreatureId::Crab).Throw == ThrowManner::Loves);
        CHECK(CharacterOf(CreatureId::Ghost).Throw == ThrowManner::Slips);
        CHECK(CharacterOf(CreatureId::Slime).Throw == ThrowManner::Splats);
        CHECK(CharacterOf(CreatureId::Dragon).Throw == ThrowManner::Furious);
        CHECK(CharacterOf(CreatureId::Mimic).Pet == PetManner::Snaps);
        CHECK(CharacterOf(CreatureId::Cat).Pet == PetManner::Moody);
        CHECK(CharacterOf(CreatureId::Pufferfish).Pet == PetManner::Touchy);
        CHECK(CharacterOf(CreatureId::Capybara).Pet == PetManner::Unmoved && CharacterOf(CreatureId::Capybara).Throw == ThrowManner::Unmoved);
        CHECK(CharacterOf(CreatureId::Sloth).Pet == PetManner::Unmoved);

        CHECK(OutcomeOfPet(CreatureId::Slime, 0, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Bunny, 0, 1) == PetOutcome::Dodge);
        CHECK(OutcomeOfPet(CreatureId::Bunny, 1, 1) == PetOutcome::Dodge);
        CHECK(OutcomeOfPet(CreatureId::Bunny, 2, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Mimic, 0, 1) == PetOutcome::Bite);
        CHECK(OutcomeOfPet(CreatureId::Mimic, 3, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Pufferfish, 0, 1) == PetOutcome::Puff);
        CHECK(OutcomeOfPet(CreatureId::Pufferfish, 4, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Capybara, 5, 1) == PetOutcome::Shrug);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 1) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 3) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 4) == PetOutcome::Bite);
        CHECK(OutcomeOfPet(CreatureId::Cat, 0, 5) == PetOutcome::Joy);
        CHECK(OutcomeOfPet(CreatureId::Cat, 4, 4) == PetOutcome::Joy);
        CHECK(ThrowMannerOf(CreatureId::Bunny, 0) == ThrowManner::Flees);
        CHECK(ThrowMannerOf(CreatureId::Bunny, 3) == ThrowManner::Unmoved);
        CHECK(ThrowMannerOf(CreatureId::Dragon, 0) == ThrowManner::Furious);
        CHECK(ThrowMannerOf(CreatureId::Dragon, 4) == ThrowManner::Hates);
        CHECK(ThrowMannerOf(CreatureId::Crab, 5) == ThrowManner::Loves);

        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        const auto run = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests, double seconds)
            {
                for (double passed = 0.0; passed < seconds; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };
        const auto walker = [](CreatureId id, int bond = 0, bool asleep = false) { return TerrariumGuest{ 1, Habit::Walks, asleep, id, bond }; };
        const auto throwIt = [&](Terrarium& box, const std::vector<TerrariumGuest>& guests)
            {
                box.Grab(1);
                box.Carry(1, 100.0f, 10.0f);
                box.Drop(1, size);
                for (double passed = 0.0; passed < 3.0 && box.Residents()[0].State == ResidentState::Settling; passed += kStep)
                    box.Update(guests, size, kStep, true);
            };

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Crab) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 20.0f, 70.0f);
            throwIt(box, guests);
            const TerrariumResident& crab = box.Residents()[0];
            CHECK(crab.Reaction == TerrariumReaction::Eager);
            CHECK(crab.Squash > 0.0f);
            float highest = 0.0f;
            for (double passed = 0.0; passed < 7.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                highest = std::max(highest, crab.Lift);
            }
            CHECK(std::fabs(crab.X - 20.0f) <= 5.0f);
            CHECK(highest > 1.0f);
            CHECK(crab.Emote == TerrariumEmote::Note);
            box.SetPointer(true, 150.0f, 70.0f);
            run(box, guests, 0.5);
            CHECK(crab.Reaction == TerrariumReaction::Eager);
            run(box, guests, 3.0);
            CHECK(crab.Reaction == TerrariumReaction::None);
            CHECK(crab.Lift == 0.0f && crab.Emote == TerrariumEmote::None && crab.Squash == 0.0f);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Bunny) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 100.0f, 70.0f);
            throwIt(box, guests);
            const TerrariumResident& bunny = box.Residents()[0];
            CHECK(bunny.Reaction == TerrariumReaction::Scared);
            run(box, guests, 4.0);
            CHECK(std::fabs(bunny.X - 100.0f) > 40.0f);
            CHECK(bunny.Emote == TerrariumEmote::Alarm);

            const std::vector<TerrariumGuest> trusting{ walker(CreatureId::Bunny, 3) };
            Terrarium other(4u);
            other.Update(trusting, size, kStep, false);
            other.SetPointer(true, 100.0f, 70.0f);
            throwIt(other, trusting);
            CHECK(other.Residents()[0].Reaction == TerrariumReaction::Sulking);
        }

        {
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Floats, false, CreatureId::Ghost, 0 } };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.Grab(1);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            run(box, guests, 0.3);
            CHECK(box.Residents()[0].State == ResidentState::Held);
            CHECK(box.Residents()[0].Emote == TerrariumEmote::Question);
            run(box, guests, 0.5);
            CHECK(box.Residents()[0].State != ResidentState::Held);
            box.Carry(1, 5.0f, 5.0f);
            box.Drop(1, size);
            run(box, guests, 6.0);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Slime) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            throwIt(box, guests);
            const TerrariumResident& slime = box.Residents()[0];
            CHECK(slime.Reaction == TerrariumReaction::Sulking);
            CHECK(slime.Squash > 0.9f);
            const float x = slime.X;
            run(box, guests, 3.0);
            CHECK(slime.X == x);
            CHECK(slime.Squash > 0.0f && slime.Squash < 0.9f);
            run(box, guests, 8.0);
            CHECK(slime.Reaction == TerrariumReaction::None && slime.Squash == 0.0f);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Hamster) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            throwIt(box, guests);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Dizzy);
            bool wobbled = false;
            for (double passed = 0.0; passed < 2.0; passed += kStep)
            {
                box.Update(guests, size, kStep, true);
                wobbled |= box.Residents()[0].ShiftX != 0.0f;
                CHECK(box.Residents()[0].Emote == TerrariumEmote::Stars);
            }
            CHECK(wobbled);

            const std::vector<TerrariumGuest> angry{ walker(CreatureId::Dragon) };
            Terrarium other(4u);
            other.Update(angry, size, kStep, false);
            other.SetPointer(true, 30.0f, 70.0f);
            throwIt(other, angry);
            CHECK(other.Residents()[0].Reaction == TerrariumReaction::Furious);
            run(other, angry, 4.0);
            CHECK(std::fabs(other.Residents()[0].X - 30.0f) <= 5.0f);
            CHECK(other.Residents()[0].Emote == TerrariumEmote::Anger);
        }

        {
            const std::vector<TerrariumGuest> shy{ walker(CreatureId::Bunny) };
            Terrarium box(4u);
            box.Update(shy, size, kStep, false);
            box.SetPointer(true, box.Residents()[0].X, box.Residents()[0].Y);
            const float x = box.Residents()[0].X;
            CHECK(box.Pet(1) == PetOutcome::Dodge);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Dodging);
            run(box, shy, 1.0);
            CHECK(std::fabs(box.Residents()[0].X - x) > 5.0f);
            CHECK(box.Residents()[0].Emote == TerrariumEmote::Dots);
            run(box, shy, 1.0);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None);

            const std::vector<TerrariumGuest> moody{ walker(CreatureId::Cat) };
            Terrarium cat(4u);
            cat.Update(moody, size, kStep, false);
            CHECK(cat.Pet(1) == PetOutcome::Joy);
            CHECK(cat.Pet(1) == PetOutcome::Joy);
            CHECK(cat.Pet(1) == PetOutcome::Joy);
            CHECK(cat.Pet(1) == PetOutcome::Bite);
            CHECK(cat.Residents()[0].Reaction == TerrariumReaction::Snapping);
            run(cat, moody, 13.0);
            CHECK(cat.Pet(1) == PetOutcome::Joy);

            const std::vector<TerrariumGuest> happy{ walker(CreatureId::Slime) };
            Terrarium slime(4u);
            slime.Update(happy, size, kStep, false);
            CHECK(slime.Pet(1) == PetOutcome::Joy);
            run(slime, happy, 0.15);
            CHECK(slime.Residents()[0].Lift > 0.5f && slime.Residents()[0].Emote == TerrariumEmote::Heart);
            run(slime, happy, 1.0);
            CHECK(slime.Residents()[0].Reaction == TerrariumReaction::None && slime.Residents()[0].Lift == 0.0f);

            CHECK(slime.Pet(99) == PetOutcome::Joy);
            slime.Grab(1);
            CHECK(slime.Pet(1) == PetOutcome::Joy);
            CHECK(slime.Residents()[0].State == ResidentState::Held);
            slime.Drop(1, size);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Crab, 0, true) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 20.0f, 70.0f);
            throwIt(box, guests);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::Eager);
            run(box, guests, 4.0);
            CHECK(std::fabs(box.Residents()[0].X - 20.0f) <= 5.0f);
            run(box, guests, 8.0);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None);
            CHECK(box.Residents()[0].State == ResidentState::Resting);
        }

        {
            const std::vector<TerrariumGuest> friendly{ walker(CreatureId::Bunny, 4) };
            Terrarium box(4u);
            box.Update(friendly, size, kStep, false);
            box.SetPointer(true, 60.0f, 70.0f);
            bool visited = false;
            for (double passed = 0.0; passed < 120.0 && !visited; passed += kStep)
            {
                box.Update(friendly, size, kStep, true);
                visited = box.Residents()[0].Reaction == TerrariumReaction::Visiting;
            }
            CHECK(visited);
            run(box, friendly, 5.0);
            CHECK(std::fabs(box.Residents()[0].X - 60.0f) <= 5.0f);

            const std::vector<TerrariumGuest> strange{ walker(CreatureId::Bunny, 3) };
            Terrarium other(4u);
            other.Update(strange, size, kStep, false);
            other.SetPointer(true, 60.0f, 70.0f);
            for (double passed = 0.0; passed < 120.0; passed += kStep)
            {
                other.Update(strange, size, kStep, true);
                CHECK(other.Residents()[0].Reaction != TerrariumReaction::Visiting);
            }

            Terrarium away(4u);
            away.Update(friendly, size, kStep, false);
            for (double passed = 0.0; passed < 120.0; passed += kStep)
            {
                away.Update(friendly, size, kStep, true);
                CHECK(away.Residents()[0].Reaction != TerrariumReaction::Visiting);
            }
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Cat) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, box.Residents()[0].X + 20.0f, 70.0f);
            box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].State != ResidentState::Resting || box.Residents()[0].Direction == 1);
            box.SetPointer(true, box.Residents()[0].X - 20.0f, 70.0f);
            box.Update(guests, size, kStep, true);
            CHECK(box.Residents()[0].State != ResidentState::Resting || box.Residents()[0].Direction == -1);
        }

        for (const CreatureInfo& info : AllCreatures())
        {
            const std::vector<TerrariumGuest> guests{ TerrariumGuest{ 1, Habit::Walks, false, info.Id, 0 } };
            Terrarium box(info.Key[0]);
            box.Update(guests, size, kStep, false);
            box.SetPointer(true, 60.0f, 60.0f);
            box.Pet(1);
            run(box, guests, 2.0);
            throwIt(box, guests);
            run(box, guests, 12.0);
            const TerrariumResident& resident = box.Residents()[0];
            CHECK(resident.Reaction == TerrariumReaction::None || resident.Reaction == TerrariumReaction::Visiting);
            CHECK(resident.X >= 0.0f && resident.X <= size.Width - size.SpriteWidth);
            CHECK(resident.Y >= 0.0f && resident.Y <= size.Height - size.SpriteHeight);
        }

        {
            const std::vector<TerrariumGuest> guests{ walker(CreatureId::Slime) };
            Terrarium box(4u);
            box.Update(guests, size, kStep, false);
            box.Pet(1);
            box.Update(guests, size, kStep, false);
            CHECK(box.Residents()[0].Emote == TerrariumEmote::Heart && box.Residents()[0].Lift == 0.0f);
            for (double passed = 0.0; passed < 1.0; passed += kStep)
                box.Update(guests, size, kStep, false);
            CHECK(box.Residents()[0].Reaction == TerrariumReaction::None && box.Residents()[0].Emote == TerrariumEmote::None);
        }
    }

    void TestCovers()
    {
        const TerrariumSize size{ 200.0f, 130.0f, 16.0f, 14.0f };
        constexpr double kStep = 0.05;
        std::vector<TerrariumGuest> guests;
        for (std::uint32_t seed = 1; seed <= 4; ++seed)
            guests.push_back(TerrariumGuest{ seed, Habit::Walks, false, seed % 2 == 0 ? CreatureId::Bunny : CreatureId::Crab, 0 });
        Terrarium box(17u);
        const std::vector<TerrariumCover> covers{ { 60.0f, 100.0f, 3.0f, 12.0f }, { 140.0f, 90.0f, 2.0f, 0.0f } };
        box.SetCovers(covers);
        box.Update(guests, size, kStep, false);
        int inFoot = 0;
        int hidden = 0;
        for (double passed = 0.0; passed < 600.0; passed += kStep)
        {
            box.Update(guests, size, kStep, true);
            for (const TerrariumResident& resident : box.Residents())
            {
                if (resident.State != ResidentState::Resting)
                    continue;
                const float middle = resident.X + size.SpriteWidth * 0.5f;
                const float feet = resident.Y + size.SpriteHeight;
                if (resident.Encounter == TerrariumEncounter::None && box.InFoot(middle, feet))
                    ++inFoot;
                if (resident.Hiding && feet < covers[0].Ground && feet > covers[0].Ground - 4.0f && std::fabs(middle - covers[0].X) <= covers[0].Cover)
                    ++hidden;
                CHECK(!(resident.Hiding && std::fabs(middle - covers[1].X) < 3.0f && feet < covers[1].Ground && feet > covers[1].Ground - 4.0f));
            }
        }
        CHECK(inFoot == 0);
        CHECK(hidden > 0);
        CHECK(box.InFoot(60.0f, 100.0f) && box.InFoot(62.5f, 101.0f) && !box.InFoot(64.0f, 100.0f) && !box.InFoot(60.0f, 104.0f));
    }
}
