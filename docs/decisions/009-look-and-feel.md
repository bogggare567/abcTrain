# 009. A shared dark theme (`AbcTrainLookAndFeel`) across all four plugins, kept basic on purpose

## Status

Accepted, implemented (basic pass only - see "What was deliberately trimmed" below).

## Context

Each editor was still using JUCE's stock default look - grey buttons,
the deprecated `Font(float, styleFlags)` constructor everywhere, and each
of the three Learner plugins picking its own one-off accent colour
(`deepskyblue` for LearnerEQ/LearnerComp, `mediumpurple` for LearnerVerb)
independently. The ask was a real redesign: a consistent dark theme,
rounded controls, smooth animated micro-interactions, gradient-filled
meters, pill-shaped tooltips, `FlexBox` layout - a large scope. Given
everything else landing in this same pass (five new games), the explicit
instruction was to build this iteratively: the shared look, the font
migration, and the colour scheme now; defer hover-fade timers,
press-scale springs, and gradient/shadow-heavy custom painting to a
later pass.

## Decision

**One shared `shared/AbcTrainLookAndFeel` (extends `juce::LookAndFeel_V4`)**,
constructed as a member of each of the four editors (`EarTrainerEditor`,
`LearnerEQEditor`, `LearnerCompEditor`, `LearnerVerbEditor`) rather than a
shared static instance - a real plugin can have several editor instances
open at once (e.g. three LearnerEQ instances in the same session), and a
per-editor member is the simplest way to avoid any shared-mutable-state
concern between them, at negligible cost (the class holds no per-instance
audio state, just paint logic and a colour scheme).

**`AbcTrainLookAndFeel` is declared as the *first* member in every
editor's class**, so C++'s reverse-order member destruction guarantees it
outlives every child `Component` that might still reference it while the
editor is being torn down. Each editor calls `setLookAndFeel(&lookAndFeel)`
as the first statement in its constructor and `setLookAndFeel(nullptr)`
in its destructor.

**Colour scheme**: `juce::LookAndFeel_V4::ColourScheme`'s nine slots map
directly onto specific component `colourId`s inside
`LookAndFeel_V4::initialiseColours()` (e.g. `highlightedFill` becomes
`Slider::rotarySliderFillColourId` *and* `TextButton::buttonOnColourId`
automatically). Setting this once is what lets the same blue/orange
accent apply to every rotary knob across all three Learner plugins,
instead of each editor calling `slider.setColour(...)` individually with
its own one-off colour - those per-instance overrides
(`deepskyblue`/`mediumpurple`) were removed for exactly this reason.

**`drawButtonBackground` must start from the `backgroundColour` parameter
JUCE passes in, not overwrite it.** The first version of this override
ignored that parameter and hardcoded a fill colour - which would have
silently broken EarTrainer's correct/wrong-answer colour-coding on choice
buttons (set per-instance via `setColour(TextButton::buttonColourId, ...)`
at answer time), since every button would have painted identically
regardless of what a caller asked for. Caught on read-through before this
shipped, not by a user report - see the fix in the same commit as this
ADR. The rounded-corner (6 px radius) + 1 px border treatment now
brightens whatever `backgroundColour` already resolved to on hover/press,
rather than replacing it.

**Deprecated `Font` migration**: every `juce::Font(float, styleFlags)` /
`juce::Font(float)` call across all four editors and
`shared/LessonController.cpp` was replaced with
`juce::Font(juce::FontOptions(...))`. Where a label just wanted the plain
14 px body size (the new default from `AbcTrainLookAndFeel::getLabelFont`),
the explicit `setFont()` call was removed entirely rather than kept as a
redundant override. `AbcTrainLookAndFeel::titleFont()`/`monoFont()` are
static helpers for the two sizes that *do* need a per-instance override
(22 px bold titles, 16 px monospaced numeric readouts) that a global
`getLabelFont()` default can't express, since only the specific label
knows it wants that size.

**One animation, not several**: `EarTrainerEditor::rebuildChoiceButtons()`
now fades each new choice button in over 200 ms via
`juce::Desktop::getInstance().getAnimator().fadeIn()` - the one explicitly
scoped-in animation, covering both an actual game switch and a mid-session
choice-count change (`ReverbGame`'s difficulty tiers). No custom
`ComponentAnimator` instance was needed; the shared `Desktop` animator is
the standard JUCE mechanism for a one-off fade like this.

## What was deliberately trimmed for this pass

- **Hover/press animations beyond the fade-in above.** `drawButtonBackground`
  changes colour immediately based on the `shouldDrawButtonAsHighlighted`/
  `shouldDrawButtonAsDown` flags JUCE already passes in - no `Timer`-driven
  alpha ramp, no press-scale-then-spring-back transform.
- **Gradient fills under the spectrum/waveform curves and pill-shaped
  tooltip backgrounds for the guide labels.** Both are real, visible
  polish items, not attempted here - `guideLabel` and friends are still
  plain text `Label`s with a coloured background from
  `AbcTrainLookAndFeel`'s scheme, not a custom rounded/shadowed component.
- **`FlexBox`-based layout.** Every editor still uses the same explicit
  `Rectangle::removeFrom*` layout it always has; only the colours, fonts,
  and control rendering changed in this pass.
- **Screenshots/mockups of the new look in `README.md`.** Described in
  text instead - this sandbox has no way to render a real JUCE window and
  capture it.

Revisit any of these once there's a concrete reason to (e.g. a real user
complaint that the flat colour-change-on-hover feels abrupt).

## Consequences

- All three Learner plugins now genuinely look like one product instead
  of three independently-styled ones - same accent colours, same button
  shape, same font sizes everywhere.
- `AbcTrainLookAndFeel` has no unit test - it's pure `Graphics`/`Font`
  drawing code with no logic to assert on, same class of thing as
  `SpectrumAnalyzerComponent`'s FFT/paint code (see
  [decisions/006](006-unified-visualization.md)) - verified by an actual
  local build of all four plugin targets instead.
- Five new games' `setDifficulty` methods follow two different shapes
  depending on what their choice labels actually mean - see
  `docs/diagrams/game-engine.md` for exactly which games keep fixed
  labels with converging values (`PanGame`/`StereoWidthGame`, same
  precedent as `CompressionGame`) versus fixed labels with a different
  scaled parameter (`DelayGame`/`DistortionGame`) versus the one game
  whose labels themselves change (`DBGame`).
