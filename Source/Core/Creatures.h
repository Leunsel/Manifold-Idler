#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Idler
{
    enum class CreatureId : int
    {
        Slime, Ghost, Cat, Robot,
        Frog, Whale, Bunny, Mushroom, Bat, Penguin, LeetCat, Dragon, Ufo, SuperSlime,
        Snail, Turtle, Octopus, Jellyfish,
        Hourglass, Mimic,
        Hamster, Phoenix,
        Centipede, Trophy, Sheep, Owl,
        Pumpkin, Snowman, PartyBlob, BlackCat, Fish, Cake,
        Bug, Manifold, HeartBuddy, Star,
        Spaceship, Cube, Hat, Chicken, Crab, KingSlime, CryingKid, Bean, NightCam, Zombie, Wyvern, Dwarf, Bonfire, ErrorSign, RocketCar, Cheese,
        Duck, Pufferfish, Otter, Shark, Bee, Firefly, LoveLetter, Parrot, Capybara, Sloth, Cactus, CoffeeMug, Ghillie, Mole, ParkaKid, Demon,
        Count
    };

    inline constexpr std::size_t kCreatureCount = static_cast<std::size_t>(CreatureId::Count);
    using CreatureSet = std::bitset<kCreatureCount>;

    enum class Rarity
    {
        Common,
        Uncommon,
        Rare,
        Epic,
        Legendary,
        Count
    };
    Rarity RarityOf(CreatureId id);
    const char* RarityName(Rarity rarity);
    int RarityWeight(Rarity rarity);

    enum class UnlockKind
    {
        Start,
        TotalHours,
        GamesIdled,
        OneGameHours,
        DaysInARow,
        GamesAtOnce,
        GoalsReached,
        BreaksTaken,
        NightHour,
        Date,
        FridayThe13th,
        Game,
        Pets,
        Shiny,
        Award
    };

    struct CreatureInfo
    {
        CreatureId Id;
        const char* Key;
        const char* Name;
        UnlockKind Kind;
        double Amount;
        double Until;
        std::uint32_t Games[4];
        const char* Hint;
        const char* How;
        const char* Flavour;
    };

    std::span<const CreatureInfo> AllCreatures();
    const CreatureInfo& InfoOf(CreatureId id);
    std::optional<CreatureId> CreatureWithKey(std::string_view key);

    struct CreatureFacts
    {
        double TotalHours = 0.0;
        double BestGameHours = 0.0;
        int GamesIdled = 0;
        int DaysInARow = 0;
        std::vector<std::uint32_t> IdledApps;
        std::vector<std::uint32_t> RunningApps;
        int Hour = -1;
        int Month = 0;
        int Day = 0;
        int Weekday = -1;
        std::uint32_t Goals = 0;
        std::uint32_t Breaks = 0;
        std::uint32_t Pets = 0;
    };

    inline constexpr double kGameCountsAfterSeconds = 30.0 * 60.0;
    void NoteGameTotal(CreatureFacts& facts, std::uint32_t appId, double seconds);

    bool IsEarned(const CreatureInfo& info, const CreatureFacts& facts);
    float ProgressShare(const CreatureInfo& info, const CreatureFacts& facts);
    std::string ProgressText(const CreatureInfo& info, const CreatureFacts& facts);

    std::vector<CreatureId> NextFinds(const CreatureSet& found, const CreatureFacts& facts, int count);

    int CountDaysInARow(const std::vector<double>& secondsPerDay);

    enum class Relation
    {
        None,
        Friends,
        Chases
    };
    Relation RelationOf(CreatureId from, CreatureId to);

    struct CreatureFamily
    {
        const char* Name;
        std::span<const CreatureId> Members;
    };
    std::span<const CreatureFamily> AllFamilies();
    int FoundInFamily(const CreatureFamily& family, const CreatureSet& found);
    bool IsInFamily(const CreatureFamily& family, CreatureId id);

    inline constexpr int kMaxBondLevel = 5;
    double BondHours(double idledSeconds, std::uint32_t pets, std::uint32_t playMinutes = 0);
    int BondLevel(double hours);
    double BondHoursOfLevel(int level);
    const char* BondName(int level);
    float BondShare(double hours);

    inline constexpr int kEvolveAtBond = 3;
    struct CreatureEvolution
    {
        CreatureId Id;
        const char* Name;
        const char* Flavour;
    };
    inline constexpr CreatureEvolution kEvolutions[] = {
        { CreatureId::Slime, "Bloom Slime", "Grew a flower and will not say how." },
        { CreatureId::Ghost, "Lantern Ghost", "Brings a light to your backlog." },
        { CreatureId::Cat, "Twintail Cat", "Sits on two games at once." },
        { CreatureId::Robot, "Robot Mk II", "Runs two loops. Both are good loops." },
        { CreatureId::Frog, "Prince Frog", "Has been waiting for a kiss since 2014." },
        { CreatureId::Whale, "Star Whale", "The answer was in the stars all along." },
        { CreatureId::Bunny, "Antler Bunny", "Grew antlers to know even more." },
        { CreatureId::Mushroom, "Glow Mushroom", "Glows best when nobody looks." },
        { CreatureId::Bat, "Vampire Bat", "Awake when you are not, and dressed for it." },
        { CreatureId::Penguin, "Crested Penguin", "Did its hair. There is still no occasion." },
        { CreatureId::LeetCat, "Leet Cat Pro", "Too cool to play anything. The cap stays on." },
        { CreatureId::Dragon, "Gold Dragon", "Hoarded so many hours they turned to gold." },
        { CreatureId::Ufo, "Landed Visitor", "Came to study your library. Decided to stay." },
        { CreatureId::SuperSlime, "Ultra Super Slime", "Its hair is over 9000 too." },
        { CreatureId::Snail, "Turbo Snail", "Never late, and now in a hurry about it." },
        { CreatureId::Turtle, "Island Turtle", "Older than your oldest unplayed game. Grew a palm tree." },
        { CreatureId::Octopus, "Dapper Octopus", "One tentacle per genre, one hat for all of them." },
        { CreatureId::Jellyfish, "Crowned Jellyfish", "Drifts through your library like it owns it." },
        { CreatureId::Hourglass, "Golden Hourglass", "Time is gold, it turns out." },
        { CreatureId::Mimic, "Treasure Mimic", "That is not a loot box. Those are not jewels." },
        { CreatureId::Hamster, "Golden Hamster", "Its wheel is gold now. It idles anyway." },
        { CreatureId::Phoenix, "Sun Phoenix", "Rises with every new day, and brings the sun." },
        { CreatureId::Centipede, "Spiny Centipede", "One leg per game, one spine per achievement." },
        { CreatureId::Trophy, "Champion Trophy", "It says: winner. Finally." },
        { CreatureId::Sheep, "Dream Sheep", "Counts itself to sleep. Dressed for it." },
        { CreatureId::Owl, "Wizard Owl", "Who? You. Go to bed. That is an order." },
        { CreatureId::Pumpkin, "Haunted Pumpkin", "Carved by someone who was idling. Lit by someone who was not." },
        { CreatureId::Snowman, "Frost Snowman", "Runs at 60 frames per second and does not melt." },
        { CreatureId::PartyBlob, "Party Blob Deluxe", "This year's plan: keep idling, in style." },
        { CreatureId::BlackCat, "Witch's Black Cat", "Crossed your queue with a spell. Still nothing happened." },
        { CreatureId::Fish, "Clown Fish", "Still not a joke. Mostly." },
        { CreatureId::Cake, "Birthday Cake", "It is not a lie, and it is your birthday somewhere." },
        { CreatureId::Bug, "Horned Bug", "Not a bug. A feature with a horn." },
        { CreatureId::Manifold, "Manifold Prime", "It was the logo all along. Now it glows." },
        { CreatureId::HeartBuddy, "Angel Heart Buddy", "You are a good friend. It checked." },
        { CreatureId::Star, "Shooting Star", "One in 128. Make a wish." },
        { CreatureId::Spaceship, "Spaceship Mk II", "The oldest test flight on Steam, still testing." },
        { CreatureId::Cube, "Glowing Heart Cube", "It will never threaten to stab you. It glows about it." },
        { CreatureId::Hat, "Unusual Fancy Hat", "It is about the hats. This one is on fire." },
        { CreatureId::Chicken, "Commando Chicken", "The only one on the map not camping. It is on the attack." },
        { CreatureId::Crab, "Champion Crab", "Champion of the sideways, with the belt to prove it." },
        { CreatureId::KingSlime, "High King Slime", "Heavier is the head that bounces higher." },
        { CreatureId::CryingKid, "Happy Crying Kid", "Still crying. Happy tears this time." },
        { CreatureId::Bean, "Suspicious Bean", "Looks very suspicious now." },
        { CreatureId::NightCam, "Night Cam Pro", "Brought its own lamp. The dark is no excuse any more." },
        { CreatureId::Zombie, "Romantic Zombie", "Welcome to the family. It brought flowers." },
        { CreatureId::Wyvern, "Elder Wyvern", "Carves its own tail, just in case. Grew horns, also just in case." },
        { CreatureId::Dwarf, "Dwarf Veteran", "Rock and stone, and a golden pick to prove it." },
        { CreatureId::Bonfire, "Kindled Bonfire", "Rest here. It is warmer now." },
        { CreatureId::ErrorSign, "Fatal Error", "You do not have this texture either." },
        { CreatureId::RocketCar, "Supersonic Rocket Car", "What a save. At supersonic." },
        { CreatureId::Cheese, "Very Bad Cheese", "It is watching you, and it smells." },
        { CreatureId::Duck, "Debug Duck", "Listens to your bugs, and now it can read them." },
        { CreatureId::Pufferfish, "Grumpy Pufferfish", "It asked you to stop. It meant it." },
        { CreatureId::Otter, "Pearl Otter", "Swapped its stone for a clam with a pearl. Will not let go of it." },
        { CreatureId::Shark, "Laser Shark", "It ate your backlog. Now it has a laser." },
        { CreatureId::Bee, "Queen Bee", "Looks busy. Is the boss of the busy ones." },
        { CreatureId::Firefly, "Starlight Firefly", "Leaves all the lights on for you." },
        { CreatureId::LoveLetter, "Scented Love Letter", "Dear backlog, I will get to you. This time I mean it." },
        { CreatureId::Parrot, "Captain Parrot", "Knows two words now. Both are arr." },
        { CreatureId::Capybara, "Friendly Capybara", "Gets along with everyone. Someone moved in." },
        { CreatureId::Sloth, "Mossy Sloth", "Will start any minute now. Moss grew in the meantime." },
        { CreatureId::Cactus, "Blooming Cactus", "Needs nothing. Flowers anyway." },
        { CreatureId::CoffeeMug, "Fancy Coffee Mug", "On its fifth cup of the break. This one has cream." },
        { CreatureId::Ghillie, "Flowery Ghillie Bush", "You definitely see this bush." },
        { CreatureId::Mole, "Ranger Tunnel Mole", "Nobody told it the air is fine up here. It brought a lamp anyway." },
        { CreatureId::ParkaKid, "Masked Parka Kid", "Says something heroic. Nobody knows what." },
        { CreatureId::Demon, "Tiny Demon Lord", "Ripped. Torn. Still quite small." },
    };
    const CreatureEvolution* EvolutionOf(CreatureId id);
    bool EvolvesAt(CreatureId id, int bond);

    enum class PetManner
    {
        Loves,
        Shy,
        Moody,
        Snaps,
        Touchy,
        Unmoved
    };
    enum class ThrowManner
    {
        Hates,
        Loves,
        Flees,
        Dizzy,
        Unmoved,
        Slips,
        Splats,
        Furious
    };
    struct CreatureCharacter
    {
        PetManner Pet;
        ThrowManner Throw;
    };
    CreatureCharacter CharacterOf(CreatureId id);

    enum class PetOutcome
    {
        Joy,
        Shrug,
        Dodge,
        Bite,
        Puff
    };
    PetOutcome OutcomeOfPet(CreatureId id, int bond, int streak);
    ThrowManner ThrowMannerOf(CreatureId id, int bond);

    std::uint32_t MixSeed(std::uint32_t value);

    std::optional<CreatureId> CreatureOfGame(std::uint32_t appId);
    CreatureId PickCreature(std::uint32_t seed, const CreatureSet& found);
    bool IsShiny(std::uint32_t seed);
}
