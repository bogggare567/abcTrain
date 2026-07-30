# The teaching plugins

> In a plugin browser they are **ABC Learner EQ**, **ABC Learner Comp** and
> **ABC Learner Verb**, by *soundkorb*. Everything sorts under ABC so the
> four arrive together rather than scattered across the alphabet. This page
> uses the short names in prose.

Three real effects that process your host's audio and explain themselves
while doing it. Every parameter is host-automatable and saves with the
session, like any other plugin.

| | What it is |
|---|---|
| **ABC Learner EQ** | Four bands - low shelf, two bells, high shelf - over a live spectrum with the response curve drawn on it. Guide text appears while you turn a frequency knob. |
| **ABC Learner Comp** | A soft-knee compressor with a gain-reduction meter that fills **downward**, because that is the direction the sound goes. |
| **ABC Learner Verb** | Room, hall, plate and spring, with decay, pre-delay, size, damping, mix and width. |

## Practice audio

Open one outside a DAW and it would be silent - so the title row has a
**source** selector. Off by default (a plugin that starts injecting audio
into a session on its own is a bug), and it plays from the same library Ear
Trainer imports your music into. See
[Training on your own audio](Training-on-your-own-audio).

## Training modules

Learner Comp and Learner Verb have seven each, one per control. Open them
with the checklist icon in the title row.

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
4. **Result.** What it was, what you said, and whether that counted.

### The tolerance is per-control, and that is the point

| Control | Graded in |
|---|---|
| attack, release, pre-delay, decay | **percent of the value** |
| threshold, makeup, band gain | **dB** |
| ratio | **ratio** |
| frequency | **octaves** |
| damping, size, mix, width | **percent of the range** |

Being 5 ms out on a 3 ms attack and on a 300 ms attack are completely
different mistakes, so attack is graded as a proportion. Three tiers -
roughly, confidently, precisely - and no more, because one knob does not
need ten.

### The sounds are generated, not sampled

A fixed set of files is a fixed set of answers: play the same kick twenty
times and you stop hearing a kick and start recognising a recording. A kick
here is a starting pitch, a pitch decay, an amplitude decay, a click amount
and a drive, each drawn fresh per hit.

## Walkthroughs

Under the modules, past a divider, are the two multi-knob lessons each
plugin has always had - a whole workflow rather than one control.

## Why Learner EQ has no modules

Deliberately. Graphical EQs with many bands, dynamic processing and better
analysis already exist and are excellent; teaching someone to turn *these*
four knobs competes with them and loses. What a beginner is actually
missing is **where things live** - a kick's fundamental against its beater
click, where a voice's body ends and its harshness begins. That is a map,
not a knob drill, and it is not built yet. See
[ADR 027](https://github.com/bogggare567/abcTrain/blob/main/docs/decisions/027-training-modules.md).
