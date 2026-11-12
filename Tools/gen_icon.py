#!/usr/bin/env python3
import io
import os
import struct

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "Dependencies", "Fonts", "fa-solid-900.ttf")
ICO = os.path.join(ROOT, "Source", "Resources", "icon.ico")

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
PNG_FROM = 128
BASE = 1024

HOURGLASS_HALF = ""
EDGE = (116, 226, 186, 255)
TILE = (20, 22, 26, 255)
GLYPH = (46, 219, 158, 255)


def draw_base():
    image = Image.new("RGBA", (BASE, BASE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    margin = 40
    draw.rounded_rectangle((margin, margin, BASE - margin, BASE - margin), radius=190, fill=EDGE)
    edge = 44
    draw.rounded_rectangle((margin + edge, margin + edge, BASE - margin - edge, BASE - margin - edge), radius=190 - edge, fill=TILE)

    font = ImageFont.truetype(FONT, 600)
    left, top, right, bottom = draw.textbbox((0, 0), HOURGLASS_HALF, font=font)
    draw.text(((BASE - (right - left)) / 2 - left, (BASE - (bottom - top)) / 2 - top), HOURGLASS_HALF, font=font, fill=GLYPH)
    return image


def dib_entry(image):
    width, height = image.size
    pixels = image.load()
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    colour = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            colour += bytes((b, g, r, a))
    return header + bytes(colour) + bytes(((width + 31) // 32) * 4 * height)


def png_entry(image):
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def main():
    base = draw_base()
    entries = []
    for size in SIZES:
        scaled = base.resize((size, size), Image.Resampling.LANCZOS)
        entries.append((size, png_entry(scaled) if size >= PNG_FROM else dib_entry(scaled)))

    out = bytearray(struct.pack("<HHH", 0, 1, len(entries)))
    offset = 6 + 16 * len(entries)
    for size, data in entries:
        out += struct.pack("<BBBBHHII", 0 if size == 256 else size, 0 if size == 256 else size, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, data in entries:
        out += data
    with open(ICO, "wb") as file:
        file.write(out)
    print("%s: %d images, %d bytes" % (os.path.relpath(ICO, ROOT), len(entries), len(out)))


if __name__ == "__main__":
    main()
