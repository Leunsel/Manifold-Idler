#pragma once

#include "Core/MiniGames.h"
#include "Core/Queue.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Idler
{
    struct StoredEntry
    {
        std::uint32_t Account = 0;
        std::string List = kDefaultListName;
        QueueEntry Entry;
    };

    struct StoredShown
    {
        std::uint32_t Account = 0;
        std::uint32_t AppId = 0;
    };

    struct StoredList
    {
        std::uint32_t Account = 0;
        std::string Name;
    };

    struct StoredCreature
    {
        std::string Key;
        std::uint32_t Found = 0;
        std::uint32_t Pets = 0;
        bool Evolved = false;
        bool FirstForm = false;
    };

    struct StoredPlay
    {
        std::string Key;
        PlayRecord Record;
    };

    enum class SceneSeason
    {
        ByDate,
        Spring,
        Summer,
        Autumn,
        Winter,
        Count
    };
    const char* SceneSeasonName(SceneSeason season);
    SceneSeason SeasonIn(SceneSeason choice, int month);

    struct Settings
    {
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;
        bool Maximised = false;
        bool Placed = false;

        int Slots = 1;
        Goal DefaultGoal;
        bool PauseWhilePlaying = true;
        bool KeepInTray = true;
        bool ResumeOnStart = false;
        bool WasActive = false;

        bool KeepAwake = true;
        bool RefundGuard = true;
        bool OnlyWhenAway = false;
        float AwayMinutes = 5.0f;
        bool OnlyInHours = false;
        int FromHour = 22;
        int ToHour = 8;

        bool VaryTimes = false;
        float VarySpread = 25.0f;
        float CardWarmupHours = 3.0f;
        std::vector<StoredShown> ShownGames;

        bool InvisibleWhileIdling = false;
        bool SetInvisible = false;

        std::vector<StoredList> Lists;
        std::vector<StoredList> ActiveLists;
        std::vector<StoredEntry> Queue;

        bool ShowTerrarium = true;
        SceneSeason Season = SceneSeason::ByDate;
        bool ShowLog = false;
        bool LibraryCovers = false;
        bool QueueCovers = false;
        bool CreatureHome = false;
        bool HomeBall = false;
        bool CreaturesTugMouse = true;
        std::vector<StoredCreature> Creatures;
        std::string CreatureBuddy;
        std::uint32_t CreatureGoals = 0;
        std::uint32_t CreatureBreaks = 0;
        std::uint32_t CreaturePets = 0;
        std::vector<StoredPlay> CreaturePlays;

        void Load(const std::filesystem::path& path);
        bool Save(const std::filesystem::path& path) const;
    };
    int TakeOverOldName(const std::filesystem::path& settings);
}
