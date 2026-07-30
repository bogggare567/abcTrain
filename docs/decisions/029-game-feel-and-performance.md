# 029. Game feel and performance: the overhaul the polish passes were not

## Status

In progress (stage 1 done). 2026-07-31.

## Context

After v1.3.1 the user's verdict on the product was blunt and, on
inspection, correct: everything works, but there is no *game* in it — no
pull to come back, modes that feel like settings, feedback that whispers,
lag, and font sizes that wander between screens. The previous design
passes (ADR 009, 018, 019, 023) tinted surfaces — gradients, shadows,
theming — but never touched the loop the player actually lives in:
hear → answer → find out → next.

The honest diagnosis, screen by screen:

- **The moment of answer teaches nothing and celebrates nothing.** The
  scale showed a marker and a sentence; points arrived silently in a
  label; a *level-up happened with no fanfare at all* —
  `registerAnswer()` literally discarded the "level changed" boolean that
  `applyAnswerToProgress()` returns.
- **Modes are a setting, not a choice.** Practice/Survival/Blitz are
  pills parked next to the EQ toggle; a run starts by clicking a pill,
  which is how you'd toggle a checkbox, not how you'd enter an event.
- **It lagged, for findable reasons** (below), and three strings ignored
  the user's text-size setting, so changing it made the UI *less*
  consistent rather than more.

## Stage 1 — the foundation: performance and typography

Three real defects, each verified in code rather than guessed:

1. **`GuideTooltip` re-blurred the world on every frame.** Its `paint()`
   called `createComponentSnapshot` on the *parent editor* — re-rendering
   the spectrum, the waveform and every knob — then ran a full 2D
   Gaussian convolution, and it sits over displays that repaint at 30 Hz,
   so that work ran tens of times a second for the whole length of every
   knob drag. This was the single biggest source of "the plugin feels
   laggy". Worse: the result was drawn offset by the card's own position
   in parent space into its local context and **clipped away entirely** —
   the "real Gaussian blur" ADR 019 was proud of was, at those bounds,
   invisible. All cost, no pixels.

   Fix: the blur is built **once per appearance**, in `timerCallback`, at
   the moment the card is about to appear — while its own `paint()` is
   still a no-op, so the snapshot can neither recurse into it nor contain
   an echo of its previous frame. The cached image is then drawn (at the
   correct local coordinates) for every frame of the fade and hold. The
   card is translucent glass; the content behind it being a moment old is
   not observable. `AbcTrainLookAndFeel::blurredSnapshot()` is the split
   primitive; `paintBlurredBackdrop` remains for one-shot callers.

2. **The welcome screen repainted the whole window at 60 Hz forever.**
   It opens on every launch, and its bouncing wordmark drove a
   full-window `repaint()` — gradient, noise texture, every label — sixty
   times a second for as long as it stayed open. The app's first
   impression was its most wasteful screen. Fix: once the three-word
   reveal has finished, only the wordmark's own strip is repainted (the
   strip is remembered from `paint()` and expanded past the hop height).

3. **Three strings bypassed the type ladder** (`ChoiceSliderComponent`'s
   scale-mark labels, `AchievementToast`'s title, `GuideTooltip`'s body):
   raw `FontOptions` literals get neither the chosen typeface nor the
   text-size slider's scale, which is why moving that slider made the UI
   *drift apart* — everything scaled except those three. All three now
   resolve through the ladder.

What was checked and deliberately left alone: `WidgetStateRegistry`,
`HomeScreenComponent`, `GainReductionMeter`, `IconButton` and
`ChoiceSliderComponent` all already settle their eased values and repaint
only what moved; their idle timer ticks are arithmetic on a handful of
floats, not repaints. The radial-gradient window background repaints only
within JUCE's dirty-region clip, so caching it into an image would buy
little and cost invalidation complexity.

## Stage 2 — the moment of answer

The heart of the loop, rebuilt around one principle: **the moment you
find out is the product**. Everything here serves that ten seconds.

- **`ProgressManager::AnswerOutcome` + `onAnswerScored`.** The manager
  always knew what an answer produced - `applyAnswerToProgress` even
  returned "the level changed" as its bool - and `registerAnswer` threw
  it away. It now fills an outcome struct (points awarded including
  bonuses, promotion opened/failed, level taken) and fires a callback,
  synchronously, the same null-safe single-slot shape as
  `onAchievementEarned`. Covered by a new test group driving the callback
  through a full grind-open-fail-pass cycle.
- **The reveal is motion, not a repaint.** On a continuous answer the
  tolerance band *slides* from the player's guess to the true value
  (colour crossing from accent to verdict as it goes), and the target
  marker sweeps down as it arrives - the size of the miss is shown as
  travel distance, which no pair of printed numbers ever conveyed.
  Discrete zones fade their verdict colours in on the same eased value.
  Driven by the slider's existing 60 Hz timer; `completeAnimation()` is
  the snapshot seam.
- **Points fly.** `PointsFlyup` lifts "+12" (or "+65" when the daily
  bonus lands, in the warm accent) from the verdict line up through the
  exercise header. The number was always paid; now it is *felt*.
- **The promotion test is visible.** `PromotionPips` sits beside the
  verdict line while a test is live: five dots, the newest popping in
  with an ease-out-back overshoot. Opening the test and taking the level
  both announce themselves through the existing `AchievementToast` - one
  vocabulary for "something was earned".
- **Instructions collapse once familiar** (3 lifetime correct answers on
  that exercise): the paragraph's height flows to the answer scale, and a
  "?" `IconButton` in the exercise row (new `AppIcons::Icon::help` glyph)
  brings it back for a session. Familiarity is per-exercise and the
  pin is deliberately not persisted - "show me again" is a moment, not a
  setting.
- **`EditorSnapshots` gained an `answered` extra** rendering a
  deliberate near-miss, so the state stage 2 exists for is on the
  contact sheet in both themes.

## Stages 3-4 (planned)

- **Stage 3, modes as events**: a run starts with a countdown and a HUD,
  not a pill toggle; "one more run" is one tap on the results screen.
- **Stage 4, anchors**: the daily streak and daily challenge surfaced as
  first-class cards on Home, achievement pulse, session goal.

Each stage lands as its own commit with snapshots re-rendered before and
after.
