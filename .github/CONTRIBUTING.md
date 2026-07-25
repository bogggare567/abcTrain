# Contributing

*(English below; [Русская версия](#вклад-в-проект) further down.)*

Thanks for considering contributing to abcTrain! This is a small,
actively-developed project — read [CLAUDE.md](../CLAUDE.md) first for the
full per-file architecture breakdown, and [docs/roadmap.md](../docs/roadmap.md)
for what's actually planned vs. built.

## Before you start

- **Read the relevant ADR.** Most non-obvious design decisions (why
  LearnerComp has a custom compressor engine, why the book-library work
  stops at a bibliography, why i18n is scoped to a core string set) are
  documented in [docs/decisions/](../docs/decisions/). If you're about to
  change something covered by an ADR, read it first — there's usually a
  reason the current shape exists.
- **Build and run the tests locally before opening a PR.** `cmake -B build
  && cmake --build build --config Release`, then
  `./build/EarTrainerTests_artefacts/Release/EarTrainerTests`. CI runs the
  same thing on push, but catching a broken build yourself first saves a
  round trip.
- **Small, focused PRs** are much easier to review than one PR touching
  five unrelated things.

## Ways to contribute

- **New EarTrainer games.** Implement the `Game` interface
  (`Source/Games/Game.h`), register it in `GameManager`'s constructor, add
  the files to `CMakeLists.txt`, write a test following the existing
  `tests/*GameTest.cpp` template. No processor/editor changes needed —
  see [docs/architecture.md](../docs/architecture.md).
- **Translations.** `shared/i18n/strings/*.json` — each is a flat
  `{"key": "text"}` table. Fixing/improving an existing language's
  strings, or adding a 13th language (one new JSON file + a few lines in
  `CMakeLists.txt`/`LocalisationManager.cpp` — see
  [decisions/011](../docs/decisions/011-i18n.md)), are both welcome. Only
  the core UI string set is translated today (not yet every tooltip/
  lesson) — see `docs/roadmap.md` for the current scope.
- **Bug fixes.** Please include a failing test that your fix makes pass,
  where practical.
- **Documentation.** Docs drift is a real risk here given how much of this
  project's reasoning lives in ADRs/`CLAUDE.md` — if you spot something
  out of date, a fix is genuinely useful on its own.

## Code style

- JUCE house style: space before parens (`if (x)`, not `if(x)`).
- No `juce::Font(float, styleFlags)` (deprecated) — use
  `juce::Font(juce::FontOptions(...))`, matching
  `shared/AbcTrainLookAndFeel.h`.
- `AudioProcessorValueTreeState` for per-plugin-instance parameters that
  should save/restore with the host session; `juce::PropertiesFile` for
  per-user data that should persist across every session/project
  regardless of host (see the "Conventions" section of
  [CLAUDE.md](../CLAUDE.md)). Don't mix them up.

## Reporting bugs / requesting features

Use the issue templates — they ask for the specific details (plugin,
format, host, OS, version string) that make a bug report actually
actionable.

---

# Вклад в проект

*(Русская версия; [English version](#contributing) выше.)*

Спасибо за интерес к участию в разработке abcTrain! Сначала прочитайте
[CLAUDE.md](../CLAUDE.md) — там подробный разбор архитектуры по каждому
файлу, и [docs/roadmap.md](../docs/roadmap.md) — что реально запланировано,
а что уже сделано.

## Перед началом

- **Прочитайте соответствующий ADR.** Большинство неочевидных решений
  (почему у LearnerComp свой движок компрессора, почему работа с
  библиотекой книг ограничена библиографией, почему i18n покрывает только
  базовый набор строк) задокументированы в
  [docs/decisions/](../docs/decisions/). Если собираетесь менять то, что
  описано в ADR — сначала прочитайте его, обычно на то есть причина.
- **Соберите проект и прогоните тесты локально до PR.** `cmake -B build
  && cmake --build build --config Release`, затем
  `./build/EarTrainerTests_artefacts/Release/EarTrainerTests`. CI делает то
  же самое при пуше, но проверить самому заранее — быстрее, чем ждать
  цикл ревью.
- **Небольшие, сфокусированные PR** рецензировать намного проще, чем один
  PR, затрагивающий пять несвязанных вещей.

## Как можно помочь

- **Новые игры для EarTrainer.** Реализуйте интерфейс `Game`
  (`Source/Games/Game.h`), зарегистрируйте в конструкторе `GameManager`,
  добавьте файлы в `CMakeLists.txt`, напишите тест по шаблону
  существующих `tests/*GameTest.cpp`. Изменения в процессоре/редакторе не
  нужны — см. [docs/architecture.md](../docs/architecture.md).
- **Переводы.** `shared/i18n/strings/*.json` — плоская таблица
  `{"ключ": "текст"}` для каждого языка. Приветствуются как исправления
  существующих языков, так и добавление 13-го языка (один новый JSON-файл
  + несколько строк в `CMakeLists.txt`/`LocalisationManager.cpp` — см.
  [decisions/011](../docs/decisions/011-i18n.md)). Сегодня переведён
  только базовый набор строк интерфейса (ещё не все подсказки/уроки) —
  актуальный охват смотрите в `docs/roadmap.md`.
- **Исправления багов.** По возможности приложите тест, который падает
  без вашего исправления и проходит с ним.
- **Документация.** Здесь реально высок риск, что документация устареет,
  учитывая сколько обоснований живёт в ADR/`CLAUDE.md` — если заметили
  что-то неактуальное, исправление само по себе полезно.

## Стиль кода

- Стиль JUCE: пробел перед скобкой (`if (x)`, а не `if(x)`).
- Не используйте `juce::Font(float, styleFlags)` (устарел) — используйте
  `juce::Font(juce::FontOptions(...))`, как в
  `shared/AbcTrainLookAndFeel.h`.
- `AudioProcessorValueTreeState` — для параметров конкретного экземпляра
  плагина, которые должны сохраняться/восстанавливаться с сессией хоста;
  `juce::PropertiesFile` — для пользовательских данных, которые должны
  сохраняться между сессиями/проектами независимо от хоста (см. раздел
  "Conventions" в [CLAUDE.md](../CLAUDE.md)). Не путайте эти два
  механизма.

## Баг-репорты и запросы фич

Используйте шаблоны issue — они запрашивают именно те детали (плагин,
формат, хост, ОС, строка версии), которые делают баг-репорт реально
пригодным для работы.
