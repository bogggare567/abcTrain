# 063 — Visual review: pictures on the ruler, honest hints, a search you can see

Date: 2026-10-23. Status: accepted (Bogdan on the «abcTrain · визуальный
разбор» canvas: «это очень круто добавляй на пк», «главное чтоб он так не
пиковал», «мне не нравится эта панель совсем»).

## Problem

On a phone the hint pictures read as empty boxes: the spectrum hugged a
fixed 0 dBFS ceiling, the goniometer was a coin-sized circle with a lone
"1.00", the compression envelope was a hairline. The ruler games printed
two rows of numbers that looked like two scales ("-90 -70" over "-100 -50").
Live had no picture of waiting at all: a line of text with a clock in it.

## Decision

- **The ruler draws the player's own answer** (`Game::getScaleIllustration`):
  a bell where the EQ would sit (Guess the Band), a source on a stage seen
  from above (Pan), the train of repeats an echo at that time would make
  (Delay). The round's own value appears beside it only once the answer is
  in. It gives the number a shape without giving anything away — a picture
  of the *round* would be a free hint.
- **One row of numbers** on every ruler; the half-step lines stay unlabelled.
- **A grip on the line** on touch screens.
- **Spectrum hint auto-ranges**: the top follows the loudest part of the
  signal (fast up, 2.5 s down) over a 66 dB window. Learner plugins keep
  the absolute scale.
- **Goniometer**: the cloud scaled to its own loudest point; a correlation
  bar from −1 to +1 under it when there is room.
- **Compression hint**: the waveform scaled to the signal and a crest meter
  — peak against RMS and their difference in dB, the number compression
  changes.
- **Search radar** (`SearchRadar`): your rating in the middle, the server's
  window (±200, +100 every 10 s, anyone after 40 s, ADR 059) as a widening
  ring; a found room as a draining ring with the seconds left to accept.
- Phone fixes: the room QR no longer painted into an empty card at the top
  left; the rating table drops the country column under 480 pt; the invite
  list stacks name over e-mail under 420 pt.
- Results: rounds as columns (height = how close, a short warm block = a
  miss); ranges as bars with counts instead of a line through seven points.
