# 035 — The staircase, thresholds in units, two layers of achievements

**Status:** accepted
**Date:** 2026-09-19

The mechanics review (published as the "Механика abcTrain" artifact) found
seven progress counters and none of them answering the one question a
sound engineer has: *how precisely can I hear this now?* The user approved
all five proposals from it. This records what changed and why.

## Levels are a staircase

**Before:** +10 points per correct answer (+0..5 for precision); level L→L+1
cost 100·L points; once the points were there, a five-in-a-row test took
the level; a wrong answer cost nothing. Level 10 needed ~360 correct answers
per exercise — about an hour — and after that nothing moved again. The
user's own record: 8,195 rounds of "Guess the Band", level 10, i.e. ~7,800
rounds in which the app learnt nothing new about him.

**Now:** three correct in a row → one step harder; one wrong → one step
easier. This is the 3-down/1-up transformed staircase (Levitt, JASA 1971),
the standard psychoacoustic method for measuring a discrimination
threshold. It settles where the listener is right ~79% of the time, which
sits next to the ~85% that Wilson et al. (2019) found optimal for learning
and is where a round feels on the edge. `ProgressManagerTest` simulates a
listener and checks that it does settle there.

Because the staircase moves both ways, each exercise keeps two numbers:
**level** (where it is now — today's form) and **bestLevel** (the record,
never drops). Screens lead with the record, so a wrong answer is never
shown as a loss: the accept band simply widens on the scale. Only a new
record is announced.

Migration: a points-era save has `level` and no `bestLevel`; that level
becomes both the starting step and the record. Nobody who upgrades finds an
exercise reset to 1. Points, the promotion test and their settings keys are
gone.

## A level is shown as what it means

`Game::describeLevel (level)` reports the level in the exercise's own
terms, and the editor formats it:

| exercise | unit | level 1 → level 10 |
|---|---|---|
| Guess the Band | ± octaves | 1.0 → 0.2 |
| Pan | ± % of one side | 35 → 7 |
| Gain change | ± dB | 2.5 → 0.8 |
| Delay | ± % of the time | 35 → 8 |
| the five two-alternative ones | the closest pair that level can offer | e.g. Room / Plate → Room / Chamber |

For the rulers the tolerance comes from one `toleranceForLevel` per game,
used by both `setDifficulty` and `describeLevel`, so the two cannot
disagree. For the pairs, `PresetFamily::hardestPairForLevel` reads the same
ranking and window `drawPair` draws from.

On top of the unit, a **rank** in five rungs of two steps each —
*Hears the difference · Steady ear · Working ear · Mixing ear · Mastering
ear*. Every name says what the player *can* do; there is no rung called
"beginner". The home screen draws the next rung as a dashed line on each
exercise's ruler, so the distance to the next name is a length you can see.

"Overall level" (the maximum across nine exercises, which read as a total)
is gone. The daily task now picks the exercise with the **lowest record**,
and says so, instead of a random one worth "+50 points".

## Home is a list of thresholds

Nine identical cards saying "LEVEL 1" became nine rows: star, icon, name
(and accuracy/rounds in words), a ten-segment ruler (fill = record, white
tick = today, dashed = next rung), and the threshold on the right in the
mono face like a meter reading. The achievement strip left the home screen;
achievements have one page.

## Achievements: 12 milestones and 53 stamps

24 achievements were too many for each to be an event and too few to be
earned often, and **9 of the 24 asked for lifetime accuracy** — a ratio
that gets harder to move the more you play (60% over 8,195 rounds needed
~4,900 correct in a row to reach 75%). They were traps.

Now two layers with different jobs:

- **Milestones (12)** — step 8 in each exercise, step 5 everywhere, step 9
  everywhere, 30 days in a row. Each has a medal with its own drawing of
  the subject (`AchievementsScreenComponent::drawArt`: a filter bell, a
  compressor's knee, an impulse response, a pan pot, echoes, a clipped
  sine, a stereo fan, two levels a decibel apart, a spectrum, the four
  family colours, a calendar). Described in units: "threshold ±0.35 oct in
  Guess the Band".
- **Stamps (53)** — rounds (50, 300), ten in a row and step 4 per exercise;
  totals, day streaks, Survival/Blitz scores, all nine touched. Small
  squares, many, earned often.

No rule asks for lifetime accuracy any more, and a test says so. Ids are
generated from the rule (`st.reverb.rounds.300`, `ms.eq.level.8`), so they
are stable and readable in a settings file.

## Removed from the bar: the volume slider

In a plugin the level is the channel fader; in the standalone it is the
interface and the system. A third volume did nothing useful and moved the
reference the gain exercise is judged against. Output is fixed at unity;
a level saved by the old slider is ignored rather than left stuck with no
control to undo it.

## A bug found on the way

`translateChoiceLabel` looked labels up as `choice.weak`, `choice.narrow`…
and `describeChoice` looks descriptions up as `"choice." + lower-cased
name` — the same keys. The description won. In every non-English build the
compression and stereo-width panels printed a whole sentence at 52px where
"Weak" or "Narrow" belonged. Labels now use `choiceLabel.*`, in all twelve
languages. Found by rendering the new home screen, which shows the closest
pair's labels.

## Type

Every step of the type ladder went up ~15-20% (micro 10→12, label 12→14,
body 13→15, heading 15→17, title 20→23, display 30→36). JUCE sizes by
ascent+descent, so the old micro drew 7px capitals.

The gain-reduction meter in LearnerComp keeps its "GR ↓" caption at 11px:
it is a fixed-size instrument, and at the new caption size the label ran
into the ring.

## Two more found by rendering

- The lit tab in the bar was forced back to "Exercises" on every refresh,
  so the achievements and settings pages sat under a bar that said you
  were somewhere else. It now follows the page that is showing.
- The licence text in Settings kept the dark theme's near-white after
  switching to light (a TextEditor colours text when it is inserted) and
  was unreadable. It is recoloured on every theme change.
