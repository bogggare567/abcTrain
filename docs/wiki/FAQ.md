# FAQ

**Is it really free?**
Yes, and staying that way. No paid tier, no subscription, nothing behind
anything. The licence is all-rights-reserved (see `LICENSE`) - the source is
open to read and build, not to redistribute.

**Do I need an account?**
No, and there is nowhere to make one. No server exists.

**Does it send anything anywhere?**
Only when you press the update button, which asks GitHub whether there is a
newer release. Nothing else, ever. No telemetry.

**Why is it not signed?**
Signing means buying a certificate issued against a verified legal identity
- around $99/year from Apple, $200-500/year from a Windows CA. That has not
been bought yet. You can always build from source instead.

**Will my progress survive an update?**
Yes. It lives in a settings file no installer touches.

**Which is better, VST3 or AU?**
No difference here. Use whichever your host prefers.

**Can I use the Learner plugins as normal effects on a real mix?**
Yes. They are real DSP with host-automatable parameters that save with the
session. The teaching is additive, not a mode.

**Why per-exercise levels instead of one?**
Because being good at panning says nothing about hearing 400 Hz. One number
would average away the only useful information.

**Why no leaderboard?**
There is no server, so a comparison against other people would either be
fabricated or require a completely different product with accounts and a
privacy position. Comparison against your own past is honest and, for a
skill, more useful.

**Can I get it in my language?**
Twelve are built in: English, Russian, German, French, Spanish, Portuguese,
Italian, Polish, Ukrainian, Simplified Chinese, Japanese, Korean. Nine of
them have only been machine-checked - corrections are a very welcome pull
request, one JSON file in `shared/i18n/strings/`.

Parameter tooltips, lesson text and the training modules are still
English-only.

**Can I add an exercise?**
Yes, and the process is short: implement the `Game` interface, **append** it
to `GameManager` (never insert - per-exercise stats are keyed by index), add
it to `CMakeLists.txt` and to `categoryForGame()`. `CLAUDE.md` has the
detail.

**Where do I ask something not answered here?**
[Discussions → Q&A](https://github.com/bogggare567/abcTrain/discussions).
Something broken goes in
[Issues](https://github.com/bogggare567/abcTrain/issues/new/choose).
