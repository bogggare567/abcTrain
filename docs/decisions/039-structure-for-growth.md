# 039 — Structure for growth: shared/ as libraries, the engine seam, a lighter repository

**Status:** accepted
**Date:** 2026-09-21

The user asked for three things at once: clean the project up, give it an
architecture, and make further growth possible — including the education
direction recorded in the roadmap the same day (Training → Teaching →
Live). ADR 038 had deferred an outside review's proposal to restructure
the tree "before the dependencies are mapped". This ADR maps them, then
does the part of the restructuring the map justifies.

## The map, measured

Computed from the real `#include` graph, **transitively** — a product
that includes `LearnerEditorBase.h` uses everything that header pulls in.
(A direct-include count gives the wrong answer: it called `WindowFit`
trainer-only, when every Learner editor reaches it through its base.)

| Used by | Units |
|---|---|
| all four products | 20 — the theme and widgets, i18n, the analysers, the training library and stems, the update check |
| the three Learner plugins only | 12 — modules, lessons, the Learner editor base, A/B, meters |
| the trainer only | 8 — Vectorscope, TourOverlay, the test-signal and ambience generators, gain match |
| nobody | `LessonController` (dead since ADR 037), `TestUtils` (tests only), `VersionChannel` (tested, not yet wired) |

**No layer violation:** nothing in `shared/` includes a product's code.
That held without any rule enforcing it, and the new layout keeps it
checkable at a glance.

The build had the real problem. The same twenty-odd shared `.cpp` files
were listed by hand **seven times** — four plugins, the test binary, the
editor tools, RoundBench — and the trainer's fourteen engine files three
times. One of those lists carried a comment warning that two lists "would
drift the first time a screen was added to only one of them". They
already had: `LessonController.cpp` was compiled into five targets that
never called it, and `TourOverlay.cpp` into three plugins that never
showed it.

## What changed

### shared/ is folders by responsibility

```
shared/
  ui/        theme, look-and-feel, fonts, icons, small widgets, window fit,
             tour overlay, screensaver
  analysis/  what the screen draws from audio: spectrum, waveform,
             gain-reduction meter, vectorscope, A-weighted meter
  audio/     sound that is not a plugin's processing: the training library,
             the slicer, stems, generators, preset families
  learning/  modules, lessons, the Learner editor base, A/B, the practice
             source, the difficulty ramp
  updates/   version, update check and prompt, installed plugins
  i18n/      the localisation manager and the string tables (unchanged)
```

**`shared/` means "may be used by any product", not "is used by two".**
The eight trainer-only units stay: `Vectorscope` is on the roadmap for
Learner Verb, and `TourOverlay` was already compiled into the plugins for
a tour they will get. Moving them to `Source/` today and back tomorrow is
churn.

### Each folder is one CMake library

`abc_ui`, `abc_analysis`, `abc_audio`, `abc_learning`, `abc_updates`,
`abc_i18n`, plus `abc_shared` (all of them, for the tests and tools). Each
lists its sources **once**. A product links what it uses; a group links
the groups it depends on:

```
abc_learning ──► abc_ui, abc_analysis, abc_audio, abc_updates
abc_analysis ──► abc_ui
abc_updates  ──► abc_ui
abc_ui       ──► abc_i18n
abc_audio    ──► (nothing)
```

They are **INTERFACE** libraries, deliberately. JUCE code must be compiled
with each product's own configuration (plugin name and codes, the
`JucePlugin_*` defines), so a static library shared between products would
be wrong. An INTERFACE library compiles its sources into each consumer
exactly as the hand-written lists did: one list, the same build. Products
link them `PRIVATE`; a `PUBLIC` link would hand the sources on to the
VST3/AU/Standalone wrappers and define every symbol twice.

The trainer links `ui analysis audio updates`; each Learner plugin links
`learning`, which brings the rest. Two plugins now compile one or two
translation units they did not before (the screensaver, the vectorscope);
that is the price of not maintaining exceptions, and it is small.

### The trainer's engine is its own library

`abc_trainer_engine`: the nine games, `GameManager`, `SessionManager`,
`ProgressManager`, `Achievements`, `HearingGuard`. None of it draws
anything (`SessionManager` includes the GUI module header but uses no
component). This is the seam the education layers need — an exercise that
can be asked and graded without the trainer's window — and it removed the
three copies of fourteen files. RoundBench keeps its own short list: it
times six games and must not pull in the managers.

### Includes are written from the repository root

`#include "shared/ui/AbcTrainTheme.h"`, from anywhere. Before, a file's
includes counted `../` segments from wherever it happened to sit, so any
move broke every includer. The rewrite was done by resolving each include
against the **old** layout before moving anything, so every path is the
file it always meant, not a name match. `shared/updates/Version.h` already
included the generated `shared/VersionInfo.h` this way.

### The repository got lighter

- **Only the screenshots a page shows are committed** — 14 images, the ones
  the README, the wiki and the website brief link to. The other 61 (30 MB)
  were the full gallery, re-committed on every visual pass; each pass added
  18–30 MB to the history. `tools/EditorSnapshots` still renders the whole
  gallery, into `editor-snapshots/`, now ignored.
- `mac install.mp4` (13 MB, linked from nowhere) left the repository; a copy
  stays in the project folder beside it.
- `shared/LessonController` deleted; `shared/TestUtils.h` moved to `tests/`.

**History was not rewritten.** The pack is ~290 MB and would shrink a lot
without the old galleries, but rewriting published history breaks every
existing clone and fork. Stopping the growth is the part that matters.

## Where the next things go

| Next thing | Where |
|---|---|
| A new Learner plugin (Delay, Saturation…) | its own folder like `LearnerVerb/`, links `abc_learning` |
| A new exercise | `Source/Games/`, added to `abc_trainer_engine` — **one** list now |
| A new shared widget or meter | the matching `shared/<group>/`, added to that group's library |
| **Teaching** (assignments, teacher reports) | the engine plus a file format — see the roadmap; no server |
| **Live** (a hall votes from their phones) | `website/`, next to the browser demo that already ports the exercises; the C++ stays offline |

The Live layer is deliberately **not** C++. The browser demo already runs
the exercises in JavaScript, fed by `website/src/generated/plugin-facts.json`,
which `website/tools/sync-from-plugin.mjs` extracts from this source so the
two cannot drift. A presenter page and an audience page belong beside it.
Anything that needs a server — relaying votes — is a separate, optional
deployable; the product's promise of no account, no server and no
telemetry stays true for everything that is installed.

## Not done

- **No `products/` folder.** Moving `Source/` and `Learner*/` under one
  more directory buys nothing a reader needs and rewrites every path in
  every document.
- **No static libraries** (see above) and no precompiled headers yet; build
  time is unchanged, not improved.
- The layer rule (`shared/` never includes a product; `abc_audio` depends on
  nothing) is kept by review, not by a check. A script over the include
  graph could enforce it in CI; it would take an afternoon.
- Architecture decision records 001–038 keep the paths that were true when
  they were written. No file was renamed, only moved: `shared/X.h` is now
  `shared/<group>/X.h`, and the folder list above says which group.
