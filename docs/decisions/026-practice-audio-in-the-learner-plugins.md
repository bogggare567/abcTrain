# 026 — Practice audio in the Learner plugins

**Status:** accepted
**Date:** 2026-07-28

## Context

Open Learner EQ as a standalone app and it is silent. Every knob works,
the spectrum is empty, the response curve moves and nothing happens. The
plugin whose job is to teach you what an EQ does had nothing to do it to.

Inside a DAW that is correct — the track is the audio. Outside one it
made the teaching half of the product unusable to anyone who had not
already set up a session, which is precisely the person it is for. It
also broke one of the three advantages [the journey
doc](../user-journey.md) says every feature should pay into: *it trains
on your own material* was true in one of the four plugins.

Meanwhile Ear Trainer already had the whole apparatus: a library, an
importer that slices your music into loops and sorts them by character
(see [025](025-audio-slicing.md)), and per-user persistence of what you
chose.

## Decision

Play from the same library, in all three Learner plugins.

**`shared/PracticeAudioSource.h`** — a real-time-safe looping player over
whatever clip `ReferenceAudioLibrary` currently has active. It runs at the
very top of each `processBlock`, before anything else touches the buffer,
so every meter, curve and knob downstream behaves exactly as it would on a
real track. Nothing else in those processors needed to change.

**`shared/PracticeSourceSelector.{h,cpp}`** — one title-row control for
all three, rather than the same forty lines pasted into each editor. The
Bypass and Updates buttons are deliberately duplicated across editors,
because each is two lines of wiring; this one carries real behaviour
(scan, persist, restore, apply at the right sample rate) and three copies
of it would drift.

**Five files moved from `Source/` to `shared/`** —
`ReferenceAudioLibrary`, `AudioSliceAnalyzer`, `TestSignalGenerator`,
`PinkNoiseGenerator` and their headers. `Source/` is Ear Trainer's; four
plugins now use these.

### Off by default, always

A plugin that starts injecting audio into a session on its own is a bug,
however useful the audio is. "Host audio" is item one and the default, and
nothing plays until someone picks something else.

### Product-wide preference, not APVTS state

Which clip you practise on is a property of the person, not of the host
session, so it lives in the shared `abcTrain` `PropertiesFile` alongside
theme and language — not in the `AudioProcessorValueTreeState`. It should
not travel inside a saved project and reappear on someone else's machine.
It is restored **by category name rather than by index**: the list is
rebuilt from whatever folders exist, so an index saved last week can point
at a different category this week.

### No importing from here

The import flow is a file picker, a background thread, a progress bar and
a slicer. Duplicating it into three plugins would be three more places for
it to go wrong. These offer what already exists; Ear Trainer is where
material gets added.

## Consequences

- Each Learner processor now constructs a `PropertiesFile` and a
  `ReferenceAudioLibrary`. That is real (small) file I/O at plugin
  instantiation, on the message thread — the same cost Ear Trainer's
  processor already pays.
- `prepareToPlay` loads the selected clip at the host's rate, which is
  file I/O on whatever thread the host calls it from. Same precedent as
  `GameManager::prepare`; both are documented as message-thread-only work
  that hosts do not call from the audio thread.
- Enabling and disabling crossfade over ~30 ms rather than switching on a
  block boundary. A hard switch into a waveform mid-cycle is a click, and
  a plugin that clicks when you press its own button reads as broken.
  `tests/PracticeAudioSourceTest.cpp` checks the off state is *exactly*
  at rest after the fade rather than nearly — a gain that never quite
  settles would leave the host's audio permanently scaled by 0.999.
- Not tested: that the audio the source produces is the clip. That needs
  a real loaded buffer, which needs a real file and a real load, and the
  thing worth protecting here is the passthrough contract, not the copy.
