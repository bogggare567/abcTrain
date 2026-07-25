# 008. Real per-OS installers: pkg/DMG (macOS), Inno Setup (Windows), tar.gz + install.sh (Linux)

## Status

Accepted, implemented. macOS and Linux verified locally end-to-end. The
Windows Inno Setup script **did fail its first real CI compile** (a typo
in a `[Files]` flag) - caught, fixed, and now confirmed - see "What's
verified vs. what isn't" below.

## Context

Downloadable CI artifacts (see [decisions/007](007-update-checker.md))
are just the raw `*_artefacts/Release` folders - copying the right files
into the right plugin folder is still on the user. The ask was a real
installer per OS: pick which of the four plugins, pick which format(s)
(VST3/AU/Standalone), pick where they go, see the license first.

## Decision

**macOS**: `installer/macos/build_installer.sh` builds one `pkgbuild`
component package per (plugin × format) - 12 packages - then combines
them with `productbuild --distribution installer/macos/distribution.xml`
into a single product `.pkg`, wrapped in a `.dmg` alongside a
double-clickable `Open Plugins Folder.command` helper. The distribution's
`<choices-outline>` nests three format checkboxes (VST3/AU/Standalone,
each with its own plain-language description) under each of the four
plugin checkboxes - unchecking a plugin's parent box unchecks its
children too, which is standard `Installer.app` tree behavior, no extra
scripting needed.

**System vs. user install location, natively, via `<domains
enable_currentUserHome="true">`**: for any component whose
`--install-location` falls under `/Library` (VST3, AU), setting this one
distribution-level flag makes `Installer.app` show its own built-in
"Install for all users of this computer" vs. "Install for me only"
toggle, relocating those components to `~/Library/...` automatically when
the user picks the latter. No custom code, no two copies of each
component package.

**What macOS's `.pkg` format genuinely cannot do: a free-text custom
install path.** The stock `Installer.app` UI only lets the user pick
which *disk/volume* to install to, not an arbitrary folder path - that's
a fundamental limitation of the format, not something `productbuild`
exposes and this project simply didn't use. Building real custom-path
support would mean writing an actual Installer plugin bundle (its own
Xcode target, a distinct and much larger effort), so it was not attempted
here. System/user covers the realistic need; anyone who genuinely wants a
different folder can move the installed bundle afterward (VST3/AU/AU
hosts don't care where the file lives as long as it's on their scan path).
Standalone `.app` builds always go to `/Applications/abcTrain` -
standard convention for Mac software, not something users typically want
to relocate, so no extra choice was added there.

**"Open the plugins folder" after install**: `Installer.app`'s conclusion
pane has no custom action button, and running a script reliably only-
once-at-the-very-end across 12 independently-ordered component packages
isn't something to depend on. Solved by *not* trying to hook the
installer's own lifecycle: the DMG ships a separate
`Open Plugins Folder.command` the user can double-click afterward, same
practical effect with no ordering assumptions.

**Windows**: `installer/windows_setup.iss` (Inno Setup 6) - a real
`[Components]` tree (same plugin/format shape as macOS), the standard
Select Destination Location page for `{app}` (Standalone `.exe`s, default
`{pf}\abcTrain`), *plus a second custom directory page*
(`CreateInputDirPage`, anchored after `wpSelectComponents`) specifically
for VST3, defaulting to `{commoncf}\VST3` - **this is the one platform
where a genuine free-text custom path was actually built**, since Inno
Setup's directory-page widgets support it natively where macOS's
`Installer.app` doesn't. The VST3 page is skipped entirely
(`ShouldSkipPage`) if no VST3 format was selected on the components page.
License page reads the repo's `LICENSE` file directly. Start Menu
shortcuts per Standalone app, an uninstall entry (automatic from
`AppName`/`AppVersion`), and a post-install prompt to open the install
folder or launch Ear Trainer.

**A real ordering bug caught on read-through, not compile:** the custom
VST3 directory page was first anchored after `wpSelectDir` (the standard
page), which comes *before* `wpSelectComponents` in Inno's default wizard
order - meaning `ShouldSkipPage`'s `IsComponentSelected()` check would
have run before the user had picked any components yet, always seeing
whatever the initial/default selection was rather than the user's actual
choice. Fixed by anchoring after `wpSelectComponents` instead. Caught by
tracing the actual default page order rather than assuming the anchor
point didn't matter - the kind of bug that would only have shown up as
"the VST3 path page appears with stale skip-logic" during a manual click-
through, not a compile error.

**Linux**: kept as a `tar.gz` (no `.deb`/`.rpm` - no evidence yet that
distro-specific packaging is worth the added complexity for a pre-release
build), but now with real structure:
`installer/linux/package_tar.sh` lays out `<Plugin>/VST3/` and
`<Plugin>/Standalone/` per plugin plus `install.sh`/`README.txt`/
`LICENSE` at the top level. `install.sh` interactively asks which
plugins to install and where VST3s should go (`$HOME/.vst3`,
`/usr/lib/vst3` with `sudo`, or a typed custom path - Linux has no
`Installer.app`-style constraint, so the full free-text option was easy
here too) and optionally where to put the Standalone builds.

## What's verified vs. what isn't

This sandbox has Homebrew + Xcode command line tools, so
`pkgbuild`/`productbuild`/`hdiutil` are all real, locally-runnable tools -
**the macOS installer was built and inspected end-to-end**: `pkgutil
--expand` on the actual output DMG's `.pkg` confirmed all 12 components
have the right `identifier`/`install-location`/`auth`, the `Distribution`
script and `Welcome.txt`/`ReadMe.txt`/`LICENSE.txt` resources are embedded
correctly, and the whole thing built with zero errors - not just "no
error printed," but structurally correct.

**`install.sh` was run interactively too** (piped answers, a
`HOME`-overridden test run), confirming plugin selection, the VST3-
destination menu, and the Standalone copy all work as written - bash
logic is fully portable regardless of the actual `.vst3`/binary format
inside, so this genuinely exercises the same code path Linux CI will run.

**`installer/windows_setup.iss` could not be compiled locally** - there's
no Windows in this environment, so it was checked carefully on
read-through only (including catching and fixing the page-ordering bug
above) before its first real CI compile. **That first real compile
failed**: every `[Files]` line copying a VST3 bundle used
`Flags: recursesubdirs createallsubdirdirs ignoreversion`, but the real
Inno Setup 6 flag is `createallsubdirs` (one "dir", not "dirdirs") -
`iscc` rejected it outright with "Parameter Flags includes an unknown
flag" on the first such line and aborted before compiling anything else.
Read-through alone didn't catch it because the misspelled flag still
*reads* like a plausible real one; only an actual `iscc` run surfaces an
unknown-flag error. Fixed by correcting all four occurrences (one per
plugin's VST3 line) to `createallsubdirs`. Same lesson as the
visualization-unification commit's build failure earlier in this
project's history (see
[docs/diagrams/ci-pipeline.md](../diagrams/ci-pipeline.md), bug 3): a file
that can't be exercised locally needs its first real CI run watched
closely, not assumed correct because it reads correctly.

## Consequences

- **These builds are unsigned.** macOS Gatekeeper will show an
  "unidentified developer" block on the `.pkg` (users need
  right-click → Open, or System Settings → Privacy & Security → "Open
  Anyway"); Windows SmartScreen will show a "Windows protected your PC"
  warning on the `.exe` (users need "More info" → "Run anyway"). Code
  signing and notarization are real, separate pieces of future work -
  packaging is now done, signing is not (see `docs/roadmap.md`, split out
  from what used to be one combined "packaging + signing" line item).
- **Two places now hold a version number by hand**: `shared/Version.h`'s
  `CurrentVersion::string` (compiled into the plugin, compared by
  `UpdateChecker` - see [decisions/007](007-update-checker.md)) and
  `project(EarTrainer VERSION ...)` in `CMakeLists.txt` (read by all three
  packaging scripts for the installer filename and, on macOS, the pkg
  version field). Both need bumping together for a release; nothing
  enforces that today.
- Every plugin's install location is a documented, deliberate default
  (`/Library/Audio/Plug-Ins/VST3`, `{commoncf}\VST3`, `$HOME/.vst3`,
  etc.) matching each OS's real DAW-scanned conventions - not guessed.
- The macOS free-text-custom-path gap and the "open folder" DMG-helper
  substitution are both deliberate, documented trims, not oversights -
  same spirit as every other scope decision recorded in this project's
  ADRs (see [decisions/005](005-microlesson-architecture.md)'s
  per-control-highlighting cut for the precedent).
