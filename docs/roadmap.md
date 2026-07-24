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
| LearnerVerb: Room/Hall/Plate (`juce::dsp::Reverb`) + Spring (custom allpass cascade) via one `ReverbEngine`, pre-delay, waveform visualization, 4 teaching presets (see [decisions/004](decisions/004-learnerverb-scope.md)) | ✓ — **scope trimmed**: no impulse-response "cloud," no decay-vs-frequency graph, no stereo correlometer/vectorscope. Just the live wet/dry waveform + peak meters, matching LearnerComp's shape. Those three visualizations are each a real feature in their own right, not something to bolt on by default — see the ADR. |
| Unit tests: `LearnerVerbTest` (behavioral/smoke: tail persists after input stops, dryWet=0 is exact passthrough, every type produces sound, preset application) — no closed-form target the way compression has | ✓ |
| Shared visualization component factored out for reuse across Learner plugins | ✓ `shared/WaveformDisplay.{h,cpp}`, extracted from LearnerComp once LearnerVerb needed the identical shape. `SpectrumAnalyserComponent` (LearnerEQ) is still separate — it's FFT-based, a fundamentally different data shape from the time-domain peak-tracking the other two share, so there's nothing to unify there. |
| Micro-lessons: `MicroLesson`/`LessonController` (see [decisions/005](decisions/005-microlesson-architecture.md)), one lesson per Learner plugin (Vocal EQ Basics / Vocal Compression / Space for Vocals), a "Lesson" button in each editor | ✓ — per-control UI highlighting was cut (the parameter's own `SliderAttachment`/`ComboBoxAttachment` already makes the knob visibly move when a step sets it, which does the same job), see the ADR |
| Unit tests: `MicroLessonTest` (step navigation state machine — pure logic, no APVTS/GUI needed) | ✓ |
| Unified visualization across Learner plugins: `shared/SpectrumAnalyzer` (extracted from LearnerEQ), Waveform+Spectrum in all three, Bypass/A-B toggle in all three next to Lesson (see [decisions/006](decisions/006-unified-visualization.md)) | ✓ |
| Integration-level tests (editor button clicks -> GameManager state, or a lesson step actually landing on the right APVTS values through `LessonController`) | ⏳ |

## 1.0 — knowledge base, more exercises/plugins

| Feature | Status |
|---|---|
| More EarTrainer exercises (stereo width, delay type, distortion type) | ⏳ |
| LearnerSat | ⏳ |
| LearnerVerb: impulse-response visualization, decay-vs-frequency graph, stereo correlometer/vectorscope | ⏳ trimmed from the initial LearnerVerb build, see [decisions/004](decisions/004-learnerverb-scope.md) |
| In-plugin contextual tooltips beyond one-liners (LearnerEQ, LearnerComp, and LearnerVerb all have one-liners today) | ⏳ |
| Per-control lesson-step highlighting (beyond the moving-knob cue) | ⏳ trimmed from the initial MicroLesson build, see [decisions/005](decisions/005-microlesson-architecture.md) |
| More lessons per plugin (each Learner plugin has exactly one today) | ⏳ |
| Glossary with audio examples | ⏳ |
| Golden-file audio regression tests for critical DSP chains | ⏳ |
| Packaging/installer, code signing, licensing | ⏳ |

## 2.0 — AI assistant, B2B

| Feature | Status |
|---|---|
| Synthetic dataset generator (dry/wet pairs with known processing) | ⏳ |
| Reference-track analysis model (detect likely EQ/comp/reverb applied) | ⏳ |
| "Try this on LearnerEQ/LearnerComp/LearnerVerb" suggestion flow from analysis results | ⏳ |
| B2B: school/studio accounts, progress tracking, LMS integration | ⏳ |
| Sales site + payment integration | ⏳ |

## What's explicitly out of scope for now

Anything in the 2.0 row above is a substantial separate effort (model
training pipeline, hosting, a completely different kind of infra) and
shouldn't be started opportunistically alongside plugin/game work — it
needs its own planning pass when the earlier phases are further along.
