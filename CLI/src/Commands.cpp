#include "Commands.h"

#include "App/IdlerModel.h"
#include "Core/RuntimeLog.h"
#include "Core/Text.h"
#include "Request.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Idler::Cli
{
    namespace
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
            std::string text;
            const auto line = [&](const char* label, const std::string& value) { text += Padded(label, 14) + value + "\n"; };

            const Steam::Status& steam = model.SteamStatus();
            if (!steam.Installed())
                line("Steam", "not found");
            else if (!steam.Running())
                line("Steam", "not running");
            else if (!steam.LoggedOn())
                line("Steam", steam.MayConnect() ? "running, asking it who is logged on" : "nobody is logged on");
            else
                line("Steam", "logged on" + (model.Persona().empty() ? std::string() : " as " + model.Persona()));

            if (model.Scanning() && !model.ScannedOnce())
                line("Library", "being read");
            else if (!model.ScanProblem().empty())
                line("Library", model.ScanProblem());
            else if (model.ScannedOnce())
                line("Library", GamesText(model.Games().size()));

            line("Queue", model.Lists()[static_cast<std::size_t>(model.ActiveList())].Name + ", " + GamesText(model.Queue().size()));
            line("State", StateText(model));
            line("At once", std::to_string(model.Slots()));
            line("This session", DurationText(static_cast<std::uint64_t>(model.SessionSeconds())));

            const QueueForecast forecast = model.Forecast();
            if (forecast.Endless)
                line("Time left", "no end: a game has no limit");
            else if (forecast.Seconds > 0.0)
                line("Time left", DurationText(static_cast<std::uint64_t>(std::ceil(forecast.Seconds))) + (model.Options().VaryTimes ? " plus breaks" : ""));
            return Done(std::move(text));
        }

        CommandResult ShowQueue(const IdlerModel& model)
        {
            if (model.Queue().empty())
                return Done("The queue is empty.\n");
            std::string text;
            for (const QueueEntry& entry : model.Queue())
            {
                text += Padded(std::to_string(entry.AppId), 10) + Padded(EntryState(model, entry), 30) + Padded(GoalWords(entry.Goal), 24) +
                    Padded(DurationText(static_cast<std::uint64_t>(entry.IdledSeconds)), 14) + GameName(model, entry.AppId) + "\n";
            }
            return Done(std::move(text));
        }

        bool LibraryReady(const IdlerModel& model, std::string& problem)
        {
            if (!model.SteamStatus().LoggedOn())
            {
                problem = "Steam is not running, or nobody is logged on.";
                return false;
            }
            if (!model.ScannedOnce())
            {
                problem = "The library is still being read. Try again in a moment.";
                return false;
            }
            return true;
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
            std::string text;
            int shown = 0;
            int found = 0;
            for (const Game& game : model.Games())
            {
                if (!search.empty() && game.Folded.find(search) == std::string::npos && std::to_string(game.AppId) != search)
                    continue;
                ++found;
                if (shown < limit)
                {
                    text += Padded(std::to_string(game.AppId), 10) + Padded(PlaytimeText(game.PlaytimeMinutes), 14) + game.Name + "\n";
                    ++shown;
                }
            }
            text += std::to_string(shown) + " of " + GamesText(found) + " shown.\n";
            return Done(std::move(text));
        }

        CommandResult Lists(const IdlerModel& model)
        {
            std::string text;
            const auto& lists = model.Lists();
            for (std::size_t index = 0; index < lists.size(); ++index)
            {
                if (model.IsListHidden(static_cast<int>(index)))
                    continue;
                text += (static_cast<int>(index) == model.ActiveList() ? "* " : "  ") + Padded(lists[index].Name, 30) + GamesText(lists[index].Entries.size()) + "\n";
            }
            return Done(std::move(text));
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
            std::string text;
            for (std::size_t index = first; index < entries.size(); ++index)
                text += entries[index].Time + "  " + Padded(RuntimeLog::LevelName(entries[index].Level), 9) + entries[index].Message + "\n";
            return Done(std::move(text));
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
        if (command == "quit")
        {
            CommandResult result = Done("Ending.\n");
            result.Quit = true;
            return result;
        }
        return Failed("Unknown command: " + command);
    }
}
