# Ear Trainer / Learner EQ / Learner Comp

[![Build and Test](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml)

Three VST3/AU/Standalone plugins built with [JUCE](https://juce.com):

- **Ear Trainer** — ear-training games. It feeds you pink noise through a
  hidden filter and you guess what changed (which band was boosted/cut,
  how strong the compression is, which reverb type). Tracks points, a
  level (1-10, which scales the games' difficulty up as you improve), a
  daily login streak, and a daily challenge.
- **Learner EQ** — a real 4-band EQ for your own tracks, with a live
  spectrum display, a response-curve overlay, and a one-line plain-
  language explanation of what a frequency region does while you drag it.
- **Learner Comp** — a real compressor for your own tracks, with a
  scrolling input/output waveform that highlights in red wherever the
  compressor is actively reducing gain, a live gain-reduction meter,
  input/output peak meters, a one-line explanation per control, and 4
  presets to learn from (Vocal Smoothing, Punchy Drums, Bass Control,
  Limiter).

Long-term direction is a small learning ecosystem — more trainer games,
more "teaching" plugins in the LearnerEQ shape, an in-plugin knowledge
base — see [docs/roadmap.md](docs/roadmap.md) for what's actually planned
vs. built so far.

## Architecture

Dashed boxes below aren't built yet. Full breakdown, plus class- and
component-level diagrams for each part, in
[docs/diagrams/](docs/diagrams/).

```mermaid
flowchart TB
    subgraph EarTrainer["EarTrainer plugin (VST3 / AU / Standalone)"]
        ETProc["PluginProcessor"]
        ETEdit["PluginEditor (generic)"]
        GM["GameManager"]
        GameIface["Game interface"]
        EQGame["EQGame"]
        CompGame["CompressionGame"]
        RevGame["ReverbGame"]
        PM["ProgressManager"]

        ETProc --> GM
        ETProc --> PM
        ETEdit --> GM
        ETEdit --> PM
        GM --> GameIface
        GameIface --> EQGame
        GameIface --> CompGame
        GameIface --> RevGame
        PM --> GM
    end

    subgraph LearnerEQ["LearnerEQ plugin (VST3 / AU / Standalone)"]
        LEProc["PluginProcessor"]
        LEEdit["PluginEditor"]
        Spectrum["SpectrumAnalyserComponent"]
        APVTS[("AudioProcessorValueTreeState")]

        LEProc --> APVTS
        LEEdit --> APVTS
        LEEdit --> Spectrum
    end

    subgraph LearnerComp["LearnerComp plugin (VST3 / AU / Standalone)"]
        LCProc["PluginProcessor"]
        LCEdit["PluginEditor"]
        Engine["CompressorEngine"]
        Waveform["WaveformDisplay"]

        LCProc --> Engine
        LCEdit --> Waveform
    end

    subgraph Tests["EarTrainerTests (console app)"]
        Runner["TestRunner (juce::UnitTestRunner)"]
    end

    LearnerVerb["LearnerVerb plugin"]

    Runner -. "compiles & runs directly" .-> EQGame
    Runner -. "compiles & runs directly" .-> CompGame
    Runner -. "compiles & runs directly" .-> RevGame
    Runner -. "compiles & runs directly" .-> LEProc
    Runner -. "compiles & runs directly" .-> LCProc
    LCProc -.-> LearnerVerb

    classDef planned stroke-dasharray:4 3,opacity:0.55;
    class LearnerVerb planned;
```

## Building

Requires CMake 3.22+ and a C++17 toolchain (Xcode command line tools on
macOS, MSVC on Windows). JUCE itself is fetched automatically by CMake —
no separate JUCE install needed.

```bash
cmake -B build
cmake --build build --config Release
```

All three plugins build from the one root `CMakeLists.txt`. Artifacts land
under `build/EarTrainer_artefacts/Release/`,
`build/LearnerEQ_artefacts/Release/`, and
`build/LearnerComp_artefacts/Release/` (or `Debug/`), each with `VST3/`,
`AU/`, and `Standalone/` subfolders. Copy the `.vst3`/`.component` into
your system plugin folder, or run the Standalone build directly to test
without a DAW.

## Testing

Same build also produces a console `EarTrainerTests` target
(`juce::UnitTestRunner`-based; no plugin host or GUI needed to run it):

```bash
cmake --build build --target EarTrainerTests --config Release
./build/EarTrainerTests_artefacts/Release/EarTrainerTests
```

Exits non-zero if any test fails. Covers the games' scoring/state logic
(`tests/EQGameTest.cpp`, `tests/CompressionGameTest.cpp`,
`tests/ReverbGameTest.cpp`, `tests/GameManagerTest.cpp`), progress/level/
streak/daily-challenge logic (`tests/ProgressManagerTest.cpp`), and DSP
regression checks for both Learner plugins (`tests/LearnerEQTest.cpp` —
boosting a band raises measured output level at that frequency;
`tests/LearnerCompTest.cpp` — closed-form compression/makeup-gain math,
bypass passthrough, preset application). Also runs on push/PR via
`.github/workflows/build_and_test.yml` (badge above).

## Status

**Ear Trainer:** three exercises implemented — "guess the boosted/cut
band" (8 octave bands, 100 Hz–12.8 kHz), "guess the compression strength"
(weak/medium/strong), and "guess the reverb type" (room/hall/plate/
spring) — sharing a common `Game` interface driving one generic UI, plus
a `ProgressManager` (points, level 1-10 that scales each game's
difficulty, daily login streak, one daily challenge) — see
[docs/architecture.md](docs/architecture.md) and
[docs/decisions/002-difficulty-scaling.md](docs/decisions/002-difficulty-scaling.md).

**Learner EQ:** 4-band EQ (low shelf, 2 bells, high shelf) processing real
host audio, host-automatable via `AudioProcessorValueTreeState`, live
spectrum + response curve, contextual tooltip per frequency range while
dragging.

**Learner Comp:** compressor with a custom soft-knee gain-computer engine
(threshold/ratio/attack/release/knee/makeup/dry-wet, plus bypass),
processing real host audio, host-automatable, scrolling waveform with
gain-reduction highlighting, GR/peak meters, contextual tooltip per
control, 4 teaching presets — see
[docs/decisions/003-learnercomp-engine.md](docs/decisions/003-learnercomp-engine.md)
for why it isn't built on `juce::dsp::Compressor`.

## Documentation

- [docs/architecture.md](docs/architecture.md) — the `Game`/`GameManager`
  design doc and rationale
- [docs/diagrams/](docs/diagrams/) — system overview, game engine class
  diagram, Learner-plugin component diagrams, proposed CI pipeline
- [docs/decisions/001-game-interface.md](docs/decisions/001-game-interface.md) —
  why one generic `Game` interface instead of per-game UI
- [docs/decisions/002-difficulty-scaling.md](docs/decisions/002-difficulty-scaling.md) —
  `setDifficulty`/`ProgressManager`
- [docs/decisions/003-learnercomp-engine.md](docs/decisions/003-learnercomp-engine.md) —
  why LearnerComp has a custom compressor engine
- [docs/testing-strategy.md](docs/testing-strategy.md)
- [docs/roadmap.md](docs/roadmap.md)
- [CLAUDE.md](CLAUDE.md) — full per-file architecture breakdown, kept
  current for anyone (human or Claude) picking this project back up
