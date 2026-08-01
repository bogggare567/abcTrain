# The nine exercises

Grouped by the skill they build rather than by name. Each has its own level
(1-10), which rises from accurate answers rather than from volume.

There are two kinds of question here, and the level means a different thing
in each.

**Four exercises answer with a value on a ruler** - a frequency, a delay
time, a pan position, a number of decibels. There the level narrows the
**tolerance band**: how far off you may be and still be counted right.

**Five exercises answer by naming a thing.** There you are always given
**exactly two alternatives** - at every level, from your first round to
your last. The level never adds a third button. It decides *which* two you
get: level 1 offers a cathedral against a broom cupboard, level 10 offers
two things that genuinely take work to separate. And it decides how
textbook an example you hear - an easy round plays the clearest hall there
is, a hard one plays a hall small enough to almost be a chamber.

Two reasons for that. A third button makes a round harder by giving you
more to read and lowering your odds of a lucky guess, and neither of those
is your ear getting better. And a fixed 50/50 means a streak of six says
the same thing at level 9 as it did at level 2.

## Frequency

### Find the frequency
A peak filter boosts or cuts a frequency drawn anywhere between 100 Hz and
12.8 kHz - not one of eight fixed octave centres, because fixed points get
memorised as positions rather than heard as frequencies. Answered on a
**log scale**. Tolerance is in **octaves** (±1.0 → ±0.35), so the slack is
the same ratio at 200 Hz as at 8 kHz.

### Name the range
The same idea, but you name the *range* - sub-bass, bass, low-mids, mids,
high-mids, presence, air - and you get two of the seven to choose between.
The boosted frequency moves around inside the range each round, and how far
it may wander is the level's business: an easy round lifts the middle of
Bass, a hard one lifts the boundary where Bass stops being Bass.

The width of the lift moves too. A broad one is what a whole named range
sounds like; a narrow one is a single tone that happens to live inside it,
which is what you are really hunting when you chase a resonance.

## Dynamics

### Guess the compression
A repeating percussive burst through a compressor, and two of three
settings - weak, medium, strong - to choose between. Makeup gain is matched
per setting so loudness alone is not a tell. Higher levels both bring the
three settings closer together and make it likelier that the two you are
offered are neighbours rather than the two extremes.

### Guess the gain change
A level offset between −9 and +9 dB. Answered on a linear scale, because dB
is already the perceptual unit. Tolerance stops narrowing at **1 dB** on
purpose: below roughly that, a level difference is not reliably audible at
all, and a tighter band would test luck.

## Space and stereo

### Guess the reverb
Room, chamber, hall, plate or spring - two of them per round. Each type is
a **family** of several genuinely different spaces rather than one setting:
a tiled booth and a big live room are both rooms, and someone who can only
recognise one of them has not learned what a room sounds like.

Spring sits outside that. Its character is a *mechanism*, not a size, so it
is unmistakable next to a hall and a real question next to a plate - two
pieces of metal being excited rather than air in a space. You will meet it
at both ends of the difficulty range for exactly that reason. It is built
as a cascade of resonant allpass filters rather than a room simulation,
because a Freeverb-style algorithm does not produce the metallic boing a
spring tank has.

### Guess the delay time
Any time between 20 and 640 ms. Answered on a **log scale**, with tolerance
as a **ratio** (±35% → ±13%) - being 20 ms out at 40 ms and at 500 ms are
completely different mistakes.

### Guess the pan position
Equal-power panning anywhere between hard left and hard right. Reads as
`C`, `L86`, `R40`. Linear scale, because equal-power panning already makes
perceived position track the control linearly.

### Guess the stereo width
Mid/side processing on two decorrelated noise sources, and two of four
named widths to choose between.

Width is a single number, so "a family of widths" would just be the
neighbouring answer. What varies instead is *how* the width is arrived at:
how much of the low end is left in the middle. A mix that widens everything
and a mix that keeps everything under 150 Hz centred can carry the same
nominal width and not sound like it - and the width you notice is mostly
the width above the bass. Hearing past that is the exercise.

> This one **always uses pink noise**, even when you have your own audio
> selected. Imported clips are downmixed to mono when loaded, and a mono
> signal has no side channel to widen. A known limit, not a bug you found.

## Character

### Guess the distortion
Soft clipping, hard clipping, tape saturation or overdrive - two per round.
Each has a family of voicings behind it, differing in how hard they are
driven, how much top is rolled off afterwards, and how lopsided the knee
is. The borderline tape is bright enough to almost be a soft clip; the
borderline overdrive is symmetric enough to almost be one too. Those turn
up as you climb.

Every voicing is **measured** and levelled to the same loudness before you
hear it, rather than matched by ear. A family that varies the drive varies
the volume, and a round you can win by noticing which one is louder is not
a round about distortion.
