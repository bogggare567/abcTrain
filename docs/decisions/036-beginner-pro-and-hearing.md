# 036 — Beginner / Pro settings, and looking after the ears

**Status:** accepted
**Date:** 2026-09-19

The user asked for two things: as many settings as make sense, but with a
beginner mode where everything is standard; and warnings based on the WHO
safe-listening norms, **on by default and optional**.

## Two modes, one list

`Source/TrainerSettings.h` is the whole list: every setting's key, default,
allowed values and whether a beginner sees it. **Beginner** answers every
training rule with its default; **Pro** honours what is stored. Switching
back to Beginner erases nothing — the Pro values stay in the file and come
back with Pro. Settings about the *person* (hearing protection on/off, the
calibration) are honoured in both modes.

In Beginner the Training rows are still shown, greyed, holding the defaults:
what *can* be tuned is visible and one click away, without being a thing to
fiddle with in the first week. A rule you can change is a rule you start
negotiating with, and someone who does not yet know what "three in a row"
is for cannot choose a better number.

What Pro opens, and why each one exists:

| setting | values | default | why tunable |
|---|---|---|---|
| step harder after | 2 / 3 / 4 right in a row | 3 | the staircase's target: 71% / 79% / 84% correct (Levitt) |
| pause after an answer | short / normal / long | normal | reading speed differs |
| hints | off / on | on | "ears only" training |
| Survival and Blitz | after a streak / right away | after a streak | an experienced engineer needs no walk-in |
| lives in Survival | 1 / 3 / 5 | 3 | |
| Blitz clock | 60 / 90 / 120 / 180 s | 90 | |
| Blitz penalty | none / 5 / 10 s | 5 | |
| break reminder | off / 30 / 45 / 60 / 90 min | 60 | |
| tired-ear hint | off / on | on | |
| weekly limit | 80 / 75 dB(A) | 80 | H.870 mode 1 / mode 2 |

Run rules change at the next run, never mid-run (a Blitz clock that grew by
a minute half-way through is a score that means nothing); Practice picks
them up at once. The hint button disappears when hints are off, rather than
staying as a control that always says no.

## Hearing

What the software can know is dBFS; what the ear receives is dB SPL, which
depends on the monitor knob. So the feature has two tiers.

**Without calibration — time.** After 60 minutes of sound without ten quiet
minutes (WHO's advice for long listening), a strip under the navigation bar
says so, with "Take a break" (goes home, which silences the signal) and "In
15 min". Ten quiet minutes anywhere count as the break. And the trainer
notices **fatigue** the way only a trainer can: 30+ minutes into a session,
a staircase two steps below that session's own best in the same exercise is
the ear tiring, and the strip says exactly that, without a word about doing
badly.

**With calibration — the weekly dose.** The player plays our calibration
noise (pink, −20 dBFS RMS), measures it at the listening position with any
dB(A) meter — WHO itself points to phone apps such as NIOSH SLM — and
enters the number. `shared/AWeightedMeter` measures every second of output
through the IEC 61672 A curve (six first-order sections from the analogue
poles; `HearingGuardTest` checks it at 100 Hz, 1 kHz, 4 kHz and 10 kHz);
`HearingGuard` converts with the one offset and adds Pa²·h per day. The
limit is ITU-T H.870's: 1.6 Pa²·h a week (80 dB(A) for 40 h), or 0.51 (75
dB(A)) in the stricter mode; equal energy, so +3 dB halves the time. Half
and the full allowance are each announced once, when crossed. The navigation
bar shows the week as ten segments, warm past half, never red: this is
information about a week, not an alarm about now.

Gigs and mixing days can be typed in (hours × dB(A)) so the week means the
week — for a sound engineer the trainer is the smallest part of it.

**What it deliberately does not claim.** It is an estimate of the trainer's
own share of the week, plus what was typed in. Turning the monitor knob
after calibrating makes the number wrong, and the page says so beside it.
It does not claim H.870 conformance — that requires measurement to EN 50332
on specific headphones. Nothing ever blocks training: H.870 itself only asks
that the listener be told.

## Found on the way

`tools/EditorSnapshots` and `tools/ClickMap` backed up
`~/.config/abcTrain/abcTrain.settings` before touching settings — but the
app's file lives wherever `LocalisationManager::makeDefaultOptions()` says,
which on Linux is `~/abcTrain/`. The "backup" protected a file that did not
exist, and both tools wrote into the real one. Found because the new
settings seams leaked Pro mode and a week of dose into the next render. Both
now ask `makeDefaultOptions()` for the path.
