#include "TestKit.h"

#include "Core/Cards.h"
#include "Core/Library.h"

using namespace Idler;

namespace Tests
{
    void TestCardQueue()
    {
        QueueEntry waiting = Entry(1, GoalKind::CardDrops);
        CHECK(!IsFinished(waiting));
        waiting.DropsLeft = 2;
        CHECK(!IsFinished(waiting));
        CHECK(Progress(waiting) < 0.0f && SecondsLeft(waiting) == 0.0);
        waiting.DropsLeft = 0;
        CHECK(IsFinished(waiting));

        const double warmup = 3.0 * 3600.0;
        std::vector<QueueEntry> queue = { Entry(1, GoalKind::CardDrops, 0.0f, 0.0, 200), Entry(2, GoalKind::CardDrops, 0.0f, 0.0, 10), Entry(3, GoalKind::CardDrops, 0.0f, 3600.0, 0), Entry(4, GoalKind::CardDrops) };
        CHECK(PlayedSeconds(queue[0]) == 200.0 * 60.0);
        CHECK(PlayedSeconds(queue[2]) == 3600.0);
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 1 }));
        std::vector<QueueEntry> warmedLast = { Entry(2, GoalKind::CardDrops, 0.0f, 0.0, 10), Entry(3, GoalKind::CardDrops), Entry(1, GoalKind::CardDrops, 0.0f, 0.0, 200), Entry(5, GoalKind::CardDrops, 0.0f, 0.0, 300) };
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 1 }));
        warmedLast[2].DropsLeft = 0;
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 5 }));
        warmedLast[3].Failed = true;
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 2, 3 }));
        warmedLast[3].Failed = false;
        warmedLast[3].GuardSecondsLeft = 0.0;
        CHECK((WantedForCards(warmedLast, warmup) == std::vector<std::uint32_t>{ 2, 3 }));

        queue[0].DropsLeft = 0;
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 2, 3, 4 }));
        queue[2].IdledSeconds = warmup;
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 3 }));
        queue[2].DropsLeft = 0;
        CHECK((WantedForCards(queue, warmup) == std::vector<std::uint32_t>{ 2, 4 }));
        queue[2].DropsLeft = -1;
        queue[2].IdledSeconds = 3600.0;

        queue[0].DropsLeft = -1;
        CHECK((WantedForCards(queue, 0.0) == std::vector<std::uint32_t>{ 1 }));
        queue[0].DropsLeft = 0;
        CHECK((WantedForCards(queue, 0.0) == std::vector<std::uint32_t>{ 2 }));
        queue[1].Failed = true;
        queue[2].GuardSecondsLeft = 0.0;
        CHECK((WantedForCards(queue, 0.0) == std::vector<std::uint32_t>{ 4 }));
        queue[3].DropsLeft = 0;
        CHECK(WantedForCards(queue, 0.0).empty());
        CHECK(!HasWorkLeft(queue));

        std::vector<QueueEntry> many;
        for (std::uint32_t appId = 1; appId <= 40; ++appId)
            many.push_back(Entry(appId, GoalKind::CardDrops));
        CHECK(WantedForCards(many, warmup).size() == kMaxSlots);

        CHECK(ForecastQueue({ Entry(1, GoalKind::CardDrops) }, 1).Endless);
    }

    void TestCardPages()
    {
        const std::string loggedIn = R"(<script>g_steamID = "76561198000000000";</script>)";
        const auto row = [](const std::string& href, const std::string& drops)
            {
                return R"(<div class="badge_row is_link"><a class="badge_row_overlay" href="https://steamcommunity.com/id/x)" + href + R"("></a>)" +
                    R"(<div class="badge_title_stats_drops">)" + drops + "</div></div>";
            };

        const std::string page = loggedIn +
            row("/gamecards/730/", R"(<span class="progress_info_bold">3 card drops remaining</span>)") +
            row("/gamecards/440/", R"(<span class="progress_info_bold">1 card drop remaining</span>)") +
            row("/gamecards/570/", R"(<span class="progress_info_bold">No card drops remaining</span>)") +
            row("/gamecards/730/?border=1", R"(<span class="progress_info_bold">9 card drops remaining</span>)") +
            row("/gamecards/620/", R"(<span class="progress_info_bold">Something else</span>)") +
            row("/gamecards/630/", "") +
            R"(<a class="badge_row_overlay" href="https://steamcommunity.com/id/x/badges/1"></a>)" +
            R"(<a class="pagelink" href="?p=2">2</a><a class="pagelink" href="?p=12">12</a>)";
        const BadgePage parsed = ParseBadgePage(page);
        CHECK(parsed.LoggedIn);
        CHECK(parsed.LastPage == 12);
        CHECK(parsed.Games.size() == 3);
        if (parsed.Games.size() == 3)
        {
            CHECK(parsed.Games[0].AppId == 730 && parsed.Games[0].Drops == 3);
            CHECK(parsed.Games[1].AppId == 440 && parsed.Games[1].Drops == 1);
            CHECK(parsed.Games[2].AppId == 570 && parsed.Games[2].Drops == 0);
        }

        CHECK(ParseBadgePage(R"(<a class="pagelink" href="?l=english&p=7">7</a>)").LastPage == 7);
        CHECK(ParseBadgePage(R"(<a class="pagelink" href="?l=english&amp;p=9">9</a>)").LastPage == 9);
        CHECK(ParseBadgePage(R"(<a href="/search?q=3">no page</a>)").LastPage == 1);

        const BadgePage visitor = ParseBadgePage(R"(<script>g_steamID = false;</script>)" + row("/gamecards/730/", ""));
        CHECK(!visitor.LoggedIn && visitor.Games.empty() && visitor.LastPage == 1);
        CHECK(ParseBadgePage("").Games.empty());

        CHECK(ParseCardPage(loggedIn + R"(<span class="progress_info_bold">2 card drops remaining</span>)") == 2u);
        CHECK(ParseCardPage(loggedIn + R"(<span class="progress_info_bold">No card drops remaining</span>)") == 0u);
        CHECK(!ParseCardPage(loggedIn + "<span>nothing</span>").has_value());
        CHECK(!ParseCardPage(R"(g_steamID = false; <span class="progress_info_bold">2 card drops remaining</span>)").has_value());

    }

    void TestMarkGamesWithCards()
    {
        std::vector<Game> games(3);
        games[0].AppId = 1;
        games[0].HasCards = true;
        games[1].AppId = 2;
        games[2].AppId = 3;

        CHECK(MarkGamesWithCards(games, { { 2, 0 }, { 99, 5 } }));
        CHECK(games[0].HasCards && games[1].HasCards && !games[2].HasCards);
        CHECK(!MarkGamesWithCards(games, { { 2, 0 } }));
        CHECK(!MarkGamesWithCards(games, {}));
        CHECK(games[0].HasCards && games[1].HasCards && !games[2].HasCards);
    }

    void TestCardSet()
    {
        const std::string_view page = R"(<div class="badge_card_set_cards">

	<div class="badge_card_set_card owned">
		<div data-panel="{&quot;focusable&quot;:true,&quot;clickOnActivate&quot;:true}" role="button" onclick="" class="game_card_ctn ">
			<img class="gamecard" src="https://community.fastly.steamstatic.com/economy/image/IzMF03bk9-Wp_Sb/96fx96f">
		</div>
		<div class="badge_card_set_text badge_card_set_title ellipsis">
			<div class="badge_card_set_text_qty">(4)</div>
			Anarchist										<div style="clear: right"></div>
		</div>
		<div class="badge_card_set_text ellipsis">
			1 of 5, Series 1																			</div>
	</div>
	<div class="badge_card_set_card unowned">
		<div class="game_card_ctn "><img class="gamecard" src="https://community.fastly.steamstatic.com/economy/image/Second"></div>
		<div class="badge_card_set_text badge_card_set_title ellipsis">
			Heavy &amp; Medic&#39;s Card										<div style="clear: right"></div>
		</div>
		<div class="badge_card_set_text ellipsis">2 of 5, Series 1</div>
	</div>
	<div class="badge_card_set_card owned">
		<div class="game_card_ctn "><img class="gamecard" src="https://example.com/economy/image/Third"></div>
		<div class="badge_card_set_text badge_card_set_title ellipsis">
			No number										<div style="clear: right"></div>
		</div>
	</div>
</div><div class="badge_card_set_text_qty">(9)</div>)";

        const std::vector<GameCard> cards = ParseCardSet(page);
        CHECK(cards.size() == 3);
        if (cards.size() == 3)
        {
            CHECK(cards[0].Name == "Anarchist" && cards[0].Owned == 4);
            CHECK(cards[0].Picture == "https://community.fastly.steamstatic.com/economy/image/IzMF03bk9-Wp_Sb/96fx96f");
            CHECK(cards[1].Name == "Heavy & Medic's Card" && cards[1].Owned == 0);
            CHECK(!cards[1].Picture.empty());
            CHECK(cards[2].Name == "No number" && cards[2].Owned == 1 && cards[2].Picture.empty());
        }
        CHECK(ParseCardSet("<div class=\"badge_card_set_cards\"></div>").empty());

        CHECK(IsCardPicture("https://community.fastly.steamstatic.com/economy/image/Ab-c_1/96fx96f"));
        CHECK(IsCardPicture("https://community.akamai.steamstatic.com/economy/image/x"));
        CHECK(!IsCardPicture("http://community.fastly.steamstatic.com/economy/image/x"));
        CHECK(!IsCardPicture("https://steamstatic.com.example.org/economy/image/x"));
        CHECK(!IsCardPicture("https://.steamstatic.com/economy/image/x"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/other/x"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/economy/image/"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/economy/image/../../x"));
        CHECK(!IsCardPicture("https://community.fastly.steamstatic.com/economy/image/x?y=1"));
        CHECK(!IsCardPicture(""));
    }

    void TestCardLog()
    {
        constexpr std::uint64_t kOne = 76561198000000001ull;
        constexpr std::uint64_t kTwo = 76561198000000002ull;
        const auto set = [](std::uint32_t a, std::uint32_t b)
            {
                return std::vector<GameCard>{ { "A|1", "https://x.steamstatic.com/economy/image/a", a }, { "B", "https://x.steamstatic.com/economy/image/b", b } };
            };

        CardLog log;
        CHECK(!log.Knows(kOne, 10));

        CHECK(log.NotePage(kOne, 10, 3, set(1, 0), 100));
        CHECK(log.Knows(kOne, 10) && !log.Knows(kTwo, 10) && !log.Knows(kOne, 11));
        CHECK(log.Received().empty());

        CHECK(!log.NotePage(kOne, 10, 3, set(1, 0), 200));
        CHECK(log.Received().empty());

        CHECK(log.NotePage(kOne, 10, 2, set(1, 1), 300));
        CHECK(log.Received().size() == 1);
        CHECK(log.Received().back().Name == "B" && log.Received().back().Time == 300 && log.Received().back().AppId == 10 && log.Received().back().Account == kOne);
        CHECK(log.Received().back().Picture == "https://x.steamstatic.com/economy/image/b");

        CHECK(log.NotePage(kOne, 10, 2, set(2, 1), 400));
        CHECK(log.Received().size() == 1);

        CHECK(log.NotePage(kOne, 10, 0, set(5, 1), 500));
        CHECK(log.Received().size() == 3);
        CHECK(log.Received()[1].Name == "A|1" && log.Received()[2].Name == "A|1");

        CardLog foil;
        foil.NotePage(kOne, 20, 1, set(1, 1), 100);
        CHECK(foil.NotePage(kOne, 20, 0, set(1, 1), 200));
        CHECK(foil.Received().size() == 1 && foil.Received()[0].Name.empty() && foil.Received()[0].Picture.empty());

        CardLog crafted;
        crafted.NotePage(kOne, 30, 2, set(3, 3), 100);
        crafted.NotePage(kOne, 30, 2, set(2, 2), 200);
        CHECK(crafted.Received().empty());

        CardLog other;
        other.NotePage(kOne, 1, 5, set(0, 0), 100);
        other.NotePage(kOne, 2, 3, set(0, 0), 100);
        other.NotePage(kOne, 3, 2, set(0, 0), 100);
        other.NotePage(kTwo, 2, 9, set(0, 0), 100);
        std::vector<std::uint32_t> fallen = other.Fallen(kOne, { { 1, 5 }, { 2, 2 } });
        std::sort(fallen.begin(), fallen.end());
        CHECK((fallen == std::vector<std::uint32_t>{ 2, 3 }));
        CHECK(other.KnownDrops(kTwo).size() == 1 && other.KnownDrops(kTwo).at(2) == 9);

        CardLog restored = CardLog::FromText(log.ToText());
        CHECK(restored.ToText() == log.ToText());
        CHECK(restored.Received().size() == 3 && restored.Received()[1].Name == "A|1");
        CHECK(restored.Received()[1].Picture == "https://x.steamstatic.com/economy/image/a");
        CHECK(restored.Knows(kOne, 10));
        CHECK(!restored.NotePage(kOne, 10, 0, set(5, 1), 600));

        const CardLog damaged = CardLog::FromText("Card|1\nnonsense\n\nSet|\nCard|1|2|3|https://example.com/economy/image/x|Name\r\n");
        CHECK(damaged.Received().size() == 1 && damaged.Received()[0].Name == "Name" && damaged.Received()[0].Picture.empty());
    }
}
