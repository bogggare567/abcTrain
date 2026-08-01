# 031 — Every categorical exercise offers two, and each answer is a family

**Status:** accepted
**Date:** 2026-08-01

## The two problems, which turn out to be one

A player asked for two things in the same breath: that the answer always
be a choice between two, and that a category like "Hall" stop being one
setting with a small random nudge on it — that it become a *family* of
settings.

Those sound unrelated. They are the same change seen from either end.

**The old difficulty lever was the number of buttons.** `ReverbGame` was
the only game that moved it — two choices at levels 1–3, three at 4–6,
four at 7–10 — and that was as far as the idea could go. Adding a button
does make the round harder, but not by asking more of the ear: it asks
you to read more, and it lowers the odds of a lucky guess. A player who
cannot tell a plate from a spring is not helped by also being offered a
room.

**The old variety lever was a jitter.** Each category was one preset with
a few percent of random wobble on it. Over a few dozen rounds that
teaches recognition of a recording rather than of a category. `EQGame`
already learned this once, when its eight fixed octave centres turned out
to be memorisable as *positions* — you could win without ever learning
what 2 kHz sounds like. A category you can only ever hear one example of
is a sample, not a category.

Both are answered by the same move: **difficulty is distance.**

## The decision

Every categorical exercise — reverb, compression, distortion, stereo
width, named frequency range — offers exactly **two** alternatives,
forever, at every level. What the level chooses is *which* two, and how
archetypal an example of its category the correct one is.

`shared/PresetFamily.h` holds both halves and nothing else:

- `drawPair (positions, level, random)` — each game writes down where its
  categories sit on one axis of *character*: how bright a space is, how
  hard a clipper bites, where a band lives in the spectrum. A level
  admits pairs no further apart than a ceiling and no closer than a
  floor, both sliding down together. Level 1 offers the two extremes;
  level 10 offers neighbours. The floor matters as much as the ceiling —
  without it a hard tier still hands out the giveaway pair now and then,
  which reads as the difficulty being broken rather than as variety.
- `choose (family, level, random)` — every member of a family carries how
  *archetypal* it is: 1 is the textbook example, 0 is the one sitting
  right against the neighbouring category. A level sees a **window** from
  the top of that ordering, widening as the level rises. A window, not a
  shift: the archetypes never stop appearing, because a hard tier made
  only of edge cases stops teaching the category and starts teaching the
  edge.

Those are drawn **independently**. Conflating them would make a hard pair
always arrive with a hard example, which is twice as hard as intended and
impossible to reason about from either end.

## What a family means per exercise

It is not the same thing in each, and that is the point.

- **Reverb** — several genuinely different spaces per type. A tiled booth
  and a big live room are both rooms, and someone who can only recognise
  one of them has not learned what a room sounds like. The least
  archetypal room is one large enough to nearly be a chamber.
- **Compression** — threshold/ratio pairs that converge on the
  neighbouring setting.
- **Distortion** — four voicings per shaper, varying drive, post-shaping
  rolloff and knee asymmetry. The borderline tape is bright enough to
  nearly be a soft clip; the borderline overdrive is symmetric enough to
  nearly be one too.
- **Stereo width** — width is a single number, so a family of settings
  would just *be* the neighbouring category. What varies instead is how
  the width is *arrived at*: how much of the low end stays centred. Two
  mixes at the same nominal width sound meaningfully different when one
  keeps everything under 150 Hz in the middle, and the width you notice
  is mostly the width above the bass.
- **Named frequency range** — the family is a continuum, so the *breadth
  of the draw* is what moves: an easy round boosts the middle of Bass,
  a hard one boosts the boundary with Low-mids. Filter Q is the second
  axis — a broad lift is what a range sounds like, a narrow one is a
  single tone that happens to live there.

## Loudness stopped being allowed to answer the question

`DistortionGame` carried one hand-tuned makeup gain per type, "tuned by
ear, not measured" — near enough while each type had exactly one voicing
at one drive. A family that varies the drive varies the loudness, and
then the round is winnable by hearing which one is louder, which is the
one thing all nine exercises exist to prevent.

So the compensation is now **measured**: a fixed, seeded signal is run
through that exact voicing on the message thread in `newRound()`, and the
result is scaled to a fixed RMS. Same seed every time, so a voicing always
gets the same compensation and nothing here drifts. `tests/DistortionGameTest`
re-measures the compensated output and asserts it lands within 0.005 of
the target across every type, every voicing and three drive amounts.

## What this broke, and what caught it

`PluginEditor::refreshFromGameState()` rebuilt the answer panel only when
the *number* of choices changed. That was true enough when a game's labels
were a fixed table and only `ReverbGame`'s length ever moved. With every
categorical game now offering exactly two, redrawn from its family each
round, the count is permanently 2 — so the panel kept the previous
round's two names while the verdict came from the current one. After the
first round the names on screen would never have changed again.

Nothing in the test suite could see it: the games were right, the widget
was right, and the wiring between them was wrong. It was caught by
`tools/EditorSnapshots` rendering an answered zoned round, which read
*"Not quite. It was Chamber reverb"* above two zones labelled Hall and
Room. The comparison is now against the labels themselves.

That is the fourth entry in this project's ledger of bugs that compiled,
passed every test, and were obvious within ten seconds of looking at a
picture.

## The answer panel lost its tick marks

A tick is a slider's way of saying "the value is *here* on a continuum".
Two named alternatives are not a continuum, and the line down the middle
of each zone — inherited from the ruler this widget started as — only
ever pointed at its own label. Removed, along with the two-row label
stagger built for eight or nine choices and the `< first · last >` axis
caption that now repeated the only two words on screen.

The alternating zone shading went too, at two choices only: one lighter
half beside one darker half reads as *the left one is already selected*,
which is a lie the moment the panel opens.

What is left is what was asked for — two halves, one hairline, the name
of each written large and centred in the region you click.

## Consequences

- **Difficulty is now comparable across exercises.** "Level 7" means the
  same kind of thing everywhere: the two things on offer are close
  together and the example is not the textbook one.
- **The odds of a lucky guess are fixed at 50%** and no longer improve
  as a player gets better, which is the opposite of what the old
  count-based scheme did. Streaks and Survival runs mean the same thing
  at every level now.
- **`ReverbGame` no longer changes its choice count at runtime**, which
  was the sole reason `refreshFromGameState()` had a count comparison in
  it — see above for what that cost.
- Per-exercise stats stay comparable across the change: nothing about
  scoring, points or `ProgressManager` moved.

## What was deliberately not done

- **No third or fourth alternative at any level, ever.** The request was
  explicit, and the reasoning above says why it is also right.
- **No family for the four continuous exercises** (EQ, pan, dB, delay).
  Their answer is a value on a ruler, so their variety is already total
  and their difficulty is already a tolerance band — see
  [020](020-continuous-answers.md). Applying `PresetFamily` there would
  be a second, redundant difficulty axis.
- **No re-tuning of `CompressionGame`'s makeup gains by measurement**, the
  way `DistortionGame`'s were. Its presets change threshold and ratio
  rather than drive, and the existing compensation is per-preset rather
  than per-type, so the loudness tell it is exposed to is much smaller.
  Worth doing when that game gets its own pass; not worth changing tuned
  audio behaviour as a side effect of this one.
