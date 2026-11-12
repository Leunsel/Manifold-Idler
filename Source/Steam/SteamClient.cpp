#include "Steam/SteamClient.h"

#include "Core/Errors.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Idler::Steam
{
    namespace
    {
        constexpr char kClientVersion[] = "SteamClient020";
        constexpr char kUserVersion[] = "SteamUser020";
        constexpr char kUtilsVersion[] = "SteamUtils009";
        constexpr char kAppsVersion[] = "STEAMAPPS_INTERFACE_VERSION008";
        constexpr char kFriendsVersion[] = "SteamFriends017";
        constexpr char kUserStatsVersion[] = "STEAMUSERSTATS_INTERFACE_VERSION011";

        struct CallbackMessage
        {
            int User;
            int Callback;
            std::uint8_t* Data;
            int Size;
        };

        using CreateInterfaceFn = void* (*)(const char* version, int* result);
        using GetCallbackFn = bool (*)(int pipe, CallbackMessage* message, int* call);
        using FreeCallbackFn = bool (*)(int pipe);

#ifdef _WIN32
        void* OpenLibrary(const std::filesystem::path& library)
        {
            return ::LoadLibraryExW(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }

        void* FindSymbol(void* module, const char* name)
        {
            return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(module), name));
        }
#else
        void* OpenLibrary(const std::filesystem::path& library)
        {
            return ::dlopen(library.c_str(), RTLD_NOW);
        }

        void* FindSymbol(void* module, const char* name)
        {
            return ::dlsym(module, name);
        }
#endif
    }

    Connection::~Connection()
    {
        if (m_client && m_pipe)
        {
            if (m_globalUser)
                m_client->ReleaseUser(m_pipe, m_globalUser);
            m_client->BReleaseSteamPipe(m_pipe);
        }
    }

    bool Connection::Connect(const std::filesystem::path& library, std::string& error)
    {
        void* module = OpenLibrary(library);
        if (!module)
        {
            error = ErrorMessage(ErrorCode::ClientLibraryNotLoaded);
            return false;
        }

        const auto create = reinterpret_cast<CreateInterfaceFn>(FindSymbol(module, "CreateInterface"));
        m_getCallback = FindSymbol(module, "Steam_BGetCallback");
        m_freeCallback = FindSymbol(module, "Steam_FreeLastCallback");
        if (!create)
        {
            error = ErrorMessage(ErrorCode::NoCreateInterface);
            return false;
        }

        m_client = static_cast<IClient*>(create(kClientVersion, nullptr));
        if (!m_client)
        {
            error = ErrorMessage(ErrorCode::ClientInterfaceRefused);
            return false;
        }

        m_pipe = m_client->CreateSteamPipe();
        if (!m_pipe)
        {
            error = ErrorMessage(ErrorCode::NoSteamPipe);
            return false;
        }
        m_globalUser = m_client->ConnectToGlobalUser(m_pipe);
        if (!m_globalUser)
        {
            error = ErrorMessage(ErrorCode::NoGlobalUser);
            return false;
        }

        m_user = static_cast<IUser*>(m_client->GetISteamUser(m_globalUser, m_pipe, kUserVersion));
        m_utils = static_cast<IUtils*>(m_client->GetISteamUtils(m_pipe, kUtilsVersion));
        m_apps = static_cast<IApps*>(m_client->GetISteamApps(m_globalUser, m_pipe, kAppsVersion));
        m_friends = static_cast<IFriends*>(m_client->GetISteamFriends(m_globalUser, m_pipe, kFriendsVersion));
        m_userStats = static_cast<IUserStats*>(m_client->GetISteamUserStats(m_globalUser, m_pipe, kUserStatsVersion));
        if (!m_user || !m_utils || !m_apps || !m_friends || !m_userStats)
        {
            error = ErrorMessage(ErrorCode::InterfaceMissing);
            return false;
        }
        return true;
    }

    void Connection::DrainCallbacks(const std::function<void(int callback, const void* data)>& handle)
    {
        if (!m_getCallback || !m_freeCallback || !m_pipe)
            return;

        const auto get = reinterpret_cast<GetCallbackFn>(m_getCallback);
        const auto free = reinterpret_cast<FreeCallbackFn>(m_freeCallback);
        CallbackMessage message{};
        int call = 0;
        while (get(m_pipe, &message, &call))
        {
            handle(message.Callback, message.Data);
            free(m_pipe);
        }
    }
}
