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

---

## Addendum: Learner EQ becomes graphical, and gains pass filters

ADR 027 gave Learner EQ no knob modules, on the grounds that graphical EQs
with many bands already exist and what an EQ actually teaches is *where* a
problem lives. That reasoning was right and the plugin did not follow it:
it was four fixed bands and twelve rotary knobs, which is the opposite of
pointing at a place.

- **Eight free bands, any type**, added and removed on the curve itself.
  Type is a property of the band rather than of its slot, so a shelf can
  live anywhere. **High-pass, low-pass and notch exist at last** - between
  them, most of what anyone reaches for first.
- Eight rather than unlimited because every band is a set of APVTS
  parameters that must exist up front to stay host-automatable and to save
  with the session. Past eight, a *teaching* EQ stops being a lesson.
- **One row of controls follows the selection** instead of one row per
  band. No APVTS attachment: an attachment binds to one parameter for
  life, and the selection moves. Values are pushed in on the timer and
  written back through the parameter, which keeps automation and undo
  honest.
- **The spectrum is labelled in sensations** (`FrequencyZones.h`): Sub,
  Bass, Boom, Body, Honk, Presence, Sibilance, Air, with a line under the
  pointer saying what too much of that zone does. "Cut 300 Hz" is an
  instruction you can follow without learning anything; the map of where a
  mix gets boxy is the thing a mixer actually carries. Boundaries are the
  conventional teaching ones, shared with `FrequencyRangeGame`, and the
  zones can be switched off.
- **Two new lessons the plugin previously could not teach**: what a
  high-pass costs (the resonant bump of a high-Q corner, why phase
  rotation is inaudible solo'd and audible in the sum, why "brickwall" is
  a description rather than a goal) and what a low-pass is actually for
  (housekeeping versus distance, and when a high shelf was what you
  meant). Each step leaves a curve you can see and a sound you can hear
  rather than a claim to take on trust.

Three more bugs found by rendering rather than reading: zone names landed
on the base class's own frequency labels (two rows of text in one strip);
the frequency readout printed `999.9999390`, because a `Slider` prints its
raw double unless told otherwise; and the nodes were invisible on the
first frame, because the display's band list only arrived on the 30 Hz
timer.

---

## Addendum 2: the update you can watch, and one door for lessons

**The progress was always real and always invisible.** `UpdatePrompt`
reported download progress through a `say(text)` callback, and every
editor routed that into a *tooltip* — a line of text next to the pointer,
over a control nobody is hovering while a 40 MB transfer runs. Work with
no visible surface is work that appears not to be happening, which is
exactly the complaint.

`shared/UpdateWindow` is a real panel: what is being fetched, how far
along in both a bar and megabytes, and **what is already installed on this
machine and where** (`shared/InstalledPlugins.h` scans the standard VST3 /
AU / Standalone locations, both shared and per-user, and reads each
bundle's version). "Which copy is this replacing" was the one question
nobody could answer.

It is a child of the editor, not an OS dialogue, for the same reason every
other panel here is — and it never runs a nested message loop, because a
plugin that blocks the host's event loop hangs the DAW.

**On automatic restart, the honest limit:** the standalone app is its own
process and now closes itself after the installer starts, so the new
version takes over. A *plugin* cannot restart the DAW hosting it, and no
program on any platform can replace a dynamic library the host already
has mapped into memory. That is why the closing sentence differs by
context rather than promising the same thing everywhere.

**Learner EQ's lessons moved into the shelf** Comp and Verb use, replacing
a bare `ComboBox`. Same door, same shape, in all three. It carries no knob
modules — ADR 027's reasoning stands, and the shelf now hides its
"walkthroughs" sub-heading when there is no module list above to
distinguish them from.
