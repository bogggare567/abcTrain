# 013. Third-party UI libraries: phased, one library/one component at a time

## Status

In progress. Phase 0 (feasibility), Phase 1 (JUCE's own animation module,
one component), and Phase 2 investigation (`gin` - **declined**, see
below) are done. Phase 3 (`foleys_gui_magic`) has had its module metadata
checked but no actual prototype build attempted yet - see "Plan" below.

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

## Phase 2: `gin` - investigated, declined

Cloned `FigBug/Gin` to inspect its actual module structure before writing
any integration code (the plan's own Phase-2 first step). Its top-level
`CMakeLists.txt` `add_subdirectory()`s its own JUCE checkout and builds
its own demo/example/test targets - not something to pull in wholesale
via `FetchContent_MakeAvailable`, since it would fight this project's
already-fetched JUCE 8.0.15 and drag in unrelated targets. The correct
pattern would be vendoring just the specific module folder(s) via
`juce_add_module()` directly, bypassing Gin's own top-level
`CMakeLists.txt` entirely - the same approach that would work for any
JUCE module.

That's where it stops, though: **every single module in the repository -
`gin`, `gin_gui`, `gin_dsp`, `gin_graphics`, `gin_metadata`,
`gin_network`, `gin_plugin` - declares `minimumCppStandard: 20`** in its
own `BEGIN_JUCE_MODULE_DECLARATION` block. This project is pinned to
C++17 (`CMAKE_CXX_STANDARD 17` in the root `CMakeLists.txt`). There's no
"pick a smaller module to dodge this" option - it's a repository-wide
policy, checked across all seven modules, not a `gin_gui`-specific
requirement. Adopting *any* part of `gin` means raising this whole
project's C++ standard to C++20 as a prerequisite, not something scoped
to whichever component uses it.

**Declining `gin` for now**, for two independent reasons:
1. A project-wide C++ standard bump is its own separate, real,
   independently-risky change (however low the actual likelihood of
   breakage - JUCE 8 itself is fine with C++20) - it's not something that
   should ride along as a side effect of wanting one nicer-looking level
   meter.
2. `gin`'s own components come with their own visual style, which would
   need reskinning to match `AbcTrainLookAndFeel`'s existing dark theme
   (ADR 009) anyway - at which point the actual win over just continuing
   to invest directly in `AbcTrainLookAndFeel` is small. The "consistent,
   custom-themed look across all four plugins" goal ADR 009 already
   delivers is arguably in tension with dropping in a library that ships
   its own opinionated look.

Revisit if a future need specifically requires something `gin` provides
that isn't reasonably buildable in-house (its DSP-adjacent modules
`gin_dsp`/`gin_simd` might be worth a separate look for actual audio
processing needs, independent of anything GUI-related - but that's a
different question than this ADR's UI-library scope).

## Phase 3: `foleys_gui_magic` - metadata checked, no build attempted yet

Cloned `ffAudio/foleys_gui_magic` for the same inspection. Its module
declaration (`modules/foleys_gui_magic/foleys_gui_magic.h`) - unlike
every `gin` module - **does not declare a `minimumCppStandard`
requirement at all**; only its own repo's top-level `CMakeLists.txt` (for
building its own example/test targets) opts into C++20, which - same as
with Gin - wouldn't be inherited by vendoring just the module folder via
`juce_add_module()` directly. A quick scan of the module's ~96 source
files for hard C++20-only constructs (concepts, `<ranges>`, `co_await`,
`consteval`) found none. This is a promising signal, not proof - the only
way to actually know is a real compile.

**That real compile hasn't been attempted yet.** A genuine Phase 3
prototype (an isolated scratch CMake target, not a real plugin, per the
Plan above) is real, separate work - fetching the module, working out its
actual API for a `MagicProcessorState`-driven editor, and confirming it
configures and compiles against this project's pinned JUCE 8.0.15 at
C++17 - that this session ran out of room for after Phases 0-2. Left as
the next concrete step if this initiative continues.

## Consequences

- One real, working, verified animation improvement landed, with zero net
  new external dependencies (JUCE's own module, already fetched).
- The "juce_animate doesn't exist" finding is a reminder that a request
  naming a specific library is a claim someone made, not a guarantee it
  exists or is still maintained - worth a cheap reachability check before
  writing integration code against it, same lesson as this project's
  memory-recall guidance around "the memory says X exists" not meaning
  "X exists now."
- `gin` is declined, not deferred - readopting it later would still need
  the same C++20 discussion, this isn't a "come back to it" item without
  that being resolved first.
- Phase 3 (`foleys_gui_magic`) is the one still genuinely open: metadata
  looks favorable, but nothing has actually been compiled against it yet.
  This ADR will be updated (or superseded) once a real prototype attempt
  happens.
