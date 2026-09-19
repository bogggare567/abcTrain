import { useCallback, useEffect, useRef, useState } from 'react';
import HintCanvas from './HintCanvas.jsx';
import {
  EXERCISES, FAMILY_LABEL, toNorm, fromNorm, drawTarget, bandWidth, drawPair,
  describeLevel, rankFor,
} from './exercises.js';

// ABC Ear Trainer, in a browser tab.
//
// Not a landing page with a toy on it: the same two screens the plugin
// has, in the same order, with the same controls doing the same things.
// Home lists the nine exercises, each with its record as a threshold in
// its own units and a ten-step ruler; picking one starts it. The training screen is the exercise header, the
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

// The staircase, as ProgressManager::applyAnswerToProgress runs it
// (ADR 035): three right in a row -> one step harder, one wrong -> one
// step easier, steps 1..10. Each exercise keeps where it is now (`level`)
// and its record (`bestLevel`), which never drops.
const STEP_UP_AFTER = 3;
const MAX_LEVEL = 10;

const blank = () => ({ level: 1, bestLevel: 1, stepRun: 0, rounds: 0, correct: 0 });

// A save from the points era has `points` and no `level`. Like the
// plugin's migration, the level those points had earned becomes both the
// starting step and the record, so nobody finds an exercise reset to 1.
const migrate = (r) => {
  if (!r) return blank();
  if (r.level != null) return { ...blank(), ...r };

  let level = 1;
  while (level < MAX_LEVEL && (r.points ?? 0) >= ((level + 1) * level * 100) / 2) level += 1;
  return { level, bestLevel: level, stepRun: 0, rounds: r.rounds ?? 0, correct: r.correct ?? 0 };
};

const loadProgress = () => {
  try {
    const raw = JSON.parse(localStorage.getItem(STORE)) || {};
    return Object.fromEntries(Object.entries(raw).map(([k, v]) => [k, migrate(v)]));
  } catch {
    return {};
  }
};

const progressOf = (progress, key) => progress[key] ?? blank();

const step = (r, wasCorrect) => {
  let { level, stepRun } = r;

  if (wasCorrect) {
    stepRun += 1;
    if (stepRun >= STEP_UP_AFTER) {
      if (level < MAX_LEVEL) {
        level += 1;
        stepRun = 0;
      } else {
        // At the top step the run stays full; there is nowhere further.
        stepRun = STEP_UP_AFTER - 1;
      }
    }
  } else {
    stepRun = 0;
    level = Math.max(1, level - 1);
  }

  return {
    level,
    stepRun,
    bestLevel: Math.max(r.bestLevel, level),
    rounds: r.rounds + 1,
    correct: r.correct + (wasCorrect ? 1 : 0),
  };
};

// Ten segments: filled up to the record, a tick at today's step, and the
// next rank's first step dashed - the plugin's home-screen ruler.
function Ruler({ level, bestLevel }) {
  const nextRung = 2 * Math.ceil(bestLevel / 2) + 1;

  return (
    <div className="tr-ruler" aria-hidden="true">
      {Array.from({ length: MAX_LEVEL }, (_, i) => {
        const n = i + 1;
        return (
          <span
            key={n}
            data-filled={n <= bestLevel}
            data-today={n === level}
            data-next={n === nextRung}
          />
        );
      })}
    </div>
  );
}

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
  const { level, bestLevel, rounds, correct } = record;
  const accuracy = rounds > 0 ? Math.round((correct / rounds) * 100) : 0;

  // Screens lead with the record, so a wrong answer is never shown as a
  // loss (ADR 035).
  return (
    <button type="button" className={`tr-card tr-card--${exercise.family}`} onClick={onOpen}>
      <div className="tr-card__top">
        <span className="tr-card__family">{FAMILY_LABEL[exercise.family]}</span>
        <span className="tr-card__level num">
          <b>{describeLevel(exercise, bestLevel)}</b>
        </span>
      </div>
      <div className="tr-card__name">{exercise.name}</div>
      <div className="tr-card__stats">
        {rounds > 0 ? `${accuracy}% correct · ${rounds} rounds` : 'not started yet'}
        {' · '}{rankFor(bestLevel)}
      </div>
      <Ruler level={level} bestLevel={bestLevel} />
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
  // Set when the last answer beat the record - the one staircase event
  // the plugin announces.
  const [newRecord, setNewRecord] = useState(false);
  // The step this round is asked at. Held from the round's start, so the
  // accept band drawn on the reveal is the one the answer was judged
  // against, not the step the answer has just moved to.
  const [roundLevel, setRoundLevel] = useState(1);

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

  const play = useCallback((ex, value, withProcessing, level = 1) => {
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
    const stage = withProcessing ? ex.build(ctx, value, level)
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
    setNewRecord(false);
    setRoundLevel(atLevel);
    // Each round starts *unprocessed*: you hear the clean reference first,
    // then switch. That is the order an engineer A/Bs in, and it stops the
    // treated version being the only thing ever heard.
    setProcessed(false);
    play(ex, value, false, atLevel);
  }, [exercise, play]);

  // Read when the round starts, so a step taken by the last answer applies
  // to the very next question - as in the plugin.
  newRoundRef.current = () =>
    newRound(exercise, progressOf(progress, exercise.key).level);

  const openExercise = useCallback((i) => {
    stop();
    setIndex(i);
    setScreen('training');
    setSession({ correct: 0, played: 0 });
    newRound(EXERCISES[i], progressOf(progress, EXERCISES[i].key).level);
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

  const record = useCallback((wasCorrect) => {
    setSession((s) => ({ correct: s.correct + (wasCorrect ? 1 : 0), played: s.played + 1 }));

    // One answer, one step of the staircase. No points, no promotion test.
    const previous = progressOf(progress, exercise.key);
    setNewRecord(step(previous, wasCorrect).bestLevel > previous.bestLevel);

    setProgress((p) => {
      const next = { ...p, [exercise.key]: step(progressOf(p, exercise.key), wasCorrect) };

      try {
        localStorage.setItem(STORE, JSON.stringify(next));
      } catch {
        // A browser with storage disabled still plays; it just forgets.
      }

      return next;
    });
  }, [exercise, progress]);

  const answerContinuous = useCallback((clientX) => {
    if (revealed) return;

    const box = scaleRef.current?.getBoundingClientRect();
    if (!box) return;

    const value = fromNorm(exercise, (clientX - box.left) / box.width);
    const err = exercise.error(value, target);
    const correct = err <= exercise.toleranceAt(roundLevel);

    setGuess({ value, err, correct });
    stop();
    record(correct);
  }, [revealed, exercise, target, stop, record, roundLevel]);

  const answerZoned = useCallback((choice) => {
    if (revealed) return;

    const correct = choice === target;
    setGuess({ value: choice, correct });
    stop();
    record(correct);
  }, [revealed, target, record]);

  const setAB = useCallback((wantProcessed) => {
    setProcessed(wantProcessed);
    play(exercise, target, wantProcessed, roundLevel);
  }, [exercise, target, play, roundLevel]);

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
              record={progressOf(progress, ex.key)}
              onOpen={() => openExercise(i)}
            />
          ))}
        </div>

        <Toolbar />
      </div>
    );
  }

  // ---- training ---------------------------------------------------------
  const { level, bestLevel, stepRun } = progressOf(progress, exercise.key);

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
    if (newRecord) verdict += ' New record.';
  }

  const half = exercise.kind === 'continuous'
    ? bandWidth(exercise, exercise.toleranceAt(roundLevel))
    : 0;
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
        {/* The step as what it means - the accept band in this exercise's
            units - plus three squares for the run toward the next step.
            The plugin's "Your threshold" line and its pips. */}
        <span className="tr-exercise__level num">
          Your threshold {describeLevel(exercise, level)}
          {bestLevel > level && <> · record {describeLevel(exercise, bestLevel)}</>}
        </span>
        <span
          className="tr-pips"
          title={level >= MAX_LEVEL ? 'top step - keep it' : '3 in a row makes it harder'}
          aria-label={`${stepRun} of ${STEP_UP_AFTER} in a row`}
        >
          {Array.from({ length: STEP_UP_AFTER }, (_, i) => (
            <span key={i} data-on={i < stepRun} />
          ))}
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
              if (!playing) play(exercise, target, processed, roundLevel);
            }}
          >
            Show the sound
          </button>
        )}

        {/* No "Next round" button: rounds advance on their own, as in the
            plugin, and a button that duplicates something automatic is a
            button to remove. Play stays because a browser will not start
            sound without a click. */}
        {!revealed && (
          <button
            type="button"
            className="tr-btn tr-btn--primary"
            onClick={() => (playing ? stop() : play(exercise, target, processed, roundLevel))}
          >
            {playing ? 'Stop' : 'Play'}
          </button>
        )}
      </div>

      <Toolbar />
    </div>
  );
}
