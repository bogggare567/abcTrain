# Security policy

## What this software touches

Being honest about the surface first, because it is small and that matters
more than a long policy would:

- **Reads audio files you point it at.** Whatever `juce::AudioFormatManager`
  supports - wav, aiff, mp3, flac, ogg. A malformed file is decoded by JUCE
  and by the OS codecs underneath it.
- **Writes to two places only:** its own folder under your application data
  (settings and imported clips) and, on request, your Downloads folder when
  fetching an update.
- **Makes exactly one kind of network request:** an unauthenticated GET to
  `api.github.com` for the latest release, and a download from
  `github.com` if you accept the update. Both only when you press the
  button. There is no background timer and no telemetry.
- **Runs an installer you accepted.** The update button opens the
  downloaded installer; on Windows that installer requests elevation
  itself.

The builds are **not code-signed** - see the README. That is a known,
documented gap, not something to report.

## Reporting a vulnerability

Use GitHub's private reporting: **Security → Report a vulnerability** on
this repository. That keeps the report out of public view until there is
something to say about it.

If that is unavailable to you, email the repository owner. Please do not
open a public issue for anything exploitable.

### What to include

What you did, what happened, and on which version and OS. A crash that is
only a crash can go in a normal
[issue](https://github.com/bogggare567/abcTrain/issues/new/choose) - a
crash you can steer somewhere useful should not.

### What to expect

A reply from one person who also has a day. I will confirm receipt, say
whether I can reproduce it, and tell you plainly if it is something I
cannot fix - there is no security team here and pretending otherwise would
waste your time.

## Supported versions

The latest release only. This is a pre-release project with one maintainer;
backporting to older tags is not something I can honestly promise.
