#!/usr/bin/env python3
"""Пакет abcTrain из папки ваншотов и лупов — одной командой, нужен только ffmpeg.

    python3 tools/library/pack_from_folder.py ~/Desktop/work/abcTrain/audio \
        --only "Drums" "Drums Loops" "Live Instruments" "Synths" "Vocals" \
        --id bogdan-studio --title-ru "Студия Богдана" --title-en "Bogdan's studio" \
        --author "Bogdan Korablev" --license CC-BY-4.0 --version 1.0.0 --out ../packs

    # стартовый набор, встроенный в приложение (assets/starter-pack):
    python3 tools/library/pack_from_folder.py ~/Desktop/work/abcTrain/audio \
        --only "Drums" "Synths" "Vocals" "Live Instruments" --per-category 2 --max-seconds 6 \
        --author "Bogdan Korablev" --license CC-BY-4.0 --embed assets/starter-pack

build_pack.py режет длинные треки на клипы по CSV. Здесь другое: каждый файл —
уже готовый звук (бочка, снейр, шот, луп), категория — его папка. Скрипт:
  1. отказывается работать без автора и лицензии из белого списка (то же
     правило, что ReferenceAudioLibrary::isAllowedLicense);
  2. обрезает тишину в начале, ограничивает длину (--max-seconds), делает
     фейд 20 мс в конце и нормализует пик до -1 dBFS — громкость между
     сторонами выравнивает само приложение;
  3. пишет FLAC 16 бит 44,1 кГц, pack.json (категория = подпапка), CREDITS.md
     и zip — или, с --embed, плоский набор «Категория__NN.flac» для сборки.

--per-category N берёт N файлов из каждой папки, равномерно по списку, —
чтобы стартовый набор был разнообразным, а не «первые десять бочек».
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ALLOWED_LICENSES = {"CC0-1.0", "PD", "CC-BY-3.0", "CC-BY-4.0", "CC-BY-SA-3.0", "CC-BY-SA-4.0", "permission"}
AUDIO = {".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg"}

# Что за инструмент — по словам в пути (теги для фильтра в приложении).
INSTRUMENTS = [
    ("kick", "kick"), ("snare", "snare"), ("clap", "clap"), ("snap", "snap"), ("tom", "toms"),
    ("hat", "hi-hat"), ("ride", "cymbals"), ("crash", "cymbals"), ("cymbal", "cymbals"), ("splash", "cymbals"),
    ("china", "cymbals"), ("gong", "cymbals"), ("shaker", "percussion"), ("percussion", "percussion"),
    ("orchestral drums", "orchestral-drums"), ("vocal", "vocals"), ("choir", "vocals"), ("guitar", "guitar"),
    ("bass", "bass"), ("synth", "synth"), ("arp", "synth"), ("strings", "strings"), ("brass", "brass"),
    ("duduk", "woodwind"), ("flute", "woodwind"), ("whistle", "woodwind"), ("ney", "woodwind"),
    ("sitar", "strings-ethnic"), ("oud", "strings-ethnic"), ("santur", "strings-ethnic"),
]


def fail(msg):
    sys.exit("Ошибка: " + msg)


def slug(text):
    s = re.sub(r"[^0-9A-Za-zА-Яа-яЁё]+", "-", text.strip()).strip("-").lower()
    return s or "x"


def instruments_of(rel):
    low = rel.lower()
    found = []
    for word, tag in INSTRUMENTS:
        if word in low and tag not in found:
            found.append(tag)
    return found


def peak_db(src, chain):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(src), "-af", chain + ",volumedetect",
                        "-f", "null", "-"], capture_output=True, text=True)
    m = re.search(r"max_volume:\s*(-?[\d.]+) dB", r.stderr)
    return float(m.group(1)) if m else 0.0


def duration(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
                       capture_output=True, text=True)
    try:
        return float(r.stdout.strip())
    except ValueError:
        return 0.0


def convert(src, dst, max_seconds):
    trim = ("silenceremove=start_periods=1:start_threshold=-60dB:start_silence=0.001,"
            f"atrim=0:{max_seconds}")
    gain = -1.0 - peak_db(src, trim)
    chain = f"{trim},areverse,afade=t=in:d=0.02,areverse,volume={gain:.2f}dB"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(src), "-af", chain,
                    "-ar", "44100", "-sample_fmt", "s16", str(dst)], check=True)


def pick_even(files, n):
    if n <= 0 or len(files) <= n:
        return files
    step = len(files) / n
    return [files[int(i * step + step / 2)] for i in range(n)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("--only", nargs="*", default=None, help="верхние папки, которые брать")
    ap.add_argument("--id", default="pack")
    ap.add_argument("--title-ru", default="")
    ap.add_argument("--title-en", default="")
    ap.add_argument("--author", required=True)
    ap.add_argument("--url", default="https://soundkorb.ru")
    ap.add_argument("--license", required=True)
    ap.add_argument("--version", default="1.0.0")
    ap.add_argument("--max-seconds", type=float, default=12.0)
    ap.add_argument("--per-category", type=int, default=0)
    ap.add_argument("--out", default="dist/packs")
    ap.add_argument("--fresh", action="store_true", help="собрать пакет заново, а не продолжить")
    ap.add_argument("--embed", help="папка для встроенного набора: плоские файлы Категория__NN.flac")
    args = ap.parse_args()

    if args.license not in ALLOWED_LICENSES:
        fail(f"лицензия {args.license} не из списка: {', '.join(sorted(ALLOWED_LICENSES))}")
    if not args.author.strip():
        fail("нужен автор: для CC BY это единственное условие лицензии")
    if not shutil.which("ffmpeg"):
        fail("нужен ffmpeg")

    root = Path(args.root).expanduser().resolve()
    tops = [root / t for t in args.only] if args.only else [p for p in root.iterdir() if p.is_dir()]

    # Категория — папка, в которой лежит файл.
    by_folder = {}
    for top in tops:
        for f in sorted(top.rglob("*")):
            if f.suffix.lower() in AUDIO and not f.name.startswith("."):
                by_folder.setdefault(f.parent, []).append(f)

    chosen = []
    for folder, files in sorted(by_folder.items()):
        for f in pick_even(files, args.per_category):
            chosen.append((folder, f))

    if args.embed:
        out = Path(args.embed)
        out.mkdir(parents=True, exist_ok=True)
        for old in out.glob("*.flac"):
            old.unlink()
        counters = {}
        lines = [f"# Встроенный набор / Starter set", "",
                 f"Записи: **{args.author}**, {args.license}. Отобрано из библиотеки автора "
                 f"скриптом tools/library/pack_from_folder.py: тишина в начале обрезана, длина до "
                 f"{args.max_seconds:g} с, пик -1 dBFS.", ""]
        for folder, f in chosen:
            category = folder.name.strip()
            counters[category] = counters.get(category, 0) + 1
            name = f"{slug(category)}__{counters[category]:02d}.flac"
            convert(f, out / name, args.max_seconds)
            lines.append(f"- `{name}` — {f.relative_to(root)}")
            print(name)
        (out / "CREDITS.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"{len(chosen)} файлов в {out}")
        return

    # Повторный запуск продолжает: уже сконвертированные файлы не трогаются
    # (на тысяче файлов это десятки минут). --fresh — начать заново.
    pack_dir = Path(args.out).expanduser() / f"{args.id}-{args.version}"
    if args.fresh and pack_dir.exists():
        shutil.rmtree(pack_dir)
    pack_dir.mkdir(parents=True, exist_ok=True)

    # Подпапка пакета = категория в приложении. Одноимённые папки в разных
    # местах (Crashes у электронных, акустических и оркестровых) получают
    # имя родителя, иначе файлы одной перезаписали бы другую.
    subs, used = {}, {}
    for folder in sorted({fo for fo, _ in chosen}):
        name = slug(folder.name)
        if name in used and used[name] != folder:
            name = slug(folder.parent.name + "-" + folder.name)
        used.setdefault(name, folder)
        subs[folder] = name

    clips = []
    for n, (folder, f) in enumerate(chosen, 1):
        rel = folder.relative_to(root)
        sub = subs[folder]
        (pack_dir / sub).mkdir(exist_ok=True)
        name = f"{sub}/{slug(f.stem)}.flac"
        if any(c["file"] == name for c in clips):
            name = f"{sub}/{slug(f.stem)}-{n}.flac"
        if not (pack_dir / name).exists():
            tmp = pack_dir / (name + ".part.flac")
            convert(f, tmp, args.max_seconds)
            tmp.rename(pack_dir / name)
        clips.append({
            "file": name,
            "seconds": round(duration(pack_dir / name), 2),
            "tags": {"genre": [], "content": "vocal-male" if "vocal" in str(rel).lower() else "instrumental",
                     "instruments": instruments_of(str(rel / f.name)), "character": ""},
            "source": {"title": f.stem.strip(), "author": args.author, "url": args.url, "license": args.license,
                       "modified": f"silence trimmed, up to {args.max_seconds:g} s, peak -1 dBFS"},
        })
        if n % 50 == 0:
            print(f"{n}/{len(chosen)}")

    manifest = {"abcTrainPack": 1, "id": args.id,
                "title": {"en": args.title_en or args.id, "ru": args.title_ru or args.title_en or args.id},
                "version": args.version, "clips": clips}
    (pack_dir / "pack.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (pack_dir / "CREDITS.md").write_text(
        f"# {manifest['title']['ru']} / {manifest['title']['en']}\n\n"
        f"Все звуки: **{args.author}**, {args.license} — {args.url}\n", encoding="utf-8")

    zip_path = pack_dir.parent / (pack_dir.name + ".zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_STORED) as z:   # FLAC уже сжат
        for p in sorted(pack_dir.rglob("*")):
            if p.is_file():
                z.write(p, p.relative_to(pack_dir.parent))
    print(f"{len(clips)} клипов → {zip_path} ({zip_path.stat().st_size / 1e6:.0f} МБ)")


if __name__ == "__main__":
    main()
