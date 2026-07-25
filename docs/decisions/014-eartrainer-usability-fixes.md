# 014. EarTrainer usability pass: a real button-collapse bug, Updates UX, colour palette, manual level

## Status

Accepted, implemented, verified by actually running the built Standalone
app and clicking through it - not just reading the code.

## Context

Real user feedback after actually running the app: choice buttons
disappeared and became unclickable in every game, the "Updates" button
gave no visible reaction to a click, the correct/wrong-answer colours
looked "childish," and progress (level) felt like something that only
happened *to* the player, never something they could see clearly or
control. Each is addressed separately below.

## The button-collapse bug (the real, confirmed root cause)

Reproduced by building the actual Standalone app and clicking a choice
button: every choice button across every EarTrainer game silently
shrank to zero size and stopped responding to clicks roughly 200ms after
appearing - both on first launch and on every subsequent game switch.

**Root cause, found by reading JUCE's own `ComponentAnimator` source**:
`EarTrainerEditor::rebuildChoiceButtons()` called
`juce::Desktop::getInstance().getAnimator().fadeIn(button, 200)` on each
freshly-created button *before* `resized()` had ever given it real
bounds. `ComponentAnimator::fadeIn()` snapshots `component->getBounds()`
at the moment it's called as the animation's destination rectangle, and
- critically - `AnimationTask::moveToFinalDestination()` **unconditionally
resets the component to that exact snapshot once the fade completes**.
Since the buttons were still `(0, 0, 0, 0)` at that moment (fresh
`TextButton`s, never laid out), 200ms later every one of them snapped
back to zero size - invisible and unclickable - regardless of whatever
real bounds `resized()` had assigned to them in the meantime.

**Fix**: `rebuildChoiceButtons()` now calls `resized()` itself, right
after creating and adding the buttons and *before* starting any fade, so
the bounds `fadeIn()` snapshots are the real ones. `setSize()` in the
constructor was also moved earlier (before the first
`rebuildChoiceButtons()` call) so that internal `resized()` call has a
correctly-sized editor to lay out against, not a zero-size one.

This is the same class of bug ADR 009 already warned about in spirit
(`AbcTrainLookAndFeel` has no unit test - "verified by an actual local
build" instead) - except this one needed an actual *running, clicked*
build, not just a compiling one, to surface at all. Local `cmake --build`
success and `EarTrainerTests` passing were both green the entire time
this bug existed; nothing in the test suite exercises real
`ComponentAnimator` timing. Worth remembering next time a change touches
animation-plus-layout together.

## "Updates" button: always show *some* outcome

`UpdateChecker::checkForUpdatesAsync`'s callback deliberately never fires
on failure (no internet, rate limiting, or - this repo's actual current
state - zero GitHub releases published yet, see
[decisions/007](007-update-checker.md)). That's the right behavior for
network failures, but it meant clicking "Updates" gave no visible
reaction *at all* in the common case of "nothing to report" - exactly the
confusion reported.

Every "Updates" button (all four editors) now:
1. Disables itself and shows "Checking..." immediately on click.
2. On a real response: shows the existing "update available" prompt, or
   a brief "Up to date" acknowledgement that reverts to "Updates" after
   2.5s.
3. If nothing comes back at all within 6 seconds (comfortably past the
   network call's own 5s connection timeout): shows "Couldn't check",
   then reverts the same way.

A `std::shared_ptr<bool> handled` flag, shared between the real callback
and the 6-second fallback (`juce::Timer::callAfterDelay`), makes sure
only one of the two ever actually updates the button - whichever
resolves first wins, the other is a no-op.

## Colour palette: replacing stock JUCE colours with the theme's own

Found by literally looking at the running app: `Colours::limegreen`
(correct answer) and `Colours::orangered` (wrong answer) are bright,
fully-saturated "web-safe" colours that clash against the deliberately
muted dark palette `AbcTrainLookAndFeel` established in ADR 009 - a real,
visible inconsistency, not just a matter of taste. Same issue with
`Colours::deepskyblue` (spectrum/waveform), `Colours::darkgrey`/`white`/
`lightgrey`/`grey`/`red` scattered across `Source/PluginEditor.cpp`,
`shared/SpectrumAnalyzer.cpp`, and `shared/WaveformDisplay.cpp`.

Replaced with tones from (or matched to) the theme's own scheme:

| Use | Before | After |
|---|---|---|
| Correct answer | `Colours::limegreen` | `#5fbf7d` (muted green) |
| Wrong answer | `Colours::orangered` | `#d9615f` (muted coral, sibling to the theme's `#d98c5f` orange) |
| Default choice button | `Colours::darkgrey` | `#2a2a3a` (theme's `widgetBackground`) |
| Body/feedback text | `Colours::white` | `#e0e0e0` (theme's `defaultText`) |
| Muted/secondary text | `Colours::lightgrey` | `#a0a0b0` (already used elsewhere for hint labels) |
| Spectrum fill/stroke | `Colours::deepskyblue` | `#5b9bd5` (theme's accent blue) |
| Waveform input trace | `Colours::grey` | `#e0e0e0` at low alpha |
| Waveform GR highlight | `deepskyblue` → `red` | `#5b9bd5` → `#d9615f` |

The rotary-knob pointer line's `Colours::white.withAlpha(0.85f)` was left
as-is - a light indicator line on a dark knob is a standard, tasteful
choice (FabFilter-style plugins do the same), not part of the "childish"
complaint.

## Manual level control: progress you can see *and* set

Level (1-10) was previously a pure function of accumulated points -
visible, but not controllable. `ProgressManager::setLevelManually(int)`
now lets a player jump straight to any level; it works by setting
`totalScore` to that level's exact point threshold
(`pointsRequiredForLevel`), so level stays *derived* from score exactly
as before (see `addPoints()`) - there's no second, independent notion of
"level" to ever disagree with the points total. A `levelSelector`
`ComboBox` (items 1-10) sits next to the level label in EarTrainer's
editor; picking one calls `setLevelManually` directly, which also
re-applies difficulty to every game via the existing
`GameManager::setDifficultyForAllGames`, exactly like a real level-up
would.

## Consequences

- All four editors' "Updates" button now behaves identically and
  predictably; EarTrainer's version routes its three new status strings
  ("Checking...", "Up to date", "Couldn't check") through
  `LocalisationManager` (new keys in all 12 language files), the other
  three plugins use plain English (they don't have a language picker
  yet, see [decisions/011](011-i18n.md)).
- The button-collapse bug is a reminder that this project's own testing
  strategy doc (`docs/testing-strategy.md`) already flags real
  `ComponentAnimator`/GUI timing as something no automated test here
  covers - this is a concrete instance of exactly that gap, found only by
  actually running and clicking the built app.
- `docs/roadmap.md`'s "childish/ugly" complaint is addressed for the
  specific colours identified above; a fuller visual pass (gradients,
  shadows, pill tooltips, hover/press animations) is still the separately
  tracked, deliberately deferred scope from ADR 009.
