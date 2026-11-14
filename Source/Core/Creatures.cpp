#include "Core/Creatures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Idler
{
    namespace
    {
        using enum UnlockKind;

        constexpr const char* kGameHint = "A certain game knows this one, given 30 minutes.";

        constexpr CreatureInfo kCreatures[] = {
            { CreatureId::Slime, "slime", "Slime", Start, 0, 0, {}, "", "Here from the start.", "Goes where the floor goes." },
            { CreatureId::Ghost, "ghost", "Ghost", Start, 0, 0, {}, "", "Here from the start.", "Haunts your backlog." },
            { CreatureId::Cat, "cat", "Cat", Start, 0, 0, {}, "", "Here from the start.", "Sits on whatever you are idling." },
            { CreatureId::Robot, "robot", "Robot", Start, 0, 0, {}, "", "Here from the start.", "Runs one loop. It is a good loop." },

            { CreatureId::Frog, "frog", "Frog", TotalHours, 10, 0, {}, "Idle 10 hours.", "Idled 10 hours.", "Has been waiting for a fly since 2014." },
            { CreatureId::Whale, "whale", "Whale", TotalHours, 42, 0, {}, "Idle 42 hours.", "Idled 42 hours.", "The answer was a whale all along." },
            { CreatureId::Bunny, "bunny", "Bunny", TotalHours, 100, 0, {}, "Idle 100 hours.", "Idled 100 hours.", "Its ears know something you do not." },
            { CreatureId::Mushroom, "mushroom", "Mushroom", TotalHours, 250, 0, {}, "Idle 250 hours.", "Idled 250 hours.", "Grows best when nobody looks." },
            { CreatureId::Bat, "bat", "Bat", TotalHours, 500, 0, {}, "Idle 500 hours.", "Idled 500 hours.", "Awake when you are not." },
            { CreatureId::Penguin, "penguin", "Penguin", TotalHours, 1000, 0, {}, "Idle 1000 hours.", "Idled 1000 hours.", "Dressed for the occasion. There is no occasion." },
            { CreatureId::LeetCat, "leetcat", "Leet Cat", TotalHours, 1337, 0, {}, "Idle 1337 hours.", "Idled 1337 hours.", "Too cool to play anything." },
            { CreatureId::Dragon, "dragon", "Dragon", TotalHours, 2500, 0, {}, "Idle 2500 hours.", "Idled 2500 hours.", "Hoards hours instead of gold." },
            { CreatureId::Ufo, "ufo", "Visitor", TotalHours, 5000, 0, {}, "Idle 5000 hours.", "Idled 5000 hours.", "Came to study your library. Still reading." },
            { CreatureId::SuperSlime, "superslime", "Super Slime", TotalHours, 9001, 0, {}, "Idle 9001 hours.", "Idled 9001 hours.", "Its hours are over 9000." },

            { CreatureId::Snail, "snail", "Snail", GamesIdled, 10, 0, {}, "Idle 10 different games, 30 minutes each.", "Idled 10 different games.", "Never late. Never early either." },
            { CreatureId::Turtle, "turtle", "Turtle", GamesIdled, 50, 0, {}, "Idle 50 different games, 30 minutes each.", "Idled 50 different games.", "Older than your oldest unplayed game." },
            { CreatureId::Octopus, "octopus", "Octopus", GamesIdled, 100, 0, {}, "Idle 100 different games, 30 minutes each.", "Idled 100 different games.", "One arm per genre." },
            { CreatureId::Jellyfish, "jellyfish", "Jellyfish", GamesIdled, 250, 0, {}, "Idle 250 different games, 30 minutes each.", "Idled 250 different games.", "Drifts through your library." },

            { CreatureId::Hourglass, "hourglass", "Hourglass", OneGameHours, 100, 0, {}, "Idle one game for 100 hours.", "Idled one game for 100 hours.", "Counts so you do not have to." },
            { CreatureId::Mimic, "mimic", "Mimic", OneGameHours, 1000, 0, {}, "Idle one game for 1000 hours.", "Idled one game for 1000 hours.", "That is not a loot box." },

            { CreatureId::Hamster, "hamster", "Hamster", DaysInARow, 7, 0, {}, "Idle 7 days in a row.", "Idled 7 days in a row.", "Its wheel is idling too." },
            { CreatureId::Phoenix, "phoenix", "Phoenix", DaysInARow, 30, 0, {}, "Idle 30 days in a row.", "Idled 30 days in a row.", "Rises with every new day." },

            { CreatureId::Centipede, "centipede", "Centipede", GamesAtOnce, 32, 0, {}, "Idle 32 games at once.", "Idled 32 games at once.", "One leg per game." },
            { CreatureId::Trophy, "trophy", "Trophy", GoalsReached, 1, 0, {}, "Let a game reach its goal.", "A game reached its goal.", "It says: participant." },
            { CreatureId::Sheep, "sheep", "Sheep", BreaksTaken, 1, 0, {}, "Let a game take a break.", "A game took a break.", "Counts itself to sleep." },
            { CreatureId::Owl, "owl", "Owl", NightHour, 3, 0, {}, "Idle deep in the night.", "Idled at 3 in the night.", "Who? You. Go to bed." },

            { CreatureId::Pumpkin, "pumpkin", "Pumpkin", Date, 1031, 1031, {}, "Idle on a spooky day.", "Idled on Halloween.", "Carved by someone who was idling." },
            { CreatureId::Snowman, "snowman", "Snowman", Date, 1224, 1226, {}, "Idle on a snowy holiday.", "Idled at Christmas.", "Melts at 30 frames per second." },
            { CreatureId::PartyBlob, "partyblob", "Party Blob", Date, 101, 101, {}, "Idle on the first day of a year.", "Idled on New Year's Day.", "This year's plan: keep idling." },
            { CreatureId::BlackCat, "blackcat", "Black Cat", FridayThe13th, 0, 0, {}, "Idle on an unlucky day.", "Idled on a Friday the 13th.", "Crossed your queue. Nothing happened." },
            { CreatureId::Fish, "fish", "Fish", Date, 401, 401, {}, "Idle on a day of jokes.", "Idled on April 1st.", "Not a joke. Look at the others that day." },
            { CreatureId::Cake, "cake", "Cake", Date, 912, 912, {}, "Idle on Steam's birthday.", "Idled on Steam's birthday.", "It is not a lie." },

            { CreatureId::Bug, "bug", "Bug", Award, 0, 0, {}, "Some codes never get old.", "Entered the code.", "Not a bug. A feature with legs." },
            { CreatureId::Manifold, "manifold", "Manifold", Award, 0, 0, {}, "Someone keeps clicking things.", "Clicked the logo a lot.", "It was the logo all along." },
            { CreatureId::HeartBuddy, "heartbuddy", "Heart Buddy", Pets, 100, 0, {}, "Be nice to your creatures.", "Petted 100 times.", "You are a good friend." },
            { CreatureId::Star, "star", "Star", Shiny, 0, 0, {}, "Some games shine.", "A shiny creature idled.", "One in 128. You looked." },

            { CreatureId::Spaceship, "spaceship", "Spaceship", Game, 0, 0, { 480 }, kGameHint, "Idled Spacewar.", "The oldest test flight on Steam." },
            { CreatureId::Cube, "cube", "Heart Cube", Game, 0, 0, { 400, 620 }, kGameHint, "Idled Portal.", "It will never threaten to stab you." },
            { CreatureId::Hat, "hat", "Fancy Hat", Game, 0, 0, { 440 }, kGameHint, "Idled Team Fortress 2.", "It is about the hats." },
            { CreatureId::Chicken, "chicken", "Chicken", Game, 0, 0, { 730 }, kGameHint, "Idled Counter-Strike 2.", "The only one on the map not camping." },
            { CreatureId::Crab, "crab", "Crab", Game, 0, 0, { 774801 }, kGameHint, "Idled Crab Champions.", "Champion of the sideways." },
            { CreatureId::KingSlime, "kingslime", "King Slime", Game, 0, 0, { 105600 }, kGameHint, "Idled Terraria.", "Heavy is the head that bounces." },
            { CreatureId::CryingKid, "cryingkid", "Crying Kid", Game, 0, 0, { 250900 }, kGameHint, "Idled The Binding of Isaac.", "It will be fine. Probably." },
            { CreatureId::Bean, "bean", "Bean", Game, 0, 0, { 945360 }, kGameHint, "Idled Among Us.", "Looks a little suspicious." },
            { CreatureId::NightCam, "nightcam", "Night Cam", Game, 0, 0, { 238320, 414700 }, kGameHint, "Idled Outlast.", "Battery at 3 percent." },
            { CreatureId::Zombie, "zombie", "Zombie", Game, 0, 0, { 418370, 2050650 }, kGameHint, "Idled Resident Evil.", "Welcome to the family." },
            { CreatureId::Wyvern, "wyvern", "Wyvern", Game, 0, 0, { 1446780, 582010 }, kGameHint, "Idled Monster Hunter.", "Carves its own tail, just in case." },
            { CreatureId::Dwarf, "dwarf", "Dwarf", Game, 0, 0, { 548430 }, kGameHint, "Idled Deep Rock Galactic.", "Rock and stone." },
            { CreatureId::Bonfire, "bonfire", "Bonfire", Game, 0, 0, { 374320, 1245620 }, kGameHint, "Idled a game of ash and fire.", "Rest here." },
            { CreatureId::ErrorSign, "error", "Error", Game, 0, 0, { 4000 }, kGameHint, "Idled Garry's Mod.", "You do not have this model." },
            { CreatureId::RocketCar, "rocketcar", "Rocket Car", Game, 0, 0, { 252950 }, kGameHint, "Idled Rocket League.", "What a save." },
            { CreatureId::Cheese, "cheese", "Bad Cheese", Game, 0, 0, { 3094480 }, kGameHint, "Idled Bad Cheese.", "It is watching you." },

            { CreatureId::Duck, "duck", "Duck", TotalHours, 3000, 0, {}, "Idle 3000 hours.", "Idled 3000 hours.", "Listens to your bugs. Does not judge." },
            { CreatureId::Pufferfish, "pufferfish", "Pufferfish", Pets, 1000, 0, {}, "Be very nice to your creatures.", "Petted 1000 times.", "It has asked you to stop." },
            { CreatureId::Otter, "otter", "Otter", GoalsReached, 50, 0, {}, "Reach 50 goals.", "Reached 50 goals.", "Keeps its favourite shell in a pocket it has." },
            { CreatureId::Shark, "shark", "Shark", GamesIdled, 500, 0, {}, "Idle 500 different games, 30 minutes each.", "Idled 500 different games.", "It ate your backlog. It is still hungry." },
            { CreatureId::Bee, "bee", "Bee", GamesAtOnce, 16, 0, {}, "Idle 16 games at once.", "Idled 16 games at once.", "Looks busy. That is most of the job." },
            { CreatureId::Firefly, "firefly", "Firefly", NightHour, 22, 0, {}, "Idle late in the evening.", "Idled at 10 in the evening.", "Leaves a light on for you." },
            { CreatureId::LoveLetter, "loveletter", "Love Letter", Date, 214, 214, {}, "Idle on a day of hearts.", "Idled on Valentine's Day.", "Dear backlog, I will get to you." },
            { CreatureId::Parrot, "parrot", "Parrot", Date, 919, 919, {}, "Idle on a day of pirates.", "Idled on Talk Like a Pirate Day.", "Knows one word. It is arr." },
            { CreatureId::Capybara, "capybara", "Capybara", TotalHours, 15000, 0, {}, "Idle 15000 hours.", "Idled 15000 hours.", "Gets along with everyone. Does nothing about it." },
            { CreatureId::Sloth, "sloth", "Sloth", TotalHours, 30000, 0, {}, "Idle 30000 hours.", "Idled 30000 hours.", "Will start any minute now." },
            { CreatureId::Cactus, "cactus", "Cactus", OneGameHours, 5000, 0, {}, "Idle one game for 5000 hours.", "Idled one game for 5000 hours.", "Needs nothing, like that one game you left running." },
            { CreatureId::CoffeeMug, "coffee", "Coffee Mug", BreaksTaken, 100, 0, {}, "Let games take 100 breaks.", "Games took 100 breaks.", "On its fifth cup of the break." },
            { CreatureId::Ghillie, "ghillie", "Ghillie Bush", Game, 0, 0, { 238090, 312660, 1029690, 2169200 }, kGameHint, "Idled Sniper Elite.", "You did not see this bush." },
            { CreatureId::Mole, "mole", "Tunnel Mole", Game, 0, 0, { 286690, 287390 }, kGameHint, "Idled Metro.", "Nobody told it the air is fine up here." },
            { CreatureId::ParkaKid, "parkakid", "Parka Kid", Game, 0, 0, { 213670, 488790 }, kGameHint, "Idled South Park.", "Says something. Nobody knows what." },
            { CreatureId::Demon, "demon", "Tiny Demon", Game, 0, 0, { 379720, 782330 }, kGameHint, "Idled DOOM.", "Ripped. Torn. Quite small." },
        };

        static_assert(std::size(kCreatures) == kCreatureCount);

        using enum CreatureId;

        constexpr CreatureId kSlimes[] = { Slime, SuperSlime, KingSlime };
        constexpr CreatureId kCats[] = { Cat, LeetCat, BlackCat };
        constexpr CreatureId kLake[] = { Whale, Octopus, Jellyfish, Fish, Duck, Pufferfish, Otter, Shark };
        constexpr CreatureId kNightShift[] = { Bat, Owl, Firefly, NightCam, Phoenix };
        constexpr CreatureId kFireAndScales[] = { Dragon, Wyvern, Bonfire, Demon };
        constexpr CreatureId kHolidays[] = { Pumpkin, Snowman, PartyBlob, LoveLetter, Cake, Parrot };
        constexpr CreatureId kMachines[] = { Robot, Ufo, Spaceship, RocketCar, ErrorSign, Manifold };
        constexpr CreatureId kMeadow[] = { Frog, Bunny, Sheep, Chicken, Hamster, Mushroom, Bee };
        constexpr CreatureId kSlowOnes[] = { Snail, Turtle, Sloth, Capybara, Hourglass };
        constexpr CreatureId kBumps[] = { Ghost, Zombie, Mimic, CryingKid, Bean };
        constexpr CreatureId kDownBelow[] = { Dwarf, Mole, Centipede, Bug, Crab };
        constexpr CreatureId kOddsAndEnds[] = { Trophy, Cube, Hat, CoffeeMug, Cheese, Cactus, Star };
        constexpr CreatureFamily kFamilies[] = {
            { "Slimes", kSlimes },
            { "Cats", kCats },
            { "The lake", kLake },
            { "Night shift", kNightShift },
            { "Fire and scales", kFireAndScales },
            { "Holidays", kHolidays },
            { "Machines", kMachines },
            { "The meadow", kMeadow },
            { "The slow ones", kSlowOnes },
            { "Things that go bump", kBumps },
            { "Down below", kDownBelow },
            { "Odds and ends", kOddsAndEnds },
        };

        struct RelationEntry
        {
            CreatureId From;
            CreatureId To;
            Relation What;
        };
        constexpr RelationEntry kRelations[] = {
            { Cat, Hamster, Relation::Chases }, { Cat, Bug, Relation::Chases }, { LeetCat, Hamster, Relation::Chases }, { BlackCat, Hamster, Relation::Chases },
            { Owl, Hamster, Relation::Chases }, { Chicken, Bug, Relation::Chases }, { Chicken, Centipede, Relation::Chases }, { Frog, Bug, Relation::Chases },
            { Mole, Bug, Relation::Chases }, { Dwarf, Bug, Relation::Chases }, { Dwarf, Demon, Relation::Chases }, { Dragon, Sheep, Relation::Chases },
            { Dragon, Chicken, Relation::Chases }, { Demon, Sheep, Relation::Chases }, { Demon, CryingKid, Relation::Chases }, { Zombie, Bunny, Relation::Chases },
            { Zombie, CryingKid, Relation::Chases }, { Zombie, ParkaKid, Relation::Chases }, { Zombie, NightCam, Relation::Chases }, { Zombie, Dwarf, Relation::Chases },
            { Hamster, Cheese, Relation::Chases }, { RocketCar, Cheese, Relation::Chases }, { RocketCar, Slime, Relation::Chases }, { Bean, Robot, Relation::Chases },
            { Shark, Fish, Relation::Chases }, { Shark, Duck, Relation::Chases }, { Shark, Pufferfish, Relation::Chases }, { Otter, Fish, Relation::Chases },
            { Bat, Firefly, Relation::Chases }, { Parrot, Firefly, Relation::Chases }, { Spaceship, Ufo, Relation::Chases }, { Wyvern, Bee, Relation::Chases },
            { Dwarf, Mimic, Relation::Friends }, { Turtle, Crab, Relation::Friends }, { Penguin, Snowman, Relation::Friends }, { ParkaKid, Snowman, Relation::Friends },
            { Bee, Cactus, Relation::Friends }, { Firefly, Bonfire, Relation::Friends }, { Hat, Cat, Relation::Friends }, { Robot, CoffeeMug, Relation::Friends },
            { Sheep, Sloth, Relation::Friends }, { HeartBuddy, LoveLetter, Relation::Friends }, { Ghost, Bat, Relation::Friends }, { CryingKid, Cake, Relation::Friends },
            { Bug, ErrorSign, Relation::Friends }, { Dwarf, Trophy, Relation::Friends }, { Cube, HeartBuddy, Relation::Friends },
        };

        struct CharacterEntry
        {
            CreatureId Id;
            PetManner Pet;
            ThrowManner Throw;
        };
        using P = PetManner;
        using T = ThrowManner;
        constexpr CharacterEntry kCharacters[] = {
            { Slime, P::Loves, T::Splats }, { Ghost, P::Shy, T::Slips }, { Cat, P::Moody, T::Hates }, { Robot, P::Unmoved, T::Dizzy },
            { Frog, P::Loves, T::Loves }, { Whale, P::Loves, T::Unmoved }, { Bunny, P::Shy, T::Flees }, { Mushroom, P::Unmoved, T::Dizzy },
            { Bat, P::Shy, T::Flees }, { Penguin, P::Loves, T::Loves }, { LeetCat, P::Moody, T::Unmoved }, { Dragon, P::Moody, T::Furious },
            { Ufo, P::Unmoved, T::Slips }, { SuperSlime, P::Loves, T::Splats },
            { Snail, P::Shy, T::Unmoved }, { Turtle, P::Loves, T::Unmoved }, { Octopus, P::Loves, T::Dizzy }, { Jellyfish, P::Touchy, T::Slips },
            { Hourglass, P::Unmoved, T::Dizzy }, { Mimic, P::Snaps, T::Hates },
            { Hamster, P::Loves, T::Dizzy }, { Phoenix, P::Loves, T::Hates },
            { Centipede, P::Shy, T::Dizzy }, { Trophy, P::Unmoved, T::Unmoved }, { Sheep, P::Loves, T::Flees }, { Owl, P::Unmoved, T::Hates },
            { Pumpkin, P::Unmoved, T::Splats }, { Snowman, P::Loves, T::Splats }, { PartyBlob, P::Loves, T::Loves }, { BlackCat, P::Moody, T::Flees },
            { Fish, P::Shy, T::Flees }, { Cake, P::Loves, T::Splats },
            { Bug, P::Shy, T::Flees }, { Manifold, P::Unmoved, T::Slips }, { HeartBuddy, P::Loves, T::Loves }, { Star, P::Loves, T::Dizzy },
            { Spaceship, P::Unmoved, T::Loves }, { Cube, P::Loves, T::Unmoved }, { Hat, P::Unmoved, T::Unmoved }, { Chicken, P::Touchy, T::Flees },
            { Crab, P::Moody, T::Loves }, { KingSlime, P::Unmoved, T::Splats }, { CryingKid, P::Shy, T::Flees }, { Bean, P::Shy, T::Flees },
            { NightCam, P::Shy, T::Dizzy }, { Zombie, P::Snaps, T::Furious }, { Wyvern, P::Moody, T::Furious }, { Dwarf, P::Loves, T::Loves },
            { Bonfire, P::Unmoved, T::Dizzy }, { ErrorSign, P::Unmoved, T::Dizzy }, { RocketCar, P::Unmoved, T::Loves }, { Cheese, P::Unmoved, T::Splats },
            { Duck, P::Loves, T::Hates }, { Pufferfish, P::Touchy, T::Flees }, { Otter, P::Loves, T::Loves }, { Shark, P::Snaps, T::Furious },
            { Bee, P::Touchy, T::Furious }, { Firefly, P::Shy, T::Slips }, { LoveLetter, P::Loves, T::Slips }, { Parrot, P::Moody, T::Loves },
            { Capybara, P::Unmoved, T::Unmoved }, { Sloth, P::Unmoved, T::Unmoved }, { Cactus, P::Snaps, T::Unmoved }, { CoffeeMug, P::Unmoved, T::Splats },
            { Ghillie, P::Shy, T::Unmoved }, { Mole, P::Shy, T::Flees }, { ParkaKid, P::Unmoved, T::Dizzy }, { Demon, P::Snaps, T::Furious },
        };
        static_assert(std::size(kCharacters) == kCreatureCount);
        constexpr bool CharactersAreInOrder()
        {
            for (std::size_t index = 0; index < std::size(kCharacters); ++index)
            {
                if (static_cast<std::size_t>(kCharacters[index].Id) != index)
                    return false;
            }
            return true;
        }
        static_assert(CharactersAreInOrder());

        constexpr int kShyUntilBond = 2;
        constexpr int kSnapsUntilBond = 3;
        constexpr int kMoodyUntilBond = 4;
        constexpr int kTouchyUntilBond = 4;
        constexpr int kMoodyPatience = 4;
        constexpr int kFleesUntilBond = 3;
        constexpr int kFuriousUntilBond = 4;

        constexpr double kBondLevelHours[kMaxBondLevel + 1] = { 0.0, 1.0, 10.0, 50.0, 250.0, 1000.0 };
        constexpr double kHoursPerPet = 0.1;

        bool Contains(const std::vector<std::uint32_t>& apps, std::uint32_t appId)
        {
            return appId != 0 && std::find(apps.begin(), apps.end(), appId) != apps.end();
        }

        double CurrentAmount(const CreatureInfo& info, const CreatureFacts& facts)
        {
            switch (info.Kind)
            {
            case TotalHours: return facts.TotalHours;
            case GamesIdled: return facts.GamesIdled;
            case OneGameHours: return facts.BestGameHours;
            case DaysInARow: return facts.DaysInARow;
            case GamesAtOnce: return static_cast<double>(facts.RunningApps.size());
            case GoalsReached: return facts.Goals;
            case BreaksTaken: return facts.Breaks;
            case Pets: return facts.Pets;
            default: return -1.0;
            }
        }

        const char* UnitOf(UnlockKind kind)
        {
            switch (kind)
            {
            case TotalHours:
            case OneGameHours: return "hours";
            case GamesIdled:
            case GamesAtOnce: return "games";
            case DaysInARow: return "days";
            case GoalsReached: return "goals";
            case BreaksTaken: return "breaks";
            case Pets: return "pets";
            default: return "";
            }
        }
    }

    Relation RelationOf(CreatureId from, CreatureId to)
    {
        if (from == to)
            return Relation::None;
        for (const RelationEntry& entry : kRelations)
        {
            if (entry.From == from && entry.To == to)
                return entry.What;
            if (entry.From == to && entry.To == from)
                return entry.What == Relation::Friends ? Relation::Friends : Relation::None;
        }
        if (from == Capybara || to == Capybara || from == HeartBuddy || to == HeartBuddy)
            return Relation::Friends;
        for (const CreatureFamily& family : kFamilies)
        {
            if (IsInFamily(family, from) && IsInFamily(family, to))
                return Relation::Friends;
        }
        return Relation::None;
    }

    CreatureCharacter CharacterOf(CreatureId id)
    {
        const CharacterEntry& entry = kCharacters[static_cast<std::size_t>(id)];
        return { entry.Pet, entry.Throw };
    }

    PetOutcome OutcomeOfPet(CreatureId id, int bond, int streak)
    {
        switch (CharacterOf(id).Pet)
        {
        case PetManner::Shy:
            return bond >= kShyUntilBond ? PetOutcome::Joy : PetOutcome::Dodge;
        case PetManner::Moody:
            return bond < kMoodyUntilBond && streak > 0 && streak % kMoodyPatience == 0 ? PetOutcome::Bite : PetOutcome::Joy;
        case PetManner::Snaps:
            return bond >= kSnapsUntilBond ? PetOutcome::Joy : PetOutcome::Bite;
        case PetManner::Touchy:
            return bond >= kTouchyUntilBond ? PetOutcome::Joy : PetOutcome::Puff;
        case PetManner::Unmoved:
            return PetOutcome::Shrug;
        default:
            return PetOutcome::Joy;
        }
    }

    ThrowManner ThrowMannerOf(CreatureId id, int bond)
    {
        const ThrowManner manner = CharacterOf(id).Throw;
        if (manner == ThrowManner::Flees && bond >= kFleesUntilBond)
            return ThrowManner::Unmoved;
        if (manner == ThrowManner::Furious && bond >= kFuriousUntilBond)
            return ThrowManner::Hates;
        return manner;
    }

    std::span<const CreatureFamily> AllFamilies()
    {
        return kFamilies;
    }

    int FoundInFamily(const CreatureFamily& family, const CreatureSet& found)
    {
        int count = 0;
        for (const CreatureId id : family.Members)
            count += found.test(static_cast<std::size_t>(id)) ? 1 : 0;
        return count;
    }

    bool IsInFamily(const CreatureFamily& family, CreatureId id)
    {
        return std::find(family.Members.begin(), family.Members.end(), id) != family.Members.end();
    }

    double BondHours(double idledSeconds, std::uint32_t pets, std::uint32_t playMinutes)
    {
        return idledSeconds / 3600.0 + static_cast<double>(pets) * kHoursPerPet + static_cast<double>(playMinutes) / 60.0;
    }

    int BondLevel(double hours)
    {
        int level = 0;
        while (level < kMaxBondLevel && hours >= kBondLevelHours[level + 1])
            ++level;
        return level;
    }

    double BondHoursOfLevel(int level)
    {
        return kBondLevelHours[std::clamp(level, 0, kMaxBondLevel)];
    }

    const CreatureEvolution* EvolutionOf(CreatureId id)
    {
        for (const CreatureEvolution& evolution : kEvolutions)
        {
            if (evolution.Id == id)
                return &evolution;
        }
        return nullptr;
    }

    bool EvolvesAt(CreatureId id, int bond)
    {
        return bond >= kEvolveAtBond && EvolutionOf(id) != nullptr;
    }

    const char* BondName(int level)
    {
        static constexpr const char* kNames[kMaxBondLevel + 1] = { "Just met", "Familiar", "Friend", "Close friend", "Best friend", "Inseparable" };
        return kNames[std::clamp(level, 0, kMaxBondLevel)];
    }

    float BondShare(double hours)
    {
        const int level = BondLevel(hours);
        if (level >= kMaxBondLevel)
            return 1.0f;
        const double low = kBondLevelHours[level];
        return static_cast<float>((hours - low) / (kBondLevelHours[level + 1] - low));
    }

    std::span<const CreatureInfo> AllCreatures()
    {
        return kCreatures;
    }

    const CreatureInfo& InfoOf(CreatureId id)
    {
        return kCreatures[static_cast<std::size_t>(id)];
    }

    std::optional<CreatureId> CreatureWithKey(std::string_view key)
    {
        for (const CreatureInfo& info : kCreatures)
        {
            if (key == info.Key)
                return info.Id;
        }
        return std::nullopt;
    }

    void NoteGameTotal(CreatureFacts& facts, std::uint32_t appId, double seconds)
    {
        if (seconds < kGameCountsAfterSeconds)
            return;
        ++facts.GamesIdled;
        facts.IdledApps.push_back(appId);
    }

    bool IsEarned(const CreatureInfo& info, const CreatureFacts& facts)
    {
        const bool running = !facts.RunningApps.empty();
        switch (info.Kind)
        {
        case Start:
            return true;
        case NightHour:
            return running && facts.Hour == static_cast<int>(info.Amount);
        case Date:
        {
            const int today = facts.Month * 100 + facts.Day;
            return running && today >= static_cast<int>(info.Amount) && today <= static_cast<int>(info.Until);
        }
        case FridayThe13th:
            return running && facts.Day == 13 && facts.Weekday == 5;
        case Game:
            return std::any_of(std::begin(info.Games), std::end(info.Games), [&](std::uint32_t appId) { return Contains(facts.IdledApps, appId); });
        case Shiny:
            return std::any_of(facts.RunningApps.begin(), facts.RunningApps.end(), [](std::uint32_t appId) { return IsShiny(appId); });
        case Award:
            return false;
        default:
            return CurrentAmount(info, facts) >= info.Amount;
        }
    }

    float ProgressShare(const CreatureInfo& info, const CreatureFacts& facts)
    {
        const double current = CurrentAmount(info, facts);
        if (current < 0.0)
            return -1.0f;
        return static_cast<float>(std::clamp(current / info.Amount, 0.0, 1.0));
    }

    std::string ProgressText(const CreatureInfo& info, const CreatureFacts& facts)
    {
        const double current = CurrentAmount(info, facts);
        if (current < 0.0)
            return {};
        char text[64];
        std::snprintf(text, sizeof(text), "%.0f of %.0f %s", std::floor(std::min(current, info.Amount)), info.Amount, UnitOf(info.Kind));
        return text;
    }

    std::vector<CreatureId> NextFinds(const CreatureSet& found, const CreatureFacts& facts, int count)
    {
        std::vector<CreatureId> finds;
        for (const CreatureInfo& info : kCreatures)
        {
            if (!found.test(static_cast<std::size_t>(info.Id)) && ProgressShare(info, facts) >= 0.0f)
                finds.push_back(info.Id);
        }
        std::stable_sort(finds.begin(), finds.end(), [&](CreatureId lhs, CreatureId rhs) { return ProgressShare(InfoOf(lhs), facts) > ProgressShare(InfoOf(rhs), facts); });
        if (static_cast<int>(finds.size()) > count)
            finds.resize(static_cast<std::size_t>(count));
        return finds;
    }

    int CountDaysInARow(const std::vector<double>& secondsPerDay)
    {
        auto day = secondsPerDay.rbegin();
        if (day != secondsPerDay.rend() && *day <= 0.0)
            ++day;
        int count = 0;
        for (; day != secondsPerDay.rend() && *day > 0.0; ++day)
            ++count;
        return count;
    }

    std::uint32_t MixSeed(std::uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        value *= 0x846CA68Bu;
        value ^= value >> 16;
        return value;
    }

    Rarity RarityOf(CreatureId id)
    {
        switch (id)
        {
        case CreatureId::Slime: case CreatureId::Ghost: case CreatureId::Cat: case CreatureId::Robot:
        case CreatureId::Frog: case CreatureId::Whale: case CreatureId::Bunny: case CreatureId::Snail:
        case CreatureId::Trophy: case CreatureId::Sheep:
            return Rarity::Common;
        case CreatureId::Pumpkin: case CreatureId::Snowman: case CreatureId::PartyBlob: case CreatureId::Fish:
        case CreatureId::Cake: case CreatureId::LoveLetter: case CreatureId::Parrot:
        case CreatureId::Penguin: case CreatureId::LeetCat: case CreatureId::Octopus: case CreatureId::Bee:
        case CreatureId::CoffeeMug: case CreatureId::Otter:
            return Rarity::Rare;
        case CreatureId::Dragon: case CreatureId::Duck: case CreatureId::Ufo: case CreatureId::Jellyfish:
        case CreatureId::Mimic: case CreatureId::Phoenix: case CreatureId::Centipede: case CreatureId::BlackCat:
        case CreatureId::Pufferfish:
            return Rarity::Epic;
        case CreatureId::SuperSlime: case CreatureId::Capybara: case CreatureId::Sloth: case CreatureId::Shark:
        case CreatureId::Cactus: case CreatureId::Star: case CreatureId::Bug: case CreatureId::Manifold:
            return Rarity::Legendary;
        default:
            return Rarity::Uncommon;
        }
    }

    const char* RarityName(Rarity rarity)
    {
        switch (rarity)
        {
        case Rarity::Common: return "Common";
        case Rarity::Rare: return "Rare";
        case Rarity::Epic: return "Epic";
        case Rarity::Legendary: return "Legendary";
        default: return "Uncommon";
        }
    }

    int RarityWeight(Rarity rarity)
    {
        switch (rarity)
        {
        case Rarity::Common: return 8;
        case Rarity::Rare: return 3;
        case Rarity::Epic: return 2;
        case Rarity::Legendary: return 1;
        default: return 5;
        }
    }

    std::optional<CreatureId> CreatureOfGame(std::uint32_t appId)
    {
        if (appId == 0)
            return std::nullopt;
        for (const CreatureInfo& info : kCreatures)
        {
            if (info.Kind == Game && std::find(std::begin(info.Games), std::end(info.Games), appId) != std::end(info.Games))
                return info.Id;
        }
        return std::nullopt;
    }

    CreatureId PickCreature(std::uint32_t seed, const CreatureSet& found)
    {
        if (const auto own = CreatureOfGame(seed); own && found.test(static_cast<std::size_t>(*own)))
            return *own;

        CreatureId best = CreatureId::Slime;
        double bestScore = 0.0;
        for (const CreatureInfo& info : kCreatures)
        {
            const auto index = static_cast<std::uint32_t>(info.Id);
            if (info.Kind == Game || !found.test(index))
                continue;
            const double drawn = (static_cast<double>(MixSeed(seed ^ MixSeed(index + 1u))) + 0.5) / 4294967296.0;
            const double score = -static_cast<double>(RarityWeight(RarityOf(info.Id))) / std::log(drawn);
            if (score >= bestScore)
            {
                bestScore = score;
                best = info.Id;
            }
        }
        return best;
    }

    bool IsShiny(std::uint32_t seed)
    {
        return MixSeed(seed ^ 0x51E1D00Du) % 128u == 0u;
    }
}
