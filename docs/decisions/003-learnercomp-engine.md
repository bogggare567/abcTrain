# 003. A custom compressor engine for LearnerComp, not `juce::dsp::Compressor`

## Status

Accepted, implemented.

## Context

`EarTrainer`'s `CompressionGame` already uses `juce::dsp::Compressor` for
its "guess the compression strength" exercise, where the player never
sees or controls the underlying gain reduction — only threshold/ratio
presets picked internally. `LearnerComp` is different: it's a real,
user-facing compressor plugin, and the spec for it (matching what any
real teaching compressor needs) requires:

1. A **Knee** control (hard vs. soft knee) — a core compression concept
   worth teaching.
2. A **live gain-reduction reading**, both as a numeric meter and to
   color-highlight the output waveform wherever the compressor is
   actively working.

`juce::dsp::Compressor` supports neither. It's a hard-knee-only black box
with no accessor for its internal gain-reduction state — by design, it's
meant to be dropped into a signal chain and just work, not introspected.

## Decision

`LearnerComp/Source/CompressorEngine.h` implements a small custom
feed-forward compressor instead:

- **Gain computer**: the standard soft-knee formula from Giannoulis,
  Massberg, and Reiss, *"Digital Dynamic Range Compressor Design"* — hard
  knee when `kneeDb <= 0`, a quadratic blend region within
  `±kneeDb/2` of the threshold otherwise.
- **Envelope**: a one-pole smoother over the gain-reduction value itself
  (not over the input level), using the attack coefficient while
  reduction is increasing and the release coefficient while it's
  decreasing — this is what makes `getLastGainReductionDb()` meaningful
  as "how many dB is being pulled right now."
- **Detection is computed separately from where gain is applied**:
  `computeGain(detectionSample)` takes one detection value and returns a
  gain to apply to arbitrary channel data. `LearnerCompProcessor` computes
  the detection value as the loudest of the input channels at each
  sample (stereo-linked detection), then applies the *same* resulting
  gain to every channel — avoiding the stereo-image pumping that
  independent per-channel compression can cause.

## Consequences

- More DSP code to maintain than wrapping `juce::dsp::Compressor` would
  have been, and it hasn't been validated against a reference
  implementation beyond the closed-form steady-state math checked in
  `tests/LearnerCompTest.cpp` (0 dBFS in, fixed threshold/ratio/knee=0,
  assert the settled output level). Attack/release *transient* behavior
  (how it sounds during the ramp, not just the steady state) is
  unverified by any automated test — only by ear, and only once someone
  actually builds and runs this.
- `getLastGainReductionDb()` existing at all is what makes the waveform
  highlighting and the GR meter possible — this was as much a
  visualization requirement as a knee requirement.
- Any future Learner plugin that needs a different JUCE `dsp::` building
  block but also needs to expose live internal state to its UI (envelope
  followers, filter response, etc.) will likely hit the same "the
  built-in class doesn't expose what the teaching UI needs" wall — this
  is the second time it's happened (`ReverbGame`'s Spring type, built as
  a custom allpass cascade because `juce::dsp::Reverb` can't produce that
  character, was the first). Worth remembering when starting LearnerVerb.
