# The redesign, measured

Every number here was read off the computed styles of the user's own
mockup (`abcTrain Redesign.dc.html`, board **1a — тёмная тема**), not
estimated from a screenshot. It is the specification the four editors are
being brought onto; when the code and this file disagree, this file is
right and the code is behind.

The mockup's own palette turned out to be **this project's palette** —
`#1e1e2e`, `#5b9bd5`, `#4fa3c7`, `#c77f4f`, `#5fb98c`, `#a878c9` and the
rest are already in `shared/AbcTrainTheme.cpp` to the digit. That is
because the design was drawn over screenshots of the real app. So the
colours were never the gap. The gap is the **grammar**: square corners,
hairline frames, registration marks, tracked capitals, segmented bars,
and a nav bar across the top instead of a rail down the side.

## Frame

Board is **1182 × 880**. Window `#1e1e2e`, 1px border `#3a3a4a`.
**Every corner radius in the entire mockup is 0.**

## Nav bar — 59px tall

| | |
|---|---|
| Brand cell | x21, w127, h58, 1px divider on its right |
| Mark | 26 × 26 |
| Wordmark | "abcTrain" 20px/700 Condensed, tracking 0.8, `#f2f2f7` |
| Tabs | start x172, h33; label 15px Condensed uppercase, tracking 1.5 |
| — active | fill `#5b9bd5`, text `#1e1e2e`, weight 600 |
| — inactive | no fill, text `#a0a0b0`, weight 500 |
| Streak | "СЕРИЯ 2 ДН." 14px/600 Condensed tracking 1.68 `#d98c5f` |
| Streak dots | 5 × 5, pitch 8; lit `#d98c5f`, unlit `#3a3a4a` |
| Language | own cell, 1px divider on its left; "RU" 14px/600 tracking 1.4 |

## Focus band — 120px tall, `#242434`

Split by a 1px divider at x802.

**Left.** Kicker "ТВОЙ ФОКУС · СЕГОДНЯ" 13px/600 Condensed uppercase,
tracking **2.6**, `#a0a0b0`. Headline 30px/600 Condensed `#f2f2f7`.
Reward "+50 очков за задание" 18px/600 Condensed `#d98c5f`, right of the
headline on the same line. Progress: **five segments 34 × 4, pitch 39**,
done `#5b9bd5`, left `#3a3a4a`, then "3 из 5" 13px SemiCondensed.

**Right.** "ОБЩИЙ УРОВЕНЬ" 12px/400 SemiCondensed uppercase tracking
1.68; the number 26px/700 Condensed. Primary button **190 × 44**, fill
`#5b9bd5`, label `#1e1e2e` 16px/600 Condensed uppercase tracking **1.92**,
with four registration marks.

## Registration marks

The one ornament the system has, and it is load-bearing rather than
decorative: it is what says "this is a frame, not a filled box". Four
`+` glyphs at the inner corners, inset 3px, drawn in the frame's own
colour — on the primary button in the label colour at 50%.

## Section header — 26px tall

A **9 × 9 filled square** in the family colour, then the family name
15px/600 Condensed uppercase tracking **2.7** `#e0e0e0`, then the English
name 12px/400 SemiCondensed uppercase tracking 1.68 `#a0a0b0`, then the
count right-aligned, 12px SemiCondensed `#a0a0b0`.

## Exercise card — 276 × 148

Quiet: 1px `#3a3a4a`, no fill, no marks. Current: 1px in the family
colour, fill the family colour at **6%**, four marks. Padding 15 / 14.

| Row | Offset | Content |
|---|---|---|
| icon + level | +14, h32 | icon 22 × 22; right: "УРОВЕНЬ" micro caps over the number |
| name | +55, h42 | 19px/600 Condensed `#f2f2f7`, English name beneath |
| meta | +105, h16 | 13px/400 SemiCondensed `#a0a0b0`, or `#d98c5f` when it is today's streak |
| progress | +133, h3 | **ten segments 23 × 3, pitch 25**, done in the family colour, left `#32323f` |

## What this changes about the current build

- The **left rail goes**. It was added in the previous pass and the
  mockup answers the same question with a nav bar, which gives the
  content the full width back.
- Progress bars become **segmented**. A continuous bar says "somewhere
  between"; ten segments say "seven of ten", which is the sentence the
  number under it is already saying.
- Every card gains a **quiet state**. Today every card is a filled panel,
  so nine of them shout equally; in the mockup only the one you are on
  is filled, and the rest are outlines.
