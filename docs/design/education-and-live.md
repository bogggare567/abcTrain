# Education: Training → Teaching → Live

A design note, not a spec. Recorded 2026-09-21 from the user's idea and an
outside assistant's expansion of it; the assessment and the sketches are
ours. Status and cost live in [../roadmap.md](../roadmap.md); where the
code would go is in [ADR 039](../decisions/039-structure-for-growth.md).

## The idea

**The user's:** offer abcTrain to sound-engineering schools such as Music
Head. A teacher can *explain* what 300 Hz or a slow attack is; a student
has to *hear* it, and that takes practice nobody schedules. Later, a live
format: at a seminar the room runs abcTrain together and votes — on a
site or in Telegram — for what they heard.

**The expansion** (an outside assistant, kept because most of it is right):

- Position abcTrain as a tool that forms a sound engineer's listening
  skills, not as "free plugins". EQ, compressors and reverbs are a crowded
  market; *explain → hear → identify → apply → check* is a method.
- Three layers: **Training** (one person, exists today), **Teaching**
  (a teacher sets work, sees results), **Live** (a room of 20–200 answers
  at once).
- Keep offline-first; Live as a separate layer, web first, Telegram
  optional.
- Richer questions than "which frequency": which parameter changed, which
  of two nearly identical mixes is right.
- First contact with a school is a free experiment on one group, not a sale.

## What we agree with, and what we would change

**The staircase is the real asset here, and it is already built.** A
teacher does not get "Ivan scored 87%", which says nothing; he gets
"Ivan's frequency threshold is ±0.35 octave, down from ±0.8 three weeks
ago". That is a measurement, and it is what makes a before/after pilot
possible at all.

**Step 0 needs no code.** Take today's app into one class. Everyone does
the placement run, then fifteen minutes a day for three weeks, then again.
If thresholds move, Teaching is worth building; if they don't, no amount of
teacher dashboard will fix it. This is the cheapest experiment in the whole
direction and it answers the only question that matters first.

**The live hall has an acoustic problem the expansion misses.** In a room,
everyone hears the PA *through the room*. Low-frequency room modes swing
±10 dB from seat to seat — larger than the boosts being asked about — and
the PA's own curve sits on top. A "collective hearing measurement" in a
hall measures the room and the PA at least as much as the ears. Two ways
out, and the second is better:

1. Ask only what survives a room: A/B comparisons ("which one has the
   boost"), which parameter changed (attack or release), big categorical
   differences. Useful, but coarse.
2. **Let each phone play the sound.** The audience page renders the
   stimulus itself (Web Audio, the same exercise code the browser demo
   already runs) into the listener's own earbuds. The presenter controls
   the round, not the audio. The room disappears from the result; cheap
   earbuds become the limit, which is honest and the same for everyone who
   brought the same kind. Phones only need to agree on *which round*, not on
   sample timing.

**Live collects answers, so it must say so.** The installed product
promises no account, no server, no telemetry. Live cannot keep all three —
votes have to travel. So: anonymous, one session, deleted when the session
ends, and only people who scan the code take part. The promise stays true
for everything that is installed.

**The reveal is the lesson, not the leaderboard.** The expansion itself
warns this can become "Kahoot for sound engineers". What stops that is the
step after the vote: the presenter plays the answer, then toggles A/B while
the room listens again knowing what to listen for. That re-listen is where
people learn; the percentage bar is only the reason to pay attention.

**Teaching needs no server either.** An assignment can be a file the
teacher sends; a result can be a file the student sends back. A page in
`website/` reads a folder of dropped result files in the browser and draws
the group — nothing uploaded anywhere. A student could edit a result file;
for classwork that is acceptable, and signing files is not worth the
complexity until someone grades exams with it.

## Sketches

### Live — presenter's screen (projector)

```
┌──────────────────────────────────────────────────────────────┐
│  abcTrain Live · EQ · round 4 of 10            join: AB73 ▣  │
│                                                              │
│              Where is the boost?                             │
│                                                              │
│     ┌───────────────────────────────────────────────┐        │
│     │ 20   50  100  200  500  1k   2k   5k  10k 20k │        │
│     └───────────────────────────────────────────────┘        │
│                                                              │
│     answered  ████████████████████████░░░░░░  38 / 47        │
│                                                              │
│     [ ▶ play again ]            [ reveal ]    [ next round ] │
└──────────────────────────────────────────────────────────────┘
```

### Live — audience phone

```
┌────────────────────┐
│ AB73 · round 4     │
│                    │
│  🎧 earbuds in?    │
│  [ ▶ listen ]      │
│                    │
│  ──────●────────   │
│  20 Hz      20 kHz │
│      ~ 320 Hz      │
│                    │
│   [  answer  ]     │
└────────────────────┘
```

### Live — reveal

```
┌──────────────────────────────────────────────────────────────┐
│  Round 4 · the boost was at 330 Hz (+6 dB, Q 1.4)            │
│                                                              │
│  100 Hz  █                                                   │
│  200 Hz  ███                                                 │
│  250 Hz  ██████████                                          │
│  315 Hz  ███████████████████   ← the room's median: 300 Hz   │
│  400 Hz  ███████                                             │
│  500 Hz  ██                                                  │
│                                                              │
│  within ±⅓ octave: 31 of 47                                  │
│                                                              │
│  [ A/B: listen again knowing the answer ]                    │
└──────────────────────────────────────────────────────────────┘
```

### Teaching — an assignment, as a file

```json
{
  "abcTrainAssignment": 1,
  "title": "Week 3 — low mids",
  "exercise": "eq",
  "range": { "fromHz": 150, "toHz": 800 },
  "rounds": 20,
  "rules": "beginner",
  "due": "2026-10-05"
}
```

The trainer opens it (double-click, or drop it on the window), runs exactly
that, and offers to save a result file when it is done.

### Teaching — the teacher's page (reads dropped result files, locally)

```
┌──────────────────────────────────────────────────────────────┐
│  Week 3 — low mids                          12 of 14 handed in│
│                                                              │
│  name         threshold        change     weakest region     │
│  Ivan         ±0.35 oct        ▼ 0.45     250–400 Hz         │
│  Maria        ±0.52 oct        ▼ 0.20     150–250 Hz         │
│  Pyotr        ±0.90 oct        —          everywhere         │
│  …                                                           │
│                                                              │
│  group median ±0.55 oct · hardest region for the group:      │
│  250–400 Hz → spend the next class there                     │
└──────────────────────────────────────────────────────────────┘
```

## Order, and what each step costs

| Step | What | Cost | Decides |
|---|---|---|---|
| 0 | Pilot with one teacher and today's app: placement run, 3 weeks × 15 min, placement run again | no code; a letter and a follow-up | whether Teaching is worth building |
| 1 | Result export: the trainer saves a run as a file (exercise, threshold, per-round answers) | small — the engine already has all of it | whether teachers will actually collect files |
| 2 | Assignment files + the teacher's page in `website/` | medium | — |
| 3 | Live, presenter-controlled rounds, audio on the phones | large — a relay service, two web pages, a session protocol | whether seminars use it more than once |
| 4 | Telegram as a second way to join | small, once 3 exists | — |

Steps 1 and 2 are pure additions to `abc_trainer_engine` and `website/`;
nothing about the installed product changes shape. Step 3 is the first
thing in abcTrain that needs a server, and it should stay a separate,
optional deployable.
