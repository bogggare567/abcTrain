#!/usr/bin/env python3
"""Тур по приложению из настоящих рендеров — для сайта и README.

    python3 tools/make_tour.py <папка со снимками EditorSnapshots>

Снимки берутся из EditorSnapshots (SNAP_DARK=1 SNAP_SIZE=940x620), то есть
это то, что рисует сама сборка, а не макет и не запись экрана — картинки не
могут разойтись с кодом. На выходе:

  website/public/abctrain-demo.mp4        видео для сайта (лёгкое)
  website/public/abctrain-demo-poster.jpg первый кадр
  docs/screenshots/abctrain-tour.gif      то же для README

Нужен ffmpeg.
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Порядок — как человек впервые проходит по продукту.
FRAMES = [
    "EarTrainer-Home-dark.png",
    "EarTrainer-Training-dark.png",
    "EarTrainer-ZonedAnswered-dark.png",
    "EarTrainer-Distortion-dark.png",
    "EarTrainer-Achievements-dark.png",
    "EarTrainer-Results-dark.png",
    "EarTrainer-StudioEQ-dark.png",
    "EarTrainer-StudioComp-dark.png",
    "LearnerComp-Modules-dark.png",
    "EarTrainer-StudioVerb-dark.png",
]

HOLD = 2.6      # секунд на экран
FADE = 0.5      # перекрёстное затухание


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    if shutil.which("ffmpeg") is None:
        sys.exit("Нужен ffmpeg")

    shots = Path(sys.argv[1])
    frames = [shots / name for name in FRAMES]
    missing = [f.name for f in frames if not f.is_file()]
    if missing:
        sys.exit("Нет снимков: " + ", ".join(missing))

    inputs = []
    for f in frames:
        inputs += ["-loop", "1", "-t", str(HOLD + FADE), "-i", str(f)]

    # Цепочка xfade: каждый следующий экран наплывает на предыдущий.
    chain = []
    last = "[0:v]"
    offset = HOLD
    for i in range(1, len(frames)):
        out = f"[v{i}]"
        chain.append(f"{last}[{i}:v]xfade=transition=fade:duration={FADE}:offset={offset:.2f}{out}")
        last = out
        offset += HOLD

    with tempfile.TemporaryDirectory() as tmp:
        master = Path(tmp) / "tour.mp4"

        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *inputs,
                        "-filter_complex", ";".join(chain) + f";{last}scale=1100:-2,format=yuv420p[out]",
                        "-map", "[out]", "-r", "25", "-c:v", "libx264", "-crf", "30", "-preset", "slow",
                        "-movflags", "+faststart", str(master)], check=True)

        public = ROOT / "website/public"
        shutil.copy(master, public / "abctrain-demo.mp4")

        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(frames[0]),
                        "-vf", "scale=1100:-2", "-q:v", "4", str(public / "abctrain-demo-poster.jpg")], check=True)

        # GIF для README: меньше и реже, с одной палитрой на весь ролик.
        palette = Path(tmp) / "palette.png"
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(master),
                        "-vf", "fps=10,scale=880:-1:flags=lanczos,palettegen=max_colors=128", str(palette)], check=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(master), "-i", str(palette),
                        "-lavfi", "fps=10,scale=880:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5",
                        str(ROOT / "docs/screenshots/abctrain-tour.gif")], check=True)

    for f in (public / "abctrain-demo.mp4", public / "abctrain-demo-poster.jpg", ROOT / "docs/screenshots/abctrain-tour.gif"):
        print(f"{f.relative_to(ROOT)}  {f.stat().st_size // 1024} КБ")


if __name__ == "__main__":
    main()
