## 2.3

### Что нового

- **Версии плагинов.** У Learner EQ, Comp и Verb теперь своя версия, её видно в DAW. До сих пор везде стояло 1.0.0. Плагин предлагает обновиться, только когда изменился сам. На странице «О программе» указаны версии приложения, плагинов и уроков.
- **Уроки от авторов.** Звукорежиссёр или преподаватель может предложить свой урок: текстовым файлом через pull request, кнопкой «Add file» на GitHub или через форму «Урок». Формат проверяется автоматически, ошибки отмечаются на строках файла. Автор указан в уроке. Как это сделать: [lessons/CONTRIBUTING.md](https://github.com/bogggare567/abcTrain/blob/main/lessons/CONTRIBUTING.md).
- **Android.** Первая сборка, экспериментальная: APK лежит в файлах релиза. Нужен Android 8.0 или новее.
- **iPhone и iPad.**
  - Исправления после проверки на симуляторе iPhone 17: вырез, рамка окна, вкладки, единицы. Все экраны проверяются на пяти размерах окна.
  - Ставятся пока своей сборкой из Xcode.
  - Как установить на телефон: [На телефоне и планшете](https://github.com/bogggare567/abcTrain/wiki/ru-Mobile).
- **Упражнения на частоту** спрашивают только о тех полосах, в которых в звучащем клипе есть звук.
- **Исправлено:** на телефоне перестало перерисовываться всё окно ради одной затемнённой кнопки.

Почему сделано так: [ADR 069–071](https://github.com/bogggare567/abcTrain/tree/main/docs/decisions).

---

**abcTrain 2.3**

- **Plugin versions.** Learner EQ, Comp and Verb carry their own version in your DAW (they all said 1.0.0) and offer an update only when they changed. Settings → About lists every part's version.
- **Lessons from contributors** by pull request, the GitHub site or an issue form, checked automatically and credited: [lessons/CONTRIBUTING.md](https://github.com/bogggare567/abcTrain/blob/main/lessons/CONTRIBUTING.md).
- **Android**: a first, experimental APK in the release assets (Android 8.0+).
- **iPhone and iPad**: fixes from the iPhone 17 simulator; install by building with Xcode - [On a phone or tablet](https://github.com/bogggare567/abcTrain/wiki/Mobile).
- **Frequency exercises** ask only about bands the playing clip has sound in.

Builds are **unsigned**: macOS and Windows will warn on first launch.
