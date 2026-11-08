#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Idler
{
#ifdef IDLER_CARD_FARM
    inline constexpr bool kCardFarmEnabled = true;
#else
    inline constexpr bool kCardFarmEnabled = false;
#endif

    struct CardDrops
    {
        std::uint32_t AppId = 0;
        std::uint32_t Drops = 0;
    };

    struct BadgePage
    {
        bool LoggedIn = false;
        int LastPage = 1;
        std::vector<CardDrops> Games;
    };

    BadgePage ParseBadgePage(std::string_view html);

    std::optional<std::uint32_t> ParseCardPage(std::string_view html);

    struct GameCard
    {
        std::string Name;
        std::string Picture;
        std::uint32_t Owned = 0;
    };

    std::vector<GameCard> ParseCardSet(std::string_view html);

    bool IsCardPicture(std::string_view address);

    struct ReceivedCard
    {
        std::uint64_t Account = 0;
        std::uint32_t Time = 0;
        std::uint32_t AppId = 0;
        std::string Name;
        std::string Picture;
    };

    class CardLog
    {
    public:
        bool NotePage(std::uint64_t account, std::uint32_t appId, std::uint32_t drops, std::vector<GameCard> cards, std::uint32_t now);
        bool Knows(std::uint64_t account, std::uint32_t appId) const;
        std::vector<std::uint32_t> Fallen(std::uint64_t account, const std::unordered_map<std::uint32_t, std::uint32_t>& drops) const;
        std::unordered_map<std::uint32_t, std::uint32_t> KnownDrops(std::uint64_t account) const;
        const std::vector<ReceivedCard>& Received() const { return m_received; }

        std::string ToText() const;
        static CardLog FromText(std::string_view text);

    private:
        struct CardSet
        {
            std::uint32_t Drops = 0;
            std::vector<GameCard> Cards;
        };

        std::map<std::pair<std::uint64_t, std::uint32_t>, CardSet> m_sets;
        std::vector<ReceivedCard> m_received;
    };
}
