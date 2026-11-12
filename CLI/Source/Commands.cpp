#include "Commands.h"

#include "App/IdlerModel.h"
#include "Core/Errors.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Request.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Idler::Cli
{
    CommandResult Done(std::string text)
    {
        CommandResult result;
        result.Text = std::move(text);
        return result;
    }

    CommandResult Failed(std::string text)
    {
        CommandResult result;
        result.Ok = false;
        result.Text = std::move(text);
        return result;
    }

    std::string Padded(std::string text, std::size_t width)
    {
        if (text.size() < width)
            text.append(width - text.size(), ' ');
        return text;
    }

    std::string GamesText(std::size_t count)
    {
        return std::to_string(count) + (count == 1 ? " game" : " games");
    }

    std::string GameName(const IdlerModel& model, std::uint32_t appId)
    {
        const Game* game = model.FindGame(appId);
        return game ? game->Name : "App " + std::to_string(appId);
    }

    bool LibraryReady(const IdlerModel& model, std::string& problem)
    {
        if (!model.SteamStatus().LoggedOn())
        {
            problem = ErrorMessage(model.SteamStatus().Running() ? ErrorCode::NobodyLoggedOn : ErrorCode::SteamNotRunning);
            return false;
        }
        if (!model.ScannedOnce())
        {
            problem = "The library is still being read. Try again in a moment.";
            return false;
        }
        return true;
    }

    namespace
    {
        std::string StateText(const IdlerModel& model)
        {
            switch (model.Phase())
            {
            case QueuePhase::Starting:
                if (model.WantedCount() == 0)
                    return "Starting";
                return "Starting " + std::to_string(model.ReadyCount()) + " of " + std::to_string(model.WantedCount());
            case QueuePhase::Running:
                return "Idling " + std::to_string(model.RunningCount());
            case QueuePhase::Stopping:
                return "Stopping " + std::to_string(model.AliveCount());
            case QueuePhase::Waiting:
                switch (model.CurrentHold())
                {
                case Hold::Steam: return "Waiting: Steam is not running, or nobody is logged on";
                case Hold::Playing: return "Waiting: another game runs, " + GameName(model, model.PlayingApp());
                case Hold::Hours: return "Waiting: outside the hours set for idling";
                case Hold::Away: return "Waiting: somebody is at the computer";
                default: return "Waiting: all games are on a break";
                }
            default:
                return "Stopped";
            }
        }

        std::string GoalWords(const Goal& goal)
        {
            switch (goal.Kind)
            {
            case GoalKind::ForHours: return "for " + HoursText(goal.Hours);
            case GoalKind::UntilTotal: return "until " + HoursText(goal.Hours) + " played";
            case GoalKind::CardDrops: return "until the cards dropped";
            default: return "no limit";
            }
        }

        std::string GoalKindWord(const Goal& goal)
        {
            switch (goal.Kind)
            {
            case GoalKind::ForHours: return "for";
            case GoalKind::UntilTotal: return "until";
            case GoalKind::CardDrops: return "cards";
            default: return "none";
            }
        }

        std::string HoursNumber(float hours)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%g", static_cast<double>(hours));
            return text;
        }

        std::string EntryState(const IdlerModel& model, const QueueEntry& entry)
        {
            switch (model.StateOf(entry.AppId))
            {
            case RunState::Running: return "idling";
            case RunState::Starting: return "starting";
            case RunState::Stopping: return "stopping";
            default: break;
            }
            if (entry.Failed)
                return "failed: " + entry.Error;
            if (IsHeld(entry))
                return "held before the refund limit";
            if (IsFinished(entry))
                return "finished";
            if (const double rest = model.RestSecondsLeft(entry.AppId); rest > 0.0)
                return (entry.IdledSeconds > 0.0 ? "on a break, back in " : "starts in ") + DurationText(static_cast<std::uint64_t>(std::ceil(rest)));
            return "waiting";
        }

        CommandResult Status(const IdlerModel& model)
        {
            CommandResult result;
            result.Columns = { "key", "label", "value" };
            const auto line = [&](const char* key, const char* label, const std::string& value)
                {
                    result.Text += Padded(label, 14) + value + "\n";
                    result.Rows.push_back({ key, label, value });
                };

            const Steam::Status& steam = model.SteamStatus();
            if (!steam.Installed())
                line("steam", "Steam", "not found");
            else if (!steam.Running())
                line("steam", "Steam", "not running");
            else if (!steam.LoggedOn())
                line("steam", "Steam", steam.MayConnect() ? "running, asking it who is logged on" : "nobody is logged on");
            else
                line("steam", "Steam", "logged on" + (model.Persona().empty() ? std::string() : " as " + model.Persona()));

            if (model.Scanning() && !model.ScannedOnce())
                line("library", "Library", "being read");
            else if (!model.ScanProblem().empty())
                line("library", "Library", model.ScanProblem());
            else if (model.ScannedOnce())
                line("library", "Library", GamesText(model.Games().size()));

            line("queue", "Queue", model.Lists()[static_cast<std::size_t>(model.ActiveList())].Name + ", " + GamesText(model.Queue().size()));
            line("state", "State", StateText(model));
            line("slots", "At once", std::to_string(model.Slots()));
            line("session", "This session", DurationText(static_cast<std::uint64_t>(model.SessionSeconds())));

            const QueueForecast forecast = model.Forecast();
            if (forecast.Endless)
                line("left", "Time left", "no end: a game has no limit");
            else if (forecast.Seconds > 0.0)
                line("left", "Time left", DurationText(static_cast<std::uint64_t>(std::ceil(forecast.Seconds))) + (model.Options().VaryTimes ? " plus breaks" : ""));
            result.Rows.push_back({ "active", "", model.Active() ? "yes" : "no" });
            result.Rows.push_back({ "persona", "", steam.LoggedOn() ? model.Persona() : std::string() });
            result.Rows.push_back({ "undo", "", model.Undoable().Any() ? "yes" : "no" });
            return result;
        }

        std::string EntryFlags(const IdlerModel& model, const QueueEntry& entry)
        {
            std::string flags;
            const auto add = [&](const char* flag) { flags += (flags.empty() ? "" : " ") + std::string(flag); };
            const RunState state = model.StateOf(entry.AppId);
            if (state == RunState::Running || state == RunState::Starting)
                add("running");
            if (entry.Failed)
                add("failed");
            if (IsHeld(entry))
                add("held");
            if (IsFinished(entry))
                add("finished");
            if (model.RestSecondsLeft(entry.AppId) > 0.0)
                add("resting");
            return flags;
        }

        CommandResult ShowQueue(const IdlerModel& model)
        {
            CommandResult result;
            result.Columns = { "app", "name", "state", "goal", "idled", "kind", "hours", "progress", "left", "flags" };
            if (model.Queue().empty())
            {
                result.Text = "The queue is empty.\n";
                return result;
            }
            for (const QueueEntry& entry : model.Queue())
            {
                const std::string app = std::to_string(entry.AppId);
                const std::string state = EntryState(model, entry);
                const std::string goal = GoalWords(entry.Goal);
                const std::string idled = DurationText(static_cast<std::uint64_t>(entry.IdledSeconds));
                const std::string name = GameName(model, entry.AppId);
                const bool limited = entry.Goal.Kind != GoalKind::None;
                const double left = limited ? SecondsLeft(entry) : 0.0;
                char progress[16] = "";
                if (limited)
                    std::snprintf(progress, sizeof(progress), "%.3f", static_cast<double>(std::clamp(Progress(entry), 0.0f, 1.0f)));
                result.Text += Padded(app, 10) + Padded(state, 30) + Padded(goal, 24) + Padded(idled, 14) + name + "\n";
                result.Rows.push_back({ app, name, state, goal, idled, GoalKindWord(entry.Goal), HoursNumber(entry.Goal.Hours), progress,
                    left > 0.0 ? DurationText(static_cast<std::uint64_t>(std::ceil(left))) : std::string(), EntryFlags(model, entry) });
            }
            return result;
        }

        std::string NotQueued(const std::vector<std::string>& words, std::size_t at)
        {
            return (words.size() > at ? words[at] : std::string("That")) + " is not in the queue.";
        }

        std::optional<std::uint32_t> QueuedApp(const IdlerModel& model, const std::vector<std::string>& words, std::size_t at)
        {
            const auto appId = words.size() > at ? ParseUnsigned(words[at]) : std::nullopt;
            if (appId && model.FindEntry(*appId))
                return appId;
            return std::nullopt;
        }

        CommandResult Move(IdlerModel& model, const std::vector<std::string>& words)
        {
            const auto appId = QueuedApp(model, words, 1);
            if (!appId)
                return Failed(NotQueued(words, 1));
            const auto& queue = model.Queue();
            const auto position = words.size() == 3 ? ParseUnsigned(words[2]) : std::nullopt;
            if (!position || *position == 0 || *position > queue.size())
                return Failed("A place is from 1 to " + std::to_string(queue.size()) + ".");
            const auto from = std::find_if(queue.begin(), queue.end(), [&](const QueueEntry& entry) { return entry.AppId == *appId; }) - queue.begin();
            model.MoveEntry(static_cast<int>(from), static_cast<int>(*position) - 1);
            return Done(GameName(model, *appId) + " is number " + std::to_string(*position) + " now.\n");
        }

        CommandResult Sort(IdlerModel& model, const std::vector<std::string>& words)
        {
            constexpr std::pair<std::string_view, QueueOrder> kOrders[] = {
                { "name", QueueOrder::Name }, { "left", QueueOrder::TimeLeft }, { "least", QueueOrder::LeastPlayed }, { "most", QueueOrder::MostPlayed } };
            for (const auto& [word, order] : kOrders)
            {
                if (words.size() == 2 && words[1] == word)
                {
                    model.SortQueue(order);
                    return Done(std::string("Sorted: ") + QueueOrderName(order) + ".\n");
                }
            }
            return Failed("Sort by name, left, least or most.");
        }

        CommandResult Retry(IdlerModel& model, const std::vector<std::string>& words)
        {
            const auto appId = QueuedApp(model, words, 1);
            if (!appId)
                return Failed(NotQueued(words, 1));
            model.Retry(*appId);
            return Done("Trying again: " + GameName(model, *appId) + "\n");
        }

        CommandResult IdleNow(IdlerModel& model, const std::vector<std::string>& words)
        {
            std::string problem;
            if (!LibraryReady(model, problem))
                return Failed(problem);
            const auto appId = words.size() == 2 ? ParseUnsigned(words[1]) : std::nullopt;
            if (!appId || !model.FindGame(*appId))
                return Failed((words.size() >= 2 ? words[1] : std::string("That")) + " is not a game of this account.");
            model.IdleNow(*appId);
            return Done("Idles first now: " + GameName(model, *appId) + "\n");
        }

        CommandResult PastRefund(IdlerModel& model, const std::vector<std::string>& words)
        {
            const auto appId = QueuedApp(model, words, 1);
            if (!appId)
                return Failed(NotQueued(words, 1));
            model.AllowPastRefund(*appId);
            return Done(GameName(model, *appId) + " goes on past the refund limit. Steam will not refund it after that.\n");
        }

        CommandResult Clear(IdlerModel& model, const std::vector<std::string>& words)
        {
            if (words.size() == 2 && words[1] == "finished")
            {
                const auto count = static_cast<std::size_t>(std::count_if(model.Queue().begin(), model.Queue().end(), [](const QueueEntry& entry) { return IsFinished(entry); }));
                if (count == 0)
                    return Failed("No game in the queue has finished.");
                model.RemoveFinished();
                return Done("Removed " + GamesText(count) + " that finished.\n");
            }
            if (words.size() == 2 && words[1] == "all")
            {
                const std::size_t count = model.Queue().size();
                if (count == 0)
                    return Failed("The queue is empty.");
                model.RemoveAll();
                return Done("Removed all " + GamesText(count) + ".\n");
            }
            return Failed("Clear what: finished or all?");
        }

        CommandResult UndoLast(IdlerModel& model)
        {
            if (!model.Undoable().Any())
                return Failed("Nothing to undo.");
            model.Undo();
            return Done("Undone.\n");
        }

        int FindList(const IdlerModel& model, std::string_view name)
        {
            const std::string wanted = FoldCase(name);
            const auto& lists = model.Lists();
            for (std::size_t index = 0; index < lists.size(); ++index)
            {
                if (!model.IsListHidden(static_cast<int>(index)) && FoldCase(lists[index].Name) == wanted)
                    return static_cast<int>(index);
            }
            return -1;
        }

        CommandResult ListCommand(IdlerModel& model, const std::vector<std::string>& words)
        {
            const std::string action = words.size() >= 2 ? words[1] : std::string();
            if (action == "new" && words.size() == 3)
            {
                if (Trim(words[2]).empty())
                    return Failed("A list needs a name.");
                model.CreateList(words[2]);
                return Done("New list: " + model.Lists()[static_cast<std::size_t>(model.ActiveList())].Name + ". It is the queue now.\n");
            }
            if (action == "rename" && words.size() == 4)
            {
                const int index = FindList(model, words[2]);
                if (index < 0)
                    return Failed("There is no list called " + words[2] + ".");
                if (Trim(words[3]).empty())
                    return Failed("A list needs a name.");
                model.RenameList(index, words[3]);
                return Done("Renamed: " + model.Lists()[static_cast<std::size_t>(index)].Name + "\n");
            }
            if (action == "delete" && words.size() == 3)
            {
                const int index = FindList(model, words[2]);
                if (index < 0)
                    return Failed("There is no list called " + words[2] + ".");
                if (model.VisibleListCount() < 2)
                    return Failed("The last list stays.");
                const std::string name = model.Lists()[static_cast<std::size_t>(index)].Name;
                model.DeleteList(index);
                return Done("Deleted: " + name + ". Undo brings it back.\n");
            }
            return Failed("list new <name>, list rename <name> <new name> or list delete <name>.");
        }

        CommandResult Rescan(IdlerModel& model)
        {
            if (!model.SteamStatus().LoggedOn())
                return Failed(ErrorMessage(model.SteamStatus().Running() ? ErrorCode::NobodyLoggedOn : ErrorCode::SteamNotRunning));
            if (model.Scanning())
                return Failed("The library is being read already.");
            model.Rescan();
            return Done("Reading the library again.\n");
        }

        CommandResult Add(IdlerModel& model, const std::vector<std::string>& words)
        {
            const auto split = std::find(words.begin(), words.end(), std::string("--"));
            if (split == words.end() || split + 1 == words.end())
                return Failed("Which games? Give their app ids.");
            std::string problem;
            if (!LibraryReady(model, problem))
                return Failed(problem);

            const std::vector<std::string> goalWords(words.begin() + 1, split);
            Goal goal;
            const bool withGoal = !goalWords.empty();
            if (withGoal && !ParseGoal(goalWords, 0, goal, problem))
                return Failed(problem);

            std::vector<std::uint32_t> apps;
            for (auto word = split + 1; word != words.end(); ++word)
            {
                const auto appId = ParseUnsigned(*word);
                if (!appId || !model.FindGame(*appId))
                    return Failed(*word + " is not a game of this account.");
                apps.push_back(*appId);
            }

            std::string text;
            for (const std::uint32_t appId : apps)
            {
                const bool known = model.FindEntry(appId) != nullptr;
                model.AddToQueue(appId);
                if (withGoal)
                    model.SetGoal(appId, goal);
                text += (known ? "Already in the queue: " : "Added: ") + GameName(model, appId) + "\n";
            }
            return Done(std::move(text));
        }

        CommandResult Remove(IdlerModel& model, const std::vector<std::string>& words)
        {
            if (words.size() < 2)
                return Failed("Which games? Give their app ids.");
            std::vector<std::uint32_t> apps;
            for (std::size_t index = 1; index < words.size(); ++index)
            {
                const auto appId = ParseUnsigned(words[index]);
                if (!appId || !model.FindEntry(*appId))
                    return Failed(words[index] + " is not in the queue.");
                apps.push_back(*appId);
            }
            std::string text;
            for (const std::uint32_t appId : apps)
            {
                text += "Removed: " + GameName(model, appId) + "\n";
                model.Remove(appId);
            }
            return Done(std::move(text));
        }

        CommandResult SetGoal(IdlerModel& model, const std::vector<std::string>& words)
        {
            const auto appId = words.size() >= 2 ? ParseUnsigned(words[1]) : std::nullopt;
            if (!appId || !model.FindEntry(*appId))
                return Failed((words.size() >= 2 ? words[1] : std::string("That")) + " is not in the queue.");
            Goal goal;
            std::string problem;
            if (!ParseGoal(words, 2, goal, problem))
                return Failed(problem);
            model.SetGoal(*appId, goal);
            return Done(GameName(model, *appId) + ": " + GoalWords(goal) + "\n");
        }

        CommandResult Games(const IdlerModel& model, const std::vector<std::string>& words)
        {
            std::string problem;
            if (!LibraryReady(model, problem))
                return Failed(problem);
            const int limit = words.size() >= 2 ? static_cast<int>(ParseUnsigned(words[1]).value_or(50)) : 50;
            const std::string search = words.size() >= 3 ? FoldCase(words[2]) : std::string();
            CommandResult result;
            result.Columns = { "app", "name", "playtime", "queued" };
            std::string& text = result.Text;
            int shown = 0;
            int found = 0;
            for (const Game& game : model.Games())
            {
                if (!search.empty() && game.Folded.find(search) == std::string::npos && std::to_string(game.AppId) != search)
                    continue;
                ++found;
                if (shown < limit)
                {
                    const std::string app = std::to_string(game.AppId);
                    const std::string playtime = PlaytimeText(game.PlaytimeMinutes);
                    text += Padded(app, 10) + Padded(playtime, 14) + game.Name + "\n";
                    result.Rows.push_back({ app, game.Name, playtime, model.FindEntry(game.AppId) ? "yes" : "no" });
                    ++shown;
                }
            }
            text += std::to_string(shown) + " of " + GamesText(found) + " shown.\n";
            return result;
        }

        CommandResult Lists(const IdlerModel& model)
        {
            CommandResult result;
            result.Columns = { "name", "games", "active" };
            const auto& lists = model.Lists();
            for (std::size_t index = 0; index < lists.size(); ++index)
            {
                if (model.IsListHidden(static_cast<int>(index)))
                    continue;
                const bool active = static_cast<int>(index) == model.ActiveList();
                result.Text += (active ? "* " : "  ") + Padded(lists[index].Name, 30) + GamesText(lists[index].Entries.size()) + "\n";
                result.Rows.push_back({ lists[index].Name, std::to_string(lists[index].Entries.size()), active ? "yes" : "no" });
            }
            return result;
        }

        CommandResult UseList(IdlerModel& model, const std::vector<std::string>& words)
        {
            if (words.size() != 2)
                return Failed("Which list?");
            const std::string wanted = FoldCase(words[1]);
            const auto& lists = model.Lists();
            for (std::size_t index = 0; index < lists.size(); ++index)
            {
                if (!model.IsListHidden(static_cast<int>(index)) && FoldCase(lists[index].Name) == wanted)
                {
                    model.SelectList(static_cast<int>(index));
                    return Done("The queue is now " + lists[index].Name + ".\n");
                }
            }
            return Failed("There is no list called " + words[1] + ".");
        }

        CommandResult Log(const std::vector<std::string>& words)
        {
            const std::size_t wanted = words.size() >= 2 ? ParseUnsigned(words[1]).value_or(30) : 30;
            const std::vector<LogEntry> entries = RuntimeLog::Entries();
            const std::size_t first = entries.size() > wanted ? entries.size() - wanted : 0;
            CommandResult result;
            result.Columns = { "time", "level", "message" };
            for (std::size_t index = first; index < entries.size(); ++index)
            {
                const LogEntry& entry = entries[index];
                result.Text += entry.Time + "  " + Padded(RuntimeLog::LevelName(entry.Level), 9) + entry.Message + "\n";
                result.Rows.push_back({ entry.Time, RuntimeLog::LevelName(entry.Level), entry.Message });
            }
            return result;
        }
    }

    CommandResult RunCommand(IdlerModel& model, const std::vector<std::string>& words)
    {
        const std::string command = words.empty() ? std::string() : words[0];
        if (command == "ping")
            return Done("pong\n");
        if (command == "status")
            return Status(model);
        if (command == "queue")
            return ShowQueue(model);
        if (command == "add")
            return Add(model, words);
        if (command == "remove")
            return Remove(model, words);
        if (command == "goal")
            return SetGoal(model, words);
        if (command == "games")
            return Games(model, words);
        if (command == "lists")
            return Lists(model);
        if (command == "use")
            return UseList(model, words);
        if (command == "log")
            return Log(words);
        if (command == "start")
        {
            if (!HasWorkLeft(model.Queue()))
                return Failed("Nothing in the queue can run.");
            model.SetActive(true);
            return Done("Started.\n");
        }
        if (command == "stop")
        {
            model.SetActive(false);
            return Done("Stopped.\n");
        }
        if (command == "slots")
        {
            const auto slots = words.size() == 2 ? ParseUnsigned(words[1]) : std::nullopt;
            if (!slots || *slots == 0 || *slots > static_cast<std::uint32_t>(kMaxSlots))
                return Failed("Between 1 and " + std::to_string(kMaxSlots) + " games can idle at once.");
            model.SetSlots(static_cast<int>(*slots));
            return Done("Up to " + std::to_string(*slots) + " games at once.\n");
        }
        if (command == "move")
            return Move(model, words);
        if (command == "sort")
            return Sort(model, words);
        if (command == "retry")
            return Retry(model, words);
        if (command == "now")
            return IdleNow(model, words);
        if (command == "refund")
            return PastRefund(model, words);
        if (command == "clear")
            return Clear(model, words);
        if (command == "undo")
            return UndoLast(model);
        if (command == "list")
            return ListCommand(model, words);
        if (command == "rescan")
            return Rescan(model);
        if (command == "quit")
        {
            CommandResult result = Done("Ending.\n");
            result.Quit = true;
            return result;
        }
        if (CommandResult result; RunPageCommand(model, words, result))
            return result;
        return Failed("Unknown command: " + command);
    }
}
