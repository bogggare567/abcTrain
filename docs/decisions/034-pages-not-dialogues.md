# 034 — Pages, not dialogues; and the space a screen does not need

**Status:** accepted
**Date:** 2026-09-18

ADR 033 brought the grammar of the user's mockup into the code — square
corners, hairline frames, tracked capitals, segmented bars, a bar across
the top. This pass went looking at the result with the renders rather than
at the diff, and found that the *grammar* had arrived while three
structural things had not.

The brief for this pass was explicit about the standard: not a literal
fitting to the mockup, but a check on legibility, priority and whether a
person reading the screen gets what the screen is for. Where the two
disagree, this file says which won and why.

## Settings, Achievements and Training sounds were dialogues

All three were full-window overlays drawing a centred card, with a drop
shadow and a "Close" button in the corner. That is the shape of something
you must dismiss to get back to the app. They are reached by clicking a
**tab**, and a tab is a place you go.

They are now pages: sized to `contentBounds()`, filling everything under
the navigation bar, no shadow, no outline, no Close. Boards D1/D2/E1 of
the mockup say the same thing.

Three things followed from that, and only the first was expected:

- **The tabs did not close each other.** Nothing ever set any of the three
  invisible except its own Close button. While each covered the whole
  window this could not be noticed — the bar was underneath it. As pages,
  opening Achievements over Settings would have stacked them, and pressing
  "Trainings" would have run the home screen behind a page still covering
  it. `topNav.onItemChosen` now shows exactly one, and `showScreen` closes
  all three.
- **The content underneath stayed live.** `tools/ClickMap` caught it: with
  Settings open, the home screen's "Continue" and both footer links were
  invisible, full-size and still answering clicks aimed at the page.
  `hideContentUnderNavPage()` is the fix. A control you cannot see but can
  still press is worse than a broken one, because nothing on screen
  explains what just happened.
- **The snapshots were of a layout no player sees.** `EditorSnapshots`
  runs against a fresh settings file, so the editor opens on the welcome
  screen — where the bar is deliberately hidden. Every one of those three
  seams now goes to Home first.

**Settings also opens on Appearance rather than About.** Nobody walks into
Settings to read a licence; About stays first in the rail, which is where
a person looks for it when they do want it.

## The answer section was taking every spare pixel

The training screen's answer section was floored, not capped, on the
reasoning that "a taller scale is a more precise scale". The render
disagrees on both counts:

- A **ruler is horizontal**. Width is its resolution; height past a point
  is empty well. At 880px it drew a black rectangle 500px tall containing
  nothing but grid lines.
- Two **named panels** 600px tall carried a name and one sentence in their
  top third. ADR 033 removed an earlier 190px cap on exactly the right
  grounds — two small cards adrift in a large space read as unfinished —
  and then overshot.

Capped at 420 (ruler) and 380 (panels), top-anchored, which are the
mockup's own proportions. What is left over stays plain background: the
same call the hint section already makes, since an empty stretch of
background reads as space where a drawn frame around nothing reads as a
broken element.

**The scale now draws at rest.** The tolerance band and the value were
shown only once the cursor was engaged, so before the first drag the
largest object on the screen said nothing — and the one thing on screen
that shows what a level actually changes was invisible until you no longer
needed it.

## One sentence, once

"Drag along the scale to answer" appeared three times on one screen: in
the exercise line, in the answer band's heading, and as placeholder text
inside the widget it describes. The two copies inside the answer section
are gone, and the heading's second half now carries information instead of
a repeat — "two options, never a third" for the categorical exercises,
"drag along the scale" for the rulers.

Same disease on the home screen, in English only: `card.englishName` and
the family's English name were drawn unconditionally under their
translated versions, so every card and every heading in the English build
said its own name twice. The second name is for the eleven languages that
are not English, and is now shown only there.

## An untouched exercise said nothing

`statsLine` was empty until a round had been played, so on a fresh install
eight of nine cards were an outlined box with a name in the corner and
90px of nothing under it. It now says "not started yet" in all twelve
languages, and each family heading carries its exercise count, as the
mockup does.

## Where the mockup did not win

- **The run results keep one large number and three small ones.** The
  mockup's C5 is a four-column band of equal statistics. Equal size makes
  a table, and a table is what you read when looking something up — not
  what belongs at the end of ninety seconds of concentrating. The score is
  what just happened; accuracy, streak and personal best are how to read
  it. That hierarchy stays.
- **No confirm button on the answer**, for the reason ADR 033 already
  gave: it is an interaction change, not a visual one.
- **The Training settings page stays reverted** (`a83f901`). `CLAUDE.md`
  had gone on describing it for three weeks; that drift is fixed here.

## What is still open

- The welcome screen puts "Support the project" and "Star on GitHub" in
  front of a first-time user, one screen before they have heard anything.
  Moving both to the later showing is a behaviour change and belongs in
  its own decision.
- The Learner plugins are untouched: the user's redesign for them is
  coming separately, and bringing them onto this grammar is easier once
  the trainer has settled.
