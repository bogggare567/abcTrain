# 037 — The Learner plugins made honest

**Status:** accepted
**Date:** 2026-09-20

The user asked for the three teaching plugins to be reworked "for the
better": study them first, then plan the route. Later they pointed at their
own redesign mockup (`abcTrain Redesign.html`, frames F1–F4) as the
direction. Studying the plugins before touching them found the same kind of
problem in each: **a claim on screen that the audio did not back up.**

| what the screen said | what was true |
|---|---|
| Verb: *Decay 2.5 s* | Freeverb's `roomSize`, mapped by ear; the measured RT60 was anything from 0.6 to 4 s |
| Verb: *Size* | nothing audible once Decay was set — both drove the same `roomSize` |
| Modules: *tier 3 of 3* | three widths of accept band, no memory of misses, a band drawn at `tolerance * 0.5` of a log scale — about twice as wide as what would actually pass |
| Result: *Passed* | nothing about by how much, in what unit, or what happens next |
| EQ: a module panel | it could not check anything — no override path into the DSP |
| Comp preset *Limiter* | a 10:1 compressor with 1 ms attack; not a limiter |
| Comp/Verb *Mix 1* | a 0–1 knob in a lesson that told you to set "100%" |

Everything below is one of those corrected, or the shell they now share.

## One editor, three subclasses

`shared/LearnerEditorBase` is what Comp, EQ and Verb have in common: the
title row, theme, language, update flow, bypass veil, module panel, guide
tooltip and site link. Each plugin supplies only its analysis section, its
controls and its modules. The three editors had been ~95% the same file,
and every fix had to be made three times — which is how Learner EQ ended up
with a module panel that could not check anything.

The title row now follows the mockup: icon, name, a hairline, the **family**
in its own colour (Frequency / Dynamics / Space), then the practice source,
**A/B**, Bypass, and three icons.

### A/B

`shared/ABCompare.h`: two slots of knob settings. Switching stores the knobs
into the slot being left and loads the one being entered; an empty slot
keeps the current knobs, so B starts as a copy of A and you change one
thing. The slots are a child of the APVTS state tree, so they are **saved
with the host project** like any knob. **Bypass is excluded** — flipping
A/B must never silently switch the plugin off. The buttons are disabled
while a module runs: a module saves every parameter on entry and restores
them on exit, and switching slots in between would hand it the wrong ones.

### The family colour, everywhere

A filled ("primary") button used the palette's accent — the product blue —
so every chosen preset in Learner Verb was blue on a green plugin.
`AbcTrainLookAndFeel` now keeps a per-instance `primaryFill`, set from the
accent the editor passed to `refreshFromTheme`. It is deliberately **not**
`TextButton::buttonOnColourId`: JUCE maps that from the scheme's warm slot,
and the trainer's mode pills rely on it (found by rendering — the first
attempt turned every primary button orange). `SegmentedChoice::setAccent`
does the same for the segmented bars.

## Modules: a staircase, in the knob's own units

The same rule as the trainer (ADR 035), per module: **three passes in a row
step up, one miss steps down**, ten steps, a record that never drops
(`shared/ModuleProgress`). An old tier migrates to `1 + 3·tier` as both the
current step and the record. The accept band narrows geometrically from
`toleranceAtTierOne` to `toleranceAtTopTier` across the ten steps
(`TrainingModule::toleranceForLevel`, the same ramp as the trainer).

`TrainingModule::acceptRange` is the band **in the knob's units**, and the
check scale draws exactly that range — a test asserts that the edges drawn
are the edges that pass. The result speaks units: *off by 3 dB, the band
was ±5.4 dB*, what the staircase did (*step 4 → 5: a narrower band*), and
the three squares toward the next step.

The check now says what to do in one sentence ("the reference plays one
value of Attack; turn the plugin's own Attack knob until yours sounds the
same, then press Submit") — "Match this" on its own left people looking for
a second slider. It still does not draw a second slider or the reference's
curve: the mockup's F3 shows the reference transfer curve in grey, which
would answer the question it asks.

The **shelf is a grid of cards** (four columns at the default width), each
with the name, a line on what the knob does, the ten-step ruler and the
threshold reached — or *Not tried*. Walkthroughs (the old multi-knob
lessons, now checkless modules) sit under their own caption as numbered
cards with their length. The list it replaced ran off the bottom and hid
the walkthroughs under a scroll nobody found.

**Learner EQ has modules now**: frequency (octaves), gain (dB), Q
(proportion) and high-pass (octaves), each on band 1, checked through the
same override path the other two use (`LearnerEQProcessor::setCheckOverride`
— the knob never moves to the answer).

## Learner Verb: a reverb whose knobs are true

`LearnerVerb/Source/ReverbEngine.h` was rewritten:

- **Room and Hall**: an 8-line feedback delay network (Jot), Householder
  mixing, per-line gains computed from the RT60 so **Decay is seconds**,
  and one-pole absorption filters so damping shortens the top first.
  **Size** scales the delay lengths and the early reflections — it changes
  the sound without changing the length.
- **Plate**: Dattorro's figure-of-eight tank.
- **Spring**: dispersive allpass loops, two springs.
- Switching type crossfades through silence; pre-delay glides.

`tests/LearnerRedesignTest` measures it: the Schroeder-integrated T30 of the
impulse response is within 30% of the Decay knob for Room, Hall and Plate
at 1 and 2.5 s; two Sizes give clearly different early responses and the
same length; the spring rings and decays.

### The echogram instead of the spectrum

`LearnerVerb/Source/EchogramView.h` draws what the reverb does to one
click: the dry hit, the gap the pre-delay leaves, the first reflections,
the tail falling away, and the tail length **measured** off that response
(`ReverbMeasure::rt60`, the same function the test uses). It runs a fresh
engine on the message thread whenever a knob changes, at most every 80 ms.
A live spectrum in a reverb mostly showed the source; the room is in time,
not in frequency. This was on the roadmap since ADR 004 as the
"impulse-response cloud".

The type is a full-width **TYPE** bar at the top of the controls with a
**THIS IS** line under it saying what that type is. It stays in the
controls rather than moving up beside the echogram as in the mockup,
because the type module is answered by that bar, and the module panel
covers the analysis section during a check.

## Learner Comp: the transfer curve

`LearnerComp/Source/TransferCurveView.h` replaces the spectrum with level
in against level out — the one picture that *is* a compressor — drawn from
`CompressorEngine::staticReductionDb`, now a public static used by the DSP
itself, so the bend on screen is the bend in the audio. A dot rides the
line at the current input level. The gain-reduction meter became a
24-segment bar hanging left from 0 dB with a readout and a scale.

Presets carry makeup and mix; *Limiter* is renamed **Peak Catcher**,
because it is not one. Makeup, mix and bypass are smoothed (bypass is a
crossfade), so moving them no longer clicks.

## Learner EQ

Band **chips** (one per active band, and "+") beside the type bar, so the
bands are countable things to pick, not only dots. Frequencies read
**kHz** above 1000, gain carries its sign and unit, and every number uses
the language's decimal mark. The zone names on the spectrum (Sub, Bass,
Boom…) are translated. Filters glide (coefficients every 32 samples) and
reset when a band is switched on, so dragging a node is silent.

## Language

The plugins now read the same product-wide settings file as the trainer
(`LocalisationManager::makeDefaultOptions()` — they had been reading the
language from the practice-library file, where it never was). Every
string, including module text, presets and the parameter guide, is in all
12 languages: 326 new keys. `tools/LearnerStrings` lists every key the
plugins ask for so a missing one shows up as a list rather than as an
English word in a Russian window. The same pass filled a gap in the trainer:
17 answer descriptions (`choice.*`) had only ever existed in English and
Russian.

## What was not done

- The mockup's big knob inside the check panel: you answer with the
  plugin's own knob (ADR 027), and a second one would be a second
  answering mechanic.
- The EQ mockup drops the waveform so the curve takes the whole section;
  here the waveform stays, shorter, because it is the only place the EQ
  shows the signal in time.
- LearnerSat, and golden-file audio regression tests, remain on the
  roadmap.
