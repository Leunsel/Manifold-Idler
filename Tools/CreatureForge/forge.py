#!/usr/bin/env python3
"""Creature forge: renders and checks the idle creatures of Manifold Idler the way the program draws them. Standard library only.

Every creature is a JSON file in creatures/, and the evolved form of one, where it has one, in creatures/evolved/ under the same name:
  {"key": "frog", "id": "Frog", "body": "#5CC756", "second": "#D1E58F", "third": null,
   "floats": false, "objectLike": false, "facing": 0, "stays": false,
   "frames": [[14 strings of 16 cells, as grid.json sets], ...],
   "steps": [[frame, lift, shift, blink, ticks], ...],
   "sleep": 0}

Cells:
  .  nothing
  #  body         o  body in shade     +  body in light
  a  second       d  second in shade   l  second in light
  c  third        C  third in shade
  *  eye on body (a blink closes it with body shade)
  e  eye on second (a blink closes it with second shade)
  r  red   y  yellow   w  white   k  dark grey

Commands (run from anywhere):
  forge.py sheet KEY                  out/KEY.png and the lint report; an evolved form is evolved/KEY
  forge.py lint [KEY]                 lint report
  forge.py publish KEY ROUND CODE     lint, keep versions/KEY/rROUND.json, and blind/CODE-large.png + CODE-small.png to compare drafts
  forge.py contact OUT [--scale N] [--frame N] [--keys a,b] [--sleep] [--shadow] [--nolabel] [--columns N]
  forge.py restore KEY ROUND          creatures/KEY.json from a snapshot
  forge.py header OUT                 generates the header, OUT being Source/UI/CreatureSprites.h
"""

import json
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
if '--root' in sys.argv:
    _at = sys.argv.index('--root')
    HERE = os.path.abspath(sys.argv[_at + 1])
    del sys.argv[_at:_at + 2]
CREATURES = os.path.join(HERE, 'creatures')
OUT = os.path.join(HERE, 'out')
VERSIONS = os.path.join(HERE, 'versions')
BLIND = os.path.join(HERE, 'blind')

SETTINGS_FILE = os.path.join(HERE, 'grid.json')
COLUMNS, ROWS = 12, 11
if os.path.exists(SETTINGS_FILE):
    with open(SETTINGS_FILE, encoding='utf-8') as _file:
        _grid = json.load(_file)
    COLUMNS, ROWS = _grid['columns'], _grid['rows']

ORDER = ['slime', 'ghost', 'cat', 'robot', 'frog', 'whale', 'bunny', 'mushroom', 'bat', 'penguin', 'leetcat', 'dragon', 'ufo', 'superslime',
         'snail', 'turtle', 'octopus', 'jellyfish', 'hourglass', 'mimic', 'hamster', 'phoenix', 'centipede', 'trophy', 'sheep', 'owl',
         'pumpkin', 'snowman', 'partyblob', 'blackcat', 'fish', 'cake', 'bug', 'manifold', 'heartbuddy', 'star', 'spaceship', 'cube', 'hat',
         'chicken', 'crab', 'kingslime', 'cryingkid', 'bean', 'nightcam', 'zombie', 'wyvern', 'dwarf', 'bonfire', 'error', 'rocketcar', 'cheese', 'duck', 'pufferfish', 'otter', 'shark', 'bee', 'firefly', 'loveletter', 'parrot', 'capybara', 'sloth', 'cactus', 'coffee', 'ghillie', 'mole', 'parkakid', 'demon']
IDS = {'slime': 'Slime', 'ghost': 'Ghost', 'cat': 'Cat', 'robot': 'Robot', 'frog': 'Frog', 'whale': 'Whale', 'bunny': 'Bunny', 'mushroom': 'Mushroom',
       'bat': 'Bat', 'penguin': 'Penguin', 'leetcat': 'LeetCat', 'dragon': 'Dragon', 'ufo': 'Ufo', 'superslime': 'SuperSlime', 'snail': 'Snail',
       'turtle': 'Turtle', 'octopus': 'Octopus', 'jellyfish': 'Jellyfish', 'hourglass': 'Hourglass', 'mimic': 'Mimic', 'hamster': 'Hamster',
       'phoenix': 'Phoenix', 'centipede': 'Centipede', 'trophy': 'Trophy', 'sheep': 'Sheep', 'owl': 'Owl', 'pumpkin': 'Pumpkin', 'snowman': 'Snowman',
       'partyblob': 'PartyBlob', 'blackcat': 'BlackCat', 'fish': 'Fish', 'cake': 'Cake', 'bug': 'Bug', 'manifold': 'Manifold', 'heartbuddy': 'HeartBuddy',
       'star': 'Star', 'spaceship': 'Spaceship', 'cube': 'Cube', 'hat': 'Hat', 'chicken': 'Chicken', 'crab': 'Crab', 'kingslime': 'KingSlime',
       'cryingkid': 'CryingKid', 'bean': 'Bean', 'nightcam': 'NightCam', 'zombie': 'Zombie', 'wyvern': 'Wyvern', 'dwarf': 'Dwarf', 'bonfire': 'Bonfire',
       'error': 'ErrorSign', 'rocketcar': 'RocketCar', 'cheese': 'Cheese'}

CELLS = set('.#o+adlcC*erywk')


def evolved_keys():
    return ['evolved/' + key for key in ORDER if os.path.exists(os.path.join(CREATURES, 'evolved', key + '.json'))]

ACCENT = (0.18, 0.86, 0.62)
SIDEBAR = (0.043, 0.046, 0.054)
PAGE = (0.058, 0.061, 0.071)
CARD = (0.082, 0.086, 0.100)
MUTED = (0.50, 0.52, 0.56)
LABEL = (0.78, 0.79, 0.82)
RED = (0.90, 0.30, 0.26)
YELLOW = (1.00, 0.82, 0.30)
WHITE = (0.94, 0.95, 0.97)
DARK = (0.27, 0.27, 0.33)
GOLD = (1.00, 0.78, 0.22)
GOLD_LIGHT = (1.00, 0.92, 0.60)
RUNNING_ROW = (0.100, 0.205, 0.176)
SELECTED = (0.095, 0.195, 0.170)
TICK_SECONDS = 0.125


def to8(color):
    return tuple(max(0, min(255, int(round(c * 255)))) for c in color)


class Canvas:
    def __init__(self, width, height, background=PAGE):
        self.width = width
        self.height = height
        r, g, b = to8(background)
        self.rows = [bytearray(bytes((r, g, b)) * width) for _ in range(height)]

    def rect(self, x, y, w, h, color):
        c = bytes(to8(color)) if len(color) == 3 and isinstance(color[0], float) else bytes(color)
        for row in range(max(0, y), min(self.height, y + h)):
            start = max(0, x)
            end = min(self.width, x + w)
            if end > start:
                self.rows[row][start * 3:end * 3] = c * (end - start)

    def save(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        raw = b''.join(b'\0' + bytes(row) for row in self.rows)

        def chunk(tag, data):
            return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', self.width, self.height, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
        with open(path, 'wb') as file:
            file.write(png)


FONT = {
    'A': '01110 10001 10001 11111 10001 10001 10001', 'B': '11110 10001 10001 11110 10001 10001 11110', 'C': '01111 10000 10000 10000 10000 10000 01111',
    'D': '11110 10001 10001 10001 10001 10001 11110', 'E': '11111 10000 10000 11110 10000 10000 11111', 'F': '11111 10000 10000 11110 10000 10000 10000',
    'G': '01111 10000 10000 10011 10001 10001 01111', 'H': '10001 10001 10001 11111 10001 10001 10001', 'I': '11111 00100 00100 00100 00100 00100 11111',
    'J': '00111 00010 00010 00010 00010 10010 01100', 'K': '10001 10010 10100 11000 10100 10010 10001', 'L': '10000 10000 10000 10000 10000 10000 11111',
    'M': '10001 11011 10101 10101 10001 10001 10001', 'N': '10001 11001 10101 10011 10001 10001 10001', 'O': '01110 10001 10001 10001 10001 10001 01110',
    'P': '11110 10001 10001 11110 10000 10000 10000', 'Q': '01110 10001 10001 10001 10101 10010 01101', 'R': '11110 10001 10001 11110 10100 10010 10001',
    'S': '01111 10000 10000 01110 00001 00001 11110', 'T': '11111 00100 00100 00100 00100 00100 00100', 'U': '10001 10001 10001 10001 10001 10001 01110',
    'V': '10001 10001 10001 10001 10001 01010 00100', 'W': '10001 10001 10001 10101 10101 11011 10001', 'X': '10001 10001 01010 00100 01010 10001 10001',
    'Y': '10001 10001 01010 00100 00100 00100 00100', 'Z': '11111 00001 00010 00100 01000 10000 11111',
    '0': '01110 10001 10011 10101 11001 10001 01110', '1': '00100 01100 00100 00100 00100 00100 01110', '2': '01110 10001 00001 00010 00100 01000 11111',
    '3': '11110 00001 00001 01110 00001 00001 11110', '4': '00010 00110 01010 10010 11111 00010 00010', '5': '11111 10000 11110 00001 00001 10001 01110',
    '6': '00110 01000 10000 11110 10001 10001 01110', '7': '11111 00001 00010 00100 01000 01000 01000', '8': '01110 10001 10001 01110 10001 10001 01110',
    '9': '01110 10001 10001 01111 00001 00010 01100', ' ': '00000 00000 00000 00000 00000 00000 00000', '-': '00000 00000 00000 01110 00000 00000 00000',
    '.': '00000 00000 00000 00000 00000 00110 00110', ':': '00000 00110 00110 00000 00110 00110 00000', '+': '00000 00100 00100 11111 00100 00100 00000',
    '/': '00001 00010 00010 00100 01000 01000 10000', '#': '01010 11111 01010 01010 01010 11111 01010',
}


def text(canvas, x, y, string, color=LABEL, scale=1):
    for character in string.upper():
        glyph = FONT.get(character, FONT[' ']).split()
        for row, line in enumerate(glyph):
            for column, bit in enumerate(line):
                if bit == '1':
                    canvas.rect(x + column * scale, y + row * scale, scale, scale, color)
        x += 6 * scale


def text_width(string, scale=1):
    return len(string) * 6 * scale


def parse_color(value):
    if value in (None, ''):
        return None
    if value == 'accent':
        return ACCENT
    value = value.lstrip('#')
    return tuple(int(value[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def scaled(color, factor):
    return tuple(min(1.0, c * factor) for c in color)


def lightened(color):
    return tuple(c + (1.0 - c) * 0.6 for c in color)


def palette(body, second, third, blink=False):
    second = second if second is not None else body
    third = third if third is not None else second
    shade = scaled(body, 0.62)
    second_shade = scaled(second, 0.62)
    third_shade = scaled(third, 0.62)
    return {'#': body, 'o': shade, '+': lightened(body), 'a': second, 'd': second_shade, 'l': lightened(second), 'c': third, 'C': third_shade,
            '*': shade if blink else SIDEBAR, 'e': second_shade if blink else SIDEBAR, 'r': RED, 'y': YELLOW, 'w': WHITE, 'k': DARK}


def creature_palette(creature, blink=False):
    return palette(parse_color(creature.get('body')), parse_color(creature.get('second')), parse_color(creature.get('third')), blink)


def grey_palette():
    colors = palette(MUTED, LABEL, scaled(MUTED, 0.85), True)
    colors['r'] = colors['y'] = scaled(MUTED, 1.15)
    colors['w'] = LABEL
    return colors


def shiny_palette(blink=False):
    return palette(GOLD, GOLD_LIGHT, GOLD, blink)


def draw_frame(canvas, x, y, frame, pixel, colors):
    for row, line in enumerate(frame):
        for column, cell in enumerate(line):
            color = colors.get(cell)
            if color is not None:
                canvas.rect(x + column * pixel, y + row * pixel, pixel, pixel, color)


def draw_shadow(canvas, x, y, frame, pixel, background):
    color = tuple(b + (1.0 - b) * 0.10 for b in background)
    for row, line in enumerate(frame):
        for column, cell in enumerate(line):
            if cell != '.':
                canvas.rect(x + column * pixel, y + row * pixel, pixel, pixel, color)


def draw_grid(canvas, x, y, pixel):
    color = (0.13, 0.135, 0.15)
    for column in range(COLUMNS + 1):
        canvas.rect(x + column * pixel, y, 1, ROWS * pixel, color)
    for row in range(ROWS + 1):
        canvas.rect(x, y + row * pixel, COLUMNS * pixel, 1, color)


def path_of(key):
    return os.path.join(CREATURES, key + '.json')


def load(key):
    with open(path_of(key), encoding='utf-8') as file:
        return json.load(file)


def save(creature, path=None):
    path = path or path_of(creature['key'])
    os.makedirs(os.path.dirname(path), exist_ok=True)
    head = {k: v for k, v in creature.items() if k not in ('frames', 'steps')}
    lines = ['{']
    for name, value in head.items():
        lines.append('  %s: %s,' % (json.dumps(name), json.dumps(value)))
    lines.append('  "frames": [')
    for index, frame in enumerate(creature['frames']):
        lines.append('    [')
        lines.extend('      %s%s' % (json.dumps(row), ',' if i < len(frame) - 1 else '') for i, row in enumerate(frame))
        lines.append('    ]%s' % (',' if index < len(creature['frames']) - 1 else ''))
    lines.append('  ],')
    lines.append('  "steps": [')
    for index, step in enumerate(creature['steps']):
        lines.append('    %s%s' % (json.dumps(step), ',' if index < len(creature['steps']) - 1 else ''))
    lines.append('  ]')
    lines.append('}')
    with open(path, 'w', encoding='utf-8', newline='\n') as file:
        file.write('\n'.join(lines) + '\n')


def bounding_box(frame):
    xs = [column for line in frame for column, cell in enumerate(line) if cell != '.']
    ys = [row for row, line in enumerate(frame) if line.strip('.')]
    if not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def luminance(color):
    def channel(c):
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * channel(color[0]) + 0.7152 * channel(color[1]) + 0.0722 * channel(color[2])


def contrast(one, two):
    a, b = luminance(one), luminance(two)
    return (max(a, b) + 0.05) / (min(a, b) + 0.05)


def lint(creature):
    errors = []
    warnings = []
    frames = creature.get('frames') or []
    steps = creature.get('steps') or []
    if not frames:
        errors.append('no frames')
    if len(frames) > 8:
        errors.append('more than 8 frames')
    second = parse_color(creature.get('second'))
    third = parse_color(creature.get('third'))
    body = parse_color(creature.get('body'))
    if body is None:
        errors.append('no body color')

    for index, frame in enumerate(frames):
        if len(frame) != ROWS:
            errors.append('frame %d has %d rows, not %d' % (index, len(frame), ROWS))
        for row, line in enumerate(frame):
            if len(line) != COLUMNS:
                errors.append('frame %d row %d has %d cells, not %d: %r' % (index, row, len(line), COLUMNS, line))
            unknown = set(line) - CELLS
            if unknown:
                errors.append('frame %d row %d has unknown cells %s' % (index, row, sorted(unknown)))
            if second is None and set(line) & set('adle'):
                errors.append('frame %d row %d uses second-color cells but "second" is empty' % (index, row))
            if third is None and set(line) & set('cC'):
                errors.append('frame %d row %d uses third-color cells but "third" is empty' % (index, row))
    if errors:
        return errors, warnings

    if not steps:
        errors.append('no steps')
    total = 0
    for index, step in enumerate(steps):
        if len(step) != 5:
            errors.append('step %d must be [frame, lift, shift, blink, ticks]' % index)
            continue
        frame, lift, shift, blink, ticks = step
        if not 0 <= frame < len(frames):
            errors.append('step %d names frame %d' % (index, frame))
        if not 0 <= lift <= 4:
            errors.append('step %d: lift must be 0 to 4' % index)
        if not -2 <= shift <= 2:
            errors.append('step %d: shift must be -2 to 2' % index)
        if ticks < 1:
            errors.append('step %d: ticks must be at least 1' % index)
        total += ticks
    sleep = creature.get('sleep', 0)
    if not 0 <= sleep < len(frames):
        errors.append('sleep names frame %d' % sleep)
    if errors:
        return errors, warnings

    used = {step[0] for step in steps} | {sleep}
    for index in range(len(frames)):
        if index not in used:
            warnings.append('frame %d is never used' % index)
    seconds = total * TICK_SECONDS
    if seconds < 2.0:
        warnings.append('the loop is only %.1f s long: it will look hectic' % seconds)
    if seconds > 14.0:
        warnings.append('the loop is %.1f s long: its action will rarely be seen' % seconds)

    has_eyes = any(set(line) & set('*e') for frame in frames for line in frame)
    if has_eyes and not any(step[3] for step in steps):
        warnings.append('it has eyes but never blinks')
    if not has_eyes and not creature.get('objectLike'):
        warnings.append('no eye cells (* or e) in any frame: set "objectLike": true if that is on purpose')

    floats = creature.get('floats')
    grounds = []
    for index, frame in enumerate(frames):
        box = bounding_box(frame)
        if box is None:
            errors.append('frame %d is empty' % index)
            continue
        left, top, right, bottom = box
        grounds.append(bottom)
        cells = sum(1 for line in frame for cell in line if cell != '.')
        if cells < COLUMNS * ROWS * 0.2:
            warnings.append('frame %d has only %d cells: it will look small next to the others' % (index, cells))
        width = right - left + 1
        height = bottom - top + 1
        if max(width, height) < min(COLUMNS, ROWS) - 2:
            warnings.append('frame %d is only %dx%d: use more of the %dx%d grid' % (index, width, height, COLUMNS, ROWS))
        centre = (left + right) / 2.0
        if abs(centre - (COLUMNS - 1) / 2.0) > 2.0:
            warnings.append('frame %d sits off centre by %.1f cells' % (index, centre - (COLUMNS - 1) / 2.0))
        for row, line in enumerate(frame):
            for column, cell in enumerate(line):
                if cell == '.':
                    continue
                neighbours = 0
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        y, x = row + dy, column + dx
                        if 0 <= y < ROWS and 0 <= x < COLUMNS and frame[y][x] != '.':
                            neighbours += 1
                if neighbours == 0 and cell not in 'ywr':
                    warnings.append('frame %d: lone cell %r at row %d column %d' % (index, cell, row, column))
        for row, line in enumerate(frame):
            for column, cell in enumerate(line):
                if cell not in '*e':
                    continue
                around = [frame[y][x] for y in (row - 1, row, row + 1) for x in (column - 1, column, column + 1)
                          if 0 <= y < ROWS and 0 <= x < COLUMNS and (x != column or y != row)]
                bodyish = sum(1 for c in around if c in '#o+')
                secondish = sum(1 for c in around if c in 'adl')
                if cell == '*' and secondish > bodyish:
                    warnings.append("frame %d row %d column %d: eye '*' sits on the second color: use 'e' so it shuts in the right color" % (index, row, column))
                if cell == 'e' and bodyish > secondish:
                    warnings.append("frame %d row %d column %d: eye 'e' sits on the body: use '*' so it shuts in the right color" % (index, row, column))
    if grounds and not floats:
        if len(set(grounds)) > 1:
            warnings.append('the lowest cell moves between frames %s: feet should stay on the ground, or set "floats": true' % grounds)
        if max(grounds) != ROWS - 1:
            warnings.append('it stands on row %d, not on the ground row %d: or set "floats": true' % (max(grounds), ROWS - 1))

    seen = {}
    for index, frame in enumerate(frames):
        fingerprint = '\n'.join(frame)
        if fingerprint in seen:
            warnings.append('frames %d and %d are identical' % (seen[fingerprint], index))
        seen[fingerprint] = index

    for index in range(len(steps)):
        a, b = frames[steps[index][0]], frames[steps[(index + 1) % len(steps)][0]]
        if a is b:
            continue
        changed = sum(1 for row in range(ROWS) for column in range(COLUMNS) if a[row][column] != b[row][column])
        drawn = max(1, sum(1 for line in a for cell in line if cell != '.'))
        if changed > max(16, drawn * 0.5):
            warnings.append('step %d to %d changes %d cells of %d: it may pop instead of move' % (index, (index + 1) % len(steps), changed, drawn))

    for name, color in (('body', body), ('second', second), ('third', third)):
        if color is None:
            continue
        used_here = {'body': '#o+', 'second': 'adl', 'third': 'cC'}[name]
        if not any(set(line) & set(used_here) for frame in frames for line in frame):
            if name != 'body':
                warnings.append('%s color is set but never used' % name)
            continue
        for surface, surface_name in ((CARD, 'the card'), (RUNNING_ROW, 'a running row')):
            ratio = contrast(color, surface)
            if ratio < 2.2:
                warnings.append('%s color has low contrast on %s (%.1f:1): it will vanish' % (name, surface_name, ratio))
    return errors, warnings


def report(key, creature=None):
    creature = creature or load(key)
    errors, warnings = lint(creature)
    for line in errors:
        print('ERROR', key + ':', line)
    for line in warnings:
        print('WARN ', key + ':', line)
    if not errors and not warnings:
        print('OK   ', key)
    return errors, warnings


def sheet(key):
    creature = load(key)
    errors, _ = lint(creature)
    frames = creature['frames']
    steps = creature['steps']
    colors = creature_palette(creature)
    blink_colors = creature_palette(creature, True)
    big = 10
    gap = 12
    columns = min(6, max(1, len(frames)))
    width = max(820, columns * (COLUMNS * big + gap) + gap)
    frame_rows = (len(frames) + columns - 1) // columns
    small = 4
    per_line = max(1, (width - gap) // (COLUMNS * small + 8))
    strip_rows = (len(steps) + per_line - 1) // per_line
    height = 26 + frame_rows * (ROWS * big + 22) + 16 + strip_rows * (ROWS * small + 30) + 22 + ROWS * 8 + 40
    canvas = Canvas(width, height, CARD)

    y = 6
    text(canvas, gap, y, 'FRAMES OF %s' % key, MUTED)
    y += 14
    for index, frame in enumerate(frames):
        row = index // columns
        x = gap + (index % columns) * (COLUMNS * big + gap)
        top = y + row * (ROWS * big + 22)
        label = 'F%d' % index + (' SLEEP' if index == creature.get('sleep', 0) else '')
        text(canvas, x, top, label, MUTED)
        draw_grid(canvas, x, top + 10, big)
        draw_frame(canvas, x, top + 10, frame, big, colors)
    y += frame_rows * (ROWS * big + 22) + 4

    total = sum(step[4] for step in steps)
    text(canvas, gap, y, 'THE LOOP %d STEPS %.1f S: FRAME LIFT SHIFT TICKS' % (len(steps), total * TICK_SECONDS), MUTED)
    y += 12
    for index, step in enumerate(steps):
        frame, lift, shift, blink, ticks = step
        x = gap + (index % per_line) * (COLUMNS * small + 8)
        top = y + (index // per_line) * (ROWS * small + 30)
        canvas.rect(x - 2, top + 6, COLUMNS * small + 4, ROWS * small + 10, SIDEBAR)
        draw_frame(canvas, x + shift * small, top + 14 - lift * small, frames[frame], small, blink_colors if blink else colors)
        text(canvas, x, top + 8 + ROWS * small + 10, 'F%d %d' % (frame, ticks), MUTED)
    y += strip_rows * (ROWS * small + 30) + 8

    x = gap
    text(canvas, x, y, 'ASLEEP', MUTED)
    draw_frame(canvas, x, y + 10, frames[creature.get('sleep', 0)], 8, grey_palette())
    x += COLUMNS * 8 + 18
    text(canvas, x, y, 'LOCKED', MUTED)
    draw_shadow(canvas, x, y + 10, frames[0], 8, CARD)
    x += COLUMNS * 8 + 18
    text(canvas, x, y, 'SHINY', MUTED)
    draw_frame(canvas, x, y + 10, frames[0], 8, shiny_palette())
    x += COLUMNS * 8 + 18
    text(canvas, x, y, 'IN A LIST', MUTED)
    canvas.rect(x, y + 10, 70, 40, CARD)
    draw_frame(canvas, x + 8, y + 16, frames[0], 2, colors)
    canvas.rect(x + 74, y + 10, 70, 40, RUNNING_ROW)
    draw_frame(canvas, x + 82, y + 16, frames[0], 2, colors)
    text(canvas, x, y + 56, 'ON A RUNNING ROW RIGHT', MUTED)
    x += 160
    text(canvas, x, y, 'ON ITS PAGE', MUTED)
    canvas.rect(x, y + 10, COLUMNS * 3 + 8, ROWS * 3 + 8, SIDEBAR)
    draw_frame(canvas, x + 4, y + 14, frames[0], 3, colors)

    path = os.path.join(OUT, key + '.png')
    canvas.save(path)
    print('sheet', path)
    report(key, creature)
    return errors


def blind(creature, code):
    frames = creature['frames']
    colors = creature_palette(creature)
    order = []
    for step in creature['steps']:
        if step[0] not in order:
            order.append(step[0])
    order = order[:4]
    pixel = 12
    gap = 16
    canvas = Canvas(gap + len(order) * (COLUMNS * pixel + gap), ROWS * pixel + 2 * gap, CARD)
    for index, frame in enumerate(order):
        draw_frame(canvas, gap + index * (COLUMNS * pixel + gap), gap, frames[frame], pixel, colors)
    canvas.save(os.path.join(BLIND, code + '-large.png'))

    pixel = 4
    canvas = Canvas(COLUMNS * pixel + 40, ROWS * pixel + 40, CARD)
    draw_frame(canvas, 20, 20, frames[0], pixel, colors)
    canvas.save(os.path.join(BLIND, code + '-small.png'))


def publish(key, version, code):
    creature = load(key)
    errors, warnings = report(key, creature)
    if errors:
        print('NOT PUBLISHED: fix the errors first')
        return 1
    save(creature, os.path.join(VERSIONS, key, 'r%s.json' % version))
    sheet_errors = sheet(key)
    blind(creature, code)
    print('published', key, 'round', version, 'as', code, 'warnings', len(warnings), 'sheet errors', len(sheet_errors))
    return 0


def contact(path, scale=4, frame_index=0, keys=None, mode='normal', labels=True, per_row=8):
    keys = keys or ORDER
    cell_w = COLUMNS * scale + 18
    cell_h = ROWS * scale + (26 if labels else 12)
    rows = (len(keys) + per_row - 1) // per_row
    canvas = Canvas(per_row * cell_w + 8, rows * cell_h + 8, CARD)
    for index, key in enumerate(keys):
        creature = load(key)
        frame = creature['frames'][min(frame_index, len(creature['frames']) - 1)]
        x = 8 + (index % per_row) * cell_w
        y = 6 + (index // per_row) * cell_h
        if mode == 'sleep':
            draw_frame(canvas, x, y, creature['frames'][creature.get('sleep', 0)], scale, grey_palette())
        elif mode == 'shadow':
            draw_shadow(canvas, x, y, frame, scale, CARD)
        else:
            draw_frame(canvas, x, y, frame, scale, creature_palette(creature))
        if labels:
            text(canvas, x, y + ROWS * scale + 4, key[:max(1, cell_w // 6 - 1)], MUTED)
    canvas.save(path)
    print('contact', path, canvas.width, canvas.height)


def camel(key_id):
    return 'k' + key_id


def color_code(value):
    if value == 'accent':
        return 'ManifoldTheme::Accent'
    if value in (None, ''):
        return 'kNoColor'
    return 'Rgb(0x%s)' % value.lstrip('#').upper()


def header(path):
    groups = [
        ('From the start', ['slime', 'ghost', 'cat', 'robot']),
        ('Hours idled', ['frog', 'whale', 'bunny', 'mushroom', 'bat', 'penguin', 'leetcat', 'dragon', 'ufo', 'superslime']),
        ('Different games', ['snail', 'turtle', 'octopus', 'jellyfish']),
        ('One game for long', ['hourglass', 'mimic']),
        ('Days in a row', ['hamster', 'phoenix']),
        ('What the queue does', ['centipede', 'trophy', 'sheep', 'owl']),
        ('Certain days', ['pumpkin', 'snowman', 'partyblob', 'blackcat', 'fish', 'cake']),
        ('Secrets', ['bug', 'manifold', 'heartbuddy', 'star']),
        ('Games of their own', ['spaceship', 'cube', 'hat', 'chicken', 'crab', 'kingslime', 'cryingkid', 'bean', 'nightcam', 'zombie', 'wyvern', 'dwarf', 'bonfire', 'error', 'rocketcar', 'cheese']),
        ('Found later', ['duck', 'pufferfish', 'otter', 'shark', 'bee', 'firefly', 'loveletter', 'parrot', 'capybara', 'sloth', 'cactus', 'coffee', 'ghillie', 'mole', 'parkakid', 'demon']),
    ]
    assert [key for _, keys in groups for key in keys] == ORDER
    lines = []
    add = lines.append
    add('#pragma once')
    add('')
    add('#include "Core/Creatures.h"')
    add('#include "UI/Theme.h"')
    add('')
    add('#include "imgui.h"')
    add('')
    add('#include <array>')
    add('#include <cstddef>')
    add('#include <cstdint>')
    add('')
    add('namespace Idler::UI::CreatureSprites')
    add('{')
    add('    inline constexpr int kColumns = %d;' % COLUMNS)
    add('    inline constexpr int kRows = %d;' % ROWS)
    add('')
    add('    using Frame = std::array<const char*, kRows>;')
    add('')
    add('    struct Step')
    add('    {')
    add('        int FrameIndex;')
    add('        int Lift;')
    add('        int Shift;')
    add('        bool Blink;')
    add('        int Ticks;')
    add('    };')
    add('')
    add('    struct Sprite')
    add('    {')
    add('        CreatureId Id;')
    add('        const Frame* Frames;')
    add('        int FrameCount;')
    add('        const Step* Steps;')
    add('        int StepCount;')
    add('        int SleepFrame;')
    add('        ImVec4 Body;')
    add('        ImVec4 Second;')
    add('        ImVec4 Third;')
    add('        int Facing;')
    add('        bool Floats;')
    add('        bool Swims;')
    add('        bool Stays;')
    add('    };')
    add('')
    add('    constexpr ImVec4 Rgb(std::uint32_t rgb)')
    add('    {')
    add('        return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f, static_cast<float>(rgb & 0xFF) / 255.0f, 1.0f);')
    add('    }')
    add('')
    add('    inline constexpr ImVec4 kNoColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);')
    add('')
    table = []
    for _, keys in groups:
        for key in keys:
            creature = load(key)
            name = camel(creature['id'])
            add('    inline constexpr Frame %s[] = {' % name)
            for frame in creature['frames']:
                for index, row in enumerate(frame):
                    prefix = '        { ' if index == 0 else '          '
                    suffix = ' },' if index == len(frame) - 1 else ','
                    add('%s"%s"%s' % (prefix, row, suffix))
            add('    };')
            add('    inline constexpr Step %sSteps[] = {' % name)
            row = []
            for frame, lift, shift, blink, ticks in creature['steps']:
                row.append('{ %d, %d, %d, %s, %d },' % (frame, lift, shift, 'true' if blink else 'false', ticks))
                if len(row) == 7:
                    add('        ' + ' '.join(row))
                    row = []
            if row:
                add('        ' + ' '.join(row))
            add('    };')
            add('')
            table.append('        Make(CreatureId::%s, %s, %sSteps, %d, %s, %s, %s, %d, %s, %s, %s),' % (creature['id'], name, name, creature.get('sleep', 0),
                                                                                         color_code(creature.get('body')), color_code(creature.get('second')), color_code(creature.get('third')),
                                                                                         creature.get('facing', 0), 'true' if creature.get('floats') else 'false', 'true' if creature.get('swims') else 'false', 'true' if creature.get('stays') else 'false'))
    evolved_table = []
    evolved = evolved_keys()
    for key in evolved:
        creature = load(key)
        first = load(key.split('/', 1)[1])
        assert creature['id'] == first['id'], key
        for name in ('facing', 'floats', 'swims', 'stays'):
            assert creature.get(name, 0) == first.get(name, 0), (key, name)
        name = camel(creature['id']) + 'Evolved'
        add('    inline constexpr Frame %s[] = {' % name)
        for frame in creature['frames']:
            for index, row in enumerate(frame):
                prefix = '        { ' if index == 0 else '          '
                suffix = ' },' if index == len(frame) - 1 else ','
                add('%s"%s"%s' % (prefix, row, suffix))
        add('    };')
        add('    inline constexpr Step %sSteps[] = {' % name)
        row = []
        for frame, lift, shift, blink, ticks in creature['steps']:
            row.append('{ %d, %d, %d, %s, %d },' % (frame, lift, shift, 'true' if blink else 'false', ticks))
            if len(row) == 7:
                add('        ' + ' '.join(row))
                row = []
        if row:
            add('        ' + ' '.join(row))
        add('    };')
        add('')
        evolved_table.append('        Make(CreatureId::%s, %s, %sSteps, %d, %s, %s, %s, %d, %s, %s, %s),' % (creature['id'], name, name, creature.get('sleep', 0),
                                                                                              color_code(creature.get('body')), color_code(creature.get('second')), color_code(creature.get('third')),
                                                                                              creature.get('facing', 0), 'true' if creature.get('floats') else 'false', 'true' if creature.get('swims') else 'false', 'true' if creature.get('stays') else 'false'))
    add('    template <std::size_t Frames, std::size_t Steps>')
    add('    constexpr Sprite Make(CreatureId id, const Frame (&frames)[Frames], const Step (&steps)[Steps], int sleepFrame, ImVec4 body, ImVec4 second, ImVec4 third, int facing, bool floats, bool swims, bool stays)')
    add('    {')
    add('        return { id, frames, static_cast<int>(Frames), steps, static_cast<int>(Steps), sleepFrame, body, second, third, facing, floats, swims, stays };')
    add('    }')
    add('')
    add('    inline constexpr Sprite kSprites[] = {')
    lines.extend(table)
    add('    };')
    add('')
    add('    inline constexpr Sprite kEvolvedSprites[] = {')
    lines.extend(evolved_table)
    add('    };')
    add('')
    add('    constexpr bool IsCell(char cell)')
    add('    {')
    add("        return cell == '.' || cell == '#' || cell == 'o' || cell == '+' || cell == 'a' || cell == 'd' || cell == 'l' || cell == 'c' || cell == 'C' ||")
    add("            cell == '*' || cell == 'e' || cell == 'r' || cell == 'y' || cell == 'w' || cell == 'k';")
    add('    }')
    add('')
    add('    constexpr bool SpriteIsValid(const Sprite& sprite)')
    add('    {')
    add('        if (sprite.SleepFrame >= sprite.FrameCount)')
    add('            return false;')
    add('        for (int step = 0; step < sprite.StepCount; ++step)')
    add('        {')
    add('            if (sprite.Steps[step].FrameIndex >= sprite.FrameCount)')
    add('                return false;')
    add('        }')
    add('        for (int frame = 0; frame < sprite.FrameCount; ++frame)')
    add('        {')
    add('            for (const char* line : sprite.Frames[frame])')
    add('            {')
    add('                int length = 0;')
    add("                for (; line[length] != '\\0'; ++length)")
    add('                {')
    add('                    if (!IsCell(line[length]))')
    add('                        return false;')
    add('                }')
    add('                if (length != kColumns)')
    add('                    return false;')
    add('            }')
    add('        }')
    add('        return true;')
    add('    }')
    add('')
    add('    constexpr bool SpritesAreValid()')
    add('    {')
    add('        for (std::size_t index = 0; index < std::size(kSprites); ++index)')
    add('        {')
    add('            if (static_cast<std::size_t>(kSprites[index].Id) != index || !SpriteIsValid(kSprites[index]))')
    add('                return false;')
    add('        }')
    add('        for (std::size_t index = 0; index < std::size(kEvolvedSprites); ++index)')
    add('        {')
    add('            const Sprite& sprite = kEvolvedSprites[index];')
    add('            const Sprite& first = kSprites[static_cast<std::size_t>(sprite.Id)];')
    add('            if (!SpriteIsValid(sprite) || (index > 0 && kEvolvedSprites[index - 1].Id >= sprite.Id) || sprite.Facing != first.Facing || sprite.Floats != first.Floats ||')
    add('                sprite.Swims != first.Swims || sprite.Stays != first.Stays)')
    add('                return false;')
    add('        }')
    add('        return true;')
    add('    }')
    add('')
    add('    static_assert(std::size(kSprites) == kCreatureCount);')
    add('    static_assert(SpritesAreValid());')
    add('}')
    with open(path, 'w', encoding='utf-8', newline='\r\n') as file:
        file.write('\n'.join(lines) + '\n')
    print('header', path, len(lines), 'lines')


def option(args, name, default=None):
    if name in args:
        index = args.index(name)
        return args[index + 1]
    return default


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 0
    command = args[0]
    if command == 'sheet':
        return 1 if sheet(args[1]) else 0
    elif command == 'lint':
        keys = args[1:] or ORDER + evolved_keys()
        problems = 0
        for key in keys:
            errors, warnings = report(key)
            problems += len(errors)
        return 1 if problems else 0
    elif command == 'publish':
        return publish(args[1], args[2], args[3])
    elif command == 'restore':
        with open(os.path.join(VERSIONS, args[1], 'r%s.json' % args[2]), encoding='utf-8') as file:
            save(json.load(file))
        print('restored', args[1], 'round', args[2])
    elif command == 'contact':
        keys = option(args, '--keys')
        contact(args[1], int(option(args, '--scale', 4)), int(option(args, '--frame', 0)), keys.split(',') if keys else None,
                'sleep' if '--sleep' in args else 'shadow' if '--shadow' in args else 'normal', '--nolabel' not in args, int(option(args, '--columns', 8)))
    elif command == 'header':
        header(args[1])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
