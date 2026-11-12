#!/usr/bin/env python3
"""Builds the Linux program from any system, with Zig as a cross compiler. It lands in Build/CLI/linux-x86_64.

The sources are those of CMakeLists.txt. glibc stays dynamic, as Steam's client library needs it; the C++ library is linked in.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = "x86_64-linux-gnu.2.31"
CORE = ["AppInfo", "Cards", "Creatures", "Errors", "FileLock", "Library", "MiniGames", "Queue", "RuntimeLog", "Settings", "Stats", "Terrarium", "Text", "Vdf", "WebPolicy"]
STEAM = ["LibraryScan", "SteamClient", "SteamLocal", "Worker", "WorkerProcessPosix"]
BASE = ["Http", "Ipc", "Paths", "Request", "Service", "Themes", "Web", "WebAssets", "WebPage", "WebServer"]
PROGRAM = ["main", "Commands", "PageCommands", "Serve", "Desktop"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", default=shutil.which("zig"), help="zig, if it is not on the path")
    parser.add_argument("--source", default=os.path.join(HERE, ".."), help="the Manifold-Idler folder")
    arguments = parser.parse_args()
    if not arguments.zig:
        sys.exit("Zig was not found. Give it with --zig.")

    root = os.path.abspath(arguments.source)
    source = os.path.join(root, "Source")
    src = os.path.join(HERE, "Source")
    out = os.path.join(root, "Build", "CLI", "linux-x86_64")
    objects = os.path.join(out, "obj")
    os.makedirs(objects, exist_ok=True)

    core = [os.path.join(source, "Core", name + ".cpp") for name in CORE]
    engine = [os.path.join(source, "App", "IdlerModel.cpp")] + [os.path.join(source, "Steam", name + ".cpp") for name in STEAM + ["SteamWebNone"]]
    base = [os.path.join(src, name + ".cpp") for name in BASE]
    program = [os.path.join(src, name + ".cpp") for name in PROGRAM]
    cli_tests = os.path.join(HERE, "Tests", "CliTests.cpp")
    core_tests = os.path.join(root, "Tests", "Tests.cpp")

    includes = ["-I", os.path.join(HERE, "Shim"), "-I", source, "-I", src, "-isystem", os.path.join(root, "Dependencies", "CLI11")]
    flags = ["-target", TARGET, "-std=c++20", "-O2", "-Wall", "-Wextra", "-DIDLER_CLI"]

    def compile_one(path):
        name = os.path.basename(os.path.dirname(path)) + "_" + os.path.splitext(os.path.basename(path))[0] + ".o"
        target = os.path.join(objects, name)
        run = subprocess.run([arguments.zig, "c++"] + flags + includes + ["-c", path, "-o", target], capture_output=True, text=True, encoding="utf-8", errors="replace")
        return path, target, run.returncode, run.stderr

    built = {}
    failed = False
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        for path, target, code, messages in pool.map(compile_one, core + engine + base + program + [cli_tests, core_tests]):
            built[path] = target
            if messages.strip():
                print(os.path.basename(path) + ":\n" + messages)
            failed = failed or code != 0
    if failed:
        sys.exit("It did not compile.")

    def link(name, sources):
        command = [arguments.zig, "c++", "-target", TARGET, "-s"] + [built[path] for path in sources] + ["-o", os.path.join(out, name), "-lpthread", "-ldl"]
        if subprocess.run(command).returncode != 0:
            sys.exit("It did not link: " + name)
        print(os.path.join(out, name))

    link("idler", core + engine + base + program)
    link("idler_tests", [cli_tests] + base + core)
    link("idler_core_tests", [core_tests] + core)


if __name__ == "__main__":
    main()
