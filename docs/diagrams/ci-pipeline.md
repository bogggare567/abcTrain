# CI pipeline

`.github/workflows/build_and_test.yml` exists and runs this on every push/
PR. **Confirmed green on all three OSes** as of commit `a2f2944`
(2026-07-24) — Windows, macOS, and Ubuntu all built every target and ran
`EarTrainerTests` to completion successfully — and again on `dd207d1`
(LearnerComp added). LearnerVerb, added after `dd207d1`, hasn't been
confirmed yet. The first green run was not the first attempt: two real
bugs surfaced and got fixed along the way, both worth knowing about if the
build ever regresses:

1. `LearnerEQProcessor::getName()` returned the `JucePlugin_Name` macro,
   which JUCE only defines for real `juce_add_plugin` targets. It's
   undefined for `EarTrainerTests` (a `juce_add_console_app` that compiles
   `LearnerEQ/Source/PluginProcessor.cpp` directly), so every compiler
   failed there with an undeclared-identifier error. Fixed by returning a
   literal string instead.
2. `cmake --build ... --parallel` with no number, on the Unix Makefiles
   generator (the default on Linux/macOS), means unlimited concurrent
   `make` jobs, not "one per core." Building three targets' worth of JUCE
   unity-build translation units at once OOM-killed the Ubuntu runner
   (exit 143). Fixed by capping it explicitly (`--parallel 2`).

This is the actual shape of `.github/workflows/build_and_test.yml`, not a
proposal — kept in sync with the real file, not an earlier draft of it:

```mermaid
flowchart TD
    A["Push / Pull Request"] --> B{"Matrix: ubuntu-latest,\nmacos-latest, windows-latest"}
    B --> C["Checkout"]
    C --> D{"Linux?"}
    D -- yes --> E["Install X11/GL/alsa/freetype dev packages\n(needed to compile, not just to run)"]
    D -- no --> F["Cache JUCE FetchContent\n(keyed on hash of CMakeLists.txt)"]
    E --> F
    F --> G["Configure: cmake -B build"]
    G --> H["Build: cmake --build build --parallel 2\n(all 4 plugins + EarTrainerTests)"]
    H --> I["Run EarTrainerTests"]
    I -- pass --> J["Check succeeds"]
    I -- fail --> K["Check fails, blocks merge"]
```

Deliberately **not** in this pipeline, all still open:

- **No artifact upload.** A green run proves the code builds and passes
  tests; it doesn't produce a downloadable `.vst3`/`.component` for anyone
  to actually try. Add this once there's a reason to hand someone a build
  without them cloning and compiling it themselves.
- **No code signing / notarization.** Out of scope until there's an actual
  release process (see phase 1.0 in `../roadmap.md`) — unsigned builds
  aren't distributable as-is on macOS/Windows regardless.
- **No separate fast/slow split.** Every OS builds all four plugins plus
  the test binary in one job; there's no cheap "just run the tests"
  Linux-only fast-fail step ahead of the (slower) macOS/Windows plugin
  builds. Worth revisiting if CI turnaround time becomes annoying.
