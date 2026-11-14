#include "App/IdlerModel.h"

#include "App/Model/ModelParts.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"

#include <algorithm>
#include <ctime>
#include <iterator>
#include <utility>

namespace Idler
{
    using namespace ModelParts;

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
}
