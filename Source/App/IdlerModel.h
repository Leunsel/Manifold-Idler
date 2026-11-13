#pragma once

#include "App/CardService.h"
#include "Core/Clock.h"
#include "Core/Creatures.h"
#include "Core/Library.h"
#include "Core/Queue.h"
#include "Core/Settings.h"
#include "Core/Backups.h"
#include "Core/Stats.h"
#include "Steam/LibraryScan.h"
#include "Steam/SteamLocal.h"
#include "Steam/WorkerProcess.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Idler
{
    enum class RunState
    {
        Off,
        Starting,
        Running,
        Stopping
    };

    enum class Hold
    {
        None,
        Steam,
        Playing,
        Hours,
        Away
    };

    struct WindowMessage
    {
        enum class Kind
        {
            GoalReached,
            QueueFinished,
            LastRunCrashed
        };
        Kind What = Kind::GoalReached;
        std::uint32_t AppId = 0;
    };

    struct UndoRecord
    {
        std::string List;
        int ListPlace = -1;
        std::vector<RemovedEntry> Entries;
        std::vector<std::uint32_t> Order;
        QueueOrder SortedBy = QueueOrder::Name;
        bool EndedIdling = false;

        bool Any() const { return ListPlace >= 0 || !Entries.empty() || !Order.empty(); }
    };

    struct QueueList
    {
        std::string Name;
        std::vector<QueueEntry> Entries;
    };

    enum class AchievementAccess
    {
        Unknown,
        Here,
        Server
    };

    struct AchievementRow
    {
        std::string Id;
        std::string Name;
        std::string Description;
        bool Achieved = false;
        bool Hidden = false;
        std::uint32_t UnlockTime = 0;
        std::string Icon;
        AchievementAccess Access = AchievementAccess::Unknown;
        std::string ProgressStat;
        double ProgressMin = 0.0;
        double ProgressMax = 0.0;
    };

    struct StatRow
    {
        StatSchema Schema;
        double Value = 0.0;
        bool Settable = false;
    };

    enum class AchievementPhase
    {
        Idle,
        Busy,
        Ready,
        Writing,
        Failed
    };

    class IdlerModel
    {
    public:
        IdlerModel(Settings& settings, std::filesystem::path settingsPath);
        ~IdlerModel();

        IdlerModel(const IdlerModel&) = delete;
        IdlerModel& operator=(const IdlerModel&) = delete;

        void Update();
        double Now() const;
        double ClockPace() const { return m_pace.Pace(); }
        void Finish();

        const Steam::Status& SteamStatus() const { return m_steam; }
        const std::string& Persona() const { return m_persona; }
        std::uint32_t Account() const { return m_loadedAccount; }
        std::uint64_t SteamId() const;

        const std::vector<Game>& Games() const { return m_games; }
        const Game* FindGame(std::uint32_t appId) const;
        std::uint64_t LibraryGeneration() const { return m_libraryGeneration; }
        bool Scanning() const { return m_scanning; }
        bool ScannedOnce() const { return m_scannedOnce; }
        const std::string& ScanProblem() const { return m_scanProblem; }
        void Rescan();
        unsigned TakeFontNeeds();

        const CreatureSet& FoundCreatures() const { return m_creatures; }
        CreatureFacts CreatureFactsNow() const;
        std::uint32_t CreatureFoundAt(CreatureId id) const;
        void AwardCreature(CreatureId id);
        void PetCreature(CreatureId id);
        const PlayRecord& PlayOf(CreatureId id) const;
        std::uint32_t PlayBondLeftToday(CreatureId id) const;
        struct PlayResult
        {
            std::uint32_t Bond = 0;
            bool NewBest = false;
            bool DayFull = false;
        };
        PlayResult FinishGame(CreatureId id, MiniGame game, int score);
        void FetchedBall(CreatureId id);
        std::uint32_t CreaturePets() const { return m_settings.CreaturePets; }
        double CreatureBondHours(CreatureId id) const { return m_bondHours[static_cast<std::size_t>(id)]; }
        std::vector<CreatureId> TakeBondNews();
        bool CreatureEvolved(CreatureId id) const;
        bool CreatureInFirstForm(CreatureId id) const;
        void SetCreatureFirstForm(CreatureId id, bool first);
        CreatureSet EvolvedCreatures() const;
        std::uint32_t CreatureEvolutions(CreatureId id) const { return m_evolutions[static_cast<std::size_t>(id)]; }
        std::vector<CreatureId> TakeEvolutionNews();
        std::vector<const CreatureFamily*> TakeFinishedFamilies();
        std::optional<CreatureId> Buddy() const { return m_buddy; }
        void SetBuddy(std::optional<CreatureId> buddy);
        bool IsNewCreature(CreatureId id) const { return m_newCreatures.test(static_cast<std::size_t>(id)); }
        bool HasNewCreatures() const { return m_newCreatures.any(); }
        void MarkCreatureSeen(CreatureId id) { m_newCreatures.reset(static_cast<std::size_t>(id)); }
        std::vector<CreatureId> TakeFreshCreatures();
        std::string TakeCreatureNews();
        bool IsUpsideDownDay() const { return m_month == 4 && m_day == 1; }
        SceneSeason CurrentSeason() const { return SeasonIn(m_settings.Season, m_month != 0 ? m_month : CurrentMonth()); }
        void SetPretendDate(int month, int day);
        void SetAllCreaturesFound();

        const std::vector<QueueList>& Lists() const { return m_lists; }
        int ActiveList() const { return m_activeList; }
        bool IsListHidden(int index) const;
        int VisibleListCount() const;
        void SelectList(int index);
        void CreateList(std::string_view name);
        void RenameList(int index, std::string_view name);
        void DeleteList(int index);
        void AddToList(int index, std::uint32_t appId);
        void AddGamesToList(int index, const std::vector<std::uint32_t>& apps);

        const std::vector<QueueEntry>& Queue() const { return m_lists[static_cast<std::size_t>(m_activeList)].Entries; }
        const QueueEntry* FindEntry(std::uint32_t appId) const;
        void AddToQueue(std::uint32_t appId);
        void AddAllToQueue(const std::vector<int>& indices);
        void IdleNow(std::uint32_t appId);
        void Remove(std::uint32_t appId);
        void RemoveFinished();
        void RemoveAll();
        void SortQueue(QueueOrder order);
        const UndoRecord& Undoable() const { return m_undo; }
        std::uint64_t UndoGeneration() const { return m_undoGeneration; }
        void Undo();
        void ForgetUndo() { m_undo = {}; }
        void MoveEntry(int from, int to);
        void SetGoal(std::uint32_t appId, Goal goal);
        void Retry(std::uint32_t appId);
        void AllowPastRefund(std::uint32_t appId);

        std::uint32_t ShownApp() const { return m_shownApp; }
        void SetShownApp(std::uint32_t appId);

        CardService& Cards() { return m_cards; }
        const CardService& Cards() const { return m_cards; }
        bool CardLoginIsOtherAccount() const;
        void FarmCards();
        bool QueueFarmsCards() const;
        int CardsDropped() const { return m_cardsDropped; }
        bool TakeCardDropped();

        bool Active() const { return m_active; }
        QueuePhase Phase() const;
        int WantedCount() const { return m_wantedCount; }
        int ReadyCount() const { return m_readyCount; }
        int AliveCount() const { return m_pretendRunning ? m_pretendAlive : static_cast<int>(m_sessions.size()); }
        void SetActive(bool active);
        int Slots() const { return m_settings.Slots; }
        void SetSlots(int slots);
        RunState StateOf(std::uint32_t appId) const;
        int RunningCount() const;
        bool IdlingOrOnBreak() const;
        Hold CurrentHold() const;
        std::uint32_t PlayingApp() const { return m_playingApp; }
        double SessionSeconds() const { return m_sessionSeconds; }
        double RestSecondsLeft(std::uint32_t appId) const;
        QueueForecast Forecast() const;
        bool TakeQueueFinished();
        std::vector<WindowMessage> TakeWindowMessages();
        void SetPretendMessages(bool pretend) { m_pretendMessages = pretend; }

        bool ShutdownArmed() const { return m_shutdownArmed; }
        void SetShutdownArmed(bool armed) { m_shutdownArmed = armed; }
        double ShutdownSecondsLeft() const;
        void CancelShutdown();
        bool ShutdownFailed() const { return m_shutdownFailed; }
        bool TakeWantsWindow();

        const IdleStats& Stats() const { return m_stats; }

        void LoadAchievements(std::uint32_t appId);
        void CloseAchievements();
        AchievementPhase AchievementsPhase() const { return m_achievementPhase; }
        const std::vector<AchievementRow>& AchievementList() const { return m_achievements; }
        const std::string& AchievementsProblem() const { return m_achievementProblem; }
        std::filesystem::path AchievementIconFile(const std::string& icon) const;
        std::uint64_t AchievementIconsGeneration() const { return m_iconGeneration.load(); }
        void ChangeAchievements(std::vector<std::string> unlock, std::vector<std::string> lock, bool undoable = false);
        const std::string& AchievementsNotice() const { return m_achievementNotice; }
        bool AchievementsNoticeIsProblem() const { return m_achievementNoticeProblem; }
        std::size_t UndoableUnlocks() const { return m_undoUnlocks.size(); }
        void UndoUnlocks();
        const std::vector<StatRow>& StatList() const { return m_gameStats; }
        const StatRow* FindStatRow(std::string_view name) const;
        void SetGameStat(const std::string& name, double value);

        void SetPretendRunning(bool pretend) { m_pretendRunning = pretend; }

        Settings& Options() { return m_settings; }
        const std::filesystem::path& SettingsPath() const { return m_settingsPath; }
        const Settings& Options() const { return m_settings; }
        void SaveNow();

        std::optional<std::string> NewerVersion() const;
        bool TakeUpdateNews();
        void SetUpdatePreview(std::string version) { m_updatePreview = std::move(version); }

        Backups::Result BackUpNow();
        Backups::Result RestoreBackup(const std::filesystem::path& backup);
        bool SavesBlocked() const { return m_savesBlocked; }

    private:
        struct Session
        {
            std::uint32_t AppId = 0;
            WorkerProcess Worker;
            bool Ready = false;
            double RunSeconds = 0.0;
            bool ReportedError = false;
            bool StopRequested = false;
            double StopRequestedAt = 0.0;
        };

        std::vector<QueueEntry>& CurrentQueue() { return m_lists[static_cast<std::size_t>(m_activeList)].Entries; }
        bool LeaveHiddenList();
        bool InsertInList(int index, std::uint32_t appId);
        void NoteRemoved(std::vector<RemovedEntry> taken);
        std::string GameText(std::uint32_t appId) const;
        std::string QueueText() const;

        void RefreshSteam(double now);
        void RefreshRules();
        void RefreshGuards();
        void RefreshVariation(double now);
        void StaggerStarts(double now);
        void RefreshCards(double now);
        void RefreshCreatures(double now);
        void FindCreature(const CreatureInfo& info);
        double Random();
        void PollPlaying(double now);
        void StartScan();
        void CollectScan(double now);
        void PollSessions(double now, double elapsed);
        void HandleWorkerLine(Session& session, const std::string& line, double now);
        void Schedule(double now);
        Session* FindSession(std::uint32_t appId) const;
        void StopSession(Session& session, double now);
        void LoadListsOfAccount();
        void StoreListsOfAccount();
        std::string UniqueListName(std::string_view name, int except) const;
        void MarkDirty() { m_dirty = true; }
        void BackUpDaily(std::int32_t today);

        struct UpdateJob
        {
            std::atomic<bool> Done{ false };
            bool Ok = false;
            std::string Body;
            std::string Problem;
        };
        void RefreshUpdates(double now);
        void FinishUpdateCheck(double now);
        std::shared_ptr<UpdateJob> m_updateJob;
        double m_updateRetryAt = 0.0;
        bool m_updateWarned = false;
        std::string m_updatePreview;
        bool m_updatePreviewNoted = false;
        void StopAllWorkers();
        void UpdateKeepAwake(bool force);
        void UpdateInvisible(double now);
        void UpdateShutdown(double now);
        void PollAchievements(double now);
        void HandleAchievementLine(const std::string& line);
        void FinishAchievements();
        void FinishAchievementChange();
        void ReadAchievementSchema();
        void CountAchievementsOfGame();
        void FetchAchievementIcons(std::uint32_t appId, std::vector<std::string> icons);
        void AchievementIconLoop();
        void StopAchievementIcons();

        Settings& m_settings;
        std::filesystem::path m_settingsPath;
        std::filesystem::path m_statsPath;
        double m_origin = Clock::Seconds();
        Clock::PaceMeter m_pace;
        double m_lastUpdate = 0.0;
        double m_nextSteamCheck = 0.0;
        double m_nextPlayingCheck = 0.0;
        double m_nextStart = 0.0;
        double m_nextSave = 0.0;
        double m_nextScan = 0.0;
        double m_steamRetryAt = 0.0;
        bool m_dirty = false;
        bool m_savesBlocked = false;
        std::int32_t m_backupDay = 0;

        Steam::Status m_steam;
        std::uint32_t m_clientAccount = 0;
        std::uint32_t m_clientProcess = 0;
        std::string m_persona;
        std::uint32_t m_loadedAccount = 0;

        std::vector<Game> m_games;
        std::uint64_t m_libraryGeneration = 0;
        std::string m_scanProblem;
        bool m_scanning = false;
        bool m_scannedOnce = false;
        unsigned m_fontNeeds = 0;
        unsigned m_fontsAsked = 0;

        std::thread m_scanThread;
        std::atomic<bool> m_scanDone{ false };
        Steam::ScanResult m_scanResult;

        std::vector<QueueList> m_lists;
        int m_activeList = 0;
        UndoRecord m_undo;
        std::uint64_t m_undoGeneration = 0;
        std::vector<std::unique_ptr<Session>> m_sessions;
        std::unordered_map<std::uint32_t, double> m_stoppedAt;
        std::uint32_t m_playingApp = 0;
        bool m_outsideHours = false;
        bool m_userHere = false;
        bool m_active = false;
        bool m_startingUp = false;
        int m_wantedCount = 0;
        int m_readyCount = 0;
        void PretendPhase(double now);
        double m_pretendSwitchedAt = -100.0;
        int m_pretendAlive = 0;
        bool m_queueFinished = false;
        std::vector<WindowMessage> m_windowMessages;
        bool m_pretendMessages = false;
        double m_sessionSeconds = 0.0;
        bool m_steamLogged = false;
        bool m_settingsUnsaved = false;
        bool m_statsUnsaved = false;
        Hold m_loggedHold = Hold::None;

        CreatureSet m_creatures;
        CreatureSet m_newCreatures;
        std::optional<CreatureId> m_buddy;
        void RefreshBonds(double now);
        void NoteBondLevels();
        std::array<double, kCreatureCount> m_bondHours{};
        std::array<int, kCreatureCount> m_bondLevels{};
        std::uint32_t m_bondAccount = 0;
        bool m_bondsKnown = false;
        double m_nextBondCheck = 0.0;
        std::vector<CreatureId> m_bondNews;
        std::array<std::uint32_t, kCreatureCount> m_evolutions{};
        std::vector<CreatureId> m_evolutionNews;
        StoredCreature& StoredFor(CreatureId id);
        const StoredCreature* FindStored(CreatureId id) const;
        std::vector<const CreatureFamily*> m_finishedFamilies;
        std::vector<CreatureId> m_freshCreatures;
        int m_creatureNewsCount = 0;
        std::string m_creatureNewsName;
        double m_nextCreatureCheck = 0.0;
        int m_month = 0;
        int m_day = 0;
        int m_pretendMonth = 0;
        int m_pretendDay = 0;

        IdleStats m_stats;
        std::int32_t m_today = 0;
        int m_hour = CurrentHour();

        bool m_keepingAwake = false;
        double m_lastRunningAt = 0.0;

        bool m_shutdownArmed = false;
        double m_shutdownAt = -1.0;
        bool m_shutdownFailed = false;
        bool m_wantsWindow = false;

        bool m_pretendRunning = false;
        std::mt19937 m_random{ std::random_device{}() };

        std::uint32_t m_shownApp = 0;
        double m_shownStartedAt = -1.0;
        double m_otherStartedAt = -1.0;

        std::uint64_t m_cardsGeneration = 0;
        std::uint64_t m_receivedGeneration = 0;
        double m_nextCardCheck = 0.0;
        double m_nextCardFullRead = 0.0;
        double m_nextCardSetCheck = 0.0;
        std::unordered_set<std::uint32_t> m_cardSetsAsked;
        int m_cardsDropped = 0;
        bool m_cardDropped = false;

        std::unique_ptr<WorkerProcess> m_achievementWorker;
        std::uint32_t m_achievementApp = 0;
        double m_achievementDeadline = 0.0;
        AchievementPhase m_achievementPhase = AchievementPhase::Idle;
        std::vector<AchievementRow> m_achievements;
        std::filesystem::path m_iconFolder;
        std::thread m_iconThread;
        std::mutex m_iconLock;
        std::condition_variable m_iconWake;
        std::uint32_t m_iconApp = 0;
        std::vector<std::string> m_iconsWanted;
        bool m_iconStop = false;
        std::atomic<std::uint64_t> m_iconGeneration{ 0 };
        std::string m_achievementProblem;
        std::vector<std::string> m_achievementsUnlocking;
        std::vector<std::string> m_achievementsLocking;
        std::vector<AchievementRow> m_achievementsIncoming;
        std::vector<StatRow> m_gameStats;
        std::vector<StatRow> m_gameStatsIncoming;
        std::string m_statSetting;
        double m_statSettingTo = 0.0;
        bool m_achievementChangeUndoable = false;
        bool m_achievementWorkerChanges = false;
        std::vector<std::string> m_undoUnlocks;
        std::string m_achievementNotice;
        bool m_achievementNoticeProblem = false;

        CardService m_cards;
    };
}
