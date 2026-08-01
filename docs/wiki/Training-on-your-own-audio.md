# Training on your own audio

*[По-русски](ru-Training-on-your-own-audio)*

Pink noise teaches you to hear pink noise. The point of importing your own
music is that the exercises then hide their changes inside the records you
actually mix against.

## Importing

Open **Training sounds** from the bar at the bottom of the home screen.

**Add music…** opens a file picker - pick as many files as you like. Each
one is decoded, cut into eight-second loops, and each loop is sorted by
what it measurably *is*. Progress is shown; it runs on its own thread, so
the window stays alive.

Files are **copied** into the app's own folder. Your originals are never
moved, renamed or touched.

## How clips are sorted

By measurable character, into five folders:

| Character | What lands there | Good for |
|---|---|---|
| **Percussive** | dense transients with gaps between them | compression, delay, distortion |
| **Bass** | energy concentrated low | the bottom of the frequency exercises |
| **Mid range** | energy where voices and leads sit | general purpose |
| **Bright** | cymbals, air, bright synths | the top end |
| **Full range** | broad, even, usually wide - a finished mix | the realistic and hardest case |

### It does not separate stems

It cannot tell a vocal from a mix, and it does not pretend to. That is
source separation - a trained-model problem - and a heuristic pretending to
do it would mislabel most real music while sounding confident. A lead vocal
in a sparse arrangement usually lands in *mid range*; the same vocal over a
dense mix lands in *full range*, correctly, because that is what the audio
is.

## Choosing what to train on

Two panes. The left rail is **what to train on** - pink noise, the built-in
synthesized categories, then everything you imported. The right pane is the
**actual files** in whichever one is selected.

- Click a **category** → the exercises shuffle through its clips, a
  different one each round. This is the default and the better one: twenty
  drum loops should be twenty drum loops.
- Click a **file** → training pins to that clip alone. Click the category
  again to go back to shuffling.

The footer says which of the two you are in, and the folder path is on
screen with a button that opens it.

## What is bundled

Five originally-synthesized clips in two always-present categories, so
there is a real non-noise option with zero setup. Nothing else - this
project does not fetch, bundle or vet the legality of any audio. What you
put in your library is yours and your responsibility.

## Known limits

- **Stereo width always uses pink noise.** Clips are downmixed to mono on
  load, and mono has no side channel to widen.
- **Compression and delay are harder to hear on a dense mix** than on the
  synthesized bursts, because both exercises are built around single hits
  with gaps where an envelope and a repeat are audible. A loop of finished
  music eats that.
