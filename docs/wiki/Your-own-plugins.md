# Your own plugins

*[Русская версия](ru-Your-own-plugins)*

Studio → **Your plugins**. Train on the plugins installed on your
computer: your own compressor, equaliser, reverb. It works in the abcTrain
app on a computer: VST3 on macOS, Windows and Linux, AU on macOS. There is no such
mode on a phone: the plugins live on the computer.

## The list

On the left: this computer's plugins. They are found without being loaded,
so the list builds fast and a misbehaving plugin cannot crash it.

- **By purpose**: equalisers, dynamics, space (reverbs and delays),
  saturation, modulation, other. The group comes from the VST3's own
  description, or is guessed from the name.
- **A–Z** and **Vendor** sort it differently.
- A plugin's AU and VST3 are one row. The VST3 is loaded.
- Instruments are left out: they have nothing to do on a drum loop.

Click a plugin: it loads and its own window appears above. If the window
does not fit on the page, it opens beside the app. A practice drum loop
plays.

**Your setting is A.** Set the plugin up the way you like in its window.
The setting is kept: next time the plugin opens the way you left it.

## 1 · Lessons

A lesson is a few steps of text while the knobs travel in your plugin's
own window to the values it talks about: "Threshold -20 dB", "Attack
30 ms". One lesson works on any compressor: the knob is found by what its
name means, the value by what the plugin itself shows.

- Only the lessons the plugin has every knob for are listed.
- **Bypass** compares with the dry sound.
- **Back** returns the knobs to where the previous step had them.
- When a lesson ends, or is left half-way, the knobs go back to **your**
  setting.
- If the plugin cannot reach a lesson's value (a threshold that stops at
  -40 dB), the knob goes to the nearest one and a line under the text says
  so.
- On a multi-band equaliser a lesson turns one band. If that band is
  switched off in the plugin, switch it on - a lesson only turns knobs.

Finished lessons get a tick. The ticks are shared with the Studio's
Lessons window.

There are lessons for compressors (Threshold and Ratio, Attack and
Release, Mix), equalisers (sweep and cut, wide and narrow Q), reverbs
(Decay and Mix, Pre-delay), saturators (Drive and Mix), delays (Time and
Feedback) and modulation (Rate and Depth).

## 2 · Get to know it

A card for every knob the glossary knows (about thirty common names:
Threshold, Ratio, Attack, Decay, Pre-delay, Mix, Drive…): what it does and
what to listen for.

- **Less / More**: your setting with this knob at 15% and 85% of its
  travel, labelled in the plugin's own units.
- **Sweep**: the knob travels its whole range and comes back.
- **Your setting**: back to A.

Identical knobs of different bands (Pro-Q's 24 bands) are one card. Knobs
without a description come last: turn them in the plugin's window.

## 3 · Guess

B is a copy of your setting with one knob moved. The options are that
knob's values in the plugin's own words. Pressing an option is the answer:
there is no Submit button, as everywhere in the trainer.

- **A**, **B**, **Dry** switch what you hear.
- The knobs asked about are ticked below - by default the ones you listened
  to in the "Get to know it" step.
- If moving a knob is inaudible with your setting, the round is skipped and
  the app says which knob it skipped.

## Keys

Settings → Keys → Studio: A, B, dry, next. In the lessons step "next"
turns the page, "dry" compares, "A" goes a step back.

## When something is wrong

- **The plugin did not load**: the reason is under the list. Some AUs do
  not survive two copies at once, which is why the VST3 is loaded first.
- **A plugin is missing**: press "Look for plugins". The standard VST3 and
  AU folders are searched.
- **No lesson is listed**: the plugin lacks one of the lesson's knobs, or
  is in another group. The knobs one by one are in the "Get to know it" step.
