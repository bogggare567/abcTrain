# Ear Trainer — project notes

Ear-training JUCE plugin (VST3/AU/Standalone). Long-term roadmap has three
phases: (1) this plugin with multiple ear-training exercises, (2) an AI
module that detects processing (EQ/comp/reverb/etc.) on a reference track,
(3) packaging/licensing/sales site. Currently in phase 1, MVP stage.

## Build

CMake + JUCE via `FetchContent` (pinned to tag `8.0.15` in
`CMakeLists.txt` — no local JUCE checkout needed). `cmake -B build &&
cmake --build build`.

## Architecture

- `Source/PluginProcessor.{h,cpp}` — `juce::AudioProcessor`. Ignores host
  input audio entirely; generates its own test signal. Owns one `EQGame`
  instance and forwards `prepareToPlay`/`processBlock` to it.
- `Source/EQGame.{h,cpp}` — the exercise itself: pink noise generator
  (Paul Kellet economy algorithm) run through a `juce::dsp::IIR` peak
  filter at a random octave band (100 Hz–12.8 kHz, 8 bands) with a random
  boost/cut of 9 dB. Is a `juce::ChangeBroadcaster`; call `newRound()` /
  `submitAnswer(bandIndex)`, listen via `ChangeListener` for UI updates.
  Score is in-memory only — no persistence yet.
- `Source/PluginEditor.{h,cpp}` — band buttons + score/feedback labels,
  listens to `EQGame` change messages, no polling/`Timer` needed.

There is currently no `GameManager` abstraction — `EQGame` is owned
directly by the processor. Add a manager only when a second exercise
(compression, reverb type, etc.) actually needs to be switched between at
runtime; don't pre-build the abstraction before that's true.

## Conventions

- JUCE house style formatting (space before parens: `if (x)`, not `if(x)`).
- No parameter persistence / `AudioProcessorValueTreeState` yet — nothing
  needs automation in the MVP.

## Roadmap (not yet built)

- More exercises: compression-strength guesser, reverb-type guesser →
  will need the `GameManager` mentioned above.
- Score persistence via `juce::PropertiesFile`.
- GitHub Actions CI building Win/Mac artifacts on tag push.
- Phase 2 (AI detector) and phase 3 (licensing/sales site) are unstarted;
  see prior conversation history for the full plan if picked up later.
