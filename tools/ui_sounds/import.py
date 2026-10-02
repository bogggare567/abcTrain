#!/usr/bin/env python3
"""Turn a folder of recorded takes into assets/ui-sounds/<event>-<n>.flac (ADR 054).

    python3 tools/ui_sounds/import.py ~/Desktop/sounds

The folder holds one subfolder per event (correct, wrong, step-up,
new-record, achievement, run-end, battle-won, battle-lost, round-start,
open - see assets/ui-sounds/README.md); every audio file inside is a take.
Each take is trimmed of leading/trailing silence, faded (2 ms in, 20 ms
out), peak-normalised to -6 dBFS, cut to 1.5 s at most, and written as
16-bit 48 kHz FLAC. Existing takes of an event are replaced. Needs ffmpeg.
"""
import re
import subprocess
import sys
from pathlib import Path

EVENTS = ["correct", "wrong", "step-up", "new-record", "achievement", "run-end",
          "battle-won", "battle-lost", "round-start", "open"]
AUDIO = {".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a"}
OUT = Path(__file__).resolve().parents[2] / "assets" / "ui-sounds"
PEAK_DB = -6.0
MAX_S = 1.5


def peak_db(path: Path, chain: str) -> float:
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-af", chain + ",volumedetect",
                        "-f", "null", "-"], capture_output=True, text=True)
    m = re.search(r"max_volume:\s*(-?[\d.]+) dB", r.stderr)
    return float(m.group(1)) if m else 0.0


def convert(src: Path, dst: Path) -> None:
    trim = ("silenceremove=start_periods=1:start_threshold=-55dB:start_silence=0.002,"
            "areverse,silenceremove=start_periods=1:start_threshold=-60dB:start_silence=0.01,areverse,"
            f"atrim=0:{MAX_S}")
    gain = PEAK_DB - peak_db(src, trim)
    chain = f"{trim},afade=t=in:d=0.002,areverse,afade=t=in:d=0.02,areverse,volume={gain:.2f}dB"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(src), "-af", chain,
                    "-ar", "48000", "-sample_fmt", "s16", str(dst)], check=True)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    root = Path(sys.argv[1]).expanduser()
    found = 0
    for event in EVENTS:
        folder = root / event
        if not folder.is_dir():
            continue
        takes = sorted(p for p in folder.iterdir() if p.suffix.lower() in AUDIO)
        if not takes:
            continue
        for old in OUT.glob(f"{event}-*.flac"):
            old.unlink()
        for n, take in enumerate(takes, 1):
            dst = OUT / f"{event}-{n:02d}.flac"
            convert(take, dst)
            found += 1
            print(f"{take.name} -> {dst.name}")

    unknown = [p.name for p in root.iterdir() if p.is_dir() and p.name not in EVENTS]
    if unknown:
        print("not an event, skipped:", ", ".join(unknown))
    print(f"{found} takes in {OUT}")
    return 0 if found else 1


if __name__ == "__main__":
    sys.exit(main())
