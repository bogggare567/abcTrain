# Approved look — September 2026

The author reviewed these mock-ups on the design canvas (page «Как понял
комментарии») on 2026-09-24 and asked for them to be built. They are the
target, not a picture of the current build; each screen is replaced by a
real render once it is implemented.

| Screen | What was asked | Picture |
|---|---|---|
| Welcome | two steps: what is inside (Trainings, Studio, Live "soon"), then an account step that says it comes with Live; "Continue without an account" is the main button | `Welcome1.png`, `Welcome2.png` |
| Exercises | **keep the screens as they are.** Only the hint button ("Narrow the scale" / "Show the sound") moves up into the answer row, level with the A/B pair. The compression hint opens in the free band at the bottom, not full width | `HintCompression.png` (placement only) |
| Run results | rounds, accuracy "11 of 12", median error against the band, threshold step; one cell per round with its error; ranges as hit/miss counts; "Home" without "<" | `Results.png` |
| Training sounds | a DAW-like browser: waveform and preview per clip, the chosen clip large with a playhead, "all clips / this clip only" | `Sounds.png` |
| Settings | Pro mode is one switch at the bottom of the left rail | `SettingsMode.png` |
| Hearing | calibration fader full width, 60–110 dB(A) scale, −/+ by 1 dB | `Hearing.png` |
| Learner EQ | parametric curve with nodes over an analyser; instrument zones (body, boxy, click); a lesson panel with the steps of a real task; instrument chips | `LearnerEQ.png` |
| Learner Comp | transfer curve with the live level dot beside in/out waveforms with the gain-reduction line; a row of the author's pots; material and "start from" chips | `LearnerComp.png` |
| Learner Verb | the room the knobs describe (size, distance, reflections) beside direct / early reflections / tail with RT60; each knob says what it means for the room; lessons incl. ducking | `LearnerVerb.png` |

Knobs everywhere are vector drawings of the author's potentiometers
(`master-pot/`, his own): knurled black skirt, silver ring, a cap in the
family colour, white pointer.

## Status — built 2026-09-24

All of the above is in the code. `tools/EditorSnapshots` renders each one
(names in brackets), and those renders, not the mock-ups, are now the
reference:

- Welcome, two steps (`EarTrainer-Welcome`, `EarTrainer-WelcomeAccount`).
  The sign-in controls are real and disabled; nothing connects anywhere.
- Hint button in the answer row, compression hint in the free band
  (`EarTrainer-Hint*`).
- Run results (`EarTrainer-Results`).
- Training sounds as a browser with waveforms and click-to-hear
  (`EarTrainer-Sounds`, `EarTrainer-SoundClips`); preview plays through
  `shared/audio/ClipPreview.h`, only while no exercise is sounding.
- Pro mode switch, full-width calibration fader (`EarTrainer-Settings*`).
- Knobs: the author's pot, drawn in `AbcTrainLookAndFeel::drawRotarySlider`.
- Learner Comp, EQ, Verb (`EarTrainer-Studio*`, `LearnerEQ-Kick`, `LearnerComp`,
  `LearnerVerb`): toolbar of chips (material / instrument / type), no
  panel captions, "Start from" chips; Comp's GR trace and standing meter;
  EQ's instrument maps and lesson panel (`LearnerEQ/Source/InstrumentMaps.h`);
  Verb's room view and knob notes (`LearnerVerb/Source/RoomView.h`).

Differences from the mock-ups, on purpose: the material stays a dropdown
when the chips do not fit beside the instrument/type row; "Modules" is the
existing name for the lessons button; the EQ lesson ticks a step when the
curve is shaped that way - it never asks whether it sounds right (ADR 041).
