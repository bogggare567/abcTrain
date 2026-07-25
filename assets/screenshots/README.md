# Screenshots needed here

This project's sandbox can't render or capture a real JUCE window (no
display), so no screenshots exist yet — `README.md`'s "Demo" section
currently just describes the look in text instead
(see [decisions/009](../../docs/decisions/009-look-and-feel.md)).

If you're picking this up with an actual display available, these are the
screenshots that would make the biggest difference, in priority order:

1. **EarTrainer** — the main game screen (game selector, choice buttons,
   score/level/progress bar/streak) mid-round.
2. **LearnerEQ** — the full editor: spectrum + response curve, waveform,
   4 band knobs, with a band's frequency being dragged (so the
   highlighted-region + guide-label tooltip both show).
3. **LearnerComp** — spectrum, waveform with a visible red gain-reduction
   highlight, GR/peak meters, knobs, one of the 4 presets selected.
4. **LearnerVerb** — same shape as LearnerComp, ideally on the "Concert
   Hall" or "Spring Tank" preset (visually/sonically the most distinct).
5. **The "Lesson" overlay** (any one plugin) mid-walkthrough, showing the
   step text and progress indicator.
6. **The language picker** (EarTrainer) with the dropdown open, showing
   all 12 language names.

## Format

- PNG, actual pixel size (no upscaling) at the plugin's real editor size
  (see each editor's `setSize()` call for exact dimensions).
- Name them `eartrainer.png`, `learnereq.png`, `learnercomp.png`,
  `learnerverb.png`, `lesson-overlay.png`, `language-picker.png` so
  `README.md` can reference them directly once they exist.
