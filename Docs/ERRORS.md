# Error codes

Every error Manifold Idler reports ends in a code such as `MI-1001`. A code keeps its meaning in every version and is never given to another error.

The same texts are in the program. `idler explain MI-1001` prints one; in the window, pointing at a message shows it, and a click copies its code.

When you report a problem, give the code.

This file is made from `Source/Core/Errors.cpp` with `idler explain --markdown`.

| Area | Codes |
|---|---|
| [Steam](#steam) | MI-1001 to MI-1010 |
| [Idling](#idling) | MI-2001 to MI-2007 |
| [Library](#library) | MI-3001 to MI-3009 |
| [Achievements and stats](#achievements-and-stats) | MI-4001 to MI-4012 |
| [The program](#the-program) | MI-5001 to MI-5016 |
| [Command line and server](#command-line-and-server) | MI-6001 to MI-6015 |
| [Web page](#web-page) | MI-7001 to MI-7021 |
| [Card farm](#card-farm) | MI-8001 to MI-8009 |

## Steam

### MI-1001

**Steam is not running.**

What happened: No Steam client runs for this user on this computer.

What to do: Start Steam and log on. The idler carries on by itself once Steam is there.

### MI-1002

**Nobody is logged on to Steam.**

What happened: Steam runs, but no account is logged on to it, or Steam is still starting.

What to do: Log on in Steam. If Steam already shows the account, wait a minute while it finishes starting.

### MI-1003

**Steam is offline.**

What happened: Steam runs in offline mode or has lost its connection to Steam's servers. Steam counts no idle time while offline.

What to do: Go online in Steam (Steam > Go Online) and check the internet connection.

### MI-1004

**Steam's client library could not be loaded.**

What happened: steamclient64.dll (steamclient.so on Linux) is missing from the Steam folder, or it would not load.

What to do: Restart Steam so it can repair its files. If it stays, install Steam again over the old installation; games and logins stay.

### MI-1005

**This Steam version is not supported.**

What happened: Steam's client library has no CreateInterface entry. Steam has changed in a way this version of the idler does not know.

What to do: Look for a newer Manifold Idler. If there is none, report the code with the Steam version from Steam > About Steam.

### MI-1006

**This Steam version is not supported.**

What happened: Steam does not offer the client interface version the idler asks for.

What to do: Look for a newer Manifold Idler. If there is none, report the code with the Steam version from Steam > About Steam.

### MI-1007

**This Steam version is not supported.**

What happened: Steam does not offer one of the interfaces the idler needs: user, apps, utils, friends or stats.

What to do: Look for a newer Manifold Idler. If there is none, report the code with the Steam version from Steam > About Steam.

### MI-1008

**Steam is not running.**

What happened: Steam's client library loaded, but Steam gave it no connection. Steam is closing or still starting, or it runs as another user.

What to do: Wait until Steam has fully started. Run the idler as the same user as Steam, both with or both without administrator rights.

### MI-1009

**Nobody is logged on to Steam.**

What happened: Steam took the connection but had no logged on account to hand over.

What to do: Log on in Steam. If it shows the account already, restart Steam.

### MI-1010

**Steam went away while the game idled.**

What happened: Steam closed or restarted while the game idled, so its helper ended.

What to do: Nothing to do: the idler starts the game again once Steam is back.

## Idling

### MI-2001

**This account does not own the game.**

What happened: Steam says the logged on account does not have this game, or no longer has it: a free weekend ended, it was refunded, or Family Sharing stopped.

What to do: Take the game out of the queue, or log on with the account that owns it. Try again in the queue starts it once more.

### MI-2002

**Steam did not accept the game.**

What happened: Steam did not take the helper as this game. It happens with tools, demos and other apps that Steam does not run like games.

What to do: Try again later. If it stays, Steam does not let this app be idled.

### MI-2003

**The helper process did not start.**

What happened: The idler runs every game in a small helper process, a copy of itself. The system refused to start it, often because an antivirus blocks it or the program file was moved or replaced.

What to do: Allow Manifold Idler in the antivirus, and start it from the folder it lives in.

### MI-2004

**It stopped by itself several times.**

What happened: The game's helper ended without being asked three times in a row, so the idler gave up on the game.

What to do: Look in the log for what came before it, such as MI-2005 or MI-1010. Try again in the queue starts it once more.

### MI-2005

**The helper for the game stopped by itself.**

What happened: The game's helper ended without being asked: Steam closed it, it crashed, or another program ended it.

What to do: Nothing to do: the idler starts it again. After three times in a row it gives up (MI-2004).

### MI-2006

**The game did not end in time and was ended.**

What happened: The game's helper did not end within three seconds of being asked to, so the idler ended it.

What to do: Nothing to do. If it happens often, report the code with the log.

### MI-2007

**The computer could not be shut down.**

What happened: The queue finished with the shutdown set, but the system refused: this user may not shut down, or another program held it back.

What to do: Shut down by hand. On Linux, check that this user may run systemctl poweroff.

## Library

### MI-3001

**Steam's list of apps could not be read.**

What happened: appinfo.vdf in Steam's appcache folder is missing or could not be opened.

What to do: Let Steam start fully once; it writes the file. If it stays, restart Steam.

### MI-3002

**Steam uses a newer format for its list of apps (...).**

What happened: appinfo.vdf starts with a format number this version of the idler cannot read. Steam changes the format now and then.

What to do: Look for a newer Manifold Idler. If there is none, report the code with the number in brackets.

### MI-3003

**Steam's list of apps is too short.**

What happened: appinfo.vdf ends before its header does. Steam may have been writing it at that moment.

What to do: The idler reads it again shortly. If it stays, close Steam, delete appinfo.vdf in Steam's appcache folder and start Steam: it writes a new one.

### MI-3004

**The string table in Steam's list of apps is damaged.**

What happened: The table of names at the end of appinfo.vdf could not be read.

What to do: The idler reads it again shortly. If it stays, close Steam, delete appinfo.vdf in Steam's appcache folder and start Steam: it writes a new one.

### MI-3005

**Steam's list of apps ends too early.**

What happened: appinfo.vdf ends in the middle of an entry, before its end mark.

What to do: The idler reads it again shortly. If it stays, close Steam, delete appinfo.vdf in Steam's appcache folder and start Steam: it writes a new one.

### MI-3006

**An entry in Steam's list of apps is damaged.**

What happened: An entry of appinfo.vdf claims to be longer than the file around it.

What to do: The idler reads it again shortly. If it stays, close Steam, delete appinfo.vdf in Steam's appcache folder and start Steam: it writes a new one.

### MI-3007

**The library could not be read.**

What happened: The helper that asks Steam which games the account owns did not start.

What to do: Allow Manifold Idler in the antivirus, and start it from the folder it lives in.

### MI-3008

**The library could not be read.**

What happened: The helper that asks Steam which games the account owns ended before it was done.

What to do: The idler tries again shortly. If it stays, report the code with the log.

### MI-3009

**Steam did not answer.**

What happened: Steam took more than a minute to say which games the account owns.

What to do: Steam may be busy with an update or a download. The idler tries again shortly.

## Achievements and stats

### MI-4001

**Steam did not send the achievements.**

What happened: Steam did not answer the request for this game's achievements and stats within 15 seconds.

What to do: Try again. Steam may still be fetching them from its servers.

### MI-4002

**Steam has no achievements for this game.**

What happened: Steam answered, but it has no achievements or stats for this game.

What to do: Nothing to do. Some games keep them on their own servers, where the idler cannot reach them.

### MI-4003

**Steam did not take the change.**

What happened: Steam refused to store the changed achievements or stats.

What to do: Try again. If it stays, the game guards them against changes from outside.

### MI-4004

**Steam did not confirm the change.**

What happened: Steam took the change but did not say within 15 seconds that it stored it.

What to do: Open the list again to see what Steam kept.

### MI-4005

**The list could not be read.**

What happened: The helper that reads the achievements from Steam did not start.

What to do: Allow Manifold Idler in the antivirus, and start it from the folder it lives in.

### MI-4006

**The change could not be sent to Steam.**

What happened: The helper that changes achievements and stats did not start, or did not take the change.

What to do: Allow Manifold Idler in the antivirus, and start it from the folder it lives in.

### MI-4007

**Steam did not answer.**

What happened: The helper reading the achievements got no full answer from Steam within 30 seconds.

What to do: Try again. Steam may be busy.

### MI-4008

**Steam did not answer. Open the list again to see what it took.**

What happened: The helper changing achievements or stats got no full answer from Steam within 30 seconds.

What to do: Open the list again: it shows what Steam took.

### MI-4009

**Steam did not take the new value of ....**

What happened: Steam answered, but the stat kept its old value. The game may guard it, or the value breaks one of its rules.

What to do: Some stats only the game itself may change. Those cannot be set here.

### MI-4010

**Steam refused ... of the changes.**

What happened: Steam kept some achievements or stats as they were. Some may only be unlocked by the game's own server, and some games guard them.

What to do: The log names each one Steam did not take. Those cannot be changed here.

### MI-4011

**Steam stored the change with result ....**

What happened: Steam answered the change with a result other than OK. The number is Steam's own result code.

What to do: Open the list again to see what Steam kept. If it happens often, report the code with the number.

### MI-4012

**Achievement pictures could not be fetched.**

What happened: Fetching the pictures from Steam's public picture server failed three times in a row.

What to do: Check the internet connection, then open the game again.

## The program

### MI-5001

**Could not register the window class.**

What happened: Windows refused to register the program's window class.

What to do: Restart the computer. If it stays, report the code.

### MI-5002

**Could not create the window.**

What happened: Windows refused to create the program's window.

What to do: Restart the computer. If it stays, report the code.

### MI-5003

**Could not create the Direct3D 11 device (...).**

What happened: The graphics driver gave no Direct3D 11 device, and the software one was not available either.

What to do: Update the graphics driver. Over Remote Desktop, start the program on the computer itself once.

### MI-5004

**The DXGI factory is not available (...).**

What happened: The graphics device gave no DXGI factory to make the window's swap chain with.

What to do: Update the graphics driver. Over Remote Desktop, start the program on the computer itself once.

### MI-5005

**Could not create the swap chain (...).**

What happened: The graphics driver could not make the buffers the window is drawn into.

What to do: Update the graphics driver. Over Remote Desktop, start the program on the computer itself once.

### MI-5006

**The embedded icon font is missing. Was App.rc compiled in?**

What happened: The program file has no icon font inside: it was built without its resources.

What to do: Download the program again from the releases page.

### MI-5007

**Could not build the font atlas.**

What happened: The fonts could not be turned into the picture the window draws text from.

What to do: Restart the computer. If it stays, report the code.

### MI-5008

**Could not initialise Dear ImGui.**

What happened: The user interface library did not start with the graphics device.

What to do: Update the graphics driver. Over Remote Desktop, start the program on the computer itself once.

### MI-5009

**Could not start SDL: ...**

What happened: SDL found no display to show the window on. Over SSH or without a desktop there is none.

What to do: Start it from a desktop session. Without one, the command line program runs: idler serve.

### MI-5010

**Could not create the window: ...**

What happened: SDL could not make a window with OpenGL on this desktop.

What to do: Update the graphics driver or Mesa. The text after the colon is SDL's own reason.

### MI-5011

**Could not create the OpenGL context: ...**

What happened: The graphics driver gave no OpenGL 3 context for the window.

What to do: Update the graphics driver or Mesa. The text after the colon is SDL's own reason.

### MI-5012

**OpenGL 3 is not available.**

What happened: The graphics driver offers only an older OpenGL than 3.0.

What to do: Update the graphics driver or Mesa.

### MI-5013

**The settings could not be saved in ....**

What happened: Writing the settings file failed: the folder is read-only, the disk is full, or another program holds the file.

What to do: Check that the folder can be written to. On Windows the settings live next to the program, so keep it out of Program Files.

### MI-5014

**The idle statistics could not be saved in ....**

What happened: Writing the statistics file failed: the folder is read-only, the disk is full, or another program holds the file.

What to do: Check that the folder can be written to and the disk is not full.

### MI-5015

**The program's own clock runs at ... times real time, a speedhack perhaps. Idle time, timers, creatures and mini-games keep to real time.**

What happened: The clock the program reads runs faster or slower than the system time. A speedhack or a debugger does that.

What to do: Nothing to do: everything that counts keeps to real time.

### MI-5016

**Another Manifold Idler uses these settings: the window program, or idler serve.**

What happened: Only one program may use a settings file at a time; two would idle the same queue twice and overwrite each other's statistics.

What to do: Close the other window, or end the server with idler quit. To run both, give one of them settings of its own with --settings.

## Command line and server

### MI-6001

**No idler runs with these settings. Start one with: idler serve**

What happened: The command found no running server for this settings file.

What to do: Start one with idler serve, or as a service with idler service install. When the server has --settings, the command needs the same.

### MI-6002

**The idler gave an answer that makes no sense.**

What happened: The server answered with something that is not a reply. Another version of the program may run as the server.

What to do: End the server with idler quit and start it again with this program.

### MI-6003

**The window program uses these settings: two idlers would idle the same queue twice. Close it first.**

What happened: The window program, or a server that does not answer yet, holds the same settings file and queue as this server would.

What to do: Close the window program, or give the server settings of its own with --settings.

### MI-6004

**An idler already runs with these settings.**

What happened: Another server runs with the same settings file.

What to do: Use that one, or end it with idler quit.

### MI-6005

**No socket can be made at ....**

What happened: The path of the local socket is too long, or the system's sockets could not be started.

What to do: Give a settings file with a shorter path with --settings.

### MI-6006

**This system has no local sockets. Windows needs to be version 10 1803 or newer.**

What happened: The server talks to the other commands over a local socket, which older Windows versions do not have.

What to do: Update Windows.

### MI-6007

**The socket could not be opened at ....**

What happened: The local socket could not be made: the folder is read-only, or the path is in use.

What to do: Check that the folder can be written to and the disk is not full.

### MI-6008

**Where this program lies could not be found.**

What happened: /proc/self/exe could not be read, so the service cannot name the program it starts.

What to do: Run the command again from a normal shell. If it stays, report the code.

### MI-6009

**An idler already runs with these settings, in a terminal perhaps. End it with idler quit, then run this again.**

What happened: A server with the same settings runs outside the service, and two would idle the same queue twice.

What to do: End it with idler quit, then install the service again.

### MI-6010

**... could not be written.**

What happened: The systemd unit file could not be written in ~/.config/systemd/user.

What to do: Check that the folder can be written to and the disk is not full.

### MI-6011

**systemctl --user did not work. A user service needs systemd and a login session of this user.**

What happened: systemctl --user did not run or failed. Without systemd, or without a login session of this user (as under su or sudo), there is no user service manager.

What to do: Log in as the user directly, or run idler serve in a terminal or a tmux session instead.

### MI-6012

**The service did not start. What systemd says: systemctl --user status ...**

What happened: systemd took the unit, but the server ended or did not start.

What to do: systemctl --user status and journalctl --user -u with the service's name show why.

### MI-6013

**No service is installed for these settings.**

What happened: No systemd unit was found for this settings file.

What to do: idler service install sets one up. When it was installed with --settings, give the same here.

### MI-6014

**... could not be removed.**

What happened: The systemd unit file could not be deleted.

What to do: Check that the folder can be written to, then try again.

### MI-6015

**A service is for Linux. On Windows a task at logon can run: idler serve**

What happened: idler service sets up a systemd user service, which exists on Linux only.

What to do: On Windows, make a task in the Task Scheduler that runs idler serve at logon.

## Web page

### MI-7001

**... holds no usable token: it takes 16 to 256 letters, digits, - or _. Without the file a new one is made.**

What happened: The web page's token file holds something that is not a token.

What to do: Fix the token in the file, or delete the file: the server then makes a new token.

### MI-7002

**No random token could be made for the web page.**

What happened: The system's random number source failed.

What to do: Restart the computer. If it stays, report the code.

### MI-7003

**The web page's token could not be saved in ....**

What happened: Writing the token file next to the settings failed.

What to do: Check that the folder can be written to and the disk is not full.

### MI-7004

**This system has no network sockets.**

What happened: The system's network sockets could not be started.

What to do: Restart the computer. If it stays, report the code.

### MI-7005

**Not an address: ....**

What happened: The address for the web page is not an IP address this system can read.

What to do: Give a number address such as 127.0.0.1:8090 or [::1]:8090 with --web.

### MI-7006

**No network socket could be made for ....**

What happened: The system refused a network socket for the web page.

What to do: Restart the computer. If it stays, report the code.

### MI-7007

**The web page could not be opened at ...: the port is taken, or the address is not one of this computer's.**

What happened: Another program listens on the port, or the address given with --web belongs to no network card of this computer.

What to do: Give another port with --web, for example --web 127.0.0.1:8091.

### MI-7008

**This name is not one of the idler's addresses. Use its IP address or localhost.**

What happened: The browser asked for a host name the idler does not answer to. It answers to its own addresses only, which keeps other web sites out.

What to do: Open the address idler web gives.

### MI-7009

**The idler answers its own page only.**

What happened: The request came from another web page or site.

What to do: Use the idler's own page, from idler web.

### MI-7010

**The token is missing or wrong. The whole address, with its token, comes from: idler web**

What happened: The address lacks the token, or the token has changed since the page was opened.

What to do: Run idler web and open the address it gives.

### MI-7011

**No random number for the page.**

What happened: The system's random number source failed while the page was made.

What to do: Reload the page. If it stays, restart the server.

### MI-7012

**The request has to be JSON.**

What happened: A request to /api came without the JSON content type.

What to do: Send the command as a JSON list of words, with Content-Type: application/json.

### MI-7013

**The request has to be a list of words, the first is the command.**

What happened: The body of a request to /api is not a JSON list of strings.

What to do: Send the command as a JSON list of words, for example ["status"].

### MI-7014

**Not from the web page: ...**

What happened: The web page may not run this command. It is for the terminal only.

What to do: Run it in a terminal on the computer the idler runs on.

### MI-7015

**The idler does not answer. It may have ended; this page keeps trying.**

What happened: The page got no answer from the server: it ended, or the network between them is down.

What to do: Start the server again with idler serve. The page goes on by itself once it answers.

### MI-7016

**The idler gave an answer the page cannot read (HTTP ...).**

What happened: The server answered with something that is not the page's JSON. Another program may listen on the port.

What to do: Check that the address is the one idler web gives.

### MI-7017

**No theme is called ....**

What happened: The name is none of the built-in themes and no folder in the themes folder.

What to do: idler themes lists the themes there are.

### MI-7018

**There is a folder of that name already: ...**

What happened: A copy of a theme goes into a folder of its own, and one of that name is there already.

What to do: Pick another name, or move the folder out of the way.

### MI-7019

**The theme could not be written: ...**

What happened: Copying a theme into the themes folder failed part of the way.

What to do: Check that the folder can be written to and the disk is not full.

### MI-7020

**Theme problem: ...**

What happened: A theme folder was left out or only partly read: a file it needs is missing, or theme.ini holds a line the idler cannot use.

What to do: Docs/WEB-THEMES.md lists what a theme holds. The page uses the rest of the theme as it is.

### MI-7021

**Not a name for a theme: ...**

What happened: A theme's name is its folder's name, and that may hold only a to z, 0 to 9 and -, up to 32 characters, starting with a letter or digit.

What to do: Pick a name such as my-arcade.

## Card farm

### MI-8001

**Steam's website did not answer.**

What happened: A page of Steam's website took more than a minute to load.

What to do: Check the internet connection. Steam's website may be down; the card farm tries again later.

### MI-8002

**Steam's website could not be read.**

What happened: A page of Steam's website failed to load or came back empty.

What to do: Check the internet connection. Steam's website may be down; the card farm tries again later.

### MI-8003

**Steam asks you to sign in again.**

What happened: The card farm's Steam login ran out or was ended, for example by signing out elsewhere or changing the password.

What to do: Sign in again on the Cards page.

### MI-8004

**Steam's page does not show the drops. Sign out and in again.**

What happened: Steam's badge page came back without the card drops, as it does when the login is only half valid.

What to do: Sign out on the Cards page and sign in again.

### MI-8005

**Microsoft's WebView2 Runtime is not installed (...).**

What happened: The Steam login of the card farm runs in Microsoft's WebView2, which is not on this computer.

What to do: Install Microsoft's WebView2 Runtime (Evergreen) and start the program again. Windows 11 has it already.

### MI-8006

**WebView2 did not start (...).**

What happened: Microsoft's WebView2 Runtime is there but did not start. The number is its own error code.

What to do: Install Microsoft's WebView2 Runtime (Evergreen) and start the program again. Windows 11 has it already.

### MI-8007

**WebView2 did not open (...).**

What happened: WebView2 started but could not open its view in the window. The number is its own error code.

What to do: Close the Cards page and open it again. If it stays, restart the program.

### MI-8008

**WebView2 has no page (...).**

What happened: WebView2 opened but gave no page to load Steam's website into.

What to do: Close the Cards page and open it again. If it stays, restart the program.

### MI-8009

**This login is for another account than the one logged on to Steam.**

What happened: The account signed in on the Cards page is not the one logged on in the Steam client, so the drops shown belong to another account.

What to do: Sign out on the Cards page and sign in with the account the Steam client uses.
