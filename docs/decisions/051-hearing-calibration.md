# 051 — Hearing calibration: a profile per person × headphones, compensated in the Studio only

Date: 2026-10-02. Status: accepted (Bogdan: «выглядит правильно; не грузить
интерфейс; предлагать тест перед тренировкой или когда калибровки нет;
должно быть в настройках… профили человек × наушники — пока так»).

## Problem

Two ears are rarely equal, and a pair of headphones is rarely equal on both
sides. Somebody whose right ear is 10 dB down above 6 kHz hears a mix in
the Studio tilted to the left and dull on one side, and learns an EQ move
against that picture. HEARS Perfection and the like solve this with a
short threshold test and a correction curve. The app had nothing: the
hearing guard (ADR 036) counts dose, it does not know the ears.

## Decision

**The test** — `Source/AudiometryProcedure`, pure logic, run by
`Source/HearingTestScreen` with `Source/ProbeTone` in the app processor.
It is the British Society of Audiology's *Recommended Procedure: Pure-tone
air-conduction and bone-conduction threshold audiometry* (2018), i.e. the
modified Hughson–Westlake method of ISO 8253-1:

- each ear in turn, 1, 2, 3, 4, 6, 8 kHz, then 1 kHz again, then 500 and
  250 Hz;
- every frequency starts at a comfortable −40 dBFS; "heard" → 10 dB
  quieter, "not heard" → 5 dB louder;
- the threshold is the quietest level heard on two ascending presentations
  (an ascending presentation is one reached by going up after "not heard");
- the stimulus is three 250 ms pulses with 250 ms gaps, 20 ms raised-cosine
  ramps (ISO 8253-1 asks for 20–50 ms so the switching click is not what is
  heard), after a random 1–3 s pause, in one ear only;
- levels are dBFS sine peak at the app's output, never above −10 and never
  below −100; not heard at −10 is recorded as "not heard", not chased.

Two checks a clinic gets from watching the person and a self-run test does
not: **catch trials** (about one presentation in six is silence; two
"heard" on silence and that ear is measured again from the start, two more
and it is marked unreliable) and the **1 kHz retest** (more than 5 dB from
the first 1 kHz → the result says so and suggests taking it again). The
answer buttons open only when the presentation is over: a button pressed
during the pause is a guess. Exactly two answers, «Слышно» / «Не слышно»,
and a pause. While the test plays the probe replaces every other sound,
the output level included — the same rule as the calibration noise.

**What it measures.** Not dB HL. Nobody here knows what a headphone does
with a full-scale sine, so the thresholds are relative: this person, this
pair, this volume. What *is* reliable is how the two ears compare, and
that is all the compensation uses. The page says so («Не медицинский
тест…»).

**Repeatability.** With 5 dB steps the method's own test-retest is ±5 dB.
A simulated listener whose responses follow a realistic psychometric
function (logistic, 1.5 dB spread) lands within 5 dB of the true 50 % point
89 % of the time, never further than 8.5 dB, reading on average 2.7 dB high
(ascending methods do); a perfectly consistent listener is always within
5 dB (`tests/HearingCalibrationTest`). That is why differences under 5 dB
are not corrected.

**The compensation** — `shared/audio/HearingProfile`,
`shared/dsp/HearingCompensation.h`. Per frequency, D = this ear's threshold
minus the better ear's. Boost = amount × max(0, D − 5 dB), never more than
+12 dB, never a cut. Default amount 50 %: the half-gain rule of hearing-aid
fitting (Lybarger; NAL-R and its successors sit near it for mild losses) —
recruitment makes a threshold loss smaller at working level than at
threshold, so making up all of it would be too much. The boosts are matched
bells (the same `EQCoefficients::makeMatchedBell` Learner EQ runs), Q 1.4,
left set on channel 0, right on channel 1. Neighbouring bells above 2 kHz
are closer than an octave and add up, so the bells' own gains are solved
(a small linear system, re-linearised a few times) so the *combined* curve
meets each target within 0.5 dB; a band whose solution would be negative
stays at zero rather than cut. Coefficients are designed off the audio
thread and handed over through a triple buffer.

**Where it applies: the Studio output only**, after the Learner and before
the output level, and only with a profile active and «Применять коррекцию
в Студии» on. Not in the exercises — they measure the very hearing it
would correct, and the staircase must see the ears as they are. Not in the
plugins in a DAW — a bounce would carry one listener's ears into the mix;
a monitor correction belongs in the monitor path, which the Studio is and
a DAW insert is not. Learner EQ shows the active correction as a thin
dashed line in the ears' audiogram colours (left blue, right red), in the
Studio captioned with the pair and in a DAW «коррекция слуха: только в
Студии».

**Profiles** are person × headphones, as JSON in a file of their own,
`hearing.settings` in the shared `abcTrain` folder (next to the library's
settings, which every plugin already opens). Not inside that file: each
open `PropertiesFile` writes back everything it loaded, so a Learner in a
DAW saving its practice source would have restored the profile list as it
was when the DAW started. The app is the only writer; Learner EQ reloads
when the file's date changes.

**Where it is in the interface.** Settings → Слух: one row «Профиль слуха»
(the saved pairs, «Пройти тест») and one switch «Применять коррекцию в
Студии». Before an exercise, with no profile yet, the existing hearing
strip offers «Проверить слух в этих наушниках? Около 8 минут на оба уха.»
— once per launch, never into a Survival or Blitz run, «Не сейчас» holds
for seven days, and it never blocks the round.

## Sources

- British Society of Audiology (2018). *Recommended Procedure: Pure-tone
  air-conduction and bone-conduction threshold audiometry with and without
  masking.*
- ISO 8253-1:2010. *Acoustics — Audiometric test methods — Part 1:
  Pure-tone air and bone conduction audiometry.*
- Carhart, R., Jerger, J. (1959). Preferred method for clinical
  determination of pure-tone thresholds. *JSHD* 24 — the Hughson–Westlake
  modification.
- Lybarger, S. (1944) — the half-gain rule; Byrne, D., Dillon, H. (1986).
  The National Acoustic Laboratories' (NAL) new procedure for selecting the
  gain and frequency response of a hearing aid. *Ear and Hearing* 7.
- Vicanek, M. (2016). *Matched Second Order Digital Filters* — the bell.

## Limits

- Relative thresholds only; a change of volume or headphones invalidates
  a profile. No masking, so a large asymmetry can be the better ear
  hearing across (cross-hearing through headphones starts around 40 dB) —
  the cap of +12 dB keeps that from becoming a large wrong boost.
- No correction above 8 kHz or below 250 Hz (not measured).
- Not a medical test and does not say anything about hearing loss.
