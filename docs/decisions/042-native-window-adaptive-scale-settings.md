# 042 — A normal window, a scale that follows it, and the corner moved into Settings

**Status:** accepted
**Date:** 2026-09-24

## What was there

- The app ran in JUCE's stock standalone window: a drawn title bar with
  close and minimise on the right and an "Options" button on the left.
  On a Mac that is the one window on the screen that looks wrong.
- The size of everything was a picker in the top bar (S / M / L / XL),
  applied as a transform on a layout drawn at 1180 × 880, with a floor of
  940 × 620 that the window kept bumping into.
- The top bar's right corner held four controls you set once: theme,
  update check, the size picker and the language.
- The update check ran only when the button was pressed (ADR 007): the
  offline rule said the app may connect only when the player opens Live.

## The decision

The author's call, 2026-09-24.

1. **Native title bar.** Once the editor is inside its standalone window
   it switches that window to the system's own title bar
   (`DocumentWindow::setUsingNativeTitleBar`), so macOS draws the traffic
   lights on the left, and full screen and edge resizing work as they do
   everywhere else. The "Options" button goes with the drawn bar. The one
   thing it offered that mattered, the audio device, is on
   Settings → Hearing → "Audio device".
2. **The scale follows the window.** No picker. The scale is
   `min(width / 1100, height / 780)` in screen points, in steps of 1/40,
   between 0.8 and 2.4. Taking the smaller ratio means the layout always
   gets *at least* 1100 × 780 logical. A wide window shows more, a big
   one shows everything bigger, and no screen is ever squeezed below what
   it was built for. The first launch opens at 88 % of the screen's
   usable height (and at most 1.5 times that for the width). After that
   the window opens at the size it was closed at.
3. **The corner moves into Settings.** Theme and language go to
   Appearance. The update check goes to About: "Updates: automatically /
   when asked" and a "Check now" button with its outcome under it.
4. **Updates are automatic by default.** At launch, four seconds in, one
   anonymous GET of the release list (the same request ADR 007 describes).
   It is silent unless there is something newer. Turning it off in
   Settings brings back the old "only when asked" behaviour.

## What this changes in the rules

The offline rule in `CLAUDE.md` now reads: no account, no server, no
telemetry. The only request the installed app makes on its own is the
release list, and a single switch turns that off. Nothing about the
player is sent. The request is the one a browser makes to the public
releases page.

## What it costs

- The native title bar cannot be checked in the Linux container. The
  snapshot tools never create a window, so the author checks it on the
  Mac.
- Text size is now two things: the window's scale and Settings → Text
  size. The first is automatic. The second stays for people who want
  bigger text in the same window.
- Snapshot and click-map tools still render the design size
  (`ABC_DESIGN_SIZE`), so their pictures are comparable with earlier
  ones.
