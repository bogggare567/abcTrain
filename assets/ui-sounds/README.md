# The app's own sounds (ADR 054)

Short one-shots the app plays on its own events. Bogdan Korablev's
recordings, CC BY 4.0.

A file is `<event>-<n>.flac` (or .wav): the event, then the take number.
Several takes per event are the point — the app never plays the same take
twice in a row, moves each one by up to ±35 cents and ±1.5 dB, and makes a
quick repeat of the same event quieter (`shared/audio/UiSounds`).

| event | when |
|---|---|
| correct | an answer inside the band — one take, always the same; off by default |
| wrong | an answer outside it — one take (a rim), always the same |
| step-up | the staircase took a step harder |
| new-record | the personal best moved |
| achievement | something earned (the toast) |
| run-end | a blitz or survival run is over |
| battle-won / battle-lost | the end of a battle |
| round-start | a battle round begins |
| open | the lessons window or a results card opens |

Keep each take under a second, peaks around −6 dBFS, no silence at the head.
`tools/ui_sounds/import.py <folder>` trims, normalises and converts a folder
of takes into this layout.
