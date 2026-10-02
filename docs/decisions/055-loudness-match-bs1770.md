# 055 — Loudness match: RMS or BS.1770, the player's choice — and what the difference is

Date: 2026-10-02. Status: accepted (Bogdan on t03: «и так и так на выбор и
исследуй»).

## Problem

Every A/B exercise levels the treated side against the untreated one
(`shared/audio/GainMatch`, "level is never the tell"). It matched plain RMS.
A LUFS meter weights energy by ITU-R BS.1770's K-curve: about +4 dB above
2 kHz and a steep cut below 40 Hz. If the two disagree by more than a level
difference the ear can catch (~0.5–1 dB on broadband material), RMS
matching could leave a top-end boost slightly louder than its untreated
side — a tell.

## Measured (tests/LoudnessMatchTest, `LOUDNESS_REPORT=1`)

How much the K-weighted match differs from the RMS match for a +6 dB bell
(Q 1.4) on our own beds, at 48 kHz, in dB:

| bed | 60 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 12k |
|---|---|---|---|---|---|---|---|---|---|
| pink noise | +0.43 | +0.22 | +0.11 | +0.05 | −0.13 | −0.56 | −0.81 | −0.77 | −0.64 |
| drum loop | +0.57 | −0.14 | −0.13 | −0.02 | −0.01 | −0.03 | −0.12 | −0.27 | −0.33 |
| chord | +0.06 | +0.22 | 0.00 | −0.08 | −0.13 | −0.17 | −0.07 | −0.02 | −0.01 |
| bass note | +0.62 | −0.46 | −0.51 | −0.31 | −0.15 | −0.07 | −0.02 | 0.00 | 0.00 |
| vocal | +0.07 | +0.10 | +0.29 | +0.15 | −0.13 | −0.32 | −0.15 | −0.06 | −0.02 |

Negative means BS.1770 turns the boosted side down further than RMS does.

**Conclusion.** In the middle of the spectrum the two agree within 0.3 dB —
below a level difference anyone would hear. They part ways at the edges:
up to 0.8 dB on a high boost over dense, bright material (pink noise), and
about 0.6 dB on a sub-bass boost. That is at the threshold of a level tell
and only for those bands, so RMS stays the default and BS.1770 is offered
for whoever trains on top-end and sub-bass moves with noise or full mixes.

## Decision

- `GainMatch::mode`: RMS (default) or BS.1770 — the K-weighting (both
  stages, coefficients per sample rate as in libebur128) before the mean
  square, ungated: the material is a loop that never falls silent, so a gate
  would never close.
- Used by every exercise that matches through GainMatch (EQ, Name the
  Range, Reverb, Delay, Stereo width) and by Compression's makeup. Distortion
  keeps its own absolute RMS target (it levels each side to a fixed RMS, not
  one to the other).
- Settings → Training → «Выравнивание громкости»: RMS / BS.1770. The next
  round is measured the new way.
- The test pins the weighting to the standard: +0.69 dB at 997 Hz (the
  −0.691 LUFS offset), ≈ +4 dB at 10 kHz, steep below 40 Hz, at 44.1, 48 and
  96 kHz.
