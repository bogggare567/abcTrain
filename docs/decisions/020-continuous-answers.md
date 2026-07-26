# 020. Continuous answers with a difficulty-scaled tolerance band

## Status

Accepted, implemented for EQ, Pan, Gain and Delay. Verified by
`tests/ContinuousScaleTest` (one shared contract run against all four)
plus playing each in the running app.

## Context

Every exercise scored the same way: N fixed choices, one of them right,
`submitAnswer(int)`. For the games whose skill is *categorical* - which
reverb type, which kind of saturation - that is the correct model and it
stays.

For the games whose skill is a **value**, it was wrong, and wrong in a way
that quietly defeated the point of the trainer:

- `EQGame` boosted one of eight fixed octave centres. A player could get
  good at it by memorising eight *positions* without ever learning what a
  frequency sounds like, and "1.6 kHz" is not a thing you dial - real
  frequencies are 425 Hz and 3,145 Hz.
- `PanGame` had five named positions. "Left" is not a pan value.
- `DBGame` had five round numbers, so the answer was always one of five
  round numbers.
- `DelayGame` had four fixed times.

Difficulty had the same problem. The only lever was making the *stimulus*
weaker - a smaller boost, a smaller gain step - which converges on
"inaudible" rather than "precise". Past a point that tests luck.

## Decision

`Game` gains an optional continuous-answer mode. The player drags along a
scale and is scored on how close they land, with an accept band that
narrows as difficulty rises.

```
virtual bool  usesContinuousScale() const;
virtual float getToleranceNormalised() const;
virtual float getCorrectNormalised() const;
virtual float getChosenNormalised() const;
virtual juce::String formatNormalisedValue (float) const;
virtual void  submitNormalisedAnswer (float);
virtual std::vector<GridMark> getGridMarks() const;
```

### Everything is in normalised 0..1 axis space

The game owns the mapping to real units. `ChoiceSliderComponent` is a dumb
ruler: it knows how to draw a scale, track a pointer, and show a band, and
nothing about frequencies or decibels. That is what lets one widget serve
a log frequency axis, a linear pan axis and a log time axis without a
single conditional.

### Tolerance is expressed in the unit the ear actually works in

This is the part that matters, and it is different per game:

| Game | Axis | Tolerance unit | Easy → Hard |
|---|---|---|---|
| EQ | log Hz | **octaves** | 1.0 → 0.6 → 0.35 |
| Pan | linear | field position | 0.35 → 0.22 → 0.12 |
| Gain | linear dB | **dB** | 2.5 → 1.5 → 1.0 |
| Delay | log ms | **ratio** | 35% → 22% → 13% |

An octave tolerance is a constant *ratio*, so the slack at 200 Hz is the
same as at 8 kHz - a tolerance in Hz would be absurdly tight low down and
meaningless high up. Same for delay: being 20 ms out at a 40 ms delay and
at a 500 ms delay are completely different mistakes, and only a ratio
treats them as such.

Gain is the exception that proves the rule: dB *is* already the
perceptual unit, so the tolerance is linear in it. Its hard tier stops at
±1 dB rather than going lower, because below roughly 1 dB a level
difference stops being reliably audible at all - a tighter band would be
testing luck, which is the exact failure this ADR exists to fix.

### The discrete path is preserved verbatim

`submitAnswer(int)` still exists on the converted games and keeps its
original exact-match rule against the nearest grid mark, rather than
being reimplemented in terms of the tolerance band. That is why all 134
pre-existing tests passed with no edits at all - the change is purely
additive. Every hook is non-pure-virtual with an inert default, the same
shape `setReferenceAudioLibrary` already established, so the five
categorical games are untouched.

### The ruler carries two densities

`getGridMarks()` returns labelled positions with an `emphasised` flag:
octave centres (or whole dB, or doublings) emphasised on one label row,
the boundaries between them quieter on a second row. At eight or nine
marks a single row collides at this width; staggering is what the
reference trainers do, for exactly that reason.

## Consequences

- Difficulty is now honest: harder means *more precise*, not *less
  audible*.
- The target is drawn from the range each round, so there is nothing to
  memorise. EQ draws log-uniformly (uniform in octaves - drawing
  uniformly in Hz would put nearly every target above 6 kHz); Gain
  quantises to 0.5 dB and Delay to 5 ms so the answer stays a number a
  person could plausibly name.
- The tolerance band is drawn *around the cursor while aiming*, then on
  the target once answered. Showing it while aiming is deliberate: the
  player should know how much slack they have, and it is also the only
  honest way to show that difficulty changed something.
- Two bugs came out of building the UI for this, both invisible to the
  compiler and the tests: the first and last grid labels lost half their
  text to the panel's clip region, and the tolerance band at 0.055 alpha
  was invisible against the well - which defeats the entire purpose of
  drawing it.
- Still discrete, correctly: Compression, Reverb, Distortion and
  Frequency-Range (its answer *is* a named category). Stereo Width is the
  open question - "Narrow/Wide" is arguably a value too, and it is the
  one remaining game where this decision could go either way.
