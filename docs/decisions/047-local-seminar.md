# 047 — The local seminar: the laptop is the server, phones answer, a projector shows

**Status:** accepted
**Date:** 2026-09-25

## The question

The author, 2026-09-25: make the QR code, the projector screen and the
seminar actually work (until now the Live seminar tab was a layout with
"not yet" behind every button), make the list of names and e-mails easy to
fill, and make sign-in work.

## The decision

**A local seminar needs no server but the presenter's own laptop.**

- `LocalRoom` serves one page and three JSON calls over plain HTTP on the
  venue's network (port 8930, or the next free one): `GET /`,
  `GET /api/state`, `POST /api/join`, `POST /api/vote`. Phones poll once a
  second; that survives a phone falling asleep and costs nothing on a
  hundred phones. The protocol is `handleRequest()`, testable without a
  socket (`tests/LocalRoomTest`), and the page is checked in a real browser
  by `tools/RoomDemo` + `tools/room_demo_check.mjs`.
- **The sound is in the hall, not on the phones.** The laptop plays the
  exercise into the PA; phones only answer. It is what the seminar in a club
  was always meant to be, and it keeps "level is never the tell" true for
  everyone in the room at once. Phone playback (each with headphones) stays
  on the canvas as a later step.
- **The rounds are the trainer's own.** `SeminarHost` takes the chosen
  families in turn, an exercise of the family at random, calls the same
  `newRound()`, and reveals the game's own answer and tolerance. The
  presenter never answers, so his progress is untouched — ProgressManager
  scores answers, not rounds.
- **Answer rules carry over:** tapping a choice or letting go of the slider
  is the answer, there is no Submit, and it can change until the reveal.
- **The projector window** (`ProjectorWindow`) opens full screen on a
  second display when there is one; it shows the QR code and address, then
  the question and which version is playing (A clean / B processed), the
  vote spread with the accept band, and the table. Space, A, B, S and F
  drive it, so a clicker runs the room.
- **List only** rooms give each person a four-digit code; the list is edited
  as rows (name, e-mail, code) and takes a pasted spreadsheet, CSV or mail
  header. Codes print as an A4 sheet of cards with a QR each (an HTML page
  the browser prints: no PDF writer in the app).
- **QR codes** are Nayuki's reference encoder (MIT, `shared/ui/third_party`),
  error correction M, a four-module quiet zone, always dark on white.

**Offline rule, restated.** The app opens a listening socket only when the
presenter opens a local room, only on the local network, and closes it with
the room. Nothing leaves the room; results stay on the laptop.

## Sign-in (ADR 045), what was wrong

The site's nginx listed its single-page routes one by one and did not know
`/link`, `/abctrain/rating`, `/abctrain/account`: they answered 404, so the
app's sign-in never got past the browser. The routes are now listed. The
first login e-mail also took minutes on a cold SMTP connection, which the
page showed as an error; the mail transport has 10/10/20 s timeouts now.
The sign-in overlay shows the /link page as a QR code too.

## Not in this release

Online rooms and battles between people — both need the round server
(ADR 045). The Live page says so where it is asked.
