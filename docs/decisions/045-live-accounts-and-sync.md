# 045 — Live accounts: sign in on the site, a key per computer, a small synced summary

**Status:** accepted
**Date:** 2026-09-24

## The question

The author, 2026-09-24: connect the site, register, and store data so that
the server does not take much space. Most of the data lives on the device,
and the device is what gets checked. Security, sign-in and sync.

## The decision

**Who you are is decided on the site, not in the app.** You sign in at
soundkorb.ru with an e-mail and a 6-digit code (no passwords anywhere). The
app never sees the e-mail code. It links the computer instead:

1. The app asks for a short code: `link/start`, about six characters, lives
   10 minutes.
2. On soundkorb.ru/link?code=… the player signs in and presses "Connect".
   The page shows which computer is asking, and warns not to confirm a code
   you did not start yourself: a link someone sends you is how an account
   gets stolen.
3. The app, polling every 2.5 s, receives a **device token exactly once**
   (`link/poll`).

The server keeps only the token's SHA-256. The site lists your computers
and can switch any of them off. Signing out in the app forgets the token at
once (it works offline) and tells the server if it can.

**What the server stores, and why it is small.** About 2–4 KB per account:

- e-mail, only to send codes;
- nick and country;
- device names and token hashes;
- the sync summary, capped at 16 KB (typically 1–2 KB);
- battle results.

Round history, skill buckets and the hearing log stay on the computer. A
thousand players come to about 4 MB. Details and the deletion procedure
are in the site repo: `docs/abctrain-live-server.md`.

**Sync is a summary with a merge rule, not a copy.**
`ProgressManager::makeSyncSummary` sends, per exercise:

- level and record;
- rounds and correct answers;
- best streak, Survival and Blitz bests;

plus streak days, practice time and achievements. `mergeSyncSummary`
brings the other side in:

- records and counts take the larger value;
- achievements are united;
- the current level comes from the server only for an exercise never
  played on this computer. A new computer starts where you are, and a
  computer you use keeps today's form.

The server does not interpret the data. On a version conflict (two
computers at once) it answers 409 and the app merges again.

**When the app talks to the server.** Only when the player acts:

- opening Live, which checks the connection (ADR 044);
- signing in;
- and, once signed in with sync on, a sync shortly after launch and every
  quarter of an hour when something changed.

A computer that never signed in makes no request beyond the update check
(ADR 042). That is the offline rule, restated.

**Security, in short.**

- Tokens are 32 random bytes and stored only as hashes. E-mail codes are
  hashed with a server pepper, expire in 10 minutes and allow 5 attempts.
- Rate limits: per e-mail, per IP (the last `X-Forwarded-For`, which nginx
  writes) and per account.
- Web actions that change something require same-origin and JSON; they are
  never cached.
- The app only ever opens links to its own site, whatever a response says.
- The app keeps its token in its own settings file (`abcTrain/…live`), not
  the progress file, so it never travels with an export or a bug report.
  It is not encrypted. Anyone who can read your user folder can already
  read your progress, and the token grants nothing more than that
  progress and your nick.
- Honest limit: a rating decided by a client can be forged by a modified
  client. Battles will be scored on the server from rounds it hands out.
  That makes casual cheating pointless, but it is not proof against
  someone who rewrites the app.

**Russia.** The domain is .ru and the server is in Russia. From some
countries it may be slow or blocked. The app says so where sign-in happens
and in the "server not answering" advice. If the audience abroad grows,
accounts move to (or replicate on) a server in Europe. The protocol
carries a base URL (`LiveAccount::baseUrl`) and nothing else ties it to
this host.

## Not in this release

Battles between people and online seminar rooms: the Live page says so
plainly. They need a round server (the server hands out rounds and scores
them), and that is the next step. Until then the Battle tab offers a battle
against a bot, which needs no server at all (ADR 046). The rating page already works, fed by the battle-results API.
