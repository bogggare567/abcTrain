"""Тесты tools/library/prepare_audio.py на синтезированном звуке.

    python3 -m unittest discover -s tests/tools -v

Каждый тест строит сигнал с известным ответом (темп, инструмент, тишина)
и проверяет, что скрипт его нашёл — а не то, что он «что-то выдал».
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "library"))
import prepare_audio as pa  # noqa: E402

RATE = 44100
RNG = np.random.default_rng(1234)


def silence(seconds: float) -> np.ndarray:
    return np.zeros(int(seconds * RATE))


def kick_hit() -> np.ndarray:
    """Синус, падающий с 120 до 50 Гц, затухание 60 мс — как бочка."""
    t = np.arange(int(0.4 * RATE)) / RATE
    frequency = 50 + 70 * np.exp(-t / 0.03)
    phase = 2 * np.pi * np.cumsum(frequency) / RATE
    return 0.9 * np.sin(phase) * np.exp(-t / 0.06)


def hat_hit() -> np.ndarray:
    """Шум выше 7 кГц, затухание 30 мс."""
    n = int(0.15 * RATE)
    noise = RNG.standard_normal(n)
    spectrum = np.fft.rfft(noise)
    spectrum[np.fft.rfftfreq(n, 1 / RATE) < 7000] = 0
    return 0.5 * np.fft.irfft(spectrum, n) / np.std(np.fft.irfft(spectrum, n)) * 0.3 * np.exp(-np.arange(n) / RATE / 0.03)


def pattern(hit: np.ndarray, bpm: float, per_beat: int, seconds: float) -> np.ndarray:
    out = np.zeros(int(seconds * RATE) + hit.size)
    step = 60.0 / bpm / per_beat
    t = 0.0
    while t < seconds:
        i = int(round(t * RATE))
        out[i:i + hit.size] += hit
        t += step
    return out[:int(seconds * RATE)]


def bass_line(bpm: float, seconds: float) -> np.ndarray:
    """Нота на такт (55/73/65/49 Гц), с маленькой паузой перед следующей."""
    bar = 240.0 / bpm
    notes = [55.0, 73.4, 65.4, 49.0]
    out = np.zeros(int(seconds * RATE))
    for k in range(int(seconds / bar)):
        a, b = int(k * bar * RATE), int(((k + 1) * bar - 0.05) * RATE)
        t = np.arange(b - a) / RATE
        out[a:b] = 0.5 * np.sin(2 * np.pi * notes[k % 4] * t) * np.minimum(1, t / 0.01)
    return out


def write(path: Path, audio: np.ndarray) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(path), audio, RATE, subtype="PCM_24")
    return path


def run(src: Path, out: Path, **option_overrides) -> pa.RunResult:
    meta = pa.PackMeta(pack_id="test", title_en="Test", title_ru="Тест", author="Test Author", license="CC-BY-4.0")
    options = pa.Options(**option_overrides)
    return pa.prepare(src, out, meta, options, quiet=True)


def wrap_jump(clip: np.ndarray) -> tuple[float, float]:
    """Скачок на стыке петли и самый большой скачок внутри клипа."""
    mono = clip if clip.ndim == 1 else clip.mean(axis=1)
    return abs(mono[0] - mono[-1]), float(np.max(np.abs(np.diff(mono))))


class PrepareAudioTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.src = self.root / "src"
        self.out = self.root / "pack"

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_kick_at_120_loops_four_bars_and_skips_silence(self) -> None:
        audio = np.concatenate([silence(20), pattern(kick_hit(), 120, 1, 32), silence(20)])
        write(self.src / "kick groove.wav", audio)

        # Темп отдельно — на конверте всего файла.
        group = pa.group_sessions(pa.discover(self.src, pa.Options()).sources)[0]
        scan = pa.scan_group(group)
        tempo = pa.detect_tempo(scan.flux, scan.fps, pa.Options(), scan.low_flux)
        self.assertTrue(tempo.detected, tempo.note)
        self.assertAlmostEqual(tempo.bpm, 120.0, delta=1.0)

        result = run(self.src, self.out)
        self.assertTrue(result.clips)
        four_bars = round(4 * 4 * 60 / 120 * RATE)
        for entry in result.clips:
            tags, source = entry["tags"], entry["source"]
            self.assertEqual(tags["loop"], "bars")
            self.assertEqual(tags["bars"], 4)
            self.assertAlmostEqual(tags["bpm"], 120.0, delta=1.0)
            self.assertEqual(tags["instrument"], "kick")

            clip, rate = sf.read(str(self.out / entry["file"]))
            self.assertEqual(rate, RATE)
            self.assertLessEqual(abs(clip.shape[0] - four_bars), 1)

            # Тишина пропущена: клип целиком внутри звучащей части.
            start = source["startSeconds"]
            self.assertGreaterEqual(start, 20.0 - 0.05)
            self.assertLessEqual(start + clip.shape[0] / RATE, 52.0 + 0.05)

            jump, largest = wrap_jump(clip)
            self.assertLess(jump, 1e-3)
            self.assertLessEqual(jump, largest)

        report = result.reports[0]
        self.assertGreater(report.silence_percent, 45)
        self.assertTrue((self.out / "pack.json").exists())
        self.assertTrue((self.out / "report.md").exists())

    def test_sine_pad_without_pulse_is_a_free_loop(self) -> None:
        t = np.arange(int(40 * RATE)) / RATE
        pad = sum(np.sin(2 * np.pi * f * t) for f in (220.0, 277.2, 329.6)) * 0.15
        pad *= np.minimum(1.0, t / 2.0) * (1 + 0.1 * np.sin(2 * np.pi * 0.2 * t))
        write(self.src / "Pad take.wav", np.stack([pad, pad * 0.9], axis=1))

        result = run(self.src, self.out)
        self.assertTrue(result.clips)
        for entry in result.clips:
            self.assertEqual(entry["tags"]["loop"], "free")
            self.assertIsNone(entry["tags"]["bpm"])
            clip, _ = sf.read(str(self.out / entry["file"]))
            self.assertTrue(8.0 <= clip.shape[0] / RATE <= 10.0 + 1e-6)
            jump, largest = wrap_jump(clip)
            self.assertLessEqual(jump, 1.5 * largest)

    def test_hihat_is_recognised_by_sound_when_name_says_nothing(self) -> None:
        audio = np.concatenate([silence(3), pattern(hat_hit(), 120, 2, 30), silence(3)])
        write(self.src / "Файл 3.wav", audio)

        result = run(self.src, self.out)
        label = result.reports[0].label
        self.assertIsNotNone(label)
        self.assertEqual(label.category, "hihat", label.reason)
        self.assertEqual(label.source, "audio")
        self.assertGreaterEqual(label.confidence, 0.6)
        self.assertTrue(all(e["file"].startswith("hihat/") for e in result.clips))

    def test_sample_library_file_is_flagged(self) -> None:
        name = "Leorint 11-Cymatics - Joker Drum Loop.wav"
        self.assertIsNotNone(pa.third_party_reason(name))
        self.assertIsNone(pa.third_party_reason("Vital lead take 2.wav"))   # синтезатор — игра автора
        self.assertIsNone(pa.third_party_reason("vanya.wav"))

        write(self.src / name, pattern(kick_hit(), 120, 1, 20))
        result = run(self.src, self.out)
        self.assertEqual([f for f, _ in result.found.flagged], [name])
        self.assertFalse(result.clips)
        self.assertIn("Cymatics", result.report_text)

        included = run(self.src, self.root / "pack2", include_flagged=True)
        self.assertEqual([f for f, _ in included.found.flagged_included], [name])
        self.assertTrue(included.clips)

    def test_exclude_pattern(self) -> None:
        write(self.src / "take" / "scratch vox.wav", pattern(kick_hit(), 120, 1, 12))
        result = run(self.src, self.out, excludes=("scratch",))
        self.assertEqual(result.found.excluded, [("take/scratch vox.wav", "scratch")])

    def test_ambiguous_signal_goes_to_other(self) -> None:
        n = int(30 * RATE)
        noise = RNG.standard_normal(n)
        spectrum = np.fft.rfft(noise)
        freqs = np.fft.rfftfreq(n, 1 / RATE)
        spectrum[(freqs < 300) | (freqs > 3000)] = 0
        band = np.fft.irfft(spectrum, n)
        band /= np.std(band)
        t = np.arange(n) / RATE
        audio = 0.1 * band * (1 + 0.3 * np.sin(2 * np.pi * 0.37 * t)) + 0.05 * np.sin(2 * np.pi * 700 * t)
        write(self.src / "Audio 3.wav", audio)

        result = run(self.src, self.out)
        label = result.reports[0].label
        self.assertEqual(label.category, "other", label.reason)
        self.assertTrue(label.best_guess)
        self.assertTrue(all(e["file"].startswith("other/") for e in result.clips))

    def test_three_stem_session_is_cut_at_identical_ranges(self) -> None:
        seconds = 60
        pad = lambda x: np.concatenate([silence(10), x, silence(10)])  # noqa: E731
        write(self.src / "song" / "Kick.wav", pad(pattern(kick_hit(), 120, 1, seconds)))
        write(self.src / "song" / "HH.wav", pad(pattern(hat_hit(), 120, 2, seconds)))
        write(self.src / "song" / "Bass.wav", pad(bass_line(120, seconds)))

        result = run(self.src, self.out)
        self.assertEqual({r.session for r in result.reports}, {"song"})
        self.assertEqual({r.role for r in result.reports}, {"stem"})
        self.assertEqual({r.label.category for r in result.reports}, {"kick", "hihat", "bass"})

        ranges: dict[str, set] = {}
        for entry in result.clips:
            key = Path(entry["source"]["file"]).stem
            ranges.setdefault(key, set()).add((entry["source"]["startSeconds"], entry["seconds"]))
            self.assertEqual(entry["tags"]["session"], "song")
            self.assertEqual(entry["tags"]["role"], "stem")
        self.assertEqual(set(ranges), {"Kick", "HH", "Bass"})
        self.assertGreaterEqual(len(ranges["Kick"]), 2)
        self.assertEqual(ranges["Kick"], ranges["HH"])
        self.assertEqual(ranges["Kick"], ranges["Bass"])

    def test_keywords(self) -> None:
        cases = {
            "drums/Kick In.wav": "kick", "OH L.wav": "cymbals", "Tom2.wav": "toms", "бас-гитара.wav": "bass",
            "Вокал Ваня.wav": "vocal", "Snare Top.wav": "snare", "Bass Drum.wav": "kick", "дудка.wav": "wind",
            "ANA lead.wav": "keys", "gtr L.wav": "guitar", "Full Mix v3.wav": "mix", "mix/song.wav": "mix",
        }
        for path, expected in cases.items():
            hit = pa.keyword_category(path)
            self.assertIsNotNone(hit, path)
            self.assertEqual(hit.category, expected, path)
        for path in ("vanya.wav", "Файл 3.wav", "Audio 3.wav", "Leorint 11.wav"):
            self.assertIsNone(pa.keyword_category(path), path)

    def test_pack_manifest_matches_app_format(self) -> None:
        write(self.src / "kick.wav", np.concatenate([silence(2), pattern(kick_hit(), 120, 1, 24)]))
        run(self.src, self.out)
        manifest = json.loads((self.out / "pack.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["abcTrainPack"], 1)
        self.assertEqual(manifest["title"], {"en": "Test", "ru": "Тест"})
        clip = manifest["clips"][0]
        self.assertTrue((self.out / clip["file"]).is_file())
        self.assertIn(clip["source"]["license"], pa.ALLOWED_LICENSES)
        self.assertEqual(clip["source"]["author"], "Test Author")
        for key in ("genre", "content", "instruments", "character"):
            self.assertIn(key, clip["tags"])
        self.assertEqual(sf.info(str(self.out / clip["file"])).subtype, "PCM_16")
        # Громкость как у build_pack: -18 LUFS, если пик не упёрся в -1 dBFS.
        self.assertLessEqual(clip["loudnessLufs"], pa.TARGET_LUFS + 0.5)


if __name__ == "__main__":
    unittest.main()
