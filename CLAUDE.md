# Ear Trainer / Learner EQ — project notes

Two JUCE plugins in one repo/CMake build, both VST3/AU/Standalone:

- **EarTrainer** — multiple-choice ear-training games (existing).
- **LearnerEQ** — a real 4-band EQ that processes the host's own audio,
  with a live spectrum + response-curve display and short contextual
  tooltips while dragging a band's frequency knob (new).

Longer-term product direction (not all built): a learning ecosystem of
"teaching" plugins (EQ done, compressor/reverb/saturation planned) paired
with the trainer games and a lightweight in-plugin knowledge base, plus a
phase-2 AI module that analyzes a reference track and suggests which
teaching plugins to try. Treat any specific market-size/competitor claims
from prior planning conversations as unverified marketing copy, not fact
— re-check before making decisions that depend on them.

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
  `newRound`/`submitAnswer(int)`, `getNumChoices`/`getChoiceLabel(int)`,
  answer/feedback getters, score getters. Is a `juce::ChangeBroadcaster`.
- `Source/Games/EQGame.{h,cpp}` — "guess the band": pink noise through an
  `IIR` peak filter, random octave band (100 Hz–12.8 kHz, 8 choices),
  random 9 dB boost/cut.
- `Source/Games/CompressionGame.{h,cpp}` — "guess the compression":
  repeating percussive noise burst through `juce::dsp::Compressor` at one
  of 3 fixed threshold/ratio presets (weak/medium/strong), with a fixed
  makeup-gain compensation per preset (tuned by ear, not measured) so
  loudness alone isn't a tell.
- `Source/PinkNoiseGenerator.h` — shared pink-noise source (Paul Kellet
  economy algorithm) used by both games above.
- `Source/GameManager.{h,cpp}` — owns all registered `Game`s, tracks the
  active one, prepares *all* games up front in `prepare()` so switching
  games never needs an audio-thread re-prepare.
- `Source/PluginProcessor.{h,cpp}` — ignores host input entirely;
  generates its own test signal via `GameManager::process`.
- `Source/PluginEditor.{h,cpp}` — fully generic: `ComboBox` game selector,
  choice buttons rebuilt to `getNumChoices()` on switch, no per-game
  editor code.

Adding a new exercise: create `Source/Games/NewGame.{h,cpp}` implementing
`Game`, register it in `GameManager`'s constructor, add the two files to
`CMakeLists.txt`. No processor/editor changes needed.

EarTrainer score is in-memory only — no persistence yet (planned:
`juce::PropertiesFile`).

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

## Conventions

- JUCE house style formatting (space before parens: `if (x)`, not `if(x)`).
- EarTrainer has no parameter persistence (nothing needs automation in a
  game). LearnerEQ does, via APVTS — don't add raw member-variable state
  for anything a user should be able to automate or that should survive
  session reload; add it as an APVTS parameter instead.

## Roadmap (not yet built)

- More EarTrainer exercises (reverb type, delay type, stereo width, ...).
- More teaching plugins: LearnerComp, LearnerVerb, LearnerSat — same
  pattern as LearnerEQ (own `juce_add_plugin` target, APVTS params, a
  visualization + contextual guide text).
- Score persistence via `juce::PropertiesFile`.
- LearnerEQ "analyze reference" mode + richer knowledge base/micro-lessons.
- GitHub Actions CI building Win/Mac artifacts on tag push.
- Phase 2 (AI detector) and phase 3 (licensing/sales site/B2B) are
  unstarted; see prior conversation history for the full plan if picked
  up later.
