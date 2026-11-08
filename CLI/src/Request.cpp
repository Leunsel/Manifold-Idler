#include "Request.h"

#include <cstdlib>

namespace Idler::Cli
{
    std::string JoinRequest(const std::vector<std::string>& words)
    {
        std::string line;
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            if (index > 0)
                line.push_back('\t');
            for (const char character : words[index])
                line.push_back(character == '\t' || character == '\r' || character == '\n' ? ' ' : character);
        }
        return line;
    }

    std::vector<std::string> SplitRequest(std::string_view line)
    {
        std::vector<std::string> words;
        std::size_t start = 0;
        while (start <= line.size() && !line.empty())
        {
            const std::size_t end = line.find('\t', start);
            if (end == std::string_view::npos)
            {
                words.emplace_back(line.substr(start));
                break;
            }
            words.emplace_back(line.substr(start, end - start));
            start = end + 1;
        }
        return words;
    }

    std::string MakeReply(bool ok, std::string_view text)
    {
        std::string reply = ok ? "ok\n" : "error\n";
        reply.append(text);
        return reply;
    }

    bool ReadReply(std::string_view reply, bool& ok, std::string& text)
    {
        const std::size_t end = reply.find('\n');
        if (end == std::string_view::npos)
            return false;
        const std::string_view head = reply.substr(0, end);
        if (head != "ok" && head != "error")
            return false;
        ok = head == "ok";
        text = std::string(reply.substr(end + 1));
        return true;
    }

    bool ParseHours(std::string_view text, float& hours)
    {
        std::string number(text);
        for (char& character : number)
            character = character == ',' ? '.' : character;
        if (number.empty())
            return false;
        char* end = nullptr;
        const double value = std::strtod(number.c_str(), &end);
        if (end != number.c_str() + number.size() || !(value > 0.0) || value > 1000000.0)
            return false;
        hours = static_cast<float>(value);
        return true;
    }

    bool ParseGoal(const std::vector<std::string>& words, std::size_t first, Goal& goal, std::string& problem)
    {
        const std::string kind = first < words.size() ? words[first] : std::string();
        const std::size_t count = words.size() > first ? words.size() - first : 0;
        if (kind == "none" && count == 1)
        {
            goal.Kind = GoalKind::None;
            return true;
        }
        if ((kind == "for" || kind == "until") && count == 2)
        {
            float hours = 0.0f;
            if (!ParseHours(words[first + 1], hours))
            {
                problem = "That is not a number of hours: " + words[first + 1];
                return false;
            }
            goal.Kind = kind == "for" ? GoalKind::ForHours : GoalKind::UntilTotal;
            goal.Hours = hours;
            return true;
        }
        problem = "A goal is: none, for <hours> or until <hours>.";
        return false;
    }
}
