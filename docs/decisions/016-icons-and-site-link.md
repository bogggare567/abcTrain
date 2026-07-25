# 016. Vector game/plugin icons and a soundkorb.ru site link

## Status

Accepted, implemented, verified by actually running the built Standalone
apps for EarTrainer and LearnerEQ and confirming icons render/update and
the site link is legible - not just reading the code.

## Context

The user shared a full UI-overhaul brief (aimed at hiring a UI designer)
referencing FabFilter/iZotope/Ableton/SoundGym/Syntorial/Melodics as
visual references, asking for a complete design system, Figma mockups,
a professional icon set, and polished animations - then asked directly
for "красивое оформление, все иконки, все подписи" (nice-looking
design, icons for everything, all labels) plus a link to their site,
soundkorb.ru, somewhere in the product (a dedicated page for the plugins
there is planned separately, later).

**Scope actually built here is deliberately narrower than that brief.**
A real design system - a styleguide, professional icon pack, FabFilter-
grade spectrum/meter animations, Figma mockups for a hired designer to
work from - is a genuine design job this codebase can't produce on its
own; there's no Figma access, no icon-asset pipeline, and no way to
render/preview a real visual comp outside of actually compiling and
running the plugin (same limitation ADR 009 already noted for
screenshots/mockups in README.md). What follows is the concrete,
buildable subset of that request: real icons for every game/plugin, and
a working site link - not a substitute for the full design pass.

## Icons: programmatic vector paths, not an asset pipeline

`shared/AppIcons.h/.cpp` defines a `juce::Path` per icon (EQ, Compression,
Reverb, Pan, Delay, Distortion, StereoWidth, Gain, FrequencyRange for the
9 EarTrainer games; LearnerEQ/LearnerComp/LearnerVerb for the three
plugins), normalised to a 24x24 box and scaled to fit via
`Path::scaleToFit()`. Deliberately not SVG/PNG assets: this avoids adding
an asset pipeline (icon font, `juce_add_binary_data` entries, an actual
external icon pack license) for a first pass, at the cost of the icons
being simple stroke-based line art rather than the polished multi-weight
icon sets (Feather/Phosphor) the original brief mentioned as references.

- `AppIcons::iconForGameName()` maps a `Game::getName()` string to its
  icon, mirroring `Source/PluginEditor.cpp`'s existing
  `translateGameName()`/`gameI18nKeys` lookup shape exactly (same
  fallback behaviour: an unrecognised name - e.g. a future 10th game
  added before its icon exists - falls back to the EQ icon rather than
  asserting or crashing).
- `AppIconComponent` is a thin `Component` wrapper for spots that want
  the icon as its own child (EarTrainer's game selector row); `AppIcons::draw()`
  is the free-function form for anywhere that would rather draw inline
  (not used yet, but kept available since some future spot may prefer it
  over adding another child component).
- EarTrainer: an `AppIconComponent` sits to the left of `gameSelector`,
  updated in `refreshFromGameState()` (the same place that already
  re-reads the active game's name/instructions on every switch), so it
  tracks the combo box, a fresh round, or a difficulty-driven change
  alike.
- LearnerEQ/LearnerComp/LearnerVerb: a small `AppIconComponent` sits to
  the left of each editor's `titleLabel`, set once at construction (the
  plugin identity doesn't change at runtime the way the active game
  does).

## soundkorb.ru: a plain link, not a dedicated page

A `juce::HyperlinkButton` pointing at `https://soundkorb.ru` was added to
all four editors' bottom-right corner, styled to match the theme (accent
blue, mono font, no underline-on-hover to match the rest of the UI's flat
button style). Clicking it opens the URL in the system's default browser
via JUCE's own `HyperlinkButton` (no custom network/URL-handling code
needed). The user was explicit that an actual dedicated in-product page
for the plugins on that site is separate, future work - this is just the
pointer that exists today.

**A real bug found by actually running the app**: the link's bounds were
first given only 100px width, and `soundkorb.ru` in the theme's mono font
clipped to `soundkorb.…`. Widened to 130px in all four editors after
seeing the clipped text on screen - the exact same "reserved width didn't
account for the font actually being used" mistake as decisions/015's
tick-label clipping bug, just in a different component.

## What this explicitly does not cover

- A real design system: colour tokens beyond the existing
  `AbcTrainLookAndFeel` scheme, a light theme, documented spacing/
  typography rules, component states (hover/press/disabled) beyond what
  `AbcTrainLookAndFeel` already does per ADR 009.
- Professional/licensed icon assets - these are simple original line-art
  paths, not a Feather/Phosphor-equivalent icon set.
- FabFilter/iZotope-style spectrum/meter visual polish (gradients, glow,
  smooth curve rendering) - `shared/SpectrumAnalyzer`/`WaveformDisplay`
  are unchanged by this pass.
- Figma files or any other hand-off artifact for a hired designer - if
  the user does bring one on, this codebase's existing `AbcTrainLookAndFeel`
  constants and this ADR are the starting reference point, not a
  replacement for that work.
- A dedicated soundkorb.ru page for the plugins themselves - explicitly
  deferred by the user to a later pass.

## Consequences

- No new unit test: icon rendering and the hyperlink are pure `paint()`/
  JUCE-widget behaviour, same "needs an actual running app, not
  `EarTrainerTests`' console harness" reasoning as `ChoiceSliderComponent`
  (decisions/015) and `AbcTrainLookAndFeel` (decisions/009).
- `AppIcons::iconForGameName()`'s string-matching table needs a new entry
  whenever a 10th EarTrainer game is added, same maintenance shape as the
  existing i18n `gameI18nKeys` table - easy to forget, falls back
  silently to the EQ icon rather than failing loudly if missed.
