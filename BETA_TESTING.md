# Бета-тестирование abcTrain

*[English below](#beta-testing-abctrain)*

Спасибо, что пробуете. Нужны не похвалы, а всё, что непонятно, сломано,
скучно или звучит не так, как в жизни. Это займёт 15 минут.

## Как поставить

1. Скачайте установщик для своей системы со страницы последнего
   **пре-релиза** на [GitHub Releases](https://github.com/bogggare567/abcTrain/releases):
   `.dmg` для macOS, `-setup.exe` для Windows, `.tar.gz` для Linux.
2. Сборки **не подписаны**, система предупредит. На macOS: правый клик по
   установщику → «Открыть»; если не помогло — «Системные настройки →
   Конфиденциальность и безопасность → Всё равно открыть». На Windows:
   «Подробнее → Выполнить в любом случае». Подробно:
   [Troubleshooting](docs/wiki/ru-Troubleshooting.md).
3. Чтобы получать следующие беты сами: **Настройки → О программе →
   «Бета-версии: вкл»**. Если у вас уже стоит бета, она и так предложит
   следующую бету и потом финальную версию.

## Что проверить за 15 минут

**Тренажёр (ABC Ear Trainer)**

1. Откройте **Звуки для тренировки**. Сверху должно быть выбрано «Звук
   упражнения».
2. **Угадай дисторшн** — теперь звучит аккорд, а не шум. Слышна ли разница
   между типами? Звучит ли это как настоящий перегруз, а не как «что-то
   сломалось»?
3. **Угадай компрессию** — барабанный луп. Похоже ли на то, что делает
   компрессор на барабанной шине?
4. **Угадай реверберацию** — одиночный удар. Узнаются ли комната, зал,
   пластина, пружина так, как вы их знаете по работе?
5. **Угадай полосу** — пройдите 30–40 раундов. На первых ступенях должны
   быть только подъёмы, вырезы появляются дальше. Чувствуется ли, что
   упражнение чаще спрашивает там, где вы ошибаетесь?
6. На главном экране справа у каждого упражнения — порог. После пары
   десятков ответов он становится измеренным порогом (а не рекордом).
   Совпадает ли он с ощущением «вот тут я начинаю путаться»?
7. Переключите на «Розовый шум» и обратно — должно работать без щелчков
   и провалов громкости.

**Обучающие плагины (Learner EQ / Comp / Verb)** — в вашей DAW на вашем
материале: всё ли звучит и сохраняется с проектом, не трещит ли.
Learner EQ: колокол на 10–16 кГц теперь должен звучать так же широко,
как нарисован.

**Везде:** окно на вашем экране (особенно 13-дюймовом ноутбуке), русский
текст — ничего не обрезано, всё читается.

## Как сообщить

- В тренажёре: **Настройки → О программе → «Сообщить или предложить…»**. Откроется страница GitHub с уже подставленными версией
  и системой (нужен аккаунт GitHub).
- Или просто напишите Богдану как удобно: текст, скриншот, запись экрана.
  Он перенесёт на [доску](https://github.com/users/bogggare567/projects/1).

Самое ценное: **где вы запутались**, **что звучит неправдоподобно** и
**на каком шаге стало скучно**.

## Что уже известно (не надо сообщать)

- Сборки не подписаны.
- Своей музыки в комплекте пока нет — только синтезированные звуки и
  то, что вы импортируете сами. Пакеты с настоящей музыкой будут.
- «Разделить на стемы» убрано специально.
- Кнопка «Сообщить» пока только в тренажёре, не в плагинах.

---

# Beta testing abcTrain

Thanks for trying it. What helps most is anything confusing, broken,
boring, or that doesn't sound like the real thing. It takes 15 minutes.

**Install:** download the installer for your system from the latest
**pre-release** on [GitHub Releases](https://github.com/bogggare567/abcTrain/releases).
Builds are unsigned, so your system will warn you: on macOS right-click →
Open; on Windows More info → Run anyway. To be offered later betas: Settings
→ About → "Beta versions: on".

**Check:** the distortion, compression and reverb exercises now play their
own material (a chord, a drum loop, a single hit) — does it sound like the
real thing? Guess the Band should ask only boosts at first and ask more
where you miss. The home screen shows a measured threshold once there is
enough data — does it match where you start to get lost? In your DAW: do
the Learner plugins sound right and save with the project?

**Report:** Settings → About → "Report a problem or suggest…" opens a
GitHub issue with the version and system filled in — or just message the
author.
