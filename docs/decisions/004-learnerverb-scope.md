# 004. Trimming LearnerVerb's visualization scope; decay-to-`roomSize` approximation

## Status

Accepted, implemented.

## Context

The original spec for LearnerVerb asked for three separate visualizations
on top of the reverb itself:

1. An impulse-response "cloud" (early reflections + tail).
2. A decay-time-vs-frequency graph (how fast highs vs. lows decay).
3. A stereo correlometer/vectorscope for the Width control.

Each of these is a legitimate, real feature — but bundled together they're
roughly three times the visualization work LearnerComp needed for its one
waveform view, on top of building the reverb engine itself. Building all
three by default, silently, would have meant either rushing them (thin,
half-working versions of each) or ballooning this single "add LearnerVerb"
task into something closer in size to three separate features.

## Decision

Ship LearnerVerb with the same visualization shape as LearnerComp: a live
scrolling wet/dry waveform (now `shared/WaveformDisplay`, extracted from
LearnerComp for this second use) plus input/output peak meters. Skip the
impulse-response cloud, the decay-vs-frequency graph, and the
correlometer/vectorscope for this pass — they're listed in
`docs/roadmap.md`'s 1.0 phase, not silently dropped.

This also motivated extracting `WaveformDisplay` out of
`LearnerComp/Source/` into `shared/`: with a second real consumer needing
the identical FIFO-accumulate/30 Hz-timer/scrolling-columns shape, the
abstraction is no longer a guess from one data point (see
[decisions/001](001-game-interface.md) and the roadmap note this closes
out). `SpectrumAnalyserComponent` (LearnerEQ) stayed separate *at the
time* — FFT-based, a different data shape entirely from time-domain peak
tracking. It was later extracted too, once LearnerComp and LearnerVerb
both wanted a plain live spectrum of their own — see
[decisions/006](006-unified-visualization.md).

## Decay is mapped onto `juce::dsp::Reverb`'s `roomSize`, not modeled

`juce::dsp::Reverb::Parameters` has no "decay in seconds" field — only
`roomSize` (0-1) and `damping` (0-1), which jointly and non-linearly
determine how long the Freeverb-derived algorithm's tail actually lasts.
`ReverbEngine::setParameters` maps the user-facing `Decay` knob
(0.1-10 s) onto `roomSize` linearly (`decaySeconds / 10`), with a couple
of per-type tweaks (Room halves it, Plate cuts damping further for a
brighter tail). This is a by-ear approximation, not a physical or even a
calibrated model — the same "tuned, not measured" approach already used
for `CompressionGame`'s presets and `ReverbEngine`'s Spring-type Q
mapping. The actual measured tail length for a given `Decay` setting has
not been verified against the number on the knob; if that mismatch turns
out to matter for the teaching goal, revisit this mapping specifically
rather than assuming it's exact.

## Consequences

- LearnerVerb ships faster and with the same quality bar as LearnerComp,
  rather than a rushed version of a much bigger visualization set.
- The `Decay` knob is not a precise "this exact number of seconds," and
  no test asserts it is (`LearnerVerbTest` checks behavioral properties -
  a tail exists, `dryWet=0` is dry, every type produces sound - not decay
  timing accuracy).
- The impulse-response cloud, decay-vs-frequency graph, and
  correlometer/vectorscope remain real, worthwhile follow-ups - tracked in
  `docs/roadmap.md`, not forgotten.
