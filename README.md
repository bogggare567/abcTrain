# abcTrain — Ear Trainer / Learner EQ / Learner Comp / Learner Verb

[![Build and Test](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml)
[![License](https://img.shields.io/badge/license-all%20rights%20reserved-lightgrey)](LICENSE)
[![Platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux-blue)](#-download)
[![Formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU%20%7C%20Standalone-blue)](#-download)
[![Languages](https://img.shields.io/badge/languages-12-orange)](#-supported-languages)
[![GitHub Downloads](https://img.shields.io/github/downloads/bogggare567/abcTrain/total)](https://github.com/bogggare567/abcTrain/releases)

Pre-release, unsigned, actively developed - see [BETA_TESTING.md](BETA_TESTING.md)
before you dig in, and [docs/roadmap.md](docs/roadmap.md) for what's real vs. planned.

## 🎧 What is abcTrain?

Four free VST3/AU/Standalone plugins built with [JUCE](https://juce.com),
built around one idea: **train your ears on synthetic examples, then
apply what you hear on your own tracks with real, host-automatable
effects that teach as you use them.**

- **Ear Trainer** — a multiple-choice ear-training game. Doesn't touch
  your audio; it generates its own test signal and quizzes you on what a
  hidden effect changed.
- **Learner EQ / Learner Comp / Learner Verb** — real, usable EQ/
  compressor/reverb for your own tracks, each with a live spectrum/
  waveform display, plain-language tooltips instead of just numbers, and
  a guided step-by-step "Lesson."

## 🎮 9 games

| | Game | What you're guessing |
|---|---|---|
| 🎚️ | Guess the Band | Which of 8 octave bands (100 Hz–12.8 kHz) got boosted or cut |
| 🥁 | Guess the Compression | How strong the compression is (weak/medium/strong) |
| 🏛️ | Guess the Reverb | Room / Hall / Plate / Spring |
| ↔️ | Guess the Pan Position | Hard Left – Left – Center – Right – Hard Right |
| ⏱️ | Guess the Delay Time | 50 / 150 / 300 / 500 ms |
| 🔥 | Guess the Distortion | Soft Clip / Hard Clip / Tape Saturation / Overdrive |
| 📐 | Guess the Stereo Width | Narrow – Normal – Wide – Extra Wide |
| 🔊 | Guess the Gain Change | ±dB (the one game whose choice labels change with difficulty) |
| 🎯 | Name the Range | Sub-bass / Bass / Low-mids / Mids / High-mids / Presence / Air |

Every game shares one `Game` interface, one generic UI, adaptive
difficulty (level 1–10), a daily login streak, and a daily challenge — see
[docs/diagrams/game-engine.md](docs/diagrams/game-engine.md).

By default every game generates its own pink noise, chosen deliberately
for its flat spectrum (fair for any frequency/width question). A
"Training Sounds" button lets you train on real audio instead: two
always-available built-in categories (five short, originally-synthesized
percussive/sustained samples - no third-party or copyrighted content), or
point it at your own folder of audio via a "Choose Folder..." button.
Categories unlock progressively with your level, same as the games
themselves. This project never fetches, bundles, or vets the legality of
audio in a folder you point it at - that's on you. See
[decisions/015](docs/decisions/015-choice-slider-and-training-sounds.md)
and [decisions/018](docs/decisions/018-ui-polish-and-builtin-samples.md).

## 🎛️ Teaching plugins

- **Learner EQ** — a real 4-band EQ (low shelf, 2 bells, high shelf),
  host-automatable, live spectrum + response-curve display, a scrolling
  input/output waveform, a richer plain-language explanation per
  frequency region (practical values + a "Learn more" book pointer) while
  you drag it, a Bypass/A-B toggle, and a step-by-step "Lesson"
  (Vocal EQ Basics).
- **Learner Comp** — a real compressor with a custom soft-knee engine
  (threshold/ratio/attack/release/knee/makeup/dry-wet), live spectrum, a
  scrolling waveform that highlights in red wherever it's actively
  reducing gain, GR/peak meters, a richer explanation per control, 4
  teaching presets (Vocal Smoothing, Punchy Drums, Bass Control, Limiter),
  Bypass/A-B, and a "Lesson" (Vocal Compression).
- **Learner Verb** — a real reverb (Room/Hall/Plate via `dsp::Reverb`,
  Spring via a custom allpass cascade), the same live spectrum/waveform
  view, a richer explanation per control, 4 presets (Vocal Ambience,
  Concert Hall, Small Room, Spring Tank), Bypass/A-B, and a "Lesson"
  (Space for Vocals).

All three share the same visualization shape and Bypass/Lesson placement
— see [decisions/006](docs/decisions/006-unified-visualization.md). All
four plugins have an "Updates" button that checks GitHub for a newer
release on request — see [Download](#-download) below.

Long-term direction is a small learning ecosystem — more games, more
teaching plugins, an in-plugin knowledge base — see
[docs/roadmap.md](docs/roadmap.md). A first step toward that knowledge
base is [docs/knowledge_base.md](docs/knowledge_base.md) — original
reference material (general, widely-taught practice, not derived from any
specific book) covering EQ, compression, reverb, psychoacoustics,
mastering, mixing, room acoustics, digital audio, saturation, stereo, and
delay/modulation, which the three Learner plugins' tooltips are written
from. [docs/library_catalog.md](docs/library_catalog.md) is a companion
bibliography of ~150 audio-engineering books each tooltip's "Learn more"
line points into — deliberately a title/author catalog only, never
extracted or quoted text; see
[decisions/010](docs/decisions/010-book-library-scope.md) for why.

## 🌍 Supported languages

EarTrainer's editor has a language picker covering its core UI (game
names/instructions, level/score/streak labels, buttons) in 12 languages:

🇬🇧 English · 🇷🇺 Русский · 🇩🇪 Deutsch · 🇫🇷 Français · 🇪🇸 Español ·
🇵🇹 Português · 🇨🇳 简体中文 · 🇯🇵 日本語 · 🇰🇷 한국어 · 🇮🇹 Italiano ·
🇵🇱 Polski · 🇺🇦 Українська

Auto-detected from your system language on first run, persisted after
that. **Scope, honestly**: only the core string set above is translated
today - parameter tooltips, lesson steps, and LearnerEQ/Comp/Verb's UI are
still English-only, and only English/Russian have been directly verified
by a native speaker on this project. See
[decisions/011](docs/decisions/011-i18n.md) for the full picture and
[docs/diagrams/i18n-architecture.md](docs/diagrams/i18n-architecture.md)
for how it fits together.

## 📸 Demo

No screenshots or recordings yet - this project has been developed in a
sandbox with no display to capture a real JUCE window from. See
[assets/screenshots/README.md](assets/screenshots/README.md) and
[assets/demo/README.md](assets/demo/README.md) for exactly what's needed
and in what priority, if you're picking this up somewhere that can render
one. In the meantime, [Look](#look) below describes the actual current
visual style in text.

## Look

All four plugins now share one dark theme (`shared/AbcTrainLookAndFeel`)
instead of each Learner plugin picking its own accent colour:

- **Dark and light themes**, both designed rather than one inverted into
  the other. Dark: `#1e1e2e` under a soft radial gradient, blue accent
  `#5b9bd5`, warm orange highlight `#d98c5f`. Light: a warm off-white
  `#e8e6e1` page with panels stepping *up* toward white, deeper accents
  (a light ground needs more colour weight to read at the same strength)
  and softer, cooler shadows. Toggle in every plugin's title row; the
  choice is product-wide and persists.
- Every colour, spacing step, corner radius and animation duration comes
  from one token layer (`shared/AbcTrainTheme`) - nothing outside it names
  a colour.
- Buttons *lift and settle* rather than switching state: the shadow grows
  and the surface sinks under a press, all eased over time (press is
  faster than release - that asymmetry is what reads as mass). Rotary
  knobs swell slightly and bloom under the pointer.
- Controls sit on captioned section panels; spectrum and waveform are
  recessed into wells. The spectrum is a smoothed, gradient-filled curve
  over a soft frequency grid; the waveform a smoothed symmetric envelope;
  Learner Comp's gain reduction is an arc that fills *downward* and glows
  as it works.
- Guide text floats in over the visualisation only while you're dragging
  a control, over a real Gaussian blur of what's behind it.
- 22 px bold titles, 14 px body text, 16 px monospaced numeric readouts
  (peak meters, score/level) - all via the modern `juce::FontOptions`
  API, replacing every deprecated `Font(float, styleFlags)` call in the
  codebase.
- EarTrainer's level progress bar "breathes" - a slow, low-amplitude glow
  pulse at the fill's leading edge - independently of its existing eased
  fill-transition animation. Its choice slider fades a soft glow around
  the thumb on a correct answer, and gives it a brief decaying wobble on
  a wrong one, instead of a flat colour swap.

This is a bounded, coding-level polish pass, not the full FabFilter/
Ableton-grade design system a "premium redesign" implies - that needs a
real designer/design tool, not something this codebase produces alone.
Per-widget hover/press *state interpolation* (press-scale-then-spring-
back), gradient-filled meter curves, pill-shaped tooltip backgrounds, a
light theme, a licensed custom typeface, and `FlexBox` layout are all
still scoped out on purpose; see
[docs/decisions/009-look-and-feel.md](docs/decisions/009-look-and-feel.md)
and
[docs/decisions/018-ui-polish-and-builtin-samples.md](docs/decisions/018-ui-polish-and-builtin-samples.md)
for the full list of what's deferred and why. No screenshots here since
this environment can't render and capture a real JUCE window - the
description above is the accurate current state.

## Download

Pre-release, **unsigned** builds only — see [LICENSE](LICENSE) before
distributing anything built from this repo. Unsigned means macOS
Gatekeeper will show an "unidentified developer" block (right-click the
installer → Open, or allow it in System Settings → Privacy & Security)
and Windows SmartScreen will show a "Windows protected your PC" warning
(click "More info" → "Run anyway") — code signing/notarization is real,
separate future work, not done yet.

- **Tagged releases (recommended):** pushing a `vX.Y.Z` tag publishes a
  [GitHub Release](https://github.com/bogggare567/abcTrain/releases) with
  a real installer for each OS:
  - **macOS** — `abcTrain-macOS-X.Y.Z.dmg`. Open it, run the `.pkg`
    inside, and you'll get a real component-selection installer: check
    which of the four plugins you want, and under each, which format(s)
    (VST3/AU/Standalone) with a one-line explanation of what each format
    is for; then choose "install for all users of this Mac" or "just me"
    for the VST3/AU locations (Apple's installer offers this natively).
    A README, the LICENSE, and an "Open Plugins Folder.command" helper
    are on the same DMG.
  - **Windows** — `abcTrain-Windows-X.Y.Z-setup.exe`. Same
    plugin/format checkbox tree, then a folder-location page for the
    Standalone apps (default `Program Files\abcTrain`) and a second,
    separate page just for the VST3 folder (default
    `Common Files\VST3`) — both are plain text fields you can retype to
    anywhere you like. Creates Start Menu shortcuts and a normal
    Add/Remove Programs entry.
  - **Linux** — `abcTrain-Linux-X.Y.Z.tar.gz`. Extract it and run
    `./install.sh` inside: it asks which plugins to install and where
    VST3s should go (`$HOME/.vst3`, `/usr/lib/vst3` via `sudo`, or a path
    you type), same as the other two OSes, just as a terminal prompt
    instead of a GUI wizard.

  See [docs/decisions/008-installers.md](docs/decisions/008-installers.md)
  for exactly what each installer can and can't do (macOS's system
  installer has no free-text custom path, unlike Windows/Linux — a real
  platform limitation, not an oversight).
- **Latest raw build from any push** (no installer, just the built
  plugins): [Actions](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml) →
  the most recent green run → **Artifacts** at the bottom of the run page
  → download `plugins-ubuntu-latest`/`plugins-macos-latest`/
  `plugins-windows-latest` (each is a zip of that OS's VST3/AU/Standalone
  builds for all four plugins, for manually copying into place).
- **In-plugin update check:** each plugin has an "Updates" button that
  checks GitHub for a newer tagged release and offers to open the release
  page — manual only, no background network calls. See
  [docs/decisions/007-update-checker.md](docs/decisions/007-update-checker.md).

## Architecture

The full system diagram (all 9 games, `shared/` components including
`LocalisationManager`/`AbcTrainLookAndFeel`/`UpdateChecker`, what
`EarTrainerTests` actually exercises, dashed boxes for what's not built
yet) lives in
[docs/diagrams/system-overview.md](docs/diagrams/system-overview.md) —
kept there as the single copy rather than duplicated here, since a second
copy in this README is exactly what went stale in an earlier version of
this file. See also
[docs/diagrams/game-engine.md](docs/diagrams/game-engine.md) (the `Game`
interface + all 9 exercises) and
[docs/diagrams/i18n-architecture.md](docs/diagrams/i18n-architecture.md).

## Building

Requires CMake 3.22+ and a C++17 toolchain (Xcode command line tools on
macOS, MSVC on Windows). JUCE itself is fetched automatically by CMake —
no separate JUCE install needed.

```bash
cmake -B build
cmake --build build --config Release
```

All four plugins build from the one root `CMakeLists.txt`. Artifacts land
under `build/EarTrainer_artefacts/Release/`,
`build/LearnerEQ_artefacts/Release/`,
`build/LearnerComp_artefacts/Release/`, and
`build/LearnerVerb_artefacts/Release/` (or `Debug/`), each with `VST3/`,
`AU/`, and `Standalone/` subfolders. Copy the `.vst3`/`.component` into
your system plugin folder, or run the Standalone build directly to test
without a DAW.

On Linux, building locally also needs `libcurl4-openssl-dev` (or your
distro's equivalent) installed — the in-plugin update checker needs
libcurl for HTTPS there, since JUCE's non-curl Linux networking has no
TLS support at all. See
[docs/decisions/007-update-checker.md](docs/decisions/007-update-checker.md).

## Testing

Same build also produces a console `EarTrainerTests` target
(`juce::UnitTestRunner`-based; no plugin host or GUI needed to run it):

```bash
cmake --build build --target EarTrainerTests --config Release
./build/EarTrainerTests_artefacts/Release/EarTrainerTests
```

Exits non-zero if any test fails. Covers the games' scoring/state logic
(`tests/EQGameTest.cpp`, `tests/CompressionGameTest.cpp`,
`tests/ReverbGameTest.cpp`, `tests/PanGameTest.cpp`, `tests/DelayGameTest.cpp`,
`tests/DistortionGameTest.cpp`, `tests/StereoWidthGameTest.cpp`,
`tests/DBGameTest.cpp`, `tests/FrequencyRangeGameTest.cpp`,
`tests/GameManagerTest.cpp`), progress/level/
streak/daily-challenge logic (`tests/ProgressManagerTest.cpp`), and DSP/
behavioral checks for all three Learner plugins (`tests/LearnerEQTest.cpp`
— boosting a band raises measured output level at that frequency;
`tests/LearnerCompTest.cpp` — closed-form compression/makeup-gain math,
bypass passthrough, preset application; `tests/LearnerVerbTest.cpp` — a
tail persists after the input stops, `dryWet=0` is an exact passthrough,
bypass forces an exact dry passthrough without clobbering `Dry/Wet`, every
reverb type produces sound, preset application), the step-navigation
logic behind the "Lesson" feature (`tests/MicroLessonTest.cpp`), and the
pure version-comparison/JSON-parsing logic behind the "Check for Updates"
button (`tests/UpdateCheckerTest.cpp` — the real network call itself
isn't tested, see
[docs/decisions/007-update-checker.md](docs/decisions/007-update-checker.md)).
Also runs on push/PR via `.github/workflows/build_and_test.yml` (badge
above).

## Status

**Ear Trainer:** 9 exercises implemented — "guess the boosted/cut band"
(8 octave bands, 100 Hz–12.8 kHz), "guess the compression strength"
(weak/medium/strong), "guess the reverb type" (room/hall/plate/spring),
"guess the pan position" (5 positions, Hard Left–Hard Right), "guess the
delay time" (50/150/300/500 ms), "guess the distortion" (Soft Clip/Hard
Clip/Tape Saturation/Overdrive), "guess the stereo width" (Narrow–Extra
Wide), "guess the gain change" (±dB, the one game whose choice labels
themselves change with difficulty), and "name the frequency range"
(Sub-bass through Air, the standard 7-range naming) — sharing a common `Game` interface
driving one generic UI, plus a `ProgressManager` (points, level 1-10 that
scales each game's difficulty, daily login streak, one daily challenge)
— see [docs/architecture.md](docs/architecture.md),
[docs/diagrams/game-engine.md](docs/diagrams/game-engine.md), and
[docs/decisions/002-difficulty-scaling.md](docs/decisions/002-difficulty-scaling.md).

**Learner EQ:** 4-band EQ (low shelf, 2 bells, high shelf) processing real
host audio, host-automatable via `AudioProcessorValueTreeState`, live
spectrum + response curve, a scrolling input/output waveform, contextual
tooltip per frequency range while dragging, and a Bypass toggle.

**Learner Comp:** compressor with a custom soft-knee gain-computer engine
(threshold/ratio/attack/release/knee/makeup/dry-wet, plus bypass),
processing real host audio, host-automatable, live spectrum, scrolling
waveform with gain-reduction highlighting, GR/peak meters, contextual
tooltip per control, 4 teaching presets — see
[docs/decisions/003-learnercomp-engine.md](docs/decisions/003-learnercomp-engine.md)
for why it isn't built on `juce::dsp::Compressor`.

**Learner Verb:** reverb with Room/Hall/Plate (`juce::dsp::Reverb`) and
Spring (custom allpass cascade) via one `ReverbEngine`, pre-delay,
processing real host audio, host-automatable, the same live spectrum +
scrolling waveform + peak-meter view as LearnerComp (now
`shared/SpectrumAnalyzer`/`shared/WaveformDisplay`), 4 teaching presets, a
Bypass toggle — see
[docs/decisions/004-learnerverb-scope.md](docs/decisions/004-learnerverb-scope.md)
for what was deliberately trimmed from the initial build (impulse-response
visualization, decay-vs-frequency graph, stereo correlometer) and why.

**Micro-lessons:** each Learner plugin has one guided "Lesson" — a
step-by-step walkthrough that jumps its own parameters to a taught value
at each step (Learner EQ: Vocal EQ Basics; Learner Comp: Vocal
Compression; Learner Verb: Space for Vocals) via shared
`MicroLesson`/`LessonController` machinery — see
[docs/decisions/005-microlesson-architecture.md](docs/decisions/005-microlesson-architecture.md)
for the split and what was trimmed (per-control highlighting).

**Unified visualization:** all three Learner plugins now share the same
shape (live spectrum, then waveform + peak meters, then controls, then
Bypass next to Lesson in the title row) via the newly-extracted
`shared/SpectrumAnalyzer` — see
[docs/decisions/006-unified-visualization.md](docs/decisions/006-unified-visualization.md).

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
- [docs/decisions/004-learnerverb-scope.md](docs/decisions/004-learnerverb-scope.md) —
  LearnerVerb's trimmed visualization scope and the decay-to-`roomSize` approximation
- [docs/decisions/005-microlesson-architecture.md](docs/decisions/005-microlesson-architecture.md) —
  `MicroLesson`/`LessonController` split and why per-control highlighting was cut
- [docs/decisions/006-unified-visualization.md](docs/decisions/006-unified-visualization.md) —
  unifying spectrum/waveform/bypass across all three Learner plugins, and
  why it isn't tested with a real `SpectrumAnalyzerComponent`
- [docs/decisions/007-update-checker.md](docs/decisions/007-update-checker.md) —
  CI artifacts/releases, the manual-only "Check for Updates" button, and
  why it needs `NEEDS_CURL TRUE` on Linux
- [docs/decisions/008-installers.md](docs/decisions/008-installers.md) —
  the real per-OS installers (`.pkg`/DMG, Inno Setup, `tar.gz`), and why
  macOS can't offer a free-text custom install path the way
  Windows/Linux can
- [docs/decisions/009-look-and-feel.md](docs/decisions/009-look-and-feel.md) —
  the shared `AbcTrainLookAndFeel` dark theme, and what was deliberately
  deferred from this first redesign pass
- [docs/decisions/010-book-library-scope.md](docs/decisions/010-book-library-scope.md) —
  why the book-library work stops at a bibliography
  ([docs/library_catalog.md](docs/library_catalog.md)) plus an original
  reference doc ([docs/knowledge_base.md](docs/knowledge_base.md))
  instead of extracting and quoting text from copyrighted books in the
  shipped plugins
- [docs/decisions/011-i18n.md](docs/decisions/011-i18n.md) — the
  flat-JSON-per-language i18n system
  ([docs/diagrams/i18n-architecture.md](docs/diagrams/i18n-architecture.md)),
  why it's scoped to a curated core string set rather than every tooltip,
  and a real UTF-8-literal bug it caught
- [docs/decisions/012-versioning.md](docs/decisions/012-versioning.md) —
  deriving the version from `git describe` instead of a hand-bumped
  literal, the stable/beta/dev channel detector, and what's still deferred
- [docs/diagrams/game-engine.md](docs/diagrams/game-engine.md) — the
  `Game` interface and all 9 exercises' class diagram
- [docs/testing-strategy.md](docs/testing-strategy.md)
- [docs/roadmap.md](docs/roadmap.md)
- [CLAUDE.md](CLAUDE.md) — full per-file architecture breakdown, kept
  current for anyone (human or Claude) picking this project back up

## 🧪 Beta testing

Trying it before it's finished is genuinely useful — see
[BETA_TESTING.md](BETA_TESTING.md) for what's worth testing right now,
and what's already known to be incomplete (so you don't file a bug for
something already tracked).

## 🤝 Contributing

New games, translations, bug fixes, docs — see
[.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) (English + Русский)
for how this project likes PRs shaped, and the
[bug report](.github/ISSUE_TEMPLATE/bug_report.md)/
[feature request](.github/ISSUE_TEMPLATE/feature_request.md) templates
for filing an issue.
