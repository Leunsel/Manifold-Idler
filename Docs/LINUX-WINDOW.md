# The window program on Linux

The Linux window program has the same pages, queue, creatures and statistics as the Windows one. It is built with SDL3 and
OpenGL 3. How to get it, build it and where its files are: [Linux/README.md](../Linux/README.md). This page says what differs
from Windows, what is still missing, how the code is split, and how to check a build.

## What differs from Windows

| | Windows | Linux |
|---|---|---|
| Window | own title bar, rounded corners | own title bar; the compositor moves and resizes it, also on Wayland; square corners |
| Double click on the title bar | maximises | maximises under X11; Wayland does not report the click |
| Window position | kept | kept under X11; under Wayland only size and maximised, since a Wayland program cannot place itself |
| A second start | brings the running window forward | says that the program runs already and ends |
| Splash | at start | none; the window opens when it is ready |
| Tray and notifications | yes | none; closing the window ends the program |
| "Only when I am away" | yes | not offered |
| The creatures' tug on the mouse | yes | under X11; off under Wayland, which does not let a program move the pointer |
| Achievement pictures | from Steam's public image server | none; the lists themselves are there |
| Keep the computer awake | while games idle | while games idle, through `systemd-inhibit --what=sleep`; the screen may still turn off |
| Shut down after the queue | Windows' shutdown | `systemctl poweroff` |
| Start with the system | "Start with Windows" | "Start on login": `~/.config/autostart/manifold-idler.desktop`, started minimised |
| Fonts | Tahoma, system fonts for other scripts | Noto Sans and Font Awesome built in; fontconfig finds fonts for other scripts |
| Card farm and Steam sign-in | only in builds made with it | none; WebView2 does not exist on Linux, and the Linux build refuses `IDLER_CARD_FARM` |

`SDL_VIDEO_DRIVER=x11` starts it under X11 (XWayland) instead of Wayland, for the double click, the position and the tug.

## Still missing

- **Tray.** `SDL_CreateTray` with Open, Start or Stop idling, and Quit. SDL builds it on libayatana-appindicator, loaded at
  run time; KDE shows it through StatusNotifierItem. Without that library there is no tray, and the tray settings stay hidden.
- **Notifications** for a finished queue and creature news, through `notify-send` (libnotify) where it is installed.
- **Splash**, a small borderless window like the Windows one.
- **A second start that brings the first window forward**, through a Unix socket in `$XDG_RUNTIME_DIR`.
- **"Only when I am away".** The time since the last input comes from the XScreenSaver extension under X11, from
  `org.freedesktop.ScreenSaver` (`GetSessionIdleTime`) under KDE, and from `org.gnome.Mutter.IdleMonitor` under GNOME.
- **Achievement pictures**, through libcurl loaded at run time, with the same host rules as on Windows.
- Possibly later: rounded corners, a `.desktop` file with icon and Wayland app id, a package such as an AppImage.

`pacman -Q libayatana-appindicator libnotify` shows whether the two libraries for the tray and the notifications are installed.

## Why SDL3 and OpenGL 3

The program draws its own title bar and window buttons. A Wayland compositor moves and resizes such a window only when the
program answers a hit test for its drag areas and edges, and of the usual libraries only SDL3 has one. SDL3 also brings a
tray, the pointer, display scale per window, the clipboard and message boxes.

SDL3 comes from the system at run time (`libSDL3.so.0`). Its headers, version 3.2, are in `Dependencies/SDL3`, so the program
only uses functions that every SDL 3 has. Where the build system has no SDL3, a stub made from those headers stands in while
linking (`Linux/Tools/cross-linux.py`, also with `--stub-only`).

Dear ImGui draws through `imgui_impl_sdl3` and `imgui_impl_opengl3`, from the same release as the vendored ImGui. The OpenGL
backend loads GL itself at run time, so nothing links against GL.

## How the code is split

| File | |
|---|---|
| `Source/App/Shell.h` | What the pages ask of a window: model, drag areas, minimise, maximise, close, zoom. The Windows `Application` and the Linux `SdlApplication` both provide it. |
| `Source/App/WindowCommon.h` | What both windows share: sizes, zoom, style scaling, frame pacing. |
| `Source/App/StartOptions.h` | The command line options of both programs. |
| `Source/UI/Covers.cpp` | The cover cache and its worker. `Windows/Source/UI/CoversWin32.cpp` decodes with WIC and makes D3D11 textures; `Linux/Source/UI/CoversSdl.cpp` decodes with stb_image and uploads to OpenGL on the main thread. |
| `Source/UI/Fonts.cpp` | Tahoma on Windows; the built-in Noto Sans and Font Awesome with `IDLER_BUNDLED_FONTS`, always on Linux, plus fontconfig. |
| `Source/Steam/SteamWebNone.cpp`, `Linux/Source/App/WebPanelNone.cpp` | Stand in for the network parts and the sign-in panel that Linux does not have. |
| `Linux/Source/MainSdl.cpp` | Start-up: the worker check, the settings lock, the options. A worker is the program itself, started again. |
| `Linux/Source/App/SdlApplication.cpp` | The window, the hit test, scale, frame pacing, `--capture`. |
| `Linux/Source/App/DesktopLinux.cpp` | Steam links (`steam <link>`), the settings path, keeping awake, shutdown, start on login, the pointer, and which settings the platform offers. |

The settings page asks the platform which rows exist, so it hides the tray rows without a tray, "Only when I am away" without
the idle time, and the tug where the pointer cannot be moved.

## Checking a build

Under Wayland, the default, and once with `SDL_VIDEO_DRIVER=x11`:

1. It starts. Without SDL3 3.2 or newer it does not: `pacman -Q sdl3` shows the installed version.
2. The title bar: drag, double click, the three buttons.
3. Resizing at all four edges and corners; the minimum size holds.
4. Scaling at 100, 125 and 200 percent: sharp text, nothing cut off.
5. Covers, the avatar, game names in other scripts such as Japanese or Chinese.
6. Idling: Steam shows the games as played.
7. The Steam links: store page, library page, Steam itself.
8. Keeping awake: `systemd-inhibit --list` shows Manifold Idler while games idle.
9. Shutting down after the queue: first with `--fake-shutdown=<file>`, which writes that file instead of shutting down.
10. Start on login: the program starts minimised after logging in.

## A test bed on Windows

The same SDL build compiles on Windows, for checks only, not for release: CMake on `Linux/` with
`-DSDL3_DIR=<SDL3-devel-VC>/cmake`, and `SDL3.dll` from SDL3's Windows development package beside the program. `--capture`
saves screenshots of the SDL path there too.
