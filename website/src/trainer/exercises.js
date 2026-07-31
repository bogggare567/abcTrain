// All nine exercises, as the plugin has them.
//
// This is not a "web version" with its own ideas. Every axis, draw range,
// tolerance and unit is the plugin's, and where a number could drift it is
// read out of the C++ instead of typed here (see tools/sync-from-plugin.mjs).
// The four skill families and their colours are the same too, because the
// grouping is the product's spine and a demo that regrouped them would be
// teaching something else.
//
// The five categorical exercises (compression, reverb, distortion, width)
// answer with zones; the four continuous ones (band, gain, pan, delay)
// answer on a ruler. That split is the plugin's, from ADR 020.

import facts from '../generated/plugin-facts.json';

// Paul Kellet's economy pink-noise filter - the same algorithm as
// shared/PinkNoiseGenerator.h, so this and the plugin listen to the same
// signal rather than to two approximations of it.
export function fillPink(data, amplitude = 0.12, seedState = null) {
  let b0 = 0, b1 = 0, b2 = 0;
  if (seedState) ({ b0, b1, b2 } = seedState);

  for (let i = 0; i < data.length; i += 1) {
    const white = Math.random() * 2 - 1;
    b0 = 0.99765 * b0 + white * 0.099046;
    b1 = 0.963 * b1 + white * 0.2965164;
    b2 = 0.57 * b2 + white * 1.0526913;
    data[i] = (b0 + b1 + b2 + white * 0.1848) * amplitude;
  }
}

export function pinkBuffer(ctx, seconds = 2.4, channels = 1) {
  const buffer = ctx.createBuffer(channels, Math.floor(ctx.sampleRate * seconds), ctx.sampleRate);
  for (let c = 0; c < channels; c += 1) fillPink(buffer.getChannelData(c));
  return buffer;
}

// A repeating percussive burst, for the exercises whose answer only exists
// in how a transient is treated. The plugin generates these too rather
// than shipping samples - a fixed set of files is a fixed set of answers.
export function burstBuffer(ctx, periodSeconds = 1.4, repeats = 4) {
  const total = Math.floor(ctx.sampleRate * periodSeconds * repeats);
  const buffer = ctx.createBuffer(1, total, ctx.sampleRate);
  const data = buffer.getChannelData(0);
  const period = Math.floor(ctx.sampleRate * periodSeconds);
  const hit = Math.floor(ctx.sampleRate * 0.11);

  for (let r = 0; r < repeats; r += 1) {
    const start = r * period;
    for (let i = 0; i < hit && start + i < total; i += 1) {
      const env = Math.exp(-8 * (i / hit));
      data[start + i] = (Math.random() * 2 - 1) * env * 0.5;
    }
  }

  return buffer;
}

const hz = (v) => (v >= 1000 ? `${(v / 1000).toFixed(v >= 10000 ? 0 : 1)}k Hz` : `${Math.round(v)} Hz`);
const ms = (v) => `${Math.round(v)} ms`;

// Curve for a WaveShaper. Each type is the same shaping the plugin's
// DistortionGame uses, with its own makeup gain so loudness is never the
// tell.
function shaperCurve(kind, drive = 6) {
  const n = 1024;
  const curve = new Float32Array(n);

  for (let i = 0; i < n; i += 1) {
    const x = (i / (n - 1)) * 2 - 1;
    const d = x * drive;
    let y;

    if (kind === 'soft') y = Math.tanh(d) * 0.75;
    else if (kind === 'hard') y = Math.max(-0.7, Math.min(0.7, d)) * 1.05;
    else if (kind === 'tape') y = Math.tanh(d * 0.8) * 0.8;
    else y = (Math.tanh(d) + 0.25 * Math.tanh(d * 2) ** 2) * 0.62;   // asymmetric

    curve[i] = y;
  }

  return curve;
}

// A decaying-noise impulse response. Not a physical model of a room - the
// same "tuned by ear, not measured" approximation ADR 004 records for the
// plugin's own reverb.
function impulse(ctx, seconds, decay, damping) {
  const len = Math.floor(ctx.sampleRate * seconds);
  const ir = ctx.createBuffer(2, len, ctx.sampleRate);

  for (let c = 0; c < 2; c += 1) {
    const data = ir.getChannelData(c);
    let lp = 0;

    for (let i = 0; i < len; i += 1) {
      const env = Math.pow(1 - i / len, decay);
      const white = (Math.random() * 2 - 1) * env;
      // A one-pole lowpass over the tail: real surfaces eat highs faster
      // than lows, which is what stops it sounding metallic.
      lp += (white - lp) * (1 - damping);
      data[i] = lp;
    }
  }

  return ir;
}

const continuous = (o) => ({ kind: 'continuous', ...o });
const zoned = (o) => ({ kind: 'zoned', ...o });

export const EXERCISES = [
  continuous({
    key: 'band',
    name: 'Guess the Band',
    family: 'freq',
    instructions:
      'Listen, then drag along the scale to where you think the boost or cut is. ' +
      "You don't have to be exact - land inside the tolerance band and it counts.",
    axisMin: 100 / Math.SQRT2,
    axisMax: 12800 * Math.SQRT2,
    log: true,
    tolerance: facts.tolerances.band,
    ticks: facts.bandTicks,
    format: hz,
    before: 'EQ off',
    after: 'EQ on',
    error: (g, t) => Math.abs(Math.log2(g / t)),
    miss: (g, t, e) => `${e.toFixed(1)} octaves too ${g > t ? 'high' : 'low'}`,
    source: (ctx) => ({ buffer: pinkBuffer(ctx), loop: true }),
    build(ctx, target) {
      const f = ctx.createBiquadFilter();
      f.type = 'peaking';
      f.frequency.value = target;
      f.Q.value = 3;
      f.gain.value = 9;
      return { input: f, output: f };
    },
  }),

  zoned({
    key: 'compression',
    name: 'Guess the Compression',
    family: 'dyn',
    instructions:
      'A repeating hit through a compressor. Listen to how much the loud part is ' +
      'held back, then pick how hard it is being squeezed.',
    choices: ['Weak', 'Medium', 'Strong'],
    before: 'Comp Off',
    after: 'Comp On',
    source: (ctx) => ({ buffer: burstBuffer(ctx), loop: true }),
    build(ctx, index) {
      const presets = [
        { threshold: -12, ratio: 2, makeup: 1.05 },
        { threshold: -20, ratio: 5, makeup: 1.5 },
        { threshold: -28, ratio: 12, makeup: 2.4 },
      ];
      const p = presets[index];

      const comp = ctx.createDynamicsCompressor();
      comp.threshold.value = p.threshold;
      comp.ratio.value = p.ratio;
      comp.attack.value = 0.003;
      comp.release.value = 0.15;
      comp.knee.value = 6;

      // Makeup tuned by ear per preset, so loudness alone is not the tell -
      // the same reasoning as CompressionGame's own compensation.
      const makeup = ctx.createGain();
      makeup.gain.value = p.makeup;
      comp.connect(makeup);
      return { input: comp, output: makeup };
    },
  }),

  zoned({
    key: 'reverb',
    name: 'Guess the Reverb',
    family: 'space',
    instructions:
      'A hit in a space. Listen to the tail - how long it lasts, how bright it ' +
      'stays - and name the kind of room it is.',
    choices: ['Room', 'Hall', 'Plate', 'Spring'],
    before: 'Dry',
    after: 'With Reverb',
    source: (ctx) => ({ buffer: burstBuffer(ctx, 2.2, 3), loop: true }),
    build(ctx, index) {
      const settings = [
        { seconds: 0.7, decay: 2.2, damping: 0.55 },   // room
        { seconds: 2.6, decay: 1.6, damping: 0.35 },   // hall
        { seconds: 1.6, decay: 1.1, damping: 0.12 },   // plate: brighter, denser
        { seconds: 1.2, decay: 1.4, damping: 0.05 },   // spring: metallic
      ][index];

      const conv = ctx.createConvolver();
      conv.buffer = impulse(ctx, settings.seconds, settings.decay, settings.damping);

      const wet = ctx.createGain();
      wet.gain.value = 0.9;
      const dry = ctx.createGain();
      dry.gain.value = 0.7;
      const out = ctx.createGain();

      const input = ctx.createGain();
      input.connect(conv).connect(wet).connect(out);
      input.connect(dry).connect(out);
      return { input, output: out };
    },
  }),

  continuous({
    key: 'pan',
    name: 'Guess the Pan Position',
    family: 'space',
    instructions:
      'Equal-power panning, so loudness is the same wherever it sits. Only the ' +
      'position moves - drag to where you hear it.',
    axisMin: -1,
    axisMax: 1,
    log: false,
    tolerance: facts.tolerances.pan,
    ticks: [-1, -0.5, 0, 0.5, 1],
    format: (p) => {
      const amount = Math.round(Math.abs(p) * 100);
      return amount < 3 ? 'C' : `${p < 0 ? 'L' : 'R'}${amount}`;
    },
    before: 'Centred',
    after: 'Panned',
    error: (g, t) => Math.abs(g - t),
    miss: (g, t, e) => `${Math.round(e * 100)}% too far ${g > t ? 'right' : 'left'}`,
    source: (ctx) => ({ buffer: pinkBuffer(ctx), loop: true }),
    build(ctx, target) {
      const p = ctx.createStereoPanner();
      p.pan.value = Math.max(-1, Math.min(1, target));
      return { input: p, output: p };
    },
  }),

  continuous({
    key: 'delay',
    name: 'Guess the Delay Time',
    family: 'space',
    instructions:
      'One repeat, no feedback. Listen to the gap between the hit and its echo, ' +
      'then drag to that many milliseconds.',
    axisMin: 20,
    axisMax: 640,
    log: true,
    tolerance: 0.35,          // a *ratio*: 20 ms out at 40 ms and at 500 ms differ
    toleranceIsRatio: true,
    ticks: [20, 50, 100, 200, 400, 640],
    format: ms,
    before: 'Dry',
    after: 'With Echo',
    error: (g, t) => Math.abs(Math.log(g / t)),
    miss: (g, t) => `${Math.round(Math.abs(g - t))} ms too ${g > t ? 'long' : 'short'}`,
    source: (ctx) => ({ buffer: burstBuffer(ctx, 1.4, 4), loop: true }),
    build(ctx, target) {
      const input = ctx.createGain();
      const delay = ctx.createDelay(1.0);
      delay.delayTime.value = target / 1000;
      const wet = ctx.createGain();
      wet.gain.value = 0.5;
      const out = ctx.createGain();

      input.connect(out);
      input.connect(delay).connect(wet).connect(out);
      return { input, output: out };
    },
  }),

  zoned({
    key: 'distortion',
    name: 'Guess the Distortion',
    family: 'char',
    instructions:
      'Four kinds of clipping on the same noise. Listen to the character of the ' +
      'edge - soft and round, hard and buzzy, dulled, or asymmetric.',
    choices: ['Soft Clip', 'Hard Clip', 'Tape', 'Overdrive'],
    before: 'Clean',
    after: 'Driven',
    source: (ctx) => ({ buffer: pinkBuffer(ctx), loop: true }),
    build(ctx, index) {
      const kinds = ['soft', 'hard', 'tape', 'overdrive'];
      const shaper = ctx.createWaveShaper();
      shaper.curve = shaperCurve(kinds[index]);
      shaper.oversample = '4x';

      const out = ctx.createGain();
      out.gain.value = 0.6;

      // Tape rolls off the top after clipping, which is most of what makes
      // it sound like tape rather than like a fuzz box.
      if (kinds[index] === 'tape') {
        const lp = ctx.createBiquadFilter();
        lp.type = 'lowpass';
        lp.frequency.value = 4200;
        shaper.connect(lp).connect(out);
      } else {
        shaper.connect(out);
      }

      return { input: shaper, output: out };
    },
  }),

  zoned({
    key: 'width',
    name: 'Guess the Stereo Width',
    family: 'space',
    instructions:
      'Two independent noise sources, so there is a real side signal to widen. ' +
      'Listen to how far the sound spreads past the speakers.',
    choices: ['Narrow', 'Normal', 'Wide', 'Extra Wide'],
    before: 'Mono',
    after: 'Widened',
    stereoSource: true,
    source: (ctx) => ({ buffer: pinkBuffer(ctx, 2.4, 2), loop: true }),
    build(ctx, index) {
      // Mid/side: side *= width. A single mono source duplicated to both
      // channels would have no side signal at all, which is why the source
      // above is two independent noise channels.
      const width = [0.35, 1.0, 1.7, 2.4][index];
      const input = ctx.createGain();
      const splitter = ctx.createChannelSplitter(2);
      const merger = ctx.createChannelMerger(2);

      const midL = ctx.createGain(); const midR = ctx.createGain();
      const sideL = ctx.createGain(); const sideR = ctx.createGain();

      midL.gain.value = 0.5; midR.gain.value = 0.5;
      sideL.gain.value = width * 0.5; sideR.gain.value = -width * 0.5;

      input.connect(splitter);
      splitter.connect(midL, 0); splitter.connect(midR, 1);
      splitter.connect(sideL, 0); splitter.connect(sideR, 1);

      midL.connect(merger, 0, 0); sideL.connect(merger, 0, 0);
      midR.connect(merger, 0, 1); sideR.connect(merger, 0, 1);

      const out = ctx.createGain();
      out.gain.value = 0.8;
      merger.connect(out);
      return { input, output: out };
    },
  }),

  continuous({
    key: 'gain',
    name: 'Guess the Gain Change',
    family: 'dyn',
    instructions:
      'A level change on pink noise. dB is already the perceptual unit, so the ' +
      'scale is linear - drag to how big the change was.',
    axisMin: -9,
    axisMax: 9,
    log: false,
    tolerance: facts.tolerances.gain,
    ticks: [-9, -6, -3, 0, 3, 6, 9],
    format: (db) => `${db > 0 ? '+' : ''}${db.toFixed(1)} dB`,
    before: 'Reference',
    after: 'Changed',
    error: (g, t) => Math.abs(g - t),
    miss: (g, t, e) => `${e.toFixed(1)} dB too ${g > t ? 'loud' : 'quiet'}`,
    source: (ctx) => ({ buffer: pinkBuffer(ctx), loop: true }),
    build(ctx, target) {
      const g = ctx.createGain();
      g.gain.value = Math.pow(10, target / 20);
      return { input: g, output: g };
    },
  }),

  zoned({
    key: 'range',
    name: 'Name the Range',
    family: 'freq',
    instructions:
      'A boost somewhere inside one named range - the frequency moves each round, ' +
      'so you learn the range rather than one point in it.',
    choices: ['Sub-bass', 'Bass', 'Low-mids', 'Mids', 'High-mids', 'Presence', 'Air'],
    before: 'EQ off',
    after: 'EQ on',
    source: (ctx) => ({ buffer: pinkBuffer(ctx), loop: true }),
    build(ctx, index) {
      // The same seven ranges FrequencyGuide and FrequencyRangeGame use.
      const spans = [
        [20, 60], [60, 250], [250, 500], [500, 2000],
        [2000, 4000], [4000, 6000], [6000, 20000],
      ][index];

      // Log-uniform *within* the range, so the target moves each round.
      const freq = spans[0] * Math.pow(spans[1] / spans[0], Math.random());

      const f = ctx.createBiquadFilter();
      f.type = 'peaking';
      f.frequency.value = freq;
      f.Q.value = 2;
      f.gain.value = 9;
      return { input: f, output: f };
    },
  }),
];

export const FAMILY_LABEL = {
  freq: 'Frequency',
  dyn: 'Dynamics',
  space: 'Space & stereo',
  char: 'Character',
};

export const toNorm = (ex, v) =>
  ex.log
    ? Math.log(v / ex.axisMin) / Math.log(ex.axisMax / ex.axisMin)
    : (v - ex.axisMin) / (ex.axisMax - ex.axisMin);

export const fromNorm = (ex, t) => {
  const c = Math.max(0, Math.min(1, t));
  return ex.log
    ? ex.axisMin * Math.pow(ex.axisMax / ex.axisMin, c)
    : ex.axisMin + (ex.axisMax - ex.axisMin) * c;
};

export const drawTarget = (ex) => fromNorm(ex, Math.random() * 0.84 + 0.08);

// The accept band as a width on the 0..1 axis, which is the only form the
// display can draw. On a log axis a tolerance in octaves (or a ratio) is a
// constant width - which is the whole reason those axes are log.
export const bandWidth = (ex) => {
  if (!ex.log) return ex.tolerance / (ex.axisMax - ex.axisMin);
  if (ex.toleranceIsRatio) return ex.tolerance / Math.log(ex.axisMax / ex.axisMin);
  return ex.tolerance / Math.log2(ex.axisMax / ex.axisMin);
};
