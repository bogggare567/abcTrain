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

### Split into stems

**Add music** sorts whole slices by character and never separates
anything. **Split into stems** does: each track is divided into four stems
and each stem is cut into loops of its own.

| Stem | How it is found |
|---|---|
| **Drums** | the percussive part of a harmonic/percussive split (what changes fast in time, not in pitch) |
| **Bass** | the harmonic part below about 180 Hz |
| **Centre (vocal)** | the rest of the harmonic part that sits in the middle of the stereo image |
| **Sides (wide)** | what is left: the harmonic part that is not in the middle |

The four always add back up to the original. It is an estimate from the
signal itself, **not a trained model**, and it says so: "centre" means "in
the middle of the image", so a centred synth lands with the vocal, and a
snare's tail or a strummed guitar can split between stems. On a mono file
there are no sides. Tracks longer than eight minutes are separated up to
the eighth minute.

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
