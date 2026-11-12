#pragma once

#if defined(IDLER_CLI)
#error "The CLI takes App/CardService.h from CLI/Shim: put CLI/Shim first in the include path"
#endif

#include "App/WebPanel.h"
#include "Core/Cards.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
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
        explicit CardService(std::filesystem::path baseFile);
        ~CardService();

        CardService(const CardService&) = delete;
        CardService& operator=(const CardService&) = delete;

        LoginState State() const;
        std::string Problem() const;
        std::string AccountName() const;
        std::uint64_t LoginSteamId() const;

        bool Reading() const { return m_reading; }
        std::uint32_t LastFullRead() const;
        std::uint64_t Generation() const { return m_generation; }
        std::unordered_map<std::uint32_t, std::uint32_t> Drops() const;

        std::vector<ReceivedCard> Received() const;
        std::uint64_t ReceivedGeneration() const { return m_receivedGeneration; }
        std::filesystem::path PictureFile(std::string_view picture) const;
        std::uint64_t PicturesGeneration() const { return m_picturesGeneration; }
        bool KnowsCardSet(std::uint32_t appId) const;

        void OpenSignIn(void* window, bool preview);
        void CloseSignIn();
        WebPanel& Web() { return m_web; }
        void SetWindow(void* window);
        void Pump();
        void CloseWeb();

        void SignOut();
        void ReadAll();
        void ReadGames(std::vector<std::uint32_t> appIds);
        void SetPretendSignedIn();

    private:
        void Work();
        void DoReadAll();
        void DoReadGames(const std::vector<std::uint32_t>& appIds);
        bool ReadPage(const std::string& path, std::string& html);
        void NoteUser(std::uint64_t steamId, std::string name);
        void LoadLog();
        void SaveLog();
        void FetchPictures();
        void SetProblem(std::string problem);
        void SetState(LoginState state);
        bool Pause(int milliseconds);

        std::filesystem::path m_webFolder;
        std::filesystem::path m_logFile;
        std::filesystem::path m_pictureFolder;

        mutable std::mutex m_lock;
        std::condition_variable m_wake;
        std::thread m_thread;
        bool m_stop = false;
        bool m_wantCheck = false;
        bool m_wantClear = false;
        bool m_wantReadAll = false;
        std::vector<std::uint32_t> m_wantGames;
        bool m_wantPretend = false;

        LoginState m_state = LoginState::SignedOut;
        std::string m_problem;
        std::string m_accountName;
        std::uint64_t m_steamId = 0;
        std::unordered_map<std::uint32_t, std::uint32_t> m_drops;
        std::uint32_t m_lastFullRead = 0;
        CardLog m_log;

        std::wstring m_pageUrl;
        std::uint64_t m_pageRequest = 0;
        std::uint64_t m_pageLoading = 0;
        std::uint64_t m_pageDone = 0;
        WebPage m_page;

        std::atomic<bool> m_reading{ false };
        std::atomic<std::uint64_t> m_generation{ 0 };
        std::atomic<std::uint64_t> m_receivedGeneration{ 0 };
        std::atomic<std::uint64_t> m_picturesGeneration{ 0 };

        bool m_pretend = false;

        WebPanel m_web;
        void* m_window = nullptr;
        bool m_signingIn = false;
        bool m_clearing = false;
        bool m_clearDone = false;
        unsigned long long m_webLastUsed = 0;
    };
}
