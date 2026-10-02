# 057 — Bogdan's own recordings: a starter set in the app, the full library as a pack

Date: 2026-10-02. Status: accepted (Bogdan: «я сделал звуки, их нужно
использовать как системные в проекте … чтоб короткие ваншоты разнообразно
звучали и не надоедали»; all his own recordings, CC BY 4.0).

## Problem

The built-in material was synthesized (ADR 018): two drums, three tones,
and BuiltInSynth's set. A trainer for sound engineers that plays only
synthesized hits teaches synthesized hits. Bogdan recorded a library —
~1,100 files, ~800 MB: kicks, snares, claps, toms, cymbals, orchestral
drums, percussion, loops, synth and bass shots, ethnic and orchestral
instruments, vocal shots and phrases.

## Decision

- **Starter set in the app** (`assets/starter-pack/`, ~17 MB, 83 files): one
  file from every folder of the library, picked evenly, by
  `tools/library/pack_from_folder.py --embed` (silence trimmed, up to 4 s,
  peak −1 dBFS). Embedded in SampleData; shown as three built-in categories
  — Drums, Instruments, Voice (recorded) — after the synthesized ones.
  Credited per clip (Bogdan Korablev, CC BY 4.0).
- **The full library as a pack**, `bogdan-studio-1.0.0.zip` (1,123 clips,
  306 MB), built by the same script without `--embed`: one category per
  folder, instrument tags from the path, up to 12 s a clip. Published in
  abcTrain-library's releases; the app lists and installs it on "Download
  packs" (s03). The installer stays tens of megabytes, not hundreds.
- **Not tiring**: the exercises already draw a different clip each round;
  with dozens of real hits instead of two synthesized ones, a kick is no
  longer the same kick every round. The interface cues (ADR 054) are taken
  from the same library — synth shots on C/E/G for a right answer, low
  percussion for a wrong one, snaps for a round start, orchestral hits and
  brass stabs for the big moments — several takes each, never the same
  twice in a row, ±35 cents and ±1.5 dB.
