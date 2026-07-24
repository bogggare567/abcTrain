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
| CI: build + run tests on push/PR | 🚧 Windows confirmed green; Ubuntu OOM'd on unbounded `make -j`, fix pushed unverified; macOS unconfirmed |

## Beta — round out the trainer, second teaching plugin

| Feature | Status |
|---|---|
| Score persistence (`juce::PropertiesFile`) | ⏳ |
| LearnerComp (compressor: envelope visualization, gain-reduction meter, A/B) | ⏳ |
| Shared visualization/hint components factored out for reuse across Learner plugins | ⏳ |
| Integration-level tests (editor button clicks -> GameManager state) | ⏳ |

## 1.0 — knowledge base, more exercises/plugins

| Feature | Status |
|---|---|
| More EarTrainer exercises (stereo width, delay type, distortion type) | ⏳ |
| LearnerVerb, LearnerSat | ⏳ |
| In-plugin contextual tooltips beyond one-liners (LearnerEQ has the one-liner today) | ⏳ |
| Micro-lessons (step-by-step guided parameter changes with explanation) | ⏳ |
| Glossary with audio examples | ⏳ |
| Adaptive difficulty | ⏳ |
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
