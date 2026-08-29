# 032 — The map of where the misses land

**Status:** accepted
**Date:** 2026-08-30

## The problem with a results screen

A run ends and the screen says: 11 points, 92% accuracy, best streak 3,
personal best 11. Every one of those numbers is about a run that is now
over. None of them answers the only question a player actually has, which
is *what do I do next time*.

The row that used to sit at the bottom — where the four skill families
stand — was closer, but it answers at the wrong resolution. "Frequency:
level 4" tells you which exercise to open. It does not tell you anything
you could not have worked out from the home screen, and inside an
exercise it is silent.

## What was built

Every exercise now divides its own subject into named parts and reports
which part each answered round belonged to. `ProgressManager` counts
attempts and misses per part, per exercise, and persists both. The run
results screen draws that as a row of buckets with the worst one picked
out, plus one sentence naming it.

The parts are the exercise's own vocabulary, not a generic scale:

| Exercise | Buckets |
|---|---|
| Find the Frequency, Name the Range | the seven named ranges (Sub-bass … Air) |
| Guess the Reverb | the five reverb types |
| Guess the Distortion | the four distortion types |
| Guess the Stereo Width | the four named widths |
| Guess the Compression | weak / medium / strong |
| Guess the Pan | five fifths of the field |
| Guess the Delay | slapback / short / medium / long |
| Guess the Gain | big cut / small cut / small boost / big boost |

Two of those are worth naming. **`EQGame` answers on a continuous axis
but reports in named ranges** — it borrows `FrequencyRangeGame`'s one
table via `rangeIndexFor()` rather than carrying a second copy that could
drift, because "you keep missing in the low-mids" is a sentence an
engineer can act on and "you keep missing around 0.38 of the axis" is
not. And **the continuous games bucket by where the *answer* was**, not
by how far the guess landed: the question is which part of the range you
cannot hear, not how badly you missed it.

## Three rules the map has to obey

**An untouched bucket is drawn empty, and that is different from a
perfect one.** Both are "no misses"; only one of them is evidence. This
is why the drawn bar has a floor of 6px rather than 2 — at 22% of a 30px
trough a real bucket drew a two-pixel line indistinguishable from an
empty one, which is the single confusion this widget must not create.

**The sentence says nothing when there is nothing to say.** A bucket
needs at least three attempts before it can be called anyone's weak spot,
and with no bucket clearing that floor there is no sentence at all. Three
rounds is not a diagnosis, and a confident wrong one sends a player to
practise the thing they were already good at.

**It is still only the player's own numbers.** No percentile, no
comparison, no cohort — for the same reason those were refused before:
there is no server, and inventing one is inventing the data.

## Why it is on the `Game` interface

`getNumSkillBuckets` / `getSkillBucketLabel` / `getSkillBucketForRound`
are non-pure-virtual with inert defaults, the same shape as
`setReferenceAudioLibrary` and the continuous-answer hooks (ADR 020). A
game that does not divide its subject returns 0 and everything downstream
skips it; `ProgressManager::registerAnswer` takes the bucket as a
defaulted trailing argument, so no existing call site changed.

Storage is a fixed `std::array<BucketStats, 8>` per exercise rather than
a vector, because it is per-user state living in a `PropertiesFile` and a
cap that a test can assert against is better than a growth path nobody
will check. `tests/SkillBucketTest` asserts every exercise fits under it,
at every level.

## What the tests actually check

`tests/SkillBucketTest.cpp` drives all nine exercises through all ten
levels, twelve rounds each, and asserts every answered round lands in a
bucket that exists — an out-of-range index would not crash, it would be
silently dropped by `registerAnswer`'s guard and the map would just be
quietly wrong. It was verified by breaking it: making one game report one
past its own end fails the test with the game's name in the message.

The rest is the counting contract (attempts and misses accumulate
separately, an untouched bucket stays at zero, nothing leaks between
exercises), the guard (an impossible bucket records nothing but the round
itself still counts — dropping the round would make the accuracy on the
same screen disagree with the map beside it), persistence across a
reload, and `rangeIndexFor` at both ends of its table.

## What was not built

No per-bucket history over time — the map is lifetime totals, so a range
you were bad at in March still weighs on it. A decay or a rolling window
would be more honest about *current* skill, and is the obvious next step
if the map turns out to be read often.

No way to start a run restricted to your worst bucket. It is the natural
follow-on and was deliberately left out of this pass: the map has to
prove it is read before it earns a mechanic hanging off it.
