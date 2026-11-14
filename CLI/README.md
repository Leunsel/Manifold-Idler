# Manifold Idler CLI

Manifold Idler without a window, for servers and terminals. A server keeps the queue going, and short commands talk to it.
It runs on Windows and on Linux. The queue, the rules and the workers are those of the window program in the folder above:
one source, two programs.

On Linux it idles with the current Steam client (seen on CachyOS): Steam tracks each worker as the game. The newer client no longer
writes the logged-on account into its files, so the idler asks Steam itself.

## What it needs

Steam has to run on the same computer, logged on, as the same user. The workers talk to that client. So a server needs Steam
installed and signed in once by hand, and on Linux a display, a virtual one will do (`xvfb-run`, see A server without a desktop).
A normal install of Steam, not the Flatpak: its sandbox cannot be reached from outside.

Steam allows one login at a time. Signing in somewhere else ends this one, and the idler waits until Steam is back.

This program holds no Steam login and no card farm. Neither is built into it.

## Building

From the repository's folder:

```
cmake -S CLI -B Build/CLI -DCMAKE_BUILD_TYPE=Release
cmake --build Build/CLI --config Release
```

It needs a C++20 compiler and the rest of the repository around it (or `-DIDLER_ROOT=<path>`). On Windows, Visual Studio
2022 will do, with Windows 10 1803 or newer to run it. On Linux, GCC 11 or newer, or Clang.

The result is `idler` in `Build/CLI`, or `Build/CLI/Release/idler.exe` on Windows.

With GCC or Clang, `-DIDLER_WARNINGS_AS_ERRORS=ON` stops the build at any compiler warning, as GitHub Actions does;
Visual Studio always does.

From Windows, a Linux program comes from `python CLI/Tools/cross-linux.py`, with [Zig](https://ziglang.org) on the path or
`--zig <zig.exe>`. It lands in `Build/CLI/linux-x86_64`, needs glibc 2.31 or newer and nothing else, and can be copied over as
it is.

## Using it

```
idler serve --start          runs in this terminal; --start starts the queue once the library is read
idler serve --web            also serves a web page, see The web page
idler service install        runs it as a systemd user service instead (Linux), see Staying up
idler status                 what it is doing, its version, and a newer one when there is one
idler games portal           the account's games, found by name or app id
idler add 620 730 --for 3    adds games, optionally --for <hours> or --until <hours played>
idler queue                  the games in the queue
idler goal 620 until 500     a game's goal: none, for <hours> or until <hours>
idler remove 620             takes a game out
idler slots 8                how many games idle at once, up to 32
idler lists                  the saved lists
idler use "Night shift"      makes a saved list the queue
idler start                  starts the queue
idler stop                   stops it
idler log 50                 what it did last
idler web                    the web page's address, with its token
idler themes                 the web page's themes; themes copy <theme> <name> makes one of your own from one
idler quit                   ends the server; Ctrl+C does too
idler backups                the backups of the settings and statistics, newest first
idler backup                 makes one now, from what was saved last
idler restore 2026-11-13     puts one back; only while neither the server nor the window program runs
idler explain MI-1001        what an error code means; without a code, all of them
```

The queue and its lists:

```
idler move 620 1             moves a game to a place in the queue, from 1
idler sort left              sorts the queue: name, left, least or most
idler now 620                puts a game first and starts the queue
idler retry 620              tries a game again that did not start
idler refund 620             lets a game idle past the refund limit
idler clear finished         takes out the games that are done; clear all takes out all
idler undo                   brings back what the last removal, sort or deleted list took
idler list new "Weekend"     a new list, which becomes the queue; also list rename <name> <new name>, list delete <name>
idler rescan                 reads the library again
```

The library, the statistics and the settings:

```
idler library --show cards --sort playtime --rating 80 portal
                             the games as the window's Library shows them
idler game 620               everything about one game
idler stats                  what was idled: today, the last 7 days, all time, records
idler stats games sessions   the games by time, sessions, longest, days or recent
idler history 2026-11-12     idle time by day; with a day, that day's games
idler settings               the settings the server goes by
idler set pause off          changes one: set goal for 3, set hours on, set from 22, ...; set updates off stops the
                             daily question to GitHub for a newer version
```

Apart from `serve`, `service`, `themes` and `explain`, the commands talk to the server, so it has to run: in a second terminal, or as
a service. One game idles at a time until `idler slots` allows more. The commands exit with 0 when it worked, 1 when not,
and 2 when no server runs. Errors end in a code such as `MI-6001`; [Docs/ERRORS.md](../Docs/ERRORS.md) explains each one.

## Where things are

The server and the window program share one settings file, with the lists, the queue, the creatures and the statistics: what
one of them idled, the other one counts. Next to it lie the statistics, the log and the web page's token.

| | Settings by default | Socket |
|---|---|---|
| Windows | `Manifold-Idler.ini` next to the program; shared when `idler.exe` lies next to `Manifold-Idler.exe` | the user's temp folder |
| Linux | `$XDG_CONFIG_HOME/manifold-idler/Manifold-Idler.ini`, the window program's file | `$XDG_RUNTIME_DIR`, or the temp folder |

Only one program at a time uses a settings file: the second one says so (MI-5016 in the window, MI-6003 here) and does not start.
The server backs up the settings and statistics once a day, as the window program does, into `Manifold-Idler.backups` next to
them, and keeps the last 7 days. A restore first backs up what is there, so it can be undone with another restore.
When the server crashes, it writes the stack trace into its log and a report into `Manifold-Idler.crashes` next to the
settings; its next start says so in the log.
Earlier versions called the file `idler.ini`. `idler serve` and `idler service install` rename it, its statistics and its token
once to the shared names, when nothing uses it and no shared file exists yet; if both exist, `idler.ini` is left as it is.

`--settings <file>` comes before the command: `idler --settings a.ini status`. Each settings file has a server of its own. Only
the same user reaches the socket, and it answers nothing but the commands above. It opens no network port; only `--web` does.

## The web page

`idler serve --web` also serves a page, at `http://127.0.0.1:8090` unless another address is given. It has the window program's
pages: Idling with the queue and its lists, the Library with the covers from Steam's own cache, Statistics, the Log and the
Settings. It does what the commands do, all but `quit`: the server ends from a terminal. The page lies inside the program and
loads nothing from elsewhere. Its look comes from a theme: Arcade, with four screens, or Manifold, the window program's own.
The four squares beside Start switch from one look to the next, Settings > Look lists them with their options, and the
browser remembers the choice.

```
idler serve --web                    127.0.0.1:8090
idler serve --web 8091               127.0.0.1:8091
idler serve --web [::1]:8090         IPv6 loopback
idler serve --web 0.0.0.0:8090       every network of this computer, see below
idler service install --web          the service serves it too
```

Every request needs a token. It is made once, at random, and kept next to the settings (`Manifold-Idler.web-token` beside
`Manifold-Idler.ini`).
The server prints the whole address when it starts, `http://127.0.0.1:8090/?token=…`, and `idler web` prints it again, also for
the service. A new token: delete the file and start the server again. One of your own: write 16 to 256 letters, digits, `-` or
`_` into the file.

The server answers its own page only. A request has to name the computer by an IP address, `localhost` or the computer's own name,
so a foreign web site that points its name at this computer gets nothing (DNS rebinding). Requests sent by other pages are refused,
and no answer allows another origin. The page itself runs no code but its own.

The page talks JSON to `/api`, and a script can too:

```
curl -H "X-Idler-Token: $(cat ~/.config/manifold-idler/Manifold-Idler.web-token)" -H "Content-Type: application/json" \
     -d '["queue"]' http://127.0.0.1:8090/api
```

The body is the command as a list of words, as on the socket: `["add", "for", "3", "--", "620"]`, `["games", "50", "portal"]`.
The answer holds the text the command prints, and for the commands that list something the same as rows. A cover comes from
`/cover/<appid>`, with the token in the address like the page's, and a theme's files from `/themes/<token>/<theme>/<file>`,
with the token in the path, so the files of a theme find each other.

### Themes

A theme is a folder with a `theme.css`, and a `theme.ini` for its name, its screens and its options. Themes of your own go
into `Manifold-Idler.themes` beside the settings, a folder each, or into the folder given with `idler serve --web --themes
<folder>`. One with the name of a built-in theme takes its place, and the files it lacks, such as fonts, come from the
built-in one. The page lists new themes after a reload.

```
idler themes                         the themes, and what is wrong with those of your own
idler themes copy arcade my-arcade   a copy of Arcade, fonts included, to change in the themes folder
```

A theme is CSS: no scripts, and nothing from other servers. [Docs/WEB-THEMES.md](../Docs/WEB-THEMES.md) lists what it can
change and where the page lets it move.

By default only this computer reaches the page. An address like `0.0.0.0:8090` opens it to the network, over plain HTTP: whoever
can see the traffic sees the token and can do what the page does. On a network of your own that may do; otherwise use a tunnel.

### From another computer, through SSH

The page stays on `127.0.0.1` on the server, and SSH carries it over:

```
ssh -N -L 8090:127.0.0.1:8090 you@server
```

While that runs, the address `idler web` prints on the server opens in the browser here. If 8090 is taken here, pick another port
for this side, `-L 9000:127.0.0.1:8090`, and open `http://localhost:9000/?token=…` with the same token.

## Staying up

Linux: one command makes the server a systemd user service. It runs at once and starts again at every login, so every other
command works from the same terminal:

```
idler service install          --start also starts the queue once the library is read
idler service remove           stops it and takes it out again
journalctl --user -u manifold-idler -f
```

The service starts the program where it lies when you install it, so move it to a lasting place first (`~/.local/bin`, say).
A service installed by an earlier version still uses `idler.ini`: `idler service remove`, then `idler service install`, moves
it to the shared file.
With `--settings`, each settings file gets a service of its own. On a server without a login session, `loginctl enable-linger`
lets user services start at boot.

Windows: a task at logon, in the session Steam runs in.

On a Linux desktop that may go to sleep: `systemd-inhibit --what=sleep idler serve`. Windows stays awake by itself while games idle.

Not here, compared with the window: creatures to look at, achievement lists and their pictures, the card farm, shutting down at the end.

## A server without a desktop

**Untested on a dedicated server.** The idler is tested on a Linux desktop (CachyOS). The steps below are the usual way to run
Steam without a screen; Steam may behave differently from one distribution to the next.

The idler needs nothing graphical. Steam does: its client wants a display even when it shows no window. A virtual one will do.

1. Install Steam from the distribution, not the Flatpak, and Xvfb (`xvfb` on Debian and Ubuntu, `xorg-server-xvfb` on Arch).
2. Sign in once, by hand. Steam asks for the password and Steam Guard on a screen, so it needs one this one time: run `steam`
   over `ssh -X` from a computer with a desktop, or start it on Xvfb and look at that display with a VNC viewer. Tick
   "Remember me". From then on Steam signs in by itself.
3. Let Steam run on a virtual display, as a user service of its own. `~/.config/systemd/user/steam-headless.service`:

   ```
   [Unit]
   Description=Steam on a virtual display

   [Service]
   ExecStart=/usr/bin/xvfb-run -a /usr/bin/steam -silent
   Restart=on-failure
   RestartSec=30

   [Install]
   WantedBy=default.target
   ```

   ```
   systemctl --user enable --now steam-headless
   ```

   Where Steam lies, `which steam` tells: `/usr/bin/steam` on Arch, `/usr/games/steam` on Debian and Ubuntu. For a quick try in a
   terminal instead: `xvfb-run -a steam -silent &`.
4. The idler as a service, with the web page: `idler service install --start --web`. It waits by itself until Steam is signed in.
5. `loginctl enable-linger $USER`, so both services start at boot without anybody logging in.
6. From your own computer: the SSH tunnel above, and the address from `idler web`.

## How it is put together

- `idler_core`: the queue, the settings, the statistics, Steam's file formats. From `../Source/Core`.
- `idler_engine`: the workers, the access to Steam and the model that runs the queue. From `../Source/{Steam,App}`, with
  `Shim/App/CardService.h` standing in for the farm, and `Source/Desktop.cpp` for the window's helpers.
- `idler_cli_base`: the words on the socket, and the socket. The web page and its small HTTP server: `Source/Http.cpp` reads
  requests and writes JSON, `Source/Web.cpp` holds the token, the checks and the routes, `Source/WebPage.cpp` puts the page
  together, `Source/WebServer.cpp` the sockets. The page itself is `Web/page.html`, `base.css` and `page.js`, its built-in themes are in
  `Web/Themes` and their fonts in `../Dependencies/Fonts`; `Tools/web-assets.py` writes all of them and the program's icon into
  `Source/WebAssets.cpp`, and the tests fail while that file is behind.
- `../Dependencies/CLI11`: [CLI11](https://github.com/CLIUtils/CLI11) 2.5.0 for the command line, BSD-3-Clause, see its
  `LICENSE`.

Sockets and processes need no library: the local socket is the same call on both systems, the web page is plain sockets with the
little of HTTP it needs, and the worker's pipes are few lines of each system's own.

`ctest --test-dir Build/CLI -C Release` runs what needs neither Steam nor a server: `idler_tests` for this program, `idler_core_tests` for the queue and
the file formats, which are the window program's tests.
