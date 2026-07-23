# 001. A single `Game` interface with index-based choices, instead of per-game UI

## Status

Accepted, implemented.

## Context

The first exercise built was `EQGame`: pink noise through a peak filter,
8 frequency-band buttons, pick the right one. It was implemented by
wiring its specific logic directly into `PluginProcessor` and
hardcoding "8 frequency-band buttons" into `PluginEditor`.

The roadmap calls for more exercises: compression strength (3 choices:
weak/medium/strong), reverb type (4 choices: room/hall/plate/spring), and
others later (stereo width, delay type, distortion type). Continuing to
hardcode each game's UI into the editor would mean either duplicating the
editor per game, or growing one editor into a pile of per-game
special-case branches — both get worse with every new exercise.

## Options considered

1. **Duplicate the editor per game.** Each game ships its own
   `PluginEditor` subclass with its own hand-laid-out buttons. Simple for
   one game, but *n* games means *n* editors with near-identical
   button/label/score-display code, and no shared "add a game" recipe.

2. **One editor with per-game branches** (`if (activeGame == EQ) { ... }
   else if (activeGame == Compression) { ... }`). Avoids duplicating the
   editor file, but the editor still needs to know about every game's
   specific choice set and grows a branch per game — the coupling moves,
   it doesn't go away.

3. **One generic interface (`Game`) covering the common shape, one
   generic editor.** Observation: every planned exercise is "play a
   processed test signal, offer N labeled choices, score the pick." Only
   two things vary per game: how the audio is generated, and what the
   choices are called. Model that directly: `Game::getNumChoices()` +
   `Game::getChoiceLabel(int)` + `Game::submitAnswer(int)`, and the editor
   renders however many choice buttons the active game reports, labeled
   however the active game says, with zero awareness of what a "choice"
   actually represents for that game.

## Decision

Went with option 3. `Source/Games/Game.h` defines the interface; `EQGame`
and `CompressionGame` both implement it; `GameManager` owns and delegates
to whichever is active; `PluginEditor` only ever calls `Game` interface
methods, never anything EQ- or compression-specific.

## Consequences

- Adding a new exercise is: create `Source/Games/NewGame.{h,cpp}`
  implementing `Game`, register it in `GameManager`'s constructor, add the
  two files to `CMakeLists.txt`. No processor or editor changes.
- The editor can't do anything visually game-specific (e.g. a compressor
  gain-reduction meter, a reverb impulse-response cloud) — it only has
  buttons, labels, and text. That's fine for multiple-choice trainer
  games, and is *not* the model used for `LearnerEQ`, which is a
  different kind of tool (a real, continuously-adjustable effect, not a
  "pick one of N" quiz) and intentionally has its own bespoke editor. The
  `Game` interface was never meant to cover that case — see
  [../diagrams/learner-plugin.md](../diagrams/learner-plugin.md).
- `submitAnswer(int)`/`getCorrectChoiceIndex()` etc. being plain `int`
  rather than a per-game enum means the compiler can't catch "passed an
  out-of-range choice index" — each game's `submitAnswer` is responsible
  for treating an already-answered round as a no-op, which is a runtime
  guard, not a type-level one. Covered by the "a second answer in the
  same round is ignored" test in both `tests/EQGameTest.cpp` and
  `tests/CompressionGameTest.cpp`.
