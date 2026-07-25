# 010. Book library: bibliography only, no text extraction or quoting

## Status

Accepted, catalog implemented. Original-text tooltip/lesson expansion is
future work — see "What's next" below.

## Context

The user has a local collection of ~150 professionally published
audio-engineering books (Bob Katz, Bobby Owsinski, Roey Izhaki, Mike
Senior, Philip Newell, Irina Aldoshina, F. Alton Everest, and many more),
kept at `~/Desktop/abcTrain/Книги/` — a sibling of this git worktree, not
inside it. The original ask was a multi-stage pipeline: extract full text
from every PDF/DjVu/EPUB, build a topic-organized `knowledge_base.md` of
each book's key ideas, and then rewrite every in-plugin tooltip and
lesson step to quote those books directly with attribution (e.g. "по Бобу
Овсински... (Овсински, Настольная книга звукорежиссёра)").

That pipeline has a real copyright problem that attribution doesn't
solve: extracting and republishing quoted/closely-paraphrased content
from ~150 copyrighted books into a commercial product's shipped UI - one
distributed publicly on GitHub - is mass reproduction of others'
published work, not fair use, regardless of whether each excerpt names
its source. This was flagged to the user directly before any extraction
happened; they chose the copyright-safe path (bibliography + originally
written content) over either doing the extraction pipeline as originally
specified or dropping the book-library idea entirely.

## Decision

**No book text is ever extracted, stored, or reproduced.** Only
filenames were read (never file bodies) to build
[docs/library_catalog.md](../library_catalog.md) - a topic-bucketed list
of title/author/format for all 158 books, auto-categorized by keyword
match against each filename. This is a bibliography, not a summary of
any book's content - nothing in it was derived from reading a single page
of any book.

**In-plugin tooltips/lessons get original content, not quotes.** Where
this project wants to go deeper than the current one-line tooltips, the
text should be written from general, widely-known audio-engineering
principles (the same way `CompressorGuide`/`ReverbGuide`/`FrequencyGuide`
were already written - nobody derived those from a specific book), with
an optional "further reading" pointer naming a relevant book from the
catalog by title/author only - never a quote, never a close paraphrase of
a specific passage.

**The books themselves never enter this repository.** They live outside
the git worktree entirely (`~/Desktop/abcTrain/Книги/`, 2.7 GB, 159
files) - there was never a `.gitignore` entry to write, because nothing
from that folder was ever staged for commit.

## What's next

Rewriting every tooltip/lesson across all three Learner plugins plus
EarTrainer's 8 games with richer *original* explanatory text (not book
quotes) is real, substantial work, and hasn't been done yet as of this
ADR - the catalog above is the first deliverable. See
[roadmap.md](../roadmap.md) for tracking.

## Consequences

- The product's teaching content stays legally safe to ship: original
  writing plus a plain bibliography, not reproduced copyrighted text.
- The catalog's topic buckets are a best-effort keyword match on
  filenames, not an editorial read of every book - a handful of entries
  likely belong under a different heading. Good enough for "which shelf,"
  not a rigorous bibliography.
- If the user later wants literal quotes from specific books (e.g. a
  "recommended reading" excerpt on their own website, not inside the
  shipped plugin), that's a separate, much smaller-scope request with its
  own fair-use analysis - not something this ADR opens the door to.
