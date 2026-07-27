# The journey, and why anyone would come back

This is the document the product decisions answer to. It exists because
"add achievements" and "add a results screen" are not reasons — they are
mechanisms, and a mechanism without a reason produces a slot machine.

## The honest problem

Mixing is a listening skill. Listening skills come from repetition with
immediate feedback. Almost nobody gets that repetition, because building
the exercise is harder than doing it: to practise hearing 400 Hz you
first have to set up a filter, randomise it, hide the answer from
yourself, and check. By the time that is built the evening is gone.

So the product's job is not to teach. It is to **remove the setup cost of
practising**, and then to be somewhere a person is willing to return to.

Those are two different jobs and the second one is harder.

## Why here rather than the alternatives

There are good ear trainers. Being honest about them is the only way to
know what this has to be.

| | What they have | What they don't |
|---|---|---|
| **Web trainers** (SoundGym, TrainYourEars) | Polished, curricula, leaderboards, a business behind them | A tab you have to leave your DAW for; a subscription; your audio never enters the building |
| **Quiztones-style plugins** | In the DAW | Narrow — usually frequency only |
| **abcTrain** | In the DAW, nine skills, your own audio, free, no account | No curriculum, no community, no accountability |

Three things are genuinely ours, and every feature should be paying into
one of them:

1. **It is where you work.** A plugin in the session, not a tab you
   deliberately open. The distance between "I should practise" and
   practising is one insert.
2. **It trains on your own material.** Import your music, and the
   exercises hide their changes inside the records you actually mix
   against. Pink noise teaches you to hear pink noise.
3. **The loop closes.** Learn to hear compression in the trainer, then
   open Learner Comp on a real track and see the meter agree with what
   you just heard. No other trainer owns both halves.

Everything else — themes, achievements, animation — is table stakes. It
buys the right to be taken seriously; it is not a reason to choose this.

## Why anyone comes back

The honest mechanism is not points. People return to things where they
can **see themselves getting better at something they care about**.
Everything below exists to make that visible, and each is deliberately
sized so it cannot become the point in itself.

### Within a minute: was I right, and by how much

Auto-advance, an accept band that shows where the answer actually was,
and points that scale with precision. A continuous exercise is not
pass/fail — landing 40 Hz out and dead on are different, and the score
says so. This is the feedback loop; everything else is scaffolding
around it.

### Within a session: something concrete is nearly in reach

Per-exercise levels with a promotion test. The tile always shows either
"20 / 100 to level 2" or "3 of 5 in a row" — so there is always a next
thing, and it is always small. Points alone would reward grinding
volume; a test alone would hand out levels on a lucky day. Together they
say *you have put the hours in, now show me.*

The run-results screen closes the session: what you scored, whether it
beat your record, and where your four skills stand — which is also the
answer to "what do I play tomorrow".

### Across weeks: a shelf that is not full

Achievements. Twenty-four of them, in four metals, three deliberately
near-unreachable. Their job is not to reward — it is to **name skills the
player did not know were separable**. "Counts the echo" and "Knows the
room" are two different abilities, and seeing them listed separately is
how someone learns that spotting a plate is not the same as spotting a
long hall.

That is why the achievements screen shows the locked ones with their
progress rather than hiding them: the unearned list is a **map of the
subject**. It says what there is to get good at. A wall of grey question
marks would say nothing.

The daily challenge and the streak sit alongside, and they are
deliberately mild: no lives lost for missing a day, no notification
begging you back. A streak someone is afraid to break is a streak that
turns practice into an obligation, and obligations get abandoned.

### Across months: the loop closes

The trainer teaches you to hear it; the Learner plugins let you use it on
your own audio while explaining what each control does. The day someone
reaches for Learner EQ because a mix sounds muddy — and knows which band
to grab because of an exercise — is the day the product actually worked.

## What is deliberately absent, and why

- **No percentiles or leaderboards.** There is no server. A fabricated
  "better than 80% of players" is a lie, and a real one is a different
  product with accounts, storage and a privacy position (see
  `roadmap.md`). Comparison against your own past is honest and, for a
  skill, more useful.
- **No streak punishment, no notifications.** See above. The app has no
  business making someone feel bad on a Tuesday.
- **No lootboxes, no currency, no levels you can buy.** Level is
  earned per exercise or it means nothing — which is exactly why the
  level dropdown was removed and why there is no "reach level N"
  achievement for a level you could pick from a menu.
- **No account required for anything.** Everything is a file on the
  player's own machine.

## How to judge a proposed feature

Three questions, in order:

1. **Does it make progress on a real skill more visible?** If it only
   makes numbers go up, it is a slot machine.
2. **Does it pay into one of the three advantages** — in the DAW, your
   own audio, the loop closing? If not, it is table stakes at best.
3. **Can it be told honestly?** If it needs a number nobody measured, it
   does not ship. This has already stopped percentiles twice, a
   stem-separator that would have mislabelled most music, and a
   star-gate that could not have been enforced.
