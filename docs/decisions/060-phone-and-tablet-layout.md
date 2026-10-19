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
  Tablets and phones in landscape keep the desktop layout.
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
  picture instead of the plugin at a third of its size. A phone is locked
  upright everywhere else and unlocked while the Studio is open, as games
  do. In landscape the switch becomes a column on the left and the plugin is
  scaled to fit.
- **App Store rules:** no donation link in an iOS build (3.1.1), no update
  checker, no microphone permission.

## Consequences

- Two layouts per screen to keep in step; the snapshot job renders five
  phone and five tablet sizes, and the text audit counts clipped strings.
- Hosting a seminar stays a desktop feature.
