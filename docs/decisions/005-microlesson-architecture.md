# 005. `MicroLesson` as pure data/state, `LessonController` as the only thing that touches APVTS or UI

## Status

Accepted, implemented.

## Context

Each Learner plugin needed a guided lesson: a sequence of steps, each with
explanation text and parameter values the plugin should jump to, that the
player can step through with Next/Back. The question was how to split
this between "what a lesson *is*" (data: steps, current position) and
"what happens when a step becomes active" (behavior: set APVTS
parameters, show text, and originally, highlight the relevant control).

## Decision

**`MicroLesson` (`shared/MicroLesson.h`) is a pure state machine** — a
title, a `std::vector<LessonStep>`, and a cursor. `LessonStep` is a plain
aggregate (`explanationText` + a list of `(parameterID, value)` pairs). No
constructor beyond aggregate init, no dependency on
`AudioProcessorValueTreeState`, no dependency on any UI class.
`start()`/`nextStep()`/`previousStep()` just move the cursor and report
whether the move was legal. This is what makes it directly unit-testable
(`tests/MicroLessonTest.cpp`) with no processor, no editor, no console-app
GUI-instantiation concerns of the kind already documented for
`applyPreset` testing in [testing-strategy.md](../testing-strategy.md).

**`LessonController` (`shared/LessonController.h`/`.cpp`) is the only
thing that touches APVTS or draws anything.** It owns a `MicroLesson`
and an `AudioProcessorValueTreeState&`; on every step change it calls
`setValueNotifyingHost` for each of that step's target parameters (same
pattern `applyPreset` already established in LearnerComp/LearnerVerb) and
updates its own text/progress labels. It's a `juce::Component`, meant to
be added as a full-size child of a Learner plugin's editor and toggled
visible via a "Lesson" button.

**Lesson content lives per-plugin, not in `shared/`.** Each Learner
plugin has its own lesson-content header (`LearnerEQ/Source/VocalEqLesson.h`,
`LearnerComp/Source/VocalCompressionLesson.h`,
`LearnerVerb/Source/VocalSpaceLesson.h`) exposing a single
`build...Lesson()` factory function that references that plugin's actual
parameter IDs. Only the *machinery* (`MicroLesson`, `LessonController`) is
shared — the content is inherently plugin-specific, the same reasoning
`CompressorGuide`/`ReverbGuide`'s preset tables already follow (tooltip
text and presets live with the plugin they describe).

## What was cut from the original scope: per-control highlighting

The initial ask included highlighting the specific UI control a step is
adjusting. This was dropped for this pass: every target parameter already
has a `SliderAttachment` (or `ComboBoxAttachment`) wired up in its editor,
so when `LessonController` calls `setValueNotifyingHost`, the matching
knob visibly rotates/moves on its own — **that motion already draws the
player's eye to the right control**, without needing a second, separate
highlight-drawing mechanism duplicated across three editors with
different knob layouts. If it turns out the moving-knob cue isn't enough
in practice (e.g. a step changes a parameter that's off-screen, or the
motion is too subtle at low knob-travel), revisit this - the `LessonStep`
data model deliberately doesn't need to change to add it back, since
`LessonController` already knows which parameters each step targets.

## Consequences

- Adding a lesson step is editing a data literal (one more `LessonStep`
  in a `build...Lesson()` function) — no `LessonController` or
  `MicroLesson` code changes needed, the same "adding a game is one file"
  shape `Game`/`GameManager` established in
  [ADR 001](001-game-interface.md).
- `LessonController`'s constructor takes `MicroLesson` **by value**
  (moved from a temporary returned by `build...Lesson()`), so each editor
  owns an independent lesson instance — restarting a lesson (clicking
  "Lesson" again) always replays from step 1 rather than resuming
  mid-lesson state from a previous session, which matches what a
  literally-titled "Lesson" button should do.
- No automated test exercises the actual APVTS-setting behavior in
  `LessonController` itself (only `MicroLesson`'s pure step logic is
  tested) — that behavior is the same shape as `applyPreset`, which *is*
  tested, so the risk is judged low, but it's still an untested path,
  consistent with this project's broader gap around integration/GUI
  testing (see `testing-strategy.md`).
