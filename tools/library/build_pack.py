#!/usr/bin/env python3
"""Собирает звуковой пакет abcTrain одной командой.

    python3 tools/library/build_pack.py packs/rock-basics.csv
    python3 tools/library/build_pack.py packs/rock-basics.csv --out dist/packs

На входе CSV со списком исходников (пример: tools/library/example-pack.csv).
На выходе папка пакета с клипами FLAC, pack.json, CREDITS.md и zip рядом.
Правила и формат описаны в docs/design/sound-library.md.

Что делает скрипт:
  1. Отказывается собирать, если у строки лицензия не из белого списка или
     нет автора. Для CC BY автор — единственное условие лицензии, без него
     клип нельзя распространять. Правило то же, что в приложении
     (ReferenceAudioLibrary::isAllowedLicense).
  2. Режет каждый исходник на клипы: по указанным секундам старта или
     равномерно, пропуская тишину.
  3. Выравнивает громкость каждого клипа по ITU-R BS.1770 (pyloudnorm)
     и ставит короткие фейды на краях: клип играется по кругу, и без
     фейдов на каждом повторе будет щелчок.
  4. Пишет pack.json, CREDITS.md и zip.

Зависимости: pip install soundfile pyloudnorm numpy
"""

import argparse
import csv
import json
import re
import sys
import zipfile
from pathlib import Path

try:
    import numpy as np
    import pyloudnorm
    import soundfile
except ImportError:
    sys.exit("Нужны зависимости: pip install soundfile pyloudnorm numpy")

# Тот же список, что в ReferenceAudioLibrary::isAllowedLicense.
ALLOWED_LICENSES = {"CC0-1.0", "PD", "CC-BY-3.0", "CC-BY-4.0", "CC-BY-SA-3.0", "CC-BY-SA-4.0", "permission"}
CONTENT_VALUES = {"", "instrumental", "vocal-male", "vocal-female", "vocal-mixed",
                  "a-cappella-male", "a-cappella-female"}

TARGET_LUFS = -18.0      # все клипы пакета на одной громкости
DEFAULT_CLIP_SECONDS = 10.0
FADE_SECONDS = 0.02
SILENCE_DBFS = -45.0     # окно тише этого — тишина, клип там не начинается


def fail(message):
    sys.exit("Ошибка: " + message)


def split_list(value):
    return [v.strip() for v in (value or "").split(";") if v.strip()]


def slug(text):
    text = re.sub(r"[^A-Za-z0-9]+", "-", text).strip("-").lower()
    return text or "clip"


def read_rows(csv_path):
    with open(csv_path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    required = {"source_file", "title", "author", "license"}
    if not rows:
        fail(f"{csv_path} пустой")
    missing = required - set(rows[0].keys())
    if missing:
        fail(f"в {csv_path} нет колонок: {', '.join(sorted(missing))}")

    problems = []
    for number, row in enumerate(rows, start=2):
        license_id = (row.get("license") or "").strip()
        if license_id not in ALLOWED_LICENSES:
            problems.append(f"строка {number}: лицензия «{license_id}» не разрешена "
                            f"(можно: {', '.join(sorted(ALLOWED_LICENSES))})")
        if not (row.get("author") or "").strip():
            problems.append(f"строка {number}: нет автора")
        if (row.get("content") or "").strip() not in CONTENT_VALUES:
            problems.append(f"строка {number}: content «{row.get('content')}» — одно из "
                            f"{', '.join(sorted(v for v in CONTENT_VALUES if v))}")
        source = (csv_path.parent / (row.get("source_file") or "")).resolve()
        if not source.is_file():
            problems.append(f"строка {number}: нет файла {source}")

    if problems:
        fail("пакет не собран:\n  " + "\n  ".join(problems))

    return rows


def loud_enough(window):
    rms = np.sqrt(np.mean(np.square(window))) if window.size else 0.0
    return rms > 0 and 20 * np.log10(rms) > SILENCE_DBFS


def pick_starts(audio, rate, clip_seconds, explicit, max_clips):
    length = int(clip_seconds * rate)
    total = audio.shape[0]

    if explicit:
        starts = [int(float(s) * rate) for s in explicit]
        return [s for s in starts if 0 <= s and s + length <= total]

    starts = []
    position = 0
    while position + length <= total and len(starts) < max_clips:
        # Начинаться со звука, а не с тишины: весь клип достаточно громкий
        # и первая секунда тоже.
        if loud_enough(audio[position:position + length]) and loud_enough(audio[position:position + int(rate)]):
            starts.append(position)
            position += length
        else:
            position += int(rate)          # сдвиг на секунду и снова
    return starts


def finish_clip(clip, rate, meter):
    loudness = meter.integrated_loudness(clip)
    if not np.isfinite(loudness):
        return None, None

    clip = pyloudnorm.normalize.loudness(clip, loudness, TARGET_LUFS)

    # Не выше -1 dBFS по пику: нормализация тихого клипа может поднять
    # пики выше нуля, а клиппинг в учебном материале — это чужой призвук.
    peak = np.max(np.abs(clip))
    if peak > 0.891:
        clip = clip * (0.891 / peak)

    fade = max(1, int(FADE_SECONDS * rate))
    ramp = np.linspace(0.0, 1.0, fade)
    clip[:fade] *= ramp[:, None] if clip.ndim == 2 else ramp
    clip[-fade:] *= ramp[::-1][:, None] if clip.ndim == 2 else ramp[::-1]

    return clip, meter.integrated_loudness(clip)


def build(csv_path, out_root, pack_id, title_en, title_ru, version, max_clips):
    rows = read_rows(csv_path)

    pack_dir = out_root / pack_id
    pack_dir.mkdir(parents=True, exist_ok=True)

    clips = []
    for row in rows:
        source = (csv_path.parent / row["source_file"]).resolve()
        audio, rate = soundfile.read(str(source), always_2d=True, dtype="float64")
        meter = pyloudnorm.Meter(rate)

        clip_seconds = float(row.get("clip_seconds") or DEFAULT_CLIP_SECONDS)
        starts = pick_starts(audio, rate, clip_seconds, split_list(row.get("start_seconds")), max_clips)

        for index, start in enumerate(starts, start=1):
            piece = audio[start:start + int(clip_seconds * rate)].copy()
            piece, lufs = finish_clip(piece, rate, meter)
            if piece is None:
                continue

            name = f"{slug(row['author'])}-{slug(row['title'])}-{index:02d}.flac"
            soundfile.write(str(pack_dir / name), piece, rate, subtype="PCM_16")

            clips.append({
                "file": name,
                "seconds": round(piece.shape[0] / rate, 2),
                "loudnessLufs": round(float(lufs), 1),
                "tags": {
                    "genre": split_list(row.get("genre")),
                    "content": (row.get("content") or "").strip(),
                    "instruments": split_list(row.get("instruments")),
                    "character": "",
                },
                "source": {
                    "title": row["title"].strip(),
                    "author": row["author"].strip(),
                    "url": (row.get("url") or "").strip(),
                    "license": row["license"].strip(),
                    "modified": f"cut to {clip_seconds:g} s from {start / rate:.1f} s, "
                                f"loudness-normalised to {TARGET_LUFS:g} LUFS",
                },
            })

    if not clips:
        fail("ни одного клипа: исходники короче длины клипа или тихие")

    manifest = {
        "abcTrainPack": 1,
        "id": pack_id,
        "title": {"en": title_en, "ru": title_ru},
        "version": version,
        "clips": clips,
    }
    (pack_dir / "pack.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                                        encoding="utf-8")

    credits = [f"# {title_ru} / {title_en}", "", "Клипы в этом пакете — фрагменты работ:", ""]
    seen = set()
    for clip in clips:
        s = clip["source"]
        key = (s["author"], s["title"])
        if key in seen:
            continue
        seen.add(key)
        line = f"- **{s['author']}** — {s['title']} · {s['license']}"
        if s["url"]:
            line += f" · {s['url']}"
        credits.append(line)
    credits += ["", "Изменения: фрагменты вырезаны и выровнены по громкости (ITU-R BS.1770)."]
    (pack_dir / "CREDITS.md").write_text("\n".join(credits) + "\n", encoding="utf-8")

    archive = out_root / f"{pack_id}-{version}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for file in sorted(pack_dir.iterdir()):
            z.write(file, f"{pack_id}/{file.name}")

    print(f"Готово: {len(clips)} клипов из {len(rows)} исходников")
    print(f"  папка: {pack_dir}")
    print(f"  архив: {archive}")
    print("Проверить в тренажёре: положить папку в «Звуки для тренировки» → «Открыть папку».")


def main():
    parser = argparse.ArgumentParser(description="Собрать звуковой пакет abcTrain")
    parser.add_argument("csv", type=Path, help="список исходников, см. example-pack.csv")
    parser.add_argument("--out", type=Path, default=Path("dist/packs"))
    parser.add_argument("--id", help="идентификатор пакета (по умолчанию имя CSV)")
    parser.add_argument("--title-en")
    parser.add_argument("--title-ru")
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--max-clips", type=int, default=3, help="клипов на исходник, если секунды не указаны")
    args = parser.parse_args()

    csv_path = args.csv.resolve()
    pack_id = args.id or slug(csv_path.stem)
    build(csv_path, args.out.resolve(), pack_id,
          args.title_en or pack_id, args.title_ru or args.title_en or pack_id,
          args.version, args.max_clips)


if __name__ == "__main__":
    main()
