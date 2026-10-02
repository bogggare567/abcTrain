# 056 — The battle server judges rounds itself (RoundOracle)

Date: 2026-10-02. Status: accepted (Bogdan on b05: «будет здорово, главное
чтоб не было большой задержки»).

## Problem

A battle round is set by the server as (exercise, level, seed) and built in
every app (ADR 048). The server learned the right answer only from what the
apps claimed, by consensus. In a duel, two different claims void the round —
so a modified app could escape every lost round by claiming its own answer
was the right one.

"The app must not know the answer" cannot be met literally while the app
renders the sound: the setting is in the audio. What can be met is that the
app's word no longer counts.

## Decision

- **`tools/RoundOracle`** — the same game engine with no window and no sound
  card. One line in (`exercise level seed`), one line out (`{"continuous":
  false,"correct":1}` or `correctNorm` + `tolerance`). ~0.2 s to start, ~15 ms
  a question. The server (`server/lib/abctrainOracle.js` on soundkorb.ru)
  keeps one running.
- The server asks it **when the round is created** — during the countdown or
  the previous round's reveal — so the player waits for nothing extra.
- Scoring uses the oracle's answer. An app whose claim differs is marked
  `disagreed` (a version desync or tampering) and scored by the truth either
  way. The answer is in the round's history only after the round closes.
- The app's "what is right" field became optional on the server; today's
  apps still send it (compatibility with a server without the oracle). No
  oracle binary, or it crashed → the old consensus judging.
- **`tests/RoundOracleTest`** holds what this rests on: a seeded round's
  answer depends on (exercise, level, seed) only — not on the sample rate,
  not on the rounds the engine played before. That is also what lets two apps
  agree at all.

## Deploying

The Linux binary lives in the site at `server/bin/RoundOracle` (not in git;
deploy.sh rsyncs it and checks it starts). It needs `libasound2t64`,
`libfontconfig1`, `libfreetype6` on the VPS — the engine links the shared UI
module. A narrower link (engine only) is a later cleanup.

## Not done

Hiding the setting from a memory reader inside the app — impossible while
the app renders the round. Rating-based matchmaking (b06), spectators and
push (b08) are separate.
