# 012. Version from `git describe`, plus a stable/beta/dev channel

## Status

Accepted, implemented for version derivation + channel detection + a
beta-aware `UpdateChecker`. CI channel wiring (nightly/beta/stable build
naming), settings-folder migration, and installer version-channel display
are deliberately deferred - see "What's not covered" below.

## Context

`shared/Version.h`'s `CurrentVersion::string` was a hand-maintained
literal, bumped by hand alongside `project(EarTrainer VERSION ...)` in
`CMakeLists.txt` and whatever git tag actually got pushed - two places to
remember to keep in sync, with nothing enforcing it. The ask was to
derive the version automatically from git tags instead, detect a release
channel (stable/beta/dev) from it, and make `UpdateChecker` channel-aware
(a beta channel should see pre-release GitHub releases; stable shouldn't).

## Decision

**`git describe --tags --dirty --always` at CMake configure time**, not
a hand-bumped literal. `--tags` includes lightweight tags (not just
annotated ones), `--dirty` appends `-dirty` when the working tree has
uncommitted changes, `--always` falls back to a bare short hash if no tag
is reachable at all in the current clone's history (rather than
`execute_process` just failing). The regex in `CMakeLists.txt`
(`^v?[0-9]+\.[0-9]+\.[0-9]+`) only catches that last "no tag reachable"
case and turns the bare hash into the spec'd `0.0.0-dev+sha1234567` form;
an exact tag (`v1.0.0`) or a tag-plus-commits description
(`v1.0.0-5-g1a2b3c4[-dirty]`) is already a real, usable version string
as-is and passes through unchanged.

**`configure_file()` into the build directory, not the source tree**:
`shared/VersionInfo.h.in` → `${CMAKE_BINARY_DIR}/generated/shared/VersionInfo.h`.
A generated file that changes on every commit (and every dirty-tree edit)
has no business being committed or even sitting in the source tree
between builds. `shared/Version.h` itself stays a real, committed,
hand-written file - it just `#include`s the generated one and re-exposes
`VersionInfo::versionString` as `CurrentVersion::string`, keeping every
existing call site (`CurrentVersion::string` in all four editors)
unchanged. Every target that needs it (`EarTrainer`, `LearnerEQ`,
`LearnerComp`, `LearnerVerb`, `EarTrainerTests`) gets
`${CMAKE_BINARY_DIR}/generated` added via `target_include_directories`.

**`shared/VersionChannel.h`** is a small, pure, header-only function
(`VersionChannel::detect(juce::String)`) completely decoupled from
CMake or git - it just pattern-matches whatever version string it's
given: `-beta` (case-insensitive, anywhere in the string) → beta; an
*exact* `vX.Y.Z` with nothing after the patch number → stable; anything
else (`-N-gHASH`, `-dirty`, the `0.0.0-dev+sha...` fallback shape) → dev.
Being pure and CMake-independent is what makes it directly unit-testable
with hand-written example strings (`tests/VersionChannelTest.cpp`)
without needing an actual tagged build to check against. Uses
`std::regex`, not a JUCE regex class - JUCE doesn't ship a general-purpose
one (`String::matchesWildcard()` only does glob patterns).

**`UpdateChecker` gained a `Channel` parameter** (`stable`/`beta`) on
`checkForUpdatesAsync` - the old two-argument signature still exists as
an inline wrapper defaulting to `Channel::stable`, so none of the four
existing editors' call sites needed to change. `stable` still hits
GitHub's `/releases/latest` endpoint (already excludes pre-releases, same
as before this ADR); `beta` hits `/releases` (the full list, newest
first) and a new pure function, `parseReleaseListJson(json,
allowPrerelease)`, walks it for the first entry matching the
pre-release constraint - `allowPrerelease=true` just takes the newest
entry outright; `false` skips leading pre-releases to find the newest
real release (useful in its own right, not just for the beta path).

## What's not covered (deferred)

- **No UI wired up yet** for picking beta vs. stable, or for an
  "automatically check once a day" toggle - the ask's channel-aware
  fetching and version-comparison logic is real and tested, but no editor
  currently calls the 3-argument `checkForUpdatesAsync` overload or
  persists a channel/auto-check preference. All four "Updates" buttons
  still do exactly what they did before (manual, stable channel).
- **CI doesn't yet tag build artifacts by channel.** Nightly artifacts
  with a `-nightly-<date>` suffix on every `main` push, and treating a
  `v*-beta*` tag's GitHub Release as "Pre-release" specifically, are both
  still just the plan, not implemented in `.github/workflows/build_and_test.yml`.
- **Two version sources still coexist.** `project(EarTrainer VERSION
  0.1.0)` in `CMakeLists.txt` is what the installer scripts' CI steps
  currently read for `abcTrain-<version>-*` filenames (see
  [decisions/008](008-installers.md)); this ADR's git-describe-derived
  `CurrentVersion::string` is a separate value used only for the
  in-plugin "Check for Updates" comparison. They coincide today (both
  read "0.1.0"-shaped output) as long as whoever cuts a release keeps
  bumping `project()`'s VERSION to match the tag being pushed, same
  discipline as before this ADR - unifying them into one source wasn't
  attempted this pass, to avoid touching the installer CI pipeline that
  was just fixed and confirmed green (see [decisions/008](008-installers.md)).
- **Settings/preset migration and a `Documents/abcTrain` settings
  location** weren't touched - `ProgressManager` and `LocalisationManager`
  still use the OS-default `PropertiesFile` location they always have.
  Moving that is a real, separate, user-visible change (existing users'
  saved progress needs an actual migration path, not just a new default
  path) that deserves its own careful pass, not a drive-by rename here.

## Consequences

- `CurrentVersion::string` is always accurate to the actual commit being
  built, including `-dirty`/`-N-gHASH` detail useful for bug reports -
  no more forgetting to bump a hand-written literal.
- `VersionChannel::detect()` and `UpdateChecker::parseReleaseListJson()`
  are both real, tested pure functions - not stubs - ready for whatever
  UI/CI wiring picks them up next.
- Local verification (this sandbox has git) shows the fallback path
  working correctly today: this repo has no tags yet, so
  `CurrentVersion::string` currently resolves to
  `0.0.0-dev+sha<short-hash>[-dirty]`, exactly the spec'd shape.
