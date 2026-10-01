# 049 — Battles by damage: how far off, not right or wrong

Date: 2026-10-01. Status: accepted (Bogdan, after a design talk with GPT).

## Problem

A battle was seven rounds of right/wrong. On a ruler exercise a miss by a
hair and a miss by four octaves counted the same, and bots (ADR 046) could
only flip a weighted coin: they were right as often as their profile said,
but never "close". Neither says anything about how well someone hears.

## Decision

- **One error scale for every exercise.** `Game::answerErrorRelative()` is
  the miss divided by the round's tolerance: 0 dead on, 1 at the edge of
  "right". The rulers already work on perceptual axes (log frequency for the
  band, log time for delay, dB, pan position), so the ratio means the same
  in all of them. `answerErrorNative()` gives the miss in the exercise's own
  units for display: octaves, dB, ms, %. A named pair (compression, reverb,
  distortion, width, range) has no distance: right 0, wrong 2. No answer: 3.
- **HP and damage.** Ten rounds, 100 HP each. Each side takes
  `damageFor(error) = 20 · (1 − exp(−max(0, e − 0.7)² / 0.8))`: nothing
  under 0.7, ≈2 at the edge of "right", ≈18 at twice the tolerance, ≈20 for
  no answer. Five wild misses end a battle; ten near misses do not. The
  winner is who has more HP left, not who won more rounds.
  `SessionManager::damageFor` and `damageFor` in the site's
  `server/lib/abctrainBattles.js` are the same curve; both test suites check
  the same three numbers.
- **Bots answer with an error.** `PerceptualModel` is the interface;
  `RuleBasedPerceptualModel` turns the bot's trained psychometric curve into
  Gaussian scatter: σ is chosen so that P(|e| ≤ 1) = the bot's chance of
  being right for that exercise, part and level
  (σ = 1 / (√2 · erf⁻¹ p)). So the Cat misses a top-octave band by
  0.7 tolerances on average and the Viper by 3; at the bottom it is the other
  way round (`tests/BattleTest`). A lapse lands 1.5–4 tolerances off. No new
  numbers: the same weights as ADR 046.
- **Online battles use the same rules.** The server already received the
  chosen and right values; it now computes the errors, damage and HP, ends a
  battle on a knock-out, and passes the HP outcome to `recordBattle`, which
  uses it for Elo instead of the round count.

## Later

`PerceptronPerceptualModel`: the same interface, trained on real players'
answers (exercise, level, target, error, reaction time, recent form) once
there are enough of them. Nothing in the battle changes when it arrives.

## Honest limits

On the five named-pair exercises damage is still right or wrong: there is
no distance to measure. A smoother signal there would need confidence or
reaction time, and reaction time rewards fast tapping over careful
listening, so it is not used.
