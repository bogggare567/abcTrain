# 018. Bounded UI polish pass, and built-in synthesized training samples

## Status

Accepted, implemented. Verified by a full local build of all four
plugins plus `EarTrainerTests` (126 test groups, 0 failures). Live
visual verification of the UI polish in a running Standalone app was
not possible this pass - the local machine's screen was locked (a real
OS session lock, `CGSSessionScreenIsLocked`) for the rest of the
session, and entering a password to unlock it is out of scope for this
agent regardless. The code compiles and follows patterns (`DropShadow`,
`ColourGradient`, `juce::Animator`) already proven working elsewhere in
this project this session (ChoiceSliderComponent's existing drag-glow,
`LevelProgressBar`'s existing eased-fill animator).

## Context

Two requests arrived together:

1. A "premium UI redesign" brief asking for FabFilter/ValhallaDSP/Ozone/
   Soundtoys/Ableton-level visual polish - gradients, shadows, glow,
   "physical" hover/press feedback, blur-in tooltips, icon morphing - and
   permission to adopt `gin`/`foleys_gui_magic`/`juce_animate` "if they
   genuinely improve the visual".
2. A request to replace `PinkNoiseGenerator` with real audio samples in
   all 9 games, pointed at a local folder outside the repository to read
   samples from.

**Neither request was implemented literally as asked, for two different, both important, reasons - documented here rather than silently narrowed.**

### Why the UI pass is "meaningful polish," not "FabFilter-level"

`gin` was already declined in
[decisions/013](013-ui-libraries.md) (every module requires C++20, not
worth a project-wide language-standard bump); `juce_animate` doesn't
exist (also ADR 013); `foleys_gui_magic` remains metadata-checked only,
no prototype attempted - none of that changed this session, so this
pass stayed within pure JUCE and the existing `AbcTrainLookAndFeel`,
consistent with [decisions/009](009-look-and-feel.md) and
[decisions/016](016-icons-and-site-link.md)'s standing position that a
true professional design system is a real design job (Figma, a hired
designer, licensed assets) this codebase cannot produce unsupervised.
What *is* real and shipped this pass:

- `AbcTrainLookAndFeel::drawButtonBackground` - a soft top-to-bottom
  gradient instead of a flat fill, plus a `juce::DropShadow` that grows
  on hover and shrinks on press (immediate, not eased - a shared,
  stateless `LookAndFeel` has no per-button animation state to interpolate
  from, so this responds to JUCE's highlighted/down flags directly rather
  than tracking a fake animation timeline per button).
- `AbcTrainLookAndFeel::drawRotarySlider` - a soft glow behind the value
  arc while a knob is being touched (drawn as progressively wider,
  fainter copies of the same arc - the standard cheap fake-blur trick,
  not a real blur filter), and a gradient-shaded, drop-shadowed knob cap
  instead of a flat disc.
- `AbcTrainLookAndFeel::paintPanelBackground` - a new shared helper (a
  gentle radial gradient instead of one flat colour) that all four
  editors' `paint()` now call instead of `g.fillAll()`.
- `ChoiceSliderComponent` - a correct answer now fades a soft glow out
  around the thumb over ~900ms (`juce::Animator`, ease-out); a wrong
  answer gives the thumb and big label a brief, decaying wobble (a
  shrinking sine, ~450ms, three quick passes) instead of a flat colour
  swap - both driven by the same `juce_animation` module already used
  for the level progress bar.
- `EarTrainerEditor::LevelProgressBar` - now "breathes": a slow (~3.4s
  cycle), low-amplitude glow pulse at the leading edge of the fill, via
  a plain 30Hz `Timer`, independent of the existing eased fill-transition
  `Animator`.

Not built (still real design work, still deferred, same as ADR 009/016):
a light theme, hover/press *state interpolation* per-widget (as opposed
to the immediate shadow response above), gradient fills under the
spectrum/waveform curves themselves, pill-shaped tooltip backgrounds,
`FlexBox` layout, a licensed custom typeface (no font asset was available
to source in this environment - the existing system-font-based
`titleFont()`/`bodyFontHeight` setup is unchanged), icon "morphing"
transitions, or a blur-in effect for the guide-label tooltips (JUCE has
no built-in blur filter; a real Gaussian blur would need a custom image
effect, not attempted here).

### Why the sample pipeline uses synthesized audio, not the pointed-at folder

That folder turned out to contain complete "name your price" commercial
album downloads - real labels and artists, full tracks, cover art. "Name
your price" is a personal-purchase payment model, not a redistribution
licence - reading, copying, or embedding that material
into this repository (which is what "replace the noise generator with
these files" would have meant) would mean shipping someone else's
copyrighted commercial music inside a piece of software distributed to
other users. That file content was never read or copied anywhere.

What was built instead, reusing the opt-in reference-audio architecture
from [decisions/015](015-choice-slider-and-training-sounds.md) rather
than replacing any game's default:

- Five short, originally-synthesized WAV files (`assets/samples/` - a
  pitch-drop kick, a noise-and-tone snare, a soft A-minor-ish pad, a
  decaying plucked tone, and a two-partial sustained tone; see
  `make_samples.py`'s synthesis for exactly how each was generated) are
  embedded via a second `juce_add_binary_data` target (`SampleData`,
  given an explicit `NAMESPACE`/`HEADER_NAME` distinct from `I18nData`'s
  default `BinaryData`/`BinaryData.h`, since both are linked into the
  same target and would otherwise collide).
- `ReferenceAudioLibrary::addBuiltInCategories()` re-materialises those
  five embedded resources as real files under a cache directory (so the
  existing File-based `selectFile()` path needs no changes), and injects
  them as two categories - "Built-in Percussive" (Kick, Snare) and
  "Built-in Sustained" (Pad, Pluck, Tone) - ahead of anything scanned
  from the user's own folder, every time `rescan()` runs. Since
  `TrainingSoundsComponent`'s unlock rule is "category index <
  maxLevelReached" and level starts at 1, the first built-in category is
  always unlocked - there's a real, non-noise training option available
  immediately, with zero setup.
- `TrainingSoundsComponent` gained a "Choose Folder..." button
  (`juce::FileChooser::launchAsync`, directory-select mode) calling
  `ReferenceAudioLibrary::setRootFolder()` - the previously-missing piece
  from decisions/015's roadmap note ("no file-chooser UI for a custom
  root folder yet"). This is the legitimate way for the user to point
  the plugin at their *own* folder (including, if they choose, the
  `audio/` folder from their own personal collection) - the plugin still
  never fetches, bundles, or vets anything itself; it only reads whatever
  real files already exist wherever the user points it.

`PinkNoiseGenerator`/`TestSignalGenerator` remain the *default* signal
path for all 9 games, unchanged. This is a deliberate choice, not an
oversight: pink noise's flat spectrum is what makes `EQGame` and
`FrequencyRangeGame` fair tests of a boost/cut at *any* frequency, and
`StereoWidthGame` needs two independently-decorrelated noise sources a
single sample file structurally cannot provide (see CLAUDE.md's existing
architecture notes on both games). A short synthesized pad or pluck also
has no real energy in, say, the 6-16kHz "air" band the way broadband
noise or real music does - selecting a built-in (or user-supplied) sample
for `EQGame` specifically will make some high-frequency questions less
answerable, a real and now-documented limitation of *any* narrow-band
source used there, not specific to the synthesized samples.

## Consequences

- Two new tests replace assumptions the built-in categories broke: the
  "no folder configured -> empty" test became "-> the two built-in
  categories"; the filesystem-scan test's expected category count grew
  by two. A new test confirms the built-in categories' names, file
  counts, and that selecting from them actually produces a non-empty
  active buffer.
- `assets/samples/` adds ~440KB of embedded audio to every plugin binary
  that links `SampleData` (currently EarTrainer + `EarTrainerTests`
  only) - a deliberately small, bounded cost for having a real always-on
  training option.
- Not built: any UI change to let a player pick a *specific* file within
  a category (still a random pick each time, same limitation
  decisions/015 already noted) or to preview/audition a category before
  selecting it.
