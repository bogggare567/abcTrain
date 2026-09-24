#!/usr/bin/env python3
"""Готовит звуковой пакет abcTrain из папки собственных записей — одной командой.

    python3 tools/library/prepare_audio.py ~/Desktop/work/abcTrain/audio \\
        --out ~/Desktop/work/abcTrain/packs/bogdan-own \\
        --author "Bogdan Korablev" --license CC-BY-4.0 \\
        --title-ru "Мои записи" --title-en "Own recordings"

Чем отличается от build_pack.py. Тот режет то, что человек уже отобрал и
разметил в CSV. Этот начинает раньше: с папки сырых записей — мультитреков
с концерта по 600 МБ, стемов бита, длинного микса спектакля — где больше
половины времени тишина, счёт палочками и чужой звук в close-микрофонах.
Что делает, по порядку:

  1. Находит файлы и отсеивает чужой материал: имена с названиями
     библиотек сэмплов и лупов (Cymatics, Splice, EZdrummer, Kontakt…).
     Такой звук принадлежит производителю, и лицензия автора пакета на него
     не распространяется. Синтезаторы (Vital, Serum, ANA) — это игра самого
     автора, их не трогаем.
  2. Собирает сессии: стемы в одной папке с одинаковой длиной и частотой —
     это одна запись, и резать их надо в одних и тех же местах, чтобы клипы
     потом можно было сложить обратно.
  3. Читает каждый файл блоками (не целиком: 644 МБ в float64 — это 2,5 ГБ
     памяти) и строит два грубых конверта: громкость каждые 50 мс и
     «где начинаются ноты» на ~11 кГц. Этого хватает, чтобы найти, где
     звук есть, и какой там темп.
  4. Выбирает участки, где инструмент действительно играет, а не только
     слышен в соседнем микрофоне. Находит темп так же, как
     AudioSliceAnalyzer в приложении, и режет ровно N тактов — тогда
     петля повторяется в такт. Шов закрывается перекрёстным фейдом «хвост
     в голову», поэтому щелчка нет в самом файле, а не только в плеере.
     Без уверенного темпа — 8–10 секунд по тихим точкам, петля «free».
  5. Подписывает инструмент: сначала по имени файла, потом сверяет со
     звуком. Если имя и звук спорят или звук неуверен — «Другое», и в
     отчёте написано, почему. Лучше честное «не знаю», чем уверенная ошибка.
  6. Выравнивает громкость как build_pack.py (BS.1770, -18 LUFS, пик не
     выше -1 dBFS), пишет FLAC 16 бит (--wav — WAV), pack.json, CREDITS.md и report.md.

Формат pack.json и правило лицензий — те же, что в build_pack.py и
ReferenceAudioLibrary. Приложение показывает пакет как одну категорию;
инструмент лежит в tags.instruments (по нему работает filesMatching),
а подпапки внутри пакета — для человека, который откроет его в Finder.

Зависимости: pip3 install numpy soundfile pyloudnorm (scipy — по желанию,
с ним прореживание для анализа чище).
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import math
import os
import re
import sys
import warnings
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional, Sequence

INSTALL_HINT = "pip3 install numpy soundfile pyloudnorm"

try:
    import numpy as np
    import pyloudnorm
    import soundfile as sf
except ImportError as error:  # pragma: no cover - зависит от машины
    sys.exit(f"Не хватает Python-модуля «{error.name}».\n"
             f"Установить: {INSTALL_HINT}\n"
             f"(по желанию ещё scipy: pip3 install scipy)")

try:
    from scipy import signal as scipy_signal
except ImportError:  # scipy не обязателен: без него прореживание грубее
    scipy_signal = None

# Правило лицензий и целевая громкость — ровно те же, что у сборщика пакетов.
# Импорт, а не копия: если список лицензий поменяют, поменяется в обоих местах.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_pack import ALLOWED_LICENSES, TARGET_LUFS  # noqa: E402
from build_pack import slug as ascii_slug  # noqa: E402


# ---------------------------------------------------------------------------
# Параметры. Имена повторяют AudioSliceAnalyzer::Options, чтобы приложение
# и скрипт говорили на одном языке: bars_per_slice здесь = barsPerSlice там.
# ---------------------------------------------------------------------------

@dataclass
class Options:
    # Как в приложении: длина клипа без темпа и цель, около которой
    # выбирается число тактов с темпом.
    slice_seconds: float = 8.0
    bars_per_slice: int = 4
    beats_per_bar: int = 4
    minimum_bpm: float = 70.0
    maximum_bpm: float = 180.0

    # В приложении уверенность — доля энергии конверта на лучшем периоде
    # (порог 0.18). Здесь — насколько автокорреляция на лучшем периоде выше
    # медианы по всем периодам. Мера строже: у ровного пэда она около нуля,
    # а доля энергии у пэда бывает 0.3. Поэтому и порог другой.
    minimum_tempo_confidence: float = 0.25

    # Как далеко можно сдвинуть рез без темпа, доля slice_seconds.
    snap_window: float = 0.15

    # Не больше стольких клипов из одного файла — см. комментарий к
    # maxSlicesPerFile в AudioSliceAnalyzer.h (751 клип с одного альбома).
    max_slices_per_file: int = 6

    # Для сессии: столько участков, и каждый режется из всех стемов.
    # 3 участка × 12 стемов = 36 клипов — уже много.
    session_ranges: int = 3

    # Где звук «есть»: сглаженная громкость выше собственного громкого
    # уровня файла минус столько дБ и выше абсолютного пола.
    active_range_db: float = 20.0
    absolute_floor_db: float = -50.0

    # Ниже этой уверенности инструмент не подписывается — клип идёт в «Другое».
    minimum_label_confidence: float = 0.6

    include_flagged: bool = False
    flac: bool = True
    excludes: tuple[str, ...] = ()


@dataclass
class PackMeta:
    pack_id: str
    title_en: str
    title_ru: str
    author: str
    license: str
    url: str = ""
    version: str = "1.0.0"
    genres: tuple[str, ...] = ()


# Внутренние константы — то, что не стоит крутить из командной строки.
ANALYSIS_RATE = 11025.0        # до этой частоты прореживаем для поиска ударов
RMS_HOP_SECONDS = 0.05         # шаг конверта громкости
ACTIVITY_SMOOTH_SECONDS = 1.0  # бочка бьёт раз в полсекунды: 50 мс мало, нужна секунда
MIN_ACTIVE_SECONDS = 8.0       # короче — это счёт, проба звука, не материал
WINDOW_SECONDS = 24.0          # окно, в котором ищется темп и режется один клип
SESSION_MIN_SHARE = 0.5        # участок сессии годится, если играет хотя бы половина стемов
SOLO_RANGES = 2                # стем молчит на общих участках — столько своих участков
SONG_MIN_SECONDS = 60.0        # одиночный файл такой длины без инструмента в имени —
SONG_MAX_SECONDS = 1200.0      # скорее всего готовая песня (см. decide_label)

ONSET_FRAME = 512              # на 11 кГц: 46 мс окно, 11.6 мс шаг
ONSET_HOP = 128
ONSET_BANDS = 24
ONSET_DEADZONE_DB = 0.5        # подъём меньше — дрожание, не нота (ровный пэд даёт ноль)
ONSET_FLOOR_POWER = 1e-8       # -80 dB: шум ниже не считается ударами

PRIOR_BPM = 120.0              # при равной автокорреляции 120 лучше, чем 80 или 160
PRIOR_OCTAVES = 1.0
BPM_SNAP_TOLERANCE = 0.2       # 119.93 → 120: DAW-материал почти всегда в целом темпе
MAX_BEAT_JITTER_SECONDS = 0.04 # удары гуляют сильнее — сетке не доверяем.
                              # 20 мс отсекали почти все живые миксы: пик конверта
                              # плотного микса дрожит на кадр-два. Период при этом
                              # берётся прямой через все доли, ошибка на шве 4 тактов
                              # ~σ/√N — единицы мс; шов ещё и меряется (seam_distance).

MIN_CLIP_SECONDS = 6.0
MAX_CLIP_SECONDS = 14.0
FREE_MIN_SECONDS = 8.0
FREE_MAX_SECONDS = 10.0
BAR_XFADE_SECONDS = 0.015      # в такт: шов в одной фазе, хватает короткого
FREE_XFADE_SECONDS = 0.12      # без такта: фразы не совпадают, нужен длиннее
PREROLL_SECONDS = 0.005        # начинать чуть до удара, чтобы фейд не съел атаку
ONSET_SEARCH_SECONDS = 0.04
ZERO_SNAP_SECONDS = 0.002
SEAM_PROBE_SECONDS = 0.25
SEAM_ROUGH_DB = 6.0            # шов хуже — в отчёте пометка «шов заметен»

PEAK_CEILING = 0.891           # -1 dBFS, как в build_pack.finish_clip

AUDIO_EXTENSIONS = {".wav", ".wave", ".aif", ".aiff", ".flac", ".ogg", ".mp3"}


# ---------------------------------------------------------------------------
# Категории. Ключ = подпапка в пакете и значение tags.instrument.
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Category:
    ru: str
    en: str
    instruments: tuple[str, ...]   # что уходит в tags.instruments (фильтр приложения)
    family: str                    # drums / tonal / mix / other — для проверки «имя против звука»


CATEGORIES: dict[str, Category] = {
    "kick":       Category("Бочка", "Kick", ("drums", "kick"), "drums"),
    "snare":      Category("Малый барабан", "Snare", ("drums", "snare"), "drums"),
    "hihat":      Category("Хай-хэт", "Hi-hat", ("drums", "hihat"), "drums"),
    "cymbals":    Category("Тарелки / оверхеды", "Cymbals / overheads", ("drums", "cymbals"), "drums"),
    "toms":       Category("Томы", "Toms", ("drums", "toms"), "drums"),
    "percussion": Category("Перкуссия / барабанный луп", "Percussion / drum loop", ("drums", "percussion"), "drums"),
    "bass":       Category("Бас", "Bass", ("bass",), "tonal"),
    "guitar":     Category("Гитара", "Guitar", ("guitar",), "tonal"),
    "keys":       Category("Клавиши / синтезатор", "Keys / synth", ("keys",), "tonal"),
    "vocal":      Category("Вокал", "Vocal", ("vocals",), "tonal"),
    "wind":       Category("Духовые", "Wind", ("wind",), "tonal"),
    "mix":        Category("Готовый микс", "Full mix", (), "mix"),
    "other":      Category("Другое", "Other", (), "other"),
}

# Пары, которые не считаются спором «имя против звука»: close-микрофон
# бочки со звуком 808, оверхед, который звучит как перкуссия, и т. п.
COMPATIBLE = {
    frozenset(("cymbals", "hihat")), frozenset(("kick", "toms")), frozenset(("snare", "toms")),
    frozenset(("kick", "bass")),
}


def compatible(a: str, b: str) -> bool:
    if a == b or "mix" in (a, b) or "percussion" in (a, b) and CATEGORIES[a].family == CATEGORIES[b].family:
        return True
    if CATEGORIES[a].family == "tonal" and CATEGORIES[b].family == "tonal" and "bass" not in (a, b):
        return True   # по звуку гитару от клавиш не отличить — это не спор
    return frozenset((a, b)) in COMPATIBLE


# Ключевые слова в имени. «*» на конце — совпадение по началу слова
# («гитар*» ловит «гитара», «гитары»). Без звёздочки — только слово целиком:
# «bd», «oh», «ana» внутри других слов значат что угодно.
KEYWORDS: dict[str, tuple[str, ...]] = {
    "kick": ("kick*", "kik", "bd", "bassdrum", "808", "кик*", "бочк*", "бочка"),
    "snare": ("snare*", "sn", "snr", "sd", "rim", "малый", "малого", "рабочий", "снейр*", "снэр*"),
    "hihat": ("hh", "hat", "hats", "hihat*", "хэт*", "хет", "хеты"),
    "cymbals": ("oh", "ohl", "ohr", "overhead*", "cym*", "ride", "crash", "тарел*", "оверхед*", "райд", "крэш"),
    "toms": ("tom", "toms", "ftom", "rtom", "htom", "mtom", "floortom", "том", "томы", "томов"),
    "percussion": ("perc*", "loop", "loops", "shaker*", "tamb*", "conga*", "bongo*", "clap*", "перк*",
                   "шейкер*", "луп", "лупы", "хлоп*"),
    "bass": ("bass", "basses", "sub", "бас", "басы", "басс"),
    "guitar": ("gtr*", "guitar*", "гитар*"),
    "keys": ("keys", "key", "piano*", "pno", "rhodes", "organ*", "synth*", "syn", "pad", "pads", "omnisphere",
             "vital", "ana", "serum", "massive", "клавиш*", "пиано", "фортепиано", "синт*", "орган*", "пэд*"),
    "vocal": ("vox", "vocal*", "voc", "voice*", "bv", "bvs", "вокал*", "голос*", "бэк*"),
    "wind": ("pipe*", "flute*", "sax*", "trumpet*", "horn*", "brass", "whistle*", "дудк*", "дудка",
             "флейт*", "сакс*", "труба", "трубы", "свирел*"),
    "mix": ("mix", "mixdown", "master*", "микс*", "мастер*", "сведение"),
}

# Сочетания из двух слов проверяются до разбиения на слова.
PHRASES: tuple[tuple[str, str], ...] = (
    (r"bass\s*drum", "kick"), (r"hi\s*-?\s*hat", "hihat"), (r"хай\s*-?\s*х[эе]т", "hihat"),
    (r"floor\s*tom", "toms"), (r"drum\s*loop", "percussion"), (r"full\s*mix", "mix"),
    (r"малый\s*барабан", "snare"), (r"бас\s*-?\s*барабан", "kick"),
)


# ---------------------------------------------------------------------------
# Чужой материал. Лицензии библиотек сэмплов разрешают делать с ними музыку,
# но не раздавать сами звуки отдельно — а звуковой пакет из изолированного
# лупа Cymatics или бочки из EZdrummer и есть раздача звука. Синтезаторы
# (Vital, Serum, ANA, Massive) не в списке: там звук — игра автора.
# ---------------------------------------------------------------------------

THIRD_PARTY: tuple[tuple[str, str], ...] = (
    (r"cymatics", "лупы/сэмплы из библиотеки Cymatics"),
    (r"splice", "сэмплы из Splice"),
    (r"loop\s*masters|loopmasters", "лупы Loopmasters"),
    (r"black\s*octopus|vengeance|kshmr|producer\s*loops|sample\s*magic", "коммерческая библиотека сэмплов"),
    (r"sample\s*-?\s*pack|loop\s*-?\s*pack", "по имени — сэмпл-пак"),
    (r"ez\s*drummer|toontrack", "барабаны из EZdrummer / Toontrack"),
    (r"superior\s*drummer|\bsd3\b", "барабаны из Superior Drummer"),
    (r"\bssd\d*\b|ssd\s*sampler|steven\s*slate", "барабаны из Steven Slate Drums"),
    (r"addictive\s*drums|\bad2\b", "барабаны из Addictive Drums"),
    (r"\bbfd\d*\b|get\s*good\s*drums|\bggd\b", "барабаны из библиотеки (BFD / GetGood)"),
    (r"\bbwb\b", "барабанная библиотека (BWB)"),
    (r"kontakt|native\s*instruments|keyscape", "ромплер: звук — сэмплы производителя"),
    (r"omnisphere", "ромплер Omnisphere: звук — сэмплы Spectrasonics"),
)


def third_party_reason(relative_path: str) -> Optional[str]:
    """Почему файл похож на чужой материал, или None."""
    text = relative_path.lower().replace("_", " ")
    for pattern, reason in THIRD_PARTY:
        if re.search(pattern, text):
            return reason
    return None


def excluded_by(relative_path: str, patterns: Sequence[str]) -> Optional[str]:
    """Шаблон --exclude, под который попал файл. Без * — поиск подстроки."""
    text = relative_path.lower()
    for pattern in patterns:
        p = pattern.lower()
        glob = p if any(ch in p for ch in "*?[") else f"*{p}*"
        if fnmatch.fnmatch(text, glob):
            return pattern
    return None


# ---------------------------------------------------------------------------
# Имена
# ---------------------------------------------------------------------------

_TRANSLIT = dict(zip("абвгдеёжзийклмнопрстуфхцчшщъыьэюя",
                     ["a", "b", "v", "g", "d", "e", "e", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p",
                      "r", "s", "t", "u", "f", "h", "ts", "ch", "sh", "sch", "", "y", "", "e", "yu", "ya"]))


def slug(text: str) -> str:
    """Имя файла только из ASCII: русские имена транслитом, а не «clip»."""
    return ascii_slug("".join(_TRANSLIT.get(ch, ch) for ch in text.lower()))


def tokens_of(text: str) -> list[str]:
    # Буквы отдельно от цифр: «Tom2» → «tom», «2»; «OH_L» → «oh», «l».
    return re.findall(r"[a-zа-яё]+|\d+", text.lower())


@dataclass
class KeywordHit:
    category: str
    word: str
    where: str   # "file" | "folder"


def _keyword_in(text: str) -> Optional[tuple[str, str]]:
    lowered = text.lower()
    for pattern, category in PHRASES:
        match = re.search(pattern, lowered)
        if match:
            return category, match.group(0)
    for token in tokens_of(text):
        for category, words in KEYWORDS.items():
            for word in words:
                if word.endswith("*"):
                    if len(token) >= len(word) - 1 and token.startswith(word[:-1]):
                        return category, token
                elif token == word:
                    return category, token
    return None


def keyword_category(relative_path: str) -> Optional[KeywordHit]:
    """Инструмент по имени файла, а если оно ничего не говорит — по папке.

    «vanya.wav», «Файл 3.wav», «Audio 3.wav» не говорят ничего — и это
    правильный ответ: тогда решает звук.
    """
    path = Path(relative_path)
    hit = _keyword_in(path.stem)
    if hit:
        return KeywordHit(hit[0], hit[1], "file")
    for folder in reversed(path.parts[:-1]):
        hit = _keyword_in(folder)
        if hit:
            return KeywordHit(hit[0], hit[1], "folder")
    return None


def mix_named(relative_path: str) -> bool:
    hit = keyword_category(relative_path)
    return hit is not None and hit.category == "mix"


# ---------------------------------------------------------------------------
# Поиск файлов и сессий
# ---------------------------------------------------------------------------

@dataclass
class SourceFile:
    path: Path
    relative: str
    rate: int
    frames: int
    channels: int


@dataclass
class Group:
    """Одна сессия (несколько стемов одной записи) или один файл."""
    members: list[SourceFile]
    session: Optional[str]
    prefix: str = ""

    @property
    def rate(self) -> int:
        return self.members[0].rate

    @property
    def frames(self) -> int:
        return min(m.frames for m in self.members)


@dataclass
class Discovery:
    sources: list[SourceFile] = field(default_factory=list)
    flagged: list[tuple[str, str]] = field(default_factory=list)     # (файл, причина)
    flagged_included: list[tuple[str, str]] = field(default_factory=list)
    excluded: list[tuple[str, str]] = field(default_factory=list)    # (файл, шаблон)
    unreadable: list[tuple[str, str]] = field(default_factory=list)


def discover(root: Path, options: Options, skip: Optional[Path] = None) -> Discovery:
    found = Discovery()
    for folder, dirs, files in os.walk(root):
        folder_path = Path(folder)
        dirs[:] = sorted(d for d in dirs if not d.startswith(".")
                         and (skip is None or (folder_path / d).resolve() != skip))
        for name in sorted(files):
            path = folder_path / name
            if name.startswith(".") or path.suffix.lower() not in AUDIO_EXTENSIONS:
                continue
            relative = path.relative_to(root).as_posix()

            pattern = excluded_by(relative, options.excludes)
            if pattern:
                found.excluded.append((relative, pattern))
                continue

            reason = third_party_reason(relative)
            if reason and not options.include_flagged:
                found.flagged.append((relative, reason))
                continue
            if reason:
                found.flagged_included.append((relative, reason))

            try:
                info = sf.info(str(path))
            except Exception as error:  # битый файл или формат, который libsndfile не знает
                found.unreadable.append((relative, str(error).splitlines()[0]))
                continue
            found.sources.append(SourceFile(path, relative, int(info.samplerate), int(info.frames),
                                            int(info.channels)))
    return found


def group_sessions(sources: Sequence[SourceFile]) -> list[Group]:
    """Стемы одной папки с одной частотой и длиной (±50 мс) — сессия.

    Файлы, названные миксом, в сессию не входят: несколько версий микса
    одной песни одинаковой длины — это не мультитрек.
    """
    by_folder: dict[str, list[SourceFile]] = {}
    for source in sources:
        by_folder.setdefault(str(Path(source.relative).parent), []).append(source)

    groups: list[Group] = []
    for folder, files in sorted(by_folder.items()):
        candidates = [f for f in files if not mix_named(f.relative)]
        singles = [f for f in files if mix_named(f.relative)]

        buckets: list[list[SourceFile]] = []
        for f in sorted(candidates, key=lambda s: (s.rate, s.frames)):
            last = buckets[-1][0] if buckets else None
            if last and last.rate == f.rate and abs(last.frames - f.frames) <= 0.05 * f.rate:
                buckets[-1].append(f)
            else:
                buckets.append([f])

        sessions = [b for b in buckets if len(b) >= 2]
        folder_name = Path(folder).name if folder != "." else "session"
        for index, bucket in enumerate(sessions, start=1):
            name = folder_name if len(sessions) == 1 else f"{folder_name}-{index}"
            groups.append(Group(sorted(bucket, key=lambda s: s.relative), name))
        singles += [b[0] for b in buckets if len(b) == 1]
        groups += [Group([s], None) for s in sorted(singles, key=lambda s: s.relative)]
    return groups


def assign_prefixes(groups: Sequence[Group]) -> None:
    """Уникальные префиксы имён клипов — до запуска, чтобы параллельные
    задачи не писали в один и тот же файл."""
    used: set[str] = set()
    for group in groups:
        base = slug(group.session) if group.session else slug(Path(group.members[0].relative).stem)
        prefix, n = base, 2
        while prefix in used:
            prefix, n = f"{base}-{n}", n + 1
        used.add(prefix)
        group.prefix = prefix


# ---------------------------------------------------------------------------
# Потоковый анализ: громкость и удары, не загружая файл целиком
# ---------------------------------------------------------------------------

class Decimator:
    """Прореживает до ~11 кГц по блокам, помня состояние фильтра между ними.

    Для ударов и темпа верх выше 5 кГц не нужен, а данных в четыре раза
    меньше. Без scipy — среднее по q отсчётам: фильтр грубый, но для
    конверта ударов достаточный.
    """

    def __init__(self, rate: int):
        self.q = max(1, int(round(rate / ANALYSIS_RATE)))
        self.out_rate = rate / self.q
        self.position = 0
        self.carry = np.zeros(0, dtype=np.float32)
        self.sos = None
        if scipy_signal is not None and self.q > 1:
            self.sos = scipy_signal.butter(8, 0.45 * self.out_rate, fs=rate, output="sos")
            self.state = np.zeros((self.sos.shape[0], 2))

    def push(self, x: np.ndarray) -> np.ndarray:
        if self.q == 1:
            return x
        if self.sos is not None:
            y, self.state = scipy_signal.sosfilt(self.sos, x, zi=self.state)
            first = (-self.position) % self.q
            self.position += len(x)
            return y[first::self.q].astype(np.float32)
        buffer = np.concatenate([self.carry, x])
        n = len(buffer) // self.q
        self.carry = buffer[n * self.q:]
        return buffer[:n * self.q].reshape(n, self.q).mean(axis=1)


class OnsetFlux:
    """Конверт «где начинаются ноты»: сумма подъёмов громкости по полосам.

    Та же идея, что onsetEnvelope в AudioSliceAnalyzer (спектральный поток,
    только подъёмы), но в дБ по 24 полосам и с мёртвой зоной 0,5 дБ. Так
    ровный синус даёт ровно ноль, а не шум округлений, в котором
    автокорреляция нашла бы «темп». Отдельно считается поток ниже 200 Гц:
    по нему угадывается сильная доля (бочка обычно на раз).
    """

    def __init__(self, rate: float):
        self.rate = rate
        self.window = np.hanning(ONSET_FRAME).astype(np.float32)
        self.norm = 1.0 / float(self.window.sum()) ** 2
        freqs = np.fft.rfftfreq(ONSET_FRAME, 1.0 / rate)
        edges = np.geomspace(30.0, rate / 2.0, ONSET_BANDS + 1)
        band = np.digitize(freqs, edges) - 1
        valid = np.flatnonzero((band >= 0) & (band < ONSET_BANDS))
        self.lo, self.hi = int(valid[0]), int(valid[-1]) + 1
        in_range = band[self.lo:self.hi]
        self.starts = np.flatnonzero(np.r_[True, np.diff(in_range) != 0])
        band_ids = in_range[self.starts]
        self.low_bands = edges[band_ids + 1] <= 200.0
        self.buffer = np.zeros(0, dtype=np.float32)
        self.previous: Optional[np.ndarray] = None
        self.flux: list[np.ndarray] = []
        self.low: list[np.ndarray] = []

    @property
    def fps(self) -> float:
        return self.rate / ONSET_HOP

    @property
    def offset_seconds(self) -> float:
        # Подъём в кадре k виден, когда удар уже вошёл в окно: примерно
        # на трёх четвертях окна. Точность ±20 мс; точное место удара
        # потом ищется по полной частоте (refine_onset).
        return 0.75 * ONSET_FRAME / self.rate

    def push(self, x: np.ndarray) -> None:
        buffer = np.concatenate([self.buffer, x.astype(np.float32)])
        count = (len(buffer) - ONSET_FRAME) // ONSET_HOP + 1 if len(buffer) >= ONSET_FRAME else 0
        if count > 0:
            frames = np.lib.stride_tricks.sliding_window_view(buffer, ONSET_FRAME)[::ONSET_HOP][:count]
            power = np.abs(np.fft.rfft(frames * self.window, axis=1)) ** 2 * self.norm
            bands = np.add.reduceat(power[:, self.lo:self.hi], self.starts, axis=1)
            db = 10.0 * np.log10(np.maximum(bands, ONSET_FLOOR_POWER))
            previous = db[:1] if self.previous is None else self.previous
            rise = np.maximum(0.0, np.diff(np.vstack([previous, db]), axis=0) - ONSET_DEADZONE_DB)
            self.previous = db[-1:]
            self.flux.append(rise.sum(axis=1))
            self.low.append(rise[:, self.low_bands].sum(axis=1))
        self.buffer = buffer[count * ONSET_HOP:]

    def result(self) -> tuple[np.ndarray, np.ndarray]:
        if not self.flux:
            return np.zeros(0), np.zeros(0)
        return np.concatenate(self.flux), np.concatenate(self.low)


@dataclass
class Scan:
    rate: int
    frames: int
    hop: int                      # отсчётов на шаг конверта громкости
    rms_db: list[np.ndarray]      # по одному на стем
    flux: np.ndarray              # сумма стемов: для темпа
    low_flux: np.ndarray
    fps: float
    flux_offset: float

    @property
    def hop_seconds(self) -> float:
        return self.hop / self.rate


def scan_group(group: Group) -> Scan:
    """Один проход по всем стемам сессии сразу, блоками по ~2 секунды.

    Сумма стемов нужна для темпа (она ближе всего к тому, что слышно), а
    громкость — по каждому стему отдельно (чтобы знать, где играет именно он).
    """
    rate, frames = group.rate, group.frames
    hop = max(1, int(round(RMS_HOP_SECONDS * rate)))
    block = hop * 40
    decimator = Decimator(rate)
    onsets = OnsetFlux(decimator.out_rate)
    power_sums: list[list[np.ndarray]] = [[] for _ in group.members]

    handles = [sf.SoundFile(str(m.path)) for m in group.members]
    try:
        streams = [h.blocks(blocksize=block, frames=frames, dtype="float32", always_2d=True) for h in handles]
        for blocks in zip(*streams):
            mono = None
            for index, data in enumerate(blocks):
                power = np.square(data).mean(axis=1)
                full = len(power) // hop
                parts = [power[:full * hop].reshape(full, hop).mean(axis=1)]
                if len(power) > full * hop:
                    parts.append(power[full * hop:].mean(keepdims=True))
                power_sums[index].append(np.concatenate(parts))
                channel_mean = data.mean(axis=1)
                mono = channel_mean if mono is None else mono + channel_mean
            if mono is not None:
                onsets.push(decimator.push(mono))
    finally:
        for h in handles:
            h.close()

    rms_db = [10.0 * np.log10(np.concatenate(p) + 1e-12) if p else np.zeros(0) for p in power_sums]
    flux, low = onsets.result()
    return Scan(rate, frames, hop, rms_db, flux, low, onsets.fps, onsets.offset_seconds)


# ---------------------------------------------------------------------------
# Где звук есть
# ---------------------------------------------------------------------------

def moving_average(x: np.ndarray, width: int) -> np.ndarray:
    if width <= 1 or x.size == 0:
        return x.astype(float)
    kernel = np.ones(width) / width
    return np.convolve(x, kernel, mode="same")


def activity_mask(rms_db: np.ndarray, hop_seconds: float, options: Options) -> np.ndarray:
    """Где этот стем сам играет.

    Сглаживание за секунду — потому что бочка бьёт раз в полсекунды, и по
    50-мс кадрам она «молчит» большую часть времени. Порог относительно
    громкого уровня самого файла, а не абсолютный: в close-микрофоне бочки
    всё время слышны соседние барабаны, и «громче -50 dBFS» там почти всё.
    Отличает бочку от чужого звука то, что её удары на 20+ дБ громче.
    """
    if rms_db.size == 0:
        return np.zeros(0, dtype=bool)
    width = max(1, int(round(ACTIVITY_SMOOTH_SECONDS / hop_seconds)))
    smooth = 10.0 * np.log10(moving_average(10.0 ** (rms_db / 10.0), width) + 1e-12)
    audible = smooth[smooth > -90.0]
    if audible.size == 0:
        return np.zeros(rms_db.size, dtype=bool)
    loud = float(np.percentile(audible, 95))
    threshold = max(options.absolute_floor_db, loud - options.active_range_db)
    mask = smooth > threshold

    # Сглаживание растягивает звук на полсекунды в тишину с каждой стороны —
    # срезаем обратно, иначе свободный рез начнётся в тишине.
    trim = width // 2
    for start, stop in runs_of(mask):
        mask[start:min(stop, start + trim)] = False
        mask[max(start, stop - trim):stop] = False
    return mask


def runs_of(mask: np.ndarray) -> list[tuple[int, int]]:
    """Непрерывные участки True как пары [начало, конец)."""
    if mask.size == 0:
        return []
    edges = np.flatnonzero(np.diff(np.r_[0, mask.astype(np.int8), 0]))
    return list(zip(edges[::2].tolist(), edges[1::2].tolist()))


def spread_evenly(items: Sequence, count: int) -> list:
    """Как в AudioSliceAnalyzer: не первые N, а равномерно по всему файлу."""
    if count <= 0 or len(items) <= count:
        return list(items)
    step = len(items) / count
    return [items[int(i * step)] for i in range(count)]


def analysis_windows(share: np.ndarray, hop_seconds: float, minimum_share: float) -> list[tuple[int, int, float]]:
    """Окна (начало, конец в шагах конверта, средняя доля играющих стемов)."""
    windows = []
    per_window = int(round(WINDOW_SECONDS / hop_seconds))
    for start, stop in runs_of(share >= minimum_share):
        length = stop - start
        if length * hop_seconds < MIN_ACTIVE_SECONDS:
            continue
        pieces = max(1, int(round(length / per_window)))
        edges = np.linspace(start, stop, pieces + 1).astype(int)
        for a, b in zip(edges[:-1], edges[1:]):
            windows.append((int(a), int(b), float(share[a:b].mean())))
    return windows


# ---------------------------------------------------------------------------
# Темп
# ---------------------------------------------------------------------------

@dataclass
class Tempo:
    detected: bool = False
    bpm: float = 0.0
    confidence: float = 0.0
    first_downbeat: float = 0.0   # секунды от начала окна
    snapped: bool = False
    beats_matched: float = 0.0    # доля долей, на которых нашёлся удар
    note: str = ""


def _interpolate(env: np.ndarray, positions: np.ndarray) -> np.ndarray:
    return np.interp(positions, np.arange(env.size), env, left=0.0, right=0.0)


def _parabolic(y: np.ndarray, i: int) -> float:
    """Дробная поправка к вершине по трём точкам."""
    if 0 < i < y.size - 1:
        a, b, c = y[i - 1], y[i], y[i + 1]
        denominator = a - 2 * b + c
        if abs(denominator) > 1e-12:
            return float(np.clip(0.5 * (a - c) / denominator, -0.5, 0.5))
    return 0.0


def _autocorrelation_period(env: np.ndarray, fps: float, options: Options) -> tuple[float, float]:
    """(период в кадрах, уверенность). Период 0 — пульса нет."""
    detrended = env - moving_average(env, int(1.5 * fps))
    detrended = detrended - detrended.mean()
    variance = float(np.mean(detrended ** 2))
    shortest = int(math.floor(fps * 60.0 / options.maximum_bpm))
    longest = int(math.ceil(fps * 60.0 / options.minimum_bpm))
    if variance <= 1e-12 or shortest < 1 or longest >= env.size // 2:
        return 0.0, 0.0

    lags = np.arange(shortest, longest + 1)
    ac = np.array([np.mean(detrended[lag:] * detrended[:-lag]) for lag in lags]) / variance
    bpms = 60.0 * fps / lags
    prior = np.exp(-0.5 * (np.log2(bpms / PRIOR_BPM) / PRIOR_OCTAVES) ** 2)
    best = int(np.argmax(np.where(ac > 0, ac * prior, -1.0)))

    # Уверенность — не высота пика, а насколько он выше «фона» остальных
    # периодов. У медленно дышащего пэда автокорреляция высокая на всех
    # задержках сразу — это не темп.
    confidence = float(np.clip(ac[best] - np.median(ac), 0.0, 1.0))
    return float(lags[best]) + _parabolic(ac, best), confidence


def _fit_beats(env: np.ndarray, period: float) -> Optional[tuple[float, float, float, float]]:
    """Уточняет период и фазу по самим ударам: (период, фаза, доля попаданий, разброс в кадрах).

    Период из автокорреляции квантован кадром (11,6 мс). За 16 долей
    ошибка в полкадра на долю — это 90 мс на шве. Поэтому ищем пик конверта
    у каждой ожидаемой доли и проводим через них прямую: наклон — период.
    """
    smooth = moving_average(env, 3)
    offsets = np.arange(0.0, period, 0.5)
    beats = np.arange(0.0, env.size, period)
    scores = [_interpolate(smooth, o + beats).sum() for o in offsets]
    phase = float(offsets[int(np.argmax(scores))])

    loud = float(np.percentile(smooth, 95))
    if loud <= 0:
        return None
    radius = max(1, int(0.2 * period))
    points = []
    count = int((env.size - phase) // period)
    for k in range(count):
        centre = int(round(phase + k * period))
        lo, hi = max(0, centre - radius), min(env.size, centre + radius + 1)
        if hi - lo < 3:
            continue
        i = lo + int(np.argmax(smooth[lo:hi]))
        if smooth[i] >= 0.25 * loud:
            points.append((k, i + _parabolic(smooth, i)))

    if count == 0 or len(points) < 6:
        return None
    k, t = np.array(points).T
    slope, intercept = np.polyfit(k, t, 1)
    jitter = float(np.std(t - (intercept + slope * k)))
    # Доля попаданий — между первым и последним ударом, а не по всему окну:
    # тишина до вступления и после конца — не «промах сетки».
    span = int(k.max() - k.min()) + 1
    return float(slope), float(intercept), len(points) / span, jitter


def detect_tempo(env: np.ndarray, fps: float, options: Options,
                 low_env: Optional[np.ndarray] = None) -> Tempo:
    """Темп и сетка по конверту ударов — идея detectTempo из AudioSliceAnalyzer.

    Как и там — не бит-трекер: одна постоянная скорость на окно, без смены
    темпа и без свинга. Поэтому окно короткое (~24 с): на живом концерте
    у каждой песни свой темп, и каждое окно считается отдельно.
    """
    tempo = Tempo()
    if env.size < fps * 6:
        tempo.note = "мало звука для темпа"
        return tempo

    period, tempo.confidence = _autocorrelation_period(env, fps, options)
    if period <= 0 or tempo.confidence < options.minimum_tempo_confidence:
        tempo.note = "ровного пульса нет"
        return tempo

    fit = _fit_beats(env, period)
    if fit is None:
        tempo.note = "удары не ложатся на сетку"
        return tempo
    period, phase, matched, jitter = fit
    tempo.beats_matched = matched
    if matched < 0.5 or jitter / fps > MAX_BEAT_JITTER_SECONDS:
        tempo.note = f"сетка неровная (попаданий {matched:.0%}, разброс {1000 * jitter / fps:.0f} мс)"
        return tempo

    bpm = 60.0 * fps / period
    if not options.minimum_bpm <= bpm <= options.maximum_bpm:
        tempo.note = "темп вне диапазона"
        return tempo
    if abs(bpm - round(bpm)) <= BPM_SNAP_TOLERANCE:
        bpm, tempo.snapped = float(round(bpm)), True
        period = 60.0 * fps / bpm

    # Сильная доля — догадка: из четырёх фаз та, где больше низа (бочка
    # обычно на раз). Для петли не критично: 4 такта с любой доли
    # повторяются в такт, просто начинаются не с «раз».
    accent = low_env if low_env is not None and low_env.size == env.size and low_env.any() else env
    beats = phase + np.arange(0.0, env.size, period)
    per_phase = [_interpolate(accent, beats[d::options.beats_per_bar]).sum()
                 for d in range(options.beats_per_bar)]
    downbeat = phase + int(np.argmax(per_phase)) * period

    tempo.detected = True
    tempo.bpm = bpm
    tempo.first_downbeat = downbeat / fps
    return tempo


# ---------------------------------------------------------------------------
# Рез и шов
# ---------------------------------------------------------------------------

@dataclass
class Cut:
    start: int              # отсчёт в исходнике
    length: int
    crossfade: int
    loop: str               # "bars" | "free"
    tempo: Tempo
    bars: Optional[int] = None
    seam_db: float = 0.0

    @property
    def seam_ok(self) -> bool:
        return self.seam_db <= SEAM_ROUGH_DB


def read_mono(members: Sequence[SourceFile], start: int, stop: int) -> np.ndarray:
    """Сумма стемов (моно) на участке — для поиска тихих точек и шва."""
    total = np.zeros(max(0, stop - start))
    for member in members:
        data, _ = sf.read(str(member.path), start=start, stop=stop, dtype="float32", always_2d=True)
        total[:len(data)] += data.mean(axis=1)
    return total


def snap_to_quiet_point(audio: np.ndarray, ideal: int, radius: int, probe: int,
                        lowest: int = 0, highest: Optional[int] = None) -> int:
    """То же, что snapToQuietPoint в приложении: ближайшее тихое место."""
    highest = audio.size - probe if highest is None else min(highest, audio.size - probe)
    lo, hi = max(lowest, ideal - radius), min(highest, ideal + radius)
    if hi <= lo:
        return int(np.clip(ideal, lowest, max(lowest, highest)))
    candidates = np.arange(lo, hi + 1, max(1, probe))
    energy = np.array([np.abs(audio[c:c + probe]).sum() for c in candidates])
    return int(candidates[int(np.argmin(energy))])


def refine_onset(audio: np.ndarray, centre: int, rate: int) -> int:
    """Точное место удара рядом с ожидаемым: самый крутой подъём в дБ."""
    radius = int(ONSET_SEARCH_SECONDS * rate)
    lo, hi = max(0, centre - radius), min(audio.size, centre + radius)
    step = max(1, rate // 2000)
    width = max(2, rate // 1000)
    if hi - lo < 4 * width:
        return centre
    env = moving_average(np.abs(audio[lo:hi]), width)[::step]
    db = 20.0 * np.log10(env + 1e-9)
    rise = db[4:] - db[:-4]
    if rise.size == 0 or rise.max() < 6.0:   # чёткого удара нет — оставляем сетку
        return centre
    return lo + (int(np.argmax(rise)) + 2) * step


def _band_levels(x: np.ndarray, rate: int) -> np.ndarray:
    spectrum = np.abs(np.fft.rfft(x * np.hanning(x.size))) ** 2
    freqs = np.fft.rfftfreq(x.size, 1.0 / rate)
    edges = np.geomspace(40.0, rate / 2.0, 17)
    return np.array([10 * np.log10(spectrum[(freqs >= a) & (freqs < b)].sum() + 1e-12)
                     for a, b in zip(edges[:-1], edges[1:])])


def seam_distance(audio: np.ndarray, start: int, length: int, probe: int, rate: int) -> float:
    """Насколько место после конца клипа похоже на его начало (и конец — на
    то, что было до начала), в дБ. Ноль — петля неотличима от продолжения.

    Громкость и спектр по полосам: если через 4 такта вступает вокал или
    стоит брейк, шов будет слышен, даже если он без щелчка.
    """
    pairs = [(start, start + length)]
    if start >= probe:
        pairs.append((start - probe, start + length - probe))
    distances = []
    for a, b in pairs:
        x, y = audio[a:a + probe], audio[b:b + probe]
        if x.size < probe or y.size < probe:
            return float("inf")
        rms = abs(20 * np.log10((np.sqrt(np.mean(x ** 2)) + 1e-9) / (np.sqrt(np.mean(y ** 2)) + 1e-9)))
        bx, by = _band_levels(x, rate), _band_levels(y, rate)
        audible = np.maximum(bx, by) > max(bx.max(), by.max()) - 60
        spectral = float(np.mean(np.abs(bx - by)[audible])) if audible.any() else 0.0
        distances.append(rms + 0.5 * spectral)
    return float(np.mean(distances))


def choose_bars(bpm: float, options: Options) -> int:
    """4 такта по умолчанию; удваиваем/делим, чтобы клип был 6–14 секунд."""
    bar = 60.0 / bpm * options.beats_per_bar
    bars = max(1, options.bars_per_slice)
    while bars * bar < MIN_CLIP_SECONDS:
        bars *= 2
    while bars * bar > MAX_CLIP_SECONDS and bars > 1:
        bars //= 2
    return bars


def plan_bar_cut(tempo: Tempo, audio: np.ndarray, origin: int, rate: int, options: Options) -> Optional[Cut]:
    """Ровно N тактов от сильной доли. Из нескольких сильных долей окна
    берётся та, где шов лучше всего; при равенстве — самая ранняя."""
    bars = choose_bars(tempo.bpm, options)
    bar = 60.0 / tempo.bpm * options.beats_per_bar
    length = int(round(bars * bar * rate))
    crossfade = int(BAR_XFADE_SECONDS * rate)
    probe = int(SEAM_PROBE_SECONDS * rate)
    margin = int(ONSET_SEARCH_SECONDS * rate)
    preroll = int(PREROLL_SECONDS * rate)

    candidates = []
    t = tempo.first_downbeat
    while True:
        beat = int(round(t * rate))
        if beat + length + max(crossfade, probe) + margin > audio.size:
            break
        if beat - preroll >= margin:
            candidates.append(beat)
        t += bar
    if not candidates:
        return None

    scores = [seam_distance(audio, c - preroll, length, probe, rate) for c in candidates]
    best = int(np.argmin(scores))
    onset = refine_onset(audio, candidates[best], rate)
    zero = int(ZERO_SNAP_SECONDS * rate)
    start = snap_to_quiet_point(audio, onset - preroll, zero, max(1, rate // 4000), lowest=0,
                                highest=audio.size - length - crossfade)
    return Cut(origin + start, length, crossfade, "bars", tempo, bars,
               seam_distance(audio, start, length, probe, rate))


def plan_free_cut(tempo: Tempo, audio: np.ndarray, origin: int, rate: int, options: Options) -> Optional[Cut]:
    """Без темпа: 8–10 секунд, оба конца в тихих местах, фейд длиннее."""
    crossfade = int(FREE_XFADE_SECONDS * rate)
    shortest, longest = int(FREE_MIN_SECONDS * rate), int(FREE_MAX_SECONDS * rate)
    usable = audio.size - crossfade
    if usable < int(MIN_CLIP_SECONDS * rate):
        return None
    if usable < shortest:
        shortest = longest = usable

    probe = max(64, shortest // 200)
    radius = int(options.snap_window * options.slice_seconds * rate)
    start = snap_to_quiet_point(audio, int(0.25 * rate), radius, probe, highest=usable - shortest)
    end_lo = start + shortest
    end_hi = min(start + longest, usable)
    end = snap_to_quiet_point(audio, (end_lo + end_hi) // 2, (end_hi - end_lo) // 2, probe,
                              lowest=end_lo, highest=end_hi)
    # Самое тихое место у редкой партии (бочка через такт, бит с паузами) —
    # цифровой ноль задолго до удара: петля начинается с дыры. Если начало
    # глухое, переносим его к первому удару, сохраняя длину.
    head = int(0.15 * rate)
    if 20 * math.log10(float(np.sqrt(np.mean(np.square(audio[start:start + head])))) + 1e-12) < -60.0:
        level = np.abs(audio[start:start + int(1.5 * rate)])
        loud = np.flatnonzero(level > 0.01 * max(float(level.max()), 1e-9))
        if loud.size:
            shift = max(0, int(loud[0]) - int(PREROLL_SECONDS * rate))
            if end + shift <= usable:
                start, end = start + shift, end + shift

    zero = int(ZERO_SNAP_SECONDS * rate)
    start = snap_to_quiet_point(audio, start, zero, 1, highest=end - shortest // 2)
    end = snap_to_quiet_point(audio, end, zero, 1, lowest=start + int(MIN_CLIP_SECONDS * rate), highest=usable)
    length = end - start
    probe_seam = int(SEAM_PROBE_SECONDS * rate)
    return Cut(origin + start, length, crossfade, "free", tempo, None,
               seam_distance(audio, start, length, probe_seam, rate))


def wrap_loop(region: np.ndarray, length: int, crossfade: int) -> np.ndarray:
    """Заворачивает хвост в голову, чтобы файл сам по себе игрался по кругу.

    На стыке после последнего отсчёта клипа (исходник[s+L-1]) плеер играет
    первый — и он должен быть тем, что в исходнике шло дальше: исходник[s+L].
    Поэтому первые X отсчётов — это продолжение после конца, плавно
    (равная мощность) переходящее в настоящее начало. Стык тогда —
    два соседних отсчёта исходника, щелчку взяться неоткуда.
    """
    clip = region[:length].astype(np.float64, copy=True)
    tail = region[length:length + crossfade].astype(np.float64)
    x = min(crossfade, tail.shape[0], length // 4)
    if x > 0:
        phase = (np.arange(x) + 0.5) / x * (np.pi / 2)
        fade_in, fade_out = np.sin(phase)[:, None], np.cos(phase)[:, None]
        clip[:x] = clip[:x] * fade_in + tail[:x] * fade_out
    return clip


# ---------------------------------------------------------------------------
# Признаки звука и классификация
# ---------------------------------------------------------------------------

@dataclass
class Features:
    centroid_hz: float = 0.0
    onsets_per_second: float = 0.0
    sustain_fraction: float = 0.0
    bass_fraction: float = 0.0      # <250 Гц, как в приложении (по амплитудам)
    mid_fraction: float = 0.0       # 250–4000
    high_fraction: float = 0.0      # >4000
    sub_fraction: float = 0.0       # <120 Гц по мощности — бочка
    lowmid_fraction: float = 0.0    # 120–400 Гц — томы
    air_fraction: float = 0.0       # >6 кГц — хэт
    flatness: float = 0.0           # 1 — шум, 0 — тон
    pitchedness: float = 0.0        # пик автокорреляции 50–1000 Гц
    crest_db: float = 0.0
    stereo_correlation: float = 1.0

    @property
    def width(self) -> float:
        return max(0.0, 1.0 - self.stereo_correlation)


def _app_summary(mono: np.ndarray, rate: int, features: Features) -> None:
    """Порт summarise() из AudioSliceAnalyzer.cpp: центроид, три полосы,
    удары в секунду и доля громких кадров. Пороги те же, чтобы character
    в пакете значил то же, что в приложении."""
    size, hop = 1024, 512
    if mono.size < size:
        return
    frames = np.lib.stride_tricks.sliding_window_view(mono, size)[::hop]
    magnitude = np.abs(np.fft.rfft(frames * np.hanning(size), axis=1))[:, 1:size // 2]
    freqs = np.arange(1, size // 2) * rate / size
    total = magnitude.sum()
    if total <= 0:
        return
    features.centroid_hz = float((magnitude * freqs).sum() / total)
    features.bass_fraction = float(magnitude[:, freqs < 250].sum() / total)
    features.mid_fraction = float(magnitude[:, (freqs >= 250) & (freqs < 4000)].sum() / total)
    features.high_fraction = float(magnitude[:, freqs >= 4000].sum() / total)

    flux = np.maximum(0.0, np.diff(magnitude, axis=0, prepend=np.zeros((1, magnitude.shape[1])))).sum(axis=1)
    threshold = max(float(np.median(flux)) * 2.2, float(flux.max()) * 0.15)
    onsets, armed = 0, True
    for value in flux:
        if armed and value > threshold:
            onsets, armed = onsets + 1, False
        elif value < threshold * 0.6:
            armed = True
    features.onsets_per_second = onsets / (mono.size / rate)

    frame = max(64, int(rate * 0.02))
    overall = float(np.sqrt(np.mean(mono ** 2)))
    count = mono.size // frame
    if overall > 1e-9 and count:
        rms = np.sqrt(np.mean(mono[:count * frame].reshape(count, frame) ** 2, axis=1))
        features.sustain_fraction = float(np.mean(rms > overall * 0.35))


def _timbre(mono: np.ndarray, rate: int, features: Features) -> None:
    """Признаки поверх приложения: доли по мощности, шумность, тональность."""
    size, hop = 4096, 2048
    if mono.size < size:
        return
    frames = np.lib.stride_tricks.sliding_window_view(mono, size)[::hop] * np.hanning(size)
    power = np.abs(np.fft.rfft(frames, axis=1)) ** 2
    freqs = np.fft.rfftfreq(size, 1.0 / rate)
    total = power[:, 1:].sum()
    if total <= 0:
        return
    features.sub_fraction = float(power[:, (freqs > 20) & (freqs < 120)].sum() / total)
    features.lowmid_fraction = float(power[:, (freqs >= 120) & (freqs < 400)].sum() / total)
    features.air_fraction = float(power[:, freqs >= 6000].sum() / total)

    energy = power.sum(axis=1)
    loud = energy > energy.max() * 1e-4      # только кадры в пределах 40 дБ от громкого
    band = (freqs >= 60) & (freqs <= min(16000.0, rate / 2))
    p = power[loud][:, band] + 1e-20
    features.flatness = float(np.median(np.exp(np.mean(np.log(p), axis=1)) / np.mean(p, axis=1)))

    # Тональность: автокорреляция кадра через спектр мощности. У ноты с
    # высотой есть сильный повтор на периоде основного тона, у шума нет.
    ac = np.fft.irfft(power[loud], axis=1)
    ac = ac / np.maximum(ac[:, :1], 1e-20)
    lags = np.arange(ac.shape[1])
    lo, hi = int(rate / 1000), min(int(rate / 50), size // 4)
    unbiased = ac[:, lo:hi] / (1.0 - lags[lo:hi] / size)
    features.pitchedness = float(np.median(unbiased.max(axis=1))) if unbiased.size else 0.0

    peak = float(np.max(np.abs(mono)))
    rms = float(np.sqrt(np.mean(mono ** 2)))
    features.crest_db = 20 * math.log10(peak / rms) if rms > 1e-9 and peak > 0 else 0.0


def clip_features(clip: np.ndarray, rate: int) -> Features:
    features = Features()
    mono = clip.mean(axis=1)
    _app_summary(mono, rate, features)
    _timbre(mono, rate, features)
    if clip.shape[1] >= 2:
        left, right = clip[:, 0], clip[:, 1]
        denominator = math.sqrt(float(np.dot(left, left)) * float(np.dot(right, right)))
        # Тишина — не «широко», а «нет данных», как в correlationOf.
        features.stereo_correlation = float(np.dot(left, right) / denominator) if denominator > 1e-9 else 1.0
    return features


def median_features(items: Sequence[Features]) -> Features:
    merged = Features()
    for name in Features.__dataclass_fields__:
        setattr(merged, name, float(np.median([getattr(f, name) for f in items])))
    return merged


def character_of(f: Features) -> str:
    """Копия AudioSliceAnalyzer::classify — те же пороги, те же имена.
    Меняете там — поменяйте здесь (тест приложения это не поймает)."""
    if f.onsets_per_second >= 2.5 and f.sustain_fraction < 0.78:
        return "percussive"
    if f.bass_fraction > 0.55:
        return "bass"
    if f.high_fraction > 0.4:
        return "bright"
    if f.bass_fraction > 0.15 and f.mid_fraction > 0.25 and f.high_fraction > 0.10:
        return "fullRange"
    if f.mid_fraction > 0.45:
        return "midRange"
    return "fullRange"


def ramp(x: float, lo: float, hi: float) -> float:
    """0 ниже lo, 1 выше hi, линейно между ними."""
    return float(np.clip((x - lo) / (hi - lo), 0.0, 1.0))


def audio_scores(f: Features) -> dict[str, float]:
    """Насколько звук похож на каждую категорию, 0..1.

    Нечёткие правила, а не модель: каждое — физическое свойство, которое
    можно проверить глазами в отчёте. Бочка — это удары и низ; хэт — удары
    и верх; бас — низ и тянется. Мелодические инструменты (гитара, клавиши,
    вокал, духовые) по этим признакам надёжно не различаются, поэтому их
    оценка нарочно ограничена сверху (≤0.55, ниже порога 0.6): по звуку
    одному их подписать нельзя, только вместе с именем файла.
    """
    percussive = ramp(0.85 - f.sustain_fraction, 0.05, 0.4) * ramp(f.crest_db, 8.0, 14.0)
    broad = min(1.0, f.bass_fraction / 0.15, f.mid_fraction / 0.25, f.high_fraction / 0.10)
    melodic = (ramp(f.pitchedness, 0.5, 0.8) * ramp(f.mid_fraction, 0.4, 0.65)
               * ramp(f.sustain_fraction, 0.3, 0.6) * (1 - ramp(f.sub_fraction, 0.3, 0.5)))
    return {
        "kick": percussive * ramp(f.sub_fraction, 0.35, 0.65) * (1 - ramp(f.air_fraction, 0.2, 0.4)),
        "snare": (percussive * ramp(f.mid_fraction, 0.35, 0.6) * ramp(f.flatness, 0.02, 0.15)
                  * (1 - ramp(f.sub_fraction, 0.25, 0.5)) * ramp(f.centroid_hz, 600, 1500)
                  * (1 - ramp(f.centroid_hz, 5000, 9000))),
        "hihat": (ramp(f.air_fraction, 0.45, 0.75) * ramp(f.centroid_hz, 4000, 7000)
                  * max(percussive, ramp(f.onsets_per_second, 3, 6)) * (1 - 0.6 * ramp(f.width, 0.15, 0.4))),
        "cymbals": (ramp(f.high_fraction, 0.3, 0.55) * max(ramp(f.sustain_fraction, 0.45, 0.8),
                                                           ramp(f.width, 0.1, 0.35))
                    * (1 - ramp(f.pitchedness, 0.7, 0.9))),
        "toms": (percussive * ramp(f.lowmid_fraction, 0.35, 0.6) * (1 - ramp(f.sub_fraction, 0.4, 0.6))
                 * (1 - ramp(f.onsets_per_second, 3, 5))),
        "percussion": percussive * ramp(f.onsets_per_second, 2.5, 4.0) * broad * 0.8,
        "bass": (ramp(f.bass_fraction, 0.5, 0.75) * ramp(f.sustain_fraction, 0.3, 0.6)
                 * ramp(f.pitchedness, 0.5, 0.8)),
        "mix": broad * ramp(f.sustain_fraction, 0.6, 0.85) * (0.5 + 0.5 * ramp(f.width, 0.03, 0.2)) * 0.8,
        "vocal": melodic * (0.5 + 0.5 * ramp(f.stereo_correlation, 0.8, 0.97)) * 0.55,
        "keys": melodic * (0.4 + 0.6 * ramp(f.width, 0.05, 0.3)) * 0.55,
        "guitar": melodic * 0.45,
        "wind": melodic * ramp(f.pitchedness, 0.85, 0.97) * 0.5,
    }


def audio_verdict(scores: dict[str, float]) -> tuple[str, float]:
    """Лучшая категория и уверенность: высота оценки, урезанная, если
    вторая близко — «похоже и на то, и на это» не уверенность."""
    ranked = sorted(scores.items(), key=lambda kv: kv[1], reverse=True)
    (best, top), second = ranked[0], ranked[1][1]
    margin = float(np.clip((top - second) / 0.3, 0.0, 1.0)) ** 0.5
    return best, round(top * margin, 3)


def describe(f: Features) -> str:
    return (f"низ<120 Гц {f.sub_fraction:.0%}, верх>6 кГц {f.air_fraction:.0%}, "
            f"центроид {f.centroid_hz:.0f} Гц, ударов {f.onsets_per_second:.1f}/с, "
            f"тянется {f.sustain_fraction:.0%}, тон {f.pitchedness:.2f}, ширина {f.width:.2f}")


@dataclass
class Label:
    category: str
    confidence: float
    source: str                # filename | audio | both
    best_guess: str
    reason: str


KEYWORD_CONFIDENCE = 0.75      # имя файла без подтверждения звуком
DISAGREE_CONFIDENCE = 0.7      # звук настолько уверен, что спорит с именем


def decide_label(keyword: Optional[KeywordHit], features: Features, options: Options,
                 song_shaped: bool = False) -> Label:
    """song_shaped — одиночный файл длиной с песню, не часть мультитрека."""
    scores = audio_scores(features)
    best, confidence = audio_verdict(scores)
    heard = f"по звуку: {CATEGORIES[best].ru.lower()} ({confidence:.2f}; {describe(features)})"

    if keyword is not None:
        named = keyword.category
        said = f"{'имя' if keyword.where == 'file' else 'папка'} «{keyword.word}» → {CATEGORIES[named].ru.lower()}"
        if named == "mix":
            return Label("mix", 0.9, "filename", "mix", f"{said}; {heard}")
        contradicts = (confidence >= DISAGREE_CONFIDENCE and scores.get(named, 0.0) < 0.15
                       and not compatible(named, best))
        if contradicts:
            return Label("other", confidence, "both", f"{named}|{best}",
                         f"{said}, но {heard} — спорят, положено в «Другое»")
        if best == named and confidence >= options.minimum_label_confidence:
            joint = 1 - (1 - KEYWORD_CONFIDENCE) * (1 - confidence)
            return Label(named, round(joint, 3), "both", named, f"{said}; звук согласен ({confidence:.2f})")
        return Label(named, KEYWORD_CONFIDENCE, "filename", best, f"{said}; {heard} — не спорит")

    if confidence >= options.minimum_label_confidence and not (song_shaped and CATEGORIES[best].family == "drums"):
        return Label(best, confidence, "audio", best, heard)
    # Одиночный файл на 1–20 минут без инструмента в имени и не из сессии —
    # это почти всегда готовая песня. Звук тут подтверждает, а не решает:
    # «похоже на микс» или широкое стерео. Стем бочки на три минуты в
    # стерео не бывает, а моно-песня, похожая на вокал, остаётся «Другим».
    if song_shaped and (best == "mix" or features.width >= 0.15 or scores["mix"] >= 0.2):
        return Label("mix", 0.7, "shape", best,
                     f"одиночный файл длиной с песню, имя без инструмента; {heard} — готовый микс")
    return Label("other", confidence, "audio", best,
                 f"имя ничего не говорит; {heard} < {options.minimum_label_confidence:.2f} — «Другое»")


# ---------------------------------------------------------------------------
# Громкость и запись
# ---------------------------------------------------------------------------

def normalise(clip: np.ndarray, rate: int) -> Optional[tuple[np.ndarray, float, float]]:
    """(клип, LUFS после, сколько дБ добавлено). Как build_pack.finish_clip,
    только без фейдов на краях: фейд к нулю на стыке петли дал бы провал
    громкости на каждом повторе, а щелчок уже убран заворотом хвоста."""
    meter = pyloudnorm.Meter(rate)
    loudness = meter.integrated_loudness(clip)
    if not np.isfinite(loudness):
        return None
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")   # pyloudnorm предупреждает о пиках — их режем ниже
        out = pyloudnorm.normalize.loudness(clip, loudness, TARGET_LUFS)
    peak = float(np.max(np.abs(out)))
    if peak > PEAK_CEILING:
        out = out * (PEAK_CEILING / peak)
    gain_db = 20 * math.log10(float(np.max(np.abs(out))) / max(float(np.max(np.abs(clip))), 1e-12))
    return out, float(meter.integrated_loudness(out)), gain_db


def output_rate(clip: np.ndarray, rate: int) -> tuple[np.ndarray, int]:
    """44.1 и 48 кГц — как есть. 88.2/96 — вдвое ниже: тренажёру не нужно,
    а место — вдвое. Без scipy — как есть."""
    if rate <= 48000 or scipy_signal is None:
        return clip, rate
    target = 44100 if rate % 44100 == 0 else 48000
    divisor = math.gcd(rate, target)
    return scipy_signal.resample_poly(clip, target // divisor, rate // divisor, axis=0), target


# ---------------------------------------------------------------------------
# Обработка одной группы (сессии или файла)
# ---------------------------------------------------------------------------

@dataclass
class FileReport:
    relative: str
    role: str = ""
    session: Optional[str] = None
    label: Optional[Label] = None
    clips: int = 0
    silence_percent: float = 0.0
    tempo: str = ""
    notes: list[str] = field(default_factory=list)


@dataclass
class GroupResult:
    files: list[FileReport]
    clips: list[dict]
    log: list[str]


def plan_cuts(group: Group, scan: Scan, masks: list[np.ndarray], options: Options,
              members: Optional[Sequence[SourceFile]] = None, cap: Optional[int] = None) -> list[Cut]:
    """Участки для реза. members/cap — для «сольного» реза одного стема сессии."""
    members = group.members if members is None else members
    solo = len(members) == 1 and group.session is not None
    playing = [m for m in masks if m.any()]
    if not playing:
        return []
    length = min(m.size for m in playing)
    share = np.mean([m[:length] for m in playing], axis=0)
    minimum = SESSION_MIN_SHARE if len(members) > 1 else 0.5
    windows = analysis_windows(share, scan.hop_seconds, minimum)
    if group.session and windows and not solo:
        # Сессия: только участки, где играет почти столько же стемов, сколько
        # в самом полном месте — там клипы можно складывать обратно в песню.
        best = max(w[2] for w in windows)
        windows = [w for w in windows if w[2] >= 0.8 * best]
    if cap is None:
        cap = options.session_ranges if group.session else options.max_slices_per_file
    windows = spread_evenly(windows, cap)

    cuts = []
    for first, last, _ in windows:
        start = first * scan.hop
        stop = min(scan.frames, last * scan.hop + int(1.0 * scan.rate))
        audio = read_mono(members, start, stop)
        f0 = max(0, int((start / scan.rate - scan.flux_offset) * scan.fps))
        f1 = min(scan.flux.size, int((last * scan.hop / scan.rate - scan.flux_offset) * scan.fps))
        tempo = detect_tempo(scan.flux[f0:f1], scan.fps, options, scan.low_flux[f0:f1])
        # Время конверта → время окна: сдвиг кадра и начало окна в кадрах.
        tempo.first_downbeat += f0 / scan.fps + scan.flux_offset - start / scan.rate
        cut = plan_bar_cut(tempo, audio, start, scan.rate, options) if tempo.detected else None
        if cut is None:
            cut = plan_free_cut(tempo, audio, start, scan.rate, options)
        if cut is not None:
            cuts.append(cut)
    return cuts


def tempo_summary(cuts: Sequence[Cut]) -> str:
    parts = []
    for cut in cuts:
        if cut.loop == "bars":
            parts.append(f"{cut.tempo.bpm:g} BPM, {cut.bars} т. (уверенность {cut.tempo.confidence:.2f})"
                         + ("" if cut.seam_ok else ", шов заметен"))
        else:
            parts.append(f"free ({cut.tempo.note or 'без темпа'})")
    unique = list(dict.fromkeys(parts))
    return "; ".join(unique) if unique else "—"


def cut_member(member: SourceFile, cut: Cut) -> np.ndarray:
    region, _ = sf.read(str(member.path), start=cut.start, stop=cut.start + cut.length + cut.crossfade,
                        dtype="float32", always_2d=True)
    return wrap_loop(region[:, :2], cut.length, cut.crossfade)


def clip_entry(file_name: str, clip: np.ndarray, rate: int, lufs: float, gain_db: float, cut: Cut,
               member: SourceFile, group: Group, role: str, label: Label, features: Features,
               meta: PackMeta) -> dict:
    """Одна запись clips[] в pack.json: поля приложения + разметка этого скрипта."""
    category = CATEGORIES[label.category]
    content = "instrumental" if category.family == "drums" or label.category in ("bass", "guitar", "keys", "wind") \
        else ""
    start_seconds = cut.start / member.rate
    return {
        "file": file_name,
        "seconds": round(clip.shape[0] / rate, 3),
        "loudnessLufs": round(lufs, 1),
        "tags": {
            "genre": list(meta.genres),
            "content": content,
            "instruments": list(category.instruments),
            "character": character_of(features),
            "instrument": label.category,
            "role": role,
            "session": group.session,
            "bpm": cut.tempo.bpm if cut.loop == "bars" else None,
            "bars": cut.bars,
            "loop": cut.loop,
            "tempoConfidence": round(cut.tempo.confidence, 3),
            "seamDb": round(cut.seam_db, 2) if math.isfinite(cut.seam_db) else None,
            "confidence": label.confidence,
            "labelSource": label.source,
            "bestGuess": label.best_guess,
            "reason": label.reason,
        },
        "source": {
            "title": Path(member.relative).stem if not group.session else f"{group.session} — {Path(member.relative).stem}",
            "author": meta.author,
            "url": meta.url,
            "license": meta.license,
            "modified": (f"cut {clip.shape[0] / rate:.2f} s from {start_seconds:.3f} s"
                         + (f" ({cut.bars} bars at {cut.tempo.bpm:g} BPM)" if cut.loop == "bars" else "")
                         + f", loop crossfade, loudness-normalised to {TARGET_LUFS:g} LUFS"),
            "file": member.relative,
            "startSeconds": round(start_seconds, 6),
            "gainDb": round(gain_db, 2),
        },
    }


def silent_head(clip: np.ndarray, rate: int) -> bool:
    """Клип, который начинается с тишины (пауза в партии, вступление после
    счёта), на каждом повторе начинается с дыры. То же правило, что в
    приложении (AudioSliceAnalyzer::analyse): первая секунда должна звучать."""
    head = clip[: int(rate)]
    rms = float(np.sqrt(np.mean(np.square(head)))) if head.size else 0.0
    return rms <= 0.0 or 20 * math.log10(rms) < -60.0


def process_member(member: SourceFile, index: int, group: Group, cuts: Sequence[Cut], mask: np.ndarray,
                   scan: Scan, options: Options, meta: PackMeta, out_dir: Optional[Path]) -> tuple[FileReport, list[dict]]:
    report = FileReport(member.relative, session=group.session,
                        silence_percent=round(100.0 * (1.0 - float(mask.mean())) if mask.size else 100.0, 1))
    report.tempo = tempo_summary(cuts)
    if not mask.any():
        report.role = "stem" if group.session else "—"
        report.notes.append("звука нет (только тишина или чужой звук в микрофоне) — пропущен")
        return report, []

    pieces: list[tuple[Cut, np.ndarray, Features]] = []
    for cut in cuts:
        a, b = cut.start // scan.hop, (cut.start + cut.length) // scan.hop
        if group.session and mask[a:b].mean() < 0.5:
            # Стем в этом месте молчит или слышен только в чужих микрофонах.
            # Клип из фона соседей нормализация подняла бы до -18 LUFS.
            report.notes.append(f"на участке {cut.start / scan.rate:.1f} с стем не играет — клипа нет")
            continue
        clip = cut_member(member, cut)
        if silent_head(clip, member.rate):
            report.notes.append(f"на участке {cut.start / scan.rate:.1f} с первая секунда стема — тишина: клипа нет")
            continue
        pieces.append((cut, clip, clip_features(clip, member.rate)))

    if not pieces and group.session:
        # Стем играет, но не там, где играет вся сессия: малый только в
        # припевах, вокал в двух песнях из десяти. Режем там, где играет он
        # сам — темп тот же (общий конверт), шов меряется по нему одному.
        # Такие клипы уже не складываются обратно в песню, но это лучше,
        # чем потерять инструмент целиком.
        solo_cuts = plan_cuts(group, scan, [mask], options, members=[member], cap=SOLO_RANGES)
        for cut in solo_cuts:
            clip = cut_member(member, cut)
            if not silent_head(clip, member.rate):
                pieces.append((cut, clip, clip_features(clip, member.rate)))
        if pieces:
            report.notes.append(f"на общих участках молчит — {len(pieces)} клип(а) с его собственных")
            report.tempo = tempo_summary(solo_cuts)

    if not pieces:
        report.role = "stem" if group.session else "—"
        return report, []

    keyword = keyword_category(member.relative)
    seconds = member.frames / member.rate
    song_shaped = group.session is None and SONG_MIN_SECONDS <= seconds <= SONG_MAX_SECONDS
    label = decide_label(keyword, median_features([p[2] for p in pieces]), options, song_shaped)
    report.label = label
    if group.session:
        report.role = "mix" if label.category == "mix" and keyword is not None else "stem"
    else:
        report.role = "stem" if label.category not in ("mix", "other") else "mix"

    entries = []
    stem_slug = slug(Path(member.relative).stem)
    for number, (cut, clip, features) in enumerate(pieces, start=1):
        finished = normalise(clip, member.rate)
        if finished is None:
            report.notes.append(f"клип {number}: тишина после выреза — пропущен")
            continue
        audio, lufs, gain_db = finished
        audio, rate = output_rate(audio, member.rate)
        extension = "flac" if options.flac else "wav"
        name = (f"{group.prefix}-{number:02d}-{stem_slug}.{extension}" if group.session
                else f"{group.prefix}-{number:02d}.{extension}")
        file_name = f"{label.category}/{name}"
        if out_dir is not None:
            (out_dir / label.category).mkdir(parents=True, exist_ok=True)
            # FLAC — без потерь и вдвое меньше: пакет скачивают, и 700 МБ
            # WAV против ~350 МБ FLAC — это разница между «скачаю» и «потом».
            # 16 бит, как у импорта в приложении: это тренировочные петли, а
            # не мастер. Упражнения прячут изменения от децибела, а 24 бита
            # стоят полтора раза места.
            sf.write(str(out_dir / file_name), audio, rate, subtype="PCM_16",
                     format="FLAC" if options.flac else "WAV")
        entries.append(clip_entry(file_name, audio, rate, lufs, gain_db, cut, member, group,
                                  report.role, label, features, meta))
    report.clips = len(entries)
    return report, entries


def process_group(group: Group, options: Options, meta: PackMeta, out_dir: Optional[Path]) -> GroupResult:
    names = ", ".join(Path(m.relative).name for m in group.members)
    log = [f"{'сессия «' + group.session + '»: ' if group.session else ''}{names}"]

    if group.frames < 4 * group.rate:
        reports = [FileReport(m.relative, "—", group.session, notes=["короче 4 секунд — пропущен"])
                   for m in group.members]
        return GroupResult(reports, [], log)

    scan = scan_group(group)
    masks = [activity_mask(db, scan.hop_seconds, options) for db in scan.rms_db]
    cuts = plan_cuts(group, scan, masks, options)

    reports, entries = [], []
    for index, member in enumerate(group.members):
        report, clips = process_member(member, index, group, cuts, masks[index], scan, options, meta, out_dir)
        reports.append(report)
        entries += clips
        category = CATEGORIES[report.label.category].ru if report.label else "—"
        log.append(f"    {Path(member.relative).name}: {category}, клипов {report.clips}, "
                   f"тишины {report.silence_percent:.0f} %, темп {report.tempo}")
    return GroupResult(reports, entries, log)


# ---------------------------------------------------------------------------
# Пакет и отчёт
# ---------------------------------------------------------------------------

def write_pack(out_dir: Path, meta: PackMeta, clips: list[dict]) -> None:
    manifest = {
        "abcTrainPack": 1,
        "id": meta.pack_id,
        "title": {"en": meta.title_en, "ru": meta.title_ru},
        "version": meta.version,
        "clips": clips,
    }
    (out_dir / "pack.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    credits = [f"# {meta.title_ru} / {meta.title_en}", "",
               f"Все клипы — записи автора: **{meta.author}** · {meta.license}"
               + (f" · {meta.url}" if meta.url else ""), "",
               "Изменения: фрагменты вырезаны, закольцованы и выровнены по громкости (ITU-R BS.1770)."]
    (out_dir / "CREDITS.md").write_text("\n".join(credits) + "\n", encoding="utf-8")


def build_report(root: Path, out_dir: Path, meta: PackMeta, found: Discovery,
                 reports: list[FileReport], clips: list[dict], dry_run: bool) -> str:
    lines = [f"# Подготовка звуков: {meta.title_ru}", ""]
    if dry_run:
        lines += ["**Пробный запуск (--dry-run): аудио не записано.**", ""]
    lines += [
        f"- Источник: `{root}`",
        f"- Пакет: `{out_dir}` (id `{meta.pack_id}`)",
        f"- Автор: {meta.author} · лицензия {meta.license}",
        f"- Файлов найдено: {len(found.sources) + len(found.flagged) + len(found.excluded) + len(found.unreadable)}, "
        f"обработано: {len(found.sources)}, клипов: {len(clips)}",
        f"- Исключено как чужой материал: {len(found.flagged)}, по --exclude: {len(found.excluded)}, "
        f"не прочитано: {len(found.unreadable)}",
        "",
        "## Как читать",
        "",
        "- **Категория** — куда положен клип. «Другое» значит: имя файла ничего не говорит и звук не "
        "уверен, или имя и звук спорят. В колонке «почему» — лучшая догадка и цифры, по которым она "
        "сделана. Лучше переименовать файл (kick, snare, bass, vox…) и запустить снова, чем верить догадке.",
        "- **Уверенность** 0…1. «имя» — подпись из имени файла, «звук» — по признакам звука, «оба» — "
        "совпали, «форма файла» — одиночный файл длиной с песню без инструмента в имени. По звуку одному скрипт уверенно узнаёт только бочку, хай-хэт, бас и готовый микс; "
        "гитару, клавиши, вокал и духовые — только вместе с именем.",
        "- **Темп**: «BPM, N т.» — клип ровно N тактов, петля идёт в такт. «free» — пульса не нашлось, "
        "клип 8–10 с по тихим местам с длинным перекрёстным фейдом.",
        "- **Тишина** — какая доля файла пропущена: тишина, счёт, места, где стем слышен только в "
        "чужих микрофонах.",
        "",
        "## Файлы",
        "",
        "| Файл | Роль | Сессия | Категория | Уверенность | Почему | Темп | Клипов | Тишина |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    source_names = {"filename": "имя", "audio": "звук", "both": "оба", "shape": "форма файла"}
    for r in sorted(reports, key=lambda r: r.relative):
        label = r.label
        category = CATEGORIES[label.category].ru if label else "—"
        confidence = f"{label.confidence:.2f} ({source_names[label.source]})" if label else "—"
        reason = label.reason if label else ""
        if r.notes:
            reason = (reason + "; " if reason else "") + "; ".join(r.notes)
        reason = reason.replace("|", "/")
        lines.append(f"| `{r.relative}` | {r.role or '—'} | {r.session or '—'} | {category} | {confidence} | "
                     f"{reason} | {r.tempo or '—'} | {r.clips} | {r.silence_percent:.0f} % |")

    lines += ["", "## Чужой материал", "",
              "Имена с названиями библиотек сэмплов, лупов и барабанных/ромплерных инструментов. "
              "Такой звук принадлежит производителю: его лицензия разрешает делать с ним музыку, но не "
              "раздавать сам звук отдельно, а клип в пакете — это именно раздача. Синтезаторы (Vital, "
              "Serum, ANA) — игра автора, их правило не касается. Если файл на самом деле ваш — "
              "переименуйте его или запустите с `--include-flagged`.", ""]
    if found.flagged:
        lines += [f"- `{name}` — {reason} (исключён)" for name, reason in found.flagged]
    if found.flagged_included:
        lines += [f"- `{name}` — {reason} (**включён** по --include-flagged)" for name, reason in found.flagged_included]
    if not found.flagged and not found.flagged_included:
        lines.append("Не найдено.")

    if found.excluded:
        lines += ["", "## Исключено по --exclude", ""]
        lines += [f"- `{name}` — шаблон «{pattern}»" for name, pattern in found.excluded]
    if found.unreadable:
        lines += ["", "## Не прочитано", ""]
        lines += [f"- `{name}` — {error}" for name, error in found.unreadable]
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Командная строка
# ---------------------------------------------------------------------------

@dataclass
class RunResult:
    reports: list[FileReport]
    clips: list[dict]
    found: Discovery
    report_text: str


def prepare(root: Path, out_dir: Path, meta: PackMeta, options: Options,
            dry_run: bool = False, jobs: int = 1, quiet: bool = False) -> RunResult:
    say = (lambda *_: None) if quiet else (lambda text: print(text, flush=True))
    found = discover(root, options, skip=out_dir.resolve())
    groups = group_sessions(found.sources)
    assign_prefixes(groups)
    target = None if dry_run else out_dir
    if target is not None:
        target.mkdir(parents=True, exist_ok=True)

    say(f"Файлов: {len(found.sources)} в {len(groups)} группах; чужих: {len(found.flagged)}, "
        f"исключено: {len(found.excluded)}")
    results: list[GroupResult] = []
    if jobs > 1 and len(groups) > 1:
        with ProcessPoolExecutor(max_workers=jobs) as pool:
            futures = {pool.submit(process_group, g, options, meta, target): g for g in groups}
            for done, future in enumerate(as_completed(futures), start=1):
                result = future.result()
                results.append(result)
                say(f"[{done}/{len(groups)}] " + "\n".join(result.log))
    else:
        for done, group in enumerate(groups, start=1):
            say(f"[{done}/{len(groups)}] {group.members[0].relative}{' …' if len(group.members) > 1 else ''}")
            result = process_group(group, options, meta, target)
            results.append(result)
            say("\n".join(result.log[1:]))

    reports = [r for result in results for r in result.files]
    clips = sorted((c for result in results for c in result.clips), key=lambda c: c["file"])
    text = build_report(root, out_dir, meta, found, reports, clips, dry_run)
    if target is not None:
        if clips:
            write_pack(target, meta, clips)
        (target / "report.md").write_text(text, encoding="utf-8")
    return RunResult(reports, clips, found, text)


def parse_args(argv: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Подготовить звуковой пакет abcTrain из папки своих записей (стемы, миксы).")
    parser.add_argument("source", type=Path, help="папка с записями (просматривается с подпапками)")
    parser.add_argument("--out", type=Path, required=True, help="папка пакета, которую создать")
    parser.add_argument("--author", required=True, help="автор записей — попадает в «Авторы»")
    parser.add_argument("--license", required=True, help=f"одна из: {', '.join(sorted(ALLOWED_LICENSES))}")
    parser.add_argument("--title-ru")
    parser.add_argument("--title-en")
    parser.add_argument("--id", help="идентификатор пакета (по умолчанию имя папки --out)")
    parser.add_argument("--url", default="", help="ссылка на автора")
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--genre", action="append", default=[], help="жанр для всех клипов, можно несколько раз")
    parser.add_argument("--exclude", action="append", default=[], metavar="PATTERN",
                        help="пропустить файлы по шаблону (glob или часть имени), можно несколько раз")
    parser.add_argument("--include-flagged", action="store_true",
                        help="взять и файлы, похожие на чужие сэмплы (если уверены, что они ваши)")
    parser.add_argument("--max-clips", type=int, default=Options.max_slices_per_file,
                        help="клипов с одного файла (по умолчанию 6)")
    parser.add_argument("--session-ranges", type=int, default=Options.session_ranges,
                        help="участков на сессию, каждый режется из всех стемов (по умолчанию 3)")
    parser.add_argument("--bars", type=int, default=Options.bars_per_slice, help="тактов в клипе (по умолчанию 4)")
    parser.add_argument("--wav", action="store_true", help="писать WAV вместо FLAC (FLAC вдвое меньше, без потерь)")
    parser.add_argument("--dry-run", action="store_true", help="только отчёт, ничего не записывать")
    parser.add_argument("--jobs", type=int, default=1, help="сколько групп обрабатывать параллельно")
    parser.add_argument("--force", action="store_true", help="перезаписать существующий пакет")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    source, out_dir = args.source.expanduser().resolve(), args.out.expanduser().resolve()

    if args.license not in ALLOWED_LICENSES:
        sys.exit(f"Ошибка: лицензия «{args.license}» не разрешена (можно: {', '.join(sorted(ALLOWED_LICENSES))}). "
                 "Правило то же, что в build_pack.py и в приложении.")
    if not args.author.strip():
        sys.exit("Ошибка: нужен --author — без автора клип нельзя указать в «Авторах».")
    if not source.is_dir():
        sys.exit(f"Ошибка: нет папки {source}")
    if not args.dry_run and (out_dir / "pack.json").exists() and not args.force:
        sys.exit(f"Ошибка: в {out_dir} уже есть пакет. --force — перезаписать.")

    meta = PackMeta(pack_id=args.id or slug(out_dir.name), title_en=args.title_en or args.title_ru or out_dir.name,
                    title_ru=args.title_ru or args.title_en or out_dir.name, author=args.author.strip(),
                    license=args.license, url=args.url, version=args.version, genres=tuple(args.genre))
    options = Options(max_slices_per_file=args.max_clips, session_ranges=args.session_ranges,
                      bars_per_slice=args.bars, include_flagged=args.include_flagged, flac=not args.wav, excludes=tuple(args.exclude))

    result = prepare(source, out_dir, meta, options, dry_run=args.dry_run, jobs=max(1, args.jobs))
    if args.dry_run:
        print()
        print(result.report_text)
        return 0
    if not result.clips:
        print(f"Ни одного клипа. Почему — в {out_dir / 'report.md'}")
        return 1
    print(f"Готово: {len(result.clips)} клипов → {out_dir}")
    print(f"  отчёт: {out_dir / 'report.md'}")
    print("Проверить: положить папку в «Звуки для тренировки» (в тренажёре «Открыть папку»).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
