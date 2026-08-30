# 033 — The redesign, made real

**Status:** accepted
**Date:** 2026-08-30

The user built a full redesign of the app in Claude Design and asked for
it back in the product **точь в точь** — exactly, with nothing invented.
This records what that turned out to mean, what was measured rather than
guessed, and the four real bugs the work uncovered.

## The colours were never the gap

The first thing worth knowing: the mockup's palette is **this project's
palette, to the digit**. `#1e1e2e`, `#5b9bd5`, `#4fa3c7`, `#c77f4f`,
`#5fb98c`, `#a878c9`, `#a0a0b0`, `#3a3a4a` were already in
`AbcTrainTheme.cpp` before any of this started, because the design was
drawn over screenshots of the real app.

So the distance between the two was never colour. It was **grammar**:

| | before | after |
|---|---|---|
| corners | 4 / 7 / 10px radii | **0 everywhere** |
| a card | a filled panel | a drawn frame; only the current one fills |
| a button | a gradient slab | a hairline frame; only a chosen one fills |
| labels | sentence case | tracked capitals |
| progress | a smooth bar | segments you can count |
| navigation | a rail down the left | a bar across the top |
| ornament | none | registration marks at frame corners |
| typeface | whatever the machine had | Barlow, in the binary |

Every number in it came off the mockup's own computed styles, not off a
screenshot — `docs/design/redesign-spec.md` is that measurement, and it is
the file the code answers to.

## The window is 1180 × 880

Not a round number somebody liked: it is what the content needs once
navigation runs across the top instead of down the side. Four exercise
cards fit a row, the training screen's five control groups fit one line,
and the scale gets the full width — which matters more there than
anywhere else in the app, because on a ruler *width is resolution*.

## Two typefaces, because Barlow has no Cyrillic

The mockup asks for Barlow. Barlow ships Latin, Latin-ext and Vietnamese.
In a browser the missing Cyrillic falls back invisibly; in a
Russian-first application it does not.

Roboto was tried first and was wrong, and the render is what said so —
"EQ ВЫКЛ" read as two different fonts on one button. Measuring found the
mismatch was **width**, not weight: Roboto at its narrowest is 27% wider
than Barlow SemiCondensed and cannot approach Barlow Condensed at all.

So `tools/build_fonts.py` now *searches*. For each Barlow cut it measures
the width-to-cap-height ratio of H/O/N and the ink coverage of the same
three letters (a good proxy for stem weight), and finds the closest point
on a candidate's variable axes. That returned **two** companions, which
is the consistent answer rather than an inconsistent one — Barlow and
Barlow Condensed are already two different designs:

- Noto Sans (`wdth 80` / `70` / `62.5`) for body and meta,
- Oswald (`wght 500` / `600`) for the condensed headings, where Noto
  cannot get narrow enough and Oswald lands within 3% on width.

Vertical metrics are then forced onto Barlow's (1 em ascent, 0.2 em
descent, 0.7 em cap). Not tidiness: JUCE scales a typeface by its own
ascent + descent, so two families that disagree draw at different sizes
for one requested height.

Twelve faces, each subset to the one script it carries, ~650 KB.

## Four real bugs

**1. `toUpperCase()` does not uppercase Cyrillic.** It goes through the C
library's `towupper()`, which is locale-dependent, and a plugin inherits
the "C" locale where every non-ASCII letter comes back unchanged. Every
"УРОВЕНЬ", "ЧАСТОТЫ" and "ДОСТИЖЕНИЯ" in the app had been quietly drawing
as tracked sentence case — invisible in English, wrong in the other
eleven languages we ship. Confirmed with a standalone `towupper` probe
before fixing; `AbcTrainLookAndFeel::toCaps` now maps the scripts the
embedded fonts actually carry.

**2. A label decided by identity instead of contrast.** The new button
code asked "is this background colour the panel colour?" to decide
whether a button was filled. A caller passing *that same colour at 25%
alpha* slipped through as filled, so Learner Comp's preset chips drew a
near-black label on a near-black chip and vanished. Contrast cannot be
fooled that way, and both the fill test and the label colour now use it.

**3. `{{days}}` on screen.** The nav bar substituted the streak
placeholder when the *caption* was set — which happens once, on a
language change — while the number arrives separately and daily. It drew
a literal `{{DAYS}}` for the whole session.

**4. A unicode range parsed as a string.** My own first font script split
`"U+0100-024F"` on `-` and took `[2:]` of each half, turning it into
`range(0x0100, 0x4F)` — empty. Latin Extended-A was silently absent, so
Polish and Czech would have had no letters. Caught by rewriting the
ranges as pairs of numbers and watching every file grow by 20 KB.

Bugs 1–3 were all found by **rendering**, not by reading; `EditorSnapshots`
has now earned its place four separate times.

## What changed that the mockup did not literally ask for

- **Volume stays in the bar.** The mockup dropped it. It is the one
  control somebody reaches for mid-round, and losing it would mean
  leaving the exercise to turn the sound down.
- **The tabs stay on the training screen.** The mockup replaces them with
  a back-link. Keeping them means Sounds is one click away from a round
  rather than two.
- **The disc strip of achievements stays.** The mockup shows one earned
  badge and a link; the strip shows how far off the unearned ones are,
  which is the argument ADR 024 made for that screen and still holds.

## What the mockup asks for and is not built yet

- **A confirm button on the answer** ("ОТВЕТИТЬ"). The app answers on
  release of the drag. Adding a commit step is an interaction change
  rather than a visual one, and it belongs in its own decision.
- **Option descriptions in ten more languages.** English and Russian are
  written; the rest fall back to English, which for a person who reads
  English plugin manuals is more useful than blank.
- **The instruction band always visible.** It still collapses once an
  exercise is familiar, which was a deliberate earlier decision (see the
  exercise section in `PluginEditor::resized`).
- Learner-plugin screens beyond the shared button and typography work —
  the user said their redesign for those is coming separately.
