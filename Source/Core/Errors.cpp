#include "Core/Errors.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>

namespace Idler
{
    namespace
    {
        constexpr std::string_view kPrefix = "MI-";
        constexpr std::string_view kPlace = "{}";
        constexpr std::string_view kReference = "https://github.com/Leunsel/Manifold-Idler/blob/main/Docs/ERRORS.md";

        constexpr std::string_view kNewerSteam = "Look for a newer Manifold Idler. If there is none, report the code with the Steam version from Steam > About Steam.";
        constexpr std::string_view kHelper = "Allow Manifold Idler in the antivirus, and start it from the folder it lives in.";
        constexpr std::string_view kAppInfo = "The idler reads it again shortly. If it stays, close Steam, delete appinfo.vdf in Steam's appcache folder and start Steam: it writes a new one.";
        constexpr std::string_view kReport = "Restart the computer. If it stays, report the code.";
        constexpr std::string_view kDriver = "Update the graphics driver. Over Remote Desktop, start the program on the computer itself once.";
        constexpr std::string_view kWritable = "Check that the folder can be written to and the disk is not full.";
        constexpr std::string_view kWebView = "Install Microsoft's WebView2 Runtime (Evergreen) and start the program again. Windows 11 has it already.";

        struct Area
        {
            int First;
            std::string_view Name;
        };

        constexpr std::array<Area, 8> kAreas{ {
            { 1, "Steam" },
            { 2, "Idling" },
            { 3, "Library" },
            { 4, "Achievements and stats" },
            { 5, "The program" },
            { 6, "Command line and server" },
            { 7, "Web page" },
            { 8, "Card farm" },
        } };

        constexpr ErrorInfo kErrors[] = {
            { ErrorCode::SteamNotRunning, "Steam is not running.",
                "No Steam client runs for this user on this computer.",
                "Start Steam and log on. The idler carries on by itself once Steam is there." },
            { ErrorCode::NobodyLoggedOn, "Nobody is logged on to Steam.",
                "Steam runs, but no account is logged on to it, or Steam is still starting.",
                "Log on in Steam. If Steam already shows the account, wait a minute while it finishes starting." },
            { ErrorCode::SteamOffline, "Steam is offline.",
                "Steam runs in offline mode or has lost its connection to Steam's servers. Steam counts no idle time while offline.",
                "Go online in Steam (Steam > Go Online) and check the internet connection." },
            { ErrorCode::ClientLibraryNotLoaded, "Steam's client library could not be loaded.",
                "steamclient64.dll (steamclient.so on Linux) is missing from the Steam folder, or it would not load.",
                "Restart Steam so it can repair its files. If it stays, install Steam again over the old installation; games and logins stay." },
            { ErrorCode::NoCreateInterface, "This Steam version is not supported.",
                "Steam's client library has no CreateInterface entry. Steam has changed in a way this version of the idler does not know.",
                kNewerSteam },
            { ErrorCode::ClientInterfaceRefused, "This Steam version is not supported.",
                "Steam does not offer the client interface version the idler asks for.",
                kNewerSteam },
            { ErrorCode::InterfaceMissing, "This Steam version is not supported.",
                "Steam does not offer one of the interfaces the idler needs: user, apps, utils, friends or stats.",
                kNewerSteam },
            { ErrorCode::NoSteamPipe, "Steam is not running.",
                "Steam's client library loaded, but Steam gave it no connection. Steam is closing or still starting, or it runs as another user.",
                "Wait until Steam has fully started. Run the idler as the same user as Steam, both with or both without administrator rights." },
            { ErrorCode::NoGlobalUser, "Nobody is logged on to Steam.",
                "Steam took the connection but had no logged on account to hand over.",
                "Log on in Steam. If it shows the account already, restart Steam." },
            { ErrorCode::SteamWentAway, "Steam went away while the game idled.",
                "Steam closed or restarted while the game idled, so its helper ended.",
                "Nothing to do: the idler starts the game again once Steam is back." },

            { ErrorCode::GameNotOwned, "This account does not own the game.",
                "Steam says the logged on account does not have this game, or no longer has it: a free weekend ended, it was refunded, or Family Sharing stopped.",
                "Take the game out of the queue, or log on with the account that owns it. Try again in the queue starts it once more." },
            { ErrorCode::GameNotAccepted, "Steam did not accept the game.",
                "Steam did not take the helper as this game. It happens with tools, demos and other apps that Steam does not run like games.",
                "Try again later. If it stays, Steam does not let this app be idled." },
            { ErrorCode::HelperNotStarted, "The helper process did not start.",
                "The idler runs every game in a small helper process, a copy of itself. The system refused to start it, often because an antivirus blocks it or the program file was moved or replaced.",
                kHelper },
            { ErrorCode::KeptStopping, "It stopped by itself several times.",
                "The game's helper ended without being asked three times in a row, so the idler gave up on the game.",
                "Look in the log for what came before it, such as MI-2005 or MI-1010. Try again in the queue starts it once more." },
            { ErrorCode::StoppedByItself, "The helper for the game stopped by itself.",
                "The game's helper ended without being asked: Steam closed it, it crashed, or another program ended it.",
                "Nothing to do: the idler starts it again. After three times in a row it gives up (MI-2004)." },
            { ErrorCode::DidNotEndInTime, "The game did not end in time and was ended.",
                "The game's helper did not end within three seconds of being asked to, so the idler ended it.",
                "Nothing to do. If it happens often, report the code with the log." },
            { ErrorCode::ShutdownFailed, "The computer could not be shut down.",
                "The queue finished with the shutdown set, but the system refused: this user may not shut down, or another program held it back.",
                "Shut down by hand. On Linux, check that this user may run systemctl poweroff." },

            { ErrorCode::AppInfoUnreadable, "Steam's list of apps could not be read.",
                "appinfo.vdf in Steam's appcache folder is missing or could not be opened.",
                "Let Steam start fully once; it writes the file. If it stays, restart Steam." },
            { ErrorCode::AppInfoNewerFormat, "Steam uses a newer format for its list of apps ({}).",
                "appinfo.vdf starts with a format number this version of the idler cannot read. Steam changes the format now and then.",
                "Look for a newer Manifold Idler. If there is none, report the code with the number in brackets." },
            { ErrorCode::AppInfoTooShort, "Steam's list of apps is too short.",
                "appinfo.vdf ends before its header does. Steam may have been writing it at that moment.",
                kAppInfo },
            { ErrorCode::AppInfoStringsDamaged, "The string table in Steam's list of apps is damaged.",
                "The table of names at the end of appinfo.vdf could not be read.",
                kAppInfo },
            { ErrorCode::AppInfoEndsEarly, "Steam's list of apps ends too early.",
                "appinfo.vdf ends in the middle of an entry, before its end mark.",
                kAppInfo },
            { ErrorCode::AppInfoEntryDamaged, "An entry in Steam's list of apps is damaged.",
                "An entry of appinfo.vdf claims to be longer than the file around it.",
                kAppInfo },
            { ErrorCode::ScanHelperNotStarted, "The library could not be read.",
                "The helper that asks Steam which games the account owns did not start.",
                kHelper },
            { ErrorCode::ScanHelperEnded, "The library could not be read.",
                "The helper that asks Steam which games the account owns ended before it was done.",
                "The idler tries again shortly. If it stays, report the code with the log." },
            { ErrorCode::ScanNoAnswer, "Steam did not answer.",
                "Steam took more than a minute to say which games the account owns.",
                "Steam may be busy with an update or a download. The idler tries again shortly." },

            { ErrorCode::StatsNotSent, "Steam did not send the achievements.",
                "Steam did not answer the request for this game's achievements and stats within 15 seconds.",
                "Try again. Steam may still be fetching them from its servers." },
            { ErrorCode::NoAchievements, "Steam has no achievements for this game.",
                "Steam answered, but it has no achievements or stats for this game.",
                "Nothing to do. Some games keep them on their own servers, where the idler cannot reach them." },
            { ErrorCode::StoreRefused, "Steam did not take the change.",
                "Steam refused to store the changed achievements or stats.",
                "Try again. If it stays, the game guards them against changes from outside." },
            { ErrorCode::StoreNotConfirmed, "Steam did not confirm the change.",
                "Steam took the change but did not say within 15 seconds that it stored it.",
                "Open the list again to see what Steam kept." },
            { ErrorCode::ListHelperNotStarted, "The list could not be read.",
                "The helper that reads the achievements from Steam did not start.",
                kHelper },
            { ErrorCode::ChangeNotSent, "The change could not be sent to Steam.",
                "The helper that changes achievements and stats did not start, or did not take the change.",
                kHelper },
            { ErrorCode::ListNoAnswer, "Steam did not answer.",
                "The helper reading the achievements got no full answer from Steam within 30 seconds.",
                "Try again. Steam may be busy." },
            { ErrorCode::ChangeNoAnswer, "Steam did not answer. Open the list again to see what it took.",
                "The helper changing achievements or stats got no full answer from Steam within 30 seconds.",
                "Open the list again: it shows what Steam took." },
            { ErrorCode::StatNotTaken, "Steam did not take the new value of {}.",
                "Steam answered, but the stat kept its old value. The game may guard it, or the value breaks one of its rules.",
                "Some stats only the game itself may change. Those cannot be set here." },
            { ErrorCode::ChangesRefused, "Steam refused {} of the changes.",
                "Steam kept some achievements or stats as they were. Some may only be unlocked by the game's own server, and some games guard them.",
                "The log names each one Steam did not take. Those cannot be changed here." },
            { ErrorCode::StoredWithResult, "Steam stored the change with result {}.",
                "Steam answered the change with a result other than OK. The number is Steam's own result code.",
                "Open the list again to see what Steam kept. If it happens often, report the code with the number." },
            { ErrorCode::PicturesNotFetched, "Achievement pictures could not be fetched.",
                "Fetching the pictures from Steam's public picture server failed three times in a row.",
                "Check the internet connection, then open the game again." },

            { ErrorCode::WindowClassFailed, "Could not register the window class.",
                "Windows refused to register the program's window class.",
                kReport },
            { ErrorCode::WindowFailed, "Could not create the window.",
                "Windows refused to create the program's window.",
                kReport },
            { ErrorCode::DeviceFailed, "Could not create the Direct3D 11 device ({}).",
                "The graphics driver gave no Direct3D 11 device, and the software one was not available either.",
                kDriver },
            { ErrorCode::FactoryFailed, "The DXGI factory is not available ({}).",
                "The graphics device gave no DXGI factory to make the window's swap chain with.",
                kDriver },
            { ErrorCode::SwapChainFailed, "Could not create the swap chain ({}).",
                "The graphics driver could not make the buffers the window is drawn into.",
                kDriver },
            { ErrorCode::IconFontMissing, "The embedded icon font is missing. Was App.rc compiled in?",
                "The program file has no icon font inside: it was built without its resources.",
                "Download the program again from the releases page." },
            { ErrorCode::FontAtlasFailed, "Could not build the font atlas.",
                "The fonts could not be turned into the picture the window draws text from.",
                kReport },
            { ErrorCode::ImGuiFailed, "Could not initialise Dear ImGui.",
                "The user interface library did not start with the graphics device.",
                kDriver },
            { ErrorCode::SdlFailed, "Could not start SDL: {}",
                "SDL found no display to show the window on. Over SSH or without a desktop there is none.",
                "Start it from a desktop session. Without one, the command line program runs: idler serve." },
            { ErrorCode::SdlWindowFailed, "Could not create the window: {}",
                "SDL could not make a window with OpenGL on this desktop.",
                "Update the graphics driver or Mesa. The text after the colon is SDL's own reason." },
            { ErrorCode::GlContextFailed, "Could not create the OpenGL context: {}",
                "The graphics driver gave no OpenGL 3 context for the window.",
                "Update the graphics driver or Mesa. The text after the colon is SDL's own reason." },
            { ErrorCode::GlTooOld, "OpenGL 3 is not available.",
                "The graphics driver offers only an older OpenGL than 3.0.",
                "Update the graphics driver or Mesa." },
            { ErrorCode::SettingsNotSaved, "The settings could not be saved in {}.",
                "Writing the settings file failed: the folder is read-only, the disk is full, or another program holds the file.",
                "Check that the folder can be written to. On Windows the settings live next to the program, so keep it out of Program Files." },
            { ErrorCode::StatsNotSaved, "The idle statistics could not be saved in {}.",
                "Writing the statistics file failed: the folder is read-only, the disk is full, or another program holds the file.",
                kWritable },
            { ErrorCode::ClockRunsOff, "The program's own clock runs at {} times real time, a speedhack perhaps. Idle time, timers, creatures and mini-games keep to real time.",
                "The clock the program reads runs faster or slower than the system time. A speedhack or a debugger does that.",
                "Nothing to do: everything that counts keeps to real time." },
            { ErrorCode::SettingsInUse, "Another Manifold Idler uses these settings: the window program, or idler serve.",
                "Only one program may use a settings file at a time; two would idle the same queue twice and overwrite each other's statistics.",
                "Close the other window, or end the server with idler quit. To run both, give one of them settings of its own with --settings." },

            { ErrorCode::NoServer, "No idler runs with these settings. Start one with: idler serve",
                "The command found no running server for this settings file.",
                "Start one with idler serve, or as a service with idler service install. When the server has --settings, the command needs the same." },
            { ErrorCode::BadReply, "The idler gave an answer that makes no sense.",
                "The server answered with something that is not a reply. Another version of the program may run as the server.",
                "End the server with idler quit and start it again with this program." },
            { ErrorCode::WindowProgramRuns, "The window program uses these settings: two idlers would idle the same queue twice. Close it first.",
                "The window program, or a server that does not answer yet, holds the same settings file and queue as this server would.",
                "Close the window program, or give the server settings of its own with --settings." },
            { ErrorCode::ServerRuns, "An idler already runs with these settings.",
                "Another server runs with the same settings file.",
                "Use that one, or end it with idler quit." },
            { ErrorCode::NoSocketPath, "No socket can be made at {}.",
                "The path of the local socket is too long, or the system's sockets could not be started.",
                "Give a settings file with a shorter path with --settings." },
            { ErrorCode::NoLocalSockets, "This system has no local sockets. Windows needs to be version 10 1803 or newer.",
                "The server talks to the other commands over a local socket, which older Windows versions do not have.",
                "Update Windows." },
            { ErrorCode::SocketNotOpened, "The socket could not be opened at {}.",
                "The local socket could not be made: the folder is read-only, or the path is in use.",
                kWritable },
            { ErrorCode::NoProgramPath, "Where this program lies could not be found.",
                "/proc/self/exe could not be read, so the service cannot name the program it starts.",
                "Run the command again from a normal shell. If it stays, report the code." },
            { ErrorCode::ServerRunsElsewhere, "An idler already runs with these settings, in a terminal perhaps. End it with idler quit, then run this again.",
                "A server with the same settings runs outside the service, and two would idle the same queue twice.",
                "End it with idler quit, then install the service again." },
            { ErrorCode::UnitNotWritten, "{} could not be written.",
                "The systemd unit file could not be written in ~/.config/systemd/user.",
                kWritable },
            { ErrorCode::SystemctlFailed, "systemctl --user did not work. A user service needs systemd and a login session of this user.",
                "systemctl --user did not run or failed. Without systemd, or without a login session of this user (as under su or sudo), there is no user service manager.",
                "Log in as the user directly, or run idler serve in a terminal or a tmux session instead." },
            { ErrorCode::ServiceNotStarted, "The service did not start. What systemd says: systemctl --user status {}",
                "systemd took the unit, but the server ended or did not start.",
                "systemctl --user status and journalctl --user -u with the service's name show why." },
            { ErrorCode::NoService, "No service is installed for these settings.",
                "No systemd unit was found for this settings file.",
                "idler service install sets one up. When it was installed with --settings, give the same here." },
            { ErrorCode::UnitNotRemoved, "{} could not be removed.",
                "The systemd unit file could not be deleted.",
                "Check that the folder can be written to, then try again." },
            { ErrorCode::ServiceIsForLinux, "A service is for Linux. On Windows a task at logon can run: idler serve",
                "idler service sets up a systemd user service, which exists on Linux only.",
                "On Windows, make a task in the Task Scheduler that runs idler serve at logon." },

            { ErrorCode::TokenUnusable, "{} holds no usable token: it takes 16 to 256 letters, digits, - or _. Without the file a new one is made.",
                "The web page's token file holds something that is not a token.",
                "Fix the token in the file, or delete the file: the server then makes a new token." },
            { ErrorCode::NoRandomToken, "No random token could be made for the web page.",
                "The system's random number source failed.",
                kReport },
            { ErrorCode::TokenNotSaved, "The web page's token could not be saved in {}.",
                "Writing the token file next to the settings failed.",
                kWritable },
            { ErrorCode::NoNetworkSockets, "This system has no network sockets.",
                "The system's network sockets could not be started.",
                kReport },
            { ErrorCode::NotAnAddress, "Not an address: {}.",
                "The address for the web page is not an IP address this system can read.",
                "Give a number address such as 127.0.0.1:8090 or [::1]:8090 with --web." },
            { ErrorCode::NoNetworkSocket, "No network socket could be made for {}.",
                "The system refused a network socket for the web page.",
                kReport },
            { ErrorCode::PortTaken, "The web page could not be opened at {}: the port is taken, or the address is not one of this computer's.",
                "Another program listens on the port, or the address given with --web belongs to no network card of this computer.",
                "Give another port with --web, for example --web 127.0.0.1:8091." },
            { ErrorCode::WrongHost, "This name is not one of the idler's addresses. Use its IP address or localhost.",
                "The browser asked for a host name the idler does not answer to. It answers to its own addresses only, which keeps other web sites out.",
                "Open the address idler web gives." },
            { ErrorCode::WrongOrigin, "The idler answers its own page only.",
                "The request came from another web page or site.",
                "Use the idler's own page, from idler web." },
            { ErrorCode::WrongToken, "The token is missing or wrong. The whole address, with its token, comes from: idler web",
                "The address lacks the token, or the token has changed since the page was opened.",
                "Run idler web and open the address it gives." },
            { ErrorCode::NoRandomNonce, "No random number for the page.",
                "The system's random number source failed while the page was made.",
                "Reload the page. If it stays, restart the server." },
            { ErrorCode::NotJson, "The request has to be JSON.",
                "A request to /api came without the JSON content type.",
                "Send the command as a JSON list of words, with Content-Type: application/json." },
            { ErrorCode::NotWords, "The request has to be a list of words, the first is the command.",
                "The body of a request to /api is not a JSON list of strings.",
                "Send the command as a JSON list of words, for example [\"status\"]." },
            { ErrorCode::NotFromPage, "Not from the web page: {}",
                "The web page may not run this command. It is for the terminal only.",
                "Run it in a terminal on the computer the idler runs on." },
            { ErrorCode::PageNoAnswer, "The idler does not answer. It may have ended; this page keeps trying.",
                "The page got no answer from the server: it ended, or the network between them is down.",
                "Start the server again with idler serve. The page goes on by itself once it answers." },
            { ErrorCode::PageBadAnswer, "The idler gave an answer the page cannot read (HTTP {}).",
                "The server answered with something that is not the page's JSON. Another program may listen on the port.",
                "Check that the address is the one idler web gives." },
            { ErrorCode::ThemeUnknown, "No theme is called {}.",
                "The name is none of the built-in themes and no folder in the themes folder.",
                "idler themes lists the themes there are." },
            { ErrorCode::ThemeFolderTaken, "There is a folder of that name already: {}",
                "A copy of a theme goes into a folder of its own, and one of that name is there already.",
                "Pick another name, or move the folder out of the way." },
            { ErrorCode::ThemeNotWritten, "The theme could not be written: {}",
                "Copying a theme into the themes folder failed part of the way.",
                kWritable },
            { ErrorCode::ThemeNotRight, "Theme problem: {}",
                "A theme folder was left out or only partly read: a file it needs is missing, or theme.ini holds a line the idler cannot use.",
                "Docs/WEB-THEMES.md lists what a theme holds. The page uses the rest of the theme as it is." },
            { ErrorCode::ThemeNameUnusable, "Not a name for a theme: {}",
                "A theme's name is its folder's name, and that may hold only a to z, 0 to 9 and -, up to 32 characters, starting with a letter or digit.",
                "Pick a name such as my-arcade." },

            { ErrorCode::WebsiteNoAnswer, "Steam's website did not answer.",
                "A page of Steam's website took more than a minute to load.",
                "Check the internet connection. Steam's website may be down; the card farm tries again later." },
            { ErrorCode::WebsiteUnreadable, "Steam's website could not be read.",
                "A page of Steam's website failed to load or came back empty.",
                "Check the internet connection. Steam's website may be down; the card farm tries again later." },
            { ErrorCode::SignInAgain, "Steam asks you to sign in again.",
                "The card farm's Steam login ran out or was ended, for example by signing out elsewhere or changing the password.",
                "Sign in again on the Cards page." },
            { ErrorCode::DropsNotShown, "Steam's page does not show the drops. Sign out and in again.",
                "Steam's badge page came back without the card drops, as it does when the login is only half valid.",
                "Sign out on the Cards page and sign in again." },
            { ErrorCode::WebViewMissing, "Microsoft's WebView2 Runtime is not installed ({}).",
                "The Steam login of the card farm runs in Microsoft's WebView2, which is not on this computer.",
                kWebView },
            { ErrorCode::WebViewNotStarted, "WebView2 did not start ({}).",
                "Microsoft's WebView2 Runtime is there but did not start. The number is its own error code.",
                kWebView },
            { ErrorCode::WebViewNotOpened, "WebView2 did not open ({}).",
                "WebView2 started but could not open its view in the window. The number is its own error code.",
                "Close the Cards page and open it again. If it stays, restart the program." },
            { ErrorCode::WebViewNoPage, "WebView2 has no page ({}).",
                "WebView2 opened but gave no page to load Steam's website into.",
                "Close the Cards page and open it again. If it stays, restart the program." },
            { ErrorCode::OtherAccount, "This login is for another account than the one logged on to Steam.",
                "The account signed in on the Cards page is not the one logged on in the Steam client, so the drops shown belong to another account.",
                "Sign out on the Cards page and sign in with the account the Steam client uses." },
        };

        int Number(ErrorCode code)
        {
            return static_cast<int>(code);
        }

        const Area* AreaOf(ErrorCode code)
        {
            const int first = Number(code) / 1000;
            for (const Area& area : kAreas)
            {
                if (area.First == first)
                    return &area;
            }
            return nullptr;
        }

        std::string Shown(std::string_view message, std::string_view detail)
        {
            std::string text(message);
            if (const std::size_t at = text.find(kPlace); at != std::string::npos)
                text.replace(at, kPlace.size(), detail);
            return text;
        }

        std::string Anchor(std::string_view heading)
        {
            std::string anchor;
            for (const char c : heading)
                anchor.push_back(c == ' ' ? '-' : (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
            return anchor;
        }

        bool IsDigit(char c)
        {
            return c >= '0' && c <= '9';
        }
    }

    std::span<const ErrorInfo> ErrorTable()
    {
        return kErrors;
    }

    const ErrorInfo* FindError(ErrorCode code)
    {
        const auto found = std::lower_bound(std::begin(kErrors), std::end(kErrors), code, [](const ErrorInfo& info, ErrorCode wanted) { return info.Code < wanted; });
        return found != std::end(kErrors) && found->Code == code ? &*found : nullptr;
    }

    const ErrorInfo* FindError(std::string_view written)
    {
        while (!written.empty() && (written.front() == ' ' || written.front() == '\t'))
            written.remove_prefix(1);
        while (!written.empty() && (written.back() == ' ' || written.back() == '\t' || written.back() == '\r' || written.back() == '\n'))
            written.remove_suffix(1);
        if (written.size() > kPrefix.size() && (written[0] == 'M' || written[0] == 'm') && (written[1] == 'I' || written[1] == 'i') && written[2] == '-')
            written.remove_prefix(kPrefix.size());
        if (written.size() != 4 || !std::all_of(written.begin(), written.end(), IsDigit))
            return nullptr;
        const int number = (written[0] - '0') * 1000 + (written[1] - '0') * 100 + (written[2] - '0') * 10 + (written[3] - '0');
        return FindError(static_cast<ErrorCode>(number));
    }

    const ErrorInfo* ErrorOf(std::string_view message)
    {
        while (!message.empty() && (message.back() == ' ' || message.back() == '\r' || message.back() == '\n'))
            message.remove_suffix(1);
        constexpr std::size_t kLength = 1 + 3 + 4 + 1;
        if (message.size() < kLength || message.back() != ')')
            return nullptr;
        const std::string_view tail = message.substr(message.size() - kLength);
        if (tail[0] != '(' || tail.substr(1, kPrefix.size()) != kPrefix)
            return nullptr;
        return FindError(tail.substr(1, kLength - 2));
    }

    std::string ErrorName(ErrorCode code)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "MI-%04d", Number(code));
        return text;
    }

    std::string_view ErrorArea(ErrorCode code)
    {
        const Area* area = AreaOf(code);
        return area ? area->Name : std::string_view();
    }

    std::string ErrorMessage(ErrorCode code, std::string_view detail)
    {
        const ErrorInfo* info = FindError(code);
        const std::string text = info ? Shown(info->Message, detail) : std::string("Something went wrong.");
        return text + " (" + ErrorName(code) + ")";
    }

    std::string ErrorShownMessage(const ErrorInfo& info)
    {
        return Shown(info.Message, "...");
    }

    std::string ErrorReference(const ErrorInfo& info)
    {
        std::string text = ErrorName(info.Code) + " (" + std::string(ErrorArea(info.Code)) + ")\n";
        text += ErrorShownMessage(info) + "\n\n";
        text += "What happened: " + std::string(info.Cause) + "\n";
        text += "What to do: " + std::string(info.Help) + "\n\n";
        std::string name = ErrorName(info.Code);
        std::transform(name.begin(), name.end(), name.begin(), [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
        text += std::string(kReference) + "#" + name + "\n";
        return text;
    }

    std::string ErrorList()
    {
        std::string text;
        const Area* last = nullptr;
        for (const ErrorInfo& info : kErrors)
        {
            const Area* area = AreaOf(info.Code);
            if (area != last)
            {
                text += (last ? "\n" : "") + std::string(area ? area->Name : std::string_view()) + "\n";
                last = area;
            }
            text += "  " + ErrorName(info.Code) + "  " + ErrorShownMessage(info) + "\n";
        }
        return text;
    }

    std::string ErrorsMarkdown()
    {
        std::string text = "# Error codes\n\n";
        text += "Every error Manifold Idler reports ends in a code such as `MI-1001`. A code keeps its meaning in every version and is never given to another error.\n\n";
        text += "The same texts are in the program. `idler explain MI-1001` prints one; in the window, pointing at a message shows it, and a click copies its code.\n\n";
        text += "When you report a problem, give the code.\n\n";
        text += "This file is made from `Source/Core/Errors.cpp` with `idler explain --markdown`.\n\n";
        text += "| Area | Codes |\n|---|---|\n";
        for (const Area& area : kAreas)
        {
            const ErrorInfo* first = nullptr;
            const ErrorInfo* last = nullptr;
            for (const ErrorInfo& info : kErrors)
            {
                if (AreaOf(info.Code) != &area)
                    continue;
                first = first ? first : &info;
                last = &info;
            }
            if (first)
                text += "| [" + std::string(area.Name) + "](#" + Anchor(area.Name) + ") | " + ErrorName(first->Code) + " to " + ErrorName(last->Code) + " |\n";
        }

        const Area* current = nullptr;
        for (const ErrorInfo& info : kErrors)
        {
            const Area* area = AreaOf(info.Code);
            if (area != current)
            {
                text += "\n## " + std::string(area ? area->Name : std::string_view()) + "\n";
                current = area;
            }
            text += "\n### " + ErrorName(info.Code) + "\n\n";
            text += "**" + ErrorShownMessage(info) + "**\n\n";
            text += "What happened: " + std::string(info.Cause) + "\n\n";
            text += "What to do: " + std::string(info.Help) + "\n";
        }
        return text;
    }
}
