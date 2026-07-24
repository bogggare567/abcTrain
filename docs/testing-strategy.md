# Testing strategy

## What exists today

`EarTrainerTests` (`tests/`, wired up in `CMakeLists.txt`) is a
`juce_add_console_app` target — a plain executable, not a plugin, so it
runs with no plugin host, no GUI, and no audio device. It uses
`juce::UnitTestRunner`; each `UnitTest` subclass self-registers via a
file-scope `static` instance and `TestRunner.cpp`'s `main()` runs all of
them, exiting non-zero if any assertion failed.

```bash
cmake --build build --target EarTrainerTests --config Release
./build/EarTrainerTests_artefacts/Release/EarTrainerTests
```

Two kinds of test currently exist, deliberately kept separate:

**Logic-level tests** (`EQGameTest.cpp`, `CompressionGameTest.cpp`,
`ReverbGameTest.cpp`, `GameManagerTest.cpp`) assert on game *state*, not
audio content: choice counts, scoring after a correct/incorrect answer,
that a second answer in the same round is a no-op, that switching games in
`GameManager` updates the active index, that an out-of-range index is
ignored. These are fast, deterministic, and would fail immediately if a
refactor broke the `Game` contract. `ReverbGameTest` additionally checks
the output buffer isn't silent after a round starts — a cheap smoke test
that both DSP paths (the `dsp::Reverb`-based types and the allpass-cascade
Spring type) actually produce sound, given they're structurally different
code paths inside `ReverbGame::process`.

**One DSP regression test** (`LearnerEQTest.cpp`) processes a real sine
wave through the real `LearnerEQProcessor` and asserts the *output level*
actually changes as expected (boosting a band's gain raises measured RMS
at that frequency). This exists because `LearnerEQ` was originally written
with no local build toolchain available to compile or run it — this test
is the first thing that has actually executed that filter chain. It's
intentionally narrow (one assertion) rather than a broad audio-correctness
suite; expanding it is listed below.

**`ProgressManagerTest.cpp`** covers level/points math (pure static
functions), streak increment/reset, daily challenge generation and
completion, and a persistence round-trip through a real (temp-named)
`juce::PropertiesFile`. None of it goes through the real
`Game → ChangeListener → ProgressManager` wiring — see the note on
`registerAnswer` below.

**`LearnerCompTest.cpp`** processes real sine waves through a real
`LearnerCompProcessor` (same approach as `LearnerEQTest`) and checks
closed-form compression math: a 0 dBFS sine at a -6 dB threshold with a
2:1 ratio and hard knee should settle at -3 dBFS; adding +3 dB makeup gain
on top of that should bring it back to ~0 dBFS. Also checks bypass leaves
the signal bit-for-bit unchanged, and that `applyPreset` sets every
parameter a preset defines. `applyPreset` deliberately lives on
`LearnerCompProcessor` rather than only inside the editor's preset-button
handler specifically so it's testable without constructing an editor — see
`docs/diagrams/learner-plugin.md` for why constructing a `Component` in
this console test binary was avoided as a matter of policy, not just for
this one case. That policy is also why the test added for
`shared/SpectrumAnalyzer.h`'s integration (once all three Learner plugins
gained a live spectrum — see
[decisions/006-unified-visualization.md](decisions/006-unified-visualization.md))
doesn't construct a `SpectrumAnalyzerComponent` directly: it's a
`juce::Component` with a `juce::Timer`, and initialising that on a headless
CI runner is a real risk, not a hypothetical one. Instead the test exercises
what actually changed and is safe to check without a message loop:
`processBlock` computing a mono downmix of the input every sample (in both
the normal and bypassed branches) behind a null check for "no analyzer
attached," the state every test runs in by construction.

**`LearnerVerbTest.cpp`** has no closed-form target the way compression
math does — a reverb tail's exact level isn't something to solve for in
closed form the way a compressor's steady-state gain reduction is — so
it's behavioral instead: a noise burst through a real `LearnerVerbProcessor`
should leave an audible tail once the burst stops (a dry passthrough
wouldn't); `dryWet = 0` should be an *exact* passthrough (not just close —
`0 * wet + 1 * dry` is exact in IEEE float, so this is a real bit-equality
check, not a tolerance fudge); every one of the 4 types should produce
non-silent output without crashing; `applyPreset`/out-of-range-index are
tested the same way as LearnerComp's.

## Why game logic and DSP output are tested differently

The games generate random noise (`PinkNoiseGenerator`) as their test
signal. Asserting on that noise's exact content would either be trivial
(any nonzero buffer passes) or flaky (comparing to expected values noise
generators don't produce deterministically without seeding, which nothing
here currently does). Testing what's actually deterministic — the choice
metadata and scoring state machine — gives real regression protection
without fighting the randomness. `LearnerEQ` is different: it processes a
*known* input (a synthesized sine wave) through *deterministic* filtering,
so asserting on output level is meaningful and repeatable.

## `ChangeBroadcaster` is asynchronous - tests can't rely on it

`juce::ChangeBroadcaster::sendChangeMessage()` (what every `Game` calls on
`newRound()`/`submitAnswer()`, and what `ProgressManager` calls after
`registerAnswer()`) is delivered via `AsyncUpdater`, which needs a running
JUCE message loop to actually invoke listeners. `EarTrainerTests` is a
plain console app that never pumps one. Concretely: calling
`game.submitAnswer(...)` in a test does *not* reliably (or possibly ever)
trigger `ProgressManager::changeListenerCallback` in that process. Rather
than write a test that might silently no-op or hang the binary waiting on
a message loop that's never running, `ProgressManager` exposes
`registerAnswer(int gameIndex, bool wasCorrect)` as a direct, synchronous
entry point that both the real `changeListenerCallback` and
`ProgressManagerTest` call — see
[decisions/002-difficulty-scaling.md](decisions/002-difficulty-scaling.md)
and the comment on `registerAnswer` in `Source/ProgressManager.h`. Net
effect: the *reaction logic* (scoring, streak-in-a-row, daily challenge)
is well-tested; the *wiring* that connects a real button click to that
logic is not, and can only be checked by actually running the plugin.

## Not yet built

- **CI is green, but confirm it stays that way.** All three OSes passed on
  commit `a2f2944`, catching two real bugs first (see
  `docs/diagrams/ci-pipeline.md`), again on `dd207d1` (LearnerComp), and
  again on `8932b84` (LearnerVerb) — each checked directly against the
  GitHub Actions API, not assumed. The MicroLesson/LessonController commit
  and the visualization-unification commit after it have not been
  confirmed yet as of this writing. Every change still needs to actually
  go through CI, not just look correct on read-through.
- **Integration tests**: nothing exercises `PluginEditor` (button clicks
  changing `GameManager` state end-to-end), the `SliderAttachment`/
  `ComboBoxAttachment` wiring in any Learner editor, or the real
  `Game → ChangeListener → ProgressManager` path (see above). Nor does
  anything exercise `LessonController`'s actual APVTS-setting behavior or
  `SpectrumAnalyzerComponent`'s FFT/timer logic directly, for the same
  reason (see decisions/006-unified-visualization.md). JUCE's `UnitTest`
  framework can run headless GUI tests with
  `juce::ScopedJuceInitialiser_GUI` plus a pumped message loop, but it
  wasn't set up here — worth adding once the editors are stable enough
  that testing them is worth the setup cost.
- **Golden-file audio regression tests**: rendering a fixed input through
  a plugin and diffing against a saved reference output (via
  `shared/TestUtils.h`'s `rms`, or a tighter per-sample comparison) would
  catch DSP regressions during refactors that the current logic-only tests
  can't see. There are now three DSP-heavy plugins (LearnerEQ, LearnerComp,
  LearnerVerb), each with only steady-state or behavioral assertions and
  no coverage of transient/attack-release behavior — this is worth doing
  soon, not just "eventually."
- **Seeded randomness for the games**: `juce::Random`'s default
  constructor seeds from system entropy; none of the game classes expose a
  way to inject a seed. If deterministic game-round tests are ever needed
  (e.g. "the 3rd round after seed X always picks band 5"), that requires
  adding a seed parameter to the games, which doesn't exist today.
