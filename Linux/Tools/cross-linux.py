#!/usr/bin/env python3
"""Builds the Linux window program from any system, with Zig as a cross compiler. It lands in Build/Linux/linux-x86_64.

The sources are those of CMakeLists.txt. glibc and SDL3 stay dynamic: SDL3 comes from the system at run time, and a stub
made from the vendored SDL3 headers stands in for it while linking. The C++ library is linked in.

--stub-only <folder> builds only that stub, with the C compiler given by --cc, for a CMake build on a system without SDL3:
cmake -S Linux -B Build/Linux -DIDLER_SDL_LIBRARY=<folder>/libSDL3.so
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
TARGET = "x86_64-linux-gnu.2.31"
CORE = ["AppInfo", "Cards", "Creatures", "Errors", "FileLock", "Library", "MiniGames", "Queue", "RuntimeLog", "Settings", "Stats", "Terrarium", "Text", "Vdf", "WebPolicy"]
STEAM = ["LibraryScan", "SteamClient", "SteamLocal", "SteamWebNone", "Worker", "WorkerProcessPosix"]
UI = ["Covers", "Fonts", "IdleAnimation", "MenuWidgets", "MiniGameView", "ScreenMain", "TerrariumView",
      "Scenes/HomeAutumn", "Scenes/HomeSpring", "Scenes/HomeSummer", "Scenes/HomeWinter", "Scenes/StageClouds", "Scenes/StageMeadow", "Scenes/StageUnderwater"]
APP = ["IdlerModel", "CardService"]
OWN = ["MainSdl", "App/SdlApplication", "App/Gl", "App/WebPanelNone", "App/DesktopLinux", "UI/CoversSdl"]
IMGUI = ["imgui", "imgui_draw", "imgui_tables", "imgui_widgets", "backends/imgui_impl_sdl3", "backends/imgui_impl_opengl3"]
FONTS = [("Text", "NotoSans-Regular.ttf"), ("Bold", "NotoSans-Bold.ttf"), ("Icons", "fa-solid-900.ttf")]


def write_if_changed(path, text):
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            if f.read() == text:
                return
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def font_data(target):
    lines = ['#include "UI/FontData.h"', "", "namespace Idler::FontData", "{", "    namespace", "    {"]
    for name, file in FONTS:
        with open(os.path.join(ROOT, "Dependencies", "Fonts", file), "rb") as f:
            data = f.read()
        lines.append("        const unsigned char k%s[] = {" % name)
        for start in range(0, len(data), 32):
            lines.append("".join("0x%02x," % byte for byte in data[start:start + 32]))
        lines.append("        };")
        lines.append("")
    lines[-1] = "    }"
    lines.append("")
    for name, _ in FONTS:
        lines += ["    Blob %s()" % name, "    {", "        return Blob{ k%s, static_cast<int>(sizeof(k%s)) };" % (name, name), "    }", ""]
    lines.append("}")
    write_if_changed(target, "\n".join(lines) + "\n")


def sdl_stub(compiler, folder):
    names = set()
    headers = os.path.join(ROOT, "Dependencies", "SDL3", "include", "SDL3")
    for header in os.listdir(headers):
        with open(os.path.join(headers, header), encoding="utf-8", errors="replace") as f:
            text = f.read()
        names.update(re.findall(r"extern\s+SDL_DECLSPEC\b[^;]*?\bSDLCALL\s+(SDL_\w+)\s*\(", text))
    source = os.path.join(folder, "sdl3-stub.c")
    write_if_changed(source, "".join("void %s(void) {}\n" % name for name in sorted(names)))
    library = os.path.join(folder, "libSDL3.so")
    run = subprocess.run(compiler + ["-shared", "-fPIC", "-Wl,-soname,libSDL3.so.0", source, "-o", library],
                         capture_output=True, text=True, encoding="utf-8", errors="replace")
    if run.returncode != 0:
        sys.exit("The SDL3 stub did not build:\n" + run.stderr)
    return len(names)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", default=shutil.which("zig"), help="zig, if it is not on the path")
    parser.add_argument("--jobs", type=int, default=min(6, os.cpu_count() or 4), help="compilers at once")
    parser.add_argument("--stub-only", metavar="FOLDER", help="build only the SDL3 stub, into this folder")
    parser.add_argument("--cc", default="cc", help="the C compiler for --stub-only")
    arguments = parser.parse_args()
    if arguments.stub_only:
        os.makedirs(arguments.stub_only, exist_ok=True)
        count = sdl_stub([arguments.cc], arguments.stub_only)
        print("SDL3 stub with %d functions: %s" % (count, os.path.join(arguments.stub_only, "libSDL3.so")))
        return
    if not arguments.zig:
        sys.exit("Zig was not found. Give it with --zig.")

    source = os.path.join(ROOT, "Source")
    linux = os.path.join(HERE, "Source")
    dependencies = os.path.join(ROOT, "Dependencies")
    out = os.path.join(ROOT, "Build", "Linux", "linux-x86_64")
    objects = os.path.join(out, "obj")
    generated = os.path.join(out, "generated")
    stub = os.path.join(out, "stub")
    for folder in (objects, generated, stub):
        os.makedirs(folder, exist_ok=True)

    font_file = os.path.join(generated, "FontData.cpp")
    font_data(font_file)
    print("SDL3 stub with %d functions" % sdl_stub([arguments.zig, "cc", "-target", TARGET], stub))

    own = [os.path.join(linux, name.replace("/", os.sep) + ".cpp") for name in OWN] + [font_file]
    own += [os.path.join(source, "Core", name + ".cpp") for name in CORE]
    own += [os.path.join(source, "Steam", name + ".cpp") for name in STEAM]
    own += [os.path.join(source, "UI", name.replace("/", os.sep) + ".cpp") for name in UI]
    own += [os.path.join(source, "App", name + ".cpp") for name in APP]
    imgui = [os.path.join(dependencies, "ImGui", name.replace("/", os.sep) + ".cpp") for name in IMGUI]

    includes = ["-I", linux, "-I", source, "-I", os.path.join(dependencies, "ImGui"), "-I", os.path.join(dependencies, "ImGui", "backends"),
                "-isystem", os.path.join(dependencies, "SDL3", "include"), "-isystem", os.path.join(dependencies, "stb")]
    common = ["-target", TARGET, "-std=c++20", "-O2"]

    def compile_one(job):
        path, flags = job
        name = os.path.relpath(path, ROOT).replace(os.sep, "_").replace("..", "up")
        target = os.path.join(objects, os.path.splitext(name)[0] + ".o")
        run = subprocess.run([arguments.zig, "c++"] + common + flags + includes + ["-c", path, "-o", target],
                             capture_output=True, text=True, encoding="utf-8", errors="replace")
        return path, target, run.returncode, run.stderr

    jobs = [(path, ["-Wall", "-Wextra", "-DIDLER_BUNDLED_FONTS"]) for path in own] + [(path, ["-w"]) for path in imgui]
    built = []
    failed = False
    with ThreadPoolExecutor(max_workers=max(1, arguments.jobs)) as pool:
        for path, target, code, messages in pool.map(compile_one, jobs):
            built.append(target)
            if messages.strip():
                print(os.path.relpath(path, ROOT) + ":\n" + messages)
            failed = failed or code != 0
    if failed:
        sys.exit("It did not compile.")

    program = os.path.join(out, "manifold-idler")
    command = [arguments.zig, "c++", "-target", TARGET, "-s"] + built + ["-o", program, "-L", stub, "-lSDL3", "-lpthread", "-ldl"]
    if subprocess.run(command).returncode != 0:
        sys.exit("It did not link.")
    print(program)


if __name__ == "__main__":
    main()
