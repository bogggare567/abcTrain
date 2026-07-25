# 013. Third-party UI libraries: phased, one library/one component at a time

## Status

In progress. Phase 0 (feasibility) and Phase 1 (JUCE's own animation
module, one component) done. Phase 2 (`gin`, one component) and Phase 3
(`foleys_gui_magic`) not started - see "Plan" below.

## Context

The ask was to replace `AbcTrainLookAndFeel` and every editor's UI
wholesale with three third-party libraries in one pass:
`foleys_gui_magic` (declarative XML/ValueTree GUI), `gin` (ready-made
styled components), and a library referred to as "juce_animate" for
animations. Doing that in one shot - swapping the entire working, tested,
ADR-009-documented editor architecture for an unproven (in this
environment) framework across all four plugins simultaneously - was
explicitly declined earlier as too high-risk: no incremental point to
verify from, and a large chance of leaving the build broken with no easy
partial rollback.

This ADR is the walked-back version: the same libraries, adopted one at a
time, one real component at a time, each phase built and tested locally
(and pushed for CI confirmation) before the next begins.

## Plan

- **Phase 0 - feasibility check.** Confirm each library is actually
  reachable and real before writing any integration code.
- **Phase 1 - lowest risk: an animation library, one component, one
  plugin.** Pick a single, currently-static piece of UI and give it a
  real eased animation.
- **Phase 2 - `gin`: one ready-made component, one plugin.** Replace a
  single custom-painted control with a `gin` equivalent, leaving
  `AbcTrainLookAndFeel` in charge of everything else.
- **Phase 3 - `foleys_gui_magic`: prototype only, not a real plugin yet.**
  Given it proposes a fundamentally different editor architecture
  (declarative XML/`ValueTree` + `MagicProcessorState`, not incremental
  component swaps), the first real step is learning its API and confirming
  it builds in this environment in an isolated scratch target - *not*
  converting a real editor - before deciding whether adopting it for real
  is even worth the rewrite cost.

Each phase: build all four plugin targets + `EarTrainerTests` locally,
confirm the test suite still passes, then commit and push for a real CI
confirmation before starting the next phase.

## Phase 0 findings

`gin` (`FigBug/Gin`) and `foleys_gui_magic` (`ffAudio/foleys_gui_magic`)
are both real, reachable repositories.

**"juce_animate" at `nick-thompson/juce_animate` does not exist** - a 404,
and a GitHub search for "juce_animate" turns up nothing under any owner.
Rather than substitute a different, unvetted third-party animation
library, this repo's already-pinned **JUCE 8.0.15 ships its own official
`juce_animation` module** (`Animator`, `ValueAnimatorBuilder`,
`VBlankAnimatorUpdater`, `Easings::createEaseOut()`/`createBounce()`/etc.)
- first-party, guaranteed compatible with the exact JUCE version already
in use, and zero new external dependency. This is a strictly better
choice for the animation piece than the originally-named library, not a
compromise.

## Phase 1: `juce_animation` on `EarTrainerEditor::LevelProgressBar`

`Source/PluginEditor.h`'s `LevelProgressBar` now eases its fill from the
previous value to the new one over 400 ms via
`Easings::createEaseOut()`, instead of snapping straight to the new
proportion - the "прогресс-бар: заполнение с ease-out" behavior from the
original ask, just via JUCE's own module rather than a nonexistent one.
`juce::juce_animation` is linked into the `EarTrainer` target only (the
only target that compiles `Source/PluginEditor.cpp` - `EarTrainerTests`
doesn't, so it needed no CMake changes).

**A real bug caught immediately on the first build**: `juce::Animator`
has no default constructor (only `explicit Animator(std::shared_ptr<Impl>)`),
so declaring `juce::Animator currentAnimator;` as a plain member left
`LevelProgressBar`'s default constructor implicitly deleted - a hard
compile error, not a runtime surprise. Fixed by giving it a real initial
value (`juce::ValueAnimatorBuilder{}.build()`, a valid-but-never-started
placeholder) that `setProgress()` overwrites on first real use.

**Also handled**: if `setProgress()` is called again while a previous
animation is still mid-flight (rapid level-ups), the old `Animator` is
fast-tracked via `.complete()` *before* `currentAnimator` is reassigned -
its `onComplete` callback reads the member by name (not a captured
value), so completing it first ensures it removes *itself* from the
`VBlankAnimatorUpdater`, not whatever animator the member holds by the
time the callback actually fires.

Verified locally: all four plugin targets build clean, `EarTrainerTests`
passes in full (116 test groups, unaffected since `EarTrainerTests`
doesn't compile EarTrainer's own `PluginEditor.cpp`).

## Consequences

- One real, working, verified animation improvement landed, with zero net
  new external dependencies (JUCE's own module, already fetched).
- The "juce_animate doesn't exist" finding is a reminder that a request
  naming a specific library is a claim someone made, not a guarantee it
  exists or is still maintained - worth a cheap reachability check before
  writing integration code against it, same lesson as this project's
  memory-recall guidance around "the memory says X exists" not meaning
  "X exists now."
- Phases 2 and 3 are unstarted; this ADR will be updated (or superseded)
  as each lands.
