# 052 — Course lessons: text files, a window in the Studio, the plugin turns its own knobs

Date: 2026-10-02. Status: accepted (Bogdan, on the map: «уроки не выносить
отдельно — окно в Студии; 3–5 минут чтения; анимация и визуализация;
сделай так, чтобы уроки удобно было сделать, если будут коллаборации со
звукорежиссёрами; читай книги»).

## Problem

The Learner plugins already teach knob by knob (modules, ADR 037) and
carry three or four walkthroughs each. What was missing is the other half
of the craft: how a vocal chain is built, where a reverb is routed, why a
high-pass goes before the compressor. Those are short courses, and they
will be written by more people than the developer — a guest sound engineer
should be able to add one. A lesson written in C++ (MicroLesson) cannot be
written by them, and its English-only text went through a translation
table that grew with every step.

## Decision

- **A lesson is a text file**, `lessons/<course>/NN-slug.lesson`, in the
  format of `lessons/FORMAT.md` (written for authors, in Russian): a header
  (id, course, order, minutes, plugin, material, author, sources, titles)
  and steps. A step is Russian and English text plus one-line commands that
  say where the knobs go: `@eq 1 bell 300 -4 1.4`, `@comp ratio=4
  attack=10`, `@sidechain kick hpf=120`, `@verb type=plate routing=send`,
  `@bypass on`, `@highlight 200 450`, `@material vocal`. Values are in the
  knob's own units, so an author writes what they would say aloud.
- **Seven courses**: EQ in practice, compression, reverb, routing, the
  channel chain, vocals, buses and the mix bus — 22 lessons to start, 3–5
  minutes each, written from the books in `materials/Книги` (Owsinski,
  Izhaki, Senior, Stavrou, Gibson, the Systematic Mixing Guide, Katz) and the
  owner's knowledge base, in our own words, with the chapter named in
  `sources`.
- **The window**, `Source/LessonsPanel`, opens from the Studio bar
  («Уроки»). It is a desktop window of its own, not a page, because the
  point is to watch the plugin while reading: courses on the left with
  progress, the lesson on the right — the step being read in large type, all
  steps listed below, any step clickable, sources under them. Opening a
  lesson brings the Studio up on that lesson's plugin.
- **The knobs travel** (`Source/LessonRunner`): every float parameter glides
  to its value over 700 ms through the host-notifying path, so the plugin's
  own knob is seen turning; switches and choices change at once.
  `@highlight` lights a band on Learner EQ's spectrum. A step says only what
  changes; the state at step n is every step up to n replayed, so going back
  sounds like going back.
- **Material**: a real voice for vocal lessons — Bogdan Korablev's live
  vocal, CC BY 4.0, two phrases embedded (`assets/lesson-media`, also the new
  `LessonAudioBed::Bed::vocal`) — and the app's own synthesized instruments
  and loops for the rest, so no lesson needs an imported library.
- **Languages**: every lesson carries Russian and English; other interface
  languages read the English, and the window says so. Twelve translations of
  long-form text per lesson would make adding a lesson a translation project,
  which is exactly what this format is meant to avoid.
- **Checked like code**: `tests/LessonFormatTest` parses every shipped file;
  an unknown command, a value outside the knob's range, a step without
  English, a parameter that does not exist, two lessons with one id — each
  fails the build with the file and the line.

The built-in walkthroughs were corrected in the same pass: the vocal ones
play the vocal instead of a chord or a bass note, Vocal EQ cuts before it
boosts, and the plate for a voice sits on a send.

## Not done

A slope command for high-pass and low-pass (the EQ has a per-band Slope;
the high-pass lesson explains it in words); the mockup's «Модули» and
«Словарь» tabs; lessons in languages other than Russian and English;
loading lessons from a folder at run time, so a guest author can try a file
without a build (today the file goes into the repository and the build).
