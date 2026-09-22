# 040 — One engine per effect, the exercise's own sound, and a measured threshold

**Status:** accepted
**Date:** 2026-09-23

The literature review ([research/2026-09-literature-audit.md](../research/2026-09-literature-audit.md))
turned up findings that all touched the same files — the exercises and the
staircase. They were done together, so each file was opened once.

## One engine per effect (finding O4)

The trainer taught reverb on `juce::dsp::Reverb` (Freeverb) tuned by ear
plus four all-pass filters standing in for a spring, and compression on
`juce::dsp::Compressor` with a hard knee. Learner Verb and Learner Comp
process audio with an FDN after Jot, Dattorro's plate, dispersive springs
and a Giannoulis/Massberg/Reiss soft-knee compressor. So a player learned
to recognise one reverb and then turned a different one.

The engines moved to **`shared/dsp/`** (a header-only `abc_dsp` library):
`ReverbEngine`, `ReverbMeasure`, `CompressorEngine`, `EQCoefficients`.
`ReverbGame` and `CompressionGame` now use them, with families written in
the units an engineer sets — RT60 in seconds, pre-delay in ms, a send level
— instead of Freeverb's unitless room size. `ReverbEngine::setTypeNow`
switches algorithm at once with an empty tank, because the trainer changes
space only between rounds and a crossfade would play the last answer over
the next question. The reverb game is now a send (dry + wet × send) rather
than 100 % wet, which is how a reverb is used.

## The matched bell (finding O1)

The RBJ bell narrows near Nyquist: at 44.1 kHz a Q 2 bell at 16 kHz is
0.9 dB half an octave from its centre where the analog bell is 3.1 dB, so
the top targets of "Guess the Band" were harder than their level said, and
Learner EQ's curve did not match what it did. `EQCoefficients::makeMatchedBell`
is Vicanek's matched design (2016): impulse-invariant poles, numerator
matched to the analog magnitude at DC, centre and Nyquist, a cut built as
the exact reciprocal of the boost (matching a deep cut directly near
Nyquist is numerically fragile — found by sweeping it). Worst error against
the analog bell over 22.05–192 kHz, 20 Hz–0.45 fs, Q 0.3–10, ±18 dB:
3.4 dB at 0.45 fs, where RBJ is 17 dB out. Shelves and pass filters stay
RBJ. `tests/SharedEnginesTest` holds the numbers.

## Distortion without aliasing (finding O2)

The waveshaper ran at the host rate, so a hard clip folded inharmonic
partials back under Nyquist — a tell no real plugin gives. Now first-order
antiderivative anti-aliasing (Parker, Zavalishin & Le Bivic, DAFx 2016),
with closed-form antiderivatives for tanh, the asymmetric tanh and the
clip. On a 5 kHz sine clipped at 44.1 kHz it cuts the off-harmonic energy
from 2.8 % to 0.6 %. No oversampling, no latency, no allocation.

## The exercise's own sound (finding M1)

Pink noise has no pitch, so a waveshaper on it cannot show odd against
even harmonics; compression is heard on drums; reverb is judged on a hit.
`TestSignalGenerator::setExerciseBed` renders a few variations of one
`LessonAudioBed` (moved to `shared/audio/`, its `Bed` enum with it) at the
same RMS as the pink noise — the exercises that care about input level
were tuned against the noise. Distortion plays a chord, Compression a drum
loop, Reverb a single hit. The burst envelope that shaped noise into hits
is applied only when noise is actually playing. Makeup gains are measured
on what the player is about to hear (`fillForMeasurement`), not on noise.
Pink noise stays available and is what `setPreferExerciseSound (false)`
gives.

## Boosts first (finding M2)

A dip is harder to hear than a peak of the same size (Bücklein 1981), so a
50/50 boost/cut draw made every staircase step two difficulties.
`Game::cutChanceForLevel`: no cuts on steps 1–3, then 10 % more per step
up to half — the order Corey and Harman's *How to Listen* use.

## Asking where you miss (finding M5)

`Game::setBucketWeights`, `drawWeighted`, `drawCorrectOfPair`, `keepDraw`.
`ProgressManager::computeBucketWeights` gives each skill bucket
0.25 + miss rate (0.75 until it has three attempts) and pushes it into the
game after every answer. EQ draws a range by octave width × weight and
then a point inside it (uniform weights reproduce the old log-uniform draw
exactly); Pan, Gain and Delay reject-and-redraw by weight; the
two-alternative games bias which of the pair is the answer. After Kaniwa
et al. (SMC 2011).

## The threshold is the reversals, not the record (finding M3)

The record is the top of a random walk. In the simulated listener of
`ProgressManagerTest` (true threshold step 4.4) the record reaches 10 while
the mean of the last reversals reads 5.0. `ProgressManager` now keeps the
last eight reversals per exercise (persisted) and
`getThresholdLevelForGame` returns the mean of the most recent even number
of them once there are six (Levitt 1971). The home screen shows that
number once it exists and the record until then; the ruler's fill stays
the record.

## Not changed

- The question each exercise asks (Weak/Medium/Strong, reverb *type*):
  finding M7 is still under discussion.
- Loudness matching is still RMS (finding M8 wants a measurement first).
