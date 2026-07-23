# Ear Trainer — project notes

Ear-training JUCE plugin (VST3/AU/Standalone). Long-term roadmap has three
phases: (1) this plugin with multiple ear-training exercises, (2) an AI
module that detects processing (EQ/comp/reverb/etc.) on a reference track,
(3) packaging/licensing/sales site. Currently in phase 1, two exercises
implemented.

## Build

CMake + JUCE via `FetchContent` (pinned to tag `8.0.15` in
`CMakeLists.txt` — no local JUCE checkout needed). `cmake -B build &&
cmake --build build`.

## Architecture

Every exercise implements a common `Game` interface
(`Source/Games/Game.h`): play a processed test signal, offer N labeled
choices, score the player's pick. This lets one generic editor and one
`GameManager` drive every exercise — see `docs/architecture.md` for the
full rationale and the plan this was built from.

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
- `Source/GameManager.{h,cpp}` — owns all registered `Game`s
  (`juce::OwnedArray<Game>`), tracks the active one, prepares *all* games
  up front in `prepare()` (not just the active one) so switching games
  never needs an audio-thread re-prepare. `process()`/the processor only
  ever talk to whichever game is active.
- `Source/PluginProcessor.{h,cpp}` — `juce::AudioProcessor`. Ignores host
  input audio entirely; generates its own test signal via
  `GameManager::process`.
- `Source/PluginEditor.{h,cpp}` — fully generic: a `ComboBox` game
  selector (from `GameManager::getGameNames()`), a row of choice buttons
  rebuilt to `getNumChoices()` whenever the active game changes,
  instructions/feedback/score labels bound to the active game's getters.
  Re-subscribes its `ChangeListener` to whichever game is active. No
  per-game editor code.

Adding a new exercise: create `Source/Games/NewGame.{h,cpp}` implementing
`Game`, register it in `GameManager`'s constructor, add the two files to
`CMakeLists.txt`. No processor/editor changes needed.

Score is in-memory only — no persistence yet (planned: `juce::PropertiesFile`).

## Conventions

- JUCE house style formatting (space before parens: `if (x)`, not `if(x)`).
- No parameter persistence / `AudioProcessorValueTreeState` yet — nothing
  needs automation in the MVP.

## Roadmap (not yet built)

- More exercises (reverb type, delay type, stereo width, ...) — just
  implement `Game` and register with `GameManager`.
- Score persistence via `juce::PropertiesFile`.
- GitHub Actions CI building Win/Mac artifacts on tag push.
- Phase 2 (AI detector) and phase 3 (licensing/sales site) are unstarted;
  see prior conversation history for the full plan if picked up later.
