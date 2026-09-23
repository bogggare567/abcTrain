## 1.8.0-beta.1 — бета для тестирования / a beta for testers

**Это пре-релиз.** Он для тех, кто согласился попробовать и рассказать, что не так: что проверить и как сообщить, написано в [BETA_TESTING.md](https://github.com/bogggare567/abcTrain/blob/main/BETA_TESTING.md). Обычная проверка обновлений его не предлагает. Бета-версии включаются в «Настройки → О программе».

### Что нового

- **Тренажёр звучит теми же движками, что и плагины.** Реверберация раньше была Freeverb, компрессор — стандартный компрессор JUCE. Теперь это FDN и пластина Dattorro из Learner Verb и компрессор с мягким коленом из Learner Comp. Вы учитесь слышать тот же прибор, который потом крутите.
- **У трёх упражнений свой звук вместо шума.** Дисторшн звучит на аккорде: у шума нет высоты тона, значит нет гармоник. Компрессия звучит на барабанах, реверберация на одиночном ударе. Розовый шум остаётся для частоты, громкости и панорамы, и его можно выбрать для всего.
- **Эквалайзер точнее на верхних частотах.** Колокол теперь строится по методу Vicanek и не сужается у верхней границы. Раньше на 16 кГц он был почти вдвое уже, чем нарисован.
- **Дисторшн без алиасинга.** Добавлено подавление ADAA: негармонических призвуков в 4–5 раз меньше.
- **Сначала подъёмы, вырезы позже.** Провал слышно хуже пика той же величины, поэтому вырезы появляются с 4-й ступени.
- **Упражнение чаще спрашивает там, где вы ошибаетесь.**
- **Порог вместо рекорда.** Главный экран показывает измеренный порог: среднее по разворотам лестницы, по методу Левитта. Рекорд систематически завышен, это вершина случайного блуждания.
- **Пакеты звуков** с жанром, вокалом, инструментами, автором и лицензией у каждого клипа. Все авторы собраны в «Настройки → О программе».
- **«Разделить на стемы» убрано.** Без обученной модели разделение не окупало своей сложности.
- **Бета-канал** обновлений и кнопка **«Сообщить или предложить»**.

Почему сделано именно так, с источниками: [сверка с литературой](https://github.com/bogggare567/abcTrain/blob/main/docs/research/2026-09-literature-audit.md) и [ADR 040](https://github.com/bogggare567/abcTrain/blob/main/docs/decisions/040-one-engine-per-effect.md).

---

**This is a pre-release** for testers — see [BETA_TESTING.md](https://github.com/bogggare567/abcTrain/blob/main/BETA_TESTING.md). The trainer now runs the same reverb and compressor engines as the Learner plugins. Distortion, compression and reverb play their own material (a chord, a drum loop, a single hit) instead of pink noise. The EQ bell is Vicanek's matched design, and distortion is anti-aliased (ADAA). Cuts come after boosts, the exercise asks more where you miss, and the home screen shows a measured threshold (mean of staircase reversals) instead of the record. Sound packs carry per-clip authors and licences. Stem splitting is removed. There is a beta update channel and an in-app "Report a problem" button.

Builds are **unsigned**: macOS and Windows will warn on first launch.
