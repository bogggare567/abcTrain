# Orientation — read this before changing anything

The map. `docs/decisions/` explains *why* each shape was chosen,
`CLAUDE.md` is the per-file breakdown, and
[architecture.md](architecture.md) is the original pre-refactor design
doc kept for its rationale.

## The one-paragraph version

Four plugins share one CMake build and one `shared/` folder. **Ear
Trainer** generates its own test signal and quizzes you on it. **Learner
EQ / Comp / Verb** process the host's real audio and teach while you use
them. Everything visual comes from `shared/AbcTrainTheme` and
`shared/AbcTrainLookAndFeel`. Everything that persists goes through
`juce::PropertiesFile`.

```
                    ┌────────────────────────────┐
                    │        shared/             │
                    │  theme · look&feel · icons │
                    │  scopes · meters · i18n    │
                    │  lessons · update checker  │
                    └────┬──────┬──────┬─────┬───┘
                         │      │      │     │
              ┌──────────┘      │      │     └──────────┐
        ┌─────▼─────┐    ┌──────▼──┐ ┌─▼───────┐ ┌──────▼────┐
        │ EarTrainer│    │LearnerEQ│ │LearnerC.│ │ LearnerV. │
        └─────┬─────┘    └─────────┘ └─────────┘ └───────────┘
              │
   ┌──────────┼──────────┬───────────────┐
┌──▼───┐ ┌────▼────┐ ┌───▼──────┐ ┌──────▼──────┐
│ Game │ │ Session │ │ Progress │ │ Reference   │
│  x9  │ │ Manager │ │ Manager  │ │ AudioLibrary│
└──────┘ └─────────┘ └──────────┘ └─────────────┘
```

## The four load-bearing ideas

### 1. `Game` is the only thing the trainer's UI knows about

`Source/Games/Game.h`. One generic editor drives all nine exercises. It
asks a `Game` what it's called, what the choices are, whether it uses a
continuous scale, whether it supports A/B — and never knows which
exercise it's talking to.

**Every capability added since the first version is a non-pure-virtual
with an inert default.** Continuous answers, reference audio,
before/after — a game that doesn't override them keeps working untouched.
Follow that when adding the next one: never add a pure virtual to `Game`
unless every existing game genuinely must implement it.

### 2. Two kinds of exercise, and the difference is real

| | Answer is | Scored by | Difficulty changes |
|---|---|---|---|
| **Categorical** | one of N | exact match | which/how many options, how subtle the stimulus |
| **Continuous** | a value | distance from target, inside a tolerance band | how wide the band is |

Compression, reverb, distortion, stereo width and frequency-range are
categorical — their answer genuinely *is* a category. EQ, pan, gain and
delay are continuous, because "1.6 kHz" is not a thing you dial and
"Left" is not a pan value. See
[ADR 020](decisions/020-continuous-answers.md), including why the
tolerance unit differs per game (octaves, a ratio, dB).

### 3. Everything visual reads the theme; nothing hardcodes a colour

`shared/AbcTrainTheme::current()` returns the active palette. A hex
literal in a `paint()` is a bug — it will be wrong in one of the two
themes.

This has bitten three times in this codebase, always identically: **a
colour captured in a constructor outlives a theme switch.** Read the
palette at paint time or in a refresh method, never once at construction.

### 4. Persistence has two homes, on purpose

- `juce::AudioProcessorValueTreeState` — per-plugin-instance parameters
  that save with the host session. The Learner plugins' knobs.
- `juce::PropertiesFile` — per-*user* state outliving every session,
  project and host: points, level, per-exercise stats, favourites,
  language, theme, UI scale.

A knob position belongs to the session; a training record belongs to the
person. Don't mix them.

## Where to put a change

| I want to… | Go to |
|---|---|
| Add an exercise | `Source/Games/` + `GameManager` — see below |
| Change how answering feels | `Source/ChoiceSliderComponent.cpp` |
| Change run rules (lives, timing, hints) | `Source/SessionManager.{h,cpp}` |
| Change what's saved about a player | `Source/ProgressManager.{h,cpp}` |
| Change a colour, spacing, duration, easing | `shared/AbcTrainTheme.cpp` — **only** here |
| Change how a JUCE widget is drawn | `shared/AbcTrainLookAndFeel.cpp` |
| Add or fix a translation | `shared/i18n/strings/<code>.json` |
| Change a Learner plugin's DSP | `Learner*/Source/*Engine.h` |
| Change the home screen or navigation | `Source/HomeScreenComponent.{h,cpp}` |

## Adding an exercise

1. `Source/Games/YourGame.{h,cpp}` implementing `Game`.
2. If the answer is a *value* rather than a category, override the
   continuous hooks (ADR 020). If there's a meaningful unprocessed
   version of the sound, override the A/B hooks too.
3. **Append** it to `GameManager`'s constructor. Never insert into the
   middle — `ProgressManager` keys per-exercise stats and favourites by
   index, so reordering shuffles every existing player's saved record.
   Nothing in the code enforces this.
4. Add it to `categoryForGame()` and `tintForGame()` in
   `Source/PluginEditor.cpp` so it lands in a home-screen group with its
   own colour. Anything unrecognised falls back to "Character".
5. Add the `.cpp` to `CMakeLists.txt` — both the `EarTrainer` target and
   `EarTrainerTests`.
6. i18n keys in `en.json` and `ru.json`: `game.<id>.name`,
   `.instructions`, `.benefit`. Other languages fall back to English.
7. A test in `tests/`, following the template the other games use. A
   continuous game is picked up automatically by `ContinuousScaleTest`'s
   shared contract.

No processor or editor changes are needed beyond steps 3–4.

## Building and testing

```bash
cmake -B build
cmake --build build                       # all four plugins
cmake --build build --target EarTrainerTests
./build/EarTrainerTests_artefacts/*/EarTrainerTests
```

JUCE is fetched by CMake — no separate install. On Linux you also need
`libcurl4-openssl-dev` for the update checker's HTTPS.

172 test groups today. What is and isn't covered, and why, is in
[testing-strategy.md](testing-strategy.md).

**One thing worth internalising:** `juce::ChangeBroadcaster::sendChangeMessage()`
is asynchronous and the test binary never pumps a message loop. That is
why `ProgressManager::registerAnswer()` and all of `SessionManager` are
directly callable — those are the seams tests use. Design new state the
same way: put the logic somewhere a test can call synchronously.

## What running it catches that the tests can't

This project has a consistent history worth knowing about: **every UI
pass has shipped at least one bug that compiled, passed every test, and
was obvious within ten seconds of launching the app.** An invisible
slider groove. Text clipped by a panel's own clip region. A layout
computed from a 1px-high rectangle. A bought hint that outlived its
round. Controls whose visibility was set in a method that no longer ran.

The ADRs record each one, because they are not accidents — they are the
category of thing this test suite structurally cannot see. If you change
anything visual, build it and look at it.

The cheapest way to look:

```bash
cmake --build build --target EditorSnapshots
./build/EditorSnapshots_artefacts/EditorSnapshots ~/shots
```

That renders all three Learner editors to PNGs, in both themes, with no
plugin host — six pictures, no DAW, no window server. Its first run found
six bugs that had passed every test, five of which predated the change
that prompted it. It asserts nothing on purpose; see
[ADR 023](decisions/023-learner-plugin-visual-pass.md).

EarTrainer's screens still need the standalone app: its editor writes to
the real per-user progress file, so rendering it would touch a player's
saved record.

## Releases

Version comes from `git describe`, not a hand-edited literal. Pushing a
`vX.Y.Z` tag builds all four plugins on three OSes, packages the
installers, and publishes a GitHub Release. The in-app update check
compares dotted-integer components and treats anything after `-` or `+`
as metadata, so a build between tags still sees a newer release and a
downgrade is impossible. See
[ADR 007](decisions/007-update-checker.md) and
[ADR 012](decisions/012-versioning.md).

Builds are **unsigned**. Gatekeeper and SmartScreen will warn until
someone does code signing and notarization — real, separate, unstarted
work.
