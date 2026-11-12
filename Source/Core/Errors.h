#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#define IDLER_ERROR_CODES(X) \
    X(SteamNotRunning, 1001) \
    X(NobodyLoggedOn, 1002) \
    X(SteamOffline, 1003) \
    X(ClientLibraryNotLoaded, 1004) \
    X(NoCreateInterface, 1005) \
    X(ClientInterfaceRefused, 1006) \
    X(InterfaceMissing, 1007) \
    X(NoSteamPipe, 1008) \
    X(NoGlobalUser, 1009) \
    X(SteamWentAway, 1010) \
    X(GameNotOwned, 2001) \
    X(GameNotAccepted, 2002) \
    X(HelperNotStarted, 2003) \
    X(KeptStopping, 2004) \
    X(StoppedByItself, 2005) \
    X(DidNotEndInTime, 2006) \
    X(ShutdownFailed, 2007) \
    X(AppInfoUnreadable, 3001) \
    X(AppInfoNewerFormat, 3002) \
    X(AppInfoTooShort, 3003) \
    X(AppInfoStringsDamaged, 3004) \
    X(AppInfoEndsEarly, 3005) \
    X(AppInfoEntryDamaged, 3006) \
    X(ScanHelperNotStarted, 3007) \
    X(ScanHelperEnded, 3008) \
    X(ScanNoAnswer, 3009) \
    X(StatsNotSent, 4001) \
    X(NoAchievements, 4002) \
    X(StoreRefused, 4003) \
    X(StoreNotConfirmed, 4004) \
    X(ListHelperNotStarted, 4005) \
    X(ChangeNotSent, 4006) \
    X(ListNoAnswer, 4007) \
    X(ChangeNoAnswer, 4008) \
    X(StatNotTaken, 4009) \
    X(ChangesRefused, 4010) \
    X(StoredWithResult, 4011) \
    X(PicturesNotFetched, 4012) \
    X(WindowClassFailed, 5001) \
    X(WindowFailed, 5002) \
    X(DeviceFailed, 5003) \
    X(FactoryFailed, 5004) \
    X(SwapChainFailed, 5005) \
    X(IconFontMissing, 5006) \
    X(FontAtlasFailed, 5007) \
    X(ImGuiFailed, 5008) \
    X(SdlFailed, 5009) \
    X(SdlWindowFailed, 5010) \
    X(GlContextFailed, 5011) \
    X(GlTooOld, 5012) \
    X(SettingsNotSaved, 5013) \
    X(StatsNotSaved, 5014) \
    X(ClockRunsOff, 5015) \
    X(SettingsInUse, 5016) \
    X(NoServer, 6001) \
    X(BadReply, 6002) \
    X(WindowProgramRuns, 6003) \
    X(ServerRuns, 6004) \
    X(NoSocketPath, 6005) \
    X(NoLocalSockets, 6006) \
    X(SocketNotOpened, 6007) \
    X(NoProgramPath, 6008) \
    X(ServerRunsElsewhere, 6009) \
    X(UnitNotWritten, 6010) \
    X(SystemctlFailed, 6011) \
    X(ServiceNotStarted, 6012) \
    X(NoService, 6013) \
    X(UnitNotRemoved, 6014) \
    X(ServiceIsForLinux, 6015) \
    X(TokenUnusable, 7001) \
    X(NoRandomToken, 7002) \
    X(TokenNotSaved, 7003) \
    X(NoNetworkSockets, 7004) \
    X(NotAnAddress, 7005) \
    X(NoNetworkSocket, 7006) \
    X(PortTaken, 7007) \
    X(WrongHost, 7008) \
    X(WrongOrigin, 7009) \
    X(WrongToken, 7010) \
    X(NoRandomNonce, 7011) \
    X(NotJson, 7012) \
    X(NotWords, 7013) \
    X(NotFromPage, 7014) \
    X(PageNoAnswer, 7015) \
    X(PageBadAnswer, 7016) \
    X(ThemeUnknown, 7017) \
    X(ThemeFolderTaken, 7018) \
    X(ThemeNotWritten, 7019) \
    X(ThemeNotRight, 7020) \
    X(ThemeNameUnusable, 7021) \
    X(WebsiteNoAnswer, 8001) \
    X(WebsiteUnreadable, 8002) \
    X(SignInAgain, 8003) \
    X(DropsNotShown, 8004) \
    X(WebViewMissing, 8005) \
    X(WebViewNotStarted, 8006) \
    X(WebViewNotOpened, 8007) \
    X(WebViewNoPage, 8008) \
    X(OtherAccount, 8009)

namespace Idler
{
    enum class ErrorCode : std::uint16_t
    {
#define IDLER_ERROR_ENUM(name, number) name = number,
        IDLER_ERROR_CODES(IDLER_ERROR_ENUM)
#undef IDLER_ERROR_ENUM
    };

    inline constexpr ErrorCode kErrorCodes[] = {
#define IDLER_ERROR_LIST(name, number) ErrorCode::name,
        IDLER_ERROR_CODES(IDLER_ERROR_LIST)
#undef IDLER_ERROR_LIST
    };

    struct ErrorInfo
    {
        ErrorCode Code;
        std::string_view Message;
        std::string_view Cause;
        std::string_view Help;
    };

    std::span<const ErrorInfo> ErrorTable();
    const ErrorInfo* FindError(ErrorCode code);
    const ErrorInfo* FindError(std::string_view written);
    const ErrorInfo* ErrorOf(std::string_view message);

    std::string ErrorName(ErrorCode code);
    std::string_view ErrorArea(ErrorCode code);
    std::string ErrorMessage(ErrorCode code, std::string_view detail = {});
    std::string ErrorShownMessage(const ErrorInfo& info);

    std::string ErrorReference(const ErrorInfo& info);
    std::string ErrorList();
    std::string ErrorsMarkdown();
}
