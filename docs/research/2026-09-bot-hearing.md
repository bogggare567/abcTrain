# Bot listeners: what the animals actually hear (2026-09-25)

The data behind the six bots (ADR 046) and behind
`tools/bots/train_bots.py`. Each number has a source; **NV** means a value
seen only second-hand or not confirmed, **not found** means the literature
search found nothing usable. Sources marked "via" were read through a later
paper or a figure page, not the original.

## Human reference

| | Value | Source |
|---|---|---|
| Range | 20 Hz – 20 kHz; best 2–5 kHz | [Hearing range](https://en.wikipedia.org/wiki/Hearing_range) |
| Minimum audible angle (MAA) | ~1° at 500 Hz, ~3.5° at 8 kHz | [PLOS ONE 2019](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0220652); [Heffner, Acoustics Today](https://acousticstoday.org/wp-content/uploads/2016/01/The-Evolution-of-Mammalian-Sound-Localization.pdf) |
| Level JND | ~1 dB | [Britannica](https://www.britannica.com/science/frequency-just-noticeable-difference) |
| Frequency JND | ~0.5 % | same |
| Gap detection | 2–3 ms | [Frontiers 2014](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2014.00763/full) |

## The six

| Bot | Species the data are from | Audiogram | Localisation | Timing |
|---|---|---|---|---|
| Hound | dog | 63 Hz – 47 kHz at 60 dB; best 8 kHz, 4 dB (Heffner 1983 via [Guérineau 2024](https://www.research.unipd.it/retrieve/9df8011a-4885-4740-9bbd-e33c27e82f94/Guerineau%20et%20al.%202024%20Vetrinary%20Sciences.pdf)) | MAA 7.6° ± 3.4° ([MDPI 2022](https://www.mdpi.com/2306-7381/9/11/619)) | not found |
| Cat | domestic cat | 48 Hz – 85 kHz at 70 dB, good 0.5–32 kHz ([Heffner & Heffner 1985](https://www.sciencedirect.com/science/article/abs/pii/0378595585901005)) | MAA ~5° (Heffner, Acoustics Today) | not found |
| Viper | **royal python** — no viper data found | best 80–160 Hz at ~78 dB SPL; senses head vibration (−54 dB re 1 m/s²), not pressure; upper limit ~1 kHz **NV** ([Christensen et al. 2012, JEB](https://journals.biologists.com/jeb/article/215/2/331/11089/Hearing-with-an-atympanic-ear-good-vibration-and)) | not found | not found |
| Owl | barn owl | best 4–8 kHz, ~0 dB SPL (Konishi 1973 via [Saberi 1998](https://sites.socsci.uci.edu/~saberi/saberietalpnas1998.pdf)); range ~0.2–12 kHz **NV** | MAA 4° broadband ([PLOS ONE 2019](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0220652)); head-turn accuracy best at 4–8 kHz ([Knudsen & Konishi 1979](https://authors.library.caltech.edu/62719/)), 1–2° **NV** | ITD range ±170 µs; frequency JND 0.5–0.7 % ([Springer](https://link.springer.com/article/10.1007/BF00606802)) |
| Bat | big brown bat | 0.85 kHz at 106 dB … best 20 kHz at 7 dB (Koay 1997, via figure page) | MAA 14° for noise ([Koay 1998](https://pubmed.ncbi.nlm.nih.gov/9641317/)) | echo-delay jitter 10–15 ns — disputed, echolocation only ([Simmons 2003](https://link.springer.com/article/10.1007/s00359-003-0444-9)) |
| Elephant | Indian elephant, **one animal** | 16 Hz at 65 dB … 10.5 kHz at 60 dB; best 1 kHz, 8 dB ([Heffner & Heffner 1980](https://en.wikipedia.org/wiki/Perception_of_infrasound)) | MAA ~1°, time and level cues ([Heffner & Heffner 1982](https://eurekamag.com/research/021/121/021121772.php)) | not found |

## What this changed

The first version of the bots (ADR 046, 2026-09-24) was drawn from the
popular picture of these animals. Three things from the data contradicted it.

1. **The elephant, not the owl, localises like a human.** The elephant's MAA
   is about 1°. The owl's is about 4° for broadband noise. The owl's fame is
   absolute accuracy when turning its head in the dark, not telling two
   close sources apart. The elephant now leads on pan, width and reverb.
   The owl keeps presence (4–8 kHz) and direction, but not the whole space
   family.
2. **The snake hears bass, not distortion.** The python's hearing sits at
   80–160 Hz and fades out by about 1 kHz. Clipping harmonics land at
   2–10 kHz, which it cannot hear. The Viper is now the sub-bass specialist
   and nearly deaf above 1 kHz. The only snake with published hearing data
   is a python; the name stays for character, and this file says so.
3. **The bat is poor in the human range.** Its best hearing is at 20 kHz.
   At 850 Hz it needs 106 dB. What it keeps is timing: delay and attack,
   from the echo-delay literature. This is an extrapolation and is marked
   as one below.

## What is a game choice, not biology

- **Overall strength** (the mean threshold, 5.2–6.1 levels) is set so every
  bot is a fair opponent. A real bat would lose every EQ round.
- **Timing ability.** Level JND and gap detection were not found for any of
  the six animals. The "temporal" score is bat > owl > cat = dog = human >
  elephant > python, from the echo-delay and ITD literature. That ordering
  is an extrapolation.
- **Adjustments inside each exercise**, the same for every bot:
  - cuts are harder than boosts;
  - weak compression is harder than strong;
  - panning near the sides is harder than near the centre (the human MAA
    grows away from the centre).
- **The python's localisation** is set to the worst of the six (16°): no
  data exists.

## From data to a bot

`tools/bots/train_bots.py` builds a teacher from the table above. It then
trains a one-neuron perceptron with a logistic output (the psychometric
function) by maximum likelihood on 40 000 simulated rounds per bot.

- **Teacher.** Every part of every exercise gets a threshold, taken from
  the relevant data: the audiogram at the band's centre frequency, the MAA
  for pan and width, and the timing score for delay and compression.
- **What the student recovers.** On teacher data the student recovers the
  thresholds to within about 0.1–0.16 of a level. That shows the model and
  the optimiser work; it is not a claim that the bots learned something
  the teacher did not already contain.
- **Next step.** The same fit on real rounds (`--answers
  game,bucket,level,correct`) is how a bot is later fitted to the pilot
  group, or how a "twin" of a player is made.
