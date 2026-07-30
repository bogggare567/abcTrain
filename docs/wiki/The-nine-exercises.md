# The nine exercises

Grouped by the skill they build rather than by name. Each has its own level
(1-10) which rises from accurate answers, not from volume - and each level
narrows the tolerance band, so "level 6" means something specific about
your ear rather than about your patience.

## Frequency

### Find the frequency
A peak filter boosts or cuts a frequency drawn anywhere between 100 Hz and
12.8 kHz - not one of eight fixed octave centres, because fixed points get
memorised as positions rather than heard as frequencies. Answered on a
**log scale**. Tolerance is in **octaves** (±1.0 → ±0.35), so the slack is
the same ratio at 200 Hz as at 8 kHz.

### Name the range
The same idea, but you name the *range* - sub-bass, bass, low-mids, mids,
high-mids, presence, air. The boosted frequency moves around inside the
range each round, so you learn where its boundaries actually are.

## Dynamics

### Guess the compression
A repeating percussive burst through a compressor at one of three settings:
weak, medium, strong. Makeup gain is matched per setting so loudness alone
is not a tell. Higher levels bring the three settings closer together.

### Guess the gain change
A level offset between −9 and +9 dB. Answered on a linear scale, because dB
is already the perceptual unit. Tolerance stops narrowing at **1 dB** on
purpose: below roughly that, a level difference is not reliably audible at
all, and a tighter band would test luck.

## Space and stereo

### Guess the reverb
Room, hall, plate or spring. Two types at low levels, four at high. Spring
is built as a cascade of resonant allpass filters rather than a room
simulation, because a Freeverb-style algorithm does not produce the
metallic boing a spring tank has.

### Guess the delay time
Any time between 20 and 640 ms. Answered on a **log scale**, with tolerance
as a **ratio** (±35% → ±13%) - being 20 ms out at 40 ms and at 500 ms are
completely different mistakes.

### Guess the pan position
Equal-power panning anywhere between hard left and hard right. Reads as
`C`, `L86`, `R40`. Linear scale, because equal-power panning already makes
perceived position track the control linearly.

### Guess the stereo width
Mid/side processing on two decorrelated noise sources. Four named widths.

> This one **always uses pink noise**, even when you have your own audio
> selected. Imported clips are downmixed to mono when loaded, and a mono
> signal has no side channel to widen. A known limit, not a bug you found.

## Character

### Guess the distortion
Soft clip, hard clip, tape saturation or overdrive, each with its own
makeup gain. Higher levels reduce the drive rather than changing the types,
so the same four answers get harder to tell apart.
