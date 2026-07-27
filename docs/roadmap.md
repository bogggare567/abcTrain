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
| Third-party UI libraries (`foleys_gui_magic`/`gin`/animation), adopted one library/one component at a time rather than a wholesale editor rewrite — see [decisions/013](decisions/013-ui-libraries.md) | 🚧 Phase 1 done: `EarTrainerEditor`'s level progress bar now eases via JUCE's own `juce_animation` module (the originally-named "juce_animate" library doesn't exist). Phase 2 (`gin`) investigated and **declined** — every module requires C++20, a project-wide bump not worth it for components that would clash with `AbcTrainLookAndFeel`'s existing theme anyway. Phase 3 (`foleys_gui_magic`) metadata checked (looks C++17-compatible), no prototype build attempted yet |
| EarTrainer usability pass: a real button-collapse bug (choice buttons in every game silently became unclickable ~200ms after appearing), the "Updates" button always showing an outcome instead of silently doing nothing, a childish/clashing colour palette fixed to match the theme, and a manual level selector — found and verified by actually running the built Standalone app, not just reading the code — see [decisions/014](decisions/014-eartrainer-usability-fixes.md) | ✓ |
| Drag-to-select `ChoiceSliderComponent` (tick-marked track + big value label) replacing every game's choice-button row, per a user-supplied reference-app screenshot; plus opt-in `ReferenceAudioLibrary`/`TestSignalGenerator` infrastructure so a player can train on their own reference-audio folders instead of only pink noise, and a "Training Sounds" picker screen — see [decisions/015](decisions/015-choice-slider-and-training-sounds.md). Two real layout bugs (edge tick-label clipping; the overlay painting *behind* the slider) found by actually running the app | ✓ **infrastructure only** — no audio content is bundled, fetched, or vetted for legality by this project; that's entirely up to whatever the user places in their own folder. A folder-chooser button and two always-available built-in synthesized categories were added since (see the row below); still no per-file picker (random pick within a category). LearnerEQ/Comp/Verb now play from the same library too, see [decisions/026](decisions/026-practice-audio-in-the-learner-plugins.md) |
| Bounded gradient/shadow/glow UI polish pass (button and knob hover/press shadows, gradient panel backgrounds, a breathing progress bar, correct/wrong feedback animations on the choice slider) plus five originally-synthesized built-in training samples (kick, snare, pad, pluck, tone) exposed as two always-unlocked `ReferenceAudioLibrary` categories, and a "Choose Folder..." button so a player can point training at their own audio — see [decisions/018](decisions/018-ui-polish-and-builtin-samples.md). A folder pointed at during this same request turned out to contain three real commercial "name your price" albums; none of that content was read, copied, or embedded — the built-in samples are 100% original synthesis | ✓ **bounded pass, not a literal FabFilter-level redesign or a default replacement of pink noise in any game** — see the ADR for exactly what was and wasn't built |
| Design-token layer (`shared/AbcTrainTheme`) as the single source of every colour/spacing/radius/duration; a separately-*designed* light theme with a product-wide toggle in all four editors; eased per-widget hover/press via `shared/WidgetStateRegistry`; letter-spaced titles; gradient-filled smoothed spectrum/waveform with a soft grid; a downward-filling `GainReductionMeter` with glow; a blur-backed floating `GuideTooltip`; captioned section panels and recessed display wells throughout — see [decisions/019](decisions/019-design-system-and-light-theme.md) | ✓ — two more bugs found only by running the app (invisible slider groove, truncated instructions + dead space) |
| Continuous answers (see [decisions/020](decisions/020-continuous-answers.md)): EQ/Pan/Gain/Delay draw a real value from a range each round instead of picking one of N fixed points, and difficulty narrows an accept band instead of only making the stimulus weaker. Tolerance is in the unit the ear works in — octaves for EQ, a ratio for delay, dB for gain — so the slack means the same thing across the whole range | ✓ — five games stay discrete because their answer genuinely is a category; Stereo Width is the open case |
| Training runs with a shape + real navigation (see [decisions/021](decisions/021-sessions-and-navigation.md)): Practice/Survival/Blitz modes, 3 lives, a 90s Blitz clock, auto-advance between rounds, per-exercise lifetime stats, and a Home screen where trainings are grouped by the skill they build with a star to pin your focus | ✓ — no end-of-run results screen yet; the run just stops with its score on the label |
| UI work still open after 019: a licensed custom typeface (no asset to source, and real licensing consequences — the one item of the brief that isn't a coding problem), icon morphing between states, `FlexBox` layout, a written design-token styleguide, and any navigation/IA rework — each editor is still one flat screen | ⏳ see [decisions/019](decisions/019-design-system-and-light-theme.md) |
| Programmatic vector icons (`shared/AppIcons`) for all 9 EarTrainer games + 3 Learner plugins, shown next to EarTrainer's game selector and each Learner editor's title; a soundkorb.ru site link added to all four editors — the buildable subset of a full UI-designer brief the user shared (Figma styleguide, professional icon pack, FabFilter-grade animations - a real design job this codebase can't produce alone) — see [decisions/016](decisions/016-icons-and-site-link.md). A real edge-clipping bug in the site link found by actually running the app | ✓ **icons + link only** — no design-system styleguide, no light theme, no professional icon assets, no Figma mockups, no dedicated soundkorb.ru page (planned separately, later) |
| A user-supplied 90-rule audio-engineering knowledge base (`baza_znanij_audio_plaginy_v2.jsonl`) folded into `docs/knowledge_base.md` (paraphrased, 89 of 90 rules used), the three Learner plugins' parameter tooltips, all 9 EarTrainer games' instructions (all 12 languages), and one new `MicroLesson` per Learner plugin plus new steps in the originals — see [decisions/017](decisions/017-knowledge-base-content-pass-and-app-icons.md). A real pre-existing lesson-overlay z-order bug (same class as decisions/015/016) found and fixed while restructuring the lesson-picker code, and real per-plugin app icons (`ICON_BIG`) fixing every Standalone app's previously blank/default icon | ✓ |
| LearnerVerb: impulse-response visualization, decay-vs-frequency graph, stereo correlometer/vectorscope | ⏳ trimmed from the initial LearnerVerb build, see [decisions/004](decisions/004-learnerverb-scope.md) |
| Richer in-plugin tooltips (LearnerEQ/LearnerComp/LearnerVerb), each 2-4 original sentences with practical values plus a "Learn more" pointer into `docs/library_catalog.md` (title/author only), backed by `docs/knowledge_base.md`; all 9 EarTrainer games' `getInstructions()` also gained a practical tip (ADR 017) | ✓ |
| Per-control lesson-step highlighting (beyond the moving-knob cue) | ⏳ trimmed from the initial MicroLesson build, see [decisions/005](decisions/005-microlesson-architecture.md) |
| More lessons per plugin (each Learner plugin has exactly two today, see [decisions/017](decisions/017-knowledge-base-content-pass-and-app-icons.md)) | ⏳ still room for more |
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

## Recorded, not started

Three ideas raised during development, written down here so they are not
lost and not half-built. None of them has been started; each is listed
with what it would actually cost, because that is the part that decides
whether it happens.

### The settings side menu grows

`SettingsScreenComponent` is a side rail with three pages (About,
Appearance, Background) precisely so more can be added without another
redesign. Adding a page is a value in the `Page` enum, a label, and a
`resized()` branch.

### Ratings — by mix, by country

The idea: see how your accuracy compares to other players, sliced by
region.

What it needs, honestly: **a backend**. Accounts or stable anonymous ids,
a submission endpoint, storage, an aggregation job, moderation for the
inevitable garbage, and a privacy position on collecting per-country data
from people who did not ask to be measured. None of that is a plugin
feature; all of it is an ongoing service with an ongoing bill.

The reason this is worth flagging rather than shipping: percentiles were
proposed once already and refused, because there is no server and a
fabricated "better than 80% of users" is a lie. That reasoning has not
changed. A real leaderboard is buildable, it is just a different project
from this one.

### A mixing marketplace

The idea: export all your stems with one button and put a track up to be
mixed.

This is not an ear-training feature and should not live inside one. It is
a two-sided marketplace — listings, payments, escrow, disputes, file
transfer of multi-gigabyte stem folders, and a legal position on hosting
other people's unreleased music. The *only* part that belongs in a plugin
is "export all tracks with one button", and even that is a DAW function
the host already has.

If it happens, it is a separate product with a separate name, and the
plugins link to it rather than contain it.
