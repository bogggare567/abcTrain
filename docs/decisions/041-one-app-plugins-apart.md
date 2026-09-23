# 041 — One app, the plugins apart

**Status:** accepted (in progress — see "Stages")
**Date:** 2026-09-23

## What was there

Four products, each built as VST3, AU and a standalone app: twelve files
to install, four icons in Applications, four audio-device windows, four
update checks. The Learner standalones were the plugin in JUCE's stock
wrapper — a window with an audio-settings button and nothing to play
unless you had an interface input or picked a library clip, which the
author had to point at the same sound library in each of them separately.

## The decision

The author's call, 2026-09-23:

- **abcTrain is one app.** The trainer (`EarTrainer` target) is built as a
  standalone only. The product is the app, not a plugin, so the VST3/AU
  of the trainer is dropped; the few sessions that had it inserted open
  without it.
- **The Learner EQ / Comp / Verb are plugins only** (VST3, AU), and they
  also run **inside the app, in a Studio tab**.
- **"Check by ear" leaves the Learner modules.** Naming a hidden setting by
  ear is a trainer task; a processor you are learning to use should teach
  what its controls do. A module is now *watch → try → Done*.
- **The first launch gets an account step marked "soon"**; accounts
  themselves arrive with the Live server (see
  [design/education-and-live.md](../design/education-and-live.md)). Until
  then the app stays offline.

## How the Studio is built

- Each Learner's processor and editor is an INTERFACE library
  (`abc_learner_eq`, `abc_learner_comp`, `abc_learner_verb`,
  together `abc_learners`) without its `PluginEntry.cpp` — the one file
  that defines `createPluginFilter()`. The plugin targets link one each
  plus their entry; the app, the tests and the editor tools link all
  three. One list of sources, so the plugin and the Studio cannot drift.
- `EarTrainerProcessor` owns the three Learner processors, prepares all
  three, and while the Studio is open runs the chosen one in place of the
  trainer's signal: its input is the interface input or its own practice
  source, exactly as in a DAW. After it, the same last two steps as the
  trainer — one output level and the A-weighted hearing meter, because an
  hour of EQ on headphones is an hour of sound. Switching is an atomic
  index, never a prepare on the audio thread.
- `StudioScreenComponent` is a page under the top bar like Settings. It
  owns only the editor on show: made when the tab opens, dropped when it
  closes, and closing sets the active effect to none so nothing plays
  under the menus. The editor is the plugin's own, with its resize corner
  switched off.
- The app's wrapper saves the three Learner states through
  `EarTrainerProcessor::get/setStateInformation`, so the Studio's knobs
  survive a restart.

## What it costs

- The Learner editor keeps its own header (theme, updates, language),
  which in the app duplicates the app's bar. Acceptable for this stage;
  the Learner rework will decide what a Learner shows when it is a page
  rather than a window.
- The app's settings and progress files are unchanged (`EarTrainer/` and
  `abcTrain/` under Application Support), so nobody loses progress.
- The graded check stays in the code (`TrainingModule`, `ModuleProgress`)
  — its tolerance rules are what a future trainer exercise on real
  EQ/Comp/Verb controls would use. Modules with a graded attempt saved
  from before count as done.

## Stages

1. ✓ Formats, installers, the Studio tab, check removed (this change).
2. Welcome with the account step, the sounds browser with waveforms and
   preview, the run results reworked — the app-wide screens.
3. The Learners reworked one by one — EQ, Comp, Verb — each first as a
   mock-up on the design canvas.
4. Later ([roadmap](../roadmap.md)): a mini-DAW inside the app; Live.
