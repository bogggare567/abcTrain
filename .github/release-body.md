## Which file do I download?

| Your system | Download this | |
|---|---|---|
| **macOS** | `abcTrain-macOS-…dmg` | Intel and Apple Silicon |
| **Windows** | `abcTrain-Windows-…setup.exe` | 64-bit |
| **Linux** | `abcTrain-Linux-…tar.gz` | extract, run `./install.sh` |

One file. That is the whole install — all four plugins, in VST3, AU (macOS)
and as standalone apps you can open without a DAW.

**Prefer to try before installing?** The
[browser demo](https://bogggare567.github.io/abcTrain/) is the real trainer,
playable, no install.

---

## Your system will warn you, and the warning is honest

These builds are **not code-signed**. Signing means buying a certificate
issued against a verified legal identity — roughly $99/year from Apple,
$200–500/year from a Windows CA — and that has not been bought. The warning
says the publisher is unverified. It does not say anything was found wrong
with the file.

**macOS** — double-click the `.dmg`, then the `.pkg` inside it. macOS will
refuse it the first time. Open **System Settings → Privacy & Security**,
scroll to the bottom, and press **Open Anyway** next to the message about
abcTrain. Then open the `.pkg` again.

*(On older macOS a right-click → Open → Open also works. On Sequoia and
later it usually does not — use the Privacy & Security route.)*

**Windows** — SmartScreen shows a blue box. Click **More info**, then
**Run anyway**.

**Linux** — extract the archive and run `./install.sh`. It asks before it
writes anything.

---

## After installing

The installer puts things here:

| | macOS | Windows |
|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3` | `C:\Program Files\Common Files\VST3` |
| Audio Unit | `/Library/Audio/Plug-Ins/Components` | — |
| Apps | `/Applications/abcTrain` | `C:\Program Files\abcTrain` |

**Your DAW will not see the plugins until it rescans.** Most hosts do this
on the next launch; Ableton, Cubase and Studio One have a rescan button in
their plugin preferences. If a plugin does not show up, that is almost
always the reason — the
[Troubleshooting page](https://github.com/bogggare567/abcTrain/wiki/Troubleshooting)
covers the rest.

No DAW? Open **ABC Ear Trainer** from your Applications folder. It runs on
its own.

---

## Что скачивать

**macOS** — файл `.dmg`. **Windows** — `setup.exe`. **Linux** — `.tar.gz`.
Один файл, в нём все четыре плагина и приложения, которые работают без DAW.

Сборки не подписаны, поэтому система предупредит. На macOS: откройте
**Системные настройки → Конфиденциальность и безопасность**, промотайте
вниз и нажмите **«Открыть всё равно»**. На Windows: **«Подробнее» →
«Выполнить в любом случае»**.

После установки **DAW не увидит плагины, пока не пересканирует** — обычно
достаточно перезапустить её. Без DAW откройте **ABC Ear Trainer** из папки
с программами.

[Попробовать в браузере без установки](https://bogggare567.github.io/abcTrain/)

---
