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
`GameManagerTest.cpp`) assert on game *state*, not audio content: choice
counts, scoring after a correct/incorrect answer, that a second answer in
the same round is a no-op, that switching games in `GameManager` updates
the active index, that an out-of-range index is ignored. These are fast,
deterministic, and would fail immediately if a refactor broke the `Game`
contract.

**One DSP regression test** (`LearnerEQTest.cpp`) processes a real sine
wave through the real `LearnerEQProcessor` and asserts the *output level*
actually changes as expected (boosting a band's gain raises measured RMS
at that frequency). This exists because `LearnerEQ` was originally written
with no local build toolchain available to compile or run it — this test
is the first thing that has actually executed that filter chain. It's
intentionally narrow (one assertion) rather than a broad audio-correctness
suite; expanding it is listed below.

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

## Not yet built

- **CI** (`docs/diagrams/ci-pipeline.md`): none of this runs automatically
  yet. Every test run so far has been a manual, careful read of the code
  against JUCE's API, not an actual compile+run, since the environment
  these plugins were written in had no CMake/compiler available. This is
  the single biggest risk in the current codebase — get CI running before
  trusting any of the DSP code without a human rebuilding and listening to
  it first.
- **Integration tests**: nothing exercises `PluginEditor` (button clicks
  changing `GameManager` state end-to-end) or the `SliderAttachment` wiring
  in `LearnerEQEditor`. JUCE's `UnitTest` framework can run headless GUI
  tests with `juce::ScopedJuceInitialiser_GUI`, but it wasn't set up here —
  worth adding once the editors are stable enough that testing them is
  worth the setup cost.
- **Golden-file audio regression tests**: rendering a fixed input through
  a plugin and diffing against a saved reference output (via
  `shared/TestUtils.h`'s `rms`, or a tighter per-sample comparison) would
  catch DSP regressions during refactors that the current logic-only tests
  can't see. Worth adding once there's more than one DSP-heavy plugin to
  protect.
- **Seeded randomness for the games**: `juce::Random`'s default
  constructor seeds from system entropy; none of the game classes expose a
  way to inject a seed. If deterministic game-round tests are ever needed
  (e.g. "the 3rd round after seed X always picks band 5"), that requires
  adding a seed parameter to `EQGame`/`CompressionGame`, which doesn't
  exist today.
