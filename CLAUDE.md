# Ear Trainer / Learner EQ / Learner Comp / Learner Verb — project notes

Four JUCE plugins in one repo/CMake build, all VST3/AU/Standalone:

- **EarTrainer** — multiple-choice ear-training games.
- **LearnerEQ** — a real 4-band EQ that processes the host's own audio,
  with a live spectrum + response-curve display and short contextual
  tooltips while dragging a band's frequency knob.
- **LearnerComp** — a real compressor processing the host's own audio,
  with a scrolling waveform highlighting where it's reducing gain, a
  GR/peak meter row, contextual tooltips, and 4 teaching presets.
- **LearnerVerb** — a real reverb (Room/Hall/Plate/Spring) processing the
  host's own audio, the same scrolling waveform/peak-meter view as
  LearnerComp, contextual tooltips, and 4 teaching presets (new).

Longer-term product direction (not all built): a learning ecosystem of
"teaching" plugins (LearnerEQ/LearnerComp/LearnerVerb done, LearnerSat
planned) paired with the trainer games and a lightweight in-plugin
knowledge base, plus a phase-2 AI module that analyzes a reference track
and suggests which teaching plugins to try. Treat any specific
market-size/competitor claims from prior planning conversations as
unverified marketing copy, not fact — re-check before making decisions
that depend on them.

## Docs directory

`docs/roadmap.md` (status of everything, done vs. planned),
`docs/diagrams/` (mermaid: system overview, game-engine class diagram,
learner-plugin component diagrams for LearnerEQ/LearnerComp/LearnerVerb,
proposed CI pipeline), `docs/decisions/` (ADRs: 001 is the `Game`
interface choice, 002 is `setDifficulty`/`ProgressManager`, 003 is why
LearnerComp has a custom compressor engine instead of
`juce::dsp::Compressor`, 004 is LearnerVerb's trimmed visualization scope
and its decay-to-`roomSize` approximation, 005 is the `MicroLesson`/
`LessonController` split and why per-control highlighting was cut),
`docs/testing-strategy.md`. This file (`CLAUDE.md`) stays the per-file
breakdown; `docs/` is the higher-level/visual layer — keep both in sync
when the architecture changes rather than letting one drift.

## Build

CMake + JUCE via `FetchContent` (pinned to tag `8.0.15` in
`CMakeLists.txt` — no local JUCE checkout needed). `cmake -B build &&
cmake --build build` builds all four plugin targets (`EarTrainer`,
`LearnerEQ`, `LearnerComp`, `LearnerVerb`) from the one root
`CMakeLists.txt`.

## Architecture — EarTrainer (`Source/`)

Every exercise implements a common `Game` interface
(`Source/Games/Game.h`): play a processed test signal, offer N labeled
choices, score the player's pick. This lets one generic editor and one
`GameManager` drive every exercise — see `docs/architecture.md` for the
full rationale.

- `Source/Games/Game.h` — the interface: `prepare`/`process`,
  `setDifficulty(int level)` (1-10, see ADR 002), `newRound`/
  `submitAnswer(int)`, `getNumChoices`/`getChoiceLabel(int)`, answer/
  feedback getters, score getters. Is a `juce::ChangeBroadcaster`.
- `Source/Games/EQGame.{h,cpp}` — "guess the band": pink noise through an
  `IIR` peak filter, random octave band (100 Hz–12.8 kHz, 8 choices).
  `setDifficulty` scales the boost/cut amount: 9 dB (levels 1-3) → 6 dB
  (4-6) → 3 dB (7-10). Choice count never changes.
- `Source/Games/CompressionGame.{h,cpp}` — "guess the compression":
  repeating percussive noise burst through `juce::dsp::Compressor` at one
  of 3 threshold/ratio presets (weak/medium/strong), with a fixed
  makeup-gain compensation per preset (tuned by ear, not measured) so
  loudness alone isn't a tell. `setDifficulty` swaps between 3 whole
  preset tables (`easyPresets`/`mediumPresets`/`hardPresets`) — same 3
  labels throughout, but the threshold/ratio values converge at higher
  tiers so "Weak" vs. "Strong" gets subtler. Choice count never changes.
- `Source/Games/ReverbGame.{h,cpp}` — "guess the reverb type": repeating
  percussive noise burst (period long enough for tails to decay audibly)
  through one of up to 4 types. Room/Hall/Plate are `juce::dsp::Reverb`
  (Freeverb-derived) with different roomSize/damping/width presets — an
  approximation tuned by ear, not physically modeled per type, same
  approach as `CompressionGame`'s presets. Spring is built separately as a
  cascade of 4 resonant allpass `IIR` filters, since Freeverb-style
  algorithms don't produce the metallic comb/allpass "boing" character a
  spring tank has. `setDifficulty` changes `getNumChoices()` itself: 2
  (Room/Hall only) at levels 1-3, 3 (+Plate) at 4-6, all 4 (+Spring) at
  7-10 — the only game where difficulty changes the choice count, which
  is why `PluginEditor::refreshFromGameState()` has to handle a
  mid-session choice-count change (see ADR 002).
- `Source/PinkNoiseGenerator.h` — shared pink-noise source (Paul Kellet
  economy algorithm) used by all three games above.
- `Source/GameManager.{h,cpp}` — owns all registered `Game`s, tracks the
  active one, prepares *all* games up front in `prepare()` so switching
  games never needs an audio-thread re-prepare. Also exposes
  `getNumGames()`/`getGame(int)` (so `ProgressManager` can listen to every
  game, not just the active one) and `setDifficultyForAllGames(int)`.
- `Source/ProgressManager.{h,cpp}` — cross-session points/level(1-10)/
  streak/daily-challenge, backed by `juce::PropertiesFile`. Listens to
  every game via `ChangeListener`; on a correct/incorrect answer it calls
  its own `registerAnswer(gameIndex, wasCorrect)`, which is also the
  direct entry point `ProgressManagerTest` uses (see the Testing section
  below for why). On level-up, calls
  `gameManager.setDifficultyForAllGames(level)`. Points-to-next-level is a
  triangular scale (level *L* needs *100·L* points to reach *L+1*, so
  each level is progressively harder). Games themselves know nothing
  about points or levels — kept out of the `Game` interface deliberately,
  see ADR 002 for the one thing that *did* need to go in (`setDifficulty`).
- `Source/PluginProcessor.{h,cpp}` — ignores host input entirely;
  generates its own test signal via `GameManager::process`. Owns
  `GameManager` then `ProgressManager` in that declaration order (matters
  — `ProgressManager`'s constructor registers listeners on every game).
- `Source/PluginEditor.{h,cpp}` — fully generic: `ComboBox` game selector,
  choice buttons rebuilt to `getNumChoices()` on switch *or* whenever a
  fresh round's choice count no longer matches the current button count
  (needed once `ReverbGame`'s choice count became difficulty-dependent),
  no per-game editor code. Also shows level/progress-bar/streak/daily-
  challenge from `ProgressManager`.

Adding a new exercise: create `Source/Games/NewGame.{h,cpp}` implementing
`Game` (including a real `setDifficulty` — there's no default), register
it in `GameManager`'s constructor, add the two files to `CMakeLists.txt`.
No processor/editor changes needed.

Each game's own `getScore()`/`getRoundsPlayed()` (the "Score: X / Y" label
per game) stay in-memory-only, session-scoped counters — unrelated to
`ProgressManager`'s points/level, which persist via `PropertiesFile`.

## Architecture — LearnerEQ (`LearnerEQ/Source/`)

Unlike EarTrainer, this processes the host's real audio and is a genuinely
usable EQ, not a game — parameters are exposed via
`juce::AudioProcessorValueTreeState` so they're host-automatable and save/
restore with the session (`getStateInformation`/`setStateInformation`).

- `LearnerEQ/Source/EQCoefficients.h` — maps band index → filter type
  (band 0 = low shelf, band 3 = high shelf, bands 1–2 = bell) and builds
  `IIR::Coefficients` from freq/gain/Q. Shared by the processor (real
  filtering) and the editor (drawing the response curve), so they can
  never disagree about what a band does.
- `LearnerEQ/Source/FrequencyGuide.h` — the log-frequency ⟷ normalised-x
  mapping used by *all three* overlays (live spectrum, response curve,
  highlighted-band region) so they line up on screen, plus the short
  plain-language descriptions shown per frequency range while dragging.
- `LearnerEQ/Source/PluginProcessor.{h,cpp}` — 4 `ProcessorDuplicator`
  filters run in series on the real audio block; recomputes coefficients
  from current APVTS values once per block (not per-sample). Feeds a
  mono-summed copy of each sample to whatever `SpectrumAnalyserComponent`
  the editor registered via `setSpectrumAnalyser` (a raw
  `std::atomic<SpectrumAnalyserComponent*>`, null-checked, since the
  editor can be closed while the processor keeps running).
- `LearnerEQ/Source/SpectrumAnalyser.{h,cpp}` — standard JUCE FFT-analyser
  pattern (fixed FIFO filled from the audio thread via
  `pushNextSampleIntoFifo`, FFT run on a 30 Hz UI timer) plus two overlays
  drawn on top: the combined 4-band response curve (via
  `EQCoefficients::make` + `Coefficients::getMagnitudeForFrequency`), and
  a translucent highlighted region for whichever band is currently being
  dragged.
- `LearnerEQ/Source/PluginEditor.{h,cpp}` — 4 columns of freq/gain/Q
  rotary sliders bound with `SliderAttachment`. `onDragStart`/
  `onValueChange`/`onDragEnd` on each band's freq slider drive the guide
  label text (via `FrequencyGuide::describe`) and
  `spectrum.setHighlightedBand`. A 30 Hz editor timer pushes current
  parameter values into the spectrum component so the response curve
  tracks knob movement even with no audio playing.
- `LearnerEQ/Source/PluginEntry.cpp` — just `createPluginFilter()`,
  deliberately split out of `PluginProcessor.cpp` (see the Testing section
  below for why).
- `LearnerEQ/Source/VocalEqLesson.h` — `buildVocalEqLesson()`, one
  `MicroLesson` (flat → boost 3 kHz presence → cut 250 Hz mud → boost
  10 kHz air → compare) driving a "Lesson" button/`LessonController`
  overlay in the editor. See the Microlessons section below.

Not yet built for LearnerEQ: "analyze reference" mode, knowledge-base
tooltips beyond the one-line frequency description, more than one lesson.

## Architecture — LearnerComp (`LearnerComp/Source/`)

Same shape as LearnerEQ (real audio, APVTS parameters, host-automatable)
but for dynamics instead of frequency. See
[decisions/003-learnercomp-engine.md](docs/decisions/003-learnercomp-engine.md)
for why it doesn't use `juce::dsp::Compressor`.

- `LearnerComp/Source/CompressorEngine.h` — custom feed-forward
  compressor: soft-knee gain computer (Giannoulis/Massberg/Reiss formula)
  plus one-pole attack/release smoothing on the gain-reduction envelope
  itself (not on the input level). `computeGain(detectionSample)` returns
  a gain factor rather than a processed sample, so the processor can feed
  it a stereo-linked detection value (loudest channel) and apply the
  *same* gain to every channel, avoiding stereo-image pumping.
  `getLastGainReductionDb()` exposes the smoothed envelope for the meter
  and waveform highlight — the whole reason this isn't `juce::dsp::Compressor`,
  which exposes neither knee nor its internal gain reduction.
- `LearnerComp/Source/ParameterGuide.h` (`CompressorGuide` namespace) —
  tooltip text per parameter ID, plus the 4 preset definitions (Vocal
  Smoothing/Punchy Drums/Bass Control/Limiter: threshold, ratio, attack,
  release, knee).
- `LearnerComp/Source/PluginProcessor.{h,cpp}` — 8 APVTS params
  (threshold/ratio/attack/release/knee/makeup/dryWet/bypass).
  `processBlock` computes one stereo-linked detection value per sample,
  gets a gain from `CompressorEngine`, applies it to every channel with
  dry/wet blending, and (unless bypassed) pushes (input, output,
  gainReductionDb) to whatever `WaveformDisplay` the editor registered.
  Bypass skips the engine entirely and passes audio through unchanged.
  `applyPreset(int)` lives here (not just in the editor's button handler)
  specifically so it's unit-testable without constructing a `Component`.
- `shared/WaveformDisplay.{h,cpp}` — same FIFO-accumulate/30 Hz-timer-flush
  pattern as `SpectrumAnalyserComponent`, but for a scrolling peak-based
  dual waveform instead of an FFT: gray input trace, output trace tinted
  from blue to red proportional to a generic `highlightAmount` (LearnerComp
  passes gain reduction) in that ~33 ms column. Also the source of the
  peak-meter readouts (via `getInputPeak`/`getOutputPeak`/
  `getCurrentHighlightAmount`). Shared with LearnerVerb — see the
  LearnerVerb section below.
- `LearnerComp/Source/PluginEditor.{h,cpp}` — 7 rotary knobs (one per
  float param) + bypass toggle + 4 preset buttons, each preset button just
  calling `processor.applyPreset(i)`. Guide label updates via
  `onDragStart`/`onDragEnd` on each knob, same pattern as LearnerEQ.
- `LearnerComp/Source/PluginEntry.cpp` — just `createPluginFilter()`, same
  reason as LearnerEQ's.
- `LearnerComp/Source/VocalCompressionLesson.h` — `buildVocalCompressionLesson()`,
  one `MicroLesson` (bypass → threshold -18 dB → ratio 3:1 → attack 5 ms →
  release 150 ms → makeup +4 dB).

Not yet built for LearnerComp: knowledge-base content beyond the one-line
tooltips, more than one lesson, any automated coverage of attack/release
*transient* behavior (only steady-state math is tested).

## Architecture — LearnerVerb (`LearnerVerb/Source/`)

Same shape again (real audio, APVTS parameters, host-automatable), for
space instead of dynamics. See
[decisions/004-learnerverb-scope.md](docs/decisions/004-learnerverb-scope.md)
for what was deliberately cut from the first build and why the `Decay`
knob isn't a precise physical measurement.

- `LearnerVerb/Source/ReverbEngine.h` — Room/Hall/Plate via
  `juce::dsp::Reverb` (Freeverb-derived); Spring via a cascade of 4
  resonant allpass filters, the same technique `EarTrainer`'s `ReverbGame`
  uses for its Spring type, reimplemented here (not literally shared —
  `ReverbGame`'s version is tightly coupled to its per-round game model,
  this one needs continuous live parameter control). `Decay` (seconds) is
  mapped onto `roomSize` by ear, since Freeverb has no literal
  decay-in-seconds parameter — an approximation, not a physical model,
  same "tuned, not measured" precedent as `CompressionGame`/`ReverbGame`'s
  presets. A `juce::dsp::DelayLine` implements pre-delay ahead of whichever
  algorithm is selected. Always renders 100% wet; `PluginProcessor` blends
  dry/wet itself, same division of responsibility as `CompressorEngine`.
- `LearnerVerb/Source/ReverbGuide.h` (`ReverbGuide` namespace) — tooltip
  text per parameter ID, plus the 4 preset definitions (Vocal Ambience/
  Concert Hall/Small Room/Spring Tank: type, decay, pre-delay, size,
  damping, dry/wet, width).
- `LearnerVerb/Source/PluginProcessor.{h,cpp}` — 7 APVTS params (type as
  an `AudioParameterChoice`, decay/preDelay/size/damping/dryWet/width as
  floats). `processBlock` makes a wet-only copy of the block
  (`wetBuffer.makeCopyOf`), runs `ReverbEngine::process` on the copy, then
  blends wet/dry per sample into the real buffer — `ReverbEngine` never
  needs to know about `Dry/Wet` at all. Pushes (dry, blended-output) to
  whatever `WaveformDisplay` the editor registered, with `highlightAmount`
  left at its default (no gain-reduction-style concept here).
  `applyPreset(int)` lives here for the same testability reason as
  LearnerComp's.
- `LearnerVerb/Source/PluginEditor.{h,cpp}` — a `ComboBox` for Type
  (`ComboBoxAttachment`, items added manually to match the choice
  parameter — attachments don't auto-populate the combo box) + 6 rotary
  knobs + 4 preset buttons. Same guide-label/tooltip pattern as the other
  two Learner plugins.
- `LearnerVerb/Source/PluginEntry.cpp` — just `createPluginFilter()`, same
  reason as the other two.
- `LearnerVerb/Source/VocalSpaceLesson.h` — `buildVocalSpaceLesson()`, one
  `MicroLesson` (dry → Plate 1.5 s/20% wet → pre-delay 40 ms → damping
  70% → compare with Hall 2.5 s).

Not yet built for LearnerVerb (deliberately trimmed, see ADR 004):
impulse-response "cloud" visualization, decay-vs-frequency graph, stereo
correlometer/vectorscope, knowledge-base content beyond one-line tooltips,
more than one lesson.

## Architecture — Microlessons (`shared/`, per-plugin lesson content)

See [decisions/005-microlesson-architecture.md](docs/decisions/005-microlesson-architecture.md)
for the full rationale; summary here.

- `shared/MicroLesson.h` — pure state machine, no APVTS/UI dependency:
  a title, a `std::vector<LessonStep>` (`explanationText` +
  `(parameterID, value)` pairs, a plain aggregate), and a step cursor
  (`start`/`nextStep`/`previousStep`). This is what `tests/MicroLessonTest.cpp`
  exercises directly, with none of the message-loop/GUI-instantiation
  concerns documented in the Testing section below.
- `shared/LessonController.{h,cpp}` — the only thing that touches APVTS or
  draws anything. A `juce::Component` owning one `MicroLesson` and an
  `AudioProcessorValueTreeState&`; on every step change it calls
  `setValueNotifyingHost` for that step's target parameters (same pattern
  `applyPreset` already uses) and updates its text/progress labels.
  Meant to be added as a full-size child of a Learner editor and toggled
  visible via a "Lesson" button — every editor's `resized()` sets its
  bounds to `getLocalBounds()` unconditionally, whether visible or not.
- Lesson **content** (the three `build...Lesson()` files listed in each
  plugin's section above) lives per-plugin, not in `shared/` — only the
  machinery is shared, since the content is inherently tied to that
  plugin's own parameter IDs. Same reasoning as `CompressorGuide`/
  `ReverbGuide`'s preset tables.
- **Per-control highlighting was cut from this pass.** Every target
  parameter already has a `SliderAttachment`/`ComboBoxAttachment`, so
  setting it via `LessonController` makes the matching knob visibly move
  on its own — that motion is the highlight. No highlight-drawing code
  was added to any of the three editors for this.

## Testing (`tests/`, `shared/`)

`EarTrainerTests` is a plain console app (`juce_add_console_app`, not a
plugin) built from the same root `CMakeLists.txt`:
`cmake --build build --target EarTrainerTests`, then run the produced
binary directly — it uses `juce::UnitTestRunner` and exits non-zero on any
failure. It compiles the game/processor source files directly (not the
plugin targets), so no plugin host or GUI is needed to run it.

- `shared/TestUtils.h` — `generateSineBuffer`/`rms` helpers for
  audio-domain assertions.
- `tests/EQGameTest.cpp`, `tests/CompressionGameTest.cpp`,
  `tests/ReverbGameTest.cpp`, `tests/GameManagerTest.cpp` — logic-level:
  scoring, answer/round state transitions, choice-count/label contracts,
  and (for all three games) that `setDifficulty` at each tier still plays
  a valid round. Deliberately don't assert on actual audio content (the
  games generate random noise), since that would be either flaky or
  trivial — `ReverbGameTest` is the partial exception, it does check the
  output buffer isn't silent (a decent smoke test given the type-specific
  DSP paths: `dsp::Reverb` vs. the allpass cascade). Note `ReverbGame`
  now defaults to the easy tier (2 choices) *before* `setDifficulty` is
  ever called, matching the other two games' easy-tier defaults — tests
  that want all 4 types must call `setDifficulty(10)` first.
- `tests/ProgressManagerTest.cpp` — level/points math, streak, daily
  challenge, and a persistence round-trip, all via `registerAnswer`/
  `updateStreakForDate`/`generateDailyChallengeForDate` called directly
  rather than through the real `ChangeListener` wiring (see below).
- `tests/LearnerEQTest.cpp` — the one test that touches real DSP output:
  boosts a band via `apvts.getRawParameterValue(...)->store(...)` and
  checks measured RMS actually goes up at that frequency. This is the
  kind of check that would have caught a broken filter chain, which
  mattered here because LearnerEQ's DSP code could not be compiled/run at
  all in the environment it was originally written in.
- `tests/LearnerCompTest.cpp` — same approach: a 0 dBFS sine through a
  real `LearnerCompProcessor` at -6 dB threshold/2:1 ratio/hard knee
  should settle at -3 dBFS; adding +3 dB makeup should bring it back to
  ~0 dBFS (measured on the buffer tail, after the fast attack has
  settled). Also checks bypass leaves the buffer bit-for-bit unchanged,
  `applyPreset` sets every parameter a preset defines, and an
  out-of-range preset index is a no-op rather than a crash.
- `tests/LearnerVerbTest.cpp` — reverb has no clean closed-form target the
  way compression math does, so this is behavioral instead: a noise burst
  through a real `LearnerVerbProcessor` should leave an audible tail after
  the burst ends (unlike a dry passthrough, which would be silent);
  `dryWet = 0` should give an exact (not just close) passthrough, since
  `0 * wet + 1 * dry` is exact in floating point; every one of the 4 types
  should produce non-silent output without crashing; `applyPreset` and the
  out-of-range-index guard are tested the same way as LearnerComp's.
- `tests/MicroLessonTest.cpp` — step-navigation state machine tests
  against `MicroLesson` directly: inactive until `start()`, `nextStep`/
  `previousStep` stop at the ends and no-op before `start()`, `stop()`
  deactivates, `getCurrentStep()` exposes the right text/target params.
  No APVTS, no `LessonController`, no `Component` involved at all.

**`juce::ChangeBroadcaster::sendChangeMessage()` is asynchronous** (needs
a running JUCE message loop to deliver), and `EarTrainerTests` never pumps
one — so a test calling `game.submitAnswer(...)` cannot reliably assert
that `ProgressManager` reacted. That's why `ProgressManager` exposes
`registerAnswer(gameIndex, wasCorrect)` as a direct synchronous entry
point (`changeListenerCallback` is a thin wrapper around it) — tests call
that instead. The real `Game → ChangeListener → ProgressManager` wiring is
only exercised by actually running the plugin. See
`docs/testing-strategy.md` for more.

`EarTrainerTests`' `target_sources` intentionally excludes
`Source/PluginProcessor.cpp` (EarTrainer's — the game logic under test
doesn't need the `AudioProcessor` wrapper) and every plugin's
`PluginEntry.cpp` (`LearnerEQ/Source/PluginEntry.cpp`,
`LearnerComp/Source/PluginEntry.cpp`, `LearnerVerb/Source/PluginEntry.cpp`
— each is *just* `createPluginFilter()`, split out of its
`PluginProcessor.cpp` specifically because `LearnerEQTest`,
`LearnerCompTest`, and `LearnerVerbTest` all need their real processor's
`PluginProcessor.cpp` linked in, and three definitions of
`createPluginFilter()` in one binary would collide. The real plugin
targets (`juce_add_plugin`) link both `PluginProcessor.cpp` *and*
`PluginEntry.cpp` — only the test binary needs the split.

CI: `.github/workflows/build_and_test.yml` builds all targets and runs
`EarTrainerTests` on push/PR across ubuntu-latest/macos-latest/
windows-latest. **Confirmed green on all three as of commit `a2f2944`**,
again on `dd207d1` (LearnerComp), and again on `8932b84` (LearnerVerb) —
each checked directly against the GitHub Actions API, not assumed. Two
real bugs were caught and fixed getting to the first green run (see
`docs/diagrams/ci-pipeline.md` for both) — that was the first actual
compile+run this codebase had ever gotten, so treat that history as a
reminder to keep watching CI on every push, not evidence the code is now
bulletproof. **MicroLesson/LessonController was added after `8932b84` and
is not yet confirmed to build/pass** — watch the next CI run on this
branch before trusting it.

## Conventions

- JUCE house style formatting (space before parens: `if (x)`, not `if(x)`).
- EarTrainer's games have no per-parameter host automation (nothing needs
  it in a game). LearnerEQ does, via APVTS — don't add raw member-variable
  state for anything a user should be able to automate or that should
  survive session reload; add it as an APVTS parameter instead.
- Two different persistence mechanisms exist for two different kinds of
  state: `AudioProcessorValueTreeState` (LearnerEQ) for per-*plugin-
  instance* parameters that should save/restore with the host session;
  `juce::PropertiesFile` (`ProgressManager`) for per-*user* progress that
  should persist across every session/instance/project regardless of
  host. Don't mix them up when adding new persisted state.

## Roadmap (not yet built)

- More EarTrainer exercises (delay type, stereo width, distortion type, ...).
- One more teaching plugin: LearnerSat — same pattern as the other three
  (own `juce_add_plugin` target, APVTS params, a visualization +
  contextual guide text, its own `PluginEntry.cpp` split).
- LearnerVerb's trimmed-for-now visualizations: impulse-response "cloud,"
  decay-vs-frequency graph, stereo correlometer/vectorscope (see ADR 004).
- LearnerEQ "analyze reference" mode + richer knowledge base beyond one-
  line tooltips.
- Per-control lesson-step highlighting, trimmed from the initial
  MicroLesson build (see ADR 005) — the moving-knob cue from each step's
  own `SliderAttachment`/`ComboBoxAttachment` stands in for it today.
- More lessons per plugin (each Learner plugin has exactly one today).
- Golden-file / transient-behavior audio regression tests for LearnerEQ,
  LearnerComp, and LearnerVerb (each currently has only steady-state or
  behavioral assertions, no golden-file comparison).
- Integration test for the real `Game → ProgressManager` `ChangeListener`
  wiring (needs a pumped message loop, not set up yet — see Testing), and
  similarly no automated test of `LessonController`'s actual
  APVTS-setting behavior (see ADR 005).
- Phase 2 (AI detector) and phase 3 (licensing/sales site/B2B) are
  unstarted; see prior conversation history for the full plan if picked
  up later.
