# Ear Trainer

An ear-training VST3/AU/Standalone plugin built with [JUCE](https://juce.com).
Play the plugin, and it feeds you pink noise through a hidden peak filter;
you guess which octave band was boosted or cut.

## Building

Requires CMake 3.22+ and a C++17 toolchain (Xcode command line tools on
macOS, MSVC on Windows). JUCE itself is fetched automatically by CMake —
no separate JUCE install needed.

```bash
cmake -B build
cmake --build build --config Release
```

Build artifacts land under `build/EarTrainer_artefacts/Release/` (or
`Debug/`), with `VST3/`, `AU/`, and `Standalone/` subfolders. Copy the
`.vst3`/`.component` into your system plugin folder, or run the
Standalone build directly to test without a DAW.

## Status

MVP: one exercise implemented — "guess the boosted/cut band" (8 octave
bands, 100 Hz–12.8 kHz). See [CLAUDE.md](CLAUDE.md) for architecture notes
and the rest of the roadmap.
