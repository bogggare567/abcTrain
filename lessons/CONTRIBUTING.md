# Как добавить урок в abcTrain

*[English below](#adding-a-lesson-to-abctrain)*

Уроки в abcTrain пишут не только разработчики. Урок — это обычный текстовый
файл, его можно написать в любом редакторе без программирования, а
приложение само крутит по нему ручки. Если вы звукорежиссёр или
преподаватель и знаете, как объяснить что-то на слух, — этот документ для
вас.

## Коротко

1. Напишите урок по [FORMAT.md](FORMAT.md).
2. Проверьте его: `LessonCheck` или проверка «Lessons» в pull request.
3. Откройте pull request (или issue «Урок», если git вам не знаком).
4. Мы прослушиваем урок в приложении, правим вместе с вами и вливаем.
5. Урок выходит с ближайшей версией. В приложении он подписан вашим именем,
   номер версии уроков (`versions.json` → `lessons`) поднимается.

## 1. Какой урок

- **Урок Студии** — для плагинов abcTrain (Learner EQ, Comp, Verb). Лежит в
  `lessons/<курс>/`: `eq`, `comp`, `verb`, `routing`, `channel`, `vocal`,
  `bus`.
- **Урок для «своего плагина»** (`plugin: any`) — играется на плагине,
  который установлен у человека: на его компрессоре, эквалайзере,
  ревербераторе. Лежит в `lessons/own/`.

Один урок — одна мысль, которую можно услышать: 4–8 шагов, 3–5 минут
чтения, 2–5 предложений в шаге. Обращение на «вы». Названия параметров —
как на плагинах, без перевода: Threshold, Ratio, Attack, Q, Pre-delay.

## 2. Шапка урока гостевого автора

```
id: own-comp-knee
course: own
order: 11
minutes: 3
plugin: any
group: dynamics
material: loop
author: Иван Петров
license: CC BY-SA 4.0
sources: R. Izhaki, «Mixing Audio», гл. 16, раздел «Колено (Knee)»
title.ru: Knee: мягкое и жёсткое колено
title.en: Knee: soft and hard
```

- `id` — постоянное имя латиницей, начинается с курса (`own-`, `eq-`…).
  Не меняйте его после выпуска: по нему хранится, что урок пройден.
- `order` — следующий свободный номер в курсе. Занятый номер `LessonCheck`
  покажет.
- `author` — так вас подпишут в приложении.
- `license` — под какой лицензией текст входит в abcTrain:
  `CC BY-SA 4.0` (рекомендуем), `CC BY 4.0` или `CC0`. Без неё гостевой
  урок не принимается. Код abcTrain остаётся под своей лицензией, уроки —
  под вашей.
- `sources` — книга и глава, откуда взяты факты и числа. Если урок
  опирается на ваш опыт — так и напишите: «опыт автора».

## 3. Проверить

**Без сборки.** Откройте pull request: проверка «Lessons» соберёт
`LessonCheck` и отметит каждую ошибку на строке файла прямо в PR.

**Локально:**
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target LessonCheck
./build/LessonCheck_artefacts/Release/LessonCheck lessons/own/11-comp-knee.lesson
```

`LessonCheck` проверяет:
- формат: команды, диапазоны значений, оба языка в каждом шаге;
- id и порядок в курсе: не заняты ли другим уроком;
- длину: минуты, шаги, слова;
- для `plugin: any`: что каждая ручка есть в словаре и каждое число
  достижимо на ручке.

Что значение урока Студии лежит в диапазоне ручки Learner, проверяет
полная сборка (тест `LessonFormat`). Она тоже запускается на PR.

**Послушать.** Положите файл в папку `lessons/…` и соберите приложение:
уроки вшиваются при сборке. Пройдите урок целиком. Каждый шаг должен
звучать так, как написано. Это главное, что проверит и рецензент.

## 4. Предложить

**Если умеете git:** форк → ветка → файл → pull request. Шаблон PR
напомнит, что отметить.

**Через сайт GitHub, без git:** откройте папку `lessons/own` (или нужный
курс) → **Add file → Create new file** → вставьте урок → **Propose new
file**. GitHub сам сделает форк и pull request.

**Совсем без GitHub-навыков:** [issue «Урок»](https://github.com/bogggare567/abcTrain/issues/new?template=lesson.yml):
вставьте текст, укажите имя и лицензию. PR оформим мы, автором будете вы.

## 5. Что проверяет рецензент

Уроки в `lessons/` требуют одобрения владельца (`.github/CODEOWNERS`).
Смотрим:
- **звучит ли шаг так, как написано**, на материале урока;
- **числа**: сверены с источником и правдоподобны для материала;
- **своими словами**: пересказ, не копия книги — цитата не длиннее
  предложения;
- **одна мысль**: урок не пытается объяснить всё сразу;
- **язык**: русский и английский, оба естественные. Остальные языки
  приложение пока показывает по-английски.

Обычно приходит одна-две правки, это нормально.

## 6. После слияния

- Урок выходит в ближайшей версии приложения. Номер `lessons` в
  `versions.json` поднимается: 1.0.0 → 1.1.0 за новые уроки, 1.0.1 за
  исправления.
- Ваше имя стоит в шапке урока и видно в окне урока.
- Поправки к уроку присылайте так же, через PR или issue.

---

# Adding a lesson to abcTrain

A lesson is a plain text file - steps of text and one-line commands that
move the knobs. No programming. If you are an engineer or a teacher who can
explain something by ear, this is for you.

1. Write it following [FORMAT.md](FORMAT.md) (in Russian; the commands are
   the same in any language). Every step needs `ru:` and `en:` text.
2. Guest lessons need `author`, `license` (`CC BY-SA 4.0` recommended,
   `CC BY 4.0` or `CC0`) and `sources` in the header.
3. Check it: open a pull request and the **Lessons** check annotates every
   problem on its line. Or build `LessonCheck` locally:
   `cmake --build build --target LessonCheck`, then
   `./build/LessonCheck_artefacts/Release/LessonCheck <file>`.
4. Propose it:
   - **with git**: a pull request;
   - **on the GitHub site**: Add file → Create new file in `lessons/<course>/`;
   - **without GitHub**: the [Lesson issue form](https://github.com/bogggare567/abcTrain/issues/new?template=lesson.yml),
     and we open the pull request with you as the author.
5. Review (CODEOWNERS) listens to it in the app and checks the numbers
   against the source, that it is your own words, and that it is one idea.
6. It ships in the next release, credited to you, and the `lessons`
   version in `versions.json` goes up.
