# 030. The Learner check speaks the trainer's language

## Status

Accepted. 2026-07-31.

## Context

Two complaints, one root. The Learner plugins crashed, and their checks
were explained in sentences in a product whose entire answer mechanic is
a scale you can read.

### The crash

`PracticeAudioSource` lives in the **processor**. The beds it played lived
in `ModuleScreenComponent`, which belongs to the **editor** — and a host
destroys the editor the moment someone closes the plugin window, while
audio keeps running.

So: the audio thread loads the bed pointer at the top of a block, the
window closes, the panel's `OwnedArray` frees every bed, and the rest of
that block reads freed memory. Clearing the pointer in the panel's
destructor cannot help — the block is already in flight.

ADR 027's fix (keep every bed rather than freeing the old one) closed a
*different* case — the next bed freeing the one being played — and left
this one open, because it never crossed the ownership boundary. The rule
it should have derived: **anything the audio thread can reach must be
owned by the processor, never by a window.**

`PracticeAudioSource::publishOverrideBuffer` takes ownership now. The
panel only publishes or clears a pointer; beds die with the processor,
which the host destroys after it has stopped calling `processBlock`.

`fillBlock` also read the override atomic twice — once for the pointer,
once to ask whether an override existed — so the message thread could
clear it in between and a block could take the bed and then decide it
wanted no audio. An audible stutter at exactly the moment a check ends.

### The check screen

It had a scale: an 8px line that showed **nothing at all while the
reference was playing**, which is most of the exercise. So the screen was
a black strip with the word "Reference" in it and the instruction "turn
the knob" written underneath, plus four identically-weighted grey buttons.

Ear Trainer already solved this. Its answer moment is a recessed well with
real-unit grid marks, your mark on it, an accept band around the mark, and
a value readout riding above it — legible without a sentence.

## Decision

**Port the trainer's vocabulary into the check, rather than inventing a
second one.**

- A **recessed well** with five grid marks in the parameter's own unit.
  A scale with no numbers cannot say what "a bit more" means, which is
  the only thing a knob check has to teach.
- **Your knob drawn live**, during the whole check — including while the
  reference plays. It is still where your knob is, and hiding it was most
  of why the screen looked dead.
- The **accept band around your mark**, its width answering "how close is
  close enough" with no number in it.
- The **value readout rides over the mark**, so the number and the line
  it belongs to are never far apart.
- **Reference/Mine becomes a centred pair** under the scale it compares —
  the same shape and the same place as the trainer's A/B, because it is
  the same act. Submit right, leave left. Four equal buttons made the two
  that matter most look like housekeeping.

## Two bugs this pass found by rendering rather than reading

1. **The readout was clipped to nothing.** It was drawn inside the well's
   clip region, and `reduceClipRegion` *intersects* — the readout sits
   above the well, so the intersection was empty and no text appeared.
   Exactly the mistake the tooltip backdrop made in ADR 029.
2. **`formatValue` printed `26.7496 ms`.** `juce::String (double, 0)`
   does not mean "round to an integer" — it means "shortest
   representation that round-trips". Every module value over ten had been
   printing at full float precision the whole time; there was simply
   nowhere it showed until the check grew a scale with numbers on it.

## Consequences

- One answering vocabulary across all four plugins. A player who has done
  a round in the trainer already knows how to read a Learner check.
- The panel no longer owns anything the audio thread can see, which is a
  rule worth applying to anything added here later.
- `tests/PracticeAudioSourceTest` models the real crash sequence: publish
  a bed, let the caller's buffer go out of scope entirely, then ask for
  audio. Under the old ownership that reads freed memory.
- Learner EQ still has no modules (ADR 027), so this changes Comp and
  Verb only.
