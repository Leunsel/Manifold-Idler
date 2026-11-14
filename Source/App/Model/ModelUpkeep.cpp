#include "App/IdlerModel.h"

#include "App/Model/ModelParts.h"
#include "App/ReleaseCheck.h"
#include "Core/Errors.h"
#include "Core/Releases.h"
#include "Core/RuntimeLog.h"
#include "Core/Version.h"

#include <ctime>
#include <thread>
#include <utility>

namespace Idler
{
    using namespace ModelParts;

    void IdlerModel::BackUpDaily(std::int32_t today)
    {
        m_backupDay = today;
        const Backups::Result result = Backups::Make(m_settingsPath, Backups::BackupKind::Daily, std::time(nullptr));
        if (result.What == Backups::Outcome::Made)
            LOG_INFO("Backed up the settings and statistics as " + PathText(result.File.filename()));
        else if (!result.Ok())
            LOG_ERROR(result.Problem);
    }

    void IdlerModel::RefreshUpdates(double now)
    {
        if (m_updateJob)
        {
            if (m_updateJob->Done.load())
                FinishUpdateCheck(now);
            return;
        }
        if (!m_settings.UpdateCheck || !m_updatePreview.empty() || now < Releases::kFirstCheckSeconds || now < m_updateRetryAt)
            return;
        if (!Releases::Due(m_settings.UpdateCheckedAt, static_cast<std::uint32_t>(std::time(nullptr))))
            return;
        m_updateJob = std::make_shared<UpdateJob>();
        std::thread([job = m_updateJob]()
            {
                job->Ok = ReleaseCheck::FetchLatest(job->Body, job->Problem);
                job->Done.store(true);
            }).detach();
    }

    void IdlerModel::FinishUpdateCheck(double now)
    {
        const std::shared_ptr<UpdateJob> job = std::exchange(m_updateJob, nullptr);
        std::string problem = job->Problem;
        std::optional<Releases::Release> release;
        if (job->Ok)
            release = Releases::ParseLatest(job->Body, problem);
        if (!release)
        {
            if (!m_updateWarned)
                LOG_WARN(ErrorMessage(ErrorCode::UpdateCheckFailed, problem));
            m_updateWarned = true;
            m_updateRetryAt = now + Releases::kRetrySeconds;
            return;
        }
        m_settings.UpdateCheckedAt = static_cast<std::uint32_t>(std::time(nullptr));
        if (Releases::IsNewer(release->Version, kVersion))
            LOG_INFO("Version " + release->Version + " is out: " + release->Page);
        else
            LOG_INFO("Looked for a new version: none is newer than " + std::string(kVersion));
        m_settings.UpdateLatest = release->Version;
        MarkDirty();
    }

    std::optional<std::string> IdlerModel::NewerVersion() const
    {
        if (!m_updatePreview.empty())
            return m_updatePreview;
        if (!m_settings.UpdateCheck || !Releases::IsNewer(m_settings.UpdateLatest, kVersion))
            return std::nullopt;
        return m_settings.UpdateLatest;
    }

    bool IdlerModel::TakeUpdateNews()
    {
        const std::optional<std::string> newer = NewerVersion();
        if (!newer)
            return false;
        if (!m_updatePreview.empty())
            return !std::exchange(m_updatePreviewNoted, true);
        if (m_settings.UpdateNoted == *newer)
            return false;
        m_settings.UpdateNoted = *newer;
        MarkDirty();
        return true;
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
}
