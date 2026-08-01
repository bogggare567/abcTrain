import { useEffect, useRef } from 'react';

// "Show the sound" - the same three views the plugin has, drawn from the
// audio that is actually playing.
//
// Which one appears is the exercise's business, not this component's, for
// the reason the plugin's Game::getHintView records: one generic pair of
// displays is right for two exercises and decoration for the rest, and
// neither a spectrum nor a vectorscope can show compression at all. A hint
// the player pays points for has to be able to show the thing.
//
//   spectrum  frequency, named range, distortion - where the energy sits
//   stereo    pan, width - the field, and how far off centre
//   envelope  compression, gain, reverb, delay - level against time, which
//             is where a tail and a clamped transient live
//
// Everything here reads a real AnalyserNode. Nothing is faked: with the
// sound stopped the displays go quiet, which is correct and is also why
// the hint is only useful while something is playing.
export default function HintCanvas({ view, analyser, splitAnalysers }) {
  const canvasRef = useRef(null);
  const historyRef = useRef([]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return undefined;

    const ctx = canvas.getContext('2d');
    let frame = 0;

    // Redrawn on every animation frame, but the *history* the envelope
    // scrolls is only appended to - so pausing the tab does not tear a
    // hole in it.
    const history = historyRef.current;

    const resize = () => {
      const scale = window.devicePixelRatio || 1;
      const { width, height } = canvas.getBoundingClientRect();
      canvas.width = Math.max(1, Math.round(width * scale));
      canvas.height = Math.max(1, Math.round(height * scale));
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      return { width, height };
    };

    let size = resize();
    const onResize = () => { size = resize(); };
    window.addEventListener('resize', onResize);

    const css = getComputedStyle(canvas);
    const ink = css.getPropertyValue('--accent').trim() || '#5b9bd5';
    const dim = css.getPropertyValue('--line').trim() || '#2a2a3a';
    const well = css.getPropertyValue('--well').trim() || '#0e0e16';

    const draw = () => {
      frame = requestAnimationFrame(draw);

      const { width, height } = size;
      ctx.fillStyle = well;
      ctx.fillRect(0, 0, width, height);

      if (view === 'spectrum') drawSpectrum(ctx, width, height, analyser, ink, dim);
      else if (view === 'stereo') drawScope(ctx, width, height, splitAnalysers, ink, dim);
      else drawEnvelope(ctx, width, height, analyser, history, ink, dim);
    };

    frame = requestAnimationFrame(draw);

    return () => {
      cancelAnimationFrame(frame);
      window.removeEventListener('resize', onResize);
    };
  }, [view, analyser, splitAnalysers]);

  return <canvas ref={canvasRef} className="tr-hint__canvas" />;
}

// A smooth filled curve, deliberately not a row of bars: a wall of
// rectangles is the tell of a fake analyser and is not what a real one
// looks like with any smoothing on. The x axis is logarithmic, so an
// octave takes the same width everywhere - which is the whole reason the
// boost is findable by eye at all.
function drawSpectrum(ctx, width, height, analyser, ink, dim) {
  if (!analyser) return;

  const bins = new Uint8Array(analyser.frequencyBinCount);
  analyser.getByteFrequencyData(bins);

  const nyquist = analyser.context.sampleRate / 2;
  const lowHz = 20;
  const highHz = Math.min(20000, nyquist);
  const span = Math.log(highHz / lowHz);

  ctx.strokeStyle = dim;
  ctx.lineWidth = 1;
  for (const hz of [100, 1000, 10000]) {
    const x = (Math.log(hz / lowHz) / span) * width;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, height);
    ctx.stroke();
  }

  ctx.beginPath();
  ctx.moveTo(0, height);

  const points = 200;
  for (let i = 0; i < points; i += 1) {
    const t = i / (points - 1);
    const hz = lowHz * Math.exp(t * span);
    const bin = Math.min(bins.length - 1, Math.round((hz / nyquist) * bins.length));
    const level = bins[bin] / 255;
    ctx.lineTo(t * width, height - level * height * 0.92);
  }

  ctx.lineTo(width, height);
  ctx.closePath();

  ctx.fillStyle = `${ink}33`;
  ctx.fill();
  ctx.strokeStyle = ink;
  ctx.lineWidth = 2;
  ctx.stroke();
}

// A Lissajous figure, rotated 45 degrees the way every vectorscope draws
// it: mono collapses to a vertical line, and anything off-centre leans.
// That lean is the answer to "which way and roughly how far".
function drawScope(ctx, width, height, splitAnalysers, ink, dim) {
  if (!splitAnalysers) return;

  const [leftAnalyser, rightAnalyser] = splitAnalysers;
  const left = new Float32Array(leftAnalyser.fftSize);
  const right = new Float32Array(rightAnalyser.fftSize);
  leftAnalyser.getFloatTimeDomainData(left);
  rightAnalyser.getFloatTimeDomainData(right);

  const cx = width / 2;
  const cy = height / 2;
  const radius = Math.min(width, height) * 0.42;

  // The centre line and the two diagonals, so "leaning left" has
  // something to lean against.
  ctx.strokeStyle = dim;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(cx, cy - radius);
  ctx.lineTo(cx, cy + radius);
  ctx.moveTo(cx - radius, cy);
  ctx.lineTo(cx + radius, cy);
  ctx.stroke();

  ctx.strokeStyle = ink;
  ctx.lineWidth = 1.2;
  ctx.beginPath();

  for (let i = 0; i < left.length; i += 1) {
    const l = left[i];
    const r = right[i];
    const x = cx + (l - r) * radius * 0.707;
    const y = cy - (l + r) * radius * 0.707;

    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }

  ctx.stroke();
}

// Peak level against time, scrolling right to left. This is where a
// compressor clamping a transient and a reverb's tail are both visible -
// neither shows up on a spectrum or a scope, which is why the plugin grew
// this third view rather than reusing the other two.
function drawEnvelope(ctx, width, height, analyser, history, ink, dim) {
  if (!analyser) return;

  const samples = new Float32Array(analyser.fftSize);
  analyser.getFloatTimeDomainData(samples);

  let peak = 0;
  for (let i = 0; i < samples.length; i += 1) peak = Math.max(peak, Math.abs(samples[i]));

  const columns = 220;
  history.push(peak);
  while (history.length > columns) history.shift();

  ctx.strokeStyle = dim;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, height / 2);
  ctx.lineTo(width, height / 2);
  ctx.stroke();

  const step = width / columns;
  const half = height * 0.46;

  // Filled symmetrically about the centre, the way a waveform is drawn -
  // an envelope hanging off the bottom reads as a bar chart again.
  ctx.beginPath();
  ctx.moveTo(0, height / 2);

  for (let i = 0; i < history.length; i += 1) {
    ctx.lineTo(i * step, height / 2 - history[i] * half);
  }

  for (let i = history.length - 1; i >= 0; i -= 1) {
    ctx.lineTo(i * step, height / 2 + history[i] * half);
  }

  ctx.closePath();
  ctx.fillStyle = `${ink}44`;
  ctx.fill();
  ctx.strokeStyle = ink;
  ctx.lineWidth = 1.6;
  ctx.stroke();
}
