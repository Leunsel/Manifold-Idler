#!/usr/bin/env python3
"""Writes src/WebFonts.cpp: the web page's two fonts from assets/fonts, as @font-face rules with the files inside.

Run it again after a font in assets/fonts changes.
"""
import argparse
import base64
import os

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FACES = [
    ("Idler Pixel", "PressStart2P.woff2", ""),
    ("Idler Text", "VT323.woff2", "size-adjust:137.5%;"),
]
WIDTH = 120


def face(family, file, extra):
    with open(os.path.join(HERE, "assets", "fonts", file), "rb") as source:
        data = base64.b64encode(source.read()).decode("ascii")
    head = '@font-face{font-family:\\"' + family + '\\";font-style:normal;font-weight:400;font-display:block;' + extra + "src:url(data:font/woff2;base64,"
    lines = ['            "' + head + '"']
    lines += ['            "' + data[at:at + WIDTH] + '"' for at in range(0, len(data), WIDTH)]
    lines.append('            ") format(\\"woff2\\")}"')
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    lines = ['#include "Web.h"', "", "namespace Idler::Cli", "{", "    std::string_view WebFontFaces()", "    {", "        return"]
    for family, file, extra in FACES:
        lines += face(family, file, extra)
    lines[-1] += ";"
    lines += ["    }", "}", ""]
    target = os.path.join(HERE, "src", "WebFonts.cpp")
    with open(target, "w", encoding="ascii", newline="\n") as out:
        out.write("\n".join(lines))
    print(target)


if __name__ == "__main__":
    main()
