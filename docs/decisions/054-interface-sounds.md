# 054 — Interface sounds: several takes, never twice the same

Date: 2026-10-02. Status: accepted (Bogdan: «звуки нужно использовать как
системные … предусмотреть, чтоб короткие ваншоты разнообразно звучали и не
надоедали»).

## Problem

The app was silent about its own events: an answer, a step up, a record, a
battle's end landed only on screen. Bogdan recorded one-shots for them. A
cue heard two hundred times an evening is the one place a trainer's sound
can wear on the person it is training — the identical sample turns into a
dripping tap within minutes.

## Decision

- Takes live in `assets/ui-sounds/<event>-<n>.flac` (CC BY 4.0, Bogdan
  Korablev), embedded as `UiSoundData`; `tools/ui_sounds/import.py` trims,
  fades and normalises a folder of raw takes into that layout.
- `shared/audio/UiSounds` plays them, and every trigger is a little
  different:
  - a random take, never the one played last for that event;
  - ±35 cents and ±1.5 dB at random — below what reads as "a different
    sound", above what the ear stops noticing as identical;
  - the same event again within 1.5 s comes 2 dB quieter per repeat, down
    to −8 dB, so a run of right answers backs off instead of hammering.
- Events: correct, wrong, step-up and new-record (a step or a record takes
  the place of "correct"), achievement, run-end, battle won/lost, round
  start, open.
- Mixed **before** the output level (the monitoring knob turns them down
  too), at −20 dB by default, under the material: a cue says "noted", it
  never covers the sound being judged. Silent during the hearing test and
  calibration. Settings → Hearing → «Звуки интерфейса»: off / quiet /
  normal.
- Audio thread: `trigger()` only writes into a lock-free FIFO; `render()`
  mixes up to six voices with linear interpolation, no allocation
  (ADR 038).
