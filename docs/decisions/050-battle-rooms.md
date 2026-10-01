# 050 — Battle rooms: ready check, 2 to 6 players, last one with HP

Date: 2026-10-01. Status: accepted (Bogdan: «логику как в КС»).

## Problem

Online battles (ADR 048) paired the first two people in a family's queue and
started at once. Two things went wrong with that. Somebody who pressed "Find"
and walked away was matched anyway and lost by forfeit; and a battle could
only ever be two people, so a class of six had to queue in threes and pairs.

## Decision

- **Ready check, as in Counter-Strike.** While a family has one person
  searching nothing happens. As soon as two or more are searching, the server
  opens a check for up to six of them (`MIN_PLAYERS` 2, `MAX_PLAYERS` 6) and
  each gets "Match found — Accept / Decline" with a 15-second countdown
  (`ABCTRAIN_BATTLE_ACCEPT_MS`). The app brings its window forward and opens
  Live → Battle for it.
- **Who plays.** Everyone who accepted, if that is at least two. A decline,
  or no answer by the deadline, takes that player out of the search; the ones
  who did accept go back to the queue in the place they had, not to its end.
- **The room.** Ten rounds on the same seed for everybody, 100 HP each, the
  damage curve of ADR 049. Consensus judges the round as before: with two
  answers that disagree, or a tie, the round is void; a minority that
  disagrees with the majority takes the "no answer" damage. A player at 0 HP
  is out and watches the rest; the last one with HP wins. If the ten rounds
  run out first, places go by HP.
- **Rating.** Two players: the old one-on-one Elo with the outcome by place.
  Three to six: pairwise Elo over every pair, K divided by N − 1, so a room
  moves a rating about as far as one duel does. Rooms are stored in
  `abctrain_rooms` and appear in the site's feed as their own event ("room").
- **The app** shows the leader — the opponent with the most HP — on the HUD's
  second bar, "nick · alive/total" beside it, and the place at the end.

## Capacity

`scripts/abctrain-battles-load.mjs` in the site repo plays virtual clients
that behave like the app (poll once a second while searching, twice in a
battle). On the container's two cores: 800 players — 1090 req/s, p95 20 ms;
1500 players — 1754 req/s, p95 34 ms, 60 % of one core. The limit is the
Node process, not the rooms; a room costs a few hundred bytes. Run it
against a copy on the VPS (`--url`, never the live site) before an event.

## Not done

Rating windows for matching (anyone in the family is matched); spectating
for people outside the room; push instead of polling.
