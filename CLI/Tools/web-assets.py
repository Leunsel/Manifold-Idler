#!/usr/bin/env python3
"""Writes Source/WebAssets.cpp: the web page from Web/page.html, page.css and page.js, its two fonts from
Dependencies/Fonts, and the program's icon from Source/Resources/icon.ico.

Run it again after any of these files changes; the CLI tests fail while Source/WebAssets.cpp is behind them.
"""
import argparse
import base64
import os
import struct

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)
FONTS = [
    ("Idler Pixel", "PressStart2P.woff2", ""),
    ("Idler Text", "VT323.woff2", "size-adjust:137.5%;"),
]
PIECE = 100
CHUNK = 12000


def read_text(name):
    with open(os.path.join(HERE, "Web", name), encoding="utf-8") as source:
        return source.read().replace("\r\n", "\n")


def font_faces():
    faces = ""
    for family, file, extra in FONTS:
        with open(os.path.join(ROOT, "Dependencies", "Fonts", file), "rb") as source:
            data = base64.b64encode(source.read()).decode("ascii")
        faces += ('@font-face{font-family:"' + family + '";font-style:normal;font-weight:400;font-display:block;' + extra +
                  "src:url(data:font/woff2;base64," + data + ') format("woff2")}\n')
    return faces


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
    return "data:image/png;base64," + base64.b64encode(best[1]).decode("ascii")


def literal(text):
    out = []
    for character in text:
        if character == "\\":
            out.append("\\\\")
        elif character == '"':
            out.append('\\"')
        elif character == "\n":
            out.append("\\n")
        elif character == "\t":
            out.append("\\t")
        elif character == "?":
            out.append("\\?")
        else:
            out.append(character)
    return '"' + "".join(out) + '"'


def array(name, text):
    lines = ["        constexpr std::string_view " + name + "[] = {"]
    for start in range(0, max(len(text), 1), CHUNK):
        chunk = text[start:start + CHUNK]
        pieces = [chunk[at:at + PIECE] for at in range(0, len(chunk), PIECE)] or [""]
        lines.append("            " + "\n            ".join(literal(piece) for piece in pieces) + ",")
    lines.append("        };")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    parts = [
        ("kFonts", "WebFontFaces", font_faces()),
        ("kLogo", "WebLogo", logo()),
        ("kHtml", "WebPageHtml", read_text("page.html")),
        ("kStyle", "WebPageStyle", read_text("page.css")),
        ("kScript", "WebPageScript", read_text("page.js")),
    ]
    lines = ['#include "Web.h"', "", "#include <string>", "#include <string_view>", "", "namespace Idler::Cli", "{", "    namespace", "    {"]
    for name, _, text in parts:
        lines += array(name, text)
        lines.append("")
    lines += ["        template <std::size_t Count>", "        std::string Join(const std::string_view (&chunks)[Count])", "        {",
              "            std::string text;", "            for (const std::string_view chunk : chunks)", "                text += chunk;",
              "            return text;", "        }", "    }"]
    for name, function, _ in parts:
        lines += ["", "    std::string " + function + "()", "    {", "        return Join(" + name + ");", "    }"]
    lines += ["}", ""]
    target = os.path.join(HERE, "Source", "WebAssets.cpp")
    with open(target, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines))
    print(target)


if __name__ == "__main__":
    main()
