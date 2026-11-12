#pragma once

#include "Core/Cards.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace Idler
{
    enum class LoginState
    {
        SignedOut,
        Checking,
        SignedIn
    };

    class CardService
    {
    public:
        explicit CardService(const std::filesystem::path&) {}

        LoginState State() const { return LoginState::SignedOut; }
        std::uint64_t LoginSteamId() const { return 0; }
        std::uint32_t LastFullRead() const { return 0; }
        std::uint64_t Generation() const { return 0; }
        std::unordered_map<std::uint32_t, std::uint32_t> Drops() const { return {}; }
        std::vector<ReceivedCard> Received() const { return {}; }
        std::uint64_t ReceivedGeneration() const { return 0; }
        bool KnowsCardSet(std::uint32_t) const { return false; }
        void Pump() {}
        void ReadAll() {}
        void ReadGames(std::vector<std::uint32_t>) {}
    };
}
