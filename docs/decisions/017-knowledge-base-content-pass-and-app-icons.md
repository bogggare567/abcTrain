# 017. Knowledge-base content pass, richer lessons, and real app icons

## Status

Accepted, implemented, verified by a full local build of all four plugins
plus `EarTrainerTests` (125 test groups, 0 failures) and by inspecting the
generated `.icns`/`Info.plist` of each built Standalone app directly - not
just reading the code.

## Context

The user supplied `baza_znanij_audio_plaginy_v2.jsonl`, a 90-rule
structured knowledge base (EQ, compression, spatial effects, mastering,
acoustics, psychoacoustics) as a "gift", and asked for four things built
from it: an expanded `docs/knowledge_base.md`, richer in-plugin tooltips/
game instructions, expanded `MicroLesson`s, and updated docs - all in this
project's own words, never copied verbatim from the source file (see
[decisions/010](010-book-library-scope.md) for why paraphrasing, not
quoting, is the standing rule here). Separately, the user reported that
every plugin "opens looking strange, just a blank white app" - the
Standalone apps had no configured icon at all, so macOS was showing its
generic default.

## Knowledge base: 89 of 90 rules folded in, in this project's own words

Each of the 90 rules was read and categorized (see the condensed summary
this pass worked from), then synthesized - not transcribed - into new
paragraphs added to `docs/knowledge_base.md`'s seven relevant existing
sections (Эквализация, Компрессия и динамическая обработка, Реверберация
и пространство, Задержки и модуляционные эффекты, Мастеринг, Акустика
помещений, Психоакустика). The file grew from 320 to 705 lines. One rule
(`SPACE-007`, interactive occlusion/environment-morphing for game audio
middleware) was deliberately left out - it doesn't apply to a fixed-
parameter mixing plugin the way the other 89 do, and forcing it in would
have been padding, not synthesis.

## Tooltips and instructions: one practical addition each

`LearnerEQ`'s `FrequencyGuide`, `LearnerComp`'s `CompressorGuide`, and
`LearnerVerb`'s `ReverbGuide` each gained one added sentence per parameter/
frequency range, paraphrased from the newly-expanded knowledge base. All 9
EarTrainer games' `getInstructions()` gained a similar short practical tip
- translated (not independently rewritten) into all 12 supported
languages, since English games in this editor are localized through
`shared/i18n/strings/*.json`, not read directly from `getInstructions()`,
whenever a game has an i18n entry (see `translateGameInstructions()` in
`Source/PluginEditor.cpp`). `instructionLabel` grew from one line to two
(24px → 40px, window height 500 → 516) to fit the longer text, the same
"grow the window instead of truncating" precedent as decisions/010's
guide-label height increases.

## Lessons: a second lesson per Learner plugin, plus new steps in the originals

Each Learner plugin gained one brand-new `MicroLesson`, each teaching a
different technique than its existing lesson:

- LearnerEQ: **Find & Fix a Resonance** - EQ-001's boost-narrow-then-
  flip-to-cut technique for locating a resonant frequency.
- LearnerComp: **Bus Glue Compression** - COMP-003's moderate-ratio/slow-
  attack/small-GR recipe.
- LearnerVerb: **Bright vs. Dark Tail** - SPACE-011's point that high
  frequencies decay faster than low ones in a real space, and that
  Damping is what lets this reverb imitate that independently of overall
  Decay length.

Each original lesson also gained one new step (a low-shelf step in Vocal
EQ Basics, a knee step in Vocal Compression, a width step in Space for
Vocals) demonstrating a control the lesson hadn't touched before.

Since `LessonController` (`shared/LessonController.h`) takes exactly one
`MicroLesson` per instance - a deliberate, still-good design choice, not
something this pass needed to change - two lessons per plugin meant two
`LessonController` members and a small `lessonSelector` `ComboBox`
replacing each editor's old single "Lesson" button; only the selected
one is ever shown/started; the other stays hidden.

**A real, pre-existing z-order bug was found and fixed while touching
this code.** All three Learner editors added their `lessonController` as
a child *before* `updateButton`/`soundkorbLink` were added - meaning, per
JUCE's paint-in-addition-order rule (the same rule
[decisions/015](015-choice-slider-and-training-sounds.md) and
[decisions/016](016-icons-and-site-link.md) already had to fix once each
for other overlays), those title-row controls would paint *on top of* a
shown lesson overlay instead of being covered by it. This bug predates
this session's changes entirely - it's the original single-lesson wiring
every Learner editor has had since ADR 005 - and was only worth finding
now because this pass was already restructuring that exact code region.
Fixed by moving both `LessonController`s' `addChildComponent()` calls to
the very end of each editor's constructor, after every other child.

## App icons: real ICON_BIG per plugin, not stock/blank

`assets/icons/{eartrainer,learnereq,learnercomp,learnerverb}.png` - four
1024×1024 PNGs generated programmatically (Python/Pillow, no design tool
available in this environment) reusing the same dark background and
accent-colour palette as `AbcTrainLookAndFeel`, each with a distinct mark:
EarTrainer gets the three-bar "level meter" family mark; LearnerEQ,
LearnerComp, and LearnerVerb each get a rasterized version of their own
`shared/AppIcons.cpp` glyph (the EQ boost curve, the gain-reduction
bars-and-arrow, and the concentric circles, respectively) for visual
consistency between the in-app icon and the actual application icon.

Wired via `ICON_BIG <path>` in each of the four `juce_add_plugin()` calls
in `CMakeLists.txt` - JUCE's own `juceaide` tool generates the platform
icon format (macOS `.icns`, embedded in `Contents/Resources/AppIcon.icns`
with `CFBundleIconFile` set in `Info.plist`) at build time; no manual
`.icns`/`.ico` authoring needed. Verified directly: `sips -g all` on each
built Standalone app's generated `.icns` confirms a real, valid
1024×1024 `com.apple.icns`-format icon, not a placeholder or empty file.
The VST3/AU bundles' `Info.plist` also reference `AppIcon.icns`, but
JUCE's build doesn't actually copy the icon file into those bundles'
`Resources/` - a real gap, but a low-priority one, since a DAW's plugin
browser doesn't render a VST3/AU bundle's Finder icon the way opening a
Standalone `.app` does; the user's actual complaint ("opens looking like
a blank white app") was specifically about the Standalone experience,
which is now fully fixed.

## Consequences

- One JSONL rule (SPACE-007) is intentionally not reflected anywhere in
  the codebase - documented here so it isn't mistaken for an oversight.
- `docs/knowledge_base.md`'s "keep in sync" comment at the top now covers
  a meaningfully larger surface than before; any future edit to the
  tooltip/instruction/lesson content this pass touched should check that
  file first, same as it always should have.
- No new automated test: tooltip/instruction/icon content and the lesson
  picker's actual click behavior are all either plain string data or
  `paint()`/JUCE-widget behavior - same "needs an actual running app, not
  `EarTrainerTests`' console harness" reasoning as `ChoiceSliderComponent`
  (decisions/015) and `AbcTrainLookAndFeel` (decisions/009). The lesson
  picker's dropdown click-through specifically could not be exercised in
  this pass - an unrelated system menu-bar/notch-hiding utility on this
  machine intercepts clicks across the top of the screen regardless of
  which app is frontmost - but the corrected layout (icon/title/Updates/
  Bypass/lesson-picker all rendering without overlap) was confirmed
  visually in a running build.
- Icon source PNGs are simple flat programmatic shapes (~10-19KB each),
  not licensed/professional icon assets - same "not a substitute for real
  design work" caveat as decisions/016's line icons.
