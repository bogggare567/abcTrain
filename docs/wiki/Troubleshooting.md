# Troubleshooting

## My system says the developer is unverified

Expected. The builds are not code-signed. See
[Installation](Installation) for the exact click path per platform, and why.

## My DAW does not list the plugin

1. Check it installed where your host looks. Paths are in
   [Installation](Installation).
2. Rescan: most hosts cache the plugin list. In Ableton, Preferences →
   Plug-Ins → Rescan. In Reaper, Preferences → VST → Re-scan.
3. On macOS, check the format. AU and VST3 install separately - if you only
   ticked one in the installer, only that one exists.
4. Apple Silicon: the builds are native arm64. If your host is running
   under Rosetta it will only load Intel plugins.

## It crashes

**First: is your build current?** A use-after-free in the training-module
panel could kill Learner Comp and Learner Verb during a check. Fixed in
**v1.2.2**. If you are on v1.2.1 or earlier, update.

If it still crashes, a crash report is worth far more than a description,
because it names the line:

- **macOS**: `~/Library/Logs/DiagnosticReports/` - the newest file
  mentioning your host or `abcTrain`.
- **Windows**: Event Viewer → Windows Logs → Application, the newest Error
  from your host.
- **Linux**: run the host from a terminal and copy what it prints.

Open an [issue](https://github.com/bogggare567/abcTrain/issues/new/choose)
with that, your OS, host and version.

## No sound in Ear Trainer

The signal starts **off** and turns on when a training screen is open - a
plugin that makes noise the moment it loads is a plugin nobody keeps. Go to
an exercise. If it is still silent, check your host is routing the track's
output and that the standalone app has the right output device (its own
audio settings, not the OS default).

## No sound in a Learner plugin

They process the host's audio - if the track is silent, so are they. To hear
something without a track, set **source** in the title row to a category.
It is off by default on purpose.

## The stereo width exercise ignores my imported audio

Working as built, and a real limit. Clips are downmixed to mono when loaded,
and mono has no side channel to widen, so that exercise always uses two
decorrelated noise sources. See
[The nine exercises](The-nine-exercises).

## Where did my imported audio go

Training sounds → the path is printed on screen, with a button that opens
the folder.

## The update button says my version is "a development build"

You built from a clone with no tags fetched, so `git describe` had nothing
to describe against. Harmless. `git fetch --tags` and rebuild if you want a
real number.

## It downloaded an update but nothing changed

Restart your DAW. Nothing can replace a plugin binary the host currently
has loaded - that is how loaded libraries work, not a limitation of this
updater.

## I want to start over

Delete the settings file listed in
[Levels, streaks and achievements](Levels-streaks-and-achievements).
Everything - levels, achievements, imported-audio choices, theme - is in
that one file.
