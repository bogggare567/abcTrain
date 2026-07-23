# Ear Trainer / Learner EQ

Two VST3/AU/Standalone plugins built with [JUCE](https://juce.com):

- **Ear Trainer** — ear-training games. It feeds you pink noise through a
  hidden filter and you guess what changed (which band was boosted/cut,
  how strong the compression is).
- **Learner EQ** — a real 4-band EQ for your own tracks, with a live
  spectrum display, a response-curve overlay, and a one-line plain-
  language explanation of what a frequency region does while you drag it.

## Building

Requires CMake 3.22+ and a C++17 toolchain (Xcode command line tools on
macOS, MSVC on Windows). JUCE itself is fetched automatically by CMake —
no separate JUCE install needed.

```bash
cmake -B build
cmake --build build --config Release
```

Both plugins build from the one root `CMakeLists.txt`. Artifacts land
under `build/EarTrainer_artefacts/Release/` and
`build/LearnerEQ_artefacts/Release/` (or `Debug/`), each with `VST3/`,
`AU/`, and `Standalone/` subfolders. Copy the `.vst3`/`.component` into
your system plugin folder, or run the Standalone build directly to test
without a DAW.

## Status

**Ear Trainer:** two exercises implemented, "guess the boosted/cut band"
(8 octave bands, 100 Hz–12.8 kHz) and "guess the compression strength"
(weak/medium/strong), sharing a common `Game` interface driving one
generic UI — see [docs/architecture.md](docs/architecture.md).

**Learner EQ:** 4-band EQ (low shelf, 2 bells, high shelf) processing real
host audio, host-automatable via `AudioProcessorValueTreeState`, live
spectrum + response curve, contextual tooltip per frequency range while
dragging.

See [CLAUDE.md](CLAUDE.md) for the full architecture breakdown and
roadmap.
