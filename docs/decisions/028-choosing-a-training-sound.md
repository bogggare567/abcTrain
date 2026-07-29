# 028 — Choosing a training sound

**Status:** accepted
**Date:** 2026-07-29

## Context

Reported directly, and fairly: *"I still don't understand how they load, how
they're saved, or how to choose them. There's no side menu with the files."*

The screen listed **categories** only. Inside one it picked a clip at
random, never showed a filename, never showed a path, and gated categories
behind the player's level. It was possible to import an album and have no
way to find out what had happened to it.

## Decision

Two panes. The left rail is what to train on — pink noise, then every
category. The right pane is the **actual files** in whichever one is
selected, and clicking one pins training to it.

### Rotation stays the default; pinning is now possible

Shuffling within a category is still the better default — twenty imported
drum loops should be twenty drum loops, and looping one clip all session
was the fastest way to stop hearing it (ADR 015). But *"let me hear that
one"* is a real request, and until now the only way to answer it was to
put a single file in a folder of its own.

`ReferenceAudioLibrary::pinFile` sets it; `advanceToRandomClip` refuses to
rotate while pinned, because rotating away from a clip the player asked
for by name is the app overruling them. Clicking the category again goes
back to shuffling. The footer says which of the two states you are in —
a filename on screen with no such label reads as a promise about what
plays next.

### The path is on screen, and there is a button that goes there

"Where did my import go" was unanswerable from this screen. The answer is
a place, so the control is one that opens it.

### Categories no longer unlock with level

They used to (`maxLevelReached > i`, ADR 015 called it "a simple
first-pass rule"). That made some sense for shipped content and none at
all for a folder of the player's own music. **Files you imported yourself
should never be locked behind anything**, and a greyed-out row saying
"reach level 3" is a large part of why this screen read as broken.

## Consequences

- The panel is 640×470 rather than 480-and-growing; two panes need width,
  and the old single column could not have held a filename beside a
  category name.
- The category rows and file rows are custom-painted rather than
  `TextButton`s. A button cannot carry a count, a tick and a filename, and
  the previous version was already painting the count and tick on top of
  its buttons by hand.
- `referencePinned` joins the shared settings file. A pin is a per-person
  preference like every other choice here.
- Still not covered by a test: the panel. `ReferenceAudioLibraryTest`
  covers scan/select/persist; pinning is checked by rendering the screen
  and by the library's own contract, not by a UI test.
