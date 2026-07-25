# 015. Drag-to-select choice slider, and opt-in reference-audio training

## Status

Accepted, implemented, verified by actually running the built Standalone
app and clicking through several games plus the new Training Sounds
screen - not just reading the code.

## Context

Real user feedback, with reference screenshots from a commercial
ear-training app: the row of separate choice buttons read as "childish,"
and the user specifically wanted a tick-marked drag slider instead - one
widget shape for numeric answers (dB/Hz/pan) and named/categorical ones
(reverb type, compression strength) alike. Separately, the same user
said they intended to add their own real-world reference tracks
(organized into genre folders) so the games wouldn't only ever use
synthesized noise, and was explicit that those files "won't be legal"
("не будут легальные").

Two independent pieces of work follow from that; each is covered below.

## Choice slider: one widget replaces every game's button row

`Source/ChoiceSliderComponent.h/.cpp` is a single persistent `Component`:
a horizontal track with one evenly-spaced tick per choice, a big label
above showing whichever choice is currently highlighted, and a draggable
thumb that snaps to the nearest tick on `mouseDown`/`mouseDrag`, firing
`onChoiceSelected(index)` on `mouseUp`. It needs nothing from a `Game`
beyond what already existed - `getNumChoices()`/`getChoiceLabel(int)` -
so it works unmodified for every one of the 9 games, whether the labels
are numbers ("100 Hz") or names ("Room"). `Source/PluginEditor.cpp`'s
`rebuildChoiceSlider()` (renamed from `rebuildChoiceButtons()`) just
calls `choiceSlider.setChoices(labels)` instead of destroying and
recreating N `TextButton`s.

**This also sidesteps decisions/014's fadeIn-collapse bug class
entirely**, not just fixes an instance of it: that bug existed because
buttons were destroyed and recreated (needing a fresh fade-in, before
`resized()` had run) every time the choice set changed. With one
persistent component whose bounds `resized()` already assigned,
`setChoices()` only ever repaints - there's no destroy/recreate-before-
layout ordering hazard left to reintroduce.

**Two real bugs were found only by actually running the app**, not by
reading the layout math:

1. Tick labels are drawn centred on their own tick position, up to 80px
   wide. With the track spanning the component's full width, the
   first/last tick sat flush against the component's edge, so half of
   "100 Hz" or "Spring" always clipped off - drawing outside a
   component's own bounds is clipped by the `Graphics` context JUCE
   already provides to `paint()`. Fixed by insetting the track 40px on
   each side (`getTrackArea()`), giving edge labels room to overhang
   without clipping.
2. `paint()` had its own separate, inline recomputation of the track
   rectangle instead of calling `getTrackArea()` - so the inset above
   was added to the mouse-handling helper but silently had *no visible
   effect*, since painting used the old unpadded math. Rebuilding and
   re-running the app after the "fix" showed identical clipped labels,
   which is what surfaced this - a reminder that duplicated layout logic
   in one component is exactly the kind of thing that quietly drifts out
   of sync with itself.

## Reference-audio training: infrastructure only, no content

`Source/ReferenceAudioLibrary.h/.cpp` scans a root folder (default: the
user's own music folder + `/ABCTrain`, matching what the user described
setting up themselves) for one subfolder per category, each holding
audio files the *user* supplies. `selectFile()` loads one (message-
thread only - real file I/O), downmixes to mono, resamples to the
processing sample rate with `juce::LagrangeInterpolator` if needed, and
publishes it via `std::atomic<const AudioBuffer<float>*>` - the same
"raw atomic pointer, null-checked" pattern LearnerEQ's processor already
uses for its spectrum/waveform registration (see CLAUDE.md). Every loaded
buffer is kept alive for the plugin instance's whole lifetime rather than
freed on the next selection, since freeing one could race with the audio
thread still mid-read of whatever the pointer currently points at - a
few `~20s` mono buffers is an accepted, documented memory tradeoff for a
simple, correct first pass, not an oversight.

`Source/TestSignalGenerator.h` is a drop-in replacement for
`PinkNoiseGenerator` - identical `nextSample()` shape - that plays the
library's active buffer on loop when one is selected, falling back to
real pink noise otherwise. 8 of the 9 games (all but `StereoWidthGame`,
which needs two *independently decorrelated* sources for its mid/side
processing - a real recorded file can't provide that, so it deliberately
keeps a plain `PinkNoiseGenerator`) swapped their `PinkNoiseGenerator
noise` member to this type, a purely mechanical change since the call
site (`noise.nextSample()`) didn't change. `Game::setReferenceAudioLibrary`
is a new **non-pure-virtual** method on the interface (default no-op),
so every existing and future `Game` keeps working unmodified unless it
opts in - `GameManager` wires the one shared `ReferenceAudioLibrary` into
every registered game right after construction.

`Source/TrainingSoundsComponent.h/.cpp` is the "Choose Training Sounds"
overlay (same full-size show/hide shape as `shared/LessonController`): a
"Pink Noise (default)" button, one button per detected category (locked/
unlocked by `ProgressManager::getMaxLevelReached()` - a simple `i <
maxLevelReached` first-pass rule, not a hand-curated unlock plan, since
these categories are whatever the user's own folder happens to contain,
not fixed shipped content), and a status label showing what's currently
active. **A third bug found only by running the app**: this overlay was
originally added as a child component *before* `choiceSlider`, so JUCE's
paint-in-addition-order rule meant the slider painted on top of the
"closed" overlay - reordering so `addChildComponent(trainingSounds)` is
the last child added (mirroring `LessonController`'s own integration
order) fixed it.

**This class never fetches, bundles, or verifies the legality of any
audio file.** It only ever reads whatever the user has already placed on
their own disk. The user explicitly said the tracks they plan to add
"won't be legal" - this project does not source, download, or ship any
such content, and provides no feature to do so; whatever a user's own
folder contains is entirely their own responsibility, exactly like
pointing any other local audio tool at a personal library.

## Consequences

- `tests/ReferenceAudioLibraryTest.cpp` covers folder scanning (real
  audio files only), `selectFile`/`clearSelection`, resampling, a
  non-audio file failing without disturbing an existing selection, and
  root-folder/selection persistence across reconstruction - all against
  real `juce::WavAudioFormat`-written test files, not fakes.
- `ChoiceSliderComponent` itself has no dedicated unit test (same
  reasoning as `AbcTrainLookAndFeel`/`SpectrumAnalyzerComponent` in prior
  ADRs - real mouse/paint behavior needs an actual running app, not
  `EarTrainerTests`' plain console harness); the folder-scan/file-load
  logic it depends on indirectly (`ReferenceAudioLibrary`) is what's
  actually unit-tested.
- Not built in this pass: a way to preview/audition a category's audio
  before selecting it, a way to pick which specific file within a
  category (currently a random pick each time), Windows/Linux default
  root-folder conventions beyond `juce::File::userMusicDirectory` (which
  is cross-platform already, just not manually verified on those OSes),
  and any UI to browse to a custom root folder from within the plugin
  (currently only the persisted default, settable only via
  `setRootFolder()` - no file-chooser button yet).
