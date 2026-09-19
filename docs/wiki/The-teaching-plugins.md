# The teaching plugins

*[По-русски](ru-The-teaching-plugins)*

> In a plugin browser they are **ABC Learner EQ**, **ABC Learner Comp** and
> **ABC Learner Verb**, by *soundkorb*. Everything sorts under ABC so the
> four arrive together rather than scattered across the alphabet. This page
> uses the short names in prose.

Three real effects that process your host's audio and explain themselves
while doing it. Every parameter is host-automatable and saves with the
session, like any other plugin.

| | What it is |
|---|---|
| **ABC Learner EQ** | A graphical EQ: up to eight bands of any type - bell, low/high shelf, high-pass, low-pass, notch - added and removed on the curve itself. Double-click empty space to add a node, double-click a node to remove it, drag to move, scroll for Q. The spectrum is labelled in *sensations* as well as numbers - Sub, Bass, Boom, Body, Honk, Presence, Sibilance, Air - with a line under the pointer saying what too much of that zone does. |
| **ABC Learner Comp** | A soft-knee compressor. The top of the window is its **transfer curve** - level in against level out, drawn by the same formula the audio goes through, with a dot at your signal's level - beside a waveform that tints where it is working, and a gain-reduction bar that grows from 0 dB, because that is the direction the sound goes. |
| **ABC Learner Verb** | Room and hall (a feedback delay network), plate (Dattorro's) and spring. **Decay is seconds**: the reverb's measured RT60 matches the knob. The top of the window is the **echogram** - what the reverb does to one click: the dry hit, the gap the pre-delay leaves, the first reflections, the tail, and the tail's length measured off it. Under the TYPE bar, one line says what that type is. |

**A / B** in the title row holds two settings one click apart: set A, press
B (it starts as a copy), change one thing, switch back. Both are saved with
your project. Bypass belongs to neither slot, so flipping A/B never switches
the plugin off.

The plugins speak the same language as Ear Trainer - all twelve - and use
its theme.

## Practice audio

Open one outside a DAW and it would be silent - so the title row has a
**source** selector. Off by default (a plugin that starts injecting audio
into a session on its own is a bug), and it plays from the same library Ear
Trainer imports your music into. See
[Training on your own audio](Training-on-your-own-audio).

## Training modules

Learner Comp and Learner Verb have seven each, one per control; Learner EQ
has four (frequency, gain, Q, high-pass). Open them with the checklist icon
in the title row. Each card shows the threshold you have reached in that
knob's units, or *Not tried*.

A module is four steps:

1. **Watch.** The plugin sets the knob for you and explains what changed,
   on a sound the control is actually audible on. Attack does nothing you
   can hear on a sustained pad; pre-delay disappears inside a busy loop.
   Choosing the material *is* the teaching.
2. **Try.** A goal in words. The knobs stay live - the panel covers the
   analysis section only.
3. **Check.** The plugin sets the control to a value it does not show you
   and plays it. You switch between **Reference** and **Mine** and turn the
   **plugin's own knob** until they match. The knob never shows the
   reference: it reaches the audio past the parameter.
4. **Result.** What it was, what you said, how far out you were and how
   wide the band was - in the knob's units - and what the staircase did.

### A staircase per knob

The same rule as the trainer: **three passes in a row take you one step
up, one miss one step down**, ten steps, and your record never drops. The
band narrows with every step, so the step you settle on is your real
precision for that control.

### The tolerance is per-control, and that is the point

| Control | Graded in |
|---|---|
| attack, release, pre-delay, decay | **percent of the value** |
| threshold, makeup, band gain | **dB** |
| ratio | **ratio** |
| frequency | **octaves** |
| damping, size, mix, width | **percent of the range** |

Being 5 ms out on a 3 ms attack and on a 300 ms attack are completely
different mistakes, so attack is graded as a proportion. The band on the
check scale is drawn exactly as wide as what would pass.

### The sounds are generated, not sampled

A fixed set of files is a fixed set of answers: play the same kick twenty
times and you stop hearing a kick and start recognising a recording. A kick
here is a starting pitch, a pitch decay, an amplitude decay, a click amount
and a drive, each drawn fresh per hit.

## Walkthroughs

Under the modules are each plugin's four lessons as numbered cards: two that
walk a whole workflow rather than one control, and two that explain a
single idea - high-pass and low-pass in the EQ, attack and release in the
compressor, pre-delay and size-versus-damping in the reverb. They have no
check; they set the knobs step by step while you listen.

## Learner EQ has modules now

It did not at first: a module that teaches you to turn a frequency knob
competes with excellent EQs that already exist. What changed the call is
that a check here is not a knob drill - it is "which frequency was that",
"how many dB", "how narrow", graded in octaves and dB on your own band 1,
with the knob never showing the answer. What a beginner is also missing is
**where things live**, and that part is the zone labels on the spectrum.
See [ADR 037](https://github.com/bogggare567/abcTrain/blob/main/docs/decisions/037-the-learner-plugins-made-honest.md).
