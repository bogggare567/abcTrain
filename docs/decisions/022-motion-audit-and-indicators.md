# 022 — The motion audit, compact indicators, and a room per exercise

Status: accepted.

Three things that were all owed from the same feedback: *"минимализм без
дёрганых анимаций"*, *"выбор языка и размера как индикаторы, а не боксы"*,
and *"возможно разный фон на разных играх"*.

## The motion audit

Every animated surface was gone through, timing by timing. Four things
came out of it, two of them real bugs.

### The wrong-answer wobble was a shake, not a sway

It ran 2.5 oscillations in 450 ms — about **5.5 Hz**. Anything above
roughly 4 Hz stops reading as a movement and starts reading as
vibration, which is exactly the texture that makes an interface feel
cheap. It also used a linear decay envelope, so the last oscillation
still had visible velocity when the animation was cut off at t = 1.

Now 1.5 oscillations over 720 ms (~2 Hz) with a squared envelope. Same
idea — a small, sympathetic head-shake — at a speed the eye reads as
deliberate, settling to a stop rather than being clipped.

The general finding is worth keeping: **the frequency matters more than
the amplitude.** Making a 5.5 Hz wobble smaller would have made it a
smaller shake, not a sway.

### The progress bar snapped when two answers landed close together

`LevelProgressBar::setProgress` read its animation start value *after*
calling `complete()` on the animation already in flight — and `complete()`
jumps that animation to its own end value. So retargeting mid-flight
snapped the fill to the *previous* target and only then eased toward the
new one: one visible jump in the middle of what should be a single
continuous move.

Reading `displayedProgress` before the `complete()` call fixes it. It
only reproduces when two answers land inside 400 ms, which is routine in
Blitz and rare enough everywhere else that it had gone unnoticed since
the bar was written.

### Two hard on/off thresholds inside a bar that is otherwise all easing

The fill can't be drawn narrower than its own corner radius without
ceasing to be a rounded shape, so it was clamped to a minimum width —
which meant it *popped* into existence at full opacity the moment
progress crossed 0.1%. The breathing glow switched on the same way at 2%.
Both now fade over the stretch they used to jump across.

### The bar repainted 30 times a second to show nothing

The breathing timer repainted unconditionally, including on an empty bar
where the glow it drives isn't drawn at all. Now guarded. Not a visual
bug, but a plugin can have many instances open at once, and free CPU
spent on an unchanged picture is not free to the person mixing.

### What was checked and left alone

`WidgetStateRegistry` (60 Hz, asymmetric press/release), the choice
slider's hover and entrance interpolations, `HomeScreenComponent`'s
per-card hover, and `IconButton`'s morph all early-out correctly and run
at durations that read as intentional. The correct-answer glow at 900 ms
is deliberately the same length as the auto-advance delay, so it decays
into the round change rather than being cut short by it.

## Language and size are indicators, not form fields

`shared/CompactSelector` — a one-or-two-glyph value with a hairline
chevron, no well and no border until hovered, opening a `PopupMenu` on
click.

A `ComboBox` draws a bordered well and a permanent arrow. That is right
for a control you *use* — the game selector, mode, level, reverb type —
and wrong for one you set once and forget. Language and window size were
taking 142 px of the title row to display two facts that never change.
They now take about 75 px and read as status rather than as a form
waiting to be filled in.

Two details worth recording:

- **It is not a `ComboBox` subclass.** Restyling it would have had to go
  through `LookAndFeel::drawComboBox`, which is shared with every other
  combo box across all four plugins — including the ones that genuinely
  should look like form fields.
- **Menu label and indicator label are separate.** The menu has to say
  "Русский"; the indicator has room for "RU". A two-letter code is in
  fact the *better* indicator here, because it stays legible whatever
  script the chosen language is written in.

Each selector reports the width its own widest value needs, rather than
being laid out against a constant sized for the longest item in some
other language.

## A room per exercise

`AbcTrainLookAndFeel::paintPanelBackground` has taken an optional tint
since ADR 019, and `tintForGame()` has existed since the home screen went
in — but nothing ever passed one to the other. The compiler had been
saying so as an unused-function warning; the audit is what made it
visible.

The training screen now tints its backdrop with the current exercise's
category colour (blue for frequency, amber for dynamics, green for
space, violet for character — the same four the home screen groups by).
The home screen stays neutral, because nine tints at once is noise rather
than orientation.

The mix strength is 11% dark / 16% light, unchanged from ADR 019's
values. The reference trainers get "each training is its own room" from
whole different background images; a hint of hue in an existing gradient
buys most of that feeling without touching contrast or asking anyone to
ship nine background assets.
