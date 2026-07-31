// The three exercises the browser demo can honestly reproduce.
//
// Each one mirrors its plugin counterpart exactly: the same axis, the same
// draw range, and a tolerance in the same *unit* the trainer uses - which
// is the part that matters. An accept band measured in octaves is the same
// slack at 200 Hz as at 8 kHz; one measured in hertz is not, and grading a
// frequency guess in hertz would teach a different skill from the one the
// product teaches.
//
// Only three, and deliberately. Compression, reverb type and delay need a
// repeating percussive bed and several seconds of listening per round;
// they work in the plugin and would be a bad first impression in a browser
// tab. Stereo width needs two decorrelated noise sources, which is the
// same reason it still trains on pink noise inside the trainer.

const PINK = { b0: 0, b1: 0, b2: 0 };

// Paul Kellet's economy pink-noise filter - the same algorithm as
// shared/PinkNoiseGenerator.h, so the demo and the plugin are listening to
// the same signal rather than to two different approximations of it.
export function fillPinkNoise(data, amplitude = 0.12) {
  let { b0, b1, b2 } = PINK;

  for (let i = 0; i < data.length; i += 1) {
    const white = Math.random() * 2 - 1;
    b0 = 0.99765 * b0 + white * 0.099046;
    b1 = 0.963 * b1 + white * 0.2965164;
    b2 = 0.57 * b2 + white * 1.0526913;
    data[i] = (b0 + b1 + b2 + white * 0.1848) * amplitude;
  }
}

const formatHz = (hz) =>
  hz >= 1000 ? `${(hz / 1000).toFixed(hz >= 10000 ? 0 : 1)}k Hz` : `${Math.round(hz)} Hz`;

export const EXERCISES = [
  {
    key: 'band',
    name: 'Guess the Band',
    family: 'freq',
    // Half an octave of margin at each end, so the extremes sit inside the
    // axis rather than on its edge - the plugin does the same.
    axisMin: 100 / Math.SQRT2,
    axisMax: 12800 * Math.SQRT2,
    log: true,
    tolerance: 1.0,           // octaves, level 1
    toleranceLabel: '±1 octave',
    ticks: [100, 200, 400, 800, 1600, 3200, 6400, 12800],
    format: formatHz,
    axisNote: '100 Hz — 12.8 kHz, log scale',
    prompt: 'Where is the boost?',

    // Distance in the exercise's own unit, which is what makes the grading
    // comparable to the plugin's.
    error: (guess, target) => Math.abs(Math.log2(guess / target)),
    describeMiss: (guess, target, err) =>
      `${err.toFixed(1)} octaves too ${guess > target ? 'high' : 'low'}`,

    build(ctx, target) {
      const filter = ctx.createBiquadFilter();
      filter.type = 'peaking';
      filter.frequency.value = target;
      filter.Q.value = 3;
      filter.gain.value = 9;
      return { input: filter, output: filter };
    },
  },

  {
    key: 'gain',
    name: 'Guess the Gain Change',
    family: 'dyn',
    axisMin: -9,
    axisMax: 9,
    log: false,
    // dB is already the perceptual unit, so the band is a fixed distance.
    // It stops at ±2.5 for level 1 and never goes below 1 dB even at level
    // 10 - below roughly that, a level difference stops being reliably
    // audible at all, and a tighter band would test luck.
    tolerance: 2.5,
    toleranceLabel: '±2.5 dB',
    ticks: [-9, -6, -3, 0, 3, 6, 9],
    format: (db) => `${db > 0 ? '+' : ''}${db.toFixed(1)} dB`,
    axisNote: '−9 dB — +9 dB',
    prompt: 'How big is the level change?',

    error: (guess, target) => Math.abs(guess - target),
    describeMiss: (guess, target, err) =>
      `${err.toFixed(1)} dB too ${guess > target ? 'loud' : 'quiet'}`,

    build(ctx, target) {
      const gain = ctx.createGain();
      gain.gain.value = Math.pow(10, target / 20);
      return { input: gain, output: gain };
    },
  },

  {
    key: 'pan',
    name: 'Guess the Pan Position',
    family: 'space',
    axisMin: -1,
    axisMax: 1,
    log: false,
    tolerance: 0.35,
    toleranceLabel: '±35% of the field',
    ticks: [-1, -0.5, 0, 0.5, 1],
    format: (p) => {
      const amount = Math.round(Math.abs(p) * 100);
      if (amount < 3) return 'C';
      return `${p < 0 ? 'L' : 'R'}${amount}`;
    },
    axisNote: 'hard left — centre — hard right',
    prompt: 'Where is it in the field?',

    error: (guess, target) => Math.abs(guess - target),
    describeMiss: (guess, target, err) =>
      `${Math.round(err * 100)}% too far ${guess > target ? 'right' : 'left'}`,

    build(ctx, target) {
      // Equal-power law, the same as PanGame's: gainL = cos(theta),
      // gainR = sin(theta). Loudness-equalised for free, so position is
      // the only cue - which is the whole point of the exercise.
      const panner = ctx.createStereoPanner();
      panner.pan.value = Math.max(-1, Math.min(1, target));
      return { input: panner, output: panner };
    },
  },
];

export const toNormalised = (ex, value) =>
  ex.log
    ? Math.log(value / ex.axisMin) / Math.log(ex.axisMax / ex.axisMin)
    : (value - ex.axisMin) / (ex.axisMax - ex.axisMin);

export const fromNormalised = (ex, t) => {
  const clamped = Math.max(0, Math.min(1, t));
  return ex.log
    ? ex.axisMin * Math.pow(ex.axisMax / ex.axisMin, clamped)
    : ex.axisMin + (ex.axisMax - ex.axisMin) * clamped;
};

// Drawn away from the very ends of the axis: a target you can find by
// dragging to the edge is a target found by feel rather than by ear.
export const drawTarget = (ex) => fromNormalised(ex, Math.random() * 0.84 + 0.08);

// The band, expressed on the 0..1 axis so the display can draw it. For a
// log axis a tolerance in octaves is a constant *width* on screen, which
// is exactly why the axis is log in the first place.
export const toleranceOnAxis = (ex) => {
  if (ex.log) {
    const decades = Math.log2(ex.axisMax / ex.axisMin);
    return ex.tolerance / decades;
  }

  return ex.tolerance / (ex.axisMax - ex.axisMin);
};
