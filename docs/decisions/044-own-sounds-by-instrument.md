# 044 — Own sounds sorted by instrument, loops cut by hand, Live that says what is wrong

**Status:** accepted
**Date:** 2026-09-24

## What was there

- Import sorted clips by *character* (percussive, bass, mid, bright, full
  mix). That is what an exercise needs, but it is not how a sound engineer
  looks for material: they look for "the kick", "the vocal".
- A clip's ends were faded to silence. That removed the click at the
  seam and left a dip on every repeat instead.
- A clip could not be removed from the app, and the only way to choose
  *which* eight seconds of a track to train on was to cut them elsewhere.
- A pack was one category, however many clips it held.
- Live said "not connected yet" for every problem, whatever the problem was.

The author, 2026-09-24: sort by instrument, "Other" when unsure; seamless
loops from the tempo; choose your own fragment; delete in the app; warn
about the network with fixes.

## The decision

**Instrument, name first** (`shared/audio/InstrumentLabel`, the same rules
as `tools/library/prepare_audio.py`). The file name decides ("kick in",
"OH L", "бочка"). If the sound plainly contradicts it (confidence ≥ 0.7,
not a compatible pair like kick/808-bass), the clip goes to **Other**. With
no telling name, the sound decides only what sound can decide: kick, snare,
hi-hat, cymbals, toms, bass, a finished mix. Guitar, keys, vocal and wind
are capped below the threshold on purpose, because a few spectral
measurements cannot separate them. A single song-length file with no
instrument name is a **Full mix**. Anything else is **Other**, with the
reason written down (in the pack report). An honest "don't know" beats a
confident mistake.

**Seamless loops.** With a steady tempo, cuts are whole bars from a beat
(as before). The seam is now closed by folding what followed the cut into
its first moments (equal power; 15 ms on the grid, 120 ms off it), so the
file loops on its own with no dip. A clip whose first second is silent is
not written.

**Your own fragment.** Drag across the big waveform, or pick any track with
"Loop from a track…". The selection is a wish: with a tempo the start moves
to the nearest beat and the length to whole bars (whole beats under a bar).
Without a tempo the ends move to quiet points. The screen says what the
snapping did.

**Deleting.** A cross on the row, then an inline "Delete / Keep". The file
goes to the system trash, never deleted outright where a trash exists.
Only files under the library folder can be deleted. A folder left empty goes too.

**Packs.** Packs are split into categories by subfolder, one per
instrument, under the pack's title. "Add sounds…" takes a .zip or a folder
with a `pack.json` and installs it as is, replacing an older copy of the
same id. The owner's own pack (`prepare_audio.py` over his recordings) is
**not committed to this repository**. It is ~130 MB of FLAC, and every
clone and CI checkout would carry it forever. It ships as a release asset
(or a separate sounds repository) and is installed with the same button.

**Live connection** (`Source/LiveLink`). The check runs when the player
opens Live, and again on "Check again". That keeps the offline rule: the
app talks to the Live server only when the player uses Live. It tells four
things apart without asking any third party:

- **no network**: no address but loopback;
- **no internet**: the system resolver cannot find soundkorb.ru;
- **server down**: it resolves but does not answer;
- **app too old**: `/api/abctrain/health` names a newer `minApp`.

Each gets a banner with what to do. A local room needs only a LAN address.
The page flags 169.254 addresses, and the room card answers "phones cannot
open it": guest Wi-Fi isolation, the firewall, mobile data.

## Consequences

- Old character folders stay where they are and keep working. New imports
  go to instrument folders. "Full Mix" keeps its old name so both land in
  one folder.
- The keyword tables and thresholds exist twice, in C++ and Python. A
  change on one side needs the same change on the other.
  `tests/InstrumentLabelTest` and `tests/tools/test_prepare_audio.py` use
  the same example names.
- Tools and tests switch the network off (`LiveLink::networkAllowed`), so
  a snapshot never depends on the rendering machine's network.
