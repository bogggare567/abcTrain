# 043 — The teaching layer in a window of its own

**Status:** accepted
**Date:** 2026-09-24

## What was there

The Learner's module panel was drawn over the analysis section: the
spectrum, the curve and the meters. The instrument lesson took a third of
the EQ's width. The "under the pointer" guide first floated over the
bottom of the analysis, then got a strip of its own. Every one of these
covered or squeezed the thing being taught. In the author's words:
«модули перекрывают обзор на то, что происходит в плагине».

## The decision

The author's call, 2026-09-24: a separate window you can drag anywhere.

- The **Modules** button (the icon in a DAW, the word in the app's
  Studio) opens and closes a `CompanionWindow`. It has a native title bar,
  can be resized, and remembers its place and whether it was open, per
  plugin.
- The window holds, top to bottom:
  - **Lesson / Modules** tabs. The lesson tab exists only where the
    plugin has one (Learner EQ).
  - **Under the pointer**: the guide text.
  - **Hearing today**:
    - in the app: this session, the week's dose and the next break,
      drawn like a usage meter;
    - in a DAW: time with the plugin open. Only the app's HearingGuard
      knows the dose.
- The editor lends the module screen and the lesson to the window and
  takes them back when it closes. With the window closed, the plugin is
  the all-in-one layout it was, so nothing is lost for someone who never
  opens it.
- The window stays above the host while the host is the active
  application (`Process::isForegroundProcess`), and is an ordinary window
  otherwise. It hides while the plugin's own window is hidden.

## Also in this change

The EQ's filter types are shape icons: bell, shelves, high-pass,
low-pass, notch. The name shows as a tooltip and in "Under the pointer",
with a line on what the filter does.

## What it costs

- In the container the window cannot be tested as a window: the snapshot
  tools render its content next to the plugin
  (`LearnerEQ-Companion-Window`). Check dragging, always-on-top and the
  remembered position on the Mac.
- A plugin now opens a second top-level window. Hosts differ in how they
  treat that, so it is worth a check in Logic, Ableton and Reaper.
