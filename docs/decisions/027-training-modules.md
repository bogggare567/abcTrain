# 027 — Training modules, and why Learner EQ does not get them

**Status:** accepted
**Date:** 2026-07-28

## Context

The Learner plugins had lessons: steps that set the knobs for you while
text explained what changed. You watched, and then you were done. Nothing
anywhere knew whether you could now *hear* the thing the lesson was about,
and there was no reason to open it a second time.

## Decision

A **module** is one knob: demonstrated, tried, then checked.

`shared/TrainingModule` is the data and the grading; `shared/LessonAudioBed`
generates the sound; `shared/ModuleProgress` remembers; and
`shared/ModuleScreenComponent` is the panel. Seven modules in Learner Comp,
seven in Learner Verb.

### The panel does not cover the whole editor

During the demonstration and the try-it step it sits over the analysis
section only, so the knobs stay visible and reachable. A lesson about a
knob you cannot touch is a slideshow. `hitTest` returns false outside the
painted panel, so everything below stays live without the editor having to
cooperate.

During the **check** it takes the whole area, and that is not layout
convenience: with the spectrum and the knobs on screen the answer is
readable off the display, and a check you can see the answer to is not a
check. It grows into place rather than jumping, so where it went is legible.

### Reference / Mine

The check sets a hidden value, plays it, and asks you to dial the same
thing. Without a way to switch back to the reference that is not a
listening task, it is a guess — so the two audition buttons are load-
bearing, and the active one is lit rather than merely stated in text.

### Walkthroughs live in the same shelf

The two existing multi-knob lessons per plugin are listed under the
modules rather than behind a second dropdown. Two doors marked "teach me
something" in one title row was the confusion this removes. A module is
one knob; a walkthrough is one workflow, and the divider says so.

### Leaving puts the plugin back

Every parameter is saved on entry and restored on exit. A teaching screen
that silently rewrites your settings is a teaching screen you stop opening.

## Learner EQ deliberately gets no modules

Asked for directly, and the reasoning is right: graphical EQs with many
bands, dynamic processing and better analysis already exist and are
excellent. Teaching someone to turn *this* EQ's four knobs competes with
FabFilter and loses, and it is not what a beginner is actually missing.

What they are missing is **where things live**: what a kick's fundamental
is against its beater click, where a voice's body ends and its harshness
starts, which range is "mud" and why. That is a map, not a knob drill, and
it wants a different feature — zones and fundamentals drawn on the
spectrum Learner EQ already has, per source. Recorded here so the absence
reads as a decision rather than an omission; the map itself is not built
yet.

## Consequences

- Module text is English, like the parameter tooltips and lesson steps it
  sits beside. A module whose *name* were translated while its own
  explanation was not would read worse than one honestly in one language.
- The processor holds a raw pointer to the panel's bed buffer via
  `PracticeAudioSource::setOverrideBuffer`, cleared in the destructor.
- `PracticeAudioSource` now plays stereo sources; the library's clips are
  mono, the beds are not.
- Not tested automatically: the panel itself. The grading and the beds are
  (`TrainingModuleTest`, `LessonAudioBedTest`); the screen is covered the
  way every other screen here is, by rendering it — `EditorSnapshots` has
  a module-shelf and a mid-check shot in both themes.
