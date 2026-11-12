# Web page themes

The web page of `idler serve --web` takes its look from a theme. The page brings a base kit: the layout, the controls and a
plain dark look, all of it built on CSS variables. A theme sets those variables and may add rules of its own, animations
included. Two themes are built in:

- **Arcade**: pixel type and neon over a moving synthwave grid, with four screens (Synthwave, Classic, Phosphor, Amber). It is
  the default.
- **Manifold**: the window program's look, calm and dark, with a slow glow behind it.

The four squares beside Start switch from one look to the next; Settings > Look lists them all with their options. Each
browser remembers its own choice.

## Where themes live

| | |
|---|---|
| Built in | Inside the program. Their sources lie in `CLI/Web/Themes`; Arcade's fonts come from `Dependencies/Fonts`. |
| Your own | `Manifold-Idler.themes` beside the settings file, one folder per theme. `--themes <folder>` on `idler serve` and `idler themes` picks another folder. |

A theme of your own with the name of a built-in one takes its place, and files it lacks still come from the built-in one: a
folder `arcade` with only a `theme.css` keeps Arcade's fonts. The page reads the list of themes each time it loads and asks for
the theme's files anew, so a reload shows what changed.

```
idler themes                          the themes, and what is wrong with those of your own
idler themes copy arcade my-arcade    a copy of a theme, fonts included, to change in the themes folder
```

A copy is the easiest start: it works as it is, and its `theme.ini` already carries the new name.

## A theme's folder

```
my-arcade/
  theme.css        needed
  theme.ini        its name, a note, screens and options
  Pixel.woff2      fonts and pictures that theme.css uses
```

The folder's name is the theme's name: `a` to `z`, `0` to `9` and `-`, up to 32 characters, not starting with `-`.
The page gets these files from the folder: `.css`, `.woff2`, `.woff`, `.ttf`, `.otf`, `.png`, `.jpg`, `.jpeg`, `.gif`,
`.webp` and `.svg`, up to four folders deep and 8 MB each. A file name holds letters, digits, `.`, `_` and `-`, and does not
start with a dot. Nothing else in the folder leaves the computer, `theme.ini` included.

## theme.ini

```ini
[theme]
name=Arcade
note=Pixel type and neon, like an arcade screen.

[variant synthwave]
name=Synthwave
swatch=#22e6ff #ff3fd2 #ffe95c #62ffb0

[variant classic]
name=Classic
swatch=#f2f2f2 #ffd84d #6fd6ff #ff7aa8

[option scanlines]
name=Scanlines
note=Fine lines over the page, like an old monitor.
default=off
```

Arcade's own `theme.ini` has the variants; the option here is an example of one.

| Section | Key | |
|---|---|---|
| `[theme]` | `name` | Up to 48 bytes. Without it, the folder's name. |
| | `note` | One line under the name in Settings > Look, up to 160 bytes. |
| | `swatch` | Up to six colours, `#rgb`, `#rgba`, `#rrggbb` or `#rrggbbaa`, for the look button. Without it, the first variant's. |
| `[variant <name>]` | `name`, `swatch` | A screen of the theme, up to 12. The first one is where the theme starts. |
| `[option <name>]` | `name`, `note`, `default` | A switch in Settings > Look, up to eight. `default` is `on` or `off`. |

Lines starting with `;` or `#` are left alone, and so is a byte order mark. A line the idler cannot use is skipped and
reported, with the code MI-7020, in Settings > Look and by `idler themes`; the rest of the theme still counts. Variant and
option names follow the same rule as folder names.

## What a theme can hook into

The page marks its state on `<html>`:

| Attribute | |
|---|---|
| `data-theme="arcade"` | The theme. |
| `data-variant="phosphor"` | The variant, when the theme has some. Select it with `:root[data-variant=phosphor]`. |
| `data-options="scanlines"` | The options that are on. Select one with `:root[data-options~=scanlines]`. |
| `data-active="yes"` | The queue runs (`no` while it is stopped). |

And these places are there for motion:

| Selector | |
|---|---|
| `.page` | A page is shown by removing `hidden`, so an animation on its children runs again on every page change. |
| `.chart[data-fresh]`, `#stats-overview[data-fresh]` | New to the eye: a page or tab was opened, a game picked. The redraw every 30 seconds has no `data-fresh`, so `[data-fresh] .tile-stat` runs once. |
| `--i` | The place of a bar in `.bar-col span`, of a cover in `.tile` and of a game in `.entry`, for animations that start one after another. |
| `.entry.running`, `.entry.failed`, `.entry.finished` | A game in the queue. A click on it opens the game panel. |
| `.entry-actions` | A game's buttons in the queue. With a mouse they show while the pointer is over the game or a button in it has the focus; on touch screens they are gone, and the game panel holds them. |
| `.art img` | Every cover: `.thumb`, `.tile-art` and `.detail-art` are its frame, the picture inside can move or zoom without leaving it. |
| `.drawer`, `.scrim` | The game panel from the right and the shade behind it; both show by losing `hidden`. |
| `.detail-queue`, `.detail-queue.running` | The queue part of the game panel. |
| `.podium-item`, `.game-link`, `.opens` | Games on Statistics that open the game panel. |
| `.pill.good`, `.pill.warn`, `.pill.bad`, `.pill.muted` | The state beside Start. |
| `#toggle.primary` | Start, while the queue is stopped. |
| `.page[data-page=library]` | Each page sets `--tone` to its own colour; also `idling`, `statistics`, `log` and `settings`. |
| `.nav a[aria-current=page]`, `.tabs [aria-selected=true]`, `.tile[aria-pressed=true]`, `.switch[aria-checked=true]` | What is chosen. |
| `.toast.show`, `.ask[open]`, `.detail` | The message at the bottom, the question box, a game's details in the Library. |
| `.account-name`, `.account-state` | The Steam name at the bottom of the side bar and whether it is logged on. |

## The variables

Each one has a value in the base kit; a theme sets those it wants on `:root`.

| Colours | |
|---|---|
| `--bg`, `--bg-image` | The page's colour, and pictures or gradients over it (`none` in the base kit). |
| `--side`, `--top` | The side bar and the bar at the top. |
| `--panel`, `--sunken`, `--raised`, `--raised-hover` | Cards; fields and list rows; buttons, and buttons under the pointer. |
| `--fg`, `--soft`, `--dim` | Text: main, softer, quiet. |
| `--line`, `--line-strong` | Borders. |
| `--accent`, `--accent-hover`, `--on-accent` | The main colour, under the pointer, and text on it. |
| `--good`, `--warn`, `--bad`, `--link` | Idling, waiting, failed, links. |
| `--tone-idling`, `--tone-library`, `--tone-statistics`, `--tone-log`, `--tone-settings` | The colour of each page. |
| `--selection`, `--on-selection`, `--focus`, `--focus-style`, `--backdrop` | Selected text, the keyboard focus ring (`solid`, `dashed`, ...) and the shade behind a question box. |

| Type | |
|---|---|
| `--font-text`, `--font-label`, `--font-heading`, `--font-figure`, `--font-mono` | Text; labels and buttons; headings; big numbers; the log. |
| `--text-size`, `--small-size`, `--big-size`, `--input-size`, `--mono-size` | Sizes of text, small notes, names, fields and the log. |
| `--label-size`, `--label-case`, `--label-spacing` | Labels and buttons: size, `text-transform` (`capitalize` in the base kit, so each word starts upper case), `letter-spacing`. |
| `--button-size`, `--button-small`, `--nav-size` | Buttons, small buttons, the side bar. |
| `--heading-size`, `--heading-weight`, `--heading-case`, `--heading-spacing`, `--title-size` | Card headings and the page title. |
| `--figure-size`, `--figure-weight`, `--smoothing` | Big numbers; `-webkit-font-smoothing`. |

| Shapes and motion | |
|---|---|
| `--border` | The width of every border. |
| `--radius`, `--radius-control`, `--radius-pill`, `--radius-dot` | Cards; buttons and fields; the state pill and switches; dots. |
| `--control`, `--control-small` | The height of buttons and fields. |
| `--shadow`, `--shadow-small`, `--shadow-pop` | Cards, the figures on Statistics, the message and the question box. |
| `--gap` | The space between cards. |
| `--ease`, `--fast`, `--slow`, `--press` | Timing function and two durations for transitions; the `transform` of a pressed button. |

A theme may bring variables of its own too; Arcade's `--glow`, `--hl-a` and `--hl-b` are such.

## Rules of the road

- **CSS only.** The page runs no code but its own. It loads styles, fonts and pictures from the idler and from `data:` URLs,
  nothing from other servers: no web fonts from elsewhere, no `@import` of a foreign file. `url(Pixel.woff2)` in
  `theme.css` means the file beside it.
- **Narrow screens.** The base kit's rules for narrow screens (up to 1100 and 760 pixels wide) come before the theme. A theme
  rule that changes the same thing wins on narrow screens too, unless the theme repeats it in a `@media` block of its own.
  Manifold does that for the bar at the bottom.
- **Fixed parts.** A game's details (up to 1100 pixels), the bar at the bottom (up to 760 pixels) and the message are fixed to
  the window. A `transform`, `filter` or `backdrop-filter` on one of their parents ties them to that parent instead, and a
  `clip-path` cuts them off. Animate `.page > :not(.detail)` rather than `.page`, and give an entrance animation
  `animation-fill-mode: backwards` so nothing of it stays once it has run.
- **Less motion.** When the system asks for less motion, the base kit turns animations and transitions off; a theme need
  not do anything for it.
- **Behind the page.** `body::before` and `body::after` are free for a theme's backdrop, as Arcade's grid and Manifold's glow
  are. Give the backdrop `position: fixed`, `pointer-events: none` and `z-index: 0`, and `.shell` `position: relative` with
  `z-index: 1`, so the page stays above it.

The smallest theme is one line, a `theme.css` that changes the main colour:

```css
:root{--accent:#ff8a3d;--accent-hover:#ffa463;--good:#ff8a3d}
```
