# Ear Trainer / Learner EQ / Learner Comp / Learner Verb — project notes

Four JUCE plugins in one repo/CMake build, all VST3/AU/Standalone:

- **EarTrainer** — multiple-choice ear-training games (9 today: EQ,
  compression, reverb type, pan position, delay time, distortion type,
  stereo width, gain/dB, named frequency range).
- **LearnerEQ** — a real 4-band EQ that processes the host's own audio,
  with a live spectrum + response-curve display, a scrolling waveform,
  short contextual tooltips while dragging a band's frequency knob, a
  Bypass toggle, and a guided Lesson.
- **LearnerComp** — a real compressor processing the host's own audio,
  with a live spectrum, a scrolling waveform highlighting where it's
  reducing gain, a GR/peak meter row, contextual tooltips, 4 teaching
  presets, a Bypass toggle, and a guided Lesson.
- **LearnerVerb** — a real reverb (Room/Hall/Plate/Spring) processing the
  host's own audio, the same live-spectrum + scrolling-waveform/peak-meter
  view as LearnerComp, contextual tooltips, 4 teaching presets, a Bypass
  toggle, and a guided Lesson.

All three Learner plugins now share the same visualization shape (live
spectrum, then waveform + peak meters) and the same Bypass/Lesson button
placement — see [decisions/006](docs/decisions/006-unified-visualization.md).
All four plugins also have an "Updates" button (manual, no background
timer) that checks GitHub for a newer release — see
[decisions/007](docs/decisions/007-update-checker.md). CI now uploads a
downloadable build artifact per OS on every push, and publishes a GitHub
Release when a `vX.Y.Z` tag is pushed — see
[docs/diagrams/ci-pipeline.md](docs/diagrams/ci-pipeline.md). All four
editors, plus `shared/LessonController`, now share one dark theme
(`shared/AbcTrainLookAndFeel`) instead of each Learner plugin picking its
own one-off accent colour — see
[decisions/009](docs/decisions/009-look-and-feel.md).

Longer-term product direction (not all built): a learning ecosystem of
"teaching" plugins (LearnerEQ/LearnerComp/LearnerVerb done, LearnerSat
planned) paired with the trainer games and a lightweight in-plugin
knowledge base, plus a phase-2 AI module that analyzes a reference track
and suggests which teaching plugins to try. Treat any specific
market-size/competitor claims from prior planning conversations as
unverified marketing copy, not fact — re-check before making decisions
that depend on them.

## Docs directory

`docs/roadmap.md` (status of everything, done vs. planned),
`docs/library_catalog.md` (bibliography of the user's local audio-
engineering book collection, title/author/topic only — no book text was
ever extracted, see ADR 010), `docs/knowledge_base.md` (original
reference material — general, widely-taught audio-engineering knowledge,
not derived from any specific book — that the in-plugin tooltip text in
`ParameterGuide.h`/`ReverbGuide.h`/`FrequencyGuide.h` is written from;
keep all four in sync), `docs/diagrams/` (mermaid: system overview,
game-engine class diagram, learner-plugin component diagrams for
LearnerEQ/LearnerComp/LearnerVerb, proposed CI pipeline),
`docs/decisions/` (ADRs: 001 is the `Game`
interface choice, 002 is `setDifficulty`/`ProgressManager`, 003 is why
LearnerComp has a custom compressor engine instead of
`juce::dsp::Compressor`, 004 is LearnerVerb's trimmed visualization scope
and its decay-to-`roomSize` approximation, 005 is the `MicroLesson`/
`LessonController` split and why per-control highlighting was cut, 006 is
unifying the spectrum/waveform/bypass shape across all three Learner
plugins and why that's not tested with a real `SpectrumAnalyzerComponent`,
007 is CI artifacts/releases plus the manual-only "Check for Updates"
button and why it needs `NEEDS_CURL TRUE` on Linux, 008 is the per-OS
installers (`.pkg`/DMG, Inno Setup, `tar.gz`) and why macOS can't offer a
free-text custom install path the way Windows/Linux can, 009 is the
shared `AbcTrainLookAndFeel` dark theme applied to all four editors and
what was deliberately deferred from that first redesign pass, 010 is why
the book-library work stops at a bibliography and never extracts or
quotes book text, 011 is the flat-JSON-per-language i18n system, why it's
scoped to a curated core string set rather than every tooltip, and a real
UTF-8-literal bug it caught, 012 is deriving the version from `git
describe` instead of a hand-bumped literal, the stable/beta/dev channel
detector, and what's still deferred (CI channel wiring, settings
migration), 013 is the phased (one library/one component at a time)
adoption of third-party UI libraries, and the finding that the originally
-named "juce_animate" library doesn't exist, 014 is a real button-collapse
bug found by actually running the app, the "Updates" button always
showing an outcome, a childish-colour-palette fix, and manual level
control, 015 is the drag-to-select `ChoiceSliderComponent` replacing every
game's button row, the opt-in `ReferenceAudioLibrary`/`TestSignalGenerator`
infrastructure for training on user-supplied reference audio instead of
pink noise, and why that infrastructure never fetches, bundles, or vets
the legality of any audio file itself, 016 is the programmatic
`shared/AppIcons` vector icon set for every game/plugin, a soundkorb.ru
site link added to all four editors, and why a full design-system/Figma
pass from the user's UI-overhaul brief is explicitly out of scope for
this codebase to produce on its own, 017 is folding a user-supplied
90-rule audio-engineering knowledge base into `docs/knowledge_base.md`/
tooltips/instructions/lessons in this project's own words, a real pre-
existing lesson-overlay z-order bug found while touching that code, and
real per-plugin `ICON_BIG` app icons replacing the previous blank/default
one, 018 is a bounded gradient/shadow/glow UI polish pass across all four
editors plus five originally-synthesized built-in training samples
(`assets/samples/`) exposed through the existing opt-in
`ReferenceAudioLibrary`, and why a folder of real commercial "name your
price" albums pointed at during that same request was deliberately never
read or embedded, 020 is the optional continuous-answer mode on the
`Game` interface (a tolerance band that narrows with difficulty, and why
its unit differs per game), 021 is training runs having a shape
(Practice/Survival/Blitz, lives, auto-advance) plus the Home->Training
screen split and why "what interests you" is a star rather than a
first-run questionnaire, 019 is the `shared/AbcTrainTheme` design-token layer, a
genuinely designed (not inverted) light theme, `shared/WidgetStateRegistry`
solving the eased-hover problem ADR 018 recorded as unsolvable, tracked
typography, gradient-filled visualisations, the `GainReductionMeter`,
the blur-backed `GuideTooltip`, and two more bugs found only by running
the app), `docs/diagrams/i18n-architecture.md`
(how a language choice becomes visible text, per ADR 011). `BETA_TESTING.md` and
`.github/CONTRIBUTING.md` (the latter bilingual EN/RU) are top-level, not
under `docs/` - repo-presentation files GitHub itself looks for/surfaces
specially, same reasoning as `LICENSE` and the `.github/ISSUE_TEMPLATE/`
pair.
`docs/testing-strategy.md`. This file (`CLAUDE.md`) stays the per-file
breakdown; `docs/` is the higher-level/visual layer — keep both in sync
when the architecture changes rather than letting one drift.

## Build

CMake + JUCE via `FetchContent` (pinned to tag `8.0.15` in
`CMakeLists.txt` — no local JUCE checkout needed). `cmake -B build &&
cmake --build build` builds all four plugin targets (`EarTrainer`,
`LearnerEQ`, `LearnerComp`, `LearnerVerb`) from the one root
`CMakeLists.txt`. This sandbox actually has Homebrew + `clang++` (and
`brew install cmake` works), so a full local build is possible here, not
CI-only — worth doing on any non-trivial change; see
[decisions/007](docs/decisions/007-update-checker.md) for a case where it
caught a real bug CI's logs couldn't be inspected for (no GitHub auth
token in this environment, and GitHub refuses raw Actions logs to an
unauthenticated request even on a public repo). All four `juce_add_plugin`
targets pass `NEEDS_CURL TRUE` (needed on Linux only, for the update
checker's HTTPS call — see the same ADR); `EarTrainerTests` deliberately
doesn't, since it never makes the real network call.

## Architecture — EarTrainer (`Source/`)

Every exercise implements a common `Game` interface
(`Source/Games/Game.h`): play a processed test signal, offer N labeled
choices, score the player's pick. This lets one generic editor and one
`GameManager` drive every exercise — see `docs/architecture.md` for the
full rationale.

- `Source/Games/Game.h` — the interface: `prepare`/`process`,
  `setDifficulty(int level)` (1-10, see ADR 002), `newRound`/
  `submitAnswer(int)`, `getNumChoices`/`getChoiceLabel(int)`, answer/
  feedback getters, score getters. Is a `juce::ChangeBroadcaster`. Also
  carries an **optional continuous-answer mode**
  (`usesContinuousScale`/`getToleranceNormalised`/`getCorrectNormalised`/
  `getChosenNormalised`/`formatNormalisedValue`/`submitNormalisedAnswer`/
  `getGridMarks`) for the four games whose skill is a *value* rather than
  a category — everything in normalised 0..1 axis space, with the game
  owning the mapping to real units so the slider stays a dumb ruler. All
  non-pure-virtual with inert defaults, same shape as
  `setReferenceAudioLibrary`, so the five categorical games and every
  existing `submitAnswer(int)` call site are untouched. See
  [decisions/020](docs/decisions/020-continuous-answers.md).
- `Source/Games/EQGame.{h,cpp}` — "find the frequency": pink noise through
  an `IIR` peak filter at a frequency drawn **log-uniformly across the
  whole 100 Hz–12.8 kHz range**, not snapped to one of eight octave
  centres (fixed centres were memorisable as positions without learning
  what a frequency sounds like). Continuous scale, log axis running half
  an octave past each end so 100 Hz and 12.8 kHz sit inside it.
  `setDifficulty` scales *both* the boost/cut (9/6/3 dB) and — the real
  lever now — the accept band: ±1.0 → 0.6 → 0.35 **octaves**, so the
  slack is the same ratio at 200 Hz as at 8 kHz. The eight octave
  frequencies survive as the emphasised grid marks, which is also what
  keeps the legacy `submitAnswer(int)` path working unchanged.
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
- `Source/Games/PanGame.{h,cpp}` — "guess the pan position": pink noise
  panned with an equal-power law (`gainL=cos(theta)`, `gainR=sin(theta)`,
  which is loudness-equalized for free) to one of 5 positions (Hard
  Left/Left/Center/Right/Hard Right). Now **continuous**: the target sits
  anywhere in -1..+1 rather than on one of five named points ("Left" is
  not a pan value), on a linear axis since equal-power panning already
  makes perceived position track the control linearly. Reads as C / L86 /
  R40. `setDifficulty` narrows the accept band (±0.35 → 0.22 → 0.12 of
  the field); the three position tables remain, driving only the legacy
  discrete path.
- `Source/Games/DelayGame.{h,cpp}` — "guess the delay time": a percussive
  noise burst through a `juce::dsp::DelayLine`, feedback 0, fixed 50%
  dry/wet. Now **continuous**: any time in 20–640 ms (quantised to 5 ms so
  the answer stays nameable), on a **log** axis, with the tolerance as a
  **ratio** (±35% → 22% → 13%) rather than a fixed number of milliseconds
  — being 20 ms out at 40 ms and at 500 ms are completely different
  mistakes. `setDifficulty` still also shortens the burst repeat period
  (1.4s → 0.9s → 0.6s). The four original times (50/150/300/500 ms)
  remain as grid marks and drive the legacy discrete path.
- `Source/Games/DistortionGame.{h,cpp}` — "guess the distortion type": a
  waveshaper applied to pink noise, one of 4 types (Soft Clip = `tanh`,
  Hard Clip = `jlimit`, Tape Saturation = `tanh` + a post-clip lowpass,
  Overdrive = asymmetric `tanh` for an even-harmonic character), each
  with its own makeup gain so loudness isn't a tell. `setDifficulty`
  scales the pre-shaping drive amount (6.0 → 3.0 → 1.5) rather than
  swapping types — same "fixed labels, scaled parameter" shape as
  `DelayGame`.
- `Source/Games/StereoWidthGame.{h,cpp}` — "guess the stereo width": mid/
  side processing (`side *= width`) on **two independent**
  `PinkNoiseGenerator`s (see below — a single mono source duplicated to
  both channels would have zero side signal, making width meaningless) to
  one of 4 named widths (Narrow/Normal/Wide/Extra Wide, no numbers
  shown). `setDifficulty` converges the underlying width multipliers
  toward 1.0 at higher tiers, same shape as `PanGame`.
- `Source/Games/DBGame.{h,cpp}` — "guess the gain change": a dB offset
  (`Decibels::decibelsToGain`) applied to pink noise. Now **continuous**:
  any value in -9..+9 dB, quantised to 0.5 dB, on a linear axis since dB
  is already the perceptual unit. `setDifficulty` narrows the accept band
  (±2.5 → 1.5 → 1.0 dB) — and deliberately stops at 1 dB rather than
  going lower, because below roughly that a level difference stops being
  reliably audible at all, so a tighter band would test luck rather than
  hearing. The 5 stepped choices remain for the legacy discrete path.
- `Source/Games/FrequencyRangeGame.{h,cpp}` — "name the range": a peak
  filter boosts or cuts a frequency chosen log-uniformly *within* one of
  7 standard named ranges (Sub-bass/Bass/Low-mids/Mids/High-mids/
  Presence/Air — the same names `LearnerEQ`'s `FrequencyGuide` and
  `docs/knowledge_base.md` use), applied to pink noise. Closest relative
  is `EQGame` — `setDifficulty` follows the exact same shape (9/6/3 dB by
  tier, 7 choices never change) — but the boosted frequency itself moves
  around inside the chosen range each round rather than landing on one
  of 8 fixed band centers, so the player has to learn the range's actual
  boundaries instead of memorizing fixed points.
- `Source/PinkNoiseGenerator.h` — shared pink-noise source (Paul Kellet
  economy algorithm) used by `StereoWidthGame`'s two channels directly;
  each instance owns its own `juce::Random`, which is what lets that game
  use two decorrelated instances for a real side signal.
- `Source/TestSignalGenerator.h` — drop-in replacement for
  `PinkNoiseGenerator` (identical `nextSample()` shape) used by the other
  8 games. Plays looped audio from `ReferenceAudioLibrary::getActiveBuffer()`
  when a player has selected a reference file (see below), otherwise falls
  back to real pink noise — real-time safe (one atomic pointer load, no
  I/O) either way. `StereoWidthGame` keeps a plain `PinkNoiseGenerator`
  instead, since a single recorded file can't provide the two
  independently-decorrelated sources its mid/side processing needs. See
  [decisions/015](docs/decisions/015-choice-slider-and-training-sounds.md).
- `Source/ReferenceAudioLibrary.{h,cpp}` — scans a root folder (default:
  the user's own music folder + `/ABCTrain`) for one subfolder per
  category, each holding audio files the *user* supplies; loads a
  selection (resampled/downmixed, message-thread only) and publishes it
  to the audio thread via `std::atomic<const AudioBuffer<float>*>`, the
  same pattern LearnerEQ's processor already uses for its spectrum/
  waveform registration. Never fetches, bundles, or vets the legality of
  any file itself — see decisions/015 for why that boundary matters here.
  `addBuiltInCategories()` (called at the start of every `rescan()`)
  re-materialises five originally-synthesized WAV files embedded via the
  `SampleData` binary-data target (`assets/samples/Percussive/`,
  `assets/samples/Sustained/`) into real cache files under
  `tempDirectory/abcTrain/BuiltInSamples/`, and injects them as two
  always-present categories ("Built-in Percussive", "Built-in Sustained")
  ahead of anything found on disk — a real non-noise training option with
  zero setup, and the reason the file-chooser button below exists. See
  [decisions/018](docs/decisions/018-ui-polish-and-builtin-samples.md).
- `Source/TrainingSoundsComponent.{h,cpp}` — the "Choose Training Sounds"
  overlay (same full-size show/hide shape as `shared/LessonController`):
  a "Choose Folder..." button (`juce::FileChooser::launchAsync`,
  directory-select mode, `Component::SafePointer`-guarded) calling
  `ReferenceAudioLibrary::setRootFolder()`, a Pink Noise button, and one
  button per detected category (built-in categories always first, then
  whatever the chosen folder's subfolders contain), locked/unlocked by
  `ProgressManager::getMaxLevelReached()`. Reachable via a "Training
  Sounds" button in `PluginEditor`'s title row.
- `Source/GameManager.{h,cpp}` — owns all registered `Game`s, tracks the
  active one, prepares *all* games up front in `prepare()` so switching
  games never needs an audio-thread re-prepare (also (re)loads whatever
  reference-audio selection was persisted, now that the real sample rate
  is known). Also exposes `getNumGames()`/`getGame(int)` (so
  `ProgressManager` can listen to every game, not just the active one),
  `setDifficultyForAllGames(int)`, and `getReferenceAudioLibrary()` (the
  one `ReferenceAudioLibrary` shared by every game, wired in via
  `Game::setReferenceAudioLibrary` — a non-pure-virtual, default-no-op
  hook on the `Game` interface, so every existing/future game keeps
  working unmodified unless it opts in).
- `Source/ProgressManager.{h,cpp}` — cross-session points/level(1-10)/
  streak/daily-challenge, backed by `juce::PropertiesFile`. Listens to
  every game via `ChangeListener`; on a correct/incorrect answer it calls
  its own `registerAnswer(gameIndex, wasCorrect)`, which is also the
  direct entry point `ProgressManagerTest` uses (see the Testing section
  below for why). On level-up, calls
  `gameManager.setDifficultyForAllGames(level)`. Points-to-next-level is a
  triangular scale (level *L* needs *100·L* points to reach *L+1*, so
  each level is progressively harder). `setLevelManually(int)` lets a
  player jump straight to a level instead of only reaching it via points
  — it sets `totalScore` to that level's exact threshold
  (`pointsRequiredForLevel`) rather than adding a second, independent
  notion of "level", so the two can never disagree; wired to a
  `levelSelector` `ComboBox` in EarTrainer's editor (see
  [decisions/014](docs/decisions/014-eartrainer-usability-fixes.md)).
  Games themselves know nothing about points or levels — kept out of the
  `Game` interface deliberately, see ADR 002 for the one thing that *did*
  need to go in (`setDifficulty`). Also keeps **lifetime per-exercise
  stats** (`GameStats`: rounds, correct, best streak, best Survival/Blitz
  score) and a **favourite flag** per game, both persisted and both
  surfaced on the home screen's cards. Deliberately separate from each
  `Game`'s own `getScore()`/`getRoundsPlayed()`, which stay in-memory
  session counters. Keyed by game *index*, so new games must be
  **appended** to `GameManager`'s registration list, never inserted —
  nothing in the code enforces that, see
  [decisions/021](docs/decisions/021-sessions-and-navigation.md).
- `Source/PluginProcessor.{h,cpp}` — ignores host input entirely;
  generates its own test signal via `GameManager::process`. `signalEnabled`
  starts **off** - it used to default to true, which meant an audible
  half-second of noise on launch, before any editor existed to say
  otherwise. Also owns the **breathing gate**: for the six games whose
  signal is continuous (i.e. `Game::hasOwnRepeatPause()` is false), the
  output is faded down for ~0.9s after every ~2.4s of sound, because an
  unbroken loop stops the ear resolving anything within a minute. The
  three burst games (compression, reverb, delay) already stop and start
  on their own and are left alone; `restartSignalCycle()` puts the gate at
  the top of its sound phase whenever a round begins. Owns
  `GameManager` then `ProgressManager` in that declaration order (matters
  — `ProgressManager`'s constructor registers listeners on every game).
- `Source/ChoiceSliderComponent.{h,cpp}` — the answer-selection widget:
  one horizontal track with an evenly-spaced tick per choice, a big label
  showing whichever choice is currently highlighted, and a draggable
  thumb that snaps to the nearest tick on release. Needs nothing from a
  `Game` beyond `getNumChoices()`/`getChoiceLabel(int)`, so it's identical
  across all 9 games whether the labels are numbers or names. Replaced
  the old row of separate `TextButton`s — see
  [decisions/015](docs/decisions/015-choice-slider-and-training-sounds.md)
  for the redesign rationale and two real layout bugs (edge-label
  clipping; `paint()` silently not using the same inset math as the mouse
  handlers) found only by actually running the app. A correct answer now
  fades a soft glow out around the thumb over ~900ms; a wrong answer gives
  the thumb and big label a brief, decaying wobble instead of a flat
  colour swap — both driven by `juce::Animator` (see
  [decisions/018](docs/decisions/018-ui-polish-and-builtin-samples.md)),
  the same `juce_animation` module already used by `LevelProgressBar`'s
  fill transition below.
- `Source/Achievements.{h,cpp}` — the named things a player can earn, as
  pure data plus pure rules over a `Snapshot` of what `ProgressManager`
  already records. No `Component`, no `PropertiesFile`, no message loop,
  so `tests/AchievementsTest` drives every rule directly. Ids are strings
  (the persistence key, stable across reordering); earned ids are never
  removed; accuracy rules carry a minimum-rounds floor so three lucky
  answers can't earn the hardest-sounding badge; and there is deliberately
  **no "reach level N"** achievement, since level is player-selectable
  from a dropdown. `ProgressManager` owns the earned set, re-checks after
  anything that could earn one, backfills silently on load, and reports
  new ones through `onAchievementEarned`. `Source/AchievementToast.h` is
  the only thing that appears on the training screen unasked. See
  [decisions/024](docs/decisions/024-achievements-and-a-quiet-training-screen.md).
- `Source/SessionManager.{h,cpp}` — the shape of one training run:
  Practice (unlimited), Survival (3 lives, a wrong answer costs one, ends
  at zero), Blitz (90s clock, a wrong answer costs 5 seconds rather than
  ending the run — the pressure is pace, not caution). Also owns the
  auto-advance delay (~0.9s after correct, ~1.9s after wrong, none once a
  run has ended). Pure state: no `Game`, no `Component`, no message loop,
  which is why `tests/SessionManagerTest` can drive every path directly.
  See [decisions/021](docs/decisions/021-sessions-and-navigation.md).
- `Source/HomeScreenComponent.{h,cpp}` — the screen you land on, replacing
  the old `ComboBox` game selector entirely. Header (level, streak), then
  trainings grouped by the **skill they build** (Frequency / Dynamics /
  Space & stereo / Character) rather than by registration order, each as a
  card with its icon, one line on what it gives you, and your own record.
  A star per card pins it to a "Your focus" group above everything else —
  the "choose what interests you" idea without a first-run questionnaire
  whose answers go stale (see ADR 021). Lives in a `juce::Viewport`: nine
  trainings across four categories already exceed the window.
- `Source/PluginEditor.{h,cpp}` — fully generic: two screens (Home ⇄
  Training, exactly one visible at a time),
  a single `ChoiceSliderComponent` rebuilt via `setChoices()` on switch
  *or* whenever a fresh round's choice count no longer matches (needed
  once `ReverbGame`'s choice count became difficulty-dependent), no
  per-game editor code. Being one persistent component (not destroyed/
  recreated per round like the old buttons) means the fadeIn-collapse bug
  class from [decisions/014](docs/decisions/014-eartrainer-usability-fixes.md)
  can't recur here. Also shows level/progress-bar/streak/daily-challenge
  from `ProgressManager` — the progress bar (`LevelProgressBar`, nested in
  this same header) now "breathes": a slow, low-amplitude glow pulse at
  the fill's leading edge via a plain 30Hz `Timer`, independent of its
  existing eased fill-transition `Animator` (see
  [decisions/018](docs/decisions/018-ui-polish-and-builtin-samples.md)) —
  including a `levelSelector` `ComboBox` (1-10)
  that calls `ProgressManager::setLevelManually` directly, so difficulty
  is player-controllable, not just an automatic side effect of points —
  a mode selector (Practice/Survival/Blitz) with a lives/clock readout,
  an "Updates" button (`shared/UpdateChecker`, see
  [decisions/007](docs/decisions/007-update-checker.md)) that now always
  shows "Checking..." → a result → (on no response within 6s) "Couldn't
  check" (see decisions/014), and a "Training Sounds" button toggling
  `TrainingSoundsComponent` — added as the *last* child component
  specifically so it paints on top of everything else (see decisions/015
  for the real z-order bug this fixes).

Adding a new exercise: create `Source/Games/NewGame.{h,cpp}` implementing
`Game` (including a real `setDifficulty` — there's no default),
**append** it to `GameManager`'s constructor, add the two files to
`CMakeLists.txt`, and add it to `categoryForGame()` in
`Source/PluginEditor.cpp` so it lands in a home-screen group (anything
unrecognised falls into "Character"). Append rather than insert:
`ProgressManager`'s per-exercise stats and favourites are keyed by index,
so reordering the list shuffles every player's saved record. No
processor changes needed.

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
  mapping used by the response curve and the highlighted-band region so
  they line up with each other and with the spectrum drawn underneath,
  plus the per-frequency-range guide text shown while dragging a band's
  frequency knob (2-4 original sentences with practical values, plus a
  "Learn more" book pointer — mirrors
  [docs/knowledge_base.md](docs/knowledge_base.md)'s "Эквализация"
  section, see decisions/010). The editor's `guideLabel` was made taller
  (24px → 52px, window height +28px) to fit this without truncating.
- `LearnerEQ/Source/PluginProcessor.{h,cpp}` — 4 `ProcessorDuplicator`
  filters run in series on the real audio block (skipped entirely when
  the `bypass` APVTS param is on); recomputes coefficients from current
  APVTS values once per block (not per-sample). Feeds a mono-summed copy
  of the (post-filter) output to whatever `SpectrumAnalyserComponent` the
  editor registered via `setSpectrumAnalyser`, and (dry, wet) per sample
  to whatever `WaveformDisplay` the editor registered via
  `setWaveformDisplay` (both raw `std::atomic<T*>`, null-checked, since
  the editor can be closed while the processor keeps running). A
  `dryBuffer` member captures the pre-filter signal each block
  specifically so the waveform's input trace still shows the untreated
  signal even when bypass is off.
- `LearnerEQ/Source/SpectrumAnalyser.{h,cpp}` — `SpectrumAnalyserComponent`
  now *extends* `shared/SpectrumAnalyzerComponent` (see
  [decisions/006-unified-visualization.md](docs/decisions/006-unified-visualization.md)),
  adding only the combined 4-band response curve (via `EQCoefficients::make`
  + `Coefficients::getMagnitudeForFrequency`) and a translucent highlighted
  region for whichever band is being dragged, both drawn via an overridden
  `paintOverlay()`. The FFT/FIFO/30 Hz-timer spectrum itself now lives in
  `shared/SpectrumAnalyzer.{h,cpp}`.
- `LearnerEQ/Source/PluginEditor.{h,cpp}` — 4 columns of freq/gain/Q
  rotary sliders bound with `SliderAttachment`, a `WaveformDisplay` +
  input/output peak labels below the spectrum, a Bypass `ToggleButton`
  (`ButtonAttachment`), and an "Updates" button (`shared/UpdateChecker`) —
  both placed next to the Lesson button in the title row.
  `onDragStart`/`onValueChange`/`onDragEnd` on each band's freq slider
  drive the guide label text (via `FrequencyGuide::describe`) and
  `spectrum.setHighlightedBand`. A 30 Hz editor timer pushes current
  parameter values into the spectrum component (so the response curve
  tracks knob movement even with no audio playing) and refreshes the peak
  labels from the waveform display.
- `LearnerEQ/Source/PluginEntry.cpp` — just `createPluginFilter()`,
  deliberately split out of `PluginProcessor.cpp` (see the Testing section
  below for why).
- `LearnerEQ/Source/VocalEqLesson.h` — `buildVocalEqLesson()`, a
  `MicroLesson` (flat → boost 60 Hz low shelf warmth → boost 3 kHz
  presence → cut 250 Hz mud → boost 10 kHz air → compare) driving a
  `lessonSelector`/`LessonController` overlay in the editor.
  `LearnerEQ/Source/FindResonanceLesson.h` — `buildFindResonanceLesson()`,
  a second lesson (boost narrow at 400 Hz to find a resonance → flip it
  into a cut → widen the Q). See the Microlessons section below.

Not yet built for LearnerEQ: "analyze reference" mode, knowledge-base
tooltips beyond the one-line frequency description, more than one lesson.

## Architecture — LearnerComp (`LearnerComp/Source/`)

Same shape as LearnerEQ (real audio, APVTS parameters, host-automatable)
but for dynamics instead of frequency. See
[decisions/003-learnercomp-engine.md](docs/decisions/003-learnercomp-engine.md)
for why it doesn't use `juce::dsp::Compressor`.

- `LearnerComp/Source/CompressorEngine.h` — custom feed-forward
  compressor: soft-knee gain computer (Giannoulis/Massberg/Reiss formula)
  plus one-pole attack/release smoothing on the gain-reduction envelope
  itself (not on the input level). `computeGain(detectionSample)` returns
  a gain factor rather than a processed sample, so the processor can feed
  it a stereo-linked detection value (loudest channel) and apply the
  *same* gain to every channel, avoiding stereo-image pumping.
  `getLastGainReductionDb()` exposes the smoothed envelope for the meter
  and waveform highlight — the whole reason this isn't `juce::dsp::Compressor`,
  which exposes neither knee nor its internal gain reduction.
- `LearnerComp/Source/ParameterGuide.h` (`CompressorGuide` namespace) —
  tooltip text per parameter ID (2-4 original sentences with practical
  values, plus a "Learn more" book pointer — mirrors
  [docs/knowledge_base.md](docs/knowledge_base.md)'s "Компрессия и
  динамическая обработка" section, see decisions/010), plus the 4 preset
  definitions (Vocal Smoothing/Punchy Drums/Bass Control/Limiter:
  threshold, ratio, attack, release, knee). The editor's `guideLabel` was
  made taller (20px → 48px, window height +28px) to fit the longer text.
- `LearnerComp/Source/PluginProcessor.{h,cpp}` — 8 APVTS params
  (threshold/ratio/attack/release/knee/makeup/dryWet/bypass).
  `processBlock` computes one stereo-linked detection value per sample and
  a mono downmix of the *input* signal (fed to whatever
  `SpectrumAnalyzerComponent` the editor registered via
  `setSpectrumAnalyzer`, in both the normal and bypassed branches — see
  [decisions/006](docs/decisions/006-unified-visualization.md) for why
  the spectrum shows input, not output, here), gets a gain from
  `CompressorEngine`, applies it to every channel with dry/wet blending,
  and (unless bypassed) pushes (input, output, gainReductionDb) to
  whatever `WaveformDisplay` the editor registered. Bypass skips the
  engine entirely and passes audio through unchanged. `applyPreset(int)`
  lives here (not just in the editor's button handler) specifically so
  it's unit-testable without constructing a `Component`.
- `shared/WaveformDisplay.{h,cpp}` — FIFO-accumulate/30 Hz-timer-flush
  pattern (same shape `shared/SpectrumAnalyzer` uses for its FFT), for a
  scrolling peak-based dual waveform: gray input trace, output trace
  tinted from blue to red proportional to a generic `highlightAmount`
  (LearnerComp passes gain reduction) in that ~33 ms column. Also the
  source of the peak-meter readouts (via `getInputPeak`/`getOutputPeak`/
  `getCurrentHighlightAmount`). Used by all three Learner plugins.
- `shared/SpectrumAnalyzer.{h,cpp}` — `SpectrumAnalyzerComponent`: the
  live-spectrum FFT/FIFO/30 Hz-timer machinery, extracted from what used
  to be LearnerEQ-only `SpectrumAnalyserComponent` once LearnerComp and
  LearnerVerb both wanted a plain live spectrum too (no response curve, no
  highlighting — that stays LearnerEQ-specific via a `paintOverlay()`
  override, see the LearnerEQ section above). Used directly, unsubclassed,
  by LearnerComp and LearnerVerb.
- `LearnerComp/Source/PluginEditor.{h,cpp}` — a `SpectrumAnalyzerComponent`
  above the waveform, 7 rotary knobs (one per float param), a Bypass
  `ToggleButton` and an "Updates" button (`shared/UpdateChecker`) next to
  the Lesson button in the title row, and 4 preset buttons, each preset
  button just calling `processor.applyPreset(i)`. Guide label updates via
  `onDragStart`/`onDragEnd` on each knob, same pattern as LearnerEQ.
- `LearnerComp/Source/PluginEntry.cpp` — just `createPluginFilter()`, same
  reason as LearnerEQ's.
- `LearnerComp/Source/VocalCompressionLesson.h` — `buildVocalCompressionLesson()`,
  a `MicroLesson` (bypass → threshold -18 dB → ratio 3:1 → attack 5 ms →
  release 150 ms → knee 6 dB → makeup +4 dB).
  `LearnerComp/Source/BusGlueLesson.h` — `buildBusGlueLesson()`, a second
  lesson (2:1 ratio, high threshold → 30 ms attack → soft knee → aim for
  only a couple dB of gain reduction → makeup +2 dB).

Not yet built for LearnerComp: knowledge-base content beyond the one-line
tooltips, more than one lesson, any automated coverage of attack/release
*transient* behavior (only steady-state math is tested).

## Architecture — LearnerVerb (`LearnerVerb/Source/`)

Same shape again (real audio, APVTS parameters, host-automatable), for
space instead of dynamics. See
[decisions/004-learnerverb-scope.md](docs/decisions/004-learnerverb-scope.md)
for what was deliberately cut from the first build and why the `Decay`
knob isn't a precise physical measurement.

- `LearnerVerb/Source/ReverbEngine.h` — Room/Hall/Plate via
  `juce::dsp::Reverb` (Freeverb-derived); Spring via a cascade of 4
  resonant allpass filters, the same technique `EarTrainer`'s `ReverbGame`
  uses for its Spring type, reimplemented here (not literally shared —
  `ReverbGame`'s version is tightly coupled to its per-round game model,
  this one needs continuous live parameter control). `Decay` (seconds) is
  mapped onto `roomSize` by ear, since Freeverb has no literal
  decay-in-seconds parameter — an approximation, not a physical model,
  same "tuned, not measured" precedent as `CompressionGame`/`ReverbGame`'s
  presets. A `juce::dsp::DelayLine` implements pre-delay ahead of whichever
  algorithm is selected. Always renders 100% wet; `PluginProcessor` blends
  dry/wet itself, same division of responsibility as `CompressorEngine`.
- `LearnerVerb/Source/ReverbGuide.h` (`ReverbGuide` namespace) — tooltip
  text per parameter ID (2-4 original sentences with practical values,
  plus a "Learn more" book pointer — mirrors
  [docs/knowledge_base.md](docs/knowledge_base.md)'s "Реверберация и
  пространство" section, see decisions/010), plus the 4 preset
  definitions (Vocal Ambience/Concert Hall/Small Room/Spring Tank: type,
  decay, pre-delay, size, damping, dry/wet, width). The editor's
  `guideLabel` was made taller (20px → 48px, window height +28px) to fit
  the longer text.
- `LearnerVerb/Source/PluginProcessor.{h,cpp}` — 8 APVTS params (type as
  an `AudioParameterChoice`, decay/preDelay/size/damping/dryWet/width as
  floats, plus `bypass`). `processBlock` makes a wet-only copy of the
  block (`wetBuffer.makeCopyOf`), runs `ReverbEngine::process` on the copy
  *every block regardless of bypass* (so the tail's internal state stays
  warm), then blends wet/dry per sample into the real buffer —
  `ReverbEngine` never needs to know about `Dry/Wet` at all. Bypass forces
  the effective wet fraction to 0 without touching the stored `Dry/Wet`
  value, so un-bypassing restores whatever the user had dialled in (see
  [decisions/006](docs/decisions/006-unified-visualization.md)). Also
  feeds a mono downmix of the input to whatever `SpectrumAnalyzerComponent`
  the editor registered via `setSpectrumAnalyzer`, and pushes
  (dry, blended-output) to whatever `WaveformDisplay` the editor
  registered, with `highlightAmount` left at its default (no
  gain-reduction-style concept here). `applyPreset(int)` lives here for
  the same testability reason as LearnerComp's.
- `LearnerVerb/Source/PluginEditor.{h,cpp}` — a `SpectrumAnalyzerComponent`
  above the waveform, a `ComboBox` for Type (`ComboBoxAttachment`, items
  added manually to match the choice parameter — attachments don't
  auto-populate the combo box) + 6 rotary knobs, a Bypass `ToggleButton`
  and an "Updates" button (`shared/UpdateChecker`) next to the Lesson
  button in the title row, and 4 preset buttons. Same guide-label/tooltip
  pattern as the other two Learner plugins.
- `LearnerVerb/Source/PluginEntry.cpp` — just `createPluginFilter()`, same
  reason as the other two.
- `LearnerVerb/Source/VocalSpaceLesson.h` — `buildVocalSpaceLesson()`, a
  `MicroLesson` (dry → Plate 1.5 s/20% wet → pre-delay 40 ms → damping
  70% → narrow width to 50% → compare with Hall 2.5 s).
  `LearnerVerb/Source/BrightVsDarkTailLesson.h` — `buildBrightVsDarkTailLesson()`,
  a second lesson (Hall with low damping stays bright throughout its
  decay → raise damping a lot at the same decay time → the high end now
  dies out much faster than the low end).

Not yet built for LearnerVerb (deliberately trimmed, see ADR 004):
impulse-response "cloud" visualization, decay-vs-frequency graph, stereo
correlometer/vectorscope, knowledge-base content beyond one-line tooltips,
more than one lesson.

## Architecture — Microlessons (`shared/`, per-plugin lesson content)

See [decisions/005-microlesson-architecture.md](docs/decisions/005-microlesson-architecture.md)
for the full rationale; summary here.

- `shared/MicroLesson.h` — pure state machine, no APVTS/UI dependency:
  a title, a `std::vector<LessonStep>` (`explanationText` +
  `(parameterID, value)` pairs, a plain aggregate), and a step cursor
  (`start`/`nextStep`/`previousStep`). This is what `tests/MicroLessonTest.cpp`
  exercises directly, with none of the message-loop/GUI-instantiation
  concerns documented in the Testing section below.
- `shared/LessonController.{h,cpp}` — the only thing that touches APVTS or
  draws anything. A `juce::Component` owning one `MicroLesson` and an
  `AudioProcessorValueTreeState&`; on every step change it calls
  `setValueNotifyingHost` for that step's target parameters (same pattern
  `applyPreset` already uses) and updates its text/progress labels.
  Meant to be added as a full-size child of a Learner editor — every
  editor's `resized()` sets its bounds to `getLocalBounds()`
  unconditionally, whether visible or not.
- Each Learner plugin now has **two** lessons (see
  [decisions/017](docs/decisions/017-knowledge-base-content-pass-and-app-icons.md)),
  so each editor owns two `LessonController` members (one per lesson,
  since one `LessonController` instance only ever holds one `MicroLesson`)
  and a small `lessonSelector` `ComboBox` — replacing the old single
  "Lesson" button — picks which one to `showAndStart()`; only the
  selected one is ever visible. Both `LessonController`s'
  `addChildComponent()` calls are the *last* thing each constructor does,
  after every other child (including the "Updates" button and the
  soundkorb.ru link) — a real bug, found while restructuring this exact
  code for the second lesson: the original single-lesson wiring added it
  *before* those controls, so a shown lesson would paint underneath them
  instead of covering them, the same z-order mistake ADR 015/016 already
  had to fix once each for other overlays.
- Lesson **content** (the `build...Lesson()` files listed in each
  plugin's section above — two per plugin now) lives per-plugin, not in
  `shared/` — only the machinery is shared, since the content is
  inherently tied to that plugin's own parameter IDs. Same reasoning as
  `CompressorGuide`/`ReverbGuide`'s preset tables.
- **Per-control highlighting was cut from this pass.** Every target
  parameter already has a `SliderAttachment`/`ComboBoxAttachment`, so
  setting it via `LessonController` makes the matching knob visibly move
  on its own — that motion is the highlight. No highlight-drawing code
  was added to any of the three editors for this.

## Architecture — Update checking (`shared/`, all four plugins)

See [decisions/007-update-checker.md](docs/decisions/007-update-checker.md)
for the full rationale; summary here.

- `shared/UpdateChecker.h/cpp` — `isNewerVersion(latest, current)` (dotted-
  integer version comparison, with or without a leading `v`),
  `parseReleaseJson(json)` (pulls `tag_name`/`html_url` out of GitHub's
  "get latest release" API response), and `parseReleaseListJson(json,
  allowPrerelease)` (walks GitHub's "list releases" array for the first
  entry matching the pre-release constraint) are pure functions with no
  networking or message-thread dependency — this is what
  `tests/UpdateCheckerTest.cpp` exercises directly.
  `checkForUpdatesAsync(currentVersion, channel, callback)` is the one
  impure piece: `Channel::stable` fetches `.../releases/latest` (already
  excludes pre-releases); `Channel::beta` fetches the full `.../releases`
  list and takes the newest entry regardless of its prerelease flag — both
  on a background thread (`juce::Thread::launch`), posting the result back
  via `juce::MessageManager::callAsync`. A 2-argument overload (no
  `channel`) still exists, defaulting to `Channel::stable`, so all four
  editors' existing call sites needed no changes. Any failure (no
  internet, rate limiting, an unexpected response shape) just means the
  callback never fires — no error UI, ever. See
  [decisions/012](docs/decisions/012-versioning.md) — no editor currently
  calls the 3-argument overload or exposes a channel/auto-check setting;
  the fetching logic is real and tested, the UI to pick a channel isn't
  wired up yet.
- `shared/Version.h`/`shared/VersionInfo.h` (the latter CMake-generated
  into the build dir, never committed) — `CurrentVersion::string` is
  derived from `git describe --tags --dirty --always` at CMake configure
  time (see decisions/012), not a hand-bumped literal. Still a plain
  `constexpr const char*`, not `JucePlugin_VersionString`/
  `ProjectInfo::versionString`, for the same reason as before: the three
  Learner `PluginEditor.cpp` files that use it are also compiled into
  `EarTrainerTests`, where `JucePlugin_*` macros aren't defined — the same
  reason `LearnerEQProcessor::getName()` returns a literal instead of
  `JucePlugin_Name` (see `docs/diagrams/ci-pipeline.md`, bug 1).
- `shared/VersionChannel.h` — `detect(version)` turns a `"vX.Y.Z"`-shaped
  string into `stable`/`beta`/`dev` via `std::regex` (JUCE has no
  general-purpose regex class) — pure, CMake-independent, so
  `tests/VersionChannelTest.cpp` exercises it with hand-written example
  strings, no real tagged build needed.
- Each editor wires its own "Updates" `TextButton` directly (duplicated
  across all four editors rather than pulled into a shared UI helper, so
  `shared/UpdateChecker.h` itself stays free of any GUI dependency — same
  reasoning as the Bypass-button wiring already being duplicated across
  the three Learner editors instead of shared). A
  `juce::Component::SafePointer` guards each callback against the editor
  having been closed while the network request was still in flight.
  `juce::AlertWindow::showAsync` with `MessageBoxOptions::makeOptionsOkCancel`
  shows the "Open Release Page" / "Later" prompt only when a newer version
  was actually found.
- **Manual only, no background daily timer** — a deliberate cut from the
  original ask. A plugin making its own unsolicited network calls is a
  bigger step than a button the user chooses to press; some hosts sandbox
  or frown on unexpected plugin network activity. Revisit if manual
  checking turns out to be too easy to forget.

## Architecture — Installers (`installer/`, all four plugins)

See [decisions/008-installers.md](docs/decisions/008-installers.md) for
the full rationale; summary here. CI builds these on every push (see the
CI section below) and attaches them to a GitHub Release when a `vX.Y.Z`
tag is pushed. These package the *already-built* `*_artefacts/Release`
tree — none of the three scripts below build anything themselves.

- `installer/macos/build_installer.sh` — builds one `pkgbuild` component
  package per (plugin × format), 12 total, then `productbuild
  --distribution installer/macos/distribution.xml` combines them into one
  product `.pkg` wrapped in a `.dmg` alongside a double-clickable
  `Open Plugins Folder.command`. `distribution.xml`'s `<domains
  enable_currentUserHome="true">` gets `Installer.app`'s native "install
  for all users" vs. "install for me only" toggle for free on every
  `/Library`-rooted component (VST3/AU) — no custom scripting. A
  free-text custom install path is *not* supported here: that's a real
  limitation of the stock `Installer.app` UI (it only lets you pick a
  disk/volume, not an arbitrary folder), not something this project chose
  to skip. Verified end-to-end locally (this sandbox has
  `pkgbuild`/`productbuild`/`hdiutil`) — `pkgutil --expand` on the actual
  built `.pkg` confirmed all 12 components' identifiers/install-locations.
- `installer/windows_setup.iss` — Inno Setup 6. Same plugin/format
  `[Components]` tree as macOS, the standard directory page for `{app}`
  (Standalone `.exe`s, default `{pf}\abcTrain`), *plus* a second custom
  directory page (`CreateInputDirPage`, anchored after
  `wpSelectComponents` — anchoring after `wpSelectDir` instead was a real
  bug caught on read-through, since that page comes *before*
  `wpSelectComponents` in Inno's default order and would've read stale
  component selections) specifically for VST3, default `{commoncf}\VST3`
  — this is the one platform that got a genuine free-text custom path,
  since Inno's directory widgets support it natively where macOS's don't.
  **Not compiled anywhere yet** (no Windows in this sandbox) — CI is its
  first real test.
- `installer/linux/package_tar.sh` + `install.sh` — lays out
  `<Plugin>/VST3/` + `<Plugin>/Standalone/` per plugin in the `tar.gz`,
  plus an interactive `install.sh` that asks which plugins to install and
  where (VST3: `$HOME/.vst3`, `/usr/lib/vst3` via `sudo`, or a typed
  path). Verified by actually running `install.sh` with piped answers
  under a fake `$HOME`.
- CI installs Inno Setup via `choco install innosetup` on the Windows
  runner (pre-installed Chocolatey shims `iscc` onto `PATH`, no need to
  hardcode its install path) and `libcurl4-openssl-dev` on Linux is
  already covered by the update-checker's own CI step (see the CI section
  below).
- **These builds are unsigned.** macOS Gatekeeper and Windows SmartScreen
  will both warn on them until code signing/notarization is done — that's
  separate, unstarted future work, not a bug in the installer scripts.

## Architecture — Look and feel (`shared/`, all four editors)

See [decisions/009](docs/decisions/009-look-and-feel.md),
[018](docs/decisions/018-ui-polish-and-builtin-samples.md) and
[019](docs/decisions/019-design-system-and-light-theme.md) for the full
rationale; summary here.

- `shared/AbcTrainTheme.h/.cpp` — **the single source of every colour,
  spacing step, corner radius, animation duration and easing curve in the
  UI**. `current()` returns the active `Palette`; `setMode()` switches
  between `dark()` and a separately-*designed* `light()` (warm off-white
  page, surfaces stepping up toward white, deeper/desaturated accents,
  softer cooler shadows, ~half the noise strength — explicitly not an
  inversion, see ADR 019). Exists separately from `AbcTrainLookAndFeel`
  because a `LookAndFeel` only reaches widgets JUCE routes through it,
  while every custom component here (spectrum, waveform, choice slider,
  lesson/training overlays, progress bar) draws itself and used to carry
  its own drifting copies of the hex literals. The light/dark choice is
  persisted in the same shared "abcTrain" `PropertiesFile` as the
  language, so it's one product-wide preference; each of the four editors
  has a theme toggle in its title row.
- `shared/WidgetStateRegistry.h/.cpp` — per-`Component` eased hover/press
  values on a 60 Hz timer, keyed by `Component::SafePointer` (so a widget
  destroyed mid-animation nulls its entry rather than dangling; dead
  entries pruned each tick). This is what makes hover/press *interpolate*
  rather than snap — the thing ADR 018 recorded as impossible for a
  stateless `LookAndFeel`. Press uses a shorter duration than release
  deliberately: that asymmetry is what reads as mass.
- `shared/GainReductionMeter.h/.cpp` — gradient arc filling **downward**
  with gain reduction, glow intensifying as it works. Downward on
  purpose (see ADR 019): GR is the one meter where "more is lower", and
  a reused upward level meter would teach the wrong model.
- `AbcTrainTheme::accentFor (Family)` — the one definition of the four
  skill-family colours (frequency blue, dynamics amber, space green,
  character violet). EarTrainer's `tintForGame()` and each Learner
  plugin's own accent both read it; the three Learner editors pass theirs
  to `AbcTrainLookAndFeel::refreshFromTheme (accent)`, to
  `SpectrumAnalyzerComponent`/`WaveformDisplay::setAccentColour` (a
  per-instance override, since the palette itself is process-wide and two
  Learner plugins can be open at once), and to `paintPanelBackground`'s
  tint. `GainReductionMeter` deliberately keeps the palette's own
  accent→accentWarm sweep: there the gradient is semantic, not
  decorative. Also in that pass: an eased bypass veil over each analysis
  section (bypass previously changed nothing on screen at all), a `what`
  sentence per preset shown in the guide card, and
  `GuideTooltip::setText`'s `autoDismissMs`. See
  [decisions/023](docs/decisions/023-learner-plugin-visual-pass.md).
- `shared/CompactSelector.h/.cpp` — a one-or-two-glyph value plus a
  hairline chevron, opening a `PopupMenu` on click; no well and no border
  until hovered. Replaces the language and window-size `ComboBox`es in
  EarTrainer's title row (142px of well → ~75px of indicator). Not a
  `ComboBox` subclass on purpose: restyling would have gone through
  `LookAndFeel::drawComboBox`, shared with every combo box in all four
  plugins that genuinely *is* a form field. Menu label and indicator label
  are separate fields, since the menu must say "Русский" where the
  indicator has room only for "RU". See
  [decisions/022](docs/decisions/022-motion-audit-and-indicators.md).
- `shared/GuideTooltip.h/.cpp` — the Learner plugins' contextual guide
  text, now a card that eases in over the visualisation only while a
  control is dragged, over a **real** Gaussian blur
  (`juce::ImageConvolutionKernel`) of what's behind it. Snapshots its
  *parent*, not itself — `createComponentSnapshot` includes children, so
  snapshotting itself would recurse into its own `paint()`.

- `shared/AbcTrainLookAndFeel.h/.cpp` — extends `juce::LookAndFeel_V4`.
  `refreshFromTheme()` reads `AbcTrainTheme::current()` into one
  `juce::LookAndFeel_V4::ColourScheme` (call it after
  `AbcTrainTheme::setMode()`, then repaint), which
  `LookAndFeel_V4::initialiseColours()` maps onto specific component
  `colourId`s automatically (e.g. `highlightedFill` becomes both
  `Slider::rotarySliderFillColourId` and `TextButton::buttonOnColourId`) —
  this is what lets one colour scheme reach every rotary knob across all
  three Learner plugins instead of each editor setting its own one-off
  accent colour (the old `deepskyblue`/`mediumpurple` per-slider overrides
  were removed for this reason). Overrides `drawButtonBackground` (rounded
  7px corners, 1px border, a subtle gradient fill, brightens whatever
  `backgroundColour` JUCE passed in on hover/press rather than replacing
  it — see the ADR for a real bug this caught on read-through, since
  overwriting that parameter would have silently broken EarTrainer's
  per-button correct/wrong answer colour-coding — plus a `DropShadow` that
  grows on hover and shrinks on press, see
  [decisions/018](docs/decisions/018-ui-polish-and-builtin-samples.md))
  and `drawRotarySlider` (arc track + value arc + pointer line via
  `Path::addCentredArc`, a cheap layered-fake-blur glow behind the value
  arc while the knob is being touched, and a gradient-shaded,
  drop-shadowed knob cap instead of a flat disc — also ADR 018). A new
  `paintPanelBackground()` static helper (a gentle radial gradient
  instead of one flat colour) replaces every editor's plain
  `g.fillAll()` in `paint()` (also ADR 018). Also provides `titleFont()`/
  `monoFont()` static helpers (22px bold / 16px monospace) plus the
  `getLabelFont`/`getTextButtonFont`/etc. overrides that return the 14px
  body size by default — all via `juce::Font(juce::FontOptions(...))`,
  replacing the deprecated `Font(float, styleFlags)` constructor
  everywhere it was still used.
- Each of the four editors (`Source/PluginEditor.h`,
  `LearnerEQ/Source/PluginEditor.h`, `LearnerComp/Source/PluginEditor.h`,
  `LearnerVerb/Source/PluginEditor.h`) owns its own
  `AbcTrainLookAndFeel` member — declared **first** in the class (so C++
  reverse-order destruction guarantees it outlives every child
  `Component` still referencing it while the editor tears down) — rather
  than one shared static instance, since a host can have several editor
  instances open at once. `setLookAndFeel(&lookAndFeel)` is the first
  constructor statement, `setLookAndFeel(nullptr)` is in the destructor.
- `Source/PluginEditor.cpp`'s choice selector fade-in
  (`juce::Desktop::getInstance().getAnimator().fadeIn()`, one animation in
  this original look-and-feel pass) was retired once the button row was
  replaced by `ChoiceSliderComponent` (decisions/015) — a single
  persistent component being re-labelled doesn't need a fade the way
  freshly created buttons did.
- `shared/LessonController.cpp` picked up the same font migration and
  background/border colours as the four editors, so the lesson overlay
  matches the rest of the theme.
- `shared/AppIcons.h/.cpp` — programmatic `juce::Path` line icons (no
  external asset pipeline) for the 9 EarTrainer games plus LearnerEQ/
  LearnerComp/LearnerVerb, scaled via `Path::scaleToFit()`.
  `AppIcons::iconForGameName()` maps a `Game::getName()` string to its
  icon, mirroring `Source/PluginEditor.cpp`'s existing
  `translateGameName()` lookup shape (unrecognised name falls back to
  the EQ icon, not a crash). `AppIconComponent` wraps this as a
  `Component` for EarTrainer's game selector row (updated in
  `refreshFromGameState()`) and each Learner editor's title row (set
  once, since plugin identity doesn't change at runtime). See
  [decisions/016](docs/decisions/016-icons-and-site-link.md) for why this
  is a deliberately narrower scope than the full designer brief it came
  from, and a real edge-clipping bug in the soundkorb.ru site link
  (`juce::HyperlinkButton`, added to all four editors' bottom-right
  corner in the same pass) found by actually running the app.
- `assets/icons/{eartrainer,learnereq,learnercomp,learnerverb}.png` — real
  per-plugin application icons (1024×1024, generated programmatically,
  reusing `AbcTrainLookAndFeel`'s palette and, for the three Learner
  plugins, a rasterized version of their own `shared/AppIcons.cpp` glyph)
  wired via `ICON_BIG <path>` in each `juce_add_plugin()` call in
  `CMakeLists.txt` — JUCE's own build tooling generates the platform icon
  format from there. Fixes a real reported bug: every Standalone app
  previously opened with macOS's generic blank/default icon, since no
  icon was ever configured. See
  [decisions/017](docs/decisions/017-knowledge-base-content-pass-and-app-icons.md).

Not yet built (deliberately deferred, see ADR 009, ADR 016, and ADR 018):
per-widget hover/press *state interpolation* (button/knob shadow and glow
responses are immediate, keyed off JUCE's highlighted/down flags, not
eased over time the way `LevelProgressBar`'s breathing glow or
`ChoiceSliderComponent`'s feedback animations are — see ADR 018 for why a
shared, stateless `LookAndFeel` has nowhere to keep a per-button animation
timeline), no press-scale-then-spring-back, gradient fills under the
spectrum/waveform curves, pill-shaped tooltip backgrounds for guide
labels, `FlexBox`-based layout (every editor still uses explicit
`Rectangle::removeFrom*`), a light theme, a documented design-token
styleguide, a licensed custom typeface, professional/licensed icon assets
(the current icons are original simple line art, not a
Feather/Phosphor-equivalent set), icon "morphing" transitions, and
screenshots/mockups in `README.md` (this sandbox can't render or capture a
real JUCE window, so the look is described in text there instead).

## Architecture — Localization / i18n (`shared/i18n/`, EarTrainer's editor)

See [decisions/011-i18n.md](docs/decisions/011-i18n.md) for the full
rationale; summary here.

- `shared/i18n/strings/*.json` — one flat `{"dot.key": "text"}` table per
  supported language (en, ru, de, fr, es, pt, zh-Hans, ja, ko, it, pl,
  uk), covering a curated core UI string set (game names/instructions,
  common labels/buttons) — not every tooltip/lesson string yet.
  `CMakeLists.txt`'s `juce_add_binary_data(I18nData SOURCES ...)` embeds
  them into every plugin target + `EarTrainerTests` as `BinaryData`,
  since a plugin reading a JSON file from a path relative to its own
  binary isn't reliable across VST3/AU/Standalone install locations.
- `shared/i18n/LocalisationManager.h/.cpp` — a `juce::ChangeBroadcaster`
  constructed from a `juce::PropertiesFile&` (same ownership pattern as
  `ProgressManager`). Auto-detects the system language
  (`juce::SystemStats::getUserLanguage()`, "zh" specifically maps to
  "zh-Hans") on first run, falls back to "en" if unsupported, and
  persists the choice via `makeDefaultOptions()`'s one shared
  `PropertiesFile` folder ("abcTrain") so the language is a single
  product-wide preference rather than per-plugin. `getText(key)` falls
  back English → the literal key string (never silently blank) if a
  translation is missing; `getText(key, placeholders)` does simple
  `"{{name}}"` substitution over that. Maps each language code to its
  `BinaryData` symbol via an explicit if-chain, since `juce_add_binary_data`'s
  filename sanitizing isn't predictable enough to derive programmatically
  (`zh-Hans.json` → `zhHans_json`, dash dropped not underscored).
- **A real bug caught before shipping**: `getDisplayName()`'s non-ASCII
  names (Русский, 简体中文, etc.) were first written as raw hex-escaped
  `const char*` literals — `juce::String`'s plain `const char*`
  constructor does *not* assume UTF-8 (a known JUCE gotcha), so those
  literals mojibake'd. Caught by a failing test, fixed by wrapping every
  non-ASCII literal in `juce::CharPointer_UTF8(...)` explicitly; the
  JSON-loaded strings never had this problem since `loadLanguageTable()`
  already used the explicit `juce::String::fromUTF8(data, size)` path.
- `Source/PluginEditor.cpp` (EarTrainer) is the one reference
  integration: a language `ComboBox` in the title row, plus a small
  `englishName -> i18n key` lookup table
  (`translateGameName()`/`translateGameInstructions()`) so all 9 `Game`
  classes need zero changes to become localizable — a game missing from
  that table just falls back to its raw English text.
  `changeListenerCallback` only rebuilds the game-selector's items when
  `LocalisationManager` itself is the broadcaster (not on every answer/
  progress tick), since rebuilding a `ComboBox`'s items on every round
  played would be wasteful and could visibly flicker it.
- `tests/LocalisationManagerTest.cpp` — loads and checks non-empty output
  for all 12 languages, unknown-key fallback, unsupported-saved-code
  fallback, `setLanguage` actually switching `getText()` output,
  placeholder substitution, and non-empty `getDisplayName()` for every
  code. Doesn't assert on `ChangeListener` delivery firing synchronously
  (same `sendChangeMessage()`-is-asynchronous reason `ProgressManager`
  exposes `registerAnswer()` directly — see Testing below).

Not yet built: LearnerEQ/LearnerComp/LearnerVerb don't have a language
selector yet (only EarTrainer, as the reference integration); parameter
tooltips, lesson step text, and each game's dynamic `getFeedbackText()`
are still English-only.

## Testing (`tests/`, `shared/`)

`EarTrainerTests` is a plain console app (`juce_add_console_app`, not a
plugin) built from the same root `CMakeLists.txt`:
`cmake --build build --target EarTrainerTests`, then run the produced
binary directly — it uses `juce::UnitTestRunner` and exits non-zero on any
failure. It compiles the game/processor source files directly (not the
plugin targets), so no plugin host or GUI is needed to run it.

`tools/EditorSnapshots.cpp` (`EditorSnapshots`, a second
`juce_add_console_app` target) renders all three Learner editors to PNGs
in **both themes**, with no plugin host — `cmake --build build --target
EditorSnapshots`, then run the binary with an output folder. It asserts
nothing on purpose (a golden-file comparison would fail on every
legitimate design change) and never pumps a message loop, so no `Timer`
fires and every eased value is captured at rest; it is a contact sheet to
look at. Its first run found six bugs that compiled and passed all 172
test groups — amber value arcs on a blue plugin, a gain-reduction meter
silently clamped to 32px, a blank lesson dropdown, a light-theme display
well brighter than its own panel, 132px of dead window, and three
unlabelled knobs per EQ band. EarTrainer's editor is deliberately
excluded (it writes to the real per-user progress file, so rendering it
would touch a player's saved record). See
[decisions/023](docs/decisions/023-learner-plugin-visual-pass.md).

- `shared/TestUtils.h` — `generateSineBuffer`/`rms` helpers for
  audio-domain assertions.
- `tests/EQGameTest.cpp`, `tests/CompressionGameTest.cpp`,
  `tests/ReverbGameTest.cpp`, `tests/PanGameTest.cpp`,
  `tests/DelayGameTest.cpp`, `tests/DistortionGameTest.cpp`,
  `tests/StereoWidthGameTest.cpp`, `tests/DBGameTest.cpp`,
  `tests/FrequencyRangeGameTest.cpp`, `tests/GameManagerTest.cpp` —
  logic-level: scoring, answer/round state transitions, choice-count/
  label contracts, and (for all nine games) that `setDifficulty` at each
  tier still plays a valid round. Deliberately don't assert on actual
  audio content (the games generate random noise), since that would be
  either flaky or trivial — `ReverbGameTest`/`DistortionGameTest` are the
  exceptions, re-rolling `newRound()` to force every type and checking
  the output buffer isn't silent (a decent smoke test given each game's
  type-specific DSP paths); `FrequencyRangeGameTest` does the same
  non-silent check once, since unlike Reverb/Distortion it has no
  discrete "type" to force through every value of.
  `StereoWidthGameTest` additionally checks left and right samples
  actually differ, verifying the two independent `PinkNoiseGenerator`s
  really decorrelate. `DBGameTest` checks the choice labels themselves at
  three difficulty levels, since `DBGame`'s legacy discrete labels are
  recomputed per tier. `FrequencyRangeGameTest` checks the 7 choice
  labels match the standard range names exactly. Note `ReverbGame` now
  defaults to the easy tier (2 choices) *before* `setDifficulty` is ever
  called, matching the other games' easy-tier defaults — tests that want
  all 4 types must call `setDifficulty(10)` first. `GameManagerTest`
  asserts 9 registered games. Note these all exercise the **discrete**
  `submitAnswer(int)` path, which the four continuous games deliberately
  kept verbatim (ADR 020) — which is why every one of them passed
  unedited through that change.
- `tests/ProgressManagerTest.cpp` — level/points math, streak, daily
  challenge, and a persistence round-trip, all via `registerAnswer`/
  `updateStreakForDate`/`generateDailyChallengeForDate` called directly
  rather than through the real `ChangeListener` wiring (see below).
- `tests/AchievementsTest.cpp` — the achievement rules against
  hand-built `Snapshot`s: unique non-empty ids, unknown id returns null,
  a blank slate earns nothing, accuracy needs the rounds floor *and* the
  ratio, totals accumulate across exercises while best-streaks do not,
  breadth counts exercises touched rather than rounds played, progress is
  monotonic and clamped, an empty games vector misses rather than reading
  past the end, and **choosing a level earns nothing** (see ADR 024).
- `tests/SessionManagerTest.cpp` — the Practice/Survival/Blitz state
  machine driven directly: practice never ends, survival spends exactly
  one life per wrong answer and reports its score once through
  `onRunEnded`, answers after a run has ended are ignored, blitz spends
  time rather than lives, ticking does nothing outside blitz, switching
  mode starts a fresh run, auto-advance waits longer after a wrong answer
  and not at all once the run is over, and a practice run reports no
  score. Pure state, so none of the message-loop concerns below apply.
- `tests/ContinuousScaleTest.cpp` — one shared contract run against all
  four continuous games (EQ/Pan/dB/Delay): on-target always passes, a
  whole axis away always fails, tolerance narrows with difficulty, a
  guess just inside the band passes and just outside fails, repeat
  answers are ignored, grid marks stay on the axis. Plus the per-game
  axis-linearity check that is the whole justification for each one's
  choice of scale — equal ratios take equal distance on the log axes
  (EQ, Delay), equal dB take equal distance on the linear one. See
  [decisions/020](docs/decisions/020-continuous-answers.md).
- `tests/ReferenceAudioLibraryTest.cpp` — writes real WAV files to a temp
  folder and checks `rescan()`'s category/file-count contract (including
  that the two always-present built-in categories, see ADR 018, are
  counted alongside whatever real subfolders exist), `selectFile()`
  loading/resampling/failing-safely behaviour, and root-folder/selection
  persistence across `PropertiesFile` reconstruction.
- `tests/LearnerEQTest.cpp` — the one test that touches real DSP output:
  boosts a band via `apvts.getRawParameterValue(...)->store(...)` and
  checks measured RMS actually goes up at that frequency. This is the
  kind of check that would have caught a broken filter chain, which
  mattered here because LearnerEQ's DSP code could not be compiled/run at
  all in the environment it was originally written in. Also checks that
  bypass leaves the buffer bit-for-bit unchanged even with a large band
  boost dialled in.
- `tests/LearnerCompTest.cpp` — same approach: a 0 dBFS sine through a
  real `LearnerCompProcessor` at -6 dB threshold/2:1 ratio/hard knee
  should settle at -3 dBFS; adding +3 dB makeup should bring it back to
  ~0 dBFS (measured on the buffer tail, after the fast attack has
  settled). Also checks bypass leaves the buffer bit-for-bit unchanged,
  `applyPreset` sets every parameter a preset defines, an out-of-range
  preset index is a no-op rather than a crash, and (in both the normal and
  bypassed branches) that `processBlock`'s mono-downmix feed for
  `SpectrumAnalyzerComponent` runs safely with no analyzer attached —
  deliberately *not* by constructing a real `SpectrumAnalyzerComponent`,
  see [decisions/006](docs/decisions/006-unified-visualization.md) for why.
- `tests/LearnerVerbTest.cpp` — reverb has no clean closed-form target the
  way compression math does, so this is behavioral instead: a noise burst
  through a real `LearnerVerbProcessor` should leave an audible tail after
  the burst ends (unlike a dry passthrough, which would be silent);
  `dryWet = 0` should give an exact (not just close) passthrough, since
  `0 * wet + 1 * dry` is exact in floating point; every one of the 4 types
  should produce non-silent output without crashing; `applyPreset` and the
  out-of-range-index guard are tested the same way as LearnerComp's; and
  bypass forces an exact dry passthrough even with `Dry/Wet` at 100%,
  without leaving the `Dry/Wet` parameter itself clobbered.
- `tests/MicroLessonTest.cpp` — step-navigation state machine tests
  against `MicroLesson` directly: inactive until `start()`, `nextStep`/
  `previousStep` stop at the ends and no-op before `start()`, `stop()`
  deactivates, `getCurrentStep()` exposes the right text/target params.
  No APVTS, no `LessonController`, no `Component` involved at all.
- `tests/UpdateCheckerTest.cpp` — `isNewerVersion` (newer patch/minor/
  major, equal/older, with or without a leading `v`, mismatched component
  counts, malformed input returns false rather than guessing) and
  `parseReleaseJson` (well-formed GitHub response, malformed JSON,
  valid-JSON-but-not-an-object, GitHub's 404 shape) tested directly.
  `checkForUpdatesAsync`'s real network call is not tested — no network
  access assumed in this console test binary, see
  [decisions/007](docs/decisions/007-update-checker.md).
- `tests/LocalisationManagerTest.cpp` — all 12 languages load a
  non-empty table, unknown-key/unsupported-saved-code fallback,
  `setLanguage` switching `getText()` output, `"{{name}}"` placeholder
  substitution, non-empty `getDisplayName()` for every code. See
  [decisions/011](docs/decisions/011-i18n.md).
- `tests/VersionChannelTest.cpp` — `VersionChannel::detect` on hand-written
  example strings: exact `vX.Y.Z` tags (stable), `-beta` suffixes
  (case-insensitive), `-N-gHASH`/`-dirty`/malformed input (all dev). See
  [decisions/012](docs/decisions/012-versioning.md).

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
`Source/PluginProcessor.cpp` (EarTrainer's — the game logic under test
doesn't need the `AudioProcessor` wrapper) and every plugin's
`PluginEntry.cpp` (`LearnerEQ/Source/PluginEntry.cpp`,
`LearnerComp/Source/PluginEntry.cpp`, `LearnerVerb/Source/PluginEntry.cpp`
— each is *just* `createPluginFilter()`, split out of its
`PluginProcessor.cpp` specifically because `LearnerEQTest`,
`LearnerCompTest`, and `LearnerVerbTest` all need their real processor's
`PluginProcessor.cpp` linked in, and three definitions of
`createPluginFilter()` in one binary would collide. The real plugin
targets (`juce_add_plugin`) link both `PluginProcessor.cpp` *and*
`PluginEntry.cpp` — only the test binary needs the split.

CI: `.github/workflows/build_and_test.yml` builds all targets and runs
`EarTrainerTests` on push/PR across ubuntu-latest/macos-latest/
windows-latest. **Confirmed green on all three as of commit `a2f2944`**,
again on `dd207d1` (LearnerComp), `8932b84` (LearnerVerb), `dd0ef5a`
(MicroLesson/LessonController), and `7accd19` (visualization unification,
see below) — each checked directly against the GitHub Actions API or web
UI, not assumed. Three real bugs have been caught and fixed getting here
(see `docs/diagrams/ci-pipeline.md` for all three) — that history is a
reminder to keep watching CI on every push, not evidence the code is now
bulletproof. **The visualization-unification commit `b3c2f88` actually
failed CI on all three OSes** — a real compile error (a derived Component
class silently losing its implicit default constructor once
`JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` was in play with no
constructor of its own declared), not a false alarm — reproduced and fixed
locally in `7accd19` by actually installing `cmake` and building (this
sandbox has Homebrew + `clang++`, so a local build is possible here, not
CI-only as earlier work in this project assumed). Confirmed green again on
`6331f89` (docs-only), and again on `148d984` (CI artifacts, tag releases,
the manual update checker, and `NEEDS_CURL`/libcurl on Linux all verified
on real CI runners, not just locally) — it was, however, also verified
with a full local build of all four plugin targets plus `EarTrainerTests`
beforehand (see [decisions/007](docs/decisions/007-update-checker.md)),
which is also how a real use-after-free in `UpdateChecker::parseReleaseJson`
(a temporary `var`'s `DynamicObject` read after the temporary was already
destroyed) and a pre-existing, previously-invisible flakiness in
`ProgressManagerTest` (its `PropertiesFile`-backed tests persisted state
across separate runs of the same binary on the same machine, since CI's
containers are always fresh but this was the first time the binary had
ever been run twice locally) were both found and fixed. **The per-OS
installer commit's first real CI run (`61c5e2c`) did fail** — Windows
only, on `iscc`'s first real compile of `windows_setup.iss`: a typo'd
`[Files]` flag (`createallsubdirdirs` instead of the real
`createallsubdirs`), caught from the actual CI log text, fixed in
`0d65778`, and **confirmed green on all three OSes** (run `30135611149`)
— macOS and Linux installers had already been fine on that first run
too, only Windows needed the fix (see
[decisions/008](docs/decisions/008-installers.md)).

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

- A full design-system pass (styleguide, professional icon pack, Figma
  mockups, FabFilter/iZotope-grade spectrum/meter animations) per the
  user's UI-overhaul brief — a real design job needing an actual
  designer/design tool, not something this codebase can produce alone;
  see [decisions/016](docs/decisions/016-icons-and-site-link.md) for what
  was built instead (programmatic vector icons for every game/plugin, a
  soundkorb.ru site link) and why the rest is out of scope here. A
  user-supplied structured audio-engineering knowledge base
  (`baza_znanij_audio_plaginy_v2.jsonl`, 90 rules across EQ/compression/
  spatial effects/psychoacoustics/acoustics/mastering) is also a good
  candidate for enriching `docs/knowledge_base.md`/the Learner plugins'
  tooltip text beyond today's coverage — not yet folded in.
- A dedicated soundkorb.ru page for the plugins themselves — the site
  link added in this pass just points at the existing site; the user
  said a specific page for abcTrain there is separate, later work.
- Training-sounds UX beyond the current pass (see
  [decisions/015](docs/decisions/015-choice-slider-and-training-sounds.md)
  and [decisions/018](docs/decisions/018-ui-polish-and-builtin-samples.md) —
  a folder-chooser button and two always-available built-in synthesized
  sample categories now exist): a way to audition a category's audio or
  pick a specific file within it (still a random pick each time), and
  LearnerEQ/LearnerComp/LearnerVerb don't have any reference-audio option
  at all (EarTrainer only, so far).
- One more teaching plugin: LearnerSat — same pattern as the other three
  (own `juce_add_plugin` target, APVTS params, a visualization +
  contextual guide text, its own `PluginEntry.cpp` split).
- The three Learner plugins' parameter tooltips are now richly rewritten
  (`ParameterGuide.h`/`ReverbGuide.h`/`FrequencyGuide.h`, see ADR 010,
  ADR 017, and `docs/knowledge_base.md`), and all 9 EarTrainer games'
  `getInstructions()` gained a practical tip in all 12 languages (ADR
  017); still not done: any of the six `MicroLesson`s' *existing* step
  text beyond the handful of new steps ADR 017 added, and folding the
  gifted `baza_znanij_audio_plaginy_v2.jsonl` knowledge base any deeper
  than the paraphrased synthesis already in `docs/knowledge_base.md`.
- More EarTrainer exercises building on standard, non-book-specific
  terminology - dB SPL vs dB FS, compressor topology (VCA/FET/Opto/
  Vari-Mu) - were considered for this pass; only `FrequencyRangeGame`
  (named frequency ranges) shipped, the other two are still just ideas.
- LearnerVerb's trimmed-for-now visualizations: impulse-response "cloud,"
  decay-vs-frequency graph, stereo correlometer/vectorscope (see ADR 004) —
  unaffected by the spectrum/waveform unification in ADR 006, which only
  brought all three plugins up to the same baseline shape.
- LearnerEQ "analyze reference" mode + richer knowledge base beyond one-
  line tooltips.
- Per-control lesson-step highlighting, trimmed from the initial
  MicroLesson build (see ADR 005) — the moving-knob cue from each step's
  own `SliderAttachment`/`ComboBoxAttachment` stands in for it today.
- More lessons per plugin (each Learner plugin has exactly two today, see
  ADR 017 - still room for more).
- Golden-file / transient-behavior audio regression tests for LearnerEQ,
  LearnerComp, and LearnerVerb (each currently has only steady-state or
  behavioral assertions, no golden-file comparison).
- Integration test for the real `Game → ProgressManager` `ChangeListener`
  wiring (needs a pumped message loop, not set up yet — see Testing), and
  similarly no automated test of `LessonController`'s actual
  APVTS-setting behavior (see ADR 005), `SpectrumAnalyzerComponent`'s
  FFT/timer logic directly (see ADR 006), or `UpdateChecker`'s real
  network call/`AlertWindow` (see ADR 007) — all four would need
  `juce::ScopedJuceInitialiser_GUI` plus a pumped message loop, which is a
  real but deliberately deferred setup cost (see `docs/testing-strategy.md`).
- Background daily update-check timer, trimmed from the initial
  `UpdateChecker` build in favor of a manual "Check for Updates" button
  (see ADR 007) — revisit if manual checking turns out too easy to forget.
- Code signing and notarization (macOS)/authenticode (Windows) — real
  per-OS installers exist now (`.pkg`/DMG, Inno Setup `.exe`,
  `tar.gz`/`install.sh`, see ADR 008), but the builds themselves are still
  unsigned: Gatekeeper and SmartScreen will both warn until this is done.
- Real licensing/monetization beyond the current all-rights-reserved
  `LICENSE` — a "free license for GitHub stargazers" idea was proposed
  and explicitly deferred (no real users yet to justify the added
  friction, and the client-side HMAC-signing approach that was proposed
  wouldn't have provided real protection anyway — trivially extractable
  from the shipped binary). Revisit once there's real user traction worth
  protecting.
- UI polish beyond the current pass (gradients/shadows/hover-glow on
  buttons and knobs, a radial-gradient panel background, breathing
  progress bar, and correct/wrong feedback animations on the choice
  slider, see ADR 018): per-widget hover/press *state interpolation*
  (press-scale-then-spring-back), gradient-filled spectrum/waveform
  curves, pill-shaped tooltip backgrounds for guide labels, `FlexBox`-
  based layout, a light theme, a licensed custom typeface — all
  deliberately deferred, see ADR 009 and ADR 018.
- Phase 2 (AI detector) and phase 3 (sales site/B2B licensing beyond the
  current `LICENSE` file) are unstarted; see prior conversation history
  for the full plan if picked up later.
