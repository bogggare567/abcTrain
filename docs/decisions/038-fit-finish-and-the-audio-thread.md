# 038 — Fit on any screen, finish the layout, and a rule for the audio thread

**Status:** accepted
**Date:** 2026-09-20

The user tried 1.6.0 by hand and reported five things:
- the smallest window size barely fits on a laptop;
- text overlaps in places;
- the hint in "Guess the Band" shows nothing when pressed;
- the analysers should be smooth and good-looking;
- the half-built splitting of an imported track should be finished.

They also passed on an outside code review (from another assistant). This
ADR covers all six.

## Windows fit the display they open on

`shared/WindowFit.h`. Every editor used to open at its design size, which
was also its floor. That meant 1180 × 880 for the trainer: on a 13-inch
laptop (about 800 points of usable height once the menu bar, dock and title
bar are gone) the smallest setting only just fitted, and nothing smaller
existed.

Now the design size is what a window *prefers*. It opens at whatever part
of that the display under the mouse can show, minus the dock and menu bar
(`userArea`) and a margin for the title bar. It never goes below a floor
each layout is built to work at:
- **trainer:** 940 × 620;
- **Learner plugins:** 820 × 600.

A saved UI scale is capped the same way, and the cap is not written back:
a size chosen on a desktop monitor comes back on that monitor.
`ABC_DESIGN_SIZE` switches fitting off for `tools/EditorSnapshots` and
`tools/ClickMap`, which must always see the design size. `SNAP_SIZE=WxH`
makes EditorSnapshots render every screen at another size. That is how
this whole pass was checked: every screen at 940 × 620 in Russian.

In a short Learner window the **analysis section gives way first**: a
spectrum 180 px tall still reads, a knob too small to grab does not.

## What the small-window renders found

Rendering everything at the floor size found, besides the size itself:

| Where | What | Fix |
|---|---|---|
| Home, focus band | the threshold cut to "Low-m…" | the right half is sized for the button plus 210 px of text, not a fixed 32% |
| Home, list | the last rows off the bottom | the list scrolls, with shorter minimum rows |
| Compression with a hint | the answer cards half under the control bar | the hint panel shrinks first; the answers keep 230 px |
| Settings, Hearing | "85 dB(A)90 dB(A)95…" drawn into itself | composite rows take shares of the row, not fixed pixels; a `SegmentedChoice` label wider than its cell drops its tracking, then its size down to ¾, before it touches a neighbour |
| Learner check | the scale squeezed to nothing, the readout on the buttons | the instruction gives up its lines first (2 → 1 → 0); on a short panel the readout rides inside the scale |
| Learner module shelf | walkthroughs below the fold with nothing saying so | a fade at whichever end has more (the fade is the scrollbar, the same rule as the achievements shelf) |
| Every ruler exercise | "632 Hz", "1.4kHz" in a Russian window | `localiseValueText` puts the game's English axis into the language's units and decimal mark: "632 Гц", "1,4 кГц", on the scale, its marks and the feedback line |
| Learner title row | "ABC Learner Comp" colliding with the controls | drops the "ABC " prefix when the row is short; the prefix is for sorting in a plugin list, and this window is already open |

**Deliberately not changed:**
- Answer names such as *Room*, *Plate* and *Low-mids* stay English in
  every language.
- The English second names under the family headings (FREQUENCY, DYNAMICS)
  stay as well.

Both look like unfinished translation and are not. They are the words
printed on every real plugin, which this exercise exists to teach (the
reasoning is in `translateChoiceLabel`).

## The hint that "showed nothing"

It did show something, and that was the bug. On the four ruler exercises a
hint shades the scale down to a region three accept bands wide on either
side of the answer (offset, so the middle is not the answer).

On *Guess the Band* at step 1 the band is ±1 octave on an 8-octave axis, so
the region was three quarters of the scale. The shading was gentle, and the
button said **"Show the sound"** when no picture of any sound appears on
these exercises. You pressed it, looked for an analyser, and saw the scale
slightly darker at the edges.

The region stays three bands wide. `tests/HintTest` holds that ratio on
purpose: a region barely wider than the band would make landing in it the
same as answering. What changed:
- outside the region is now pushed well back (the page colour at 72%);
- the region is tinted and framed in the exercise colour;
- a tag says **"The answer is in here"**;
- the button says what it will do: **"Narrow the scale"** on a ruler,
  "Show the sound" where a picture really appears.

## Analysers: smooth, and without a data race

`shared/SpectrumAnalyzer`:
- **FFT:** 4096 points, recomputed on every 60 Hz frame over the latest
  samples, so frames overlap by about 95% instead of jumping block to block.
- **Bands:** each display point takes the loudest bin across the band it
  covers (one bin per point aliased the top octaves into a comb), and
  interpolates between bins where it is narrower than one (the bottom
  octaves were a staircase).
- **Tilt:** +3 dB/octave about 1 kHz, so pink noise and a balanced mix draw
  level.
- **Ballistics:** in *time* rather than frames (15 ms attack, 300 ms
  release).
- **Peak line:** faint, holds for a second, then falls at 20 dB/s.
- **Idle:** nothing is repainted while nothing moves.

`shared/WaveformDisplay`:
- 400 columns of 256 samples instead of 100 columns of 33 ms, drained 60
  times a second, so it scrolls a few pixels at a time.
- Each column carries its **RMS** as well as its peak. The RMS is drawn as
  the brighter body inside the peak outline: the body is what you hear as
  loudness, the outline is what the meter sees.
- Level guides at −6 and −18 dBFS.

**The review was right about the threading.** Both displays shared plain
variables between the audio thread and the message thread:
- the spectrum handed a whole FFT block across with a `bool`;
- the waveform accumulated into floats that the timer also read and zeroed.

The code comments called that "visually harmless". It is a data race, which
is undefined behaviour in C++. Both now pass data one way through a
lock-free single-producer/single-consumer FIFO (`juce::AbstractFifo`):
- the audio thread touches only its own accumulators and the FIFO's write
  side;
- the message thread touches only the read side and what it draws;
- the waveform's `reset()` asks the audio thread to drop its half-built
  column through an atomic flag.

`Vectorscope` already used atomics and was left alone.

## Split into stems

`shared/StemSeparator`, wired in as `ReferenceAudioLibrary::importAndSeparateMany`
and a second import button, **"Split into stems…"**. Each track becomes four
stems, and each stem is sliced into loops under its own category.

- **Drums:** the percussive half of a harmonic/percussive split
  (Fitzgerald's median filtering: 0.4 s across time, 250 Hz across
  frequency, soft masks).
- **Bass:** the harmonic half below 180 Hz (an 8th-order curve).
- **Centre (vocal):** the rest of the harmonic half, weighted by how alike L
  and R are over the same 0.4 s (`2·Re(ΣL·R*)/Σ(|L|²+|R|²)`, a smoothstep
  from 0.55 to 0.95). Anti-phase content counts as wide.
- **Sides (wide):** what is left.

The masks sum to one in every bin, so **the four stems add back to the
input** (−138 dB in the test). One complex FFT carries both channels
(L real, R imaginary), and every mask is real and identical for both. A
3-minute stereo track takes about 5 s. Separation reads at most 8 minutes of
a file, because the mix and four stems are in memory at once.

ADR 025 said a heuristic pretending to separate sources "would mislabel
most real music confidently". This one keeps that promise by what it
*claims*:
- it is named after what it measures ("centre", not "vocal", with the
  vocal in brackets as the usual case);
- the tooltip, the wiki and the README say it is not a trained model and
  that a centred synth lands with the vocal;
- plain **Add music** still never separates anything.

It has only been tested on synthetic signals so far.

## A rule for the audio thread

The outside review proposed a written rule and it is adopted. Inside
`processBlock` and anything it calls:
- **no** allocation or free;
- **no** locks, file I/O or `String` building;
- **no** `ValueTree` edits, GUI calls, or waiting on another thread.

Data goes out through atomics or lock-free FIFOs into memory allocated
beforehand.

`tests/RealtimeSafetyTest` checks this rather than promising it. It
replaces the global allocation functions in the test binary and counts
every `new` and `delete` made by the thread inside `processBlock`, and only
while it is inside (a thread-local switch). It runs all three Learner
processors across:
- 5 sample rates (44.1–192 kHz);
- 7 block sizes (32–2048);
- every parameter automated between blocks: EQ bands sweeping their whole
  range and switching type and on/off, the compressor through every knob,
  the reverb switching type;
- Bypass toggled mid-run.

It asserts **zero** allocations, zero frees, no NaN or Inf, and a bounded
output. It also covers a block longer than the host announced.

What it found, and the fixes:
- **EQ allocated on the audio thread.** `EQCoefficients::make` returns a
  reference-counted object built with `new`, and the processor called it
  every 32 samples for every gliding band: **108–252 allocations per 24
  blocks** in the test.
  - `EQCoefficients::makeArray` builds the same filter as six floats on the
    stack (`IIR::ArrayCoefficients`), assigned into the filter's existing
    coefficients.
  - `prepareToPlay` pre-sizes that storage.
  - The test fails again if the old call is put back (checked).
- **Oversized host blocks.** `LearnerEQ`'s dry copy and `LearnerVerb`'s wet
  copy were sized exactly to the announced block, so a host sending more
  would reallocate inside `makeCopyOf`. They now have 8192 samples of
  headroom.
- **`ReverbEngine::setParameters` every block.** It recomputed every delay
  line's gain and absorption filter (`pow`/`exp` per line) each block. It
  now returns early unless something those depend on moved.
- **`PracticeAudioSource` kept every training bed for the processor's
  lifetime**, the accepted cost of not knowing when the audio thread was
  done with one. A long session of modules only grew. It now uses a hazard
  pointer:
  - the audio thread announces the bed it is about to read and confirms it
    is still the published one before touching it (a bounded loop);
  - the message thread frees every bed that is neither published nor
    announced.

  The test publishes twenty beds and checks at most two remain.
- **The compressor.** Its per-sample `log`/`pow` is the ordinary cost of a
  dB-domain detector and stays; its attack and release coefficients are
  computed once per block.

## Not done

- The review's larger restructuring (`shared/dsp/…`, `products/…`) was not
  started, and it advised against starting it before the dependencies are
  mapped. The rule and the test above are the part that pays now.
- `ReferenceAudioLibrary`'s loaded clips still live for the library's
  lifetime; the same hazard-pointer scheme would fit there too.
- No CPU benchmark in CI yet.
