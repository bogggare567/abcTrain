## 2.0 Saint Petersburg

С этого выпуска у версий abcTrain есть имена городов. **2.0 называется Saint Petersburg.**

### Что нового

- **Одно приложение вместо четырёх.** abcTrain теперь одна программа с вкладками: Тренировки, Студия, Live. Learner EQ, Comp и Verb остались плагинами VST3/AU и работают внутри приложения на вкладке «Студия». Плагин и Студия собираются из одного кода.
- **Live.**
  - Можно войти на soundkorb.ru по почте и коду, без пароля. Компьютер привязывается коротким кодом.
  - Синхронизация прогресса включается отдельно: на сервер уходит сводка около 1 КБ, история ответов остаётся на компьютере.
  - Если Live не работает, приложение пишет, в чём дело: нет сети, нет интернета, сервер не отвечает или версия устарела.
  - Есть рейтинг Decibelo.
- **Батлы с ботами.** Шесть «слушателей» с разным профилем слуха: Гончая, Кошка, Гадюка, Сова, Летучая мышь, Слон. Батл идёт семь раундов на одном и том же материале для обоих и работает без сети. Звери здесь игровые персонажи, а не биология.
- **Свои звуки по инструментам.**
  - Импорт определяет инструмент. Если не уверен, кладёт звук в «Другое».
  - Нарезает петли без шва по темпу.
  - Фрагмент из трека можно выделить мышью.
  - Клип можно удалить прямо в приложении.
- **Окно как у macOS.** Масштаб интерфейса зависит от размера окна. Модули и урок открываются в отдельном окне.
- **EQ.**
  - У HP/LP выбирается крутизна: 6, 12, 24 или 48 дБ/окт.
  - Типы фильтров показаны значками.
- **Очки за точность.** Верный ответ даёт 1 очко и ещё до 0,9 за меткость.
- **Исправлено:**
  - кнопки ведущего заходили на карточку открытой онлайн-комнаты;
  - заголовок «Твой ответ» иногда рисовался дважды;
  - сборка под Windows.

Сервер Live стоит в России (soundkorb.ru). Из некоторых стран он может открываться медленно или не открываться. Тренировки, Студия и батлы с ботами работают без него.

Почему сделано так: [ADR 041–046](https://github.com/bogggare567/abcTrain/tree/main/docs/decisions).

---

**abcTrain 2.0 "Saint Petersburg"**: from now on each release line is named after a city.

- **One app.** It has Training, Studio and Live tabs. The Learner plugins also run inside the app, in the Studio tab.
- **Live.** Optional sign-in on soundkorb.ru with an e-mail code. A computer is linked with a short code. Progress sync is opt-in and sends a ~1 KB summary. The app tells you why Live is unreachable, and shows the Decibelo rating.
- **Offline battles against six bot listeners.** Each bot has its own hearing profile. They are game characters, not biology.
- **Your own sounds, sorted by instrument.** Import cuts seamless tempo-aware loops. You can pick a fragment from a track by hand, and delete clips in the app.
- **Window and EQ.** macOS-style window with scale set by the window size. EQ HP/LP slopes of 6 to 48 dB/oct. Precision points.
- **Fixes.** The Live room overlap, a doubled answer heading and the Windows build.

The Live server is in Russia, so it may be slow or unreachable from some countries. Everything else works offline.

Builds are **unsigned**: macOS and Windows will warn on first launch.
