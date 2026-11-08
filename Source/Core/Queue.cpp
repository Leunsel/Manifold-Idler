#include "Core/Queue.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

namespace Idler
{
    namespace
    {
        double Baseline(const QueueEntry& entry)
        {
            return entry.Goal.Kind == GoalKind::UntilTotal ? static_cast<double>(entry.BaselineMinutes) * 60.0 : 0.0;
        }

        double Reached(const QueueEntry& entry)
        {
            return Baseline(entry) + entry.IdledSeconds;
        }
    }

    double GoalSeconds(const QueueEntry& entry)
    {
        const double goal = static_cast<double>(entry.Goal.Hours) * 3600.0;
        const double baseline = Baseline(entry);
        return goal <= baseline ? goal : baseline + (goal - baseline) * static_cast<double>(entry.GoalFactor);
    }

    const char* GoalName(GoalKind kind)
    {
        switch (kind)
        {
        case GoalKind::ForHours: return "For a time";
        case GoalKind::UntilTotal: return "Until a playtime";
        case GoalKind::CardDrops: return "Until the cards dropped";
        default: return "No limit";
        }
    }

    bool IsFinished(const QueueEntry& entry)
    {
        if (entry.Goal.Kind == GoalKind::CardDrops)
            return entry.DropsLeft == 0;
        return entry.Goal.Kind != GoalKind::None && Reached(entry) >= GoalSeconds(entry);
    }

    bool IsHeld(const QueueEntry& entry)
    {
        return entry.GuardSecondsLeft == 0.0;
    }

    float Progress(const QueueEntry& entry)
    {
        if (entry.Goal.Kind == GoalKind::None || entry.Goal.Kind == GoalKind::CardDrops)
            return -1.0f;
        if (GoalSeconds(entry) <= 0.0)
            return 1.0f;
        return static_cast<float>(std::clamp(Reached(entry) / GoalSeconds(entry), 0.0, 1.0));
    }

    double SecondsLeft(const QueueEntry& entry)
    {
        if (entry.Goal.Kind == GoalKind::None || entry.Goal.Kind == GoalKind::CardDrops)
            return 0.0;
        return std::max(0.0, GoalSeconds(entry) - Reached(entry));
    }

    double PlayedSeconds(const QueueEntry& entry)
    {
        return static_cast<double>(entry.BaselineMinutes) * 60.0 + entry.IdledSeconds;
    }

    bool HasWorkLeft(const std::vector<QueueEntry>& queue)
    {
        return std::any_of(queue.begin(), queue.end(), [](const QueueEntry& entry) { return !entry.Failed && !IsFinished(entry) && !IsHeld(entry); });
    }

    std::vector<std::uint32_t> WantedRunning(const std::vector<QueueEntry>& queue, int slots)
    {
        std::vector<std::uint32_t> wanted;
        const int limit = std::clamp(slots, 1, kMaxSlots);
        for (const QueueEntry& entry : queue)
        {
            if (static_cast<int>(wanted.size()) >= limit)
                break;
            if (!entry.Failed && !IsFinished(entry) && !IsHeld(entry) && !entry.Resting)
                wanted.push_back(entry.AppId);
        }
        return wanted;
    }

    std::vector<std::uint32_t> WantedWithShown(const std::vector<QueueEntry>& queue, int slots, std::uint32_t shown)
    {
        std::vector<std::uint32_t> wanted = WantedRunning(queue, slots);
        if (shown == 0 || std::find(wanted.begin(), wanted.end(), shown) != wanted.end())
            return wanted;
        const auto entry = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& known) { return known.AppId == shown; });
        if (entry == queue.end() || entry->Failed || IsFinished(*entry) || IsHeld(*entry) || entry->Resting)
            return wanted;
        if (static_cast<int>(wanted.size()) >= std::clamp(slots, 1, kMaxSlots))
            wanted.pop_back();
        wanted.push_back(shown);
        return wanted;
    }

    std::vector<std::uint32_t> WantedForCards(const std::vector<QueueEntry>& queue, double warmupSeconds)
    {
        std::vector<std::uint32_t> warming;
        for (const QueueEntry& entry : queue)
        {
            if (entry.Failed || IsFinished(entry) || IsHeld(entry) || entry.Resting)
                continue;
            if (PlayedSeconds(entry) >= warmupSeconds)
                return { entry.AppId };
            if (static_cast<int>(warming.size()) < kMaxSlots)
                warming.push_back(entry.AppId);
        }
        return warming;
    }

    bool AllOnBreak(const std::vector<QueueEntry>& queue)
    {
        bool onBreak = false;
        for (const QueueEntry& entry : queue)
        {
            if (entry.Failed || IsFinished(entry) || IsHeld(entry))
                continue;
            if (!entry.Resting)
                return false;
            onBreak = true;
        }
        return onBreak;
    }

    float DrawGoalFactor(float spread, double random)
    {
        return 1.0f + spread * static_cast<float>(random * 2.0 - 1.0);
    }

    double DrawRunSeconds(double random)
    {
        return (40.0 + 80.0 * random) * 60.0;
    }

    double DrawRestSeconds(double random)
    {
        return (5.0 + 20.0 * random) * 60.0;
    }

    void AdvanceRhythm(QueueEntry& entry, double now, bool running, double random)
    {
        if (entry.Goal.Kind == GoalKind::CardDrops)
        {
            entry.Resting = false;
            entry.RestUntil = 0.0;
            entry.RunUntil = 0.0;
            return;
        }
        if (running && entry.RunUntil <= 0.0)
        {
            entry.RunUntil = now + DrawRunSeconds(random);
        }
        else if (running && now >= entry.RunUntil)
        {
            entry.RunUntil = 0.0;
            entry.RestUntil = now + DrawRestSeconds(random);
        }
        else if (!running && entry.RestUntil <= now)
        {
            entry.RunUntil = 0.0;
        }
        entry.Resting = entry.RestUntil > now;
    }

    void MoveEntry(std::vector<QueueEntry>& queue, int from, int to)
    {
        const int size = static_cast<int>(queue.size());
        if (from < 0 || to < 0 || from >= size || to >= size || from == to)
            return;
        const auto first = queue.begin();
        if (from < to)
            std::rotate(first + from, first + from + 1, first + to + 1);
        else
            std::rotate(first + to, first + from, first + from + 1);
    }

    QueueEntry* FindEntry(std::vector<QueueEntry>& queue, std::uint32_t appId)
    {
        const auto entry = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& known) { return known.AppId == appId; });
        return entry == queue.end() ? nullptr : &*entry;
    }

    std::vector<RemovedEntry> TakeEntries(std::vector<QueueEntry>& queue, const std::function<bool(const QueueEntry&)>& match)
    {
        std::vector<RemovedEntry> taken;
        for (int place = static_cast<int>(queue.size()) - 1; place >= 0; --place)
        {
            if (!match(queue[static_cast<std::size_t>(place)]))
                continue;
            taken.push_back({ place, std::move(queue[static_cast<std::size_t>(place)]) });
            queue.erase(queue.begin() + place);
        }
        return taken;
    }

    void PutBackEntries(std::vector<QueueEntry>& queue, const std::vector<RemovedEntry>& removed)
    {
        for (auto it = removed.rbegin(); it != removed.rend(); ++it)
        {
            std::erase_if(queue, [&](const QueueEntry& entry) { return entry.AppId == it->Entry.AppId; });
            const int place = std::clamp(it->Place, 0, static_cast<int>(queue.size()));
            queue.insert(queue.begin() + place, it->Entry);
        }
    }

    const char* QueueOrderName(QueueOrder order)
    {
        switch (order)
        {
        case QueueOrder::TimeLeft: return "By time left";
        case QueueOrder::LeastPlayed: return "Least played first";
        case QueueOrder::MostPlayed: return "Most played first";
        default: return "By name";
        }
    }

    void SortQueue(std::vector<QueueEntry>& queue, QueueOrder order, const std::function<std::string(std::uint32_t)>& foldedName)
    {
        const auto by = [&](auto less) { std::stable_sort(queue.begin(), queue.end(), less); };
        switch (order)
        {
        case QueueOrder::TimeLeft:
        {
            const auto tier = [](const QueueEntry& entry) { return IsFinished(entry) ? 2 : SecondsLeft(entry) > 0.0 ? 0 : 1; };
            by([&](const QueueEntry& lhs, const QueueEntry& rhs)
                { return tier(lhs) != tier(rhs) ? tier(lhs) < tier(rhs) : SecondsLeft(lhs) < SecondsLeft(rhs); });
            break;
        }
        case QueueOrder::LeastPlayed:
            by([](const QueueEntry& lhs, const QueueEntry& rhs) { return PlayedSeconds(lhs) < PlayedSeconds(rhs); });
            break;
        case QueueOrder::MostPlayed:
            by([](const QueueEntry& lhs, const QueueEntry& rhs) { return PlayedSeconds(lhs) > PlayedSeconds(rhs); });
            break;
        default:
        {
            std::unordered_map<std::uint32_t, std::string> names;
            for (const QueueEntry& entry : queue)
                names[entry.AppId] = foldedName(entry.AppId);
            by([&](const QueueEntry& lhs, const QueueEntry& rhs)
                {
                    const std::string& left = names[lhs.AppId];
                    const std::string& right = names[rhs.AppId];
                    return left.empty() != right.empty() ? right.empty() : left < right;
                });
            break;
        }
        }
    }

    std::vector<std::uint32_t> OrderOf(const std::vector<QueueEntry>& queue)
    {
        std::vector<std::uint32_t> order;
        order.reserve(queue.size());
        for (const QueueEntry& entry : queue)
            order.push_back(entry.AppId);
        return order;
    }

    void RestoreOrder(std::vector<QueueEntry>& queue, const std::vector<std::uint32_t>& order)
    {
        const auto place = [&](const QueueEntry& entry) { return std::find(order.begin(), order.end(), entry.AppId) - order.begin(); };
        std::stable_sort(queue.begin(), queue.end(), [&](const QueueEntry& lhs, const QueueEntry& rhs) { return place(lhs) < place(rhs); });
    }

    QueueForecast ForecastQueue(const std::vector<QueueEntry>& queue, int slots)
    {
        QueueForecast forecast;
        int free = std::clamp(slots, 1, kMaxSlots);

        std::priority_queue<double, std::vector<double>, std::greater<double>> ends;
        for (const QueueEntry& entry : queue)
        {
            if (entry.Failed || IsFinished(entry) || IsHeld(entry))
                continue;

            double left = entry.Goal.Kind == GoalKind::None || entry.Goal.Kind == GoalKind::CardDrops ? -1.0 : SecondsLeft(entry);
            if (entry.GuardSecondsLeft > 0.0 && (left < 0.0 || entry.GuardSecondsLeft < left))
                left = entry.GuardSecondsLeft;

            double start = 0.0;
            if (static_cast<int>(ends.size()) >= free)
            {
                start = ends.top();
                ends.pop();
            }
            if (left < 0.0)
            {
                forecast.Endless = true;
                forecast.Seconds = std::max(forecast.Seconds, start);
                if (--free == 0)
                    break;
                continue;
            }
            const double end = start + left;
            ends.push(end);
            forecast.Seconds = std::max(forecast.Seconds, end);
        }
        return forecast;
    }

    bool IsRefundable(std::uint32_t purchased, double playedSeconds, std::uint32_t now)
    {
        if (purchased == 0 || now < purchased)
            return false;
        return now - purchased < kRefundWindowSeconds && playedSeconds < kRefundPlaySeconds;
    }

    double RefundGuardLeft(std::uint32_t purchased, double playedSeconds, std::uint32_t now)
    {
        if (!IsRefundable(purchased, playedSeconds, now))
            return -1.0;
        return std::max(0.0, kRefundPlaySeconds - kRefundMarginSeconds - playedSeconds);
    }

    bool InHourWindow(int fromHour, int toHour, int hour)
    {
        if (fromHour == toHour)
            return true;
        if (fromHour < toHour)
            return hour >= fromHour && hour < toHour;
        return hour >= fromHour || hour < toHour;
    }
}
