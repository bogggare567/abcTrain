# Spring and plate: do the knobs mean what they mean on the room? (2026-09-25)

The owner's question: the spring and the plate should react to the knobs
the way the room does. This note records what was measured before and after
the change, not what the code was assumed to do.
`tests/ReverbCharacterTest` produces these numbers; run
`REVERB_REPORT=1 EarTrainerTests ReverbCharacter` for the full table.

## What each knob should mean, on all four types

| Knob | Meaning | How it is measured |
|---|---|---|
| Decay | RT60 in seconds | Schroeder backward integration, T30 |
| Damping | treble ratio: RT60 at 5 kHz divided by RT60 at 500 Hz. 1.00 at 0 %, 1 − 0.86 = 0.14 at 100 % | octave bands around 500 Hz and 5 kHz |
| Size | changes the structure without changing the length | time to reach normalised echo density 0.8 (Abel & Huang, AES 2006); for the spring, the chirp |

## Before (Decay 2 s)

| Type | Damping 0 → treble ratio (target 1.00) | Damping 100 % → treble ratio (target 0.14) |
|---|---|---|
| Room | 0.57–0.75 | 0.44 |
| Hall | 0.78–0.92 | 0.28–0.42 |
| Plate | 0.96–0.98 | 0.52–0.68 |
| Spring | 0.41–0.53 | 0.22–0.31 |

The spring's first return arrived about 2.7 ms earlier at 3.2 kHz than at
400 Hz, at every Size. So Size did not change the chirp at all.

Three causes:

1. **The damping filter aimed at the wrong frequency.** It set the treble
   ratio at Nyquist (22 kHz), but a first-order low-pass is much gentler at
   5 kHz than at Nyquist. That applied to every type, the room included, so
   the knob did about a third of what it said.
2. **Linear interpolation inside the feedback loops.** A fractional read is
   a low-pass whose cutoff moves with the fraction. Inside a feedback loop
   it takes treble away on every pass, so the room lost high end even at
   0 % damping.
3. **The plate and the spring had their own fixed low-passes** that ignored
   Decay. The spring's was 7 kHz → 2.5 kHz regardless of loop length.
   Neither was derived from the RT60 rule used by the room.

## After (Decay 2 s)

| Type | Damping 0 | Damping 50 % (target 0.57) | Damping 100 % (target 0.14) |
|---|---|---|---|
| Room | 1.00–1.02 | 0.60–0.62 | 0.14–0.20 |
| Hall | 0.97–1.03 | 0.60–0.61 | 0.14 |
| Plate | 0.97–0.99 | 0.60–0.62 | 0.17 (small) / 0.30 (largest) |
| Spring | 0.79–0.82 | 0.53–0.55 | 0.22–0.25 |

Spring chirp, the first return at 400 Hz minus at 3.2 kHz:

| Size | Spread |
|---|---|
| 0.1 | 5.3 ms |
| 0.5 | 8.1 ms |
| 0.9 | 11.1 ms |

## What changed (shared/dsp/ReverbEngine.h)

- **One absorption rule for every loop:**
  - it covers the FDN lines, the plate's two branches and the springs;
  - each loop's filter is solved for its own length, so it loses exactly
    what RT60 at 500 Hz and at 5 kHz ask for;
  - a long loop that needs more than about 12 dB between those two
    frequencies gets up to eight one-pole filters in a row, each taking a
    root of the ratio. The loss is spread along the loop, as in a real
    plate, where the whole sheet absorbs.
- **All-pass interpolation (Dattorro 1997) on the two moving FDN lines;**
  the other lines read whole samples.
- **The spring:**
  - dispersion grows with Size: 40–120 all-pass stages, because a longer
    coil spreads the chirp further (Välimäki, Parker & Abel 2010);
  - the chain's own delay is taken out of the plain delay, so Size still
    sets the time around the coil and Decay stays in seconds;
  - the spring's roughly 5 kHz bandwidth is now a tone filter outside the
    loop, so it colours the sound without shortening the tail.

## What the spring and the plate still do not do like a room, on purpose

- **Early reflections.** A plate is a sheet of metal and a spring is a
  coil; neither has walls. They are dense, or chirping, from the first
  millisecond. Size therefore acts on their structure: the plate's tank
  length and echo density, the spring's length and chirp. It cannot move
  reflections that do not exist.
- **Echo density on the spring.** A spring never becomes dense like noise.
  It is a train of chirps, one per trip around the coil, and that is its
  sound. The test checks the spring's Size through the chirp instead.

## Remaining gaps

- **The largest plate at 100 % damping** reaches a treble ratio of 0.30,
  not 0.14. The branch is about 0.35 s long, the treble ratio needed per
  trip is about 1:1700, and the octave band around 5 kHz includes 3.5 kHz,
  where eight one-poles cut less. A true fix needs a shelving absorption
  filter, which is the next step.
- **The spring at 0 % damping** reads 0.8, not 1.0. Its chirps spread 5 kHz
  energy in time, and the T30 fit in that band sees a shorter slope. That
  is within the test's tolerance.

The golden references (`tests/golden/verb-*.flac`) were regenerated with
this change, as the golden test requires.
