#include "App/IdlerModel.h"

#include "App/Model/ModelParts.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"

#include <algorithm>
#include <utility>

namespace Idler
{
    using namespace ModelParts;

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
}
