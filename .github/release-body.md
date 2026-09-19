## What's new in 1.6.0

**Ear Trainer**
- **Levels are a staircase now.** Three right in a row make an exercise harder, one wrong makes it easier, so each exercise settles at your real threshold — shown in its own units ("±0.35 oct", "±1.2 dB", "Room vs Chamber"). Your record never drops. Points and the promotion test are gone.
- **Home is a list of thresholds**, one row per exercise with a ten-step ruler.
- **Achievements in two layers:** 12 milestones and 53 stamps. None asks for lifetime accuracy.
- **Beginner / Pro.** Beginner keeps every rule standard; Pro opens them (step-up rule, pauses, lives, Blitz clock, hints).
- **Hearing protection**, on by default and optional: a break reminder after an hour without ten quiet minutes, a tired-ear hint, and — after a one-time calibration — your weekly dose against the WHO / ITU-T H.870 limit.

**Learner EQ · Comp · Verb**
- **Modules climb the same staircase**, ten steps, with the band in the knob's own units; the result says how far out you were and what the staircase did. **Learner EQ has modules now** (frequency, gain, Q, high-pass).
- **Learner Verb:** a new engine — FDN room and hall, Dattorro plate, springs. **Decay is measured seconds**, **Size** changes the room, and the screen shows the **echogram** of the current setting.
- **Learner Comp:** the **transfer curve**, drawn by the formula the audio runs through.
- **A/B** in all three, saved with your project. The family colour everywhere, a card-grid module shelf, and **every string in all 12 languages**.

<details><summary>Что нового в 1.6.0 (по-русски)</summary>

**Тренажёр:** уровень — лестница (три верных подряд — выше, ошибка — ниже), порог в единицах упражнения, рекорд не падает; главная — список порогов; достижения в два слоя (12 вех, 53 отметки); режимы «Новичок / Профи»; защита слуха по нормам ВОЗ / ITU-T H.870 — включена по умолчанию, отключается.

**Обучающие плагины:** модули идут по той же лестнице с допуском в единицах ручки, у Learner EQ появились модули; у Learner Verb новый движок — затухание в измеряемых секундах, размер меняет комнату, на экране эхограмма; у Learner Comp — передаточная кривая; A/B во всех трёх; всё на 12 языках.

</details>

---

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
