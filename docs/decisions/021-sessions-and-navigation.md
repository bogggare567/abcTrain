# 021. Training runs have a shape, and the trainer has screens

## Status

Accepted, implemented. `SessionManager` covered by
`tests/SessionManagerTest` (8 groups, pure state - no message loop);
the screens verified by playing through them in the running app.

## Context

Two related problems, both of them "this is a demo, not a product":

1. **A training session had no shape.** Answer, press "New Round", answer
   again, forever. Nothing at stake, nothing to finish, no reason to stop
   or continue. Difficulty existed but only as a number that drifted with
   points.
2. **The editor was one flat panel.** Title row, exercise, answer,
   progress - everything stacked on one screen, with the training picker
   bolted on as an overlay dropped over the top of it. Nine exercises were
   a `ComboBox` of nine names, which said nothing about what any of them
   trained or how you were doing at it.

## Decision

### Runs: Practice, Survival, Blitz (`Source/SessionManager`)

- **Practice** - unlimited, no lives, no clock, no run score. The
  low-pressure mode for actually learning a skill, and the default.
- **Survival** - 3 lives, a wrong answer costs one, the run ends at zero.
  The score is how many you got right before that.
- **Blitz** - a 90 s clock; a wrong answer costs 5 seconds rather than
  ending the run, so the pressure is *pace* rather than caution.

`SessionManager` owns only the run's own state - lives, clock, run score,
auto-advance timing. It deliberately knows nothing about `Game`,
`GameManager` or `ProgressManager`; the editor drives it and reacts to its
callbacks. That is what makes the whole mode/lives/timer state machine
testable with no message loop, no audio device and no `Component` - the
same reasoning that put `ProgressManager::registerAnswer()` behind a
direct synchronous entry point (see `docs/testing-strategy.md`).

### Auto-advance

The next round starts on its own after an answer: ~0.9 s after a correct
one, ~1.9 s after a wrong one (more to read), and **never** once a run has
ended, so the final result stays on screen instead of being replaced by
another question.

A pending advance carries an id that is bumped whenever anything else
starts a round - a game switch, a mode switch, a manual New Round, a
screen change. Without that, a queued advance lands on a run the player
has already left.

### Screens: Home → Training (`Source/HomeScreenComponent`)

Home does three things, in the order they matter: say where you are
(level, streak), let you pick what to train, and get out of the way.

Trainings are grouped by **the skill they build** - Frequency, Dynamics,
Space & stereo, Character - not by registration order. Nine flat entries
are a list; four labelled groups are a map of the subject. Each card
carries the exercise's icon, one line on what it gives you, and your own
record on it.

Picking a card starts the training. There is no second confirm step.

### "What are you interested in" is a star, not a questionnaire

The obvious reading of "let the user choose which trainings interest them"
is a first-run survey. It was rejected: a questionnaire's answers go stale
within a week, it blocks a first-time user from the thing they opened the
app to do, and it has to be re-findable and re-editable anyway, at which
point it is just a settings screen with extra ceremony.

Instead every card has a star, and starred trainings are pinned to a
"Your focus" group above everything else. Edited in place, any time,
persisted in `ProgressManager` alongside points and per-exercise stats. A
starred training still appears in its own category too, so the map of the
subject never develops holes.

The star is hit-tested *before* the card it sits on, or starring a
training would also start it.

### Per-exercise records

`ProgressManager` now keeps lifetime stats per game - rounds, correct,
best streak, best Survival score, best Blitz score - persisted with
everything else, and surfaced on the cards. Deliberately separate from
each `Game`'s own `getScore()`/`getRoundsPlayed()`, which stay in-memory
session counters that reset every time the plugin is reopened.

Stats are keyed by game *index*, which means reordering `GameManager`'s
registration list would shuffle them. New games get appended, not
inserted. This is a real constraint and it is written down here because
nothing in the code enforces it.

## Consequences

- The home screen lives in a `Viewport`: nine trainings across four
  categories already exceed the window, and the catalogue only grows.
  Found the hard way - the fourth category was simply invisible.
- Answering is refused once a run has ended, so a late click can't score
  into a finished Survival run.
- Practice runs report no score, so `onRunEnded` never records one - a
  best-score board fed by an unlimited mode would be meaningless.
- Not built yet: a results screen at the end of a run (the run currently
  just stops with its score on the label), and any use of the Blitz/
  Survival bests beyond the line on each card.
