#include "Core/Cards.h"

#include "Core/Text.h"

#include <algorithm>
#include <charconv>

namespace Idler
{
    namespace
    {
        constexpr std::string_view kRowMarker = "badge_row_overlay";
        constexpr std::string_view kDropsMarker = "progress_info_bold";

        bool IsLoggedIn(std::string_view html)
        {
            return html.find("g_steamID = \"") != std::string_view::npos;
        }

        std::uint32_t LeadingNumber(std::string_view text)
        {
            std::uint32_t value = 0;
            std::from_chars(text.data(), text.data() + text.size(), value);
            return value;
        }

        std::optional<std::uint32_t> DropsIn(std::string_view section)
        {
            const auto marker = section.find(kDropsMarker);
            if (marker == std::string_view::npos)
                return std::nullopt;
            const auto open = section.find('>', marker);
            if (open == std::string_view::npos)
                return std::nullopt;
            const auto close = section.find('<', open);
            const std::string_view text = Trim(section.substr(open + 1, close == std::string_view::npos ? std::string_view::npos : close - open - 1));
            if (text.find("card drop") == std::string_view::npos)
                return std::nullopt;
            return LeadingNumber(text);
        }

        std::uint64_t LeadingNumber64(std::string_view text)
        {
            std::uint64_t value = 0;
            std::from_chars(text.data(), text.data() + text.size(), value);
            return value;
        }

        bool IsLetterOrDigit(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        }

        std::string CleanName(std::string_view text)
        {
            static constexpr std::pair<std::string_view, char> kEscapes[] = {
                { "&amp;", '&' }, { "&quot;", '"' }, { "&#39;", '\'' }, { "&#039;", '\'' }, { "&apos;", '\'' }, { "&lt;", '<' }, { "&gt;", '>' },
            };

            std::string out;
            out.reserve(text.size());
            for (std::size_t at = 0; at < text.size();)
            {
                bool escaped = false;
                for (const auto& [code, plain] : kEscapes)
                {
                    if (text.substr(at).starts_with(code))
                    {
                        out.push_back(plain);
                        at += code.size();
                        escaped = true;
                        break;
                    }
                }
                if (escaped)
                    continue;
                const char c = text[at++];
                out.push_back(static_cast<unsigned char>(c) < 0x20 ? ' ' : c);
            }
            return out;
        }

        std::vector<std::string_view> Fields(std::string_view line, std::size_t count)
        {
            std::vector<std::string_view> fields;
            while (fields.size() + 1 < count)
            {
                const auto bar = line.find('|');
                if (bar == std::string_view::npos)
                    break;
                fields.push_back(line.substr(0, bar));
                line.remove_prefix(bar + 1);
            }
            fields.push_back(line);
            return fields;
        }
    }

    BadgePage ParseBadgePage(std::string_view html)
    {
        BadgePage page;
        page.LoggedIn = IsLoggedIn(html);

        for (const std::string_view link : { std::string_view("?p="), std::string_view("&p="), std::string_view("&amp;p=") })
        {
            for (auto at = html.find(link); at != std::string_view::npos; at = html.find(link, at + link.size()))
                page.LastPage = (std::max)(page.LastPage, static_cast<int>(LeadingNumber(html.substr(at + link.size(), 6))));
        }

        auto row = html.find(kRowMarker);
        while (row != std::string_view::npos)
        {
            const auto next = html.find(kRowMarker, row + kRowMarker.size());
            const std::string_view section = html.substr(row, next == std::string_view::npos ? std::string_view::npos : next - row);
            row = next;

            const auto link = section.find("/gamecards/");
            if (link == std::string_view::npos)
                continue;
            const auto linkEnd = section.find('"', link);
            const std::string_view address = section.substr(link, linkEnd == std::string_view::npos ? std::string_view::npos : linkEnd - link);
            if (address.find("border=1") != std::string_view::npos)
                continue;

            const std::uint32_t appId = LeadingNumber(address.substr(11));
            const auto drops = DropsIn(section);
            if (appId != 0 && drops)
                page.Games.push_back({ appId, *drops });
        }
        return page;
    }

    std::optional<std::uint32_t> ParseCardPage(std::string_view html)
    {
        if (!IsLoggedIn(html))
            return std::nullopt;
        return DropsIn(html);
    }

    std::vector<GameCard> ParseCardSet(std::string_view html)
    {
        constexpr std::string_view kCard = "badge_card_set_card ";
        constexpr std::string_view kPicture = "class=\"gamecard\" src=\"";
        constexpr std::string_view kCount = "badge_card_set_text_qty\">(";
        constexpr std::string_view kTitle = "badge_card_set_title";

        std::vector<GameCard> cards;
        auto at = html.find(kCard);
        while (at != std::string_view::npos)
        {
            const auto next = html.find(kCard, at + kCard.size());
            const std::string_view section = html.substr(at + kCard.size(), next == std::string_view::npos ? std::string_view::npos : next - at - kCard.size());
            at = next;

            const auto title = section.find(kTitle);
            if (title == std::string_view::npos)
                continue;
            auto nameAt = section.find('>', title);
            if (nameAt == std::string_view::npos)
                continue;
            ++nameAt;

            GameCard card;
            const auto closeTitle = section.find("clear: right", title);
            const auto count = section.find(kCount, title);
            if (count != std::string_view::npos && (closeTitle == std::string_view::npos || count < closeTitle))
            {
                card.Owned = LeadingNumber(section.substr(count + kCount.size(), 8));
                constexpr std::string_view kClose = "</div>";
                const auto close = section.find(kClose, count);
                if (close == std::string_view::npos)
                    continue;
                nameAt = close + kClose.size();
            }
            else if (section.starts_with("owned"))
            {
                card.Owned = 1;
            }
            const auto nameEnd = section.find('<', nameAt);
            card.Name = CleanName(Trim(section.substr(nameAt, nameEnd == std::string_view::npos ? std::string_view::npos : nameEnd - nameAt)));
            if (card.Name.empty())
                continue;

            if (const auto picture = section.find(kPicture); picture != std::string_view::npos)
            {
                const auto begin = picture + kPicture.size();
                const auto end = section.find('"', begin);
                const std::string_view address = section.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin);
                if (IsCardPicture(address))
                    card.Picture = address;
            }
            cards.push_back(std::move(card));
        }
        return cards;
    }

    bool IsCardPicture(std::string_view address)
    {
        constexpr std::string_view kScheme = "https://";
        constexpr std::string_view kHostEnd = ".steamstatic.com";
        constexpr std::string_view kPath = "/economy/image/";
        if (!address.starts_with(kScheme))
            return false;
        const std::string_view rest = address.substr(kScheme.size());
        const auto slash = rest.find('/');
        if (slash == std::string_view::npos)
            return false;
        const std::string_view host = rest.substr(0, slash);
        const std::string_view path = rest.substr(slash);
        if (host.size() <= kHostEnd.size() || !host.ends_with(kHostEnd) || path.size() <= kPath.size() || !path.starts_with(kPath))
            return false;
        return std::all_of(host.begin(), host.end(), [](char c) { return IsLetterOrDigit(c) || c == '.' || c == '-'; }) &&
            std::all_of(path.begin(), path.end(), [](char c) { return IsLetterOrDigit(c) || c == '/' || c == '-' || c == '_'; });
    }

    bool CardLog::NotePage(std::uint64_t account, std::uint32_t appId, std::uint32_t drops, std::vector<GameCard> cards, std::uint32_t now)
    {
        const auto key = std::make_pair(account, appId);
        const auto known = m_sets.find(key);
        bool changed = known == m_sets.end();

        if (!changed)
        {
            const CardSet& before = known->second;
            changed = before.Drops != drops || before.Cards.size() != cards.size();
            std::uint32_t gone = before.Drops > drops ? before.Drops - drops : 0;
            for (const GameCard& card : cards)
            {
                const auto old = std::find_if(before.Cards.begin(), before.Cards.end(), [&](const GameCard& other) { return other.Name == card.Name; });
                std::uint32_t had = old != before.Cards.end() ? old->Owned : 0;
                changed = changed || had != card.Owned;
                for (; had < card.Owned && gone > 0; ++had, --gone)
                    m_received.push_back({ account, now, appId, card.Name, card.Picture });
            }
            for (; gone > 0; --gone)
                m_received.push_back({ account, now, appId, {}, {} });
        }

        CardSet& set = m_sets[key];
        set.Drops = drops;
        set.Cards = std::move(cards);
        return changed;
    }

    bool CardLog::Knows(std::uint64_t account, std::uint32_t appId) const
    {
        return m_sets.contains(std::make_pair(account, appId));
    }

    std::vector<std::uint32_t> CardLog::Fallen(std::uint64_t account, const std::unordered_map<std::uint32_t, std::uint32_t>& drops) const
    {
        std::vector<std::uint32_t> fallen;
        for (const auto& [key, set] : m_sets)
        {
            if (key.first != account)
                continue;
            const auto found = drops.find(key.second);
            if (set.Drops > (found != drops.end() ? found->second : 0))
                fallen.push_back(key.second);
        }
        return fallen;
    }

    std::unordered_map<std::uint32_t, std::uint32_t> CardLog::KnownDrops(std::uint64_t account) const
    {
        std::unordered_map<std::uint32_t, std::uint32_t> drops;
        for (const auto& [key, set] : m_sets)
        {
            if (key.first == account)
                drops[key.second] = set.Drops;
        }
        return drops;
    }

    std::string CardLog::ToText() const
    {
        std::string text;
        for (const ReceivedCard& card : m_received)
            text += "Card|" + std::to_string(card.Account) + "|" + std::to_string(card.Time) + "|" + std::to_string(card.AppId) + "|" + card.Picture + "|" + card.Name + "\n";
        for (const auto& [key, set] : m_sets)
        {
            const std::string game = std::to_string(key.first) + "|" + std::to_string(key.second) + "|";
            text += "Set|" + game + std::to_string(set.Drops) + "\n";
            for (const GameCard& card : set.Cards)
                text += "Has|" + game + std::to_string(card.Owned) + "|" + card.Name + "\n";
        }
        return text;
    }

    CardLog CardLog::FromText(std::string_view text)
    {
        CardLog log;
        while (!text.empty())
        {
            const auto end = text.find('\n');
            const std::string_view line = Trim(text.substr(0, end));
            text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);

            if (line.starts_with("Card|"))
            {
                const auto fields = Fields(line, 6);
                if (fields.size() == 6)
                    log.m_received.push_back({ LeadingNumber64(fields[1]), LeadingNumber(fields[2]), LeadingNumber(fields[3]), std::string(fields[5]), IsCardPicture(fields[4]) ? std::string(fields[4]) : std::string() });
            }
            else if (line.starts_with("Set|"))
            {
                const auto fields = Fields(line, 4);
                if (fields.size() == 4)
                    log.m_sets[{ LeadingNumber64(fields[1]), LeadingNumber(fields[2]) }].Drops = LeadingNumber(fields[3]);
            }
            else if (line.starts_with("Has|"))
            {
                const auto fields = Fields(line, 5);
                if (fields.size() == 5)
                    log.m_sets[{ LeadingNumber64(fields[1]), LeadingNumber(fields[2]) }].Cards.push_back({ std::string(fields[4]), {}, LeadingNumber(fields[3]) });
            }
        }
        return log;
    }
}
