## 2.2 Saint Petersburg

### Что нового

- **Свои плагины.** Студия → «Свои плагины»: тренажёр на VST3/AU, которые установлены у вас на компьютере.
  - Плагины находятся без загрузки и раскладываются по назначению: эквалайзеры, динамика, пространство, сатурация, модуляция. AU и VST3 одного плагина показаны одной строкой.
  - **Уроки на вашем плагине.** Текст по шагам, ручки сами едут в окне плагина к значениям в его единицах: «Threshold −20 дБ», «Attack 30 мс». Один урок работает на любом компрессоре. Сейчас 10 уроков: компрессор, эквалайзер, ревербератор, сатурация, дилей и модуляция. Когда урок закончен, ваша настройка возвращается.
  - **Знакомство.** Карточка на каждую ручку: что делает, что слушать, «мало», «много», «прокрутить». Одинаковые ручки разных полос (24 полосы Pro-Q) показаны одной карточкой.
  - **Угадай.** B — ваша настройка, в которой сдвинута одна ручка. Варианты — значения этой ручки словами самого плагина.
  - Ваша настройка плагина сохраняется и при следующем открытии возвращается.
- **Батлы на ПК.**
  - Бой на рейтинг или дружеский.
  - Лестница ботов по рейтингу со звёздами: первая победа, три победы, без ошибок.
  - Бот дня и последние бои.
  - После боя показан урон по раундам рядом с кривыми HP.
- **Семинар.**
  - Экран ведущего с планом раундов.
  - Анонимный режим.
  - Свой звук на каждый раунд.
  - Горячие клавиши, в том числе Page Down с кликера.
  - Архив семинаров: каждый раунд, каждая ошибка, выгрузка в CSV.
- **Настройки.**
  - Поиск по настройкам и переключатель «Новичок / Pro».
  - Понятные страницы: у каждой группы написано, что она меняет, у значения по умолчанию стоит точка.
  - Новое: ответ кнопкой, окно поверх остальных.
  - Страница «Клавиши»: свои горячие клавиши для тренировки, семинара и Студии.
- **Перед забегом** открывается лист: ваш уровень, три режима с рекордами и звук. **Профиль** показывает Decibelo по семействам и его историю.
- **Подсказки** занимают всё свободное место на шкале. Закрытая подсказка показана рамкой с замком, а не пустотой. Чёрных «колодцев» на панелях больше нет.
- **Версии плагинов.** У Learner EQ, Comp и Verb теперь своя версия, её видно в DAW (раньше везде стояло 1.0.0). Плагин предлагает обновиться, только когда изменился сам. На странице «О программе» указаны версии частей.
- **Уроки от авторов.** Звукорежиссёр или преподаватель может предложить свой урок: текстовым файлом через pull request или через форму на GitHub. Проверка формата идёт автоматически, автор указан в уроке. Как это сделать: [lessons/CONTRIBUTING.md](https://github.com/bogggare567/abcTrain/blob/main/lessons/CONTRIBUTING.md).
- **Телефоны.**
  - Исправления под iPhone: вырез, рамка окна, вкладки, единицы.
  - Частотные упражнения спрашивают только о том, что есть в клипе.
  - Первая сборка для **Android** (APK в файлах релиза, экспериментальная).
  - iPhone и iPad ставятся сборкой из Xcode.
  - Как установить: [На телефоне и планшете](https://github.com/bogggare567/abcTrain/wiki/ru-Mobile).
- **Безопасность.** Пакет звуков с именем `..` или `.` больше не может удалить папку вне библиотеки.
- **Исправлено:**
  - Студия не открывалась целиком после своего плагина;
  - кракозябры вместо «·» и «—»;
  - строки настроек наезжали друг на друга внизу длинной страницы.

Почему сделано так: [ADR 059–071](https://github.com/bogggare567/abcTrain/tree/main/docs/decisions). Руководство: [Свои плагины](https://github.com/bogggare567/abcTrain/wiki/ru-Your-own-plugins).

---

**abcTrain 2.2 "Saint Petersburg"**

- **Your own plugins.** Studio → Your plugins trains on the VST3/AU installed on your computer, grouped by purpose. **Lessons** turn your plugin's own knobs in its own window to values in its units ("Threshold -20 dB"); one lesson works on any compressor, with 10 lessons across compressors, EQs, reverbs, saturation, delay and modulation. **Knob cards** explain each knob. **Guess** hides one knob of your setting and asks for its value in the plugin's own words. Your setting is kept between sessions.
- **Battles on desktop.** Rated or friendly battles, a bot ladder with stars, a bot of the day, and damage per round after a battle.
- **Seminar.** A presenter screen with the round plan, anonymous mode, a sound per round, hotkeys and an archive with CSV export.
- **Settings.** Search, Beginner / Pro, clearer pages and a Keys page for your own hotkeys.
- **A start sheet** before each run, and a **profile** with Decibelo by family.
- **Hints** fill the free space on the ruler, and a locked hint is shown as locked.
- **Plugin versions.** Learner EQ, Comp and Verb carry their own version in your DAW (they all said 1.0.0) and offer an update only when they changed.
- **Lessons from contributors** by pull request or a GitHub form, checked automatically and credited: [lessons/CONTRIBUTING.md](https://github.com/bogggare567/abcTrain/blob/main/lessons/CONTRIBUTING.md).
- **Phones.** iPhone fixes, and frequency exercises ask only about what the clip contains. A first, experimental **Android** APK is in the release assets. iPhone and iPad install by building with Xcode: [On a phone or tablet](https://github.com/bogggare567/abcTrain/wiki/Mobile).
- **Security.** A sound pack can no longer delete a folder outside the library.
- **Fixes.** The Studio after your own plugin, garbled "·" and "—", and overlapping settings rows.

Builds are **unsigned**: macOS and Windows will warn on first launch.
