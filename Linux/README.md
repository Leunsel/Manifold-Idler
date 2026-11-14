# Manifold Idler on Linux

The window program, built for Linux with SDL3 and OpenGL 3. Same sources as the Windows program, other window and
desktop parts. What differs from Windows and what is still missing: [Docs/LINUX-WINDOW.md](../Docs/LINUX-WINDOW.md).

**An early version.** Tested on CachyOS; other distributions are untested. No splash, no tray, no notifications, no achievement pictures, and
"only when I am away" is not offered. Closing the window ends the program.

## What it needs

- x86_64 and OpenGL 3. The release is built on Ubuntu 24.04 and wants a glibc at least as new as that one's (2.39); the Zig
  build below runs from glibc 2.31 on.
- SDL3 3.2 or newer from the system: `sudo pacman -S sdl3` on Arch and CachyOS.
- Steam installed normally and signed in, as for the CLI. Not the Flatpak.

## Getting it

Ready-built, it is `Manifold-Idler-linux-x86_64.zip` on the [Releases](../../../releases) page: unzip, then
`./manifold-idler`. Every push also leaves it as an artifact of its run on the Actions page, for whoever is signed in to
GitHub.

## Building

From the repository's folder, on Windows or any other system, with [Zig](https://ziglang.org):

```
python Linux/Tools/cross-linux.py --zig <zig>
```

It lands in `Build/Linux/linux-x86_64/manifold-idler` and can be copied over as it is.

On Linux, with CMake and SDL3's development files:

```
cmake -S Linux -B Build/Linux -DCMAKE_BUILD_TYPE=Release
cmake --build Build/Linux
```

Without SDL3 on the build system, as on Ubuntu 24.04, a stub stands in while linking; the program still takes the real
SDL3 at run time. This is how GitHub Actions builds it:

```
python3 Linux/Tools/cross-linux.py --stub-only Build/Linux/stub --cc clang
cmake -S Linux -B Build/Linux -DCMAKE_BUILD_TYPE=Release -DIDLER_WARNINGS_AS_ERRORS=ON -DIDLER_SDL_LIBRARY="$PWD/Build/Linux/stub/libSDL3.so"
cmake --build Build/Linux
```

`-DIDLER_WARNINGS_AS_ERRORS=ON` stops the build at any compiler warning, as GitHub Actions does. Leave it out when a
newer compiler than the one in CI warns about something new and you only want the program.

`Source/` here holds what only this program has: SDL3 and OpenGL, the desktop helpers, the covers; the rest is the
repository's `Source/`.

## Where things are

The settings are in `$XDG_CONFIG_HOME/manifold-idler/Manifold-Idler.ini` (usually `~/.config/manifold-idler/`), with the
statistics and the log beside them. The CLI uses the same file, so both see the same lists and statistics; only one of them
runs at a time. "Start on login" writes `~/.config/autostart/manifold-idler.desktop`.

`SDL_VIDEO_DRIVER=x11` starts it under X11 instead of Wayland.
