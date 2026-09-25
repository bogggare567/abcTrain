# 048 — Battles with people: the server sets the round, the apps draw it

Date: 2026-09-25. Status: accepted (Bogdan chose between the two shapes below).

## Problem

The Decibelo table on soundkorb.ru was empty: there was no server that runs
a battle between two people. Two things have to hold for a rating to mean
anything: both players get *the same* round, and neither decides alone
whether he was right.

## Considered

1. **The server streams the sound.** The app gets finished audio, the answer
   never reaches it. Fully fair, but the nine exercises' DSP would have to run
   on the server (JUCE in Node, or a second engine — which ADR 040 forbids),
   and every round costs megabytes of traffic.
2. **The server sets the round, the apps draw it.** Chosen.

## Decision

- The server (`server/lib/abctrainBattles.js` in the site repo) keeps a
  queue per family, pairs the two closest ratings, picks the exercise of the
  family and, per round, a **seed** and a **level** (from the average rating,
  +1 at rounds 4 and 7). Seven rounds, as with a bot.
- Each app builds the round from the seed: `Game::seedNextRound` restarts the
  exercise's own `juce::Random`, `setSeededRounds` turns off the personal
  weighting toward the player's weak spots (with it, one seed is two rounds —
  `tests/SeededRoundTest` fails without it), the clip is a built-in one chosen
  by the seed (`ReferenceAudioLibrary::selectSeededBuiltIn`; the player's own
  library differs between computers).
- The app sends what was chosen and what its round says is right. The server
  takes the right answer only when **both** apps name the same one, and
  judges the choice itself. Changing the right answer in one's own app
  therefore voids the round for both instead of winning it.
- No answer in 45 s is a wrong answer; leaving mid-battle gives the opponent
  the remaining rounds. The result goes through `recordBattle` (Elo, K = 32)
  and the site's live feed.
- Transport: HTTPS polling (0.5 s in a battle), the same nginx and token as
  sync. A round lasts tens of seconds; half a second is invisible in it.

## Honest limit

About 95% fair: the right answer lives in the app's memory, and someone who
writes a program to read it can. Option 1 closes that and is the next step if
cheating ever shows up in the table.

## Checked by

`tests/SeededRoundTest`, `tests/OnlineBattleTest`, `tools/BattleSmoke`
(two apps against a real server: every round identical, none voided, 7:0,
Decibelo ±16), `scripts/abctrain-battles-test.mjs` in the site repo.
