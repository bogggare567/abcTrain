# 007. Downloadable CI artifacts, tag releases, and a manual "Check for Updates"

## Status

Accepted, implemented.

## Context

Before this, CI (`.github/workflows/build_and_test.yml`) only proved the
code builds and passes tests - it never produced a build anyone could
actually download and try. Testing the plugins meant cloning the repo and
compiling locally. The ask was to close that gap: make a fresh build
downloadable after every push, publish tagged releases, and let each
plugin check GitHub for a newer version on its own.

## Decision

**CI now uploads build artifacts** (`actions/upload-artifact@v4`, one
artifact per OS: `plugins-ubuntu-latest`, `plugins-macos-latest`,
`plugins-windows-latest`), uploaded right after the Build step and before
running tests - so a build that compiles but fails a test still leaves a
downloadable artifact to test manually. No separate zip step is needed;
`upload-artifact` zips the given paths on its own.

**Pushing a `vX.Y.Z` tag now publishes a GitHub Release** with all three
OS builds attached, via a `release` job gated on
`startsWith(github.ref, 'refs/tags/v')` and `needs: build-and-test` (a
failing OS blocks the release). `on: push` with no branch/tag filter
already covered tag pushes, so no separate trigger was needed - just the
new job.

**Each plugin gets a manual "Check for Updates" button**, not a
background daily timer. `shared/UpdateChecker.h/cpp` splits this cleanly:

- `isNewerVersion(latest, current)` and `parseReleaseJson(json)` are pure
  functions - dotted-integer version comparison and picking `tag_name`/
  `html_url` out of GitHub's "get latest release" JSON shape - with no
  dependency on networking or the message thread. This is what
  `tests/UpdateCheckerTest.cpp` exercises directly, same reasoning as
  `MicroLesson` (see [decisions/005](005-microlesson-architecture.md)).
- `checkForUpdatesAsync(currentVersion, callback)` is the one impure
  piece: `juce::Thread::launch` fetches
  `https://api.github.com/repos/bogggare567/abcTrain/releases/latest` on a
  background thread, then posts the result back via
  `juce::MessageManager::callAsync`. Any failure - no internet, GitHub
  rate-limiting, an unexpected response shape - is silently swallowed;
  nothing is shown to the user unless a newer version was genuinely
  found.
- Each editor wires its own "Updates" button to this (duplicated across
  all four editors rather than extracted into a shared UI helper -
  `shared/UpdateChecker.h` stays free of any GUI dependency, and this
  matches the existing precedent of the Bypass-button wiring being
  duplicated identically across the three Learner editors rather than
  shared). A `juce::Component::SafePointer` guards the callback against
  the editor having been closed while the network request was in flight.

**Why manual, not automatic:** the original ask included a background
daily timer check. That was cut for this pass - a plugin making its own
unsolicited network calls without the user clicking anything is a bigger
step than a button they choose to press, and some hosts/DAWs sandbox or
frown on unexpected plugin network activity. A "Check for Updates" button
gets the same practical benefit (the developer/tester can always see if
there's a newer build) without that concern. Revisit if it turns out
manual checking is too easy to forget.

## `NEEDS_CURL TRUE`, not a manually-set `JUCE_USE_CURL`

JUCE's own Linux networking (`juce_Network_linux.cpp`) has no TLS support
at all outside of libcurl - the non-curl fallback path is raw POSIX
sockets with no SSL/TLS handling, so `https://api.github.com/...` would
simply fail to connect on Linux without curl. JUCE's CMake integration
has a purpose-built option for this
(`extras/Build/CMake/JUCEUtils.cmake`): passing `NEEDS_CURL TRUE` to
`juce_add_plugin()`/`juce_add_console_app()` both defines
`JUCE_USE_CURL=1` *and* links `libcurl` via pkg-config, automatically, on
Linux only - it's a no-op on macOS/Windows, which use their own native
HTTPS-capable networking (NSURLSession, WinHTTP) regardless of this flag.

This is why the project's previous `JUCE_USE_CURL=0` compile definitions
were removed from the four real plugin targets (they'd have conflicted
with what `NEEDS_CURL TRUE` sets) rather than just left alongside the new
flag. `EarTrainerTests` deliberately keeps no `NEEDS_CURL`/curl linking at
all - it only calls `UpdateChecker`'s pure functions, never
`checkForUpdatesAsync`'s real network call, so it has no need for it (and
no need for `libcurl4-openssl-dev` in CI's Linux dependency install step,
which the four real plugin targets do need there now).

This was verified by reading JUCE's own source
(`juce_Network_linux.cpp`, `JUCEUtils.cmake`) directly rather than
guessed, and confirmed by an actual local build of all four plugin
targets plus `EarTrainerTests` before pushing - see
[docs/testing-strategy.md](../testing-strategy.md) and the
`project_eartrainer_local_build` memory note for why that's now the
default way to verify a change here, not just CI.

## A real bug this work exposed, unrelated to the feature itself

Running `EarTrainerTests` locally more than once on the same machine (for
the first time in this project's history) exposed that
`tests/ProgressManagerTest.cpp`'s `makeTempOptions()` helper wasn't
actually temporary: it pointed at a fixed, real
`~/Library/Application Support/EarTrainerTests_<name>/` path that
`juce::PropertiesFile` persists to disk by design. A second run on the
same machine loaded the *first* run's leftover score/streak state instead
of starting fresh, making "a correct answer awards points and can level
up" et al. fail unpredictably. CI never hit this (a fresh container every
run), so it was invisible until local testing became the practice. Fixed
by deleting any existing file for that path at the top of
`makeTempOptions()`, guaranteeing a clean slate regardless of prior local
runs - see the commit for details.

## Consequences

- The developer can download a build from the Actions tab after every
  push, or from a GitHub Release after pushing a `vX.Y.Z` tag, without
  cloning and compiling.
- Each plugin can tell the user there's a newer version available, but
  only when they ask (the "Check for Updates" button) - nothing runs in
  the background.
- `CurrentVersion::string` (`shared/Version.h`) is a plain literal, bumped
  by hand alongside `project(EarTrainer VERSION ...)` and whatever tag
  gets pushed - not wired up automatically, so it's possible to forget to
  bump it. `JucePlugin_VersionString`/`ProjectInfo::versionString` were
  both considered and rejected: the former isn't defined in
  `EarTrainerTests` (the three Learner editors are compiled into that
  target too - same class of problem as `LearnerEQProcessor::getName()`,
  see `docs/diagrams/ci-pipeline.md` bug 1), and the latter needs
  `juce_generate_juce_header()`, which this project doesn't call anywhere
  else.
- No automated test exercises the real network call, the AlertWindow, or
  the button wiring itself - consistent with this project's existing
  "no `Component`/no real network call in the console test binary"
  policy (see `docs/testing-strategy.md`).
