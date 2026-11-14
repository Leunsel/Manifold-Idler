#include "App/IdlerModel.h"

#include "App/Desktop.h"
#include "App/Model/ModelParts.h"
#include "Core/CrashReport.h"
#include "Core/Errors.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Core/Version.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <thread>
#include <utility>

namespace Idler
{
    namespace ModelParts
    {
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

    using namespace ModelParts;

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
            RefreshUpdates(now);
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

}
