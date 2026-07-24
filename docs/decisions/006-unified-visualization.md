# 006. Unifying visualization and bypass across the three Learner plugins

## Status

Accepted, implemented.

## Context

The three Learner plugins grew their visualizations independently as each
was built (LearnerEQ first, then LearnerComp, then LearnerVerb), and it
showed: LearnerEQ had a spectrum + response curve but no waveform view;
LearnerComp and LearnerVerb had a waveform view (via `shared/WaveformDisplay`,
see [decisions/004](004-learnerverb-scope.md)) but no spectrum; and only
LearnerComp had a Bypass/A-B toggle. A user moving between the three would
see a different shape of UI in each, for no reason tied to what each
plugin actually teaches.

## Decision

**Extract the live-spectrum machinery into `shared/SpectrumAnalyzer.h/.cpp`**,
the same way `shared/WaveformDisplay` was extracted once a second consumer
needed it (decisions/004). `SpectrumAnalyzerComponent` is the plain FIFO-
accumulate/FFT/30 Hz-timer spectrum view, with a protected virtual
`paintOverlay()` hook and no knowledge of EQ bands, filter coefficients, or
highlighting. LearnerEQ's existing `SpectrumAnalyserComponent`
(`LearnerEQ/Source/SpectrumAnalyser.h`) now subclasses it, moving only the
combined 4-band response curve and highlighted-band drawing into
`paintOverlay()` - the visible behavior is unchanged, but the FFT/FIFO code
that used to be duplicated is now the same class LearnerComp and LearnerVerb
use directly for a plain live spectrum of their own input signal. This
supersedes the note in [decisions/004](004-learnerverb-scope.md) that said
`SpectrumAnalyserComponent` would stay LearnerEQ-only.

**Naming note:** the shared base and the two new processor methods that
feed it (`setSpectrumAnalyzer`) use the American "Analyzer" spelling, as
specified for this task, while LearnerEQ's existing, already-shipped
class/method (`SpectrumAnalyserComponent`, `setSpectrumAnalyser`) keeps its
established British spelling rather than being renamed for cosmetic
consistency. Both names appear side by side in `LearnerEQ/Source/SpectrumAnalyser.h`
for exactly this reason - it's intentional, not a typo.

**What each plugin's spectrum shows differs by design, not oversight:**
LearnerEQ's spectrum is fed the *post-filter* (output) signal, unchanged
from before - the point is seeing what the EQ curve did to the sound.
LearnerComp's and LearnerVerb's spectrum is fed a mono downmix of the
*input* (pre-processing) signal - dynamics and reverb don't reshape the
frequency content the way an EQ does, so showing the source material's
spectrum is the more useful teaching signal, and avoids a false impression
that compression/reverb "do something" to the frequency balance.

**Bypass (A/B) is now a `bypass` APVTS bool parameter on all three
processors**, following the pattern LearnerComp already had:
- LearnerEQ: bypass skips all four filters entirely (`processBlock` runs
  the filter chain only when not bypassed), same shape as LearnerComp
  skipping its `CompressorEngine`.
- LearnerVerb: bypass forces the effective wet mix to 0% *without*
  mutating the `Dry/Wet` parameter's stored value, so un-bypassing
  restores whatever the user had dialled in rather than losing it. The
  `ReverbEngine` still runs every block regardless of bypass, so its
  internal tail state stays warm - toggling bypass off mid-tail doesn't
  produce a cold-start click, which literally zeroing `Dry/Wet` and
  reverting it would risk if the engine's state had been reset instead.

**All three editors now place the Bypass toggle immediately to the left of
the Lesson button in the title row**, rather than LearnerComp's previous
placement in the bottom row next to its presets - a single consistent
location across all three plugins beats matching LearnerComp's original,
arbitrary choice.

**Layout order is Spectrum above Waveform** in all three editors (LearnerEQ
already had this shape with its one visualization; LearnerComp and
LearnerVerb both had Waveform first and gained a Spectrum above it).

## What this deliberately does not include

**No `SpectrumAnalyzerComponent` is constructed in `EarTrainerTests`.** It's
a `juce::Component` with a `juce::Timer`, and this console test binary
doesn't run a JUCE message loop - see `docs/testing-strategy.md`'s existing
"no `Component` in the console test binary" policy, established when
`LearnerCompTest.cpp` was first written. Initialising JUCE's GUI/message
machinery in a container with no display server (the default for CI
runners) is a real, not hypothetical, risk to something this project
actively depends on: `EarTrainerTests` passing on all three OSes. Rather
than gamble that risk for one test, `LearnerCompTest.cpp`'s new coverage
targets what actually changed and is safe to test directly: `processBlock`
now computes a mono downmix of the input every sample (guarded by a null
check for "no editor attached," the state every test runs in by
construction) to feed a spectrum analyzer - that code path, in both the
normal and bypassed branches, is what's exercised.

## Consequences

- LearnerEQ, LearnerComp, and LearnerVerb now present the same shape of UI:
  Spectrum, then Waveform + peak meters, then the plugin's own controls,
  then presets (where applicable) - with Bypass next to Lesson in every
  title row.
- Each editor's window grew taller to fit the added visualization -
  LearnerEQ 760x700, LearnerComp 820x780, LearnerVerb 760x720 - chosen
  generously rather than computed to the pixel, consistent with how the
  original sizes for all three were picked.
- Headless GUI testing (`juce::ScopedJuceInitialiser_GUI` + a pumped
  message loop, already named as a future option in
  `docs/testing-strategy.md`) remains a real follow-up if
  `SpectrumAnalyzerComponent`/`LessonController`'s own logic ever needs
  direct test coverage - not ruled out, just not taken on here.
