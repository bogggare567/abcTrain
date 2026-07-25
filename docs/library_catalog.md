# Library catalog

A bibliography of the audio-engineering book collection the user keeps
locally (`~/Desktop/abcTrain/Книги/`, 159 files, ~2.7 GB — **outside this
git repository**, never committed, never uploaded anywhere). This file
lists **titles and topics only** — no book text was extracted, stored, or
reproduced anywhere in this project. See
[decisions/010-book-library-scope.md](decisions/010-book-library-scope.md)
for why: the original ask was to extract full text and embed quotes from
these books into the shipped plugins, but that would mean reproducing
copyrighted, professionally published work (Bob Katz, Bobby Owsinski,
Roey Izhaki, Mike Senior, Alton Everest, Philip Newell, Irina Aldoshina,
and many more) inside a commercial product distributed on GitHub — a real
copyright problem regardless of attribution. This catalog exists so the
in-plugin "further reading" pointers (see the ADR) can name a relevant
book by topic without quoting it.

**Method**: every filename was read (never the file body); topics below
are a best-effort keyword match against each filename, not a considered
editorial judgment about every book's actual content — a few entries
surely belong under a different heading than the one they landed in.
Good enough for "which shelf is this on," not a rigorous bibliography.
`[RU]`/`[EN]` marks which half of the collection (root folder vs.
`Книги на английском/`) a file came from.

## Микширование (16)

- [EN] Audio Effects Mixing and Mastering — Meitin, Bektas (2014)
- [EN] Mixing and Mastering in the Box: The Guide to Making Great Mixes
- [EN] Mixing Secrets for the Small Studio — Mike Senior
- [RU] Roey Izhaki — Mixing Audio: Concepts, Practices and Tools (2nd ed., пер. Трофимова А.В.)
- [RU] Секреты микс-инженеров — Dylan "3D" Dresdow
- [RU] Секреты микс-инженеров — Tony Maserati, John L.
- [RU] Секреты микс-инженеров — Mike Strange (Eminem)
- [RU] Секреты микс-инженеров — Mike Fraser (AC/DC)
- [EN] The Mixing Engineer's Handbook, 3rd ed. — Bobby Owsinski (2013)
- [EN] The Stairways to Mixing Heaven
- [RU] The Systematic Mixing Guide (рус. перевод)
- [RU] Б. Овсински — Сведение и мастеринг в T-RackS 3
- [RU] Д. Гибсон — Искусство сведения
- [RU] М. Ставроу — Сведение с разумом
- [RU] Микширование живого звука
- [RU] Микширование тяжёлой музыки

## Мастеринг (6)

- [EN] Audio Mastering: Essential Practices — Jonathan Wyner
- [EN] Mastering Audio: The Art and the Science — Bob Katz
- [EN] The Mastering Engineer's Handbook, 4th ed. — Bobby Owsinski
- [RU] Боб Катц — Мастеринг аудио. Искусство и наука (2-е издание)
- [RU] Боб Катц — Стыковка уровней
- [RU] Боб Катц — Секреты мастеринг-инженера

## Микрофоны и микрофонная техника (6)

- [EN] A Study into the Design of Steerable Microphone Arrays
- [EN] Design of Circular Differential Microphone Arrays
- [EN] Directivity-Based Multichannel Audio Signal Processing for Microphones
- [EN] Mic It! Microphones, Microphone Techniques, and Their Impact
- [EN] Russia in the Microphone Age: A History of Soviet Radio, 1919–1970
- [EN] Theory and Applications of Spherical Microphone Array Processing

## Громкоговорители и мониторинг (5)

- [EN] Loudspeaker Physics and Forced Vibration
- [EN] Loudspeakers for Music Recording and Reproduction
- [EN] Loudspeakers for Music Recording and Reproduction, 2nd ed.
- [EN] Sound Reproduction: The Acoustics and Psychoacoustics of Loudspeakers
- [RU] Высококачественные акустические системы — Алдошина

## Акустика помещений / студийная акустика (8)

- [EN] A Design Framework for Absorption and Diffusion
- [EN] Architectural Acoustics Illustrated
- [EN] Architectural Acoustics — K. B. Ginn
- [EN] The Sound System Design Primer
- [RU] Измерения в акустике зданий
- [RU] Старцев Андрей — Акустика контрольных комнат
- [RU] Филипп Ньюелл — Звукозапись, акустика помещений
- [RU] Филипп Ньюелл — Project Studio (Проджект-студии)

## Психоакустика и слух (9)

- [EN] 3D Sound for Virtual Reality and Multimedia
- [EN] Acoustics and Psychoacoustics
- [EN] Audio Bandwidth Extension: Application of Psychoacoustics, Signal Processing
- [RU] Ирина Алдошина — Основы психоакустики
- [RU] Суслов — Звук и слух
- [EN] Zwicker & Fastl — Psychoacoustics: Facts and Models
- [RU] Бережанский П.Н. — Абсолютный музыкальный слух: сущность, природа, генезис
- [RU] К определению понятия тембр
- [RU] Объективные и субъективные характеристики звука. Простота и сложность

## Электроакустика, схемотехника и электроника (20)

- [EN] Solid-State Physics (2009)
- [EN] All-in-One Electronics Guide — Cammen Chan
- [EN] Audio Wiring Guide — John Hetchman
- [EN] Audio Electronics, 2nd ed.
- [EN] Building Valve Amplifiers
- [EN] Designing Audio Power Amplifiers
- [EN] Electronic Properties of Materials: An Introduction for Engineers
- [EN] Hands-On Electronics: A Practical Introduction to Analog and Digital
- [EN] New Acoustics Based on Metamaterials
- [EN] TAB Electronics Guide to Understanding Electricity and Electronics
- [RU] Амиров Ю.Д. (1991) — Основы конструирования
- [RU] Антенны
- [RU] Вторжение комбоусилителей: как Marshall, Hiwatt, Vox и другие изменили звучание музыки
- [RU] Каплан Д., Уайт К. — Практические основы аналоговых и цифровых схем
- [RU] Ковалгин Ю.А., Вологдин Э.И. — Аудиотехника (2013)
- [RU] Основы акустики и электроакустики
- [RU] Пьезоэлектрические акселерометры и предусилители — Brüel & Kjær
- [RU] Сапожков М.А. — Электроакустика (1978)
- [RU] Цифровая обработка сигналов в базисе программируемых логических интегральных схем
- [RU] Электродинамика. Дифракция. Антенны. Научные труды

## Цифровая обработка сигналов (DSP) (13)

- [RU] Анализ аудиоданных с помощью вейвлет-функций
- [EN] DAFX — Digital Audio Effects
- [EN] Digital Signal Processing in Audio and Acoustical Engineering
- [EN] Digital Signal Processing with Field Programmable Gate Arrays
- [RU] Вальпа — Разработка устройств на основе цифровых сигнальных процессоров
- [RU] Динамический диапазон цифровых аудио трактов
- [RU] Основы аналогового и цифрового звука (2006)
- [RU] Основы цифровой обработки сигналов
- [RU] Питер Кирн — Цифровой звук
- [RU] Прогрессивное кодирование аудио с помощью сингулярного разложения
- [RU] Цифровая обработка и синтез звука
- [RU] Цифровая обработка сигналов
- [RU] Цифровая обработка сигналов. Методы предварительной обработки

## Общая акустика и физика звука (20)

- [EN] Principles of Musical Acoustics (2013)
- [EN] Physics of Oscillations and Waves (2018)
- [EN] Acoustics: A Textbook for Engineers and Physicists, Vol. I — Fundamentals
- [EN] Acoustics: A Textbook for Engineers and Physicists, Vol. II — Applications
- [EN] Acoustics: Sound Fields, Transducers and Vibration
- [EN] Fourier Acoustics: Sound Radiation and Nearfield Acoustical Holography
- [RU] Кокс — Книга звука: научная одиссея в страну акустических чудес
- [EN] Master Handbook of Acoustics
- [EN] Master Handbook of Acoustics, 5th ed. — F. Alton Everest, Ken C. Pohlmann
- [EN] Mechanical Vibration and Shock Measurements — Jens Trampe
- [RU] Акустика
- [RU] Акустика для звукорежиссёра — Швец С.И.
- [RU] Акустика музыкальных инструментов. Справочник
- [RU] Акустика. Справочник
- [RU] Волновые задачи акустики
- [RU] Глеб Анфилов — Физика и музыка
- [RU] Испытания конструкций. Часть 2: Анализ мод колебаний и моделирование
- [RU] Колебания и волны
- [RU] Кузнецов Л. — Акустика музыкальных инструментов
- [RU] Музыкальная акустика (2006)

## Музыкальная акустика и теория музыки (5)

- [EN] Music Theory for Computer Musicians — Michael Hewitt
- [EN] The Jazz of Physics: The Secret Link Between Music and the Structure of the Universe
- [EN] Tymoczko (2006)
- [RU] Борисов А. — Математические и физические аспекты теории музыки
- [RU] Занимательная теория музыки — Виноградов, Красовская (1991)

## Звукорежиссура и запись — общие руководства (21)

- [EN] Audio Engineering Explained for Professional Audio Recording
- [EN] Audio Engineering: Know It All
- [EN] Ballou — Handbook for Sound Engineers, 4th ed.
- [EN] Drum Sound and Drum Tuning: Bridging Science and Creativity
- [EN] Modern Recording Techniques, 7th ed.
- [EN] Modern Recording Techniques, 6th ed.
- [EN] Music Engineering, 2nd ed.
- [EN] Practical Recording Techniques: The Step-by-Step Approach
- [EN] Recording Music on Location: Capturing the Live Performance
- [EN] Sound and Recording: Applications and Theory — Audio Engineering Society
- [EN] Sound Engineering Explained, 2nd ed.
- [EN] Sound Reinforcement Handbook — Gary Davis, Ralph Jones
- [EN] The Art of Digital Audio Recording: A Practical Guide for Home and Studio
- [EN] The Recording Engineer's Handbook — Bobby Owsinski
- [EN] Understanding Audio, 2nd ed. — Daniel M. Thompson
- [RU] Ананьев А.Б. — Математика для звукорежиссёра, вып. 1
- [RU] Б. Овсински — Настольная книга звукорежиссёра
- [RU] Ересь звукозаписи
- [RU] Меерзон — Акустические основы звукорежиссуры
- [RU] Пол Уайт — Творческая звукозапись
- [RU] Севашко — Звукорежиссура и запись фонограмм

## Живой звук и озвучивание (sound reinforcement) (4)

- [EN] Don Davis — Sound System Engineering, 4th ed.
- [EN] Sound Reinforcement Engineering: Fundamentals and Practice
- [EN] Sound Reinforcement for Audio Engineers
- [EN] Sound Systems Design and Optimization: Modern Techniques and Tools

## Звук в кино/видео/играх (6)

- [EN] Producing Great Sound for Film and Video: Expert Tips from Preproduction
- [RU] Мишель Шион — Звук: слушать, слышать, наблюдать
- [RU] Звуковой дизайн в видеоиграх: технологии «игрового» аудио
- [RU] Н.Н. Ефимова — Звук в эфире
- [RU] Русинова Е.А. — Звук в пространстве кинематографа (монография)
- [RU] Что происходит с моей фонограммой на радио

## MIDI, музыкальные технологии и бизнес (4)

- [EN] Guide to Music Tech
- [EN] Music and Capitalism: A History of the Present
- [EN] Passman — All You Need to Know About the Music Business, 7th ed.
- [EN] The MIDI Manual, 3rd ed.: A Practical Guide to MIDI

## История и культура звука / философия звука (8)

- [EN] Acoustic Interculturalism: Listening to Performance
- [EN] Articulations of Voice, Affect, and Artifact in the Recording Studio
- [EN] How Does It Sound Now? Legendary Engineers and Vintage Gear
- [EN] Legendary Engineers and Vintage Gear
- [EN] Organised Sound, Vol. 14: Sound Art
- [EN] Wired for Sound: Engineering and Technologies in Sonic Cultures
- [RU] А. Рясов — Едва слышный гул. Введение в философию звука
- [RU] Эван Бондс — Абсолютная музыка. История идеи

## Измерения, метрология и испытания конструкций (4)

- [EN] A Sound Engineer's Guide to Audio Test and Measurement — Glen Ballou
- [EN] Acoustics Today, Winter 2022
- [EN] Audio Metering: Measurements, Standards and Practice
- [RU] Испытания конструкций. Часть 1: Измерения механической подвижности

## Смежная физика/математика (не аудио впрямую) (3)

- [EN] Group Theory (2008)
- [EN] Structural Mechanics: Modelling and Analysis of Frames and Trusses
- [RU] Искусство системного мышления

## Ключевые авторы для "дальнейшего чтения"

Эти книги в коллекции — общепризнанные учебники по конкретным темам
плагинов; именно на них будут указывать блоки "Дальнейшее чтение" в
тултипах/уроках (см. ADR 010), без цитирования:

- **Компрессия / динамика** → Bobby Owsinski (The Mixing Engineer's
  Handbook), Roey Izhaki (Mixing Audio)
- **Эквализация** → Roey Izhaki (Mixing Audio), Mike Senior (Mixing
  Secrets for the Small Studio)
- **Реверберация / пространство** → Филипп Ньюелл (Звукозапись, акустика
  помещений), F. Alton Everest (Master Handbook of Acoustics)
- **Мастеринг / громкость** → Bob Katz (Mastering Audio; Стыковка
  уровней)
- **Психоакустика** → Ирина Алдошина (Основы психоакустики), Zwicker &
  Fastl (Psychoacoustics)
- **Общее сведение** → Б. Овсински (Настольная книга звукорежиссёра),
  Mike Senior
