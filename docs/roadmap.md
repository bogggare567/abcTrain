# Roadmap

Status legend: ✓ done · 🚧 in progress · ⏳ planned, not started.

Note on scope: the "Beta"/"1.0"/"2.0" phase boundaries below are a rough
sequencing guess, not commitments — treat them as "roughly this order,"
not a schedule. Market-size or competitor claims from earlier planning
conversations are not reflected here; this table only tracks what's built
vs. not.

## MVP — trainer core + first teaching plugin

| Feature | Status |
|---|---|
| `Game` interface + `GameManager` (see [decisions/001](decisions/001-game-interface.md)) | ✓ |
| EQGame ("guess the boosted/cut band") | ✓ |
| CompressionGame ("guess the compression strength") | ✓ |
| ReverbGame ("guess the reverb type": room/hall/plate/spring) | ✓ |
| Generic `PluginEditor` (game selector + dynamic choice buttons) | ✓ |
| LearnerEQ: real 4-band EQ, host-automatable, live spectrum + response curve | ✓ |
| LearnerEQ: contextual tooltip per frequency range while dragging | ✓ |
| Unit test infra (`EarTrainerTests`, `juce::UnitTestRunner`) | ✓ |
| Unit tests: game scoring/state logic (all 3 games) | ✓ |
| DSP regression test: LearnerEQ band boost raises measured RMS | ✓ |
| CI: build + run tests on push/PR | ✓ confirmed green on all 3 OSes (commit `a2f2944`), including `EarTrainerTests` actually running and passing, not just building |

## Beta — round out the trainer, second teaching plugin

| Feature | Status |
|---|---|
| `ProgressManager`: points, level 1-10, `juce::PropertiesFile` persistence (see [decisions/002](decisions/002-difficulty-scaling.md)) | ✓ |
| Adaptive difficulty (`Game::setDifficulty`, all 3 games, driven by level) | ✓ |
| Daily login streak | ✓ |
| Daily challenge (N-in-a-row on a deterministically-picked game, bonus points) | ✓ |
| Level/progress bar/streak/daily-challenge shown in `PluginEditor` | ✓ |
| Unit tests: `ProgressManagerTest` (level math, streak, daily challenge, persistence round-trip) | ✓ |
| LearnerComp: custom soft-knee compressor engine (see [decisions/003](decisions/003-learnercomp-engine.md)), waveform visualization with gain-reduction highlighting, GR/peak meters, bypass (doubles as A/B), 4 teaching presets | ✓ |
| Unit tests: `LearnerCompTest` (closed-form compression/makeup-gain math, bypass passthrough, preset application) | ✓ |
| Shared visualization/hint components factored out for reuse across Learner plugins | ⏳ still not done — LearnerEQ's `SpectrumAnalyserComponent` and LearnerComp's `WaveformDisplay` are separate, unshared implementations despite similar shape (FIFO-fill from audio thread + timer-driven repaint). Worth extracting once a third Learner plugin needs the same pattern. |
| Integration-level tests (editor button clicks -> GameManager state) | ⏳ |

## 1.0 — knowledge base, more exercises/plugins

| Feature | Status |
|---|---|
| More EarTrainer exercises (stereo width, delay type, distortion type) | ⏳ |
| LearnerVerb, LearnerSat | ⏳ |
| In-plugin contextual tooltips beyond one-liners (LearnerEQ and LearnerComp both have one-liners today) | ⏳ |
| Micro-lessons (step-by-step guided parameter changes with explanation) | ⏳ |
| Glossary with audio examples | ⏳ |
| Golden-file audio regression tests for critical DSP chains | ⏳ |
| Packaging/installer, code signing, licensing | ⏳ |

## 2.0 — AI assistant, B2B

| Feature | Status |
|---|---|
| Synthetic dataset generator (dry/wet pairs with known processing) | ⏳ |
| Reference-track analysis model (detect likely EQ/comp/reverb applied) | ⏳ |
| "Try this on LearnerEQ/LearnerComp" suggestion flow from analysis results | ⏳ |
| B2B: school/studio accounts, progress tracking, LMS integration | ⏳ |
| Sales site + payment integration | ⏳ |

## What's explicitly out of scope for now

Anything in the 2.0 row above is a substantial separate effort (model
training pipeline, hosting, a completely different kind of infra) and
shouldn't be started opportunistically alongside plugin/game work — it
needs its own planning pass when the earlier phases are further along.
