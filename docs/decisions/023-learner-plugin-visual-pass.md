# 023 — A colour per plugin, and a way to actually look at them

Status: accepted.

The three Learner plugins had never had the pass EarTrainer got in
ADR 019/022. They shared one accent, one neutral backdrop and three wide
text buttons per title row, and — as it turned out — six real bugs that
nobody had ever seen, because nobody had ever *looked*.

## The tool that comes first

`tools/EditorSnapshots.cpp`, a console target that constructs each real
editor with no plugin host, calls `resized()`, snapshots it at 2× and
writes a PNG — in **both themes**, by driving the same persisted
preference the editors read and putting it back afterwards.

```bash
cmake --build build --target EditorSnapshots
./build/EditorSnapshots_artefacts/EditorSnapshots ~/shots
```

It asserts nothing. A golden-file comparison would fail on every
legitimate design change and on every machine whose font rendering
differs by a hair, and would be deleted within a month. This is a contact
sheet: run it, open the folder, look.

Nor does it pump a message loop, deliberately — no `Timer` fires, so
every eased value (hover, the bypass veil, the guide card's rise) is
captured at rest. That makes the output reproducible instead of dependent
on how long the process happened to take.

EarTrainer's editor is deliberately excluded: it owns a `ProgressManager`
writing to the real per-user properties file, and rendering it would
touch the player's saved progress.

**Six bugs in the first run.** All six compiled. All 172 test groups
passed with every one of them present. Every one was obvious in the first
picture:

1. **Every knob in all three plugins drew its value arc in amber**,
   including LearnerEQ, whose whole identity is blue. `LookAndFeel_V4`
   maps `Slider::rotarySliderFillColourId` from the colour scheme's
   *highlightedFill* slot, which this project fills with `accentWarm` —
   the "you are touching this" colour — not with the accent. The family
   colour reached the spectrum, the waveform, the icon and the backdrop,
   and stopped exactly at the controls.
2. **LearnerComp's gain-reduction meter was drawing as a token circle.**
   Its analysis section was 390px; the rows inside it ask for 404.
   `removeFromTop` clamps to the height available rather than
   overflowing, so the meter row silently got 32px instead of 46 and
   nothing ever reported it.
3. **The lesson dropdown was blank** in all three title rows. Nothing is
   selected until a lesson is picked, and the combo box had no
   "nothing selected" text. It also never reset on close, so the same
   lesson could not be started twice.
4. **In light mode the spectrum and waveform were the brightest things on
   screen.** `displayBackground` was lighter than `panelBackground`, so a
   well meant to read as *cut into* the surface read as a raised white
   plate — with a 1px curve expected to carry itself against it.
5. **LearnerComp had 132px of dead window** below its controls, and
   LearnerVerb 58px. Both had window heights that were guessed rather
   than derived from what `resized()` actually lays out.
6. **LearnerEQ showed three identical unlabelled knobs per band.**
   Nothing said which was frequency, which was gain and which was Q — and
   the numbers underneath don't answer it, since `0.70` could be either a
   Q or a gain.

That is the argument for the tool, not a list of regressions from this
pass: five of the six predate it.

## A colour per plugin

`AbcTrainTheme::accentFor (Family)` is now the single definition of the
four skill-family colours — frequency blue, dynamics amber, space green,
character violet. EarTrainer's home screen already grouped exercises by
these families and tinted its training backdrop with them, but it did so
from four hex literals sitting in a helper inside its own
`PluginEditor.cpp`, which is precisely the drift `AbcTrainTheme` exists
to prevent.

Each Learner plugin is squarely one family, so each takes its family's
colour: through its `LookAndFeel` (a new `refreshFromTheme (accent)`
overload), its spectrum, its waveform, its icon and its backdrop tint.
The trainer exercise and the plugin that teaches the same skill now look
like the same subject.

The accent is **per-editor, not global.** `AbcTrainTheme::current()` is
deliberately process-wide so a host with two plugins open can't show one
in light mode and the other in dark — which means it cannot also carry a
per-plugin accent. So `SpectrumAnalyzerComponent` and `WaveformDisplay`
each gained a `setAccentColour()` override, and each editor owns its own
`LookAndFeel` instance already.

`GainReductionMeter` deliberately did **not** get one. Its arc runs
accent → accentWarm as the compressor works harder; that gradient is
semantic, not decorative, and overriding one end of it would break what
it says.

## Bypass you can see

Bypass changed nothing on screen. The only way to know whether you were
hearing the plugin was to look at the checkbox you had just clicked.

Now the analysis section veils over ~260ms — desaturating rather than
hiding, because you still want to watch the signal go past, you just need
to be able to tell at a glance that nothing is being done to it. Drawn in
`paintOverChildren` (the displays are child components, so `paint` runs
underneath them) and eased on each editor's existing 30 Hz timer rather
than a second one.

## Presets that say why

Both `CompressorGuide::Preset` and `ReverbGuide::Preset` gained a `what`
field — one sentence on what the setting is *for* — shown in the floating
guide card when the preset is applied. A preset that silently moves seven
knobs teaches nothing; the knobs moving is the "what changed", this is
the "why". `GuideTooltip::setText` gained an `autoDismissMs` so a message
with no gesture behind it takes itself away instead of sitting over the
visualisation forever.

## Smaller things, same pass

- Theme and Updates are `IconButton`s (30px) instead of 62px and 76px of
  text — the treatment EarTrainer's title row already had. The update
  *outcome* moved to the guide card, which keeps ADR 014's rule that
  every click gets a visible result while giving it more room than a
  30px square ever had.
- `createSliderTextBox` is overridden to use the mono face, so a knob
  sweeping 9.8 / 10.0 / 10.2 doesn't make the number jiggle sideways as
  glyph widths change; the well and border are gone, because eighteen
  little bordered fields made the values read as inputs rather than as
  part of the knobs.
  The order matters and cost one wrong attempt: `setTextBoxStyle` has to
  be called **after** `addAndMakeVisible`, or the slider has no parent,
  `getLookAndFeel()` resolves to JUCE's default, and the call changes
  nothing at all.
