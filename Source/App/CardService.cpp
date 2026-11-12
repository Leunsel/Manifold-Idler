#include "App/CardService.h"

#include "Core/Clock.h"
#include "Core/Errors.h"
#include "Core/RuntimeLog.h"
#include "Core/WebPolicy.h"
#include "Steam/SteamWeb.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <utility>

#include "Core/Text.h"

namespace Idler
{
    namespace
    {
        constexpr int kPagePauseMilliseconds = 1200;
        constexpr int kMaxBadgePages = 100;
        constexpr int kPicturePauseMilliseconds = 200;
        constexpr int kPageWaitSeconds = 60;
        constexpr unsigned long long kWebIdleMilliseconds = 20000;
        constexpr wchar_t kCommunitySite[] = L"https://steamcommunity.com";
        constexpr wchar_t kSignInPage[] = L"https://steamcommunity.com/login/home/";

        bool ReplaceFile(const std::filesystem::path& target, const std::string& data)
        {
            auto temporary = target;
            temporary += L".tmp";
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                file.write(data.data(), static_cast<std::streamsize>(data.size()));
                if (!file)
                    return false;
            }
            std::error_code error;
            std::filesystem::rename(temporary, target, error);
            if (error)
            {
                std::filesystem::remove(temporary, error);
                return false;
            }
            return true;
        }
    }

    CardService::CardService(std::filesystem::path baseFile)
        : m_webFolder(std::filesystem::path(baseFile).replace_extension(L".web")),
        m_logFile(std::filesystem::path(baseFile).replace_extension(L".cards")),
        m_pictureFolder(std::filesystem::path(baseFile).replace_extension(L".card-pictures"))
    {
        if constexpr (kCardFarmEnabled)
            m_thread = std::thread(&CardService::Work, this);
    }

    CardService::~CardService()
    {
        {
            std::lock_guard lock(m_lock);
            m_stop = true;
        }
        m_wake.notify_all();
        if (m_thread.joinable())
            m_thread.join();
    }

    LoginState CardService::State() const
    {
        std::lock_guard lock(m_lock);
        return m_state;
    }

    std::string CardService::Problem() const
    {
        std::lock_guard lock(m_lock);
        return m_problem;
    }

    std::string CardService::AccountName() const
    {
        std::lock_guard lock(m_lock);
        return m_accountName;
    }

    std::uint64_t CardService::LoginSteamId() const
    {
        std::lock_guard lock(m_lock);
        return m_steamId;
    }

    std::uint32_t CardService::LastFullRead() const
    {
        std::lock_guard lock(m_lock);
        return m_lastFullRead;
    }

    std::unordered_map<std::uint32_t, std::uint32_t> CardService::Drops() const
    {
        std::lock_guard lock(m_lock);
        return m_drops;
    }

    std::vector<ReceivedCard> CardService::Received() const
    {
        std::lock_guard lock(m_lock);
        return m_log.Received();
    }

    std::filesystem::path CardService::PictureFile(std::string_view picture) const
    {
        if (picture.empty())
            return {};
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char c : picture)
        {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        wchar_t name[32];
        std::swprintf(name, std::size(name), L"%016llx.img", static_cast<unsigned long long>(hash));
        return m_pictureFolder / name;
    }

    bool CardService::KnowsCardSet(std::uint32_t appId) const
    {
        std::lock_guard lock(m_lock);
        return m_log.Knows(m_steamId, appId);
    }

    void CardService::OpenSignIn(void* window, bool preview)
    {
        m_signingIn = true;
        m_web.Open(window, m_webFolder, kSignInPage, true);
        if (preview)
            return;
        m_web.SetPageHandler([this](const WebPage& page)
            {
                if (page.SteamId == 0)
                    return;
                NoteUser(page.SteamId, page.UserName);
                {
                    std::lock_guard lock(m_lock);
                    m_problem.clear();
                    m_wantReadAll = true;
                }
                m_wake.notify_all();
            });
    }

    void CardService::CloseSignIn()
    {
        if (State() == LoginState::SignedIn)
            m_web.Hide();
        else
            m_web.Close();
        m_signingIn = false;
    }

    void CardService::SetWindow(void* window)
    {
        m_window = window;
    }

    void CardService::CloseWeb()
    {
        m_web.Close();
        m_window = nullptr;
    }

    void CardService::Pump()
    {
        if constexpr (kCardFarmEnabled)
        {
            m_web.Tick();
            if (!m_window || m_signingIn)
                return;

            if (m_clearDone)
            {
                m_clearing = false;
                m_clearDone = false;
                m_web.Close();
                std::lock_guard lock(m_lock);
                m_state = LoginState::SignedOut;
                m_steamId = 0;
                m_accountName.clear();
                m_drops.clear();
                m_lastFullRead = 0;
                m_problem.clear();
                ++m_generation;
                return;
            }

            bool clear = false;
            std::wstring url;
            std::uint64_t request = 0;
            bool waiting = false;
            bool keep = false;
            {
                std::lock_guard lock(m_lock);
                keep = m_state == LoginState::SignedIn;
                clear = std::exchange(m_wantClear, false);
                waiting = m_pageRequest != m_pageDone;
                if (waiting && m_pageRequest != m_pageLoading)
                {
                    m_pageLoading = m_pageRequest;
                    request = m_pageRequest;
                    url = m_pageUrl;
                }
            }

            const auto now = Clock::TickMilliseconds();
            if (clear)
            {
                m_clearing = true;
                if (!m_web.IsOpen())
                    m_web.Open(m_window, m_webFolder, {}, false);
                m_web.ClearData([this] { m_clearDone = true; });
            }
            if (!url.empty())
            {
                if (!m_web.IsOpen())
                    m_web.Open(m_window, m_webFolder, {}, false);
                m_web.Load(url, [this, request](const WebPage& page)
                    {
                        {
                            std::lock_guard lock(m_lock);
                            if (m_pageRequest == request)
                            {
                                m_page = page;
                                m_pageDone = request;
                            }
                        }
                        m_wake.notify_all();
                    });
            }
            if (waiting || m_clearing || keep)
                m_webLastUsed = now;
            else if (m_web.IsOpen() && now - m_webLastUsed > kWebIdleMilliseconds)
                m_web.Close();
        }
    }

    void CardService::SignOut()
    {
        LOG_INFO("Signed out of Steam's website");
        {
            std::lock_guard lock(m_lock);
            m_wantClear = true;
        }
        m_wake.notify_all();
    }

    void CardService::ReadAll()
    {
        {
            std::lock_guard lock(m_lock);
            m_wantReadAll = true;
        }
        m_wake.notify_all();
    }

    void CardService::ReadGames(std::vector<std::uint32_t> appIds)
    {
        {
            std::lock_guard lock(m_lock);
            for (const std::uint32_t appId : appIds)
            {
                if (std::find(m_wantGames.begin(), m_wantGames.end(), appId) == m_wantGames.end())
                    m_wantGames.push_back(appId);
            }
        }
        m_wake.notify_all();
    }

    void CardService::SetPretendSignedIn()
    {
        {
            std::lock_guard lock(m_lock);
            m_wantPretend = true;
        }
        m_wake.notify_all();
    }

    void CardService::SetProblem(std::string problem)
    {
        LOG_WARN(problem);
        std::lock_guard lock(m_lock);
        m_problem = std::move(problem);
    }

    void CardService::SetState(LoginState state)
    {
        std::lock_guard lock(m_lock);
        m_state = state;
    }

    bool CardService::Pause(int milliseconds)
    {
        std::unique_lock lock(m_lock);
        m_wake.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return m_stop; });
        return !m_stop;
    }

    void CardService::Work()
    {
        LoadLog();
        std::error_code folderError;
        if (std::filesystem::is_directory(m_webFolder, folderError))
        {
            std::lock_guard lock(m_lock);
            m_state = LoginState::Checking;
            m_wantCheck = true;
        }
        FetchPictures();
        std::uint64_t fetchedFor = m_receivedGeneration;

        while (true)
        {
            bool check = false;
            bool readAll = false;
            bool pretend = false;
            std::vector<std::uint32_t> games;
            {
                std::unique_lock lock(m_lock);
                m_wake.wait(lock, [this] { return m_stop || m_wantCheck || m_wantReadAll || !m_wantGames.empty() || m_wantPretend; });
                if (m_stop)
                    return;
                check = std::exchange(m_wantCheck, false);
                readAll = std::exchange(m_wantReadAll, false);
                games = std::exchange(m_wantGames, {});
                pretend = std::exchange(m_wantPretend, false);
            }

            if (pretend)
            {
                m_pretend = true;
                std::lock_guard lock(m_lock);
                m_steamId = m_log.Received().empty() ? 0 : m_log.Received().front().Account;
                m_state = LoginState::SignedIn;
                m_drops = m_log.KnownDrops(m_steamId);
                m_lastFullRead = static_cast<std::uint32_t>(std::time(nullptr));
                ++m_generation;
                continue;
            }
            if (m_pretend)
                continue;

            if (check)
            {
                std::string html;
                readAll = ReadPage("/?l=english", html) || readAll;
                if (State() == LoginState::Checking)
                    SetState(LoginState::SignedOut);
            }

            if (State() != LoginState::SignedIn)
                continue;
            m_reading = true;
            if (readAll)
                DoReadAll();
            if (!games.empty())
                DoReadGames(games);
            m_reading = false;

            if (fetchedFor != m_receivedGeneration)
            {
                fetchedFor = m_receivedGeneration;
                FetchPictures();
            }
        }
    }

    void CardService::NoteUser(std::uint64_t steamId, std::string name)
    {
        std::lock_guard lock(m_lock);
        if (m_state != LoginState::SignedIn || m_steamId != steamId)
            LOG_SUCCESS("Signed in to Steam as " + (name.empty() ? std::to_string(steamId) : name));
        m_steamId = steamId;
        if (!name.empty())
            m_accountName = std::move(name);
        m_state = LoginState::SignedIn;
    }

    bool CardService::ReadPage(const std::string& path, std::string& html)
    {
        std::unique_lock lock(m_lock);
        const std::uint64_t request = ++m_pageRequest;
        m_pageUrl = kCommunitySite + Utf8ToWide(path);
        const bool answered = m_wake.wait_for(lock, std::chrono::seconds(kPageWaitSeconds), [&] { return m_stop || m_pageDone == request; });
        if (m_stop)
            return false;
        if (!answered)
        {
            m_pageDone = request;
            m_problem = ErrorMessage(ErrorCode::WebsiteNoAnswer);
            LOG_WARN(path + ": " + m_problem);
            return false;
        }
        WebPage page = std::move(m_page);
        if (!page.Ok)
        {
            m_problem = ErrorMessage(ErrorCode::WebsiteUnreadable);
            LOG_WARN(path + ": " + m_problem);
            return false;
        }
        if (page.SteamId == 0)
        {
            LOG_INFO("Nobody signed in, says " + WithoutQuery(page.Address));
            if (m_state == LoginState::SignedIn)
                m_problem = ErrorMessage(ErrorCode::SignInAgain);
            m_state = LoginState::SignedOut;
            m_steamId = 0;
            m_drops.clear();
            m_lastFullRead = 0;
            ++m_generation;
            return false;
        }
        lock.unlock();
        NoteUser(page.SteamId, page.UserName);
        html = std::move(page.Html);
        return true;
    }

    void CardService::DoReadAll()
    {
        const std::string base = "/profiles/" + std::to_string(LoginSteamId()) + "/badges/?l=english&p=";
        std::unordered_map<std::uint32_t, std::uint32_t> drops;
        int lastPage = 1;
        for (int page = 1; page <= lastPage && page <= kMaxBadgePages;)
        {
            std::string html;
            if (!ReadPage(base + std::to_string(page), html))
                return;

            const BadgePage parsed = ParseBadgePage(html);
            if (!parsed.LoggedIn)
            {
                SetProblem(ErrorMessage(ErrorCode::DropsNotShown));
                return;
            }

            for (const CardDrops& game : parsed.Games)
                drops[game.AppId] = game.Drops;
            lastPage = parsed.LastPage;
            ++page;
            if (page <= lastPage && !Pause(kPagePauseMilliseconds))
                return;
        }

        std::vector<std::uint32_t> fallen;
        {
            std::lock_guard lock(m_lock);
            fallen = m_log.Fallen(m_steamId, drops);
            m_drops = std::move(drops);
            m_lastFullRead = static_cast<std::uint32_t>(std::time(nullptr));
            m_problem.clear();
            ++m_generation;
            LOG_SUCCESS("Badge pages read: " + std::to_string(m_drops.size()) + " games");
        }
        if (!fallen.empty())
            DoReadGames(fallen);
    }

    void CardService::DoReadGames(const std::vector<std::uint32_t>& appIds)
    {
        const std::uint64_t account = LoginSteamId();
        for (std::size_t index = 0; index < appIds.size();)
        {
            const std::string path = "/profiles/" + std::to_string(LoginSteamId()) + "/gamecards/" + std::to_string(appIds[index]) + "/?l=english";
            std::string html;
            if (!ReadPage(path, html))
                return;

            if (const auto drops = ParseCardPage(html))
            {
                bool changed = false;
                {
                    std::lock_guard lock(m_lock);
                    m_drops[appIds[index]] = *drops;
                    m_problem.clear();
                    ++m_generation;
                    const std::size_t before = m_log.Received().size();
                    changed = m_log.NotePage(account, appIds[index], *drops, ParseCardSet(html), static_cast<std::uint32_t>(std::time(nullptr)));
                    if (m_log.Received().size() != before)
                        ++m_receivedGeneration;
                }
                if (changed)
                    SaveLog();
            }
            ++index;
            if (index < appIds.size() && !Pause(kPagePauseMilliseconds))
                return;
        }
    }

    void CardService::LoadLog()
    {
        std::ifstream file(m_logFile, std::ios::binary);
        if (!file)
            return;
        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::lock_guard lock(m_lock);
        m_log = CardLog::FromText(text);
        ++m_receivedGeneration;
    }

    void CardService::SaveLog()
    {
        std::string text;
        {
            std::lock_guard lock(m_lock);
            text = m_log.ToText();
        }
        ReplaceFile(m_logFile, text);
    }

    void CardService::FetchPictures()
    {
        const std::vector<ReceivedCard> cards = Received();
        for (auto card = cards.rbegin(); card != cards.rend(); ++card)
        {
            const std::filesystem::path file = PictureFile(card->Picture);
            std::error_code error;
            if (file.empty() || std::filesystem::exists(file, error))
                continue;
            std::string bytes;
            if (!SteamWeb::ReadPicture(card->Picture, bytes))
                continue;
            std::filesystem::create_directories(m_pictureFolder, error);
            if (ReplaceFile(file, bytes))
                ++m_picturesGeneration;
            if (!Pause(kPicturePauseMilliseconds))
                return;
        }
    }
}
