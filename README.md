<div align="center">

**English** · [Русский](README.ru.md)

<br>

# abcTrain

### Learn to hear what you have been reading about.

Nine ear-training exercises, and three real effects that explain themselves
while they work on your own audio. Free, open source, macOS · Windows ·
Linux, as VST3 · AU · Standalone.

<br>

[![Download](https://img.shields.io/badge/Download-macOS%20%C2%B7%20Windows%20%C2%B7%20Linux-5B9BD5?style=for-the-badge&labelColor=1E1E2E)](https://github.com/bogggare567/abcTrain/releases/latest)
&nbsp;
[![Try it in your browser](https://img.shields.io/badge/Try%20it%20first-no%20install-2A2A3A?style=for-the-badge&labelColor=1E1E2E)](https://bogggare567.github.io/abcTrain/)

<br>

[![Build](https://img.shields.io/github/actions/workflow/status/bogggare567/abcTrain/build_and_test.yml?style=flat-square&labelColor=1E1E2E&color=5B9BD5&label=build)](https://github.com/bogggare567/abcTrain/actions/workflows/build_and_test.yml)
[![CodeQL](https://img.shields.io/github/actions/workflow/status/bogggare567/abcTrain/codeql.yml?style=flat-square&labelColor=1E1E2E&color=5B9BD5&label=codeql)](https://github.com/bogggare567/abcTrain/actions/workflows/codeql.yml)
[![Release](https://img.shields.io/github/v/release/bogggare567/abcTrain?style=flat-square&labelColor=1E1E2E&color=5B9BD5&label=release)](https://github.com/bogggare567/abcTrain/releases/latest)
[![Licence](https://img.shields.io/badge/licence-all%20rights%20reserved-2A2A3A?style=flat-square&labelColor=1E1E2E)](LICENSE)

[Wiki](https://github.com/bogggare567/abcTrain/wiki) ·
[Discussions](https://github.com/bogggare567/abcTrain/discussions) ·
[Telegram](https://t.me/vstabc) ·
[Roadmap](docs/roadmap.md)

</div>

---

## Why this exists

The first time somebody tells you a mix is muddy, you nod. Then you open
the session at home and hear… a mix. Not mud. So you look it up, learn that
mud lives somewhere around 250–400 Hz, sweep a filter through there — and
you still cannot hear the thing you are meant to be removing. You can only
hear that *something* changed.

That is not a knowledge problem. Everything you need to know is written
down, free, a hundred times over, by people who explain it well. It is a
**hearing** problem, and hearing is a physical skill. It comes from doing
one small thing a few hundred times with something telling you immediately
whether you got it right.

Almost nobody gets that, because building the exercise is harder than doing
it. You would have to generate a sound, hide a change inside it, make sure
that change is the *only* difference, and then grade yourself honestly
without peeking. By the time you have set it up you have lost the evening
and trained nothing.

abcTrain is that exercise, already built. And then — the part most ear
trainers leave out — the same vocabulary again in three plugins you can put
on a real track, so the thing you just practised is the thing you are now
doing.

## Who it is for

**You mix your own music and you can follow a tutorial, but you cannot hear
what it is about.** You are copying moves. This is the fastest way out of
that, and ten minutes a day is enough.

**You have been mixing a while and you are tired of guessing.** You know
what a compressor does. You want to stop reaching for −6 dB because it
worked last time and start hearing what the track is asking for.

**You teach, or you are learning alongside someone.** Every exercise
reports a number, so "getting better" stops being a feeling.

It is probably **not** for you if you want a course with a syllabus and a
certificate at the end. There isn't one, on purpose.

## What it actually does

**Train.** Nine exercises. Each hides a change inside a sound and asks what
it was. You answer, and it tells you straight away whether you were right —
and by how much you missed, in the unit that matters: octaves for
frequency, decibels for level, a ratio for delay time.

**Then apply it.** Three companion plugins process your *own* audio — a
graphical EQ, a compressor and a reverb — each with plain-language
explanations on every control and step-by-step lessons built in. They are
real effects with host-automatable parameters that save with the session;
the teaching is added on top, not a mode you switch out of.

Nothing is locked. No account, no subscription, no telemetry. The only
network call in the entire suite is the update check, and only when you
press the button.

## The nine exercises

Four ask for a **value**, answered by dragging along a scale. You do not
have to be exact — landing inside the tolerance band counts, and landing
closer counts for more. The level narrows that band, so harder means *more
precise*, not *quieter*.

| Exercise | What you are hearing |
|---|---|
| Find the frequency | Which frequency got boosted or cut, anywhere in 100 Hz – 12.8 kHz |
| Guess the pan position | Where it sits across the stereo field |
| Guess the gain change | How far the level moved, −9 … +9 dB |
| Guess the delay time | How long the echo is, 20 – 640 ms |

The other five ask you to **name** a thing, and always give you exactly two
alternatives — at every level, from your first round to your last.

| Exercise | What you are choosing between |
|---|---|
| Guess the compression | Two of: weak · medium · strong |
| Guess the reverb | Two of: room · chamber · hall · plate · spring |
| Guess the distortion | Two of: soft clipping · hard clipping · tape · overdrive |
| Guess the stereo width | Two of: narrow · normal · wide · extra wide |
| Name the range | Two of the seven standard ranges: sub-bass … air |

**Why never a third button.** It would make the round harder by giving you
more to read and improving your odds of a lucky guess, and neither of those
is your ear getting better. What the level changes instead is *which* two
you are asked about: level 1 offers a cathedral against a broom cupboard,
level 10 offers two things that genuinely take work to separate. A fixed
50/50 also means a streak of six says the same thing at level 9 as it did
at level 2.

**Every answer is a family, not a preset.** "Hall" is not one recording with
a wobble on it. A tiled booth and a big live room are both rooms, and
someone who can only recognise one of them has not learned what a room
sounds like — so the harder the level, the closer to the boundary the
example sits.

Runs have a shape: **Practice** is unlimited, **Survival** gives you three
lives, **Blitz** is a ninety-second clock where a wrong answer costs five
seconds. Each exercise keeps its own level and its own record, because
being good at panning says nothing about hearing 400 Hz.

You can train on pink noise, on the built-in synthesized clips, or on **your
own music** — imported, cut into loops and sorted by what each loop
measurably is.

## The three teaching plugins

| | |
|---|---|
| **Learner EQ** | A graphical EQ: up to eight bands of any type — bell, shelf, high-pass, low-pass, notch — added and removed on the curve itself. The spectrum is labelled in *sensations* as well as numbers: Sub, Bass, Boom, Body, Honk, Presence, Sibilance, Air. |
| **Learner Comp** | A compressor with a soft-knee engine and a gain-reduction meter that fills **downward**, because that is the direction the sound goes. Seven training modules, one per control. |
| **Learner Verb** | Room, hall, plate and spring, with decay, pre-delay, size, damping, mix and width. Seven modules of its own. |

A module is not just a lesson. After the explanation, the plugin sets the
control to a value it does **not show you**, plays it, and you turn its own
knob until it matches by ear. The accept band differs per control on
purpose: being 5 ms out on a 3 ms attack and on a 300 ms attack are
completely different mistakes.

## What it is not

- **Not a course.** No syllabus, no certificate, no lesson plan.
- **Not a mixing tutor.** It trains hearing; what you do with it is yours.
- **Not signed.** Your system will warn you the first time — see below.
- **Not connected to anything.** No account, no server, no telemetry. Your
  progress is a file on your own machine.
- **It does not separate stems.** Imported music is sorted by measurable
  character, not by instrument, and it does not pretend otherwise.

## Getting it

[**Download the latest release**](https://github.com/bogggare567/abcTrain/releases/latest)
— a real installer per platform, built by CI from the exact commit the tag
points at.

**Your system will warn you, and the warning is honest.** These builds are
not code-signed: signing means buying a certificate issued against a
verified legal identity, roughly $99/year from Apple and $200–500/year from
a Windows CA, and that has not been bought. The warning says the publisher
is unverified — not that anything was found wrong with the file.

- **macOS** — right-click the `.pkg` → **Open** → **Open** again.
- **Windows** — **More info** → **Run anyway**.
- **Linux** — extract and run `./install.sh`.

The [Installation](https://github.com/bogggare567/abcTrain/wiki/Installation)
page walks through all of it, including install paths and where your
progress lives. Or **build from source** — three commands, and it is the
same binary with nothing to click through:

```bash
git clone https://github.com/bogggare567/abcTrain.git
cd abcTrain
cmake -B build && cmake --build build
```

CMake fetches JUCE itself; no separate install needed. On Linux, add
`libcurl4-openssl-dev` first. [Testing and build
detail](docs/testing-strategy.md).

## Languages

The interface ships in twelve: English, Русский, Deutsch, Français,
Español, Português, Italiano, Polski, Українська, 简体中文, 日本語, 한국어.
Detected from your system on first run, changeable in the bottom bar.

Honestly: only English and Russian have been checked by a speaker. The
other ten are machine-checked, and a correction is one JSON file in
`shared/i18n/strings/` — a genuinely useful pull request. Parameter
tooltips, lesson text and the training modules are still English-only.

## Documentation

| | |
|---|---|
| [**Wiki**](https://github.com/bogggare567/abcTrain/wiki) | The manual, for people **using** it — install, first ten minutes, every exercise, your own audio, levels, troubleshooting. Also [in Russian](https://github.com/bogggare567/abcTrain/wiki/ru-Home). |
| [docs/orientation.md](docs/orientation.md) | The map, for people **changing** it. Short. Read it first. |
| [docs/decisions/](docs/decisions/) | Every non-obvious choice, with the alternative that was rejected and why. |
| [docs/user-journey.md](docs/user-journey.md) | What this product is for, and the three questions any proposed feature has to answer. |
| [CLAUDE.md](CLAUDE.md) | Per-file map of the whole repository. |

## Where to ask what

| You have | Go here |
|---|---|
| a question about using it | [Discussions → Q&A](https://github.com/bogggare567/abcTrain/discussions/categories/q-a) |
| something broken | [Issues](https://github.com/bogggare567/abcTrain/issues/new/choose) |
| an idea | [Discussions → Ideas](https://github.com/bogggare567/abcTrain/discussions/categories/ideas) — start with the problem, not the solution |
| something you made with it | [Discussions → Show and tell](https://github.com/bogggare567/abcTrain/discussions/categories/show-and-tell) |
| to hear about new versions | [Telegram → @vstabc](https://t.me/vstabc) |

## Contributing

Good first contributions, roughly in order of how self-contained they are:
a **translation** (one JSON file in `shared/i18n/strings/`), a **new
exercise** (seven steps, all in [docs/orientation.md](docs/orientation.md)),
or anything on the [roadmap](docs/roadmap.md).

One thing that will bite you if nobody says it: **the tests cannot see
layout.** Every visual pass in this project's history has shipped a bug
that compiled, passed all 217 test groups, and was obvious ten seconds
after looking at the thing. If you change something visual, render it and
look at it.

[.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) covers how pull requests
are shaped here, and there are issue templates for
[bugs](.github/ISSUE_TEMPLATE/bug_report.md) and
[features](.github/ISSUE_TEMPLATE/feature_request.md).

## Supporting it

Free, open source, and staying that way — no paid tier, no subscription,
nothing locked behind anything.

If it is useful to you, a **[star](https://github.com/bogggare567/abcTrain)**
helps other people find it and a
**[donation](https://www.donationalerts.com/r/bogdankorablev)** keeps it
moving. Both are optional and neither unlocks anything, because gating a
learning tool behind a favour is the opposite of the point.

<div align="center">
<br>

*ambiance · balance · clarity*

</div>
