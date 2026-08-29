import { useCallback, useEffect, useRef, useState } from 'react';
import HintCanvas from './HintCanvas.jsx';
import {
  EXERCISES, FAMILY_LABEL, toNorm, fromNorm, drawTarget, bandWidth, drawPair,
} from './exercises.js';

// ABC Ear Trainer, in a browser tab.
//
// Not a landing page with a toy on it: the same two screens the plugin
// has, in the same order, with the same controls doing the same things.
// Home lists the nine exercises as cards with their own level and record;
// picking one starts it. The training screen is the exercise header, the
// instruction, the answer scale, and one row of controls - modes, score,
// A/B, hint - laid out the way the plugin lays them out.
//
// What is deliberately different, and only this: it opens on Home rather
// than on the welcome screen (nobody arriving from a link wants a
// greeting), and progress lives in localStorage rather than in the shared
// abcTrain settings file, because a browser has no access to that.
//
// Space flips A/B here too. It is the control touched most often - once or
// twice a round, every round - and reaching for the mouse to do it breaks
// the one thing the screen is for, which is listening.

const STORE = 'abctrain-web-progress';

const loadProgress = () => {
  try {
    return JSON.parse(localStorage.getItem(STORE)) || {};
  } catch {
    return {};
  }
};

// Level L needs 100·L points to reach L+1 - the same triangular scale as
// ProgressManager::pointsRequiredForLevel, so a level here means what it
// means in the plugin.
const pointsForLevel = (level) => (level * (level - 1) * 100) / 2;
const levelForPoints = (points) => {
  let level = 1;
  while (level < 10 && points >= pointsForLevel(level + 1)) level += 1;
  return level;
};

function Toolbar() {
  return (
    <div className="tr-toolbar">
      <span className="tr-toolbar__note">
        Web demo · progress is stored in this browser only
      </span>
      <a className="tr-toolbar__link" href="https://soundkorb.ru">soundkorb.ru</a>
    </div>
  );
}

function Card({ exercise, record, onOpen }) {
  const points = record?.points ?? 0;
  const level = levelForPoints(points);
  const rounds = record?.rounds ?? 0;
  const correct = record?.correct ?? 0;
  const accuracy = rounds > 0 ? Math.round((correct / rounds) * 100) : 0;

  const floor = pointsForLevel(level);
  const ceiling = pointsForLevel(level + 1);
  const through = level >= 10 ? 1 : (points - floor) / Math.max(1, ceiling - floor);

  return (
    <button type="button" className={`tr-card tr-card--${exercise.family}`} onClick={onOpen}>
      <div className="tr-card__top">
        <span className="tr-card__family">{FAMILY_LABEL[exercise.family]}</span>
        <span className="tr-card__level">
          LEVEL <b>{level}</b>
        </span>
      </div>
      <div className="tr-card__name">{exercise.name}</div>
      <div className="tr-card__stats">
        {rounds > 0 ? `${accuracy}% correct · ${rounds} rounds` : 'not played yet'}
      </div>
      <div className="tr-card__bar">
        <span style={{ width: `${Math.max(0, Math.min(1, through)) * 100}%` }} />
      </div>
    </button>
  );
}

export default function Trainer() {
  const [screen, setScreen] = useState('home');
  const [index, setIndex] = useState(0);
  const [progress, setProgress] = useState(loadProgress);

  const exercise = EXERCISES[index];

  const [target, setTarget] = useState(null);
  // The two alternatives this round is asking about, as indices into the
  // exercise's own list. Categorical exercises always offer exactly two -
  // what the level picks is *which* two. Null on the continuous ones,
  // whose answer is a value on a ruler rather than a choice at all.
  const [pair, setPair] = useState(null);
  const [guess, setGuess] = useState(null);
  const [hover, setHover] = useState(null);
  const [processed, setProcessed] = useState(true);
  const [playing, setPlaying] = useState(false);
  const [session, setSession] = useState({ correct: 0, played: 0 });

  // The hint is bought per round and forgotten at the next one, exactly as
  // in the plugin - a display still showing the previous round's picture
  // would be answering a question already scored.
  const [hintShown, setHintShown] = useState(false);
  const [analysers, setAnalysers] = useState(null);

  const ctxRef = useRef(null);
  const nodesRef = useRef(null);

  // Auto-advance needs newRound, which is declared further down; a ref
  // keeps the effect above from having to be reordered around it.
  const newRoundRef = useRef(null);
  const scaleRef = useRef(null);

  const revealed = guess !== null;

  // Rounds advance on their own, as they do in the plugin: about a second
  // after a correct answer and about two after a wrong one, so there is
  // time to hear what the right answer sounded like before the next
  // question starts. The same numbers as SessionManager's.
  //
  // The timer is cleared on every change of round or exercise, or leaving
  // an exercise mid-reveal would drag the next round in behind you.
  useEffect(() => {
    if (guess === null) return undefined;

    const delay = guess.correct ? 900 : 1900;
    const id = setTimeout(() => newRoundRef.current?.(), delay);

    return () => clearTimeout(id);
  }, [guess]);

  const stop = useCallback(() => {
    const nodes = nodesRef.current;

    if (nodes) {
      const { gain, source, ctx } = nodes;
      gain.gain.cancelScheduledValues(ctx.currentTime);
      gain.gain.setValueAtTime(gain.gain.value, ctx.currentTime);
      gain.gain.linearRampToValueAtTime(0.0001, ctx.currentTime + 0.04);
      source.stop(ctx.currentTime + 0.06);
      nodesRef.current = null;
    }

    setPlaying(false);
  }, []);

  useEffect(() => stop, [stop]);

  const play = useCallback((ex, value, withProcessing) => {
    stop();

    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) return;

    const ctx = ctxRef.current || new Ctx();
    ctxRef.current = ctx;
    if (ctx.state === 'suspended') ctx.resume();

    const { buffer, loop } = ex.source(ctx);
    const source = ctx.createBufferSource();
    source.buffer = buffer;
    source.loop = loop;

    const gain = ctx.createGain();
    gain.gain.setValueAtTime(0.0001, ctx.currentTime);
    gain.gain.linearRampToValueAtTime(0.5, ctx.currentTime + 0.05);

    // A/B is the whole comparison: "before" is the same signal with the
    // processing bypassed, not a different signal.
    // Most exercises bypass the processing for "before". Stereo width
    // cannot: its source is two independent noise channels, so bypassing
    // plays the *widest* signal in the exercise under a button labelled
    // "Mono". Where an exercise defines buildBypass, that is its honest
    // untreated state.
    const stage = withProcessing ? ex.build(ctx, value)
                                 : (ex.buildBypass ? ex.buildBypass(ctx) : null);

    if (stage) {
      source.connect(stage.input);
      stage.output.connect(gain);
    } else {
      source.connect(gain);
    }

    // Analysers tapped off the same gain node the speakers get, so the
    // hint shows what is actually being heard rather than a re-render of
    // what was meant to be heard.
    const mono = ctx.createAnalyser();
    mono.fftSize = 2048;
    mono.smoothingTimeConstant = 0.75;
    gain.connect(mono);

    // Stereo needs the two channels apart, and it needs there to *be*
    // two. A mono source's gain node still has one output channel, so the
    // splitter fed channel 1 nothing and the scope drew a 45-degree line -
    // which reads as hard-panned when the signal is dead centre. This tap
    // is explicitly two channels, so mono up-mixes into both and centred
    // draws the vertical line it should.
    const stereoTap = ctx.createGain();
    stereoTap.channelCount = 2;
    stereoTap.channelCountMode = 'explicit';
    stereoTap.channelInterpretation = 'speakers';
    gain.connect(stereoTap);

    const splitter = ctx.createChannelSplitter(2);
    const leftAnalyser = ctx.createAnalyser();
    const rightAnalyser = ctx.createAnalyser();
    leftAnalyser.fftSize = 1024;
    rightAnalyser.fftSize = 1024;
    stereoTap.connect(splitter);
    splitter.connect(leftAnalyser, 0);
    splitter.connect(rightAnalyser, 1);

    setAnalysers({ mono, split: [leftAnalyser, rightAnalyser] });

    gain.connect(ctx.destination);
    source.start();

    nodesRef.current = { source, gain, ctx };
    setPlaying(true);
  }, [stop]);

  const newRound = useCallback((ex = exercise, atLevel = 1) => {
    let value;
    let drawn = null;

    if (ex.kind === 'zoned') {
      // Two alternatives, and the level decides how close together they
      // are - the plugin's shared/PresetFamily.h rule, not a web variant
      // of it. The correct one is then either of the two, so the answer
      // never drifts to one side of the panel.
      drawn = drawPair(ex.axis, atLevel,
        ex.distance ? (a, b) => ex.distance(a, b, ex.axis) : null);
      value = drawn[Math.random() < 0.5 ? 0 : 1];
    } else {
      value = drawTarget(ex);
    }

    setPair(drawn);
    setTarget(value);
    setHintShown(false);
    setGuess(null);
    setHover(null);
    // Each round starts *unprocessed*: you hear the clean reference first,
    // then switch. That is the order an engineer A/Bs in, and it stops the
    // treated version being the only thing ever heard.
    setProcessed(false);
    play(ex, value, false);
  }, [exercise, play]);

  // The level is read here rather than reusing the one computed further
  // down for rendering: that one is declared below this point, and a const
  // referenced before its declaration is a ReferenceError, not a warning.
  newRoundRef.current = () =>
    newRound(exercise, levelForPoints(progress[exercise.key]?.points ?? 0));

  const openExercise = useCallback((i) => {
    stop();
    setIndex(i);
    setScreen('training');
    setSession({ correct: 0, played: 0 });
    newRound(EXERCISES[i], levelForPoints(progress[EXERCISES[i].key]?.points ?? 0));
  }, [stop, newRound, progress]);

  const goHome = useCallback(() => {
    stop();
    // Clearing the guess is what actually cancels the pending auto-advance.
    // The effect above keys on `guess`, so leaving it set meant the cleanup
    // never ran, the timer fired on the home screen and started playing
    // noise at someone who had just backed out of the exercise.
    setGuess(null);
    setScreen('home');
  }, [stop]);

  const record = useCallback((wasCorrect, quality = 1) => {
    setSession((s) => ({ correct: s.correct + (wasCorrect ? 1 : 0), played: s.played + 1 }));

    setProgress((p) => {
      const key = exercise.key;
      const previous = p[key] ?? { points: 0, rounds: 0, correct: 0 };

      // 10 a round plus up to 5 for precision, exactly as
      // ProgressManager::applyAnswerToProgress awards it.
      const earned = wasCorrect ? 10 + Math.round(5 * quality) : 0;

      const next = {
        ...p,
        [key]: {
          points: previous.points + earned,
          rounds: previous.rounds + 1,
          correct: previous.correct + (wasCorrect ? 1 : 0),
        },
      };

      try {
        localStorage.setItem(STORE, JSON.stringify(next));
      } catch {
        // A browser with storage disabled still plays; it just forgets.
      }

      return next;
    });
  }, [exercise]);

  const answerContinuous = useCallback((clientX) => {
    if (revealed) return;

    const box = scaleRef.current?.getBoundingClientRect();
    if (!box) return;

    const value = fromNorm(exercise, (clientX - box.left) / box.width);
    const err = exercise.error(value, target);
    const correct = err <= exercise.tolerance;

    setGuess({ value, err, correct });
    stop();
    record(correct, correct ? 1 - err / exercise.tolerance : 0);
  }, [revealed, exercise, target, stop, record]);

  const answerZoned = useCallback((choice) => {
    if (revealed) return;

    const correct = choice === target;
    setGuess({ value: choice, correct });
    stop();
    record(correct);
  }, [revealed, target, record]);

  const setAB = useCallback((wantProcessed) => {
    setProcessed(wantProcessed);
    play(exercise, target, wantProcessed);
  }, [exercise, target, play]);

  // Space flips A/B, the same key the plugin binds.
  useEffect(() => {
    if (screen !== 'training') return undefined;

    const onKey = (e) => {
      if (e.code !== 'Space') return;

      // Space is the universal "press the focused button" key. Swallowing
      // it globally meant a keyboard user could not activate the answer
      // zones, Play, the hint or Home at all - and with the ruler focused
      // it both flipped A/B and submitted a guess at the centre of the
      // scale, which nobody chose.
      const el = e.target;
      const tag = el && el.tagName ? el.tagName.toLowerCase() : '';
      if (tag === 'button' || tag === 'input' || tag === 'select'
          || tag === 'textarea' || (el && el.isContentEditable)) return;

      e.preventDefault();
      setAB(!processed);
    };

    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [screen, processed, setAB]);

  if (screen === 'home') {
    return (
      <div className="tr">
        <div className="tr-title">ABC Ear Trainer</div>

        <div className="tr-section">
          <span className="tr-section__label">Trainings</span>
        </div>

        <div className="tr-grid">
          {EXERCISES.map((ex, i) => (
            <Card
              key={ex.key}
              exercise={ex}
              record={progress[ex.key]}
              onOpen={() => openExercise(i)}
            />
          ))}
        </div>

        <Toolbar />
      </div>
    );
  }

  // ---- training ---------------------------------------------------------
  const points = progress[exercise.key]?.points ?? 0;
  const level = levelForPoints(points);
  const floor = pointsForLevel(level);
  const ceiling = pointsForLevel(level + 1);

  let verdict = '';
  if (revealed) {
    if (exercise.kind === 'zoned') {
      verdict = guess.correct
        ? `Correct - it was ${exercise.choices[target]}.`
        : `Not quite. It was ${exercise.choices[target]}.`;
    } else {
      verdict = guess.correct
        ? `Correct! It was ${exercise.format(target)}.`
        : `${exercise.miss(guess.value, target, guess.err)} - it was ${exercise.format(target)}.`;
    }
  }

  const half = exercise.kind === 'continuous' ? bandWidth(exercise) : 0;
  const bandCentre = revealed && exercise.kind === 'continuous'
    ? toNorm(exercise, target)
    : hover;

  return (
    <div className="tr">
      <div className="tr-title">ABC Ear Trainer</div>

      <div className="tr-section">
        <span className="tr-section__label">Exercise</span>
      </div>

      <div className="tr-exercise">
        <button type="button" className="tr-btn tr-btn--home" onClick={goHome}>
          &lt; Home
        </button>
        <span className={`tr-exercise__name tr-exercise__name--${exercise.family}`}>
          {exercise.name}
        </span>
        <span className="tr-exercise__level num">
          Level {level}: {points - floor} / {ceiling - floor}
        </span>
      </div>

      <p className="tr-instructions">{exercise.instructions}</p>

      {hintShown && (
        <>
          <div className="tr-section">
            <span className="tr-section__label">What the sound looks like</span>
          </div>
          <div className="tr-hint">
            <HintCanvas
              view={exercise.hintView}
              analyser={analysers?.mono}
              splitAnalysers={analysers?.split}
            />
          </div>
        </>
      )}

      <div className="tr-section">
        <span className="tr-section__label">Your answer</span>
      </div>

      <div className="tr-answer">
        <div className="tr-readout" data-state={revealed ? (guess.correct ? 'good' : 'bad') : 'idle'}>
          {revealed
            ? (exercise.kind === 'continuous' ? exercise.format(guess.value) : exercise.choices[guess.value])
            : (exercise.kind === 'continuous' && hover !== null
                ? exercise.format(fromNorm(exercise, hover))
                : (exercise.kind === 'zoned' ? 'Click a zone to answer' : 'Drag to choose'))}
        </div>

        {exercise.kind === 'continuous' ? (
          <div
            className="tr-scale"
            ref={scaleRef}
            // A value picked along an axis is a slider, not a button. As a
            // button a screen reader announced "press me" and said nothing
            // about where the cursor was or what the range is.
            role="slider"
            tabIndex={0}
            aria-label={`${exercise.name} scale`}
            aria-valuemin={0}
            aria-valuemax={100}
            aria-valuenow={Math.round((revealed ? toNorm(exercise, guess.value) : (hover ?? 0.5)) * 100)}
            aria-valuetext={revealed ? exercise.format(guess.value)
                                     : exercise.format(fromNorm(exercise, hover ?? 0.5))}
            // Pointer events rather than mouse events, so a finger gets the
            // same live preview a cursor gets and the answer commits on
            // release. Four of the nine exercises are answered here, and on
            // a phone they were a blind tap - while the instruction said
            // "drag along the scale".
            onPointerDown={(e) => {
              if (revealed) return;
              e.currentTarget.setPointerCapture(e.pointerId);
              const box = e.currentTarget.getBoundingClientRect();
              setHover(Math.max(0, Math.min(1, (e.clientX - box.left) / box.width)));
            }}
            onPointerMove={(e) => {
              if (revealed) return;
              // Only track while down on touch; a mouse previews on hover.
              if (e.pointerType !== 'mouse' && !e.currentTarget.hasPointerCapture(e.pointerId)) return;
              const box = e.currentTarget.getBoundingClientRect();
              setHover(Math.max(0, Math.min(1, (e.clientX - box.left) / box.width)));
            }}
            onPointerUp={(e) => {
              if (revealed) return;
              if (e.currentTarget.hasPointerCapture(e.pointerId)) e.currentTarget.releasePointerCapture(e.pointerId);
              answerContinuous(e.clientX);
            }}
            onPointerLeave={(e) => {
              if (!revealed && e.pointerType === 'mouse') setHover(null);
            }}
            onKeyDown={(e) => {
              const step = e.shiftKey ? 0.01 : 0.05;
              const current = hover ?? 0.5;

              // Arrows move the cursor; Enter and Space commit it. Before
              // this the only keyboard answer was "the exact centre of the
              // scale", which is not a choice anyone made.
              if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') {
                e.preventDefault();
                if (!revealed) setHover(Math.max(0, current - step));
              } else if (e.key === 'ArrowRight' || e.key === 'ArrowUp') {
                e.preventDefault();
                if (!revealed) setHover(Math.min(1, current + step));
              } else if (e.key === 'Home') {
                e.preventDefault();
                if (!revealed) setHover(0);
              } else if (e.key === 'End') {
                e.preventDefault();
                if (!revealed) setHover(1);
              } else if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                const box = scaleRef.current?.getBoundingClientRect();
                if (box) answerContinuous(box.left + box.width * current);
              }
            }}
          >
            {exercise.ticks.map((t) => (
              <div key={t} className="tr-scale__tick" style={{ left: `${toNorm(exercise, t) * 100}%` }} />
            ))}

            {bandCentre !== null && (
              <div
                className="tr-scale__band"
                data-state={revealed ? (guess.correct ? 'good' : 'bad') : 'idle'}
                style={{
                  left: `${Math.max(0, bandCentre - half) * 100}%`,
                  width: `${(Math.min(1, bandCentre + half) - Math.max(0, bandCentre - half)) * 100}%`,
                }}
              />
            )}

            {revealed && (
              <div className="tr-scale__target" style={{ left: `${toNorm(exercise, target) * 100}%` }} />
            )}

            {(revealed || hover !== null) && (
              <div
                className="tr-scale__cursor"
                data-state={revealed ? (guess.correct ? 'good' : 'bad') : 'idle'}
                style={{ left: `${(revealed ? toNorm(exercise, guess.value) : hover) * 100}%` }}
              />
            )}

            {exercise.ticks.map((t, i) => (
              <span
                key={`l${t}`}
                className="tr-scale__label"
                // Alternating rows, the same answer the plugin's ruler
                // reached: ten octave marks across this width collide into
                // one another, and a label you cannot read is worse than a
                // label that is not there. Staggered, each series is
                // legible and neither ever overlaps the other.
                data-row={exercise.ticks.length > 6 ? i % 2 : 0}
                style={{ left: `clamp(28px, ${toNorm(exercise, t) * 100}%, calc(100% - 28px))` }}
              >
                {exercise.format(t)}
              </span>
            ))}
          </div>
        ) : (
          <div className="tr-zones">
            {(pair ?? []).map((choiceIndex) => {
              let state = 'idle';
              if (revealed && choiceIndex === target) state = 'good';
              else if (revealed && choiceIndex === guess.value) state = 'bad';

              return (
                <button
                  key={exercise.choices[choiceIndex]}
                  type="button"
                  className="tr-zone"
                  data-state={state}
                  disabled={revealed}
                  onClick={() => answerZoned(choiceIndex)}
                >
                  {exercise.choices[choiceIndex]}
                </button>
              );
            })}
          </div>
        )}

        <div className="tr-verdict" data-state={revealed ? (guess.correct ? 'good' : 'bad') : 'idle'}>
          {verdict}
        </div>
      </div>

      <div className="tr-controls">
        <span className="tr-score num">
          Score: {session.correct} / {session.played}
        </span>

        <div className="tr-ab">
          <button
            type="button"
            className="tr-btn"
            data-on={!processed}
            onClick={() => setAB(false)}
          >
            {exercise.before}
          </button>
          <button
            type="button"
            className="tr-btn"
            data-on={processed}
            onClick={() => setAB(true)}
          >
            {exercise.after}
          </button>
        </div>

        {!hintShown && !revealed && (
          <button
            type="button"
            className="tr-btn"
            onClick={() => {
              setHintShown(true);
              // Nothing to analyse if nothing is playing, so buying the
              // hint starts the sound too - in the plugin the signal is
              // already running by the time you can press it.
              if (!playing) play(exercise, target, processed);
            }}
          >
            Show the sound
          </button>
        )}

        {revealed ? (
          <button type="button" className="tr-btn tr-btn--primary" onClick={() => newRound(exercise, level)}>
            Next round
          </button>
        ) : (
          <button
            type="button"
            className="tr-btn tr-btn--primary"
            onClick={() => (playing ? stop() : play(exercise, target, processed))}
          >
            {playing ? 'Stop' : 'Play'}
          </button>
        )}
      </div>

      <Toolbar />
    </div>
  );
}
