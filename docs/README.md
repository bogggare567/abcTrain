# Docs index

## Start here

| | |
|---|---|
| [orientation.md](orientation.md) | **The map.** Read before changing anything: the four load-bearing ideas, where to put a change, how to add an exercise, how to build and test. |
| [roadmap.md](roadmap.md) | What is actually built vs. planned. Honest about scope that was trimmed and why. |
| [testing-strategy.md](testing-strategy.md) | What's covered, what isn't, and the deliberate gaps. |

## Reference

| | |
|---|---|
| [knowledge_base.md](knowledge_base.md) | The audio-engineering material the in-plugin tooltips are written from. Original text, general practice, not derived from any one book. |
| [library_catalog.md](library_catalog.md) | Bibliography the tooltips' "learn more" lines point into. Titles and authors only — see [ADR 010](decisions/010-book-library-scope.md) for why no text was ever extracted. |
| [architecture.md](architecture.md) | The original pre-refactor design doc, kept for its rationale. `orientation.md` supersedes it for practical purposes. |

## Diagrams

[system-overview](diagrams/system-overview.md) ·
[game-engine](diagrams/game-engine.md) ·
[learner-plugin](diagrams/learner-plugin.md) ·
[i18n-architecture](diagrams/i18n-architecture.md) ·
[ci-pipeline](diagrams/ci-pipeline.md)

## Decisions (ADRs)

Every non-obvious choice, with the alternative that was rejected and why.
Written to be readable by someone who wasn't there.

| # | Decision |
|---|---|
| [001](decisions/001-game-interface.md) | The `Game` interface shape, over the alternatives |
| [002](decisions/002-difficulty-scaling.md) | `setDifficulty` on `Game`; points and levels kept out of it |
| [003](decisions/003-learnercomp-engine.md) | A custom compressor instead of `juce::dsp::Compressor` |
| [004](decisions/004-learnerverb-scope.md) | What was cut from LearnerVerb, and the decay approximation |
| [005](decisions/005-microlesson-architecture.md) | `MicroLesson` / `LessonController` split |
| [006](decisions/006-unified-visualization.md) | One visualisation shape across the Learner plugins |
| [007](decisions/007-update-checker.md) | Manual-only update checking, and why no background timer |
| [008](decisions/008-installers.md) | Per-OS installers, and what each platform can't do |
| [009](decisions/009-look-and-feel.md) | The shared theme, and the first pass's deliberate limits |
| [010](decisions/010-book-library-scope.md) | Why the book work stops at a bibliography |
| [011](decisions/011-i18n.md) | Flat-JSON-per-language i18n, and a real UTF-8 bug it caught |
| [012](decisions/012-versioning.md) | Version from `git describe`, and release channels |
| [013](decisions/013-ui-libraries.md) | Third-party UI libraries: what was tried, what was declined |
| [014](decisions/014-eartrainer-usability-fixes.md) | Bugs found by actually running the app |
| [015](decisions/015-choice-slider-and-training-sounds.md) | The answer slider, and opt-in reference audio |
| [016](decisions/016-icons-and-site-link.md) | Programmatic vector icons, and the limits of a design pass |
| [017](decisions/017-knowledge-base-content-pass-and-app-icons.md) | Folding in a knowledge base; real app icons |
| [018](decisions/018-ui-polish-and-builtin-samples.md) | Gradients/shadows/glow; built-in synthesized samples |
| [019](decisions/019-design-system-and-light-theme.md) | Design tokens, a designed light theme, eased widget state |
| [020](decisions/020-continuous-answers.md) | Continuous answers with a difficulty-scaled tolerance band |
| [021](decisions/021-sessions-and-navigation.md) | Training runs with a shape; Home → Training screens |
| [022](decisions/022-motion-audit-and-indicators.md) | The motion audit; compact indicators; a tint per exercise |
| [023](decisions/023-learner-plugin-visual-pass.md) | A colour per plugin, and a tool for actually looking at them |

## Repo-level files

[../README.md](../README.md) ·
[../CLAUDE.md](../CLAUDE.md) (per-file breakdown) ·
[../BETA_TESTING.md](../BETA_TESTING.md) ·
[../.github/CONTRIBUTING.md](../.github/CONTRIBUTING.md) (EN/RU) ·
[../LICENSE](../LICENSE)
