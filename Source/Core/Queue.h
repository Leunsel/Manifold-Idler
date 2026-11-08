#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Idler
{
    inline constexpr int kMaxSlots = 32;

    inline constexpr char kDefaultListName[] = "Queue";

    enum class GoalKind
    {
        None,
        ForHours,
        UntilTotal,
        CardDrops,
        Count
    };

    struct Goal
    {
        GoalKind Kind = GoalKind::None;
        float Hours = 3.0f;
    };

    struct QueueEntry
    {
        std::uint32_t AppId = 0;
        Idler::Goal Goal;
        double IdledSeconds = 0.0;
        std::uint32_t BaselineMinutes = 0;
        bool IgnoreRefund = false;
        float GoalFactor = 1.0f;

        bool Failed = false;
        std::string Error;
        int Crashes = 0;
        double GuardSecondsLeft = -1.0;
        int DropsLeft = -1;
        bool Resting = false;
        double RestUntil = 0.0;
        double RunUntil = 0.0;
    };

    const char* GoalName(GoalKind kind);

    double GoalSeconds(const QueueEntry& entry);
    bool IsFinished(const QueueEntry& entry);
    bool IsHeld(const QueueEntry& entry);
    float Progress(const QueueEntry& entry);
    double SecondsLeft(const QueueEntry& entry);
    double PlayedSeconds(const QueueEntry& entry);

    bool HasWorkLeft(const std::vector<QueueEntry>& queue);

    std::vector<std::uint32_t> WantedRunning(const std::vector<QueueEntry>& queue, int slots);

    std::vector<std::uint32_t> WantedWithShown(const std::vector<QueueEntry>& queue, int slots, std::uint32_t shown);

    std::vector<std::uint32_t> WantedForCards(const std::vector<QueueEntry>& queue, double warmupSeconds);

    bool AllOnBreak(const std::vector<QueueEntry>& queue);

    float DrawGoalFactor(float spread, double random);
    double DrawRunSeconds(double random);
    double DrawRestSeconds(double random);
    void AdvanceRhythm(QueueEntry& entry, double now, bool running, double random);
    inline constexpr double kStartSpreadSeconds = 10.0 * 60.0;

    void MoveEntry(std::vector<QueueEntry>& queue, int from, int to);

    QueueEntry* FindEntry(std::vector<QueueEntry>& queue, std::uint32_t appId);

    struct RemovedEntry
    {
        int Place = 0;
        QueueEntry Entry;
    };
    std::vector<RemovedEntry> TakeEntries(std::vector<QueueEntry>& queue, const std::function<bool(const QueueEntry&)>& match);
    void PutBackEntries(std::vector<QueueEntry>& queue, const std::vector<RemovedEntry>& removed);

    enum class QueuePhase
    {
        Stopped,
        Starting,
        Running,
        Waiting,
        Stopping
    };
    constexpr QueuePhase PhaseOfQueue(bool active, bool held, bool startingUp, int alive)
    {
        if (!active)
            return alive > 0 ? QueuePhase::Stopping : QueuePhase::Stopped;
        if (held)
            return QueuePhase::Waiting;
        return startingUp ? QueuePhase::Starting : QueuePhase::Running;
    }

    enum class QueueOrder
    {
        Name,
        TimeLeft,
        LeastPlayed,
        MostPlayed,
        Count
    };
    const char* QueueOrderName(QueueOrder order);
    void SortQueue(std::vector<QueueEntry>& queue, QueueOrder order, const std::function<std::string(std::uint32_t)>& foldedName);
    std::vector<std::uint32_t> OrderOf(const std::vector<QueueEntry>& queue);
    void RestoreOrder(std::vector<QueueEntry>& queue, const std::vector<std::uint32_t>& order);

    struct QueueForecast
    {
        bool Endless = false;
        double Seconds = 0.0;
    };
    QueueForecast ForecastQueue(const std::vector<QueueEntry>& queue, int slots);

    inline constexpr std::uint32_t kRefundWindowSeconds = 14u * 24u * 3600u;
    inline constexpr double kRefundPlaySeconds = 2.0 * 3600.0;
    inline constexpr double kRefundMarginSeconds = 10.0 * 60.0;

    bool IsRefundable(std::uint32_t purchased, double playedSeconds, std::uint32_t now);
    double RefundGuardLeft(std::uint32_t purchased, double playedSeconds, std::uint32_t now);

    bool InHourWindow(int fromHour, int toHour, int hour);
}
