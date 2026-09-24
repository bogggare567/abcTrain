# 046 — Battles against bots: virtual listeners with a hearing profile

**Status:** accepted
**Date:** 2026-09-24

## The question

A battle needs an opponent, and a battle between people needs a round
server and other people online at the same time — neither exists yet
(ADR 045). The owner's idea: bots as animals with different hearing — a
Cat, a Hound, a Viper, an Owl, a Bat, an Elephant. The external review in
`notes/` agreed and added the shape: each bot is a *model of a listener*,
not a filter over the sound, and the app must say the animals are
characters, not biology.

## The decision

**A bot is a psychometric function per exercise.** For exercise *g* at
level *L* (1–10):

    P(right) = guess + (1 − guess − lapse) / (1 + exp(slope · (L − threshold[g])))

- `threshold` — the level where the bot is half-way between guessing and
  certain. The profile is nine numbers, one per exercise.
- `slope` — how sharply it goes from hearing to not hearing.
- `lapse` — wrong on an easy round anyway (a stray tap). Nobody is 100 %.
- `guess` — 1 / number of answers: never worse than chance.

This is the standard shape of a detection curve from psychoacoustics, so a
bot behaves like a listener: good where its profile is good, worse as the
round gets harder — exactly as the player does — and occasionally wrong on
an easy one. `Source/BotListener` is pure (no GUI, no audio) and the tests
drive it with a seeded `Random`.

| Bot | Strong at | Reaction |
|---|---|---|
| Hound | all-round, best on level and dynamics | 1.8 s |
| Cat | highs, air, transients (band, compression) | 1.2 s |
| Viper | lows, resonance, saturation (distortion, range) | 1.5 s |
| Owl | space: pan, width, reverb | 2.2 s |
| Bat | time: delay, attack | 0.9 s |
| Elephant | lows and level, slow and careful (lowest lapse) | 3.0 s |

**A battle is a run mode** (`SessionManager::Mode::duel`): seven rounds of
one exercise picked at random from the family the player chooses
(frequency / dynamics / space / character). The player answers as usual;
the bot answers the same round at the same level from its curve. No hints —
the bot gets none either. The HUD shows both scores; the results screen
says who won.

**Offline, and it stays out of the rating.** A bot battle runs without a
network and never reaches Decibelo: a fixed-profile opponent is practice,
and a rating fed by bots would be farmable.

## Why not

- **Random bots** (answer right with a fixed 70 %): no character, and they
  do not get harder with the round — a player learns nothing about "who
  hears what".
- **Bots that listen** (a filter model of an animal ear, then a detector):
  a research project of its own, and it would claim biology it cannot back.
  The review in `notes/` warned against exactly this.
- **ML-trained bots:** no data yet. The curves can later be fitted to real
  players' answers — the shape stays the same.

## Honesty

The Live page carries the line "Characters inspired by differences in
animal hearing — game profiles, not biology." A spider was suggested too;
it waits for an exercise about vibration.
