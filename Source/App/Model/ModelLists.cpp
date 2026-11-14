#include "App/IdlerModel.h"

#include "App/Model/ModelParts.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"

#include <algorithm>
#include <utility>

namespace Idler
{
    using namespace ModelParts;

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
}
