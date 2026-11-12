#!/usr/bin/env python3
"""Writes Source/WebAssets.cpp: the web page from Web/page.html, base.css and page.js, the program's icon from
Source/Resources/icon.ico, and the built-in themes, every file in Web/Themes/<theme>/ plus the fonts each one takes
from Dependencies/Fonts.

Run it again after any of these files changes; the CLI tests fail while Source/WebAssets.cpp is behind them.
"""
import argparse
import base64
import os
import struct

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)
THEME_FONTS = {
    "arcade": ["PressStart2P.woff2", "VT323.woff2"],
}
TEXT_ENDINGS = (".css", ".ini", ".html", ".js", ".svg", ".txt")
PIECE = 100
CHUNK = 12000


def read_text(name):
    with open(os.path.join(HERE, "Web", name), encoding="utf-8") as source:
        return source.read().replace("\r\n", "\n").encode("utf-8")


def read_file(path):
    with open(path, "rb") as source:
        data = source.read()
    if path.lower().endswith(TEXT_ENDINGS):
        data = data.replace(b"\r\n", b"\n")
    return data


def theme_files():
    files = []
    folder = os.path.join(HERE, "Web", "Themes")
    for theme in sorted(os.listdir(folder)):
        path = os.path.join(folder, theme)
        if not os.path.isdir(path):
            continue
        if not os.path.isfile(os.path.join(path, "theme.css")):
            raise SystemExit("Web/Themes/" + theme + " has no theme.css.")
        found = {}
        for base, _, names in os.walk(path):
            for name in names:
                full = os.path.join(base, name)
                found[os.path.relpath(full, path).replace(os.sep, "/")] = read_file(full)
        for font in THEME_FONTS.get(theme, []):
            if font in found:
                raise SystemExit("Web/Themes/" + theme + "/" + font + " is taken from Dependencies/Fonts; remove the copy.")
            found[font] = read_file(os.path.join(ROOT, "Dependencies", "Fonts", font))
        for relative in sorted(found):
            files.append((theme + "/" + relative, found[relative]))
    return files


def logo():
    with open(os.path.join(ROOT, "Source", "Resources", "icon.ico"), "rb") as source:
        data = source.read()
    count = struct.unpack("<HHH", data[:6])[2]
    best = None
    for index in range(count):
        width, _, _, _, _, _, size, offset = struct.unpack("<BBBBHHII", data[6 + 16 * index:22 + 16 * index])
        width = width or 256
        if data[offset:offset + 4] == b"\x89PNG" and width <= 128 and (best is None or width > best[0]):
            best = (width, data[offset:offset + size])
    if best is None:
        raise SystemExit("Source/Resources/icon.ico has no PNG picture of 128 pixels or less.")
    return ("data:image/png;base64," + base64.b64encode(best[1]).decode("ascii")).encode("ascii")


def literal(data):
    out = []
    octal = False
    for byte in data:
        after_octal = octal
        octal = False
        if after_octal and 0x30 <= byte <= 0x39:
            out.append("\\%03o" % byte)
            octal = True
        elif byte == 0x5C:
            out.append("\\\\")
        elif byte == 0x22:
            out.append('\\"')
        elif byte == 0x3F:
            out.append("\\?")
        elif byte == 0x0A:
            out.append("\\n")
        elif byte == 0x09:
            out.append("\\t")
        elif 0x20 <= byte < 0x7F:
            out.append(chr(byte))
        else:
            out.append("\\%03o" % byte)
            octal = True
    return '"' + "".join(out) + '"'


def array(name, data):
    lines = ["        constexpr std::string_view " + name + "[] = {"]
    for start in range(0, max(len(data), 1), CHUNK):
        chunk = data[start:start + CHUNK]
        pieces = [chunk[at:at + PIECE] for at in range(0, len(chunk), PIECE)] or [b""]
        lines.append("            " + "\n            ".join(literal(piece) for piece in pieces) + "sv,")
    lines.append("        };")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    parts = [
        ("kLogo", "WebLogo", logo()),
        ("kHtml", "WebPageHtml", read_text("page.html")),
        ("kStyle", "WebPageStyle", read_text("base.css")),
        ("kScript", "WebPageScript", read_text("page.js")),
    ]
    themes = theme_files()
    lines = ['#include "Themes.h"', '#include "Web.h"', "", "#include <string>", "#include <string_view>", "#include <vector>", "",
             "namespace Idler::Cli", "{", "    namespace", "    {", "        using namespace std::string_view_literals;", ""]
    for name, _, data in parts:
        lines += array(name, data)
        lines.append("")
    for index, (_, data) in enumerate(themes):
        lines += array("kTheme" + str(index), data)
        lines.append("")
    lines += ["        template <std::size_t Count>", "        std::string Join(const std::string_view (&chunks)[Count])", "        {",
              "            std::string text;", "            for (const std::string_view chunk : chunks)", "                text += chunk;",
              "            return text;", "        }", "    }"]
    for name, function, _ in parts:
        lines += ["", "    std::string " + function + "()", "    {", "        return Join(" + name + ");", "    }"]
    lines += ["", "    const std::vector<EmbeddedFile>& BuiltInThemeFiles()", "    {", "        static const std::vector<EmbeddedFile> files = {"]
    for index, (path, _) in enumerate(themes):
        lines.append("            { " + literal(path.encode("utf-8")) + ", Join(kTheme" + str(index) + ") },")
    lines += ["        };", "        return files;", "    }", "}", ""]
    target = os.path.join(HERE, "Source", "WebAssets.cpp")
    with open(target, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines))
    print(target)
    for path, data in themes:
        print("  theme file", path, len(data), "bytes")


if __name__ == "__main__":
    main()
