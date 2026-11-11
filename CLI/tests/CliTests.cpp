#include "Paths.h"
#include "Request.h"
#include "Service.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
    int g_checks = 0;
    int g_failed = 0;

    void Check(bool ok, const char* what, int line)
    {
        ++g_checks;
        if (!ok)
        {
            ++g_failed;
            std::printf("FAILED line %d: %s\n", line, what);
        }
    }
}

#define CHECK(condition) Check((condition), #condition, __LINE__)

using namespace Idler;
using namespace Idler::Cli;

int main()
{
    const std::vector<std::string> words{ "games", "50", "half life" };
    CHECK(SplitRequest(JoinRequest(words)) == words);
    const std::vector<std::string> withEmpty{ "games", "50", "" };
    CHECK(SplitRequest(JoinRequest(withEmpty)) == withEmpty);
    CHECK(SplitRequest("").empty());
    CHECK(SplitRequest("status") == std::vector<std::string>{ "status" });
    const std::vector<std::string> sneaky{ "use", "a\tb\nc" };
    const std::vector<std::string> cleaned{ "use", "a b c" };
    CHECK(SplitRequest(JoinRequest(sneaky)) == cleaned);

    bool ok = false;
    std::string text;
    CHECK(ReadReply(MakeReply(true, "fine\nsecond"), ok, text) && ok && text == "fine\nsecond");
    CHECK(ReadReply(MakeReply(false, "no"), ok, text) && !ok && text == "no");
    CHECK(ReadReply(MakeReply(true, ""), ok, text) && ok && text.empty());
    CHECK(!ReadReply("", ok, text));
    CHECK(!ReadReply("ok", ok, text));
    CHECK(!ReadReply("maybe\nfine", ok, text));

    float hours = 0.0f;
    CHECK(ParseHours("3", hours) && hours == 3.0f);
    CHECK(ParseHours("1.5", hours) && hours == 1.5f);
    CHECK(ParseHours("1,5", hours) && hours == 1.5f);
    CHECK(!ParseHours("", hours));
    CHECK(!ParseHours("0", hours));
    CHECK(!ParseHours("-1", hours));
    CHECK(!ParseHours("3h", hours));
    CHECK(!ParseHours("nan", hours));
    CHECK(!ParseHours("inf", hours));
    CHECK(!ParseHours("9999999", hours));

    Goal goal;
    std::string problem;
    CHECK(ParseGoal({ "goal", "620", "none" }, 2, goal, problem) && goal.Kind == GoalKind::None);
    CHECK(ParseGoal({ "goal", "620", "for", "3" }, 2, goal, problem) && goal.Kind == GoalKind::ForHours && goal.Hours == 3.0f);
    CHECK(ParseGoal({ "until", "100,5" }, 0, goal, problem) && goal.Kind == GoalKind::UntilTotal && goal.Hours == 100.5f);
    CHECK(!ParseGoal({ "goal", "620" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "for" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "for", "soon" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "none", "3" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "cards" }, 2, goal, problem));

    CHECK(SocketName("a/idler.ini") == SocketName("a/idler.ini"));
    CHECK(SocketName("a/idler.ini") != SocketName("b/idler.ini"));
    CHECK(SocketName("a/idler.ini").size() < 50);
    CHECK(SocketFile("a/idler.ini").filename().string().find(SocketName("a/idler.ini")) != std::string::npos);

    CHECK(QuoteForUnit("/home/x/idler") == "\"/home/x/idler\"");
    CHECK(QuoteForUnit("/home/a b/100%/$HOME/\"q\"\\") == "\"/home/a b/100%%/$$HOME/\\\"q\\\"\\\\\"");
    CHECK(ServiceName("/home/x/.config/manifold-idler/idler.ini", "/home/x/.config/manifold-idler/idler.ini") == "manifold-idler");
    CHECK(ServiceName("/home/x/.config/manifold-idler/./idler.ini", "/home/x/.config/manifold-idler/idler.ini") == "manifold-idler");
    CHECK(ServiceName("/srv/night shift.ini", "/home/x/.config/manifold-idler/idler.ini") == "manifold-idler-night-shift");
    const std::string unit = ServiceUnit("/home/x/bin/idler", "/home/x/.config/manifold-idler/idler.ini", false);
    CHECK(unit.find("ExecStart=\"/home/x/bin/idler\" --settings \"/home/x/.config/manifold-idler/idler.ini\" serve\n") != std::string::npos);
    CHECK(unit.find("Restart=on-failure\n") != std::string::npos);
    CHECK(unit.find("WantedBy=default.target\n") != std::string::npos);
    CHECK(ServiceUnit("/home/x/bin/idler", "/home/x/idler.ini", true).find(" serve --start\n") != std::string::npos);

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
