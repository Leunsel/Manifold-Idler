# Creature style guide

Every game that idles in Manifold Idler (a desktop app, dark UI) is shown by a tiny animated pixel creature, the way a
Tamagotchi or a game character idles. They are seen at **2 screen pixels per cell** in list rows, on a
dark card (#15161A) or on a greenish running row (#1A342D), at 3 per cell in the collection and at 8 per cell on a detail stage.
They must read at the smallest size first. Light always comes from the top left.

## The sprite

A creature is one JSON file on a grid of **16 columns by 14 rows**: every frame has exactly 14 rows of exactly 16 cells.

```
{"key": "frog", "id": "Frog", "body": "#5CC756", "second": "#D1E58F", "third": null,
 "floats": false, "objectLike": false, "sleep": 2,
 "frames": [[row, row, ...], [row, row, ...]],
 "steps": [[frame, lift, shift, blink, ticks], ...]}
```

Cells: `.` nothing | `#` body, `o` body in shade, `+` body in light | `a` second, `d` second in shade, `l` second in light |
`c` third, `C` third in shade | `*` eye on the body, `e` eye on the second color | `r` red, `y` yellow, `w` white, `k` dark grey.
Shade and light are derived from the color you give: shade is 62 % of it, light is 60 % towards white.

`floats`: true for what hovers or flies (it need not stand on the bottom row). `swims`: true for what lives in the terrarium's lake. `objectLike`: true for what has no eyes on purpose.
`sleep`: the index of the frame it sleeps in. `steps`: the loop; a tick is 1/8 s. `lift` raises the whole picture 0 to 4 pixels, `shift`
moves it -2 to 2 pixels sideways, `blink` true closes `*` and `e` in the shade of their surface for that step. Do not change `key` or `id`.

## What makes a good one

1. **Silhouette first.** Locked creatures show only the shadow of frame 0. Someone must be able to guess it from the outline.
2. **Use the grid.** The main mass is nearly the full width or height and sits centred. It stands on the bottom row unless it floats.
   Small or thin creatures look lost next to the others.
3. **No noise.** Every cell earns its place. No stray cells, no checker patterns, no specks. Curves step evenly (run lengths that
   grow or shrink steadily, like 1-1-2-3), no lumps or jags in an outline.
4. **One ramp per material.** A material uses only its own tones: `# o +` or `a d l` or `c C`. A brown shell with dots in the green
   body's shade is exactly the error to avoid: use the shell's own shade. Two or three tones per material, shade on the lower right,
   highlights rare and top left.
5. **Eyes make it alive.** Dark `*` on the body or `e` on the second color, clearly placed, not on the outer edge where they look like a
   notch. A dark body needs bright eyes (`w`, `y`), which do not blink: show closed eyes with a frame instead.
6. **Colors.** Saturated, mid to bright, with at least 2.2:1 contrast against both backgrounds. No near-black bodies. `r y w k` are for
   small accents. Every creature looks clearly different from the others at a glance.
7. **Cute.** Big head, short limbs, a simple face, a friendly round shape, also for the spooky ones. Think Kirby, not anatomy.
8. **Evoke, never copy.** No logos, no letters, no copied art of a game. A hint of the idea is enough.
9. **No dark outlines around everything.** The background is dark already. Shape comes from the fill and its shade tones, with `k` only
   where a dark part belongs, like a hat, a lens or a pupil.

## How it moves

3 or 4 frames; a fifth only when the action truly needs it. Frame 0 is its hero picture, the most recognisable pose.

- It breathes or sways gently, with few cells changing. The body stays where it is; feet stay planted. When it jumps, use `lift`.
- It has one **signature action** that says what it is: a frog croaks, a crab snips its claws, an owl turns its eyes, a whale spouts, a
  flame flickers. Long rest, short action. The loop is 3 to 10 seconds.
- Creatures with eyes blink: a step of 1 tick with `blink` true, now and then, not on a beat.
- Nothing pops: going from one step to the next changes only what moves. Check the strip in the sheet from left to right.

The frame it sleeps in is drawn in greys, body mid grey and second color light grey, with its eyes shut. Give it a settled pose of its own
when that suits it: head down, curled up, flame low. Check `ASLEEP` on the sheet.

## An evolved form

A creature can have one evolved form, in `creatures/evolved/<key>.json` with the same `id`; its name and line are in `kEvolutions` in
Core/Creatures.h, and the build stops when one of the two is missing. It is the same creature, grown: keep its body color, its face and
its way of moving, and add one or two things that change the silhouette, such as a flower, a lantern, a second tail or shoulder plates.
`facing`, `floats`, `swims` and `stays` stay those of the first form. The new name keeps the first one in it (Bloom Slime, Robot Mk II).
Check both side by side: `contact <out.png> --keys slime,evolved/slime`.

## Working on a creature

A creature is `creatures/<key>.json`, its evolved form `creatures/evolved/<key>.json`. The forge checks and renders them the way
the program draws them, with nothing but Python's standard library:

    py Tools/CreatureForge/forge.py sheet <key>                          out/<key>.png and the lint report; evolved/<key> for an evolved form
    py Tools/CreatureForge/forge.py lint                                 the lint report of every creature
    py Tools/CreatureForge/forge.py contact out/all.png --keys a,b       creatures side by side; without --keys all of them
    py Tools/CreatureForge/forge.py header Source/UI/CreatureSprites.h   the header the program is built with

Look at the sheet after each change, at the big frames and at the small `IN A LIST` pictures. Fix every lint warning unless it
clearly does not apply. Rows are text, so count cells carefully: the lint names rows of the wrong length. Draw, look, correct;
a creature that reads well at the smallest size is done.

To compare drafts, `publish <key> <round> <code>` keeps a copy in `versions/<key>/r<round>.json` and pictures of it under a
neutral code in `blind/`; `restore <key> <round>` brings a kept copy back.
