import { useCallback, useEffect, useRef, useState } from 'react';
import {
  EXERCISES,
  fillPinkNoise,
  toNormalised,
  fromNormalised,
  drawTarget,
  toleranceOnAxis,
} from '../exercises.js';

// Three real rounds of the trainer, playable in the browser.
//
// This is the hero because it *is* the product: the fastest honest way to
// explain an ear trainer is to let somebody fail at it once. A screenshot
// of a spectrum explains nothing that a hundred other audio pages have not
// already failed to explain.
//
// It reproduces the plugin's mechanic rather than imitating its look: pink
// noise from the same Kellet filter, targets drawn the same way, and an
// accept band measured in the exercise's own unit — octaves for frequency,
// dB for level, a fraction of the field for pan. That last part is the one
// that would be easy to fake and would quietly teach the wrong skill.
//
// **Nothing makes a sound until the visitor presses play.** Audio on page
// load is the rudest thing a site can do, and browsers block it anyway.

export default function PlayableRound() {
  const [exerciseIndex, setExerciseIndex] = useState(0);
  const exercise = EXERCISES[exerciseIndex];

  const [target, setTarget] = useState(() => drawTarget(EXERCISES[0]));
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
      // Ramped, not cut: stopping a noise source dead is a click, and a
      // demo that clicks reads as broken software rather than as a demo.
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
    (ex, value) => {
      stop();

      const Ctx = window.AudioContext || window.webkitAudioContext;
      if (!Ctx) return;

      const ctx = audioRef.current || new Ctx();
      audioRef.current = ctx;
      if (ctx.state === 'suspended') ctx.resume();

      const seconds = 2.4;
      const buffer = ctx.createBuffer(1, Math.floor(ctx.sampleRate * seconds), ctx.sampleRate);
      fillPinkNoise(buffer.getChannelData(0));

      const source = ctx.createBufferSource();
      source.buffer = buffer;
      source.loop = true;

      const gain = ctx.createGain();
      gain.gain.setValueAtTime(0.0001, ctx.currentTime);
      gain.gain.linearRampToValueAtTime(0.5, ctx.currentTime + 0.05);

      // Each exercise supplies its own processing; everything else about
      // the signal path is identical, which is what makes the three
      // comparable to each other.
      const stage = ex.build(ctx, value);
      source.connect(stage.input);
      stage.output.connect(gain).connect(ctx.destination);
      source.start();

      nodesRef.current = { source, gain, ctx };
      setPlaying(true);
    },
    [stop],
  );

  const answer = useCallback(
    (clientX) => {
      if (revealed) return;

      const box = scaleRef.current?.getBoundingClientRect();
      if (!box) return;

      const fraction = Math.min(1, Math.max(0, (clientX - box.left) / box.width));
      const value = fromNormalised(exercise, fraction);
      const err = exercise.error(value, target);
      const correct = err <= exercise.tolerance;

      setGuess({ value, err, correct });
      setRevealed(true);
      setScore((s) => ({ correct: s.correct + (correct ? 1 : 0), played: s.played + 1 }));
      stop();
    },
    [revealed, exercise, target, stop],
  );

  const next = useCallback(
    (ex = exercise) => {
      const value = drawTarget(ex);
      setTarget(value);
      setGuess(null);
      setRevealed(false);
      play(ex, value);
    },
    [exercise, play],
  );

  const chooseExercise = useCallback(
    (index) => {
      stop();
      setExerciseIndex(index);
      setTarget(drawTarget(EXERCISES[index]));
      setGuess(null);
      setRevealed(false);
      setScore({ correct: 0, played: 0 });
    },
    [stop],
  );

  const half = toleranceOnAxis(exercise);
  const bandCentre = revealed
    ? toNormalised(exercise, target)
    : guess
      ? toNormalised(exercise, guess.value)
      : null;

  const state = !revealed ? 'idle' : guess.correct ? 'good' : 'bad';

  let verdict = 'Press play, then click where you think it is.';
  if (playing && !revealed) verdict = exercise.prompt;
  if (revealed) {
    verdict = guess.correct
      ? `Inside the band — it was ${exercise.format(target)}.`
      : `${exercise.describeMiss(guess.value, target, guess.err)} — it was ${exercise.format(target)}.`;
  }

  return (
    <div className="round">
      <div className="round__head">
        <div className="round__tabs" role="tablist" aria-label="Exercise">
          {EXERCISES.map((ex, i) => (
            <button
              key={ex.key}
              type="button"
              role="tab"
              aria-selected={i === exerciseIndex}
              className={`round__tab round__tab--${ex.family}`}
              data-active={i === exerciseIndex}
              onClick={() => chooseExercise(i)}
            >
              {ex.name.replace('Guess the ', '')}
            </button>
          ))}
        </div>
        <span className="round__title num">
          {score.played > 0 ? `${score.correct} / ${score.played}` : ' '}
        </span>
      </div>

      <div
        className="scale"
        ref={scaleRef}
        onClick={(e) => answer(e.clientX)}
        role="button"
        tabIndex={0}
        aria-disabled={revealed}
        aria-label={`${exercise.name} — click where you think the answer is`}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            const box = scaleRef.current?.getBoundingClientRect();
            if (box) answer(box.left + box.width / 2);
          }
        }}
      >
        <div className="scale__grid">
          {exercise.ticks.map((t) => (
            <div
              key={t}
              className="scale__tick"
              style={{ left: `${toNormalised(exercise, t) * 100}%` }}
            />
          ))}
        </div>

        {(playing || revealed) && bandCentre !== null && (
          <div
            className="scale__band"
            data-state={state}
            style={{
              left: `${Math.max(0, bandCentre - half) * 100}%`,
              width: `${(Math.min(1, bandCentre + half) - Math.max(0, bandCentre - half)) * 100}%`,
            }}
          />
        )}

        {revealed && (
          <div
            className="scale__target"
            style={{ left: `${toNormalised(exercise, target) * 100}%` }}
          />
        )}

        {guess && (
          <div
            className="scale__cursor"
            style={{ left: `${toNormalised(exercise, guess.value) * 100}%` }}
          />
        )}

        {revealed && (
          <div
            className="scale__readout"
            style={{
              left: `${Math.min(88, Math.max(12, toNormalised(exercise, guess.value) * 100))}%`,
              color: guess.correct ? 'var(--good)' : 'var(--bad)',
            }}
          >
            {exercise.format(guess.value)}
          </div>
        )}

        {!playing && !revealed && <div className="scale__hint">{exercise.axisNote}</div>}

        {/* Clamped inside the panel. A label is centred on its tick, so
            the first and last lose half their text to the panel's own clip
            without this — "L100" rendered as "00". The same edge-clipping
            the plugin's scale had to fix once already. */}
        {exercise.ticks.map((t) => (
          <span
            key={`l${t}`}
            className="scale__tickLabel"
            style={{
              left: `clamp(var(--tick-inset), ${toNormalised(exercise, t) * 100}%, calc(100% - var(--tick-inset)))`,
            }}
          >
            {exercise.format(t)}
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
              onClick={() => (playing ? stop() : play(exercise, target))}
            >
              {playing ? 'Stop' : 'Play the sound'}
            </button>
          ) : (
            <button type="button" className="btn btn--primary" onClick={() => next()}>
              Next round
            </button>
          )}
        </div>
        <p className="round__note">
          Level 1 slack: {exercise.toleranceLabel}. In the plugin it narrows as you earn the
          level, and every exercise keeps its own.
        </p>
      </div>
    </div>
  );
}
