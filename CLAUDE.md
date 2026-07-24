# Ear Trainer / Learner EQ — project notes

Two JUCE plugins in one repo/CMake build, both VST3/AU/Standalone:

- **EarTrainer** — multiple-choice ear-training games (existing).
- **LearnerEQ** — a real 4-band EQ that processes the host's own audio,
  with a live spectrum + response-curve display and short contextual
  tooltips while dragging a band's frequency knob (new).

Longer-term product direction (not all built): a learning ecosystem of
"teaching" plugins (LearnerEQ done, LearnerComp/LearnerVerb/LearnerSat
planned) paired with the trainer games and a lightweight in-plugin
knowledge base, plus a phase-2 AI module that analyzes a reference track
and suggests which teaching plugins to try. Treat any specific
market-size/competitor claims from prior planning conversations as
unverified marketing copy, not fact — re-check before making decisions
that depend on them.

## Docs directory

`docs/roadmap.md` (status of everything, done vs. planned),
`docs/diagrams/` (mermaid: system overview, game-engine class diagram,
learner-plugin component diagram, proposed CI pipeline),
`docs/decisions/` (ADRs: 001 is the `Game` interface choice, 002 is
`setDifficulty`/`ProgressManager`), `docs/testing-strategy.md`. This file
(`CLAUDE.md`) stays the per-file breakdown; `docs/` is the higher-level/
visual layer — keep both in sync when the architecture changes rather
than letting one drift.

## Build

CMake + JUCE via `FetchContent` (pinned to tag `8.0.15` in
`CMakeLists.txt` — no local JUCE checkout needed). `cmake -B build &&
cmake --build build` builds both plugin targets (`EarTrainer`,
`LearnerEQ`) from the one root `CMakeLists.txt`.

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

Not yet built for LearnerEQ: "analyze reference" mode, knowledge-base
tooltips beyond the one-line frequency description, micro-lessons.

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
`Source/PluginProcessor.cpp` (EarTrainer's, not LearnerEQ's): both it and
`LearnerEQ/Source/PluginProcessor.cpp` define `createPluginFilter()`, and
linking both into one binary would collide. The game logic under test
doesn't need the `AudioProcessor` wrapper anyway — only `LearnerEQTest`
needs a real `AudioProcessor`, and it gets one from LearnerEQ.

CI: `.github/workflows/build_and_test.yml` builds all targets and runs
`EarTrainerTests` on push/PR across ubuntu-latest/macos-latest/
windows-latest. **Confirmed green on all three as of commit `a2f2944`.**
Two real bugs were caught and fixed getting here (see
`docs/diagrams/ci-pipeline.md` for both) — this was the first actual
compile+run this codebase had ever gotten, so treat that history as a
reminder to keep watching CI on every push, not evidence the code is now
bulletproof.

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
- More teaching plugins: LearnerComp, LearnerVerb, LearnerSat — same
  pattern as LearnerEQ (own `juce_add_plugin` target, APVTS params, a
  visualization + contextual guide text).
- LearnerEQ "analyze reference" mode + richer knowledge base/micro-lessons.
- Integration test for the real `Game → ProgressManager` `ChangeListener`
  wiring (needs a pumped message loop, not set up yet — see Testing).
- Phase 2 (AI detector) and phase 3 (licensing/sales site/B2B) are
  unstarted; see prior conversation history for the full plan if picked
  up later.
