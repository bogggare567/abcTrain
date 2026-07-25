# 019. Design tokens, a real light theme, and eased widget state

## Status

Accepted, implemented. Verified by a full local build of all four plugins
plus `EarTrainerTests` (126 test groups, 0 failures) **and** by running the
EarTrainer Standalone app and looking at it - which caught two real bugs
that compiled fine (see Consequences).

## Context

The same "premium UI redesign" brief that produced
[decisions/018](018-ui-polish-and-builtin-samples.md) was raised again,
unchanged. ADR 018 was deliberately a bounded pass and said so. This one
goes after the things 018, 016 and 009 each explicitly deferred, because
they were the substance of the request rather than the trim.

## Decisions

### A token layer separate from the LookAndFeel (`shared/AbcTrainTheme`)

A `LookAndFeel` can only reach widgets JUCE routes through it. Every
custom component in this project - spectrum, waveform, choice slider,
lesson overlay, training-sounds overlay, progress bar - draws itself, and
each had its own copies of `0xff1e1e2e`/`0xff5b9bd5`/`0xff14141a`. Those
literals had already drifted apart by a shade in a couple of places.

`AbcTrainTheme::current()` is now the single source for colour, and
`Spacing`/`Radius`/`Duration`/`Ease` cover the other axes. Nothing outside
the theme names a colour any more.

### The light theme is designed, not inverted

Inverting the dark palette gives glaring white panels and accents with no
contrast left against them. Light mode instead uses a warm off-white
(`#e8e6e1`) as the *page*, with panels and widgets stepping **up** toward
white - mirroring how dark mode steps up from its own base - plus deeper,
slightly desaturated accents (a light ground needs more colour weight to
read at the same strength), softer/wider/cooler shadows (on paper a shadow
is ambient occlusion, not glow), and less than half the noise-texture
strength (light surfaces show grain far more readily).

The mode is stored in the same shared `abcTrain` `PropertiesFile` the
language preference uses, so it's one product-wide choice, not per-plugin.

### Eased per-widget hover/press (`shared/WidgetStateRegistry`)

ADR 018 recorded that a stateless `LookAndFeel` "has nowhere to keep a
per-button animation timeline", so hover/press could only snap. That was a
real constraint, but not an unsolvable one: the registry maps each
`Component` to a set of eased values, ticks them at 60 Hz, and repaints
only the ones still in motion. Entries are `Component::SafePointer`, so a
widget destroyed mid-animation nulls its entry instead of dangling, and
dead entries are pruned on the next tick.

Buttons now lift and settle (shadow grows/collapses, surface sinks 1px
under the press, top-edge light hairline brightens), knobs swell ~3% and
bloom under the pointer. Press uses a shorter duration than release on
purpose - that asymmetry is most of what makes a control feel like it has
mass rather than being a light switch.

### Typography

JUCE exposes no tracking/letter-spacing control on `Font`, `Label` or
`Graphics::drawText`. `drawTrackedText` lays the string out with
`GlyphArrangement` (keeping the font's own kerning) and draws each glyph
through its own translation, which is the only way to set a title wide.
All four editors now draw their title this way rather than via a `Label`.
Numeric readouts stay monospaced so digits don't jitter as values change.

**Not solved:** a licensed custom typeface. There is no font asset to
source here and shipping one has real licensing consequences, so the
system face remains - the request for "no default system fonts" is the one
item of the brief this pass could not honour, and it is not a coding
problem.

### Visualisations

Spectrum: per-bin attack/release smoothing of the *displayed* curve (fast
attack, slow release - the analysis itself is untouched), midpoint-
quadratic smoothing into a continuous outline, a vertical gradient fill
under it, a soft frequency grid with labels, and a bloom under the stroke.
Waveform: the same smoothing, drawn as one closed symmetric envelope with
a gradient fill and thin outline instead of 100 hard columns.

New `shared/GainReductionMeter`: a gradient arc that fills **downward** as
the compressor pulls the signal down, glowing harder the more it works.
The direction matters - gain reduction is the one meter in a mixing chain
where "more is lower", and a reused level meter growing upward would
quietly teach the wrong model to exactly the audience this plugin is for.

### Guide text became a floating card (`shared/GuideTooltip`)

The three Learner plugins each had a permanent text strip under the title.
It's now a card that eases in over the visualisation only while a control
is being dragged, backed by a **real** Gaussian blur
(`juce::ImageConvolutionKernel`, the one true blur primitive JUCE ships) of
whatever is behind it. Blur rather than an opaque card because the text
appears exactly while the user is watching the knob it explains - an
opaque panel there would hide the subject.

It snapshots its *parent*, not itself: `createComponentSnapshot` renders
the target and its children, so snapshotting itself would recurse into the
paint call that asked for it.

### Grouping

All four editors now group controls into captioned section panels
(raised surface, hairline border, wide-tracked uppercase caption) with
data displays in recessed wells (inverse gradient direction, inner
top shadow) - so "controls sit on the surface, displays are cut into it"
is one consistent idea rather than everything floating on the backdrop.

## Consequences

- **Two real bugs were caught only by running the app**, not by the
  compiler or the tests:
  1. The choice slider's new track gradient ran from a dark tone to the
     panel colour, which made the groove effectively invisible against the
     section panel it now sits on. Replaced with a flat dark well plus a
     bright hairline along the *lower* inner edge only (light falling into
     a real groove catches the far wall).
  2. The exercise section was 16px too short for the two-line instruction
     text, silently truncating it with an ellipsis, and the window was
     ~30px taller than its content, leaving dead space above the footer.
  This is the fourth ADR in a row where actually launching the binary
  found something reading the code did not.
- Every editor grew slightly to pay for the section padding.
- `AbcTrainLookAndFeel` is no longer copyable-by-accident: it owns a
  `WidgetStateRegistry` with a running timer, one per editor as before.

## Still not done

A licensed typeface (above); icon morphing between states; `FlexBox`
layout; gradient fills under the *response curve* specifically; a
documented design-token styleguide document (the tokens exist in code,
the prose styleguide does not); and any navigation/IA rework - the
editors are still one flat screen each.
