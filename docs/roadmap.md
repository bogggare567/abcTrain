# Roadmap

Status legend: ✓ done · 🚧 in progress · ⏳ planned, not started.

Note on scope: the "Beta"/"1.0"/"2.0" phase boundaries below are a rough
sequencing guess, not commitments — treat them as "roughly this order,"
not a schedule. Market-size or competitor claims from earlier planning
conversations are not reflected here; this table only tracks what's built
vs. not.

## MVP — trainer core + first teaching plugin

| Feature | Status |
|---|---|
| `Game` interface + `GameManager` (see [decisions/001](decisions/001-game-interface.md)) | ✓ |
| EQGame ("guess the boosted/cut band") | ✓ |
| CompressionGame ("guess the compression strength") | ✓ |
| ReverbGame ("guess the reverb type": room/hall/plate/spring) | ✓ |
| Generic `PluginEditor` (game selector + dynamic choice buttons) | ✓ |
| LearnerEQ: real 4-band EQ, host-automatable, live spectrum + response curve | ✓ |
| LearnerEQ: contextual tooltip per frequency range while dragging | ✓ |
| Unit test infra (`EarTrainerTests`, `juce::UnitTestRunner`) | ✓ |
| Unit tests: game scoring/state logic (all 3 games) | ✓ |
| DSP regression test: LearnerEQ band boost raises measured RMS | ✓ |
| CI: build + run tests on push/PR | ✓ confirmed green on all 3 OSes (commit `a2f2944`), including `EarTrainerTests` actually running and passing, not just building |

## Beta — round out the trainer, second teaching plugin

| Feature | Status |
|---|---|
| `ProgressManager`: points, level 1-10, `juce::PropertiesFile` persistence (see [decisions/002](decisions/002-difficulty-scaling.md)) | ✓ |
| Adaptive difficulty (`Game::setDifficulty`, all 3 games, driven by level) | ✓ |
| Daily login streak | ✓ |
| Daily challenge (N-in-a-row on a deterministically-picked game, bonus points) | ✓ |
| Level/progress bar/streak/daily-challenge shown in `PluginEditor` | ✓ |
| Unit tests: `ProgressManagerTest` (level math, streak, daily challenge, persistence round-trip) | ✓ |
| LearnerComp: custom soft-knee compressor engine (see [decisions/003](decisions/003-learnercomp-engine.md)), waveform visualization with gain-reduction highlighting, GR/peak meters, bypass (doubles as A/B), 4 teaching presets | ✓ |
| Unit tests: `LearnerCompTest` (closed-form compression/makeup-gain math, bypass passthrough, preset application) | ✓ |
| LearnerVerb: Room/Hall/Plate (`juce::dsp::Reverb`) + Spring (custom allpass cascade) via one `ReverbEngine`, pre-delay, waveform visualization, 4 teaching presets (see [decisions/004](decisions/004-learnerverb-scope.md)) | ✓ — **scope trimmed**: no impulse-response "cloud," no decay-vs-frequency graph, no stereo correlometer/vectorscope. Just the live wet/dry waveform + peak meters, matching LearnerComp's shape. Those three visualizations are each a real feature in their own right, not something to bolt on by default — see the ADR. |
| Unit tests: `LearnerVerbTest` (behavioral/smoke: tail persists after input stops, dryWet=0 is exact passthrough, every type produces sound, preset application) — no closed-form target the way compression has | ✓ |
| Shared visualization component factored out for reuse across Learner plugins | ✓ `shared/WaveformDisplay.{h,cpp}`, extracted from LearnerComp once LearnerVerb needed the identical shape. `SpectrumAnalyserComponent` (LearnerEQ) is still separate — it's FFT-based, a fundamentally different data shape from the time-domain peak-tracking the other two share, so there's nothing to unify there. |
| Micro-lessons: `MicroLesson`/`LessonController` (see [decisions/005](decisions/005-microlesson-architecture.md)), one lesson per Learner plugin (Vocal EQ Basics / Vocal Compression / Space for Vocals), a "Lesson" button in each editor | ✓ — per-control UI highlighting was cut (the parameter's own `SliderAttachment`/`ComboBoxAttachment` already makes the knob visibly move when a step sets it, which does the same job), see the ADR |
| Unit tests: `MicroLessonTest` (step navigation state machine — pure logic, no APVTS/GUI needed) | ✓ |
| Unified visualization across Learner plugins: `shared/SpectrumAnalyzer` (extracted from LearnerEQ), Waveform+Spectrum in all three, Bypass/A-B toggle in all three next to Lesson (see [decisions/006](decisions/006-unified-visualization.md)) | ✓ |
| Downloadable builds: CI uploads a per-OS artifact on every push, publishes a GitHub Release with all three OS builds when a `vX.Y.Z` tag is pushed, and each plugin has a manual "Check for Updates" button (`shared/UpdateChecker`, see [decisions/007](decisions/007-update-checker.md)) | ✓ — no background daily timer, manual only; see the ADR for why |
| Unit tests: `UpdateCheckerTest` (version-comparison + JSON-parsing logic — pure, no real network call) | ✓ |
| Real per-OS installers: component/format-selectable `.pkg`/DMG (macOS), Inno Setup `.exe` with a free-text VST3 path (Windows), `tar.gz` + interactive `install.sh` (Linux) — see [decisions/008](decisions/008-installers.md) | ✓ confirmed green on all 3 OSes — Windows's first real CI compile caught a real `[Files]` flag typo (`createallsubdirdirs`), fixed, reconfirmed |
| 5 more EarTrainer exercises: `PanGame` (5 pan positions), `DelayGame` (4 fixed delay times), `DistortionGame` (4 waveshaper types), `StereoWidthGame` (4 widths), `DBGame` (5 gain deltas) — 8 games total now, see `docs/diagrams/game-engine.md` | ✓ |
| 9th EarTrainer exercise: `FrequencyRangeGame` — name the standard frequency range (Sub-bass/Bass/Low-mids/Mids/High-mids/Presence/Air) a boost/cut landed in, frequency chosen log-uniformly within the range each round | ✓ |
| Unit tests: `PanGameTest`/`DelayGameTest`/`DistortionGameTest`/`StereoWidthGameTest`/`DBGameTest` (same scoring/state template as the first 3 games, plus a decorrelation check for `StereoWidthGame` and a label-recompute check for `DBGame`) | ✓ |
| Shared dark theme: `shared/AbcTrainLookAndFeel` applied to all four editors, deprecated `Font` migrated to `FontOptions` everywhere, one consistent accent colour instead of three one-off ones, a basic 200 ms fade-in on EarTrainer's choice buttons (see [decisions/009](decisions/009-look-and-feel.md)) | ✓ — **basic pass only**: hover/press animations beyond the one fade, gradient-filled meter curves, pill-shaped tooltips, and `FlexBox` layout were all deliberately deferred, see the ADR |
| Integration-level tests (editor button clicks -> GameManager state, or a lesson step actually landing on the right APVTS values through `LessonController`) | ⏳ |

## 1.0 — knowledge base, more exercises/plugins

| Feature | Status |
|---|---|
| LearnerSat | ⏳ |
| Book-library bibliography: `docs/library_catalog.md` catalogs all 158 books in the user's local audio-engineering collection by topic (title/author only, no text extracted) — see [decisions/010](decisions/010-book-library-scope.md) for why full-text extraction + in-product quoting (the original ask) was rejected as a copyright problem | ✓ catalog only |
| Localization (i18n): `shared/i18n/LocalisationManager` + a flat JSON string table per language for 12 languages (en/ru/de/fr/es/pt/zh-Hans/ja/ko/it/pl/uk), auto-detected + persisted, with a language picker wired into EarTrainer's editor as the reference integration — see [decisions/011](decisions/011-i18n.md) | ✓ **core UI string set only**: game names/instructions + common labels/buttons, all 12 languages, tested. Parameter tooltips, lesson steps, and dynamic feedback text are still English-only; LearnerEQ/Comp/Verb don't have a language selector yet |
| Versioning: `CurrentVersion::string` derived from `git describe --tags --dirty --always` at CMake configure time (not a hand-bumped literal), `VersionChannel::detect` (stable/beta/dev), `UpdateChecker` gained beta-channel (pre-release-aware) fetching — see [decisions/012](decisions/012-versioning.md) | ✓ **version/channel derivation + channel-aware fetching only**: real and tested, but no editor UI exposes a channel/auto-check toggle yet, and CI doesn't tag nightly/beta/stable artifacts differently |
| Repo presentation: README overhaul (badges, game/plugin/language tables, links to BETA_TESTING.md/CONTRIBUTING.md), `.github/ISSUE_TEMPLATE/` (bug/feature), bilingual `.github/CONTRIBUTING.md`, `assets/screenshots/`+`assets/demo/` placeholder READMEs, `docs/diagrams/i18n-architecture.md` | ✓ — still no actual screenshots/GIFs (no display in this sandbox to capture them from), see the asset READMEs for exactly what's needed |
| UI polish beyond the basic redesign pass: hover/press animations (fade timers, press-scale springs), gradient-filled spectrum/waveform curves, pill-shaped tooltip backgrounds for guide labels, `FlexBox`-based layout | ⏳ deliberately deferred from the redesign pass, see [decisions/009](decisions/009-look-and-feel.md) |
| LearnerVerb: impulse-response visualization, decay-vs-frequency graph, stereo correlometer/vectorscope | ⏳ trimmed from the initial LearnerVerb build, see [decisions/004](decisions/004-learnerverb-scope.md) |
| Richer in-plugin tooltips (LearnerEQ/LearnerComp/LearnerVerb), each 2-4 original sentences with practical values plus a "Learn more" pointer into `docs/library_catalog.md` (title/author only), backed by `docs/knowledge_base.md` | ✓ all three Learner plugins' guides rewritten; EarTrainer's 8 other games' `getInstructions()` still one-liners |
| Per-control lesson-step highlighting (beyond the moving-knob cue) | ⏳ trimmed from the initial MicroLesson build, see [decisions/005](decisions/005-microlesson-architecture.md) |
| More lessons per plugin (each Learner plugin has exactly one today) | ⏳ |
| Glossary with audio examples | ⏳ |
| Golden-file audio regression tests for critical DSP chains | ⏳ |
| Code signing, notarization (macOS)/authenticode (Windows) | ⏳ — per-OS installers now exist (see the Beta row above) but are unsigned: macOS shows an "unidentified developer" Gatekeeper block, Windows shows a SmartScreen warning, until this is done |
| Real licensing/monetization beyond the current all-rights-reserved `LICENSE` | ⏳ — a "free for GitHub stargazers" social-license idea was considered and explicitly deferred until there's real user traction to protect; see conversation history if picked up later |

## 2.0 — AI assistant, B2B

| Feature | Status |
|---|---|
| Synthetic dataset generator (dry/wet pairs with known processing) | ⏳ |
| Reference-track analysis model (detect likely EQ/comp/reverb applied) | ⏳ |
| "Try this on LearnerEQ/LearnerComp/LearnerVerb" suggestion flow from analysis results | ⏳ |
| B2B: school/studio accounts, progress tracking, LMS integration | ⏳ |
| Sales site + payment integration | ⏳ |

## What's explicitly out of scope for now

Anything in the 2.0 row above is a substantial separate effort (model
training pipeline, hosting, a completely different kind of infra) and
shouldn't be started opportunistically alongside plugin/game work — it
needs its own planning pass when the earlier phases are further along.
