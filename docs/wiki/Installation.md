# Installation

## Download

Every release has an installer per platform, built by CI from the exact
commit the tag points at:

| Platform | File |
|---|---|
| macOS | `abcTrain-macOS-<version>.dmg` |
| Windows | `abcTrain-Windows-<version>-setup.exe` |
| Linux | `abcTrain-Linux-<version>.tar.gz` |

**[Latest release](https://github.com/bogggare567/abcTrain/releases/latest)**

The `plugins-*.zip` files on the same page are the raw built plugins with
no installer, for people who prefer to place them by hand.

## Your system will warn you. Here is why

These builds are **not code-signed**. Signing means buying a certificate
issued against a verified legal identity - roughly $99/year from Apple,
$200-500/year from a Windows CA. That has not been bought, so every OS
treats the download as coming from someone it cannot name.

The warning is accurate. It says the publisher is unverified, not that
anything was found wrong with the file.

### macOS

Right-click (or Control-click) the `.pkg` inside the disk image → **Open**
→ **Open** again. If macOS refuses outright, go to **System Settings →
Privacy & Security**, scroll to Security, and press **Open Anyway**. On
Sequoia and later that button is the only route for some downloads.

The installer lets you pick which of the four plugins to install and in
which formats, and whether to install for all users or just you.

### Windows

A blue "Windows protected your PC" panel appears. Click **More info**,
then **Run anyway**. If your browser blocked the download itself, use
*Keep* → *Keep anyway* in the downloads list.

### Linux

No gatekeeper. Extract the tarball and run `./install.sh` inside - it asks
which plugins you want and where the VST3s should go (`$HOME/.vst3`,
`/usr/lib/vst3` via sudo, or a path you type). If it will not run,
`chmod +x install.sh` first.

## Where things end up

| | macOS | Windows | Linux |
|---|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3` | `Common Files\VST3` | `~/.vst3` |
| AU | `/Library/Audio/Plug-Ins/Components` | — | — |
| Standalone | `/Applications` | `Program Files\abcTrain` | your chosen path |

Your progress, levels, achievements, language and theme live somewhere
else entirely and no installer touches them:

- macOS: `~/Library/Application Support/abcTrain/`
- Windows: `%APPDATA%\abcTrain\`
- Linux: `~/.config/abcTrain/`

## Updating

Press the download icon in any plugin's title row. It checks for a newer
release, and if there is one it offers to fetch the installer for your
system, downloads it with progress, and opens it.

It cannot replace a plugin your host already has loaded - no program can,
that is how loaded libraries work - so **restart your DAW** afterwards to
pick up the new version. Your progress survives.

## Building from source

Three commands, and it is the same binary without any warning to click
through:

```
git clone https://github.com/bogggare567/abcTrain.git
cd abcTrain
cmake -B build && cmake --build build
```

CMake fetches JUCE itself; no local checkout is needed. On Linux install
`libcurl4-openssl-dev` first.
