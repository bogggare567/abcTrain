# Training on your own audio

*[По-русски](ru-Training-on-your-own-audio)*

Pink noise teaches you to hear pink noise. The point of importing your own
music is that the exercises then hide their changes inside the records you
actually mix against.

## Importing

Open **Training sounds** from the bar at the top of the window.

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

### Packs

A folder with a `pack.json` in it is a **pack**: a set of clips somebody
prepared, each with its tags (genre, vocal or instrumental, which
instruments) and its author and licence. The author and licence of every
clip are shown next to it and collected under **Settings → About → Sound
credits**. A pack clip without an author, or under a licence that does not
allow it to be shared this way, is not offered. How packs are built and
where their audio may come from: `docs/design/sound-library.md` and
`tools/library/` in the repository.

"Split into stems" was removed in 1.8: a separator without a trained model
could not do the job well enough to be worth its complexity, and real stems
come from sources that already have them.

## Choosing what to train on

Two panes. The left rail is **what to train on** - the exercise's own
sound, pink noise, the built-in synthesized categories, then everything you
imported and every pack.

- **The exercise's own sound** (the default) - each exercise plays material
  its skill is heard on: a chord for distortion (noise has no pitch, so no
  harmonics), a drum loop for compression, a single hit for reverb, pink
  noise for frequency, level and panning.
- **Pink noise** - everything on pink noise. The right pane is the
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
  exercise's own material, because both exercises are built around single hits
  with gaps where an envelope and a repeat are audible. A loop of finished
  music eats that.
