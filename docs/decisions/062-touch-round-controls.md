# 062. A round under the thumb: the same three rows, Answer, haptics

Date: 2026-10-19. Status: accepted (Bogdan, from the "abcTrain Mobile"
mock-ups: «в мокапах были реализованы данные функции ещё»).

## Decision

- **One control layout in every exercise on a phone**, at the bottom, where
  the thumb is: `A | B | the exercise's action` ("narrow the scale", "show
  the sound" - always the third cell), then **Answer**, then the mode, the
  tally and "again". Held sideways, the same rows sit at the foot of the
  left column. A desktop and a tablet keep their layout.
- **Answer is a button on touch.** Lifting the finger only places the
  answer; Answer sends it. A thumb slipping off the glass no longer answers
  for you. With a mouse the release still answers.
- **Haptics:** a light or strong click for every step the finger crosses on
  the scale (a thirtieth of the ruler - a third of an octave on the
  frequency scale - or the next named choice). Settings > Training >
  "Vibration on the scales": off / light / strong, offered only on a phone
  or tablet. UIKit's impact generator (`Source/Haptics_ios.mm`).
- **No status bar during a round on a phone** (iOS kiosk mode): the strip
  beside the notch or the Dynamic Island shows the run's lives or clock on
  the left and the staircase's squares on the right. An SE, with 20 points
  of status bar, keeps them in the page.
- **The splash** has the mock-up's parts: the wordmark, the three family
  words, drifting waves, ten segments lighting in turn, "preparing the
  sounds" and the headphone note. The launch storyboard is its first frame's
  background.
- **Profile**: Decibelo history by week / month / all, and an activity feed
  - each finished Survival or Blitz run (with "new record") and each battle
  (won, lost, draw), kept on this computer, worded when shown.

## Not done, and why

- **Mode not preselected** (the mock-up's version): Bogdan chose "last mode
  already chosen" for ADR 061; that stays.
- **The Blitz clock in the Dynamic Island and on the lock screen** needs a
  Live Activity - an ActivityKit widget extension, a separate Swift target
  the JUCE CMake build does not make. Later, with the App Store account.
- **Support tiers and a monthly option** need in-app purchases on iOS; the
  desktop link stays a single "Support".
