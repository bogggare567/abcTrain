# Сборка звуковых пакетов

Формат, правила по лицензиям и откуда брать звук описаны в
[docs/design/sound-library.md](../../docs/design/sound-library.md).

```
pip install soundfile pyloudnorm numpy
python3 tools/library/build_pack.py packs/rock-basics.csv \
    --title-en "Rock basics" --title-ru "Рок: основы"
```

CSV — одна строка на исходник, пример в [example-pack.csv](example-pack.csv):

| колонка | что писать |
|---|---|
| `source_file` | путь к файлу относительно CSV |
| `title`, `author`, `url` | откуда трек; автор обязателен |
| `license` | `CC0-1.0`, `PD`, `CC-BY-3.0`, `CC-BY-4.0`, `CC-BY-SA-3.0`, `CC-BY-SA-4.0` или `permission` (письменное разрешение автора, шаблон в конце design-документа) |
| `genre`, `instruments` | через `;` |
| `content` | `instrumental`, `vocal-male`, `vocal-female`, `vocal-mixed`, `a-cappella-male`, `a-cappella-female` или пусто |
| `start_seconds` | через `;` — где резать; пусто — скрипт выберет сам, пропуская тишину |
| `clip_seconds` | длина клипа, по умолчанию 10 |

Скрипт откажется собирать пакет, если у строки нет автора или лицензия
не из списка. Это то же правило, по которому приложение отказывается
показывать такой клип (`ReferenceAudioLibrary::isAllowedLicense`).

Проверка: положить готовую папку пакета в папку «Звуки для тренировки»
(в тренажёре кнопка «Открыть папку») и открыть экран звуков.
