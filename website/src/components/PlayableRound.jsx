import { useCallback, useEffect, useRef, useState } from 'react';

// One real round of "Guess the Band", playable in the browser.
//
// This is the hero because it *is* the product: the fastest honest way to
// explain an ear trainer is to let someone fail at it once. A screenshot
// of a spectrum explains nothing a hundred other audio pages have not
// already tried to explain.
//
// It mirrors the plugin's own mechanic exactly (Source/Games/EQGame.cpp):
// pink noise through a peaking filter at a frequency drawn log-uniformly
// across 100 Hz - 12.8 kHz, answered on a log axis, graded with a
// tolerance measured in *octaves* so the slack is the same ratio at
// 200 Hz as at 8 kHz.
//
// Nothing makes a sound until the visitor asks it to. Audio that starts
// on page load is the rudest thing a site can do, and browsers block it
// anyway.

const F_MIN = 100;
const F_MAX = 12800;

// Half an octave of margin at each end, so 100 Hz and 12.8 kHz sit inside
// the axis rather than on its edges - the same reasoning as the plugin's.
const AXIS_MIN = F_MIN / Math.SQRT2;
const AXIS_MAX = F_MAX * Math.SQRT2;

const TOLERANCE_OCTAVES = 1.0;   // the plugin's easiest tier
const GAIN_DB = 9;               // ditto

const TICKS = [100, 200, 400, 800, 1600, 3200, 6400, 12800];

const toX = (hz) =>
  (Math.log2(hz / AXIS_MIN) / Math.log2(AXIS_MAX / AXIS_MIN)) * 100;

const fromX = (fraction) =>
  AXIS_MIN * Math.pow(AXIS_MAX / AXIS_MIN, fraction);

const formatHz = (hz) =>
  hz >= 1000 ? `${(hz / 1000).toFixed(hz >= 10000 ? 0 : 1)}k Hz` : `${Math.round(hz)} Hz`;

const drawTarget = () => fromX(Math.random() * 0.86 + 0.07);

export default function PlayableRound() {
  const [target, setTarget] = useState(drawTarget);
  const [guess, setGuess] = useState(null);
  const [revealed, setRevealed] = useState(false);
  const [playing, setPlaying] = useState(false);
  const [score, setScore] = useState({ correct: 0, played: 0 });

  const audioRef = useRef(null);
  const nodesRef = useRef(null);
  const scaleRef = useRef(null);

  const stop = useCallback(() => {
    const nodes = nodesRef.current;
    if (nodes) {
      // Ramp rather than cut: stopping a noise source dead is a click.
      const { gain, source, ctx } = nodes;
      gain.gain.cancelScheduledValues(ctx.currentTime);
      gain.gain.setValueAtTime(gain.gain.value, ctx.currentTime);
      gain.gain.linearRampToValueAtTime(0.0001, ctx.currentTime + 0.04);
      source.stop(ctx.currentTime + 0.06);
      nodesRef.current = null;
    }
    setPlaying(false);
  }, []);

  // Never leave an oscillator running behind a closed page.
  useEffect(() => stop, [stop]);

  const play = useCallback(
    (hz) => {
      stop();

      const Ctx = window.AudioContext || window.webkitAudioContext;
      if (!Ctx) return;

      const ctx = audioRef.current || new Ctx();
      audioRef.current = ctx;
      if (ctx.state === 'suspended') ctx.resume();

      // Pink noise, Paul Kellet's economy filter - the same algorithm
      // shared/PinkNoiseGenerator.h uses, so the site and the plugin are
      // literally listening to the same signal.
      const seconds = 2.4;
      const buffer = ctx.createBuffer(1, ctx.sampleRate * seconds, ctx.sampleRate);
      const data = buffer.getChannelData(0);
      let b0 = 0, b1 = 0, b2 = 0;

      for (let i = 0; i < data.length; i += 1) {
        const white = Math.random() * 2 - 1;
        b0 = 0.99765 * b0 + white * 0.0990460;
        b1 = 0.96300 * b1 + white * 0.2965164;
        b2 = 0.57000 * b2 + white * 1.0526913;
        data[i] = (b0 + b1 + b2 + white * 0.1848) * 0.12;
      }

      const source = ctx.createBufferSource();
      source.buffer = buffer;
      source.loop = true;

      const filter = ctx.createBiquadFilter();
      filter.type = 'peaking';
      filter.frequency.value = hz;
      filter.Q.value = 3;
      filter.gain.value = GAIN_DB;

      const gain = ctx.createGain();
      gain.gain.setValueAtTime(0.0001, ctx.currentTime);
      gain.gain.linearRampToValueAtTime(0.5, ctx.currentTime + 0.05);

      source.connect(filter).connect(gain).connect(ctx.destination);
      source.start();

      nodesRef.current = { source, gain, ctx };
      setPlaying(true);
    },
    [stop],
  );

  const answer = useCallback(
    (event) => {
      if (revealed) return;

      const box = scaleRef.current?.getBoundingClientRect();
      if (!box) return;

      const fraction = Math.min(1, Math.max(0, (event.clientX - box.left) / box.width));
      const hz = fromX(fraction);
      const missOctaves = Math.abs(Math.log2(hz / target));
      const correct = missOctaves <= TOLERANCE_OCTAVES;

      setGuess({ hz, missOctaves, correct });
      setRevealed(true);
      setScore((s) => ({ correct: s.correct + (correct ? 1 : 0), played: s.played + 1 }));
      stop();
    },
    [revealed, target, stop],
  );

  const next = useCallback(() => {
    const hz = drawTarget();
    setTarget(hz);
    setGuess(null);
    setRevealed(false);
    play(hz);
  }, [play]);

  const bandCentre = revealed ? target : guess?.hz ?? target;
  const bandLeft = toX(bandCentre / Math.pow(2, TOLERANCE_OCTAVES));
  const bandRight = toX(bandCentre * Math.pow(2, TOLERANCE_OCTAVES));

  const state = !revealed ? 'idle' : guess.correct ? 'good' : 'bad';

  let verdict = 'Press play, then click where you think the boost is.';
  if (playing && !revealed) verdict = 'Click the scale to answer.';
  if (revealed) {
    const direction = guess.hz > target ? 'high' : 'low';
    verdict = guess.correct
      ? `Inside the band — it was ${formatHz(target)}.`
      : `${guess.missOctaves.toFixed(1)} octaves too ${direction} — it was ${formatHz(target)}.`;
  }

  return (
    <div className="round">
      <div className="round__head">
        <span className="round__title">Guess the Band · one round</span>
        <span className="round__title num">
          {score.played > 0 ? `${score.correct} / ${score.played}` : ' '}
        </span>
      </div>

      <div
        className="scale"
        ref={scaleRef}
        onClick={answer}
        role="button"
        tabIndex={0}
        aria-disabled={revealed}
        aria-label="Frequency scale — click where you think the boost is"
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            const box = scaleRef.current?.getBoundingClientRect();
            if (box) answer({ clientX: box.left + box.width / 2 });
          }
        }}
      >
        <div className="scale__grid">
          {TICKS.map((hz) => (
            <div key={hz} className="scale__tick" style={{ left: `${toX(hz)}%` }} />
          ))}
        </div>

        {(playing || revealed) && (
          <div
            className="scale__band"
            data-state={state}
            style={{ left: `${bandLeft}%`, width: `${bandRight - bandLeft}%` }}
          />
        )}

        {revealed && <div className="scale__target" style={{ left: `${toX(target)}%` }} />}
        {guess && <div className="scale__cursor" style={{ left: `${toX(guess.hz)}%` }} />}

        {revealed && (
          <div
            className="scale__readout"
            style={{
              left: `${Math.min(88, Math.max(12, toX(guess.hz)))}%`,
              color: guess.correct ? 'var(--good)' : 'var(--bad)',
            }}
          >
            {formatHz(guess.hz)}
          </div>
        )}

        {!playing && !revealed && <div className="scale__hint">100 Hz — 12.8 kHz, log scale</div>}

        {TICKS.map((hz) => (
          <span key={`l${hz}`} className="scale__tickLabel" style={{ left: `${toX(hz)}%` }}>
            {formatHz(hz)}
          </span>
        ))}
      </div>

      <div className="round__verdict" data-state={state} aria-live="polite">
        {verdict}
      </div>

      <div className="round__foot">
        <div className="row">
          {!revealed ? (
            <button
              type="button"
              className="btn btn--primary"
              onClick={() => (playing ? stop() : play(target))}
            >
              {playing ? 'Stop' : 'Play the sound'}
            </button>
          ) : (
            <button type="button" className="btn btn--primary" onClick={next}>
              Next round
            </button>
          )}
        </div>
        <p className="round__note">
          Level 1 slack: ±1 octave. In the plugin it narrows to ±0.35 as you earn the level.
        </p>
      </div>
    </div>
  );
}
