# Beta testing

*(English below; [Русская версия](#бета-тестирование) further down.)*

Thanks for trying abcTrain before it's a finished product. This is genuinely
useful — real feedback from real DAWs/OSes is worth far more than more
time spent guessing in isolation.

## What to test

- **EarTrainer** — all 9 games (guess the band, compression, reverb type,
  pan position, delay time, distortion type, stereo width, gain change,
  named frequency range). Does difficulty actually feel like it's scaling
  as your level goes up? Does the daily challenge/streak tracking survive
  closing and reopening the plugin?
- **LearnerEQ / LearnerComp / LearnerVerb** — do they sound right on real
  material, in your actual DAW, not just the Standalone build? Does
  Bypass/A-B actually give you a clean before/after? Does the "Lesson"
  walkthrough make sense end to end?
- **The language picker** (EarTrainer only, for now — see
  [decisions/011](docs/decisions/011-i18n.md) for why the other three
  plugins don't have one yet). Switch languages and check the text
  actually looks right for your language, especially if it isn't English
  or Russian (those two are the only ones a native speaker on this project
  has directly verified so far).
- **The installer for your OS** (macOS `.pkg`/DMG, Windows `.exe`, Linux
  `.tar.gz` + `install.sh`) — does component selection work, does the
  install actually land where it says it will, do the plugins show up in
  your DAW afterward?

## What's known to be incomplete (please don't file these as new bugs)

- Only EarTrainer has a language selector; the other three plugins are
  still English-only in their UI (their tooltip *content*, however, is
  translated in principle once wired up — see the roadmap).
- Parameter tooltips/lesson step text are English-only regardless of
  language picked (see [decisions/011](docs/decisions/011-i18n.md)).
- No channel/auto-update-check toggle in the UI yet (the underlying
  stable/beta-channel logic exists — see
  [decisions/012](docs/decisions/012-versioning.md) — but nothing in any
  editor lets you pick it).
- The builds are **unsigned** — macOS Gatekeeper and Windows SmartScreen
  will both warn. This is expected until code signing/notarization
  happens (see [decisions/008](docs/decisions/008-installers.md)).
- Non-English/Russian translations haven't had a native-speaker review
  pass yet — if something reads awkwardly, that's exactly the kind of
  thing worth reporting.

See [docs/roadmap.md](docs/roadmap.md) for the full done-vs-planned
picture, and use the [bug report](.github/ISSUE_TEMPLATE/bug_report.md) /
[feature request](.github/ISSUE_TEMPLATE/feature_request.md) templates
when filing something on GitHub.

---

# Бета-тестирование

*(Русская версия; [English version](#beta-testing) выше.)*

Спасибо, что пробуете abcTrain ещё до того, как это готовый продукт. Это
реально полезно — обратная связь из настоящих DAW/ОС стоит гораздо больше,
чем ещё немного времени, потраченного на угадывание в вакууме.

## Что тестировать

- **EarTrainer** — все 9 игр (угадай полосу, компрессию, тип реверберации,
  панораму, время задержки, тип искажения, ширину стерео, изменение
  громкости, именованный частотный диапазон). Реально ли ощущается, что
  сложность растёт вместе с уровнем? Переживают ли статистика дневного
  задания/серии перезапуск плагина?
- **LearnerEQ / LearnerComp / LearnerVerb** — звучат ли они правильно на
  реальном материале, в вашей настоящей DAW, а не только в Standalone-
  сборке? Даёт ли Bypass/A-B реально чистое сравнение до/после? Понятен
  ли урок ("Lesson") от начала до конца?
- **Переключатель языка** (пока только в EarTrainer — почему у остальных
  трёх плагинов его ещё нет, см. [decisions/011](docs/decisions/011-i18n.md)).
  Переключите языки и проверьте, что текст реально выглядит корректно на
  вашем языке — особенно если это не английский и не русский (только эти
  два пока напрямую проверены носителем языка в этом проекте).
- **Установщик для вашей ОС** (macOS `.pkg`/DMG, Windows `.exe`, Linux
  `.tar.gz` + `install.sh`) — работает ли выбор компонентов, действительно
  ли установка попадает туда, куда обещано, появляются ли плагины в вашей
  DAW после установки?

## Что заведомо не готово (пожалуйста, не заводите как новые баги)

- Переключатель языка есть только в EarTrainer; у остальных трёх плагинов
  интерфейс пока только на английском (сам *текст* подсказок в принципе
  переводим, как только будет подключён — см. roadmap).
- Текст подсказок к параметрам и шагов уроков — только на английском,
  независимо от выбранного языка (см.
  [decisions/011](docs/decisions/011-i18n.md)).
- В интерфейсе пока нет переключателя канала/автопроверки обновлений
  (сама логика stable/beta-канала уже есть — см.
  [decisions/012](docs/decisions/012-versioning.md) — но выбрать её негде).
- Сборки **не подписаны** — macOS Gatekeeper и Windows SmartScreen будут
  предупреждать. Это ожидаемо до подписи/нотаризации кода (см.
  [decisions/008](docs/decisions/008-installers.md)).
- Переводы кроме английского и русского ещё не проходили вычитку
  носителем языка — если что-то звучит неестественно, это ровно то, о чём
  стоит сообщить.

Полную картину сделано/запланировано смотрите в
[docs/roadmap.md](docs/roadmap.md), а для баг-репортов/запросов фич на
GitHub используйте шаблоны
[bug report](.github/ISSUE_TEMPLATE/bug_report.md) /
[feature request](.github/ISSUE_TEMPLATE/feature_request.md).
