<div align="center">

# abcTrain

**Train your ears. Then use plugins that teach you while you mix.**

Nine ear-training exercises and three real, host-automatable effects —
one free, open-source suite for macOS, Windows and Linux.

[![Build and Test](https://img.shields.io/github/actions/workflow/status/bogggare567/abcTrain/build_and_test.yml?style=flat-square&labelColor=1E1E2E&color=5FB98C&label=build)](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml)
[![Latest release](https://img.shields.io/github/v/release/bogggare567/abcTrain?style=flat-square&labelColor=1E1E2E&color=5B9BD5&label=release)](https://github.com/bogggare567/abcTrain/releases/latest)
[![Formats](https://img.shields.io/badge/VST3%20%C2%B7%20AU%20%C2%B7%20Standalone-4A4A5A?style=flat-square&labelColor=1E1E2E)](#-download)
[![Languages](https://img.shields.io/badge/languages-12-4A4A5A?style=flat-square&labelColor=1E1E2E)](#-supported-languages)
[![Tests](https://img.shields.io/badge/tests-220%20groups-4A4A5A?style=flat-square&labelColor=1E1E2E)](docs/testing-strategy.md)
[![Licence](https://img.shields.io/badge/licence-all%20rights%20reserved-4A4A5A?style=flat-square&labelColor=1E1E2E)](LICENSE)

<br>

[![Download for macOS, Windows and Linux](https://img.shields.io/badge/%E2%AC%87%20DOWNLOAD-macOS%20%C2%B7%20Windows%20%C2%B7%20Linux-5B9BD5?style=for-the-badge&labelColor=1E1E2E)](https://github.com/bogggare567/abcTrain/releases/latest)
&nbsp;
[![Support the project on DonationAlerts](https://img.shields.io/badge/%E2%99%A5%20SUPPORT-DonationAlerts-D98C5F?style=for-the-badge&labelColor=1E1E2E)](https://www.donationalerts.com/r/bogdankorablev)
&nbsp;
[![Telegram channel](https://img.shields.io/badge/TELEGRAM-@vstabc-4FA3C7?style=for-the-badge&logo=telegram&logoColor=white&labelColor=1E1E2E)](https://t.me/vstabc)

<br>

[![Try it](https://img.shields.io/badge/Try%20it%20live-2A2A3A?style=flat-square)](https://bogggare567.github.io/abcTrain/)
[![How it works](https://img.shields.io/badge/How%20it%20works-2A2A3A?style=flat-square)](#-how-it-works)
[![The nine exercises](https://img.shields.io/badge/9%20exercises-2A2A3A?style=flat-square)](#-9-games)
[![Roadmap](https://img.shields.io/badge/Roadmap-2A2A3A?style=flat-square)](docs/roadmap.md)
[![Wiki](https://img.shields.io/badge/Wiki-2A2A3A?style=flat-square)](https://github.com/bogggare567/abcTrain/wiki)
[![Discussions](https://img.shields.io/badge/Discussions-2A2A3A?style=flat-square)](https://github.com/bogggare567/abcTrain/discussions)
[![Contribute](https://img.shields.io/badge/Contribute-2A2A3A?style=flat-square)](docs/orientation.md)

<br>

[![▶ Try three real rounds in your browser](https://img.shields.io/badge/%E2%96%B6%20TRY%20IT%20NOW-no%20install%2C%20ten%20seconds-4FA3C7?style=for-the-badge&labelColor=1E1E2E)](https://bogggare567.github.io/abcTrain/)

</div>

---

## ▶ Try it, don't look at it

**[bogggare567.github.io/abcTrain](https://bogggare567.github.io/abcTrain/)** — three
real rounds in your browser: frequency, level and pan. Same pink noise, same
log axis, same accept band the plugin grades you against, read straight out
of the plugin's own source so the two cannot drift apart.

Screenshots used to live here. A picture of an ear trainer is the one thing
that cannot show what an ear trainer does — you have to hear it get something
wrong. The demo takes about ten seconds and needs nothing installed.

*(Turn your volume down first. Nothing makes a sound until you press play.)*


## The problem

You can read that 300 Hz is "muddy" a hundred times and still not hear it.
Mixing is a listening skill, and listening skills come from repetition
with immediate feedback — not from reading.

Most people never get that repetition, because building the exercise is
harder than doing it.

## 🎧 How it works

**Train.** Nine exercises, each one hiding a change in a sound. You say
what changed. It tells you immediately whether you were right, and by how
much you missed.

**Then apply it.** Three companion plugins process your *own* audio — a
real 4-band EQ, a real compressor, a real reverb — with plain-language
explanations on every control and step-by-step lessons built in. The
thing you just trained on is the thing you're now using.

Nothing is locked. There is no account, no subscription, and no telemetry
— the only network call in the whole suite is the update check, and only
when you press the button.

---

## 🎮 9 games

| | Game | What you're guessing | Answer |
|---|---|---|---|
| 🎚️ | Find the Frequency | Which frequency got boosted or cut, anywhere in 100 Hz–12.8 kHz | scale |
| ↔️ | Guess the Pan Position | Where it sits across the stereo field | scale |
| 🔊 | Guess the Gain Change | How much the level moved, −9…+9 dB | scale |
| ⏱️ | Guess the Delay Time | How long the echo is, 20–640 ms | scale |
| 🥁 | Guess the Compression | How strong the compression is — two of weak/medium/strong | two |
| 🏛️ | Guess the Reverb | Two of Room / Chamber / Hall / Plate / Spring | two |
| 🔥 | Guess the Distortion | Two of Soft Clipping / Hard Clipping / Tape Saturation / Overdrive | two |
| 📐 | Guess the Stereo Width | Two of Narrow / Normal / Wide / Extra Wide | two |
| 🎯 | Name the Range | Two of Sub-bass … Air, the standard seven | two |

The five naming exercises always offer **exactly two** alternatives, at
every level. What the level picks is *which* two — how close together they
sit on that exercise's own axis of character — and how archetypal an
example of its category you hear, since every answer is a *family* of
genuinely different settings rather than one preset with a wobble on it. A
third button makes a round harder by giving you more to read and improving
your odds of a lucky guess, neither of which is your ear getting better.
See [docs/decisions/031](docs/decisions/031-two-alternatives-and-preset-families.md).

Every game shares one `Game` interface, one generic UI, adaptive
difficulty (level 1–10), a daily login streak, and a daily challenge — see
[docs/diagrams/game-engine.md](docs/diagrams/game-engine.md).

**Four of them ask for a value, not a choice.** EQ, pan, gain and delay
draw a real target from the whole range each round — 425 Hz, L86, −3.5 dB,
185 ms — and you drag along a scale to answer. Difficulty narrows the
tolerance band rather than making the sound weaker, so harder means *more
precise* instead of *less audible*. The band is in the unit the ear works
in: octaves for frequency, a ratio for delay, dB for gain. See
[decisions/020](docs/decisions/020-continuous-answers.md). The other five
stay multiple-choice, because their answer genuinely is a category
(which reverb, which kind of saturation).

**Runs have a shape.** *Practice* is unlimited. *Survival* gives you three
lives and ends when they're gone. *Blitz* is a 90-second clock where a
wrong answer costs five seconds instead of a life. Rounds advance on their
own after an answer, and every exercise keeps its own lifetime record. See
[decisions/021](docs/decisions/021-sessions-and-navigation.md).

You land on a home screen that groups the trainings by the skill they
build — frequency, dynamics, space & stereo, character — with a star to
pin the ones you're focusing on.

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

## 🎨 How it was designed

Dark and light themes, both *designed* rather than one inverted into the
other — a warm off-white page with surfaces stepping up toward white, not
a dark UI with the lightness flipped. Every colour, spacing step, corner
radius and animation duration comes from one token layer, so nothing
drifts.

Four UI sizes (S/M/L/XL) scale one logical layout through a transform, so
every size is the same design rather than four sets of hand-tuned numbers.

Controls have weight: buttons lift and settle, knobs bloom under the
pointer, and press eases faster than release — that asymmetry is what
reads as mass. The spectrum is a smoothed gradient-filled curve over a
soft grid; the gain-reduction meter fills *downward*, because reduction
is the one meter where more is lower.

## ⬇ Download

Pre-release, **unsigned** builds only — see [LICENSE](LICENSE) before
distributing anything built from this repo.

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


### Your system will warn you the first time. Here's why, and what to do

These builds are **not code-signed**. Signing means buying a certificate
issued against a verified legal identity — around $99/year from Apple and
$200-500/year from a Windows CA (see [docs/signing.md](docs/signing.md)
for the full breakdown and how to wire it into CI once it exists). That
hasn't been bought, so every OS treats the download as coming from
someone it can't name.

The warning is accurate. It says the publisher is unverified — not that
anything was found wrong with the file. Everything here is built by
[GitHub Actions](.github/workflows/build_and_test.yml) from the commit
the release tag points at, so you can read exactly what went in.

- **macOS** — "cannot be opened because it is from an unidentified
  developer", or "Apple could not verify...". Right-click (or
  Control-click) the `.pkg` → **Open** → **Open** again. If macOS refuses
  outright, go to **System Settings → Privacy & Security**, scroll to
  Security, and press **Open Anyway** next to the blocked item. On
  Sequoia and later that button is the only route for some downloads.
- **Windows** — a blue "Windows protected your PC" SmartScreen panel.
  Click **More info**, then **Run anyway**. If your browser blocked the
  download itself, use *Keep* → *Keep anyway* in the downloads list.
- **Linux** — no gatekeeper, but the extracted files may not be
  executable. `chmod +x install.sh` and run it.

If you'd rather not click through a warning at all, **build from source**
— it's three commands, and the [Building](#building) section below has
them. That is the same binary, made on your own machine.

## 🛠 Built to be worked on

This is meant to outlive whoever wrote it. If you want to add an
exercise, fix something, or fork it entirely:

**[📖 docs/orientation.md](docs/orientation.md)** — the map. The four
load-bearing ideas, a table of *where to put a change*, and a seven-step
recipe for adding an exercise. Read it first; it's short.

Beyond that:

| | |
|---|---|
| [docs/README.md](docs/README.md) | Index of everything — diagrams, decisions, reference material |
| [docs/decisions/](docs/decisions/) | **21 ADRs.** Every non-obvious choice with the alternative that was rejected and why, written for someone who wasn't there |
| [CLAUDE.md](CLAUDE.md) | Per-file breakdown of the whole repo |
| [docs/testing-strategy.md](docs/testing-strategy.md) | What's covered, what isn't, and which gaps are deliberate |

Three things that make changes safe here:

- **One generic UI drives all nine exercises.** Adding a tenth needs no
  editor or processor changes — implement `Game`, register it, add a
  test.
- **Every capability is an opt-in hook with an inert default.** Nothing
  you don't override can break.
- **220 test groups** run on every push across macOS, Windows and Linux.
  Pure logic is tested directly; the deliberate gaps are documented
  rather than pretended away.

And one thing that will bite you if nobody says it: **the tests cannot
see layout.** Every UI pass in this project's history has shipped a bug
that compiled, passed everything, and was obvious ten seconds after
launching the app. If you change something visual, build it and look at
it.

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

```bash
cmake --build build --target EarTrainerTests --config Release
./build/EarTrainerTests_artefacts/Release/EarTrainerTests
```

A console app, not a plugin — no host, no GUI. Exits non-zero on any
failure, and runs on every push/PR across all three OSes.

**220 test groups** covering: every game's scoring and state machine; the
shared contract for all four continuous-scale games (on-target passes, a
whole axis away fails, tolerance narrows with difficulty, the axis is
linear in the unit it claims); training-run rules including every hint
pricing boundary; progress/level/streak/daily-challenge maths and its
persistence round-trip; the reference-audio library; lesson step
navigation; version comparison and release-JSON parsing; all twelve
translations; and real DSP assertions for each Learner plugin — an EQ
boost measurably raises output at that frequency, the compressor hits its
closed-form target, reverb leaves a tail and `dryWet=0` is bit-exact.

What *isn't* tested, and why, is written down in
[docs/testing-strategy.md](docs/testing-strategy.md) rather than left for
you to discover.

## Status

**Ear Trainer:** 9 exercises implemented. Four answer on a scale — "find
the frequency" (anywhere in 100 Hz–12.8 kHz), "guess the pan position",
"guess the delay time" (20–640 ms), "guess the gain change" (±9 dB) — with
the level narrowing the accept band. Five answer by naming, always between
exactly two alternatives — "guess the compression strength", "guess the
reverb type" (room/chamber/hall/plate/spring), "guess the distortion",
"guess the stereo width", "name the frequency range" — with the level
choosing which two and how textbook the example is. All nine share a common `Game` interface
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

## 📚 Documentation

All of it is indexed in **[docs/README.md](docs/README.md)** — one list,
kept in one place, so it can't drift out of step with a second copy here.
(An earlier version of this README *was* that second copy, and it went
stale: it stopped at ADR 012 while the repo had 21.)

The short version: start at [docs/orientation.md](docs/orientation.md),
reach for [docs/decisions/](docs/decisions/) when you want to know why
something is the way it is, and [CLAUDE.md](CLAUDE.md) when you need the
per-file map.

## 🧪 Beta testing

Trying it before it's finished is genuinely useful — see
[BETA_TESTING.md](BETA_TESTING.md) for what's worth testing right now,
and what's already known to be incomplete (so you don't file a bug for
something already tracked).

## 💬 Where to ask what

Four places, and which one you want is decided by what you have, not by how
important it feels.

| You have | Go here |
|---|---|
| a **question** about using it | [Discussions → Q&A](https://github.com/bogggare567/abcTrain/discussions/categories/q-a) |
| something **broken** — a crash, a host that will not load it, an installer that fails | [Issues](https://github.com/bogggare567/abcTrain/issues/new/choose) |
| an **idea** | [Discussions → Ideas](https://github.com/bogggare567/abcTrain/discussions/categories/ideas) — start with the problem, not the solution |
| something you **made** with it | [Discussions → Show and tell](https://github.com/bogggare567/abcTrain/discussions/categories/show-and-tell) |
| to know when a **new version** lands | [Telegram → @vstabc](https://t.me/vstabc) |

[**@vstabc**](https://t.me/vstabc) is a channel, not a chat: releases and
what changed in them, and nothing else. Conversation stays in Discussions,
where it is searchable, threaded and readable by someone who arrives a year
from now - which a Telegram backlog is not.

The [**wiki**](https://github.com/bogggare567/abcTrain/wiki) answers the
questions that already have answers: installing it, what each exercise
trains, how to import your own audio, how levels work, and what to do when
something goes wrong. It is written for people using the suite; the `docs/`
folder here is written for people changing it.

Before reporting a crash, check you are on the current release — a
use-after-free that killed Learner Comp and Learner Verb mid-check was
fixed in v1.2.2. A crash report beats a description, because it names the
line; the wiki's
[Troubleshooting](https://github.com/bogggare567/abcTrain/wiki/Troubleshooting)
page says where your OS keeps them.

## 🤝 Contributing

Good first contributions, roughly in order of how self-contained they are:

- **A translation.** One JSON file in `shared/i18n/strings/`. Nine of the
  twelve languages have only been machine-checked.
- **Screenshots.** [assets/screenshots/README.md](assets/screenshots/README.md)
  lists which ones are wanted.
- **A new exercise.** Seven steps, all in
  [docs/orientation.md](docs/orientation.md).
- **Anything on the [roadmap](docs/roadmap.md)** marked ⏳.

[.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) (English + Русский)
covers how PRs are shaped here, and there are
[bug report](.github/ISSUE_TEMPLATE/bug_report.md) and
[feature request](.github/ISSUE_TEMPLATE/feature_request.md) templates.

## 💛 Supporting it

Free, open source, and staying that way — no paid tier, no subscription,
nothing locked behind anything.

If it's useful to you, a **[star](https://github.com/bogggare567/abcTrain)**
helps other people find it — GitHub lists everyone who has starred a
repository at
[/stargazers](https://github.com/bogggare567/abcTrain/stargazers), so it is
a public thank-you rather than an anonymous number, and a donation keeps it moving. Both are
optional and neither unlocks anything: gating software behind a star is
against [GitHub's rules on incentivised engagement](https://docs.github.com/en/site-policy/acceptable-use-policies/github-acceptable-use-policies),
and would be trivially bypassable anyway.

<div align="center">

[![Support the project on DonationAlerts](https://img.shields.io/badge/%E2%99%A5%20SUPPORT%20THIS%20PROJECT-DonationAlerts-D98C5F?style=for-the-badge&labelColor=1E1E2E)](https://www.donationalerts.com/r/bogdankorablev)
&nbsp;
[![Telegram channel](https://img.shields.io/badge/TELEGRAM-@vstabc-4FA3C7?style=for-the-badge&logo=telegram&logoColor=white&labelColor=1E1E2E)](https://t.me/vstabc)

</div>

There is also [soundkorb.ru](https://soundkorb.ru), which is where the
rest of this work lives.
