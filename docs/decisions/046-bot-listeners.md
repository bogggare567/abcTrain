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

**A bot is a one-neuron perceptron per exercise** with a logistic output —
the psychometric function. For exercise *g*, part *b* of it (the round's
skill bucket: frequency range, pan zone, delay class, reverb type…) and
level *L* (1–10):

    P(right) = guess + (1 − guess − lapse) / (1 + exp(slope[g] · (L − threshold[g][b])))

- `threshold` — the level where the bot is half-way between guessing and
  certain, per part of the exercise: the Cat is sure of a boost at 8 kHz and
  guesses at 35 Hz on the same level.
- `slope` — how sharply it goes from hearing to not hearing.
- `lapse` — wrong on an easy round anyway (a stray tap). Nobody is 100 %.
- `guess` — 1 / number of answers: never worse than chance.

**The weights are trained, and the data are honest about what they are.**
`tools/bots/train_bots.py` builds a teacher from published animal hearing
data (audiograms, minimum audible angle, timing — every number with its
source in `docs/research/2026-09-bot-hearing.md`, the gaps marked as game
choices), simulates 40 000 rounds per bot and fits the perceptron by maximum
likelihood; the result is `Source/BotWeights.h` (generated, not edited). No
animal has ever answered an EQ round and no player data exist yet, so the
student can only recover its teacher (to ~0.1 level) — the point of the
pipeline is the next step: the same fit on real answers
(`--answers game,bucket,level,correct`) for bots tuned on the pilot group or a
"twin" of a player.

The data overturned the first, folk-biology version (2026-09-24):

| Bot | Strong at (from the data) | Weak at | Reaction |
|---|---|---|---|
| Hound | even hearing, mids and highs, dynamics | direction (MAA ~8°) | 1.8 s |
| Cat | highs and air, transients | sub-bass, space | 1.2 s |
| Viper (python data) | sub-bass and bass, by vibration | anything above ~1 kHz | 1.5 s |
| Owl | presence 4–8 kHz, direction | the lows | 2.2 s |
| Bat | time: delay, attack; air | below ~2 kHz | 0.9 s |
| Elephant | space: pan, width, reverb (MAA ~1°); mids | air | 3.0 s |

Overall strength per bot is a game choice so that each is a fair opponent;
the *shape* is the biology.

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
