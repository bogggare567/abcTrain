# 059. Decibelo на Glicko-2

Date: 2026-10-19. Status: accepted (Bogdan: «берём обязательно, как на lichess»).
Supersedes the rating part of ADR 048 (Elo, K = 32) and ADR 050 (K / (N − 1)).

## Context

Decibelo was Elo with K = 32 and a "?" on the first 5 battles. Elo keeps one
number per player, so it cannot tell a newcomer from a veteran: a newcomer
crawls to his level over dozens of battles, and somebody back after a year
is treated as exactly as well known as yesterday. lichess solved the same
problem in the same kind of product (free, open, ratings from games between
people) with Glicko-2.

## Decision

- **Glicko-2** (Glickman 2012, steps 1–8; volatility by the Illinois
  iteration), on the server: `server/lib/glicko2.js` in the site repo.
  Rating 1500 ± 350 at the start, τ = 0.5, volatility 0.06.
- **One battle is one rating period.** Time without battles widens the
  deviation as sqrt(φ² + σ²·t), t in 4-day periods: from the floor back to
  "?" in about a year, as on lichess. The deviation never drops below 45, so
  a rating does not freeze while the player keeps improving.
- **"?" = deviation above 110** (lichess's threshold), not "fewer than 5
  battles". Such rows rank below settled ones.
- **Rooms of 3–6** (ADR 050): each player played everyone else by place,
  each pair weighted 1/(N − 1), so a room weighs one battle — the same intent
  as K / (N − 1).
- **The API does not change.** `rating`, `deviation`, `provisional`, `delta`
  are what the app already reads (`LiveAccount`, `LiveScreenComponent`).
  Deltas are no longer equal and opposite: a newcomer moves more than the
  settled opponent he played.
- **Migration** on first open of the database: rating kept, deviation = the
  one the table already showed by number of battles, volatility = start.

## Not decided here

Anonymous (no-email) battles as unrated "casual" games, like lichess's;
fair-play marking with rating refunds — both on the board.

## Checked by

Site repo: `scripts/abctrain-glicko2-test.mjs` (Glickman's worked example:
1464.06 ± 151.52, σ 0.05999), `scripts/abctrain-glicko2-migrate-test.mjs`,
`scripts/abctrain-battles-test.mjs`.
