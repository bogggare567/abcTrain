# 025 — Slicing imported audio, and what a heuristic may honestly claim

Status: accepted.

The reference-audio feature only pays off if pointing it at a folder of
your own music produces something usable. Left to itself it did not: a
four-minute track is not a training signal. The trainer would loop the
first eight seconds — usually a fade-in — over and over, and the exercise
was worthless.

`Source/AudioSliceAnalyzer` cuts long audio into loop-length clips and
sorts each one by what it is good for.

## What it will not claim

The request was to sort into **drums, instrumental and vocals**. That is
source separation: a machine-learning problem that needs a trained model.
A few spectral measurements cannot do it, and a heuristic that pretended
to would mislabel most real music while sounding confident — which is
worse than not offering the feature, because the player would trust the
folder names.

So it sorts by **measurable character** instead: how transient-heavy a
passage is, where its energy sits, how wide it is. Those are exactly the
properties that decide which exercise a clip is useful for, which is the
question actually being asked. A drum loop lands in `percussive` because
it is full of transients, not because anything here knows what a drum is.
A lead vocal over a sparse arrangement usually lands in `midRange`; the
same vocal over a dense mix lands in `fullRange`, correctly, because that
is what the audio is.

## Two bugs found by testing, not by reading

**A median-only onset threshold collapses on sustained material.** Onsets
were counted as spectral-flux frames above `median × 2.2`. For a held
tone the flux is essentially zero everywhere, so the threshold went to
nothing and floating-point noise read as a stream of onsets — a sustained
note classified as a drum loop. Caught by a unit test asserting that a
1 kHz sine is `midRange`. Fixed by also requiring a real fraction of the
loudest rise in the passage, so "loud compared to what else happens here"
still means something when nothing much happens here.

**Onset density alone calls every record percussive.** Running the
analyser over actual full-length music — locally, reading only — showed
almost every slice of every track coming back `percussive`, because any
mix with a beat has several onsets a second. A category that means "this
contains drums" is useless; it has to mean "this is mostly drums".

The fix is a duty cycle: what fraction of short frames are loud relative
to the passage's own level. A drum loop has silence between the hits and
scores around 0.4–0.6; a finished mix is continuously loud and scores
above 0.9. `percussive` now requires dense onsets **and** real gaps.

This one could not have been found any other way. The synthesized test
signals were all correct before and after — it took real music to show
that the classifier was technically right and practically worthless.

## The clips themselves

`ReferenceAudioLibrary::importAndSlice` writes them, and three details
matter:

- **Cuts are snapped to the quietest point nearby.** A loop that starts
  mid-note clicks on every repeat, and the ear locks onto the click
  instead of the thing being trained.
- **Each clip gets a 10 ms fade at both ends**, for the same reason, since
  snapping can only get so close.
- **Quiet slices are dropped.** Intros, fades and gaps make useless
  training material, and a silent clip in the library is worse than no
  clip.

Source files are never touched, moved or renamed — everything is written
into the library root.

## What was refused

The request also asked for a specific folder of audio to be sliced and
shipped with the product, described as copyright-free. Those are the same
files this project has already declined to bundle twice: three commercial
albums, and earlier in the same conversation, "I bought these tracks".
They had been renamed to `Файл N` with their metadata stripped, and carry
`com.apple.quarantine`.

Buying an album licenses listening, not redistribution, and renaming a
file changes nothing about that. The tool that makes *any* user's own
audio work is the part that was worth building, and it is the part that
shipped.


## A colour note from the same pass

Reviewing both themes side by side turned up one more thing worth
recording. The per-exercise background tint had been raised to a strength
that read well on dark and was matched on light — and 26% of amber mixed
into a warm off-white turns the whole window tan. It reads as a stain, not
as a room.

A light page has far less headroom before a tint becomes the subject of
the screen. Light is 10% now, dark stays at 32%. The general form of the
mistake is worth remembering: **a value tuned on one theme is not a value,
it is a coincidence.**


## Getting it in front of someone

Three changes, all the same idea: the feature is only worth having if
trying it costs nothing.

**Files, not a folder.** A multi-select file picker, the way every other
"add your music" dialog on the machine works. Choosing a folder made the
player answer a question about storage layout before they could find out
whether the feature was any good.

**The app owns the storage.** Clips land in the application data folder,
not in someone's music library — they are *derived* files, hundreds of
eight-second cuts the app made and manages, and littering a music library
with them is rude. Pointing at an existing folder still works; it is just
no longer the way in.

**Slicing runs on its own thread, with a progress bar.** Decoding and
analysing a handful of full-length tracks takes real seconds; a window
that freezes for them reads as a crash. The worker touches only the
library and two atomics, and everything that changes the UI comes back
through `callAsync`. Its destructor waits rather than killing the thread —
the worker may be midway through writing a WAV, and a half-written file
in the library is worse than a moment's delay closing the window.

The result is reported as a breakdown, not a total: "47 clips — 21
percussive, 14 full mix, 12 mid range" tells you whether the material you
imported was any use, which "47 clips" does not.

## Clips rotate

`ReferenceAudioLibrary` now holds the chosen **category**, not one file,
and swaps in a different clip each round. Twenty imported loops were
twenty loops in the library and one loop in practice — whichever came up
first played for the whole session, and eight seconds repeated for an hour
is the fastest way to stop hearing it.

Immediate repeats are avoided explicitly: with a handful of clips a
uniform draw repeats often enough to be noticed, and "it played the same
thing again" reads as the app being stuck rather than as chance.

A test drives twenty advances over five clips and asserts both properties
— that it moves, and that it never repeats twice running — plus the
single-file case, which has nowhere to rotate to and must stay put rather
than clearing the selection.

## One more regression the tests caught

Per-round jitter on the stereo-width exercise could push the narrow
setting's multiplier past zero, which collapses the side signal entirely:
a stereo-width exercise playing mono. Caught by the test that checks left
and right actually differ, which has been there since that game was
written and finally earned its keep.
