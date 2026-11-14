#include "App/IdlerModel.h"

#include "App/Model/ModelParts.h"
#include "Core/Errors.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Core/WebPolicy.h"
#include "Steam/SteamWeb.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace Idler
{
    using namespace ModelParts;

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
