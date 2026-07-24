# 002. Adaptive difficulty via `Game::setDifficulty(int)`, driven by `ProgressManager`

## Status

Accepted, implemented.

## Context

[ADR 001](001-game-interface.md) established one `Game` interface driving
a generic editor. Adding a leveling/progression system
(`ProgressManager`) that should make exercises harder as the player gets
better needs a way to actually change each game's difficulty — and to do
that without the editor or `GameManager` needing per-game knowledge of
what "harder" means for EQ vs. compression vs. reverb, the same way
ADR 001 kept per-game *choice* logic out of the editor.

## Decision

Extend the `Game` interface with one method:

```cpp
virtual void setDifficulty (int level) = 0;
```

`level` is 1-10, matching `ProgressManager`'s level range. Each game maps
that range to its own three tiers (levels 1-3 / 4-6 / 7-10) however makes
sense for its own DSP:

- **`EQGame`**: boost/cut amount shrinks (9 dB → 6 dB → 3 dB). Smaller
  change is harder to hear. Band count (8) and choice count stay fixed.
- **`CompressionGame`**: three separate preset tables (`easyPresets`/
  `mediumPresets`/`hardPresets`), same three labels (Weak/Medium/Strong)
  throughout, but threshold/ratio values converge toward each other at
  higher tiers, with a correspondingly smaller makeup-gain compensation.
  Choice count (3) stays fixed.
- **`ReverbGame`**: `getNumChoices()` itself shrinks — 2 types (Room/Hall)
  at the easy tier, 3 (+ Plate) at medium, all 4 (+ Spring) at hard. The
  type array's order was already most- to least-distinguishable (see
  `ReverbGame.h`), so restricting to a prefix of it is genuinely easier,
  not arbitrary.

`GameManager::setDifficultyForAllGames(int)` propagates to every
registered game (mirroring how `prepare()` already applies to all games
up front, not just the active one) so difficulty stays consistent
whichever game the player switches to mid-session.

## Consequences

- **`ReverbGame`'s choice count can now change at runtime**, which nothing
  before this ADR needed to handle: `PluginEditor` only ever rebuilt its
  choice buttons on game *switch*. Fixed by having
  `refreshFromGameState()` also rebuild buttons whenever a fresh
  (unanswered) round's `getNumChoices()` doesn't match the current button
  count — see `Source/PluginEditor.cpp`.
- **This is safe only because level never decreases.** `ProgressManager`
  has no "demote" path — a wrong answer doesn't lower the level, only
  correct answers accumulate points toward the next one. That means
  `activeNumTypes` in `ReverbGame` (and the equivalent internal state in
  the other games) is monotonically non-decreasing over a session, so a
  round's `correctChoiceIndex`, fixed at `newRound()` time, can never be
  invalidated by a difficulty change that happens later in the same
  round (e.g. from the level-up that round's own correct answer causes).
  If a "demote on a losing streak" feature is ever added, this invariant
  would need re-checking — a round in progress could end up with a
  `correctChoiceIndex` outside the new, smaller `getNumChoices()`.
- Every existing and future `Game` implementation must implement
  `setDifficulty`, even if it's a no-op — there's no default. That's
  intentional: it forces a conscious decision about what "harder" means
  for a new exercise rather than silently ignoring difficulty.
