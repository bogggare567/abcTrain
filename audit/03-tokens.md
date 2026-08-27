# Токены. Снято с shared/AbcTrainTheme.cpp — не на глаз

## Палитра
| Токен | Тёмная | Светлая |
|---|---|---|
| windowBackground | #1e1e2e | #e8e6e1 |
| panelBackground | #242434 | #f1efeb |
| widgetBackground | #2a2a3a | #f8f7f4 |
| displayBackground | #14141a | #dedad2 |
| outline | #3a3a4a | #c9c5bd |
| divider | #32323f | #d8d4cc |
| text | #e0e0e0 | #33322f |
| textDim | #a0a0b0 | #6d6a64 |
| textBright | #f2f2f7 | #1b1a18 |
| accent | #5b9bd5 | #2f6fa8 |
| accentWarm | #d98c5f | #b26134 |
| positive | #6fbf8b | #3d8a5f |
| negative | #d9615f | #b03c3a |

## Акценты семейств (AbcTrainTheme::accentFor)
| Семейство | Тёмная | Светлая |
|---|---|---|
| frequency | #4fa3c7 | #437b93 |
| dynamics | #c77f4f | #936343 |
| space | #5fb98c | #4d896a |
| character | #a878c9 | #7e5e94 |

## Шаг сетки (базис 4)
hairline 2 · tight 4 · small 8 · medium 12 · large 20 · section 28

## Радиусы
small 4 · button 7 · well 8 · panel 10

## Длительности, мс
hover 140 · press 90 · release 260 · transition 320
feedback 900 · sway 720 · breath 3400

## Кривые
out(t) = 1-(1-t)^3 · inOut кубическая · backOut c1=1.70158 (перелёт ~10%)

## Шрифты (множатся на пользовательский масштаб 0.8-1.4)
display 30 bold · title 20 bold · heading 15 bold · body 13 ·
mono 13 · label 12 · caption 11 · micro 10
Семейство: macOS SF Pro Text → Windows Segoe UI Variable → Linux Inter

## Разрядка (tracking, px)
3.0 «BYPASSED» · 1.8 заголовки окон · 1.6 крупная надпись выбора ·
1.4 подписи «LEVEL» · 1.2 заголовки секций · 0.8 CompactSelector

## Размеры окон
| Плагин | По умолчанию | Мин | Макс |
|---|---|---|---|
| EarTrainer | 680x612 | 680x612 | 1360x1224 |
| Learner EQ | 790x756 | 647x589 | 1264x1134 |
| Learner Comp | 840x758 | 688x591 | 1344x1137 |
| Learner Verb | 780x762 | 639x594 | 1248x1143 |

## Ещё два мёртвых места в интерфейсе
- **LevelProgressBar реализован полностью и нигде не размещён.**
  `PluginEditor.cpp:1245` — `progressSection = {}`. Класс с дыханием,
  плавной заливкой, свечением — и нулевые границы на обоих экранах.
- **Окно обновления перекрывается туром и заставкой.** Комментарий
  говорит «самым последним», а `addChildComponent(updateWindow)` стоит на
  `PluginEditor.cpp:482` — раньше `runResults`, `tour`, `screensaver`.
