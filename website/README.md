# abcTrain — landing page

React + Vite. The page for the four plugins: what they train, what they
do, what they don't do yet, and where to download them.

```bash
npm install
npm run dev            # http://localhost:5173
npm run build          # -> dist/
node tools/inline-build.mjs   # -> one self-contained HTML for previewing
```

## The design system, and where it comes from

**Nothing here is invented.** Every colour resolves through
[`src/styles/tokens.css`](src/styles/tokens.css), and every value in that
file is inherited from the product's own design system,
`shared/AbcTrainTheme.cpp`. A site whose blue is not the plugin's blue
reads as two different companies, and the plugin is the thing being sold.

If a colour changes in `AbcTrainTheme.cpp`, change it here too.

### Colour

The four **skill families** are the spine of the whole product — nine
exercises grouped by what they train, each with its own hue in
`AbcTrainTheme::accentFor (Family)`:

| Family | Dark | Light | Exercises |
|---|---|---|---|
| Frequency | `#4fa3c7` | `#35708a` | 2 |
| Dynamics | `#c77f4f` | `#8a5636` | 2 |
| Space & stereo | `#5fb98c` | `#3f8060` | 4 |
| Character | `#a878c9` | `#74528b` | 1 |

Semantic colour (`--good`, `--bad`, `--warm`) is deliberately separate
from the family hues: "you got it right" must never be confusable with
"this is a space exercise".

The page ground is `#0f0f14` — the plugin's own *display well*, one step
deeper than its window background. That divergence is the one deliberate
one: a plugin lives inside a DAW and has to sit politely among other
windows, while a page owns the whole screen and can be as dark as the
instrument's own screen.

The **light theme is not an inversion.** It is `AbcTrainTheme`'s designed
light palette — warm off-white paper, surfaces stepping toward white,
accents darkened and slightly desaturated, because a hue that reads as
clearly blue on near-black washes out to a pastel on paper (ADR 019).
Both themes are defined at token level, three times over: under
`prefers-color-scheme` for the OS preference, and under
`:root[data-theme=…]` for the explicit toggle, which must win over the
media query in both directions.

### Type

**Source Code Pro** (SIL OFL 1.1, embedded — no CDN, so it can never
silently fall back) carries display and data; the platform UI sans carries
running text.

Mono as a *display* face is the deliberate risk. An instrument labels
itself in mono — that is the silkscreen on the front of every piece of
audio hardware — and it keeps the site speaking in the same voice as the
plugin's own numeric readouts. Light 300 at display sizes with `0.015em`
of tracking; Medium 500 for labels and frequencies; uppercase labels get
`0.16em`, because uppercase without tracking is a wall.

Running text is capped at `62ch`. Digits that line up in columns use
`font-variant-numeric: tabular-nums`.

### Layout

The page is a **rack**: each section is a module separated by a hairline
top edge, which is the shape of the equipment this software is about and
gives sections an honest boundary without a decorative divider.

Structure is meaning, not decoration — the four families are a real
grouping from the code, so they get to be the page's spine. There is no
`01 / 02 / 03` numbering anywhere, because nothing on this page is a
sequence.

## The hero is one playable round

[`src/components/PlayableRound.jsx`](src/components/PlayableRound.jsx) is
a working round of *Guess the Band* in the browser: pink noise (Paul
Kellet's economy filter — literally the same algorithm as
`shared/PinkNoiseGenerator.h`) through a peaking filter at a frequency
drawn log-uniformly across 100 Hz – 12.8 kHz, answered on a log axis,
graded with a tolerance measured in **octaves** so the slack is the same
ratio at 200 Hz as at 8 kHz.

The fastest honest way to explain an ear trainer is to let someone fail at
it once. A screenshot of a spectrum explains nothing that a hundred other
audio pages have not already failed to explain.

**Nothing makes a sound until the visitor presses play.** Audio on page
load is the rudest thing a site can do, and browsers block it anyway.

## Honesty

[`src/content.js`](src/content.js) holds every factual claim on the page,
in one file, so it can be audited against
[`docs/website-brief.md`](../docs/website-brief.md)'s "what you must not
claim" section. The page carries a **Known limits** block stating that the
builds are unsigned, that the importer does not separate stems, that
stereo width still trains on pink noise, and that updating is not fully
automatic.

Putting those on the page is the point. A visitor finds all of it out in
the first ten minutes anyway, and an instrument that tells you what it
cannot do yet is one you can believe about what it can.

No user counts, no star counts, no download counts, and no comparison
claims against FabFilter, iZotope or SoundGym: the product is pre-release
and every one of those numbers would be invented.
