#include "App/IdlerModel.h"

#include "App/Desktop.h"
#include "Core/CrashReport.h"
#include "Core/Errors.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Core/Version.h"
#include "Core/WebPolicy.h"
#include "Steam/SteamWeb.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <fstream>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace Idler
{
    namespace
    {
        constexpr double kSecondsBetweenStarts = 0.4;
        constexpr double kSteamCheckSeconds = 1.0;
        constexpr double kPlayingCheckSeconds = 3.0;
        constexpr double kSaveSeconds = 60.0;
        constexpr double kRescanSeconds = 600.0;
        constexpr double kSteamRetrySeconds = 10.0;
        constexpr double kStopGraceSeconds = 3.0;
        constexpr double kStoppedGraceSeconds = 15.0;
        constexpr int kMaxCrashes = 3;
        constexpr double kMaxElapsed = 5.0;
        constexpr double kBackOnlineSeconds = 20.0;
        constexpr double kShutdownCountdownSeconds = 60.0;
        constexpr double kAchievementsTimeoutSeconds = 30.0;
        constexpr std::size_t kMaxListNameBytes = 40;
        constexpr double kShownRestartSeconds = 120.0;
        constexpr double kCardCheckSeconds = 15.0 * 60.0;
        constexpr double kCardFullReadSeconds = 60.0 * 60.0;
        constexpr double kCardSetCheckSeconds = 5.0;
        constexpr char kCardListName[] = "Cards";
        constexpr double kCreatureCheckSeconds = 5.0;
        constexpr double kBondCheckSeconds = 30.0;
        constexpr int kCreatureDaysLooked = 60;
        constexpr std::uint64_t kSteamIdBase = 76561197960265728ull;

        std::string PathText(const std::filesystem::path& path)
        {
            const std::u8string text = path.u8string();
            return std::string(text.begin(), text.end());
        }

        std::string CleanListName(std::string_view name)
        {
            std::string clean;
            for (const char c : Trim(name))
            {
                if (c != '|' && static_cast<unsigned char>(c) >= 0x20)
                    clean.push_back(c);
            }
            if (clean.size() > kMaxListNameBytes)
            {
                std::size_t end = kMaxListNameBytes;
                while (end > 0 && (static_cast<unsigned char>(clean[end]) & 0xC0) == 0x80)
                    --end;
                clean.resize(end);
            }
            return std::string(Trim(clean));
        }
    }

    IdlerModel::IdlerModel(Settings& settings, std::filesystem::path settingsPath)
        : m_settings(settings), m_settingsPath(std::move(settingsPath)), m_cards(m_settingsPath)
    {
        m_statsPath = m_settingsPath;
        m_statsPath.replace_extension(L".stats");
        m_iconFolder = m_settingsPath;
        m_iconFolder.replace_extension(L".achievement-icons");
        m_stats.Load(m_statsPath);
        m_lists.push_back({ kDefaultListName, {} });
        m_active = m_settings.ResumeOnStart && m_settings.WasActive;

        LOG_INFO("Manifold Idler " + std::string(kVersion) + " started");
        if constexpr (!kCardFarmEnabled)
            LOG_INFO("Card farm is off: nothing goes to Steam's web side");
        if (const std::optional<CrashReport::LastCrash> crash = CrashReport::TakeLastCrash(m_settingsPath))
        {
            const std::filesystem::path where = crash->Report.empty() ? CrashReport::FolderOf(m_settingsPath) : crash->Report;
            LOG_ERROR(ErrorMessage(ErrorCode::LastRunCrashed, PathText(where)));
            if (!crash->Problem.empty())
                LOG_ERROR("Problem: " + crash->Problem);
            for (const std::string& line : crash->Stack)
                LOG_ERROR(line);
            m_windowMessages.push_back({ WindowMessage::Kind::LastRunCrashed, 0 });
        }

        for (const CreatureInfo& info : AllCreatures())
        {
            if (info.Kind == UnlockKind::Start)
                m_creatures.set(static_cast<std::size_t>(info.Id));
        }
        for (const StoredCreature& stored : m_settings.Creatures)
        {
            if (const auto id = CreatureWithKey(stored.Key))
                m_creatures.set(static_cast<std::size_t>(*id));
        }
        m_buddy = CreatureWithKey(m_settings.CreatureBuddy);
        BackUpDaily(TodayNumber());
    }

    IdlerModel::~IdlerModel()
    {
        StopAllWorkers();
        StopAchievementIcons();
        if (m_scanThread.joinable())
            m_scanThread.join();
    }

    double IdlerModel::Now() const
    {
        return Clock::Seconds() - m_origin;
    }

    void IdlerModel::StopAllWorkers()
    {
        for (const auto& session : m_sessions)
            session->Worker.CloseInput();

        const double deadline = Clock::Seconds() + 2.0;
        while (Clock::Seconds() < deadline &&
            std::any_of(m_sessions.begin(), m_sessions.end(), [](const auto& session) { return session->Worker.Running(); }))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        for (const auto& session : m_sessions)
        {
            if (session->Worker.Running())
                session->Worker.Terminate();
        }
        m_sessions.clear();
        m_achievementWorker.reset();
    }

    void IdlerModel::Update()
    {
        m_cards.Pump();
        const double now = Now();
        const double elapsed = std::min(now - m_lastUpdate, kMaxElapsed);
        m_lastUpdate = now;
        if (m_pace.Note(Clock::OwnSeconds(), now))
        {
            if (m_pace.Pace() == 1.0)
            {
                LOG_INFO("The program's own clock keeps real time again");
            }
            else
            {
                char pace[32];
                std::snprintf(pace, sizeof(pace), "%.2f", m_pace.Pace());
                LOG_WARN(ErrorMessage(ErrorCode::ClockRunsOff, pace));
            }
        }

        CollectScan(now);
        if (now >= m_nextSteamCheck)
        {
            m_nextSteamCheck = now + kSteamCheckSeconds;
            m_today = TodayNumber();
            m_hour = CurrentHour();
            if (m_today != m_backupDay)
                BackUpDaily(m_today);
            RefreshSteam(now);
            RefreshRules();
            RefreshGuards();
            RefreshVariation(now);
            RefreshCards(now);
            if (m_pretendRunning)
            {
                int index = 0;
                for (QueueEntry& entry : CurrentQueue())
                {
                    entry.Resting = index++ % 3 == 2;
                    entry.RestUntil = entry.Resting ? now + 600.0 : 0.0;
                }
            }
            RefreshCreatures(now);
        }
        if (now >= m_nextPlayingCheck)
        {
            m_nextPlayingCheck = now + kPlayingCheckSeconds;
            PollPlaying(now);
        }
        PollSessions(now, elapsed);
        Schedule(now);
        UpdateKeepAwake(false);
        UpdateInvisible(now);
        UpdateShutdown(now);
        PollAchievements(now);

        if (m_dirty && now >= m_nextSave)
            SaveNow();
    }

    void IdlerModel::Finish()
    {
        StopAchievementIcons();
        LOG_INFO("Closing with " + std::to_string(m_sessions.size()) + " games running");
        StopAllWorkers();
        UpdateKeepAwake(true);
        if (m_settings.SetInvisible && m_steam.LoggedOn())
        {
            LOG_INFO("Steam status: online");
            Desktop::SetSteamStatus(L"online");
            m_settings.SetInvisible = false;
        }
        SaveNow();
    }

    void IdlerModel::SaveNow()
    {
        if (m_savesBlocked)
            return;
        StoreListsOfAccount();
        m_settings.WasActive = m_active;
        const bool settingsSaved = m_settings.Save(m_settingsPath);
        if (!settingsSaved && !m_settingsUnsaved)
            LOG_ERROR(ErrorMessage(ErrorCode::SettingsNotSaved, PathText(m_settingsPath)));
        m_settingsUnsaved = !settingsSaved;
        const bool statsSaved = m_stats.Save(m_statsPath);
        if (!statsSaved && !m_statsUnsaved)
            LOG_ERROR(ErrorMessage(ErrorCode::StatsNotSaved, PathText(m_statsPath)));
        m_statsUnsaved = !statsSaved;
        m_dirty = false;
        m_nextSave = Now() + kSaveSeconds;
    }

    void IdlerModel::BackUpDaily(std::int32_t today)
    {
        m_backupDay = today;
        const Backups::Result result = Backups::Make(m_settingsPath, Backups::BackupKind::Daily, std::time(nullptr));
        if (result.What == Backups::Outcome::Made)
            LOG_INFO("Backed up the settings and statistics as " + PathText(result.File.filename()));
        else if (!result.Ok())
            LOG_ERROR(result.Problem);
    }

    Backups::Result IdlerModel::BackUpNow()
    {
        SaveNow();
        const Backups::Result result = Backups::Make(m_settingsPath, Backups::BackupKind::Saved, std::time(nullptr));
        if (result.What == Backups::Outcome::Made)
            LOG_SUCCESS("Backed up the settings and statistics as " + PathText(result.File.filename()));
        else if (!result.Ok())
            LOG_ERROR(result.Problem);
        return result;
    }

    Backups::Result IdlerModel::RestoreBackup(const std::filesystem::path& backup)
    {
        SaveNow();
        const Backups::Result result = Backups::Restore(m_settingsPath, backup, std::time(nullptr));
        if (!result.Ok())
        {
            LOG_ERROR(result.Problem);
            return result;
        }
        m_savesBlocked = true;
        LOG_SUCCESS("Restored the backup " + PathText(backup.filename()) + ". The program starts again with it.");
        return result;
    }

    void IdlerModel::RefreshSteam(double now)
    {
        Steam::Status status = Steam::ReadStatus();
        if (status.AccountFromClient && status.ProcessId == m_clientProcess)
            status.Account = m_clientAccount;
        const bool changed = status.Account != m_steam.Account || status.Running() != m_steam.Running();
        m_steam = status;

        if (changed || !m_steamLogged)
        {
            m_steamLogged = true;
            if (status.LoggedOn())
                LOG_SUCCESS("Steam is running, account " + std::to_string(status.Account));
            else if (status.MayConnect())
                LOG_INFO("Steam is running; asking it which account is logged on");
            else if (status.Running())
                LOG_WARN(ErrorMessage(ErrorCode::NobodyLoggedOn));
            else
                LOG_WARN(ErrorMessage(ErrorCode::SteamNotRunning));
        }

        if (changed)
        {
            if (status.Account != 0 && status.Account != m_loadedAccount)
            {
                StoreListsOfAccount();
                m_loadedAccount = status.Account;
                LoadListsOfAccount();
                m_games.clear();
                m_persona.clear();
                ++m_libraryGeneration;
            }
            m_steamRetryAt = 0.0;
            m_nextScan = 0.0;
        }

        if (status.MayConnect() && now >= m_nextScan && !m_scanning)
            StartScan();
    }

    void IdlerModel::RefreshRules()
    {
        m_outsideHours = false;
        if (m_settings.OnlyInHours)
        {
            const std::time_t time = std::time(nullptr);
            std::tm local{};
            if (LocalTime(time, local))
                m_outsideHours = !InHourWindow(m_settings.FromHour, m_settings.ToHour, local.tm_hour);
        }

        m_userHere = m_settings.OnlyWhenAway && Desktop::SecondsSinceInput() < static_cast<double>(m_settings.AwayMinutes) * 60.0;
    }

    void IdlerModel::RefreshGuards()
    {
        const auto now = static_cast<std::uint32_t>(std::time(nullptr));
        for (QueueEntry& entry : CurrentQueue())
        {
            const bool held = IsHeld(entry);
            entry.GuardSecondsLeft = -1.0;
            if (!m_settings.RefundGuard || entry.IgnoreRefund)
                continue;
            if (const Game* game = FindGame(entry.AppId))
            {
                const double played = std::max(static_cast<double>(game->PlaytimeMinutes), static_cast<double>(entry.BaselineMinutes)) * 60.0;
                entry.GuardSecondsLeft = RefundGuardLeft(game->Purchased, std::max(played, static_cast<double>(entry.BaselineMinutes) * 60.0 + entry.IdledSeconds), now);
                if (!held && IsHeld(entry))
                    LOG_WARN("Held back by the refund guard: " + GameText(entry.AppId));
            }
        }
    }

    void IdlerModel::PollPlaying(double now)
    {
        m_playingApp = 0;
        if (!m_settings.PauseWhilePlaying || !m_steam.LoggedOn())
            return;

        for (const std::uint32_t appId : Steam::RunningApps())
        {
            if (FindSession(appId) || (m_achievementWorker && appId == m_achievementApp))
                continue;
            const auto stopped = m_stoppedAt.find(appId);
            if (stopped != m_stoppedAt.end() && now - stopped->second < kStoppedGraceSeconds)
                continue;
            m_playingApp = appId;
            return;
        }
    }

    Hold IdlerModel::CurrentHold() const
    {
        if (!m_steam.LoggedOn())
            return Hold::Steam;
        if (m_playingApp != 0)
            return Hold::Playing;
        if (m_outsideHours)
            return Hold::Hours;
        if (m_userHere)
            return Hold::Away;
        return Hold::None;
    }

    const Game* IdlerModel::FindGame(std::uint32_t appId) const
    {
        const auto game = std::find_if(m_games.begin(), m_games.end(), [&](const Game& known) { return known.AppId == appId; });
        return game == m_games.end() ? nullptr : &*game;
    }

    void IdlerModel::Rescan()
    {
        if (!m_scanning && m_steam.MayConnect())
            StartScan();
    }

    void IdlerModel::StartScan()
    {
        if (m_scanThread.joinable())
            m_scanThread.join();

        m_scanning = true;
        m_scanDone = false;
        m_scanThread = std::thread([this, status = m_steam]()
            {
                m_scanResult = Steam::ScanLibrary(status);
                m_scanDone = true;
            });
    }

    void IdlerModel::CollectScan(double now)
    {
        if (!m_scanning || !m_scanDone)
            return;

        m_scanThread.join();
        m_scanning = false;
        Steam::ScanResult result = std::move(m_scanResult);
        if (m_steam.AccountFromClient && result.Problem.empty() && result.Account != 0 && result.Account != m_steam.Account)
        {
            m_clientAccount = result.Account;
            m_clientProcess = m_steam.ProcessId;
            m_nextSteamCheck = now;
            m_nextScan = now;
            return;
        }

        m_scannedOnce = true;
        m_nextScan = now + kRescanSeconds;
        if (!result.Problem.empty() && result.Problem != m_scanProblem)
            LOG_ERROR("Library not read: " + result.Problem);
        m_scanProblem = result.Problem;
        if (result.Account != m_steam.Account || !result.Problem.empty())
        {
            m_nextScan = now + kSteamRetrySeconds;
            return;
        }

        const std::size_t gamesBefore = m_games.size();
        m_games = std::move(result.Games);
        MarkGamesWithCards(m_games, m_cards.Drops());
        if (m_games.size() != gamesBefore)
            LOG_SUCCESS("Library read: " + std::to_string(m_games.size()) + " games");
        if (!result.Persona.empty())
            m_persona = std::move(result.Persona);
        ++m_libraryGeneration;

        m_fontNeeds |= FontNeedsOf(m_persona);
        for (const Game& game : m_games)
            m_fontNeeds |= FontNeedsOf(game.Name);

        for (QueueList& list : m_lists)
        {
            for (QueueEntry& entry : list.Entries)
            {
                if (entry.IdledSeconds <= 0.0)
                {
                    if (const Game* game = FindGame(entry.AppId))
                        entry.BaselineMinutes = game->PlaytimeMinutes;
                }
            }
        }
        RefreshGuards();
    }

    unsigned IdlerModel::TakeFontNeeds()
    {
        const unsigned fresh = m_fontNeeds & ~m_fontsAsked;
        m_fontsAsked |= fresh;
        return fresh;
    }

    void IdlerModel::LoadListsOfAccount()
    {
        m_lists.clear();
        m_activeList = 0;
        m_undo = {};
        const auto listNamed = [&](const std::string& name) -> QueueList&
            {
                for (QueueList& list : m_lists)
                {
                    if (list.Name == name)
                        return list;
                }
                m_lists.push_back({ name, {} });
                return m_lists.back();
            };

        for (const StoredList& stored : m_settings.Lists)
        {
            if (stored.Account == m_loadedAccount)
                listNamed(stored.Name);
        }
        for (const StoredEntry& stored : m_settings.Queue)
        {
            if (stored.Account == m_loadedAccount)
                listNamed(stored.List).Entries.push_back(stored.Entry);
        }
        if (m_lists.empty())
            m_lists.push_back({ kDefaultListName, {} });

        for (const StoredList& stored : m_settings.ActiveLists)
        {
            if (stored.Account != m_loadedAccount)
                continue;
            for (std::size_t index = 0; index < m_lists.size(); ++index)
            {
                if (m_lists[index].Name == stored.Name)
                    m_activeList = static_cast<int>(index);
            }
        }
        if (LeaveHiddenList())
        {
            LOG_WARN("The list that farms cards does not show while the card farm is off");
            m_active = false;
        }

        m_shownApp = 0;
        for (const StoredShown& shown : m_settings.ShownGames)
        {
            if (shown.Account == m_loadedAccount)
                m_shownApp = shown.AppId;
        }

        RefreshGuards();
        if (m_active)
        {
            LOG_INFO("Queue goes on: " + QueueText());
            StaggerStarts(Now());
        }
    }

    void IdlerModel::StoreListsOfAccount()
    {
        if (m_loadedAccount == 0)
            return;
        const auto mine = [&](const auto& stored) { return stored.Account == m_loadedAccount; };
        std::erase_if(m_settings.Queue, mine);
        std::erase_if(m_settings.Lists, mine);
        std::erase_if(m_settings.ActiveLists, mine);
        std::erase_if(m_settings.ShownGames, mine);
        if (m_shownApp != 0)
            m_settings.ShownGames.push_back({ m_loadedAccount, m_shownApp });
        for (const QueueList& list : m_lists)
        {
            m_settings.Lists.push_back({ m_loadedAccount, list.Name });
            for (const QueueEntry& entry : list.Entries)
                m_settings.Queue.push_back({ m_loadedAccount, list.Name, entry });
        }
        m_settings.ActiveLists.push_back({ m_loadedAccount, m_lists[static_cast<std::size_t>(m_activeList)].Name });
    }

    bool IdlerModel::IsListHidden(int index) const
    {
        const QueueList& list = m_lists[static_cast<std::size_t>(index)];
        return !kCardFarmEnabled && list.Name == kCardListName
            && std::any_of(list.Entries.begin(), list.Entries.end(), [](const QueueEntry& entry) { return entry.Goal.Kind == GoalKind::CardDrops; });
    }

    int IdlerModel::VisibleListCount() const
    {
        int count = 0;
        for (int index = 0; index < static_cast<int>(m_lists.size()); ++index)
            count += IsListHidden(index) ? 0 : 1;
        return count;
    }

    bool IdlerModel::LeaveHiddenList()
    {
        if (!IsListHidden(m_activeList))
            return false;
        for (int index = 0; index < static_cast<int>(m_lists.size()); ++index)
        {
            if (!IsListHidden(index))
            {
                m_activeList = index;
                return true;
            }
        }
        m_lists.push_back({ kDefaultListName, {} });
        m_activeList = static_cast<int>(m_lists.size()) - 1;
        return true;
    }

    std::string IdlerModel::UniqueListName(std::string_view name, int except) const
    {
        const std::string clean = CleanListName(name);
        const std::string base = clean.empty() ? std::string("List") : clean;
        const auto taken = [&](const std::string& candidate)
            {
                for (std::size_t index = 0; index < m_lists.size(); ++index)
                {
                    if (static_cast<int>(index) != except && EqualsIgnoreCase(m_lists[index].Name, candidate))
                        return true;
                }
                return false;
            };
        std::string candidate = base;
        for (int number = 2; taken(candidate); ++number)
            candidate = base + " " + std::to_string(number);
        return candidate;
    }

    void IdlerModel::SelectList(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_lists.size()) || index == m_activeList)
            return;
        m_activeList = index;
        LOG_INFO("Queue is now " + QueueText());
        RefreshGuards();
        if (m_active)
            StaggerStarts(Now());
        MarkDirty();
    }

    void IdlerModel::CreateList(std::string_view name)
    {
        m_lists.push_back({ UniqueListName(name, -1), {} });
        m_activeList = static_cast<int>(m_lists.size()) - 1;
        LOG_INFO("List created: " + m_lists.back().Name);
        MarkDirty();
    }

    void IdlerModel::RenameList(int index, std::string_view name)
    {
        if (index < 0 || index >= static_cast<int>(m_lists.size()))
            return;
        const bool removedHere = m_undo.List == m_lists[static_cast<std::size_t>(index)].Name;
        m_lists[static_cast<std::size_t>(index)].Name = UniqueListName(name, index);
        if (removedHere)
            m_undo.List = m_lists[static_cast<std::size_t>(index)].Name;
        LOG_INFO("List renamed: " + m_lists[static_cast<std::size_t>(index)].Name);
        MarkDirty();
    }

    void IdlerModel::DeleteList(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_lists.size()) || VisibleListCount() < 2)
            return;
        QueueList& list = m_lists[static_cast<std::size_t>(index)];
        LOG_INFO("List deleted: " + list.Name);
        m_undo = {};
        m_undo.List = list.Name;
        m_undo.ListPlace = index;
        m_undo.Entries = TakeEntries(list.Entries, [](const QueueEntry&) { return true; });
        ++m_undoGeneration;
        m_lists.erase(m_lists.begin() + index);
        if (m_activeList >= index && m_activeList > 0)
            --m_activeList;
        LeaveHiddenList();
        RefreshGuards();
        MarkDirty();
    }

    bool IdlerModel::InsertInList(int index, std::uint32_t appId)
    {
        if (index < 0 || index >= static_cast<int>(m_lists.size()))
            return false;
        std::vector<QueueEntry>& entries = m_lists[static_cast<std::size_t>(index)].Entries;
        if (Idler::FindEntry(entries, appId))
            return false;
        QueueEntry entry;
        entry.AppId = appId;
        entry.Goal = m_settings.DefaultGoal;
        if (const Game* game = FindGame(appId))
            entry.BaselineMinutes = game->PlaytimeMinutes;
        entries.push_back(std::move(entry));
        RefreshGuards();
        MarkDirty();
        return true;
    }

    void IdlerModel::AddToList(int index, std::uint32_t appId)
    {
        if (InsertInList(index, appId))
            LOG_INFO("Added to \"" + m_lists[static_cast<std::size_t>(index)].Name + "\": " + GameText(appId));
    }

    void IdlerModel::AddGamesToList(int index, const std::vector<std::uint32_t>& apps)
    {
        int added = 0;
        for (const std::uint32_t appId : apps)
            added += InsertInList(index, appId) ? 1 : 0;
        if (added > 0)
            LOG_INFO("Added " + std::to_string(added) + (added == 1 ? " game to \"" : " games to \"") + m_lists[static_cast<std::size_t>(index)].Name + "\"");
    }

    std::string IdlerModel::GameText(std::uint32_t appId) const
    {
        const Game* game = FindGame(appId);
        return (game && !game->Name.empty() ? game->Name : std::string("Game")) + " (" + std::to_string(appId) + ")";
    }

    std::string IdlerModel::QueueText() const
    {
        const auto& queue = Queue();
        return "list \"" + m_lists[static_cast<std::size_t>(m_activeList)].Name + "\", " + std::to_string(queue.size()) + (queue.size() == 1 ? " game" : " games");
    }

    const QueueEntry* IdlerModel::FindEntry(std::uint32_t appId) const
    {
        const auto& queue = Queue();
        const auto entry = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& known) { return known.AppId == appId; });
        return entry == queue.end() ? nullptr : &*entry;
    }

    void IdlerModel::AddToQueue(std::uint32_t appId)
    {
        AddToList(m_activeList, appId);
    }

    void IdlerModel::AddAllToQueue(const std::vector<int>& indices)
    {
        int added = 0;
        for (const int index : indices)
            added += InsertInList(m_activeList, m_games[static_cast<std::size_t>(index)].AppId) ? 1 : 0;
        LOG_INFO("Added " + std::to_string(added) + " games: " + QueueText());
    }

    void IdlerModel::IdleNow(std::uint32_t appId)
    {
        LOG_INFO("Idle now: " + GameText(appId));
        AddToQueue(appId);
        auto& queue = CurrentQueue();
        const auto entry = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& known) { return known.AppId == appId; });
        entry->Failed = false;
        entry->Error.clear();
        entry->Crashes = 0;
        entry->Resting = false;
        entry->RestUntil = 0.0;
        if (IsFinished(*entry))
        {
            entry->IdledSeconds = 0.0;
            entry->Goal.Kind = GoalKind::None;
        }
        Idler::MoveEntry(queue, static_cast<int>(entry - queue.begin()), 0);
        SetActive(true);
        MarkDirty();
    }

    void IdlerModel::Remove(std::uint32_t appId)
    {
        LOG_INFO("Removed: " + GameText(appId));
        NoteRemoved(TakeEntries(CurrentQueue(), [&](const QueueEntry& entry) { return entry.AppId == appId; }));
        MarkDirty();
    }

    void IdlerModel::RemoveFinished()
    {
        std::vector<RemovedEntry> taken = TakeEntries(CurrentQueue(), [](const QueueEntry& entry) { return IsFinished(entry); });
        LOG_INFO("Removed " + std::to_string(taken.size()) + " finished games");
        NoteRemoved(std::move(taken));
        MarkDirty();
    }

    void IdlerModel::RemoveAll()
    {
        std::vector<RemovedEntry> taken = TakeEntries(CurrentQueue(), [](const QueueEntry&) { return true; });
        LOG_INFO("Removed all " + std::to_string(taken.size()) + " games");
        NoteRemoved(std::move(taken));
        MarkDirty();
    }

    void IdlerModel::NoteRemoved(std::vector<RemovedEntry> taken)
    {
        if (taken.empty())
            return;
        const std::string& list = m_lists[static_cast<std::size_t>(m_activeList)].Name;
        if (m_undo.ListPlace >= 0 || !m_undo.Order.empty() || m_undo.List != list)
        {
            m_undo = {};
            m_undo.List = list;
        }
        m_undo.Entries.insert(m_undo.Entries.end(), taken.begin(), taken.end());
        ++m_undoGeneration;
    }

    void IdlerModel::SortQueue(QueueOrder order)
    {
        const std::string& list = m_lists[static_cast<std::size_t>(m_activeList)].Name;
        if (m_undo.Order.empty() || m_undo.List != list)
        {
            m_undo = {};
            m_undo.List = list;
            m_undo.Order = OrderOf(CurrentQueue());
        }
        m_undo.SortedBy = order;
        ++m_undoGeneration;
        Idler::SortQueue(CurrentQueue(), order, [&](std::uint32_t appId)
            {
                const Game* game = FindGame(appId);
                return game ? game->Folded : std::string();
            });
        LOG_INFO("Sorted " + QueueText() + ": " + QueueOrderName(order));
        MarkDirty();
    }

    void IdlerModel::Undo()
    {
        if (!m_undo.Any())
            return;
        const UndoRecord record = std::exchange(m_undo, {});
        if (record.ListPlace >= 0)
        {
            const int place = std::min(record.ListPlace, static_cast<int>(m_lists.size()));
            QueueList list;
            list.Name = UniqueListName(record.List, -1);
            PutBackEntries(list.Entries, record.Entries);
            m_lists.insert(m_lists.begin() + place, std::move(list));
            if (m_activeList >= place)
                ++m_activeList;
            SelectList(place);
            LOG_INFO("List back: " + record.List);
        }
        else
        {
            const auto list = std::find_if(m_lists.begin(), m_lists.end(), [&](const QueueList& known) { return known.Name == record.List; });
            if (list == m_lists.end())
            {
                LOG_WARN("Nothing to put back in, the list is gone: " + record.List);
                return;
            }
            if (!record.Order.empty())
            {
                RestoreOrder(list->Entries, record.Order);
                LOG_INFO("Order back in \"" + record.List + "\"");
            }
            else
            {
                PutBackEntries(list->Entries, record.Entries);
                LOG_INFO("Back in \"" + record.List + "\": " + std::to_string(record.Entries.size()) + (record.Entries.size() == 1 ? " game" : " games"));
            }
        }
        RefreshGuards();
        if (record.EndedIdling)
            SetActive(true);
        MarkDirty();
    }

    void IdlerModel::MoveEntry(int from, int to)
    {
        Idler::MoveEntry(CurrentQueue(), from, to);
        MarkDirty();
    }

    void IdlerModel::SetGoal(std::uint32_t appId, Goal goal)
    {
        if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), appId))
        {
            entry->Goal = goal;
            MarkDirty();
        }
    }

    void IdlerModel::Retry(std::uint32_t appId)
    {
        if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), appId))
        {
            LOG_INFO("Trying again: " + GameText(appId));
            entry->Failed = false;
            entry->Error.clear();
            entry->Crashes = 0;
        }
    }

    void IdlerModel::AllowPastRefund(std::uint32_t appId)
    {
        if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), appId))
        {
            LOG_WARN("Idles past the refund limit: " + GameText(appId));
            entry->IgnoreRefund = true;
            RefreshGuards();
            MarkDirty();
        }
    }

    QueuePhase IdlerModel::Phase() const
    {
        const bool waiting = m_active && !m_pretendRunning && (CurrentHold() != Hold::None || AllOnBreak(Queue()));
        return PhaseOfQueue(m_active, waiting, m_startingUp, AliveCount());
    }

    void IdlerModel::SetActive(bool active)
    {
        if (m_active == active)
            return;
        m_active = active;
        m_steamRetryAt = 0.0;
        m_startingUp = active;
        m_pretendSwitchedAt = Now();
        if (active)
        {
            LOG_INFO("Queue started: " + QueueText() + ", up to " + std::to_string(m_settings.Slots) + " at once");
            StaggerStarts(Now());
        }
        else
        {
            LOG_INFO("Queue stopped");
        }
        MarkDirty();
    }

    double IdlerModel::Random()
    {
        return std::uniform_real_distribution<double>(0.0, 1.0)(m_random);
    }

    void IdlerModel::StaggerStarts(double now)
    {
        if (!m_settings.VaryTimes)
            return;
        bool first = true;
        for (QueueEntry& entry : CurrentQueue())
        {
            if (entry.Goal.Kind == GoalKind::CardDrops)
                continue;
            const bool canRun = !entry.Failed && !IsFinished(entry) && !IsHeld(entry);
            entry.RunUntil = 0.0;
            entry.RestUntil = first && canRun ? 0.0 : now + Random() * kStartSpreadSeconds;
            entry.Resting = entry.RestUntil > now;
            if (canRun)
                first = false;
        }
        if (!first)
            LOG_INFO("Varied times: the games start up to " + std::to_string(static_cast<int>(kStartSpreadSeconds / 60.0)) + " minutes apart");
    }

    void IdlerModel::RefreshVariation(double now)
    {
        const float spread = m_settings.VarySpread / 100.0f;
        for (QueueEntry& entry : CurrentQueue())
        {
            if (!m_settings.VaryTimes)
            {
                if (entry.GoalFactor != 1.0f)
                    MarkDirty();
                entry.GoalFactor = 1.0f;
                entry.Resting = false;
                entry.RestUntil = 0.0;
                entry.RunUntil = 0.0;
                continue;
            }
            if (entry.GoalFactor == 1.0f)
            {
                entry.GoalFactor = DrawGoalFactor(spread, Random());
                MarkDirty();
            }
            const Session* session = FindSession(entry.AppId);
            const bool wasResting = entry.Resting;
            AdvanceRhythm(entry, now, session && session->Ready && !session->StopRequested, Random());
            if (entry.Resting && !wasResting)
            {
                LOG_INFO("On a break for " + std::to_string(static_cast<int>((entry.RestUntil - now) / 60.0 + 0.5)) + " min: " + GameText(entry.AppId));
                ++m_settings.CreatureBreaks;
                MarkDirty();
            }
        }
    }

    double IdlerModel::RestSecondsLeft(std::uint32_t appId) const
    {
        const QueueEntry* entry = FindEntry(appId);
        return entry && entry->Resting ? std::max(0.0, entry->RestUntil - Now()) : 0.0;
    }

    void IdlerModel::SetShownApp(std::uint32_t appId)
    {
        m_shownApp = m_shownApp == appId ? 0 : appId;
        m_shownStartedAt = -1.0;
        m_otherStartedAt = Now();
        MarkDirty();
    }

    std::uint64_t IdlerModel::SteamId() const
    {
        return m_loadedAccount != 0 ? kSteamIdBase + m_loadedAccount : 0;
    }

    bool IdlerModel::CardLoginIsOtherAccount() const
    {
        const std::uint64_t login = m_cards.LoginSteamId();
        return login != 0 && m_steam.Account != 0 && login != kSteamIdBase + m_steam.Account;
    }

    bool IdlerModel::QueueFarmsCards() const
    {
        const auto& queue = Queue();
        return kCardFarmEnabled && std::any_of(queue.begin(), queue.end(), [](const QueueEntry& entry) { return entry.Goal.Kind == GoalKind::CardDrops; });
    }

    void IdlerModel::FarmCards()
    {
        if (CardLoginIsOtherAccount())
            return;

        int index = -1;
        for (std::size_t i = 0; i < m_lists.size(); ++i)
        {
            if (m_lists[i].Name == kCardListName)
                index = static_cast<int>(i);
        }
        if (index < 0)
        {
            m_lists.push_back({ UniqueListName(kCardListName, -1), {} });
            index = static_cast<int>(m_lists.size()) - 1;
        }
        m_activeList = index;

        const auto drops = m_cards.Drops();
        auto& entries = CurrentQueue();
        for (const Game& game : m_games)
        {
            const auto found = drops.find(game.AppId);
            if (found == drops.end() || found->second == 0 || Idler::FindEntry(entries, game.AppId))
                continue;
            QueueEntry entry;
            entry.AppId = game.AppId;
            entry.Goal.Kind = GoalKind::CardDrops;
            entry.BaselineMinutes = game.PlaytimeMinutes;
            entry.DropsLeft = static_cast<int>(found->second);
            entries.push_back(std::move(entry));
        }
        for (QueueEntry& entry : entries)
        {
            if (const auto found = drops.find(entry.AppId); found != drops.end())
                entry.DropsLeft = static_cast<int>(found->second);
        }

        const double now = Now();
        m_nextCardCheck = now + kCardCheckSeconds;
        m_nextCardFullRead = now + kCardFullReadSeconds;
        RefreshGuards();
        m_active = true;
        m_steamRetryAt = 0.0;
        MarkDirty();
    }

    bool IdlerModel::TakeCardDropped()
    {
        return std::exchange(m_cardDropped, false);
    }

    void IdlerModel::RefreshCards(double now)
    {
        if (m_cards.ReceivedGeneration() != m_receivedGeneration)
        {
            m_receivedGeneration = m_cards.ReceivedGeneration();
            for (const ReceivedCard& card : m_cards.Received())
                m_fontNeeds |= FontNeedsOf(card.Name);
        }

        if (m_cards.Generation() != m_cardsGeneration)
        {
            m_cardsGeneration = m_cards.Generation();
            const auto drops = m_cards.Drops();
            const bool readAll = m_cards.LastFullRead() != 0;
            if (MarkGamesWithCards(m_games, drops))
                ++m_libraryGeneration;
            for (QueueList& list : m_lists)
            {
                for (QueueEntry& entry : list.Entries)
                {
                    if (entry.Goal.Kind != GoalKind::CardDrops)
                        continue;
                    const auto found = drops.find(entry.AppId);
                    const int left = found != drops.end() ? static_cast<int>(found->second) : readAll ? 0 : entry.DropsLeft;
                    if (entry.DropsLeft > left)
                    {
                        m_cardsDropped += entry.DropsLeft - left;
                        m_cardDropped = true;
                    }
                    entry.DropsLeft = left;
                }
            }
        }

        if (!m_active || !QueueFarmsCards() || CardLoginIsOtherAccount() || m_cards.State() != LoginState::SignedIn)
            return;

        if (now >= m_nextCardSetCheck)
        {
            m_nextCardSetCheck = now + kCardSetCheckSeconds;
            std::vector<std::uint32_t> unread;
            for (const auto& session : m_sessions)
            {
                if (session->Ready && !session->StopRequested && !m_cards.KnowsCardSet(session->AppId) && m_cardSetsAsked.insert(session->AppId).second)
                    unread.push_back(session->AppId);
            }
            if (!unread.empty())
                m_cards.ReadGames(std::move(unread));
        }

        if (now >= m_nextCardFullRead)
        {
            m_nextCardFullRead = now + kCardFullReadSeconds;
            m_nextCardCheck = now + kCardCheckSeconds;
            m_cards.ReadAll();
        }
        else if (now >= m_nextCardCheck)
        {
            m_nextCardCheck = now + kCardCheckSeconds;
            std::vector<std::uint32_t> running;
            for (const auto& session : m_sessions)
            {
                if (session->Ready && !session->StopRequested)
                    running.push_back(session->AppId);
            }
            if (!running.empty())
                m_cards.ReadGames(std::move(running));
        }
    }

    CreatureFacts IdlerModel::CreatureFactsNow() const
    {
        CreatureFacts facts;
        facts.TotalHours = m_stats.Total(m_loadedAccount) / 3600.0;
        const std::vector<GameTotal> games = m_stats.ByGame(m_loadedAccount);
        facts.BestGameHours = games.empty() ? 0.0 : games.front().Seconds / 3600.0;
        for (const GameTotal& game : games)
            NoteGameTotal(facts, game.AppId, game.Seconds);
        facts.DaysInARow = CountDaysInARow(m_stats.PerDay(m_loadedAccount, m_today, kCreatureDaysLooked));

        if (m_pretendRunning)
        {
            for (const QueueEntry& entry : Queue())
            {
                if (!entry.Resting)
                    facts.RunningApps.push_back(entry.AppId);
            }
        }
        else
        {
            for (const auto& session : m_sessions)
            {
                if (session->Ready && !session->StopRequested)
                    facts.RunningApps.push_back(session->AppId);
            }
        }

        const std::time_t time = std::time(nullptr);
        std::tm local{};
        if (LocalTime(time, local))
        {
            facts.Hour = local.tm_hour;
            facts.Month = local.tm_mon + 1;
            facts.Day = local.tm_mday;
            facts.Weekday = local.tm_wday;
        }
        if (m_pretendMonth != 0)
        {
            facts.Month = m_pretendMonth;
            facts.Day = m_pretendDay;
        }
        facts.Goals = m_settings.CreatureGoals;
        facts.Breaks = m_settings.CreatureBreaks;
        facts.Pets = m_settings.CreaturePets;
        return facts;
    }

    void IdlerModel::RefreshCreatures(double now)
    {
        if (now < m_nextCreatureCheck)
            return;
        m_nextCreatureCheck = now + kCreatureCheckSeconds;

        const CreatureFacts facts = CreatureFactsNow();
        m_month = facts.Month;
        m_day = facts.Day;
        for (const CreatureInfo& info : AllCreatures())
        {
            if (!m_creatures.test(static_cast<std::size_t>(info.Id)) && IsEarned(info, facts))
                FindCreature(info);
        }
        RefreshBonds(now);
    }

    void IdlerModel::RefreshBonds(double now)
    {
        if (m_loadedAccount == 0 || (now < m_nextBondCheck && m_bondAccount == m_loadedAccount))
            return;
        m_nextBondCheck = now + kBondCheckSeconds;
        if (m_bondAccount != m_loadedAccount)
        {
            m_bondAccount = m_loadedAccount;
            m_bondsKnown = false;
        }

        std::array<double, kCreatureCount> seconds{};
        for (const GameTotal& game : m_stats.ByGame(m_loadedAccount))
            seconds[static_cast<std::size_t>(PickCreature(game.AppId, m_creatures))] += game.Seconds;
        std::array<std::uint32_t, kCreatureCount> pets{};
        for (const StoredCreature& stored : m_settings.Creatures)
        {
            if (const auto id = CreatureWithKey(stored.Key))
                pets[static_cast<std::size_t>(*id)] = stored.Pets;
        }
        std::array<std::uint32_t, kCreatureCount> played{};
        for (const StoredPlay& play : m_settings.CreaturePlays)
        {
            if (const auto id = CreatureWithKey(play.Key))
                played[static_cast<std::size_t>(*id)] = play.Record.Minutes;
        }
        for (std::size_t index = 0; index < kCreatureCount; ++index)
            m_bondHours[index] = BondHours(seconds[index], pets[index], played[index]);
        NoteBondLevels();
    }

    void IdlerModel::NoteBondLevels()
    {
        for (const CreatureInfo& info : AllCreatures())
        {
            const auto index = static_cast<std::size_t>(info.Id);
            if (!m_creatures.test(index))
                continue;
            const int level = BondLevel(m_bondHours[index]);
            if (m_bondsKnown && level > m_bondLevels[index])
            {
                m_bondNews.push_back(info.Id);
                LOG_SUCCESS(std::string("Bond with ") + info.Name + ": " + BondName(level));
            }
            m_bondLevels[index] = level;
            if (EvolvesAt(info.Id, level))
            {
                StoredCreature& stored = StoredFor(info.Id);
                if (!stored.Evolved)
                {
                    stored.Evolved = true;
                    ++m_evolutions[index];
                    m_evolutionNews.push_back(info.Id);
                    LOG_SUCCESS(std::string(info.Name) + " evolved into " + EvolutionOf(info.Id)->Name);
                    MarkDirty();
                }
            }
        }
        m_bondsKnown = true;
    }

    std::vector<CreatureId> IdlerModel::TakeBondNews()
    {
        return std::exchange(m_bondNews, {});
    }

    std::vector<CreatureId> IdlerModel::TakeEvolutionNews()
    {
        return std::exchange(m_evolutionNews, {});
    }

    const StoredCreature* IdlerModel::FindStored(CreatureId id) const
    {
        const std::string_view key = InfoOf(id).Key;
        const auto stored = std::find_if(m_settings.Creatures.begin(), m_settings.Creatures.end(), [&](const StoredCreature& known) { return known.Key == key; });
        return stored == m_settings.Creatures.end() ? nullptr : &*stored;
    }

    StoredCreature& IdlerModel::StoredFor(CreatureId id)
    {
        const std::string_view key = InfoOf(id).Key;
        const auto stored = std::find_if(m_settings.Creatures.begin(), m_settings.Creatures.end(), [&](const StoredCreature& known) { return known.Key == key; });
        if (stored != m_settings.Creatures.end())
            return *stored;
        return m_settings.Creatures.emplace_back(StoredCreature{ std::string(key), 0u, 0u });
    }

    bool IdlerModel::CreatureEvolved(CreatureId id) const
    {
        const StoredCreature* stored = FindStored(id);
        return stored && stored->Evolved && EvolutionOf(id);
    }

    bool IdlerModel::CreatureInFirstForm(CreatureId id) const
    {
        const StoredCreature* stored = FindStored(id);
        return stored && stored->FirstForm;
    }

    void IdlerModel::SetCreatureFirstForm(CreatureId id, bool first)
    {
        if (!CreatureEvolved(id))
            return;
        StoredCreature& stored = StoredFor(id);
        if (stored.FirstForm == first)
            return;
        stored.FirstForm = first;
        if (!first)
            ++m_evolutions[static_cast<std::size_t>(id)];
        LOG_INFO(std::string(InfoOf(id).Name) + (first ? ": its first form" : std::string(": ") + EvolutionOf(id)->Name));
        MarkDirty();
    }

    CreatureSet IdlerModel::EvolvedCreatures() const
    {
        CreatureSet evolved;
        for (const StoredCreature& stored : m_settings.Creatures)
        {
            if (!stored.Evolved || stored.FirstForm)
                continue;
            if (const auto id = CreatureWithKey(stored.Key); id && EvolutionOf(*id))
                evolved.set(static_cast<std::size_t>(*id));
        }
        return evolved;
    }

    std::vector<const CreatureFamily*> IdlerModel::TakeFinishedFamilies()
    {
        return std::exchange(m_finishedFamilies, {});
    }

    void IdlerModel::FindCreature(const CreatureInfo& info)
    {
        m_creatures.set(static_cast<std::size_t>(info.Id));
        m_newCreatures.set(static_cast<std::size_t>(info.Id));
        m_freshCreatures.push_back(info.Id);
        ++m_creatureNewsCount;
        m_creatureNewsName = info.Name;
        m_settings.Creatures.push_back({ info.Key, static_cast<std::uint32_t>(std::time(nullptr)) });
        LOG_SUCCESS(std::string("New creature: ") + info.Name + ". " + info.How);
        for (const CreatureFamily& family : AllFamilies())
        {
            if (IsInFamily(family, info.Id) && FoundInFamily(family, m_creatures) == static_cast<int>(family.Members.size()))
            {
                m_finishedFamilies.push_back(&family);
                LOG_SUCCESS(std::string("Set complete: ") + family.Name);
            }
        }
        MarkDirty();
    }

    std::uint32_t IdlerModel::CreatureFoundAt(CreatureId id) const
    {
        const std::string_view key = InfoOf(id).Key;
        const auto stored = std::find_if(m_settings.Creatures.begin(), m_settings.Creatures.end(), [&](const StoredCreature& known) { return known.Key == key; });
        return stored == m_settings.Creatures.end() ? 0 : stored->Found;
    }

    void IdlerModel::AwardCreature(CreatureId id)
    {
        if (!m_creatures.test(static_cast<std::size_t>(id)))
            FindCreature(InfoOf(id));
    }

    void IdlerModel::PetCreature(CreatureId id)
    {
        ++m_settings.CreaturePets;
        ++StoredFor(id).Pets;
        m_nextBondCheck = 0.0;
        RefreshBonds(Now());
        MarkDirty();
        m_nextCreatureCheck = 0.0;
    }

    const PlayRecord& IdlerModel::PlayOf(CreatureId id) const
    {
        static const PlayRecord kNone;
        const std::string_view key = InfoOf(id).Key;
        const auto found = std::find_if(m_settings.CreaturePlays.begin(), m_settings.CreaturePlays.end(), [&](const StoredPlay& play) { return play.Key == key; });
        return found == m_settings.CreaturePlays.end() ? kNone : found->Record;
    }

    std::uint32_t IdlerModel::PlayBondLeftToday(CreatureId id) const
    {
        return PlayBondLeft(PlayOf(id), m_today);
    }

    IdlerModel::PlayResult IdlerModel::FinishGame(CreatureId id, MiniGame game, int score)
    {
        const std::string key = InfoOf(id).Key;
        auto found = std::find_if(m_settings.CreaturePlays.begin(), m_settings.CreaturePlays.end(), [&](const StoredPlay& play) { return play.Key == key; });
        if (found == m_settings.CreaturePlays.end())
        {
            m_settings.CreaturePlays.push_back({ key, {} });
            found = std::prev(m_settings.CreaturePlays.end());
        }
        PlayRecord& record = found->Record;
        PlayResult result;
        const auto points = static_cast<std::uint32_t>(std::max(0, score));
        std::uint32_t& best = record.Best[static_cast<std::size_t>(game)];
        result.NewBest = points > best;
        best = std::max(best, points);
        ++record.Games;
        const std::uint32_t earned = PlayBondOf(game, score);
        result.Bond = GrantPlayBond(record, m_today, earned);
        result.DayFull = result.Bond < earned;
        LOG_INFO(std::string("Played ") + MiniGameName(game) + " with " + InfoOf(id).Name + ": " + std::to_string(score) + ", bond +" + std::to_string(result.Bond) + " min");
        m_nextBondCheck = 0.0;
        RefreshBonds(Now());
        MarkDirty();
        return result;
    }

    void IdlerModel::FetchedBall(CreatureId id)
    {
        const std::string key = InfoOf(id).Key;
        auto found = std::find_if(m_settings.CreaturePlays.begin(), m_settings.CreaturePlays.end(), [&](const StoredPlay& play) { return play.Key == key; });
        if (found == m_settings.CreaturePlays.end())
        {
            m_settings.CreaturePlays.push_back({ key, {} });
            found = std::prev(m_settings.CreaturePlays.end());
        }
        ++found->Record.Fetches;
        if (GrantPlayBond(found->Record, m_today, kFetchBond) > 0)
        {
            m_nextBondCheck = 0.0;
            RefreshBonds(Now());
        }
        MarkDirty();
    }

    void IdlerModel::SetBuddy(std::optional<CreatureId> buddy)
    {
        m_buddy = buddy;
        m_settings.CreatureBuddy = buddy ? InfoOf(*buddy).Key : "";
        LOG_INFO(std::string("At the account: ") + (buddy ? InfoOf(*buddy).Name : "its own creature"));
        MarkDirty();
    }

    std::vector<CreatureId> IdlerModel::TakeFreshCreatures()
    {
        return std::exchange(m_freshCreatures, {});
    }

    std::string IdlerModel::TakeCreatureNews()
    {
        const int count = std::exchange(m_creatureNewsCount, 0);
        if (count == 0)
            return {};
        return count == 1 ? "A new creature: " + m_creatureNewsName : std::to_string(count) + " new creatures.";
    }

    void IdlerModel::SetPretendDate(int month, int day)
    {
        m_pretendMonth = month;
        m_pretendDay = day;
    }

    void IdlerModel::SetAllCreaturesFound()
    {
        m_creatures.set();
    }

    void IdlerModel::SetSlots(int slots)
    {
        const int clamped = std::clamp(slots, 1, kMaxSlots);
        if (clamped != m_settings.Slots)
        {
            m_settings.Slots = clamped;
            MarkDirty();
        }
    }

    IdlerModel::Session* IdlerModel::FindSession(std::uint32_t appId) const
    {
        const auto session = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const auto& known) { return known->AppId == appId; });
        return session == m_sessions.end() ? nullptr : session->get();
    }

    RunState IdlerModel::StateOf(std::uint32_t appId) const
    {
        if (m_pretendRunning)
        {
            const auto& queue = Queue();
            const auto entry = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& known) { return known.AppId == appId; });
            if (entry == queue.end() || entry->Resting)
                return RunState::Off;
            return (entry - queue.begin()) % 5 == 4 ? RunState::Starting : RunState::Running;
        }
        const Session* session = FindSession(appId);
        if (!session)
            return RunState::Off;
        if (session->StopRequested)
            return RunState::Stopping;
        return session->Ready ? RunState::Running : RunState::Starting;
    }

    int IdlerModel::RunningCount() const
    {
        if (m_pretendRunning)
            return static_cast<int>(std::count_if(Queue().begin(), Queue().end(), [](const QueueEntry& entry) { return !entry.Resting; }));
        return static_cast<int>(std::count_if(m_sessions.begin(), m_sessions.end(), [](const auto& session) { return session->Ready && !session->StopRequested; }));
    }

    bool IdlerModel::IdlingOrOnBreak() const
    {
        return RunningCount() > 0 || (m_active && CurrentHold() == Hold::None && AllOnBreak(Queue()));
    }

    QueueForecast IdlerModel::Forecast() const
    {
        return ForecastQueue(Queue(), m_settings.Slots);
    }

    std::vector<WindowMessage> IdlerModel::TakeWindowMessages()
    {
        if (m_pretendMessages && !Queue().empty())
        {
            m_pretendMessages = false;
            m_windowMessages.push_back({ WindowMessage::Kind::GoalReached, Queue().front().AppId });
            m_windowMessages.push_back({ WindowMessage::Kind::QueueFinished, 0 });
        }
        return std::exchange(m_windowMessages, {});
    }

    bool IdlerModel::TakeQueueFinished()
    {
        return std::exchange(m_queueFinished, false);
    }

    void IdlerModel::HandleWorkerLine(Session& session, const std::string& line, double now)
    {
        if (line == "ready")
        {
            if (!session.Ready)
                m_stats.AddSession(m_loadedAccount, m_today, session.AppId);
            session.Ready = true;
            LOG_SUCCESS("Idling: " + GameText(session.AppId));
            if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), session.AppId))
                entry->Crashes = 0;
            return;
        }
        if (line == "gone")
        {
            session.ReportedError = true;
            LOG_WARN(GameText(session.AppId) + ": " + ErrorMessage(ErrorCode::SteamWentAway));
            return;
        }
        if (line.starts_with("error steam"))
        {
            session.ReportedError = true;
            LOG_ERROR(GameText(session.AppId) + ": " + std::string(Trim(std::string_view(line).substr(11))));
            m_steamRetryAt = now + kSteamRetrySeconds;
            return;
        }
        if (line.starts_with("error app"))
        {
            session.ReportedError = true;
            LOG_ERROR(GameText(session.AppId) + ": " + std::string(Trim(std::string_view(line).substr(9))));
            if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), session.AppId))
            {
                entry->Failed = true;
                entry->Error = std::string(Trim(std::string_view(line).substr(9)));
            }
        }
    }

    void IdlerModel::PollSessions(double now, double elapsed)
    {
        bool anyRunning = false;
        for (auto it = m_sessions.begin(); it != m_sessions.end();)
        {
            Session& session = **it;
            std::string line;
            while (session.Worker.ReadLine(line))
                HandleWorkerLine(session, line, now);

            if (session.Ready && !session.StopRequested)
            {
                if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), session.AppId))
                {
                    const bool wasFinished = IsFinished(*entry);
                    entry->IdledSeconds += elapsed;
                    if (!wasFinished && IsFinished(*entry))
                    {
                        LOG_SUCCESS("Goal reached: " + GameText(session.AppId));
                        ++m_settings.CreatureGoals;
                        m_windowMessages.push_back({ WindowMessage::Kind::GoalReached, session.AppId });
                    }
                }
                m_stats.Add(m_loadedAccount, m_today, session.AppId, elapsed);
                session.RunSeconds += elapsed;
                m_stats.NoteSessionLength(m_loadedAccount, m_today, session.AppId, session.RunSeconds);
                m_stats.AddHour(m_loadedAccount, m_hour, elapsed);
                anyRunning = true;
                MarkDirty();
            }

            if (session.Worker.Running())
            {
                if (session.StopRequested && now - session.StopRequestedAt > kStopGraceSeconds)
                {
                    LOG_WARN(GameText(session.AppId) + ": " + ErrorMessage(ErrorCode::DidNotEndInTime));
                    session.Worker.Terminate();
                }
                ++it;
                continue;
            }

            while (session.Worker.ReadLine(line))
                HandleWorkerLine(session, line, now);
            if (!session.StopRequested && !session.ReportedError)
            {
                m_steamRetryAt = now + kSteamRetrySeconds;
                LOG_WARN(GameText(session.AppId) + ": " + ErrorMessage(ErrorCode::StoppedByItself));
                if (QueueEntry* entry = Idler::FindEntry(CurrentQueue(), session.AppId); entry && ++entry->Crashes >= kMaxCrashes)
                {
                    entry->Failed = true;
                    entry->Error = ErrorMessage(ErrorCode::KeptStopping);
                    LOG_ERROR(GameText(session.AppId) + ": " + entry->Error);
                }
            }
            else if (session.StopRequested)
            {
                LOG_INFO("Stopped: " + GameText(session.AppId));
            }
            if (session.Ready)
                m_stoppedAt[session.AppId] = now;
            it = m_sessions.erase(it);
        }
        if (anyRunning)
            m_sessionSeconds += elapsed;
    }

    void IdlerModel::StopSession(Session& session, double now)
    {
        if (session.StopRequested)
            return;
        session.StopRequested = true;
        session.StopRequestedAt = now;
        session.Worker.CloseInput();
    }

    void IdlerModel::PretendPhase(double now)
    {
        constexpr double kStartSeconds = 4.0;
        constexpr double kStopSeconds = 3.0;
        const int all = std::min(static_cast<int>(Queue().size()), m_settings.Slots);
        const double since = now - m_pretendSwitchedAt;
        m_wantedCount = m_active ? all : 0;
        if (m_active)
        {
            m_readyCount = std::min(all, static_cast<int>(static_cast<double>(all) * since / kStartSeconds));
            m_pretendAlive = all;
            m_startingUp = m_readyCount < all;
        }
        else
        {
            m_readyCount = 0;
            m_pretendAlive = since < kStopSeconds ? std::max(0, all - static_cast<int>(static_cast<double>(all) * since / kStopSeconds)) : 0;
            m_startingUp = false;
        }
    }

    void IdlerModel::Schedule(double now)
    {
        if (m_pretendRunning)
        {
            PretendPhase(now);
            return;
        }

        const bool libraryKnown = !m_games.empty() || (m_scannedOnce && !m_scanning);

        const bool cards = QueueFarmsCards();
        const std::uint32_t shown = cards ? 0 : m_shownApp;

        const Hold hold = m_active ? CurrentHold() : Hold::None;
        if (hold != m_loggedHold)
        {
            m_loggedHold = hold;
            switch (hold)
            {
            case Hold::Steam: LOG_WARN("Paused: Steam is not running, or nobody is logged on"); break;
            case Hold::Playing: LOG_INFO("Paused: another game runs, " + GameText(m_playingApp)); break;
            case Hold::Hours: LOG_INFO("Paused: outside the hours set for idling"); break;
            case Hold::Away: LOG_INFO("Paused: you are at the computer"); break;
            default:
                if (m_active)
                    LOG_INFO("Queue goes on");
                break;
            }
        }

        std::vector<std::uint32_t> wanted;
        if (m_active && libraryKnown && CurrentHold() == Hold::None)
        {
            wanted = cards ? WantedForCards(Queue(), static_cast<double>(m_settings.CardWarmupHours) * 3600.0)
                           : WantedWithShown(Queue(), m_settings.Slots, shown);
        }

        for (const auto& session : m_sessions)
        {
            if (std::find(wanted.begin(), wanted.end(), session->AppId) == wanted.end())
                StopSession(*session, now);
        }

        const auto ready = [&](std::uint32_t appId)
            {
                const Session* session = FindSession(appId);
                return session && session->Ready && !session->StopRequested;
            };

        m_wantedCount = static_cast<int>(wanted.size());
        m_readyCount = static_cast<int>(std::count_if(wanted.begin(), wanted.end(), ready));
        if (hold != Hold::None)
            m_startingUp = true;
        else if (m_readyCount == m_wantedCount && libraryKnown)
            m_startingUp = false;

        const bool othersReady = std::all_of(wanted.begin(), wanted.end(), [&](std::uint32_t appId) { return appId == shown || ready(appId); });

        if (now >= m_nextStart && now >= m_steamRetryAt)
        {
            for (const std::uint32_t appId : wanted)
            {
                if (FindSession(appId))
                    continue;
                if (appId == shown && !othersReady)
                    continue;
                auto session = std::make_unique<Session>();
                session->AppId = appId;
                if (session->Worker.Start(L"--idle=" + std::to_wstring(appId)))
                {
                    LOG_INFO("Starting: " + GameText(appId));
                    m_sessions.push_back(std::move(session));
                    (appId == shown ? m_shownStartedAt : m_otherStartedAt) = now;
                }
                else
                {
                    LOG_ERROR(GameText(appId) + ": " + ErrorMessage(ErrorCode::HelperNotStarted));
                    m_steamRetryAt = now + kSteamRetrySeconds;
                }
                m_nextStart = now + kSecondsBetweenStarts;
                break;
            }
        }

        if (shown != 0 && othersReady && ready(shown) && m_otherStartedAt > m_shownStartedAt && now - m_shownStartedAt >= kShownRestartSeconds &&
            std::find(wanted.begin(), wanted.end(), shown) != wanted.end())
        {
            LOG_INFO("Starts once more to stay the game friends see: " + GameText(shown));
            StopSession(*FindSession(shown), now);
        }

        if (m_active && libraryKnown && !Queue().empty() && m_sessions.empty() && !HasWorkLeft(Queue()))
        {
            m_active = false;
            m_undo.EndedIdling = m_undo.Any();
            m_queueFinished = std::any_of(Queue().begin(), Queue().end(), [](const QueueEntry& entry) { return IsFinished(entry); });
            if (m_queueFinished)
            {
                LOG_SUCCESS("Queue finished");
                m_windowMessages.push_back({ WindowMessage::Kind::QueueFinished, 0 });
            }
            else
            {
                LOG_WARN("Queue ended: nothing left that can run");
            }
            if (m_queueFinished && m_shutdownArmed)
            {
                LOG_WARN("Shutting down in " + std::to_string(static_cast<int>(kShutdownCountdownSeconds)) + " s");
                m_shutdownAt = now + kShutdownCountdownSeconds;
                m_wantsWindow = true;
            }
            MarkDirty();
        }
    }

    void IdlerModel::UpdateKeepAwake(bool force)
    {
        const bool wanted = !force && !m_pretendRunning && m_settings.KeepAwake && IdlingOrOnBreak();
        if (wanted == m_keepingAwake)
            return;
        m_keepingAwake = wanted;
        LOG_INFO(wanted ? "Keeps the computer awake" : "No longer keeps the computer awake");
        Desktop::KeepAwake(wanted);
    }

    void IdlerModel::UpdateInvisible(double now)
    {
        const bool wanted = !m_pretendRunning && m_settings.InvisibleWhileIdling && IdlingOrOnBreak();
        if (wanted)
        {
            m_lastRunningAt = now;
            if (!m_settings.SetInvisible)
            {
                LOG_INFO("Steam status: invisible");
                Desktop::SetSteamStatus(L"invisible");
                m_settings.SetInvisible = true;
                SaveNow();
            }
            return;
        }
        if (m_settings.SetInvisible && m_steam.LoggedOn() && now - m_lastRunningAt > kBackOnlineSeconds)
        {
            LOG_INFO("Steam status: online");
            Desktop::SetSteamStatus(L"online");
            m_settings.SetInvisible = false;
            SaveNow();
        }
    }

    double IdlerModel::ShutdownSecondsLeft() const
    {
        return m_shutdownAt < 0.0 ? -1.0 : std::max(0.0, m_shutdownAt - Now());
    }

    void IdlerModel::CancelShutdown()
    {
        if (m_shutdownAt >= 0.0)
            LOG_INFO("Shutdown called off");
        m_shutdownAt = -1.0;
        m_shutdownArmed = false;
    }

    bool IdlerModel::TakeWantsWindow()
    {
        return std::exchange(m_wantsWindow, false);
    }

    void IdlerModel::UpdateShutdown(double now)
    {
        if (m_shutdownAt < 0.0 || now < m_shutdownAt)
            return;
        m_shutdownAt = -1.0;
        m_shutdownArmed = false;
        SaveNow();
        LOG_WARN("Shutting down now");
        m_shutdownFailed = !Desktop::ShutDown();
        if (m_shutdownFailed)
            LOG_ERROR(ErrorMessage(ErrorCode::ShutdownFailed));
    }

    void IdlerModel::LoadAchievements(std::uint32_t appId)
    {
        CloseAchievements();
        m_achievementApp = appId;
        if (m_pretendRunning)
        {
            for (const AchievementIcons& entry : Steam::ReadAchievementIcons(m_steam, appId))
            {
                AchievementRow row;
                row.Id = entry.Id;
                row.Name = entry.Id;
                row.Description = "A description from a check.";
                row.Achieved = m_achievements.size() % 2 == 0;
                row.UnlockTime = row.Achieved ? 1700000000u : 0u;
                m_achievements.push_back(std::move(row));
            }
            for (const StatSchema& schema : Steam::ReadStatSchema(m_steam, appId))
            {
                StatRow stat;
                stat.Schema = schema;
                const double step = static_cast<double>(m_gameStats.size() * 7 + 3);
                stat.Value = schema.Kind == StatKind::Int ? step : step * 0.75;
                if (schema.Min)
                    stat.Value = std::max(stat.Value, *schema.Min);
                if (schema.Max)
                    stat.Value = m_gameStats.size() % 3 == 0 ? *schema.Max : std::min(stat.Value, *schema.Max);
                m_gameStats.push_back(std::move(stat));
            }
            m_achievementPhase = AchievementPhase::Ready;
            FinishAchievements();
            return;
        }
        m_achievementWorker = std::make_unique<WorkerProcess>();
        LOG_INFO("Reading achievements: " + GameText(appId));
        if (!m_achievementWorker->Start(L"--achievements=" + std::to_wstring(appId)))
        {
            m_achievementWorker.reset();
            m_achievementPhase = AchievementPhase::Failed;
            m_achievementProblem = ErrorMessage(ErrorCode::ListHelperNotStarted);
            LOG_ERROR(GameText(appId) + ": " + m_achievementProblem);
            return;
        }
        m_achievementWorker->CloseInput();
        m_achievementPhase = AchievementPhase::Busy;
        m_achievementDeadline = Now() + kAchievementsTimeoutSeconds;
    }

    std::filesystem::path IdlerModel::AchievementIconFile(const std::string& icon) const
    {
        return m_iconFolder / std::to_wstring(m_achievementApp) / Utf8ToWide(icon);
    }

    void IdlerModel::StopAchievementIcons()
    {
        {
            std::lock_guard lock(m_iconLock);
            m_iconStop = true;
            m_iconsWanted.clear();
        }
        m_iconWake.notify_all();
        if (m_iconThread.joinable())
            m_iconThread.join();
    }

    void IdlerModel::FetchAchievementIcons(std::uint32_t appId, std::vector<std::string> icons)
    {
        {
            std::lock_guard lock(m_iconLock);
            if (m_iconStop)
                return;
            m_iconApp = appId;
            m_iconsWanted = std::move(icons);
            if (m_iconsWanted.empty())
                return;
        }
        if (!m_iconThread.joinable())
            m_iconThread = std::thread([this] { AchievementIconLoop(); });
        m_iconWake.notify_all();
    }

    void IdlerModel::AchievementIconLoop()
    {
        constexpr int kFailuresToGiveUp = 3;
        int failures = 0;
        std::unique_lock lock(m_iconLock);
        while (!m_iconStop)
        {
            if (m_iconsWanted.empty())
            {
                failures = 0;
                m_iconWake.wait(lock);
                continue;
            }
            const std::uint32_t appId = m_iconApp;
            const std::string icon = std::move(m_iconsWanted.back());
            m_iconsWanted.pop_back();
            const std::filesystem::path folder = m_iconFolder / std::to_wstring(appId);
            const std::filesystem::path file = folder / Utf8ToWide(icon);
            lock.unlock();

            std::error_code error;
            bool arrived = false;
            if (!std::filesystem::exists(file, error))
            {
                std::string bytes;
                arrived = SteamWeb::ReadPicture(AchievementIconAddress(appId, icon), bytes);
                if (arrived)
                {
                    std::filesystem::create_directories(folder, error);
                    std::ofstream out(file, std::ios::binary | std::ios::trunc);
                    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                    arrived = static_cast<bool>(out);
                }
                failures = arrived ? 0 : failures + 1;
                if (arrived)
                    ++m_iconGeneration;
            }

            lock.lock();
            if (failures >= kFailuresToGiveUp)
            {
                m_iconsWanted.clear();
                LOG_WARN(ErrorMessage(ErrorCode::PicturesNotFetched));
            }
        }
    }

    void IdlerModel::CloseAchievements()
    {
        FetchAchievementIcons(0, {});
        m_achievementWorker.reset();
        m_achievementWorkerChanges = false;
        m_achievementApp = 0;
        m_achievementPhase = AchievementPhase::Idle;
        m_achievements.clear();
        m_achievementProblem.clear();
        m_achievementsUnlocking.clear();
        m_achievementsLocking.clear();
        m_achievementsIncoming.clear();
        m_gameStats.clear();
        m_gameStatsIncoming.clear();
        m_statSetting.clear();
        m_undoUnlocks.clear();
        m_achievementNotice.clear();
        m_achievementNoticeProblem = false;
    }

    const StatRow* IdlerModel::FindStatRow(std::string_view name) const
    {
        const auto row = std::find_if(m_gameStats.begin(), m_gameStats.end(), [&](const StatRow& each) { return each.Schema.Name == name; });
        return row == m_gameStats.end() ? nullptr : &*row;
    }

    void IdlerModel::SetGameStat(const std::string& name, double value)
    {
        if (m_achievementPhase != AchievementPhase::Ready)
            return;
        const StatRow* row = FindStatRow(name);
        if (!row || !row->Settable || !StatChangeProblem(row->Schema, row->Value, value).empty())
            return;
        if (row->Schema.Kind == StatKind::Int)
            value = std::round(value);
        if (NumberText(value, row->Schema.Kind) == NumberText(row->Value, row->Schema.Kind))
            return;

        m_achievementNotice.clear();
        m_achievementNoticeProblem = false;
        m_undoUnlocks.clear();
        m_achievementChangeUndoable = false;
        m_achievementsUnlocking.clear();
        m_achievementsLocking.clear();
        m_achievementsIncoming.clear();
        m_gameStatsIncoming.clear();
        m_statSetting = name;
        m_statSettingTo = value;

        if (m_pretendRunning)
        {
            m_gameStatsIncoming = m_gameStats;
            for (StatRow& stat : m_gameStatsIncoming)
            {
                if (stat.Schema.Name == name)
                    stat.Value = value;
            }
            m_achievementsIncoming = m_achievements;
            const auto now = static_cast<std::uint32_t>(std::time(nullptr));
            for (AchievementRow& achievement : m_achievementsIncoming)
            {
                if (!achievement.Achieved && achievement.ProgressStat == name && achievement.ProgressMax > achievement.ProgressMin && value >= achievement.ProgressMax)
                {
                    achievement.Achieved = true;
                    achievement.UnlockTime = now;
                }
            }
            m_achievementPhase = AchievementPhase::Writing;
            FinishAchievementChange();
            return;
        }

        m_achievementWorker = std::make_unique<WorkerProcess>();
        LOG_INFO("Setting a stat, " + GameText(m_achievementApp) + ": " + name + " to " + NumberText(value, row->Schema.Kind));
        AchievementChange change;
        change.Id = name;
        change.SetsStat = true;
        change.Value = value;
        if (!m_achievementWorker->Start(L"--change-achievements=" + std::to_wstring(m_achievementApp)) || !m_achievementWorker->Write(WriteAchievementChanges({ change })))
        {
            m_achievementWorker.reset();
            m_statSetting.clear();
            m_achievementNotice = ErrorMessage(ErrorCode::ChangeNotSent);
            m_achievementNoticeProblem = true;
            LOG_ERROR(GameText(m_achievementApp) + ": " + m_achievementNotice);
            return;
        }
        m_achievementWorker->CloseInput();
        m_achievementWorkerChanges = true;
        m_achievementPhase = AchievementPhase::Writing;
        m_achievementDeadline = Now() + kAchievementsTimeoutSeconds;
    }

    void IdlerModel::ChangeAchievements(std::vector<std::string> unlock, std::vector<std::string> lock, bool undoable)
    {
        if (m_achievementPhase != AchievementPhase::Ready)
            return;
        const auto keep = [&](std::vector<std::string>& ids, bool wanted)
            {
                std::erase_if(ids, [&](const std::string& id)
                    {
                        const auto row = std::find_if(m_achievements.begin(), m_achievements.end(), [&](const AchievementRow& each) { return each.Id == id; });
                        return row == m_achievements.end() || row->Access != AchievementAccess::Here || row->Achieved == wanted;
                    });
            };
        keep(unlock, true);
        keep(lock, false);
        if (unlock.empty() && lock.empty())
            return;

        m_achievementNotice.clear();
        m_achievementNoticeProblem = false;
        m_undoUnlocks.clear();
        m_achievementChangeUndoable = undoable;
        m_achievementsUnlocking = std::move(unlock);
        m_achievementsLocking = std::move(lock);
        m_achievementsIncoming.clear();
        m_gameStatsIncoming.clear();
        m_statSetting.clear();

        if (m_pretendRunning)
        {
            m_achievementsIncoming = m_achievements;
            const auto now = static_cast<std::uint32_t>(std::time(nullptr));
            for (AchievementRow& row : m_achievementsIncoming)
            {
                if (std::find(m_achievementsUnlocking.begin(), m_achievementsUnlocking.end(), row.Id) != m_achievementsUnlocking.end())
                {
                    row.Achieved = true;
                    row.UnlockTime = now;
                }
                if (std::find(m_achievementsLocking.begin(), m_achievementsLocking.end(), row.Id) != m_achievementsLocking.end())
                {
                    row.Achieved = false;
                    row.UnlockTime = 0;
                }
            }
            m_achievementPhase = AchievementPhase::Writing;
            FinishAchievementChange();
            return;
        }

        std::vector<AchievementChange> changes;
        for (const std::string& id : m_achievementsUnlocking)
            changes.push_back({ id, true });
        for (const std::string& id : m_achievementsLocking)
            changes.push_back({ id, false });

        m_achievementWorker = std::make_unique<WorkerProcess>();
        LOG_INFO("Changing achievements, " + GameText(m_achievementApp) + ": " + std::to_string(m_achievementsUnlocking.size()) + " to unlock, " +
            std::to_string(m_achievementsLocking.size()) + " to lock");
        if (!m_achievementWorker->Start(L"--change-achievements=" + std::to_wstring(m_achievementApp)) || !m_achievementWorker->Write(WriteAchievementChanges(changes)))
        {
            m_achievementWorker.reset();
            m_achievementNotice = ErrorMessage(ErrorCode::ChangeNotSent);
            m_achievementNoticeProblem = true;
            LOG_ERROR(GameText(m_achievementApp) + ": " + m_achievementNotice);
            return;
        }
        m_achievementWorker->CloseInput();
        m_achievementWorkerChanges = true;
        m_achievementPhase = AchievementPhase::Writing;
        m_achievementDeadline = Now() + kAchievementsTimeoutSeconds;
    }

    void IdlerModel::UndoUnlocks()
    {
        std::vector<std::string> unlocked = std::move(m_undoUnlocks);
        m_undoUnlocks.clear();
        ChangeAchievements({}, std::move(unlocked));
    }

    void IdlerModel::FinishAchievementChange()
    {
        m_achievementPhase = AchievementPhase::Ready;
        std::size_t unlockedByStat = 0;
        for (AchievementRow& row : m_achievements)
        {
            const auto fresh = std::find_if(m_achievementsIncoming.begin(), m_achievementsIncoming.end(), [&](const AchievementRow& each) { return each.Id == row.Id; });
            if (fresh == m_achievementsIncoming.end())
                continue;
            if (fresh->Achieved && !row.Achieved)
                ++unlockedByStat;
            row.Achieved = fresh->Achieved;
            row.UnlockTime = fresh->UnlockTime;
        }
        m_achievementsIncoming.clear();
        for (StatRow& row : m_gameStats)
        {
            const auto fresh = std::find_if(m_gameStatsIncoming.begin(), m_gameStatsIncoming.end(), [&](const StatRow& each) { return each.Schema.Name == row.Schema.Name; });
            if (fresh != m_gameStatsIncoming.end())
                row.Value = fresh->Value;
        }
        m_gameStatsIncoming.clear();

        if (!m_statSetting.empty())
        {
            const StatRow* stat = FindStatRow(m_statSetting);
            const bool taken = stat && NumberText(stat->Value, stat->Schema.Kind) == NumberText(m_statSettingTo, stat->Schema.Kind);
            const std::string label = stat && !stat->Schema.Label.empty() ? stat->Schema.Label : m_statSetting;
            if (taken)
            {
                m_achievementNotice = label + " set to " + NumberText(stat->Value, stat->Schema.Kind) + ".";
                if (unlockedByStat > 0)
                    m_achievementNotice += " Steam unlocked " + std::to_string(unlockedByStat) + (unlockedByStat == 1 ? " achievement" : " achievements") + " with it.";
                m_achievementNoticeProblem = false;
                LOG_SUCCESS("Stat set, " + GameText(m_achievementApp) + ": " + m_achievementNotice);
            }
            else
            {
                m_achievementNotice = ErrorMessage(ErrorCode::StatNotTaken, label);
                m_achievementNoticeProblem = true;
                LOG_WARN(GameText(m_achievementApp) + ": " + ErrorMessage(ErrorCode::StatNotTaken, m_statSetting));
            }
            m_statSetting.clear();
            ReadAchievementSchema();
            CountAchievementsOfGame();
            return;
        }

        const auto stands = [&](const std::string& id, bool achieved)
            {
                const auto row = std::find_if(m_achievements.begin(), m_achievements.end(), [&](const AchievementRow& each) { return each.Id == id; });
                return row != m_achievements.end() && row->Achieved == achieved;
            };
        std::vector<std::string> unlocked;
        for (const std::string& id : m_achievementsUnlocking)
        {
            if (stands(id, true))
                unlocked.push_back(id);
        }
        const auto locked = static_cast<std::size_t>(std::count_if(m_achievementsLocking.begin(), m_achievementsLocking.end(), [&](const std::string& id) { return stands(id, false); }));
        const std::size_t refused = m_achievementsUnlocking.size() + m_achievementsLocking.size() - unlocked.size() - locked;
        m_achievementNotice = AchievementChangeText(unlocked.size(), locked, 0);
        if (refused > 0)
            m_achievementNotice += (m_achievementNotice.empty() ? "" : " ") + ErrorMessage(ErrorCode::ChangesRefused, std::to_string(refused));
        m_achievementNoticeProblem = refused > 0;
        if (refused > 0)
            LOG_WARN("Achievements changed, " + GameText(m_achievementApp) + ": " + m_achievementNotice);
        else
            LOG_SUCCESS("Achievements changed, " + GameText(m_achievementApp) + ": " + m_achievementNotice);
        if (m_achievementChangeUndoable)
            m_undoUnlocks = std::move(unlocked);
        m_achievementsUnlocking.clear();
        m_achievementsLocking.clear();

        ReadAchievementSchema();
        CountAchievementsOfGame();
    }

    void IdlerModel::HandleAchievementLine(const std::string& line)
    {
        const bool writing = m_achievementPhase == AchievementPhase::Writing;
        if (line == "done")
        {
            if (writing)
                FinishAchievementChange();
            else
                m_achievementPhase = AchievementPhase::Ready;
            return;
        }
        if (line.starts_with("error"))
        {
            std::istringstream parts(line);
            std::string word;
            std::string problem;
            parts >> word >> word;
            std::getline(parts, problem);
            problem = std::string(Trim(problem));
            if (writing)
            {
                m_achievementPhase = AchievementPhase::Ready;
                m_achievementNotice = problem;
                m_achievementNoticeProblem = true;
                m_achievementsIncoming.clear();
                m_gameStatsIncoming.clear();
                m_statSetting.clear();
                LOG_WARN("Achievements not changed, " + GameText(m_achievementApp) + ": " + problem);
                return;
            }
            m_achievementProblem = problem;
            m_achievementPhase = AchievementPhase::Failed;
            LOG_WARN("Achievements not read, " + GameText(m_achievementApp) + ": " + m_achievementProblem);
            return;
        }
        if (line.starts_with("refused "))
        {
            LOG_WARN("Steam did not take " + line.substr(8) + ", " + GameText(m_achievementApp) + " (" + ErrorName(ErrorCode::ChangesRefused) + ")");
            return;
        }
        if (line.starts_with("stored ") && line != "stored 1")
        {
            LOG_WARN(GameText(m_achievementApp) + ": " + ErrorMessage(ErrorCode::StoredWithResult, line.substr(7)));
            return;
        }
        if (line.starts_with("stat "))
        {
            const std::string_view rest = std::string_view(line).substr(5);
            const auto space = rest.find(' ');
            if (space == std::string_view::npos)
                return;
            const std::optional<double> value = ParseNumber(rest.substr(0, space));
            const std::string_view name = Trim(rest.substr(space + 1));
            if (!value || name.empty())
                return;
            StatRow row;
            row.Schema.Name = std::string(name);
            row.Value = *value;
            (writing ? m_gameStatsIncoming : m_gameStats).push_back(std::move(row));
            return;
        }
        if (!line.starts_with("ach "))
            return;

        std::istringstream parts(line.substr(4));
        int achieved = 0;
        int hidden = 0;
        AchievementRow row;
        parts >> achieved >> row.UnlockTime >> hidden;
        std::string rest;
        std::getline(parts, rest);
        const std::string_view text = Trim(rest);
        const auto firstTab = text.find('\t');
        const auto secondTab = firstTab == std::string_view::npos ? std::string_view::npos : text.find('\t', firstTab + 1);
        if (secondTab == std::string_view::npos)
            return;
        row.Id = text.substr(0, firstTab);
        row.Name = text.substr(firstTab + 1, secondTab - firstTab - 1);
        row.Description = text.substr(secondTab + 1);
        row.Achieved = achieved != 0;
        row.Hidden = hidden != 0;
        m_fontNeeds |= FontNeedsOf(row.Name) | FontNeedsOf(row.Description);
        (writing ? m_achievementsIncoming : m_achievements).push_back(std::move(row));
    }

    void IdlerModel::FinishAchievements()
    {
        m_achievementWorker.reset();
        m_stoppedAt[m_achievementApp] = Now();
        if (m_achievementPhase != AchievementPhase::Ready)
            return;

        LOG_SUCCESS("Achievements read, " + GameText(m_achievementApp) + ": " + std::to_string(m_achievements.size()));
        std::stable_sort(m_achievements.begin(), m_achievements.end(), [](const AchievementRow& lhs, const AchievementRow& rhs)
            {
                if (lhs.Achieved != rhs.Achieved)
                    return lhs.Achieved;
                return lhs.Achieved && lhs.UnlockTime > rhs.UnlockTime;
            });
        ReadAchievementSchema();
        CountAchievementsOfGame();
    }

    void IdlerModel::ReadAchievementSchema()
    {
        const std::vector<AchievementIcons> icons = Steam::ReadAchievementIcons(m_steam, m_achievementApp);
        std::vector<std::string> wanted;
        for (AchievementRow& row : m_achievements)
        {
            const auto known = std::find_if(icons.begin(), icons.end(), [&](const AchievementIcons& entry) { return entry.Id == row.Id; });
            row.Access = known == icons.end() ? AchievementAccess::Unknown : MaySetAchievement(row.Id, icons) ? AchievementAccess::Here : AchievementAccess::Server;
            row.Icon.clear();
            row.ProgressStat = known == icons.end() ? std::string() : known->ProgressStat;
            row.ProgressMin = known == icons.end() ? 0.0 : known->ProgressMin;
            row.ProgressMax = known == icons.end() ? 0.0 : known->ProgressMax;
            if (known == icons.end() || (row.Hidden && !row.Achieved))
                continue;
            const std::string& icon = row.Achieved || known->IconLocked.empty() ? known->Icon : known->IconLocked;
            if (AchievementIconAddress(m_achievementApp, icon).empty())
                continue;
            row.Icon = icon;
            wanted.push_back(icon);
        }
        std::reverse(wanted.begin(), wanted.end());
        FetchAchievementIcons(m_achievementApp, std::move(wanted));

        std::vector<StatRow> stats;
        for (const StatSchema& schema : Steam::ReadStatSchema(m_steam, m_achievementApp))
        {
            const auto sent = std::find_if(m_gameStats.begin(), m_gameStats.end(), [&](const StatRow& each) { return each.Schema.Name == schema.Name; });
            if (sent == m_gameStats.end())
                continue;
            StatRow row;
            row.Schema = schema;
            row.Value = sent->Value;
            row.Settable = MaySetStat(schema);
            stats.push_back(std::move(row));
        }
        m_gameStats = std::move(stats);

        for (Game& game : m_games)
        {
            if (game.AppId == m_achievementApp)
                game.Unlock = UnlockRuleOf(icons);
        }
    }

    void IdlerModel::CountAchievementsOfGame()
    {
        for (Game& game : m_games)
        {
            if (game.AppId != m_achievementApp)
                continue;
            game.AchievementsTotal = static_cast<std::uint32_t>(m_achievements.size());
            game.AchievementsDone = static_cast<std::uint32_t>(std::count_if(m_achievements.begin(), m_achievements.end(), [](const AchievementRow& row) { return row.Achieved; }));
        }
    }

    void IdlerModel::PollAchievements(double now)
    {
        if (!m_achievementWorker)
            return;

        const auto asking = [this]() { return m_achievementPhase == AchievementPhase::Busy || m_achievementPhase == AchievementPhase::Writing; };
        std::string line;
        while (asking() && m_achievementWorker->ReadLine(line))
            HandleAchievementLine(line);

        if (asking())
        {
            if (m_achievementWorker->Running() && now < m_achievementDeadline)
                return;
            while (asking() && m_achievementWorker->ReadLine(line))
                HandleAchievementLine(line);
            if (m_achievementPhase == AchievementPhase::Writing)
            {
                m_achievementWorker->Terminate();
                m_achievementPhase = AchievementPhase::Ready;
                m_achievementNotice = ErrorMessage(ErrorCode::ChangeNoAnswer);
                m_achievementNoticeProblem = true;
                m_achievementsIncoming.clear();
                m_gameStatsIncoming.clear();
                m_statSetting.clear();
                LOG_WARN("Achievements not changed, " + GameText(m_achievementApp) + ": " + m_achievementNotice);
            }
            else if (m_achievementPhase == AchievementPhase::Busy)
            {
                m_achievementWorker->Terminate();
                m_achievementPhase = AchievementPhase::Failed;
                m_achievementProblem = ErrorMessage(ErrorCode::ListNoAnswer);
                LOG_WARN("Achievements not read, " + GameText(m_achievementApp) + ": " + m_achievementProblem);
            }
        }
        if (m_achievementWorkerChanges)
        {
            m_achievementWorker.reset();
            m_achievementWorkerChanges = false;
            m_stoppedAt[m_achievementApp] = Now();
            return;
        }
        FinishAchievements();
    }
}
