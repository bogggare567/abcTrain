## What's new in 1.7.0

**It fits your screen.** The window used to open at its design size, which was
also its smallest — 1180 × 880 for the trainer, more than a 13-inch laptop has
room for once the menu bar and dock are gone. Now the design size is what a
window *prefers*: it opens at whatever the display can show and goes no lower
than a floor each layout is actually built to work at (940 × 620 for the
trainer, 820 × 600 for the Learner plugins). A size you chose on a desktop
monitor comes back on that monitor.

**The layout, checked at the floor.** Every screen was re-rendered at 940 × 620
in Russian, and what that found is fixed: a threshold no longer cut to
"Low-m…", the home list scrolls, the hint panel gives way before the answer
cards, settings rows take shares of the row instead of fixed pixels so labels
stop drawing into each other, a short Learner window drops instruction lines
first and moves the readout inside the scale, and the module shelf fades at
whichever end still has cards. Axis values now follow the language:
"1,4 кГц", not "1.4kHz".

**The hint in Guess the Band.** It did show something — that was the bug. The
region was three accept bands wide on an eight-octave axis, so it covered
three quarters of the scale with a gentle shade, under a button that promised
"Show the sound" where no picture exists. The region is the same width (a
narrower one would equal the answer), but outside it is now pushed well back,
inside is tinted and framed, a tag says **"The answer is in here"**, and the
button says **"Narrow the scale"**.

**The analysers.** The spectrum recomputes a 4096-point FFT on every frame
instead of jumping block to block, takes the loudest bin across each display
point's band, tilts +3 dB/octave so a balanced mix draws level, and carries a
peak line that holds and falls. The waveform draws 400 columns with **RMS as
the bright body inside the peak outline** — the body is what you hear, the
outline is what the meter sees.

**Split a track into stems.** A second import button takes a track apart into
**drums, bass, centre (vocal) and sides**, and slices each into loops under its
own category. The masks sum to one, so the four stems add back to the track
you gave it. It is not a trained model: a centred synth lands with the vocal,
and the tooltip says so. Plain **Add music** still never separates anything.

**Under the floorboards.** The audio thread now has a written rule — no
allocation, no locks, no file I/O, no GUI — and a test that counts every
allocation the thread makes inside `processBlock`, across three plugins,
five sample rates and seven block sizes. It found four real faults and all
four are fixed, including the EQ allocating memory on every gliding band.
Both analysers passed data between threads through plain variables; they now
use a lock-free queue.

<details><summary>Что нового в 1.7.0 (по-русски)</summary>

**Окно под любой дисплей.** Раньше дизайн-размер был и минимумом — 1180 × 880
у тренажёра, что на 13-дюймовый ноутбук уже не помещалось. Теперь окно
открывается настолько, насколько позволяет экран, и не ниже пола, на котором
вёрстка действительно работает: 940 × 620 у тренажёра, 820 × 600 у плагинов.

**Вёрстка проверена на этом полу.** Все экраны отрисованы на 940 × 620
по-русски: порог больше не режется, главная скроллится, панель подсказки
ужимается раньше карточек ответа, строки настроек не налезают друг на друга,
в низком окне инструкция отдаёт строки первой, а показания едут внутрь шкалы.
Единицы на оси теперь по-русски: «1,4 кГц».

**Подсказка в «Угадай полосу»** показывалась — это и был баг: область занимала
три четверти шкалы при слабом затемнении, а кнопка обещала «Показать звук».
Ширина осталась прежней, но снаружи теперь заметно темнее, внутри подсветка и
рамка, ярлык «Ответ здесь», а кнопка называется «Сузить шкалу».

**Анализаторы** стали плавными: спектр считает FFT на каждом кадре с
перекрытием, берёт максимум по полосе, наклон +3 дБ/окт, линия пиков с
удержанием; осциллограмма рисует RMS телом внутри пикового контура.

**Разделение трека на стемы:** барабаны, бас, центр (вокал) и стороны, каждый
нарезается в свою категорию. Это не обученная модель — центрированный синт
уедет к вокалу, и подсказка об этом говорит. Обычное «Добавить музыку»
по-прежнему ничего не разделяет.

**Внутри:** у звукового потока появилось письменное правило и тест, который
его проверяет, — он нашёл четыре настоящие неисправности, все починены.
Анализаторы больше не делят переменные между потоками.

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
