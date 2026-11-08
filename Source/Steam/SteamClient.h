#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Idler::Steam
{
    struct IClient
    {
        virtual int CreateSteamPipe() = 0;
        virtual bool BReleaseSteamPipe(int pipe) = 0;
        virtual int ConnectToGlobalUser(int pipe) = 0;
        virtual int CreateLocalUser(int* pipe, int accountType) = 0;
        virtual void ReleaseUser(int pipe, int user) = 0;
        virtual void* GetISteamUser(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamGameServer(int user, int pipe, const char* version) = 0;
        virtual void SetLocalIPBinding(const void* address, std::uint16_t port) = 0;
        virtual void* GetISteamFriends(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamUtils(int pipe, const char* version) = 0;
        virtual void* GetISteamMatchmaking(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamMatchmakingServers(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamGenericInterface(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamUserStats(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamGameServerStats(int user, int pipe, const char* version) = 0;
        virtual void* GetISteamApps(int user, int pipe, const char* version) = 0;
    };

    struct IUser
    {
        virtual int GetHSteamUser() = 0;
        virtual bool BLoggedOn() = 0;
#ifdef _WIN32
        virtual std::uint64_t* GetSteamID(std::uint64_t* id) = 0;
#else
        virtual std::uint64_t GetSteamID() = 0;
#endif
    };

    struct IFriends
    {
        virtual const char* GetPersonaName() = 0;
    };

    struct IUserStats
    {
        virtual bool RequestCurrentStats() = 0;
#ifdef _WIN32
        virtual bool GetStatFloat(const char* name, float* value) = 0;
        virtual bool GetStatInt(const char* name, std::int32_t* value) = 0;
        virtual bool SetStatFloat(const char* name, float value) = 0;
        virtual bool SetStatInt(const char* name, std::int32_t value) = 0;
#else
        virtual bool GetStatInt(const char* name, std::int32_t* value) = 0;
        virtual bool GetStatFloat(const char* name, float* value) = 0;
        virtual bool SetStatInt(const char* name, std::int32_t value) = 0;
        virtual bool SetStatFloat(const char* name, float value) = 0;
#endif
        virtual void Unused5() = 0;
        virtual void Unused6() = 0;
        virtual bool SetAchievement(const char* name) = 0;
        virtual bool ClearAchievement(const char* name) = 0;
        virtual bool GetAchievementAndUnlockTime(const char* name, bool* achieved, std::uint32_t* unlockTime) = 0;
        virtual bool StoreStats() = 0;
        virtual void Unused10() = 0;
        virtual const char* GetAchievementDisplayAttribute(const char* name, const char* key) = 0;
        virtual void Unused11() = 0;
        virtual std::uint32_t GetNumAchievements() = 0;
        virtual const char* GetAchievementName(std::uint32_t index) = 0;
    };

    struct IUtils
    {
        virtual std::uint32_t GetSecondsSinceAppActive() = 0;
        virtual std::uint32_t GetSecondsSinceComputerActive() = 0;
        virtual int GetConnectedUniverse() = 0;
        virtual std::uint32_t GetServerRealTime() = 0;
        virtual const char* GetIPCountry() = 0;
        virtual bool GetImageSize(int image, std::uint32_t* width, std::uint32_t* height) = 0;
        virtual bool GetImageRGBA(int image, std::uint8_t* pixels, int size) = 0;
        virtual bool GetCSERIPPort(std::uint32_t* address, std::uint16_t* port) = 0;
        virtual std::uint8_t GetCurrentBatteryPower() = 0;
        virtual std::uint32_t GetAppID() = 0;
    };

    struct IApps
    {
        virtual bool BIsSubscribed() = 0;
        virtual bool BIsLowViolence() = 0;
        virtual bool BIsCybercafe() = 0;
        virtual bool BIsVACBanned() = 0;
        virtual const char* GetCurrentGameLanguage() = 0;
        virtual const char* GetAvailableGameLanguages() = 0;
        virtual bool BIsSubscribedApp(std::uint32_t appId) = 0;
        virtual bool BIsDlcInstalled(std::uint32_t appId) = 0;
        virtual std::uint32_t GetEarliestPurchaseUnixTime(std::uint32_t appId) = 0;
        virtual bool BIsSubscribedFromFreeWeekend() = 0;
        virtual int GetDLCCount() = 0;
        virtual bool BGetDLCDataByIndex(int index, std::uint32_t* appId, bool* available, char* name, int size) = 0;
        virtual void InstallDLC(std::uint32_t appId) = 0;
        virtual void UninstallDLC(std::uint32_t appId) = 0;
        virtual void RequestAppProofOfPurchaseKey(std::uint32_t appId) = 0;
        virtual bool GetCurrentBetaName(char* name, int size) = 0;
        virtual bool MarkContentCorrupt(bool missingFilesOnly) = 0;
        virtual std::uint32_t GetInstalledDepots(std::uint32_t appId, std::uint32_t* depots, std::uint32_t count) = 0;
        virtual std::uint32_t GetAppInstallDir(std::uint32_t appId, char* folder, std::uint32_t size) = 0;
        virtual bool BIsAppInstalled(std::uint32_t appId) = 0;
    };

    inline constexpr int kCallbackSteamShutdown = 704;
    inline constexpr int kCallbackStatsReceived = 1101;
    inline constexpr int kCallbackStatsStored = 1102;
    struct StatsCallback
    {
        std::uint64_t GameId;
        int Result;
    };

    class Connection
    {
    public:
        Connection() = default;
        ~Connection();

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        bool Connect(const std::filesystem::path& library, std::string& error);

        void DrainCallbacks(const std::function<void(int callback, const void* data)>& handle);

        IUser* User() const { return m_user; }
        IUtils* Utils() const { return m_utils; }
        IApps* Apps() const { return m_apps; }
        IFriends* Friends() const { return m_friends; }
        IUserStats* UserStats() const { return m_userStats; }

    private:
        IClient* m_client = nullptr;
        int m_pipe = 0;
        int m_globalUser = 0;
        IUser* m_user = nullptr;
        IUtils* m_utils = nullptr;
        IApps* m_apps = nullptr;
        IFriends* m_friends = nullptr;
        IUserStats* m_userStats = nullptr;
        void* m_getCallback = nullptr;
        void* m_freeCallback = nullptr;
    };
}
