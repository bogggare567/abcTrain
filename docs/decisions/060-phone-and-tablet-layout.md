# 060. Phones and tablets: one app, a compact layout under 600 points

Date: 2026-10-19. Status: accepted (Bogdan: «подгоняй под все планшеты,
телефоны и версии iOS начиная с 18 … просить людей переворачивать экран, как
для игр»).

## Context

The iOS build (ADR 059's session, commit 366c473) ran the desktop layout on
an iPhone: a 940-point design squeezed into 390, the bar across the top,
two-column pages, 30-point buttons. Tablets are wide enough for the desktop
layout; phones are not.

## Decision

- **iOS 18 and up**, iPhone and iPad in one app (`TARGETED_DEVICE_FAMILY
  1,2`). Earlier versions are a small share and would cost testing time.
- **Compact layout below 600 points of width**, decided by width, not by
  device: a narrow desktop window gets it too, so the snapshot tool tests it
  on a Mac (`SNAP_SIZE`, `SNAP_SAFE` for the notch and home indicator).
  Tablets keep the desktop layout.
- **Every screen turns** (Bogdan, after the first run on an iPhone 11: «чтоб
  можно было на любом экране перевернуть и горизонтально проходить уровни»).
  A phone on its side (height under 500 points) gets the tabs as a column
  down the left edge; the training screen goes to two columns - the controls
  on the left in the order a round is played, the scale full height on the
  right; Settings and Live use their one-column phone layouts, Live's cards
  split into two halves; the results card drops its by-range block.
- **Safe area.** Every screen is laid out inside `safeBounds()`; the nav
  bar runs on through the home indicator's strip.
- **The bar goes to the bottom** on a phone: icon over a short label,
  equal cells; Achievements leaves the bar (reached from Trainings).
- **Screens rewritten for the column**, each with the same rule — one
  thing per row, controls at a thumb's height (40–46 points):
  - Home: the focus band stacks its button under the headline; tiles become
    two lines; drag to scroll.
  - Training: level and run HUD on their own row; A/B as two big halves
    under the caption and the hint; modes across the width; ruler labels
    that would collide are skipped.
  - Settings and Training sounds: master–detail — the menu is a page, a tap
    opens the section with a back button.
  - Live: one card at a time (the battle's "people / bot" become a switch);
    a phone joins a seminar but never hosts one.
  - Welcome, results: one column.
- **The Studio asks to be turned sideways.** The Learner plugins are built
  for 820×600 and up; on a phone held upright the page shows a turn-the-phone
  picture instead of the plugin at a third of its size. In landscape the
  switch becomes a column on the left and the plugin is scaled to fit.
- **Touch first.** Lists scroll under the finger with inertia and a little
  stretch at the ends (`shared/ui/TouchScroll.h`); a drag never counts as a
  tap.
- **No black screen at launch:** the launch storyboard is the app's own
  background, then the wordmark settles in (`Source/SplashOverlay.h`).
- **A phone draws in software:** the page background is drawn once and kept
  as a picture (`PanelBackgroundCache`), the meters run at 30 frames with as
  many curve points as the screen can show, the stereo scope fills its dots
  in batches.
- **App Store rules:** no donation link in an iOS build (3.1.1), no update
  checker, no microphone permission.

## Consequences

- Two layouts per screen to keep in step; the snapshot job renders five
  phone and five tablet sizes, and the text audit counts clipped strings.
- Hosting a seminar stays a desktop feature.
