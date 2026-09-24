#!/usr/bin/env python3
"""Train the bot listeners (ADR 046) - one button, reproducible.

    python3 tools/bots/train_bots.py            # literature teacher -> Source/BotWeights.h
    python3 tools/bots/train_bots.py --check    # fit only, print the table, write nothing
    python3 tools/bots/train_bots.py --answers rounds.csv --name twin
                                                # fit a listener to real answers instead

What a bot is
-------------
Each bot is a one-neuron perceptron per exercise with a logistic output - the
psychometric function of psychoacoustics:

    P(right | exercise g, bucket b, level L)
        = guess_g + (1 - guess_g - lapse) * sigmoid(slope_g * (threshold[g][b] - L))

Inputs: which exercise, which part of it the round fell in (the exercise's own
"skill bucket": the frequency range for Guess the Band, the pan zone, the delay
class, the reverb type...), and the level. Weights: a threshold per bucket, a
slope per exercise, one lapse rate. So the Cat is sure of a boost at 8 kHz and
guesses at 35 Hz, on the same level - which the first version (one threshold
per exercise) could not do.

Where the training data comes from - honestly
---------------------------------------------
There are no recorded animal answers to EQ rounds, and no player answers on a
server yet. So the teacher is a *listener model built from published animal
hearing data* (docs/research/2026-09-bot-hearing.md, sources per number):
audiograms, minimum audible angle, what is known about temporal resolution.
Where the literature has no number (level JND and gap detection for most of
these animals) the value is a labelled game choice - see INVENTED below.

The teacher's relative strengths are the biology; its overall strength is set
per bot so it is a fair opponent (a real bat would lose every EQ round in the
human range - its best hearing is at 20 kHz and above). The student perceptron
is then trained by maximum likelihood on simulated rounds from the teacher. On
teacher data the student mostly recovers the teacher - that is the check that
the model and the optimiser work. The same code fits real answers (--answers):
game,bucket,level,correct per line - which is how a "twin" of a real player, or
a bot fitted to pilot data, is made later without touching the app.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_HEADER = os.path.join(ROOT, "Source", "BotWeights.h")
OUT_JSON = os.path.join(ROOT, "tools", "bots", "bot_weights.json")

# ---- the exercises, in GameManager order (BotListener.h) --------------------
GAMES = ["band", "comp", "verb", "pan", "delay", "dist", "width", "gain", "range"]
BUCKETS = {   # the games' getSkillBucketLabel order
    "band":  ["Sub-bass", "Bass", "Low-mids", "Mids", "High-mids", "Presence", "Air"],
    "comp":  ["Weak", "Medium", "Strong"],
    "verb":  ["Room", "Chamber", "Hall", "Plate", "Spring"],
    "pan":   ["Hard L", "Left", "Centre", "Right", "Hard R"],
    "delay": ["Slapback", "Short", "Medium", "Long"],
    "dist":  ["Soft Clipping", "Hard Clipping", "Tape Saturation", "Overdrive"],
    "width": ["Narrow", "Normal", "Wide", "Extra Wide"],
    "gain":  ["Big cut", "Small cut", "Small boost", "Big boost"],
    "range": ["Sub-bass", "Bass", "Low-mids", "Mids", "High-mids", "Presence", "Air"],
}
MAX_BUCKETS = 7
CONTINUOUS = {"band", "pan", "delay", "gain"}   # a ruler: the app passes 5 choices
def guess_of(game: str) -> float:
    return 1.0 / (5 if game in CONTINUOUS else 2)

# Centre of each frequency bucket (FrequencyRangeGame::ranges, geometric mean).
BAND_CENTRES_HZ = [35, 122, 354, 1000, 2830, 4900, 10950]

# ---- the literature ---------------------------------------------------------
# Audiogram: threshold above the animal's own best threshold, in dB, at the
# bucket centres - read off the published curves (+-10 dB; see the research
# note for each source). 70 = at or beyond the edge of hearing.
#
# MAA: minimum audible angle for broadband sound, degrees (human 1).
# temporal: fine timing (echo delay, ITD, transients) relative to a human,
#           in octaves of acuity. Partly INVENTED - see the flag.
LIT = {
    "hound": dict(   # dog: 63 Hz-47 kHz at 60 dB, best 8 kHz / 4 dB; MAA 7.6 deg (Heffner 1983; Guerineau 2024; MDPI 2022)
        audiogram=[62, 38, 20, 10, 4, 1, 0], maa=7.6, temporal=0.0),
    "cat": dict(     # cat: 48 Hz-85 kHz at 70 dB, good 0.5-32 kHz; MAA 5 deg (Heffner & Heffner 1985)
        audiogram=[70, 40, 20, 10, 5, 2, 0], maa=5.0, temporal=0.2),
    "viper": dict(   # royal python: best 80-160 Hz, ~78 dB SPL, head vibration; upper limit ~1 kHz (Christensen 2012)
        audiogram=[10, 0, 15, 45, 70, 70, 70], maa=None, temporal=-1.5),
    "owl": dict(     # barn owl: best 4-8 kHz, ~0 dB; head-turn accuracy 1-2 deg (Knudsen & Konishi 1979); ITD us-scale
        audiogram=[70, 60, 40, 20, 5, 0, 15], maa=1.5, temporal=0.5),
    "bat": dict(     # big brown bat: 0.85 kHz at 106 dB .. best 20 kHz 7 dB; MAA 14 deg (Koay 1997, 1998); echo delay (Simmons)
        audiogram=[70, 70, 70, 70, 55, 45, 20], maa=14.0, temporal=1.0),
    "elephant": dict(  # Indian elephant: 16 Hz at 65 dB .. 10.5 kHz at 60 dB, best 1 kHz 8 dB; MAA ~1 deg (Heffner & Heffner 1980, 1982)
        audiogram=[42, 15, 3, 0, 8, 20, 55], maa=1.0, temporal=-0.7),
}
INVENTED = {
    "viper.maa": "no localisation data for snakes; set to the worst of the six (16 deg)",
    "*.temporal": "gap detection / level JND not found for these species; bat and owl above human "
                  "(echo-delay and ITD literature), elephant and python below - a game choice",
    "*.level": "level JND not found; derived from mid-band sensitivity instead",
    "bucket adjustments": "same for every bot: cuts harder than boosts, weak compression harder "
                          "than strong, lateral pan harder than centre (human MAA grows off-axis)",
}

# How strong each bot is overall (mean threshold, in levels) and the rest of
# its character. Strength is a game choice - the shape comes from LIT.
BOTS = {  # id: mean threshold, slope, lapse, reaction ms, Decibelo
    "hound":    (6.1, 1.1, 0.05, 1800, 1500),
    "cat":      (6.0, 1.3, 0.08, 1200, 1560),
    "viper":    (5.2, 1.0, 0.10, 1500, 1420),
    "owl":      (5.8, 1.2, 0.06, 2200, 1540),
    "bat":      (5.7, 1.4, 0.10,  900, 1530),
    "elephant": (5.9, 0.9, 0.03, 3000, 1580),
}
SPREAD = 1.3            # how far strengths and weaknesses sit from the mean, in levels
CLIP = (2.5, 9.5)


def raw_scores(lit: dict) -> dict[str, list[float]]:
    """Higher = hears it better. Units are rough octaves/20 dB steps; only the
    shape matters, the scale is normalised per bot afterwards."""
    freq = [-d / 20.0 for d in lit["audiogram"]]
    maa = lit["maa"] if lit["maa"] is not None else 16.0
    loc = -math.log2(maa)
    temporal = lit["temporal"]
    mid = float(np.mean(freq[2:5]))       # 350 Hz - 2.8 kHz: where level and dynamics are judged
    high = float(np.mean(freq[4:7]))      # 2.8-11 kHz: where clipping harmonics land
    lowmid = float(np.mean(freq[1:4]))    # 120 Hz-1 kHz: most of a reverb tail's energy

    return {
        "band":  freq,
        "range": freq,
        "pan":   [loc + a for a in (-0.5, 0.0, 0.3, 0.0, -0.5)],
        "width": [0.8 * loc + a for a in (-0.2, -0.1, 0.1, 0.2)],
        "delay": [temporal * w + 0.3 * mid for w in (1.0, 0.7, 0.4, 0.2)],
        "gain":  [mid + a for a in (0.1, -0.4, -0.2, 0.3)],
        "comp":  [0.6 * temporal + 0.4 * mid + a for a in (-0.5, 0.0, 0.5)],
        "dist":  [0.7 * high + 0.3 * mid + a for a in (-0.4, 0.5, -0.3, 0.2)],
        "verb":  [0.5 * loc + 0.5 * lowmid + a for a in (-0.2, -0.2, 0.3, 0.0, 0.2)],
    }


def teacher_thresholds(bot: str) -> dict[str, np.ndarray]:
    mean_t = BOTS[bot][0]
    scores = raw_scores(LIT[bot])
    flat = np.concatenate([np.array(v) for v in scores.values()])
    mu, sd = flat.mean(), flat.std() + 1e-9
    return {g: np.clip(mean_t + SPREAD * (np.array(v) - mu) / sd, *CLIP) for g, v in scores.items()}


# ---- the perceptron and its training -----------------------------------------
def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def simulate(thr: dict, slope: float, lapse: float, n: int, rng) -> list[tuple]:
    """Rounds as the app would play them: a random exercise and bucket, a
    level drawn around where the bot is (a staircase spends its time near
    threshold), the answer drawn from the teacher."""
    rows = []
    for _ in range(n):
        gi = int(rng.integers(len(GAMES)))
        g = GAMES[gi]
        b = int(rng.integers(len(BUCKETS[g])))
        level = int(np.clip(np.round(rng.normal(thr[g][b], 2.2)), 1, 10))
        p = guess_of(g) + (1 - guess_of(g) - lapse) * sigmoid(slope * (thr[g][b] - level))
        rows.append((gi, b, level, int(rng.random() < p)))
    return rows


def fit(rows, steps=3000, lr=0.05, seed=0):
    """Maximum likelihood by Adam. Parameters: threshold per (game, bucket),
    log-slope per game, logit-lapse shared. Returns thresholds, slopes, lapse,
    and the final mean negative log-likelihood."""
    rows = np.array(rows, dtype=float)
    gi, bi, lv, y = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2], rows[:, 3]
    guess = np.array([guess_of(GAMES[k]) for k in gi])

    thr = np.full((len(GAMES), MAX_BUCKETS), 5.5)
    log_slope = np.zeros(len(GAMES))
    lapse_logit = np.array(-3.0)
    params = [thr, log_slope, lapse_logit]
    m = [np.zeros_like(p) for p in params]
    v = [np.zeros_like(p) for p in params]

    for step in range(1, steps + 1):
        slope = np.exp(log_slope)[gi]
        lapse = sigmoid(lapse_logit) * 0.2          # lapse lives in 0..0.2
        s = sigmoid(slope * (thr[gi, bi] - lv))
        p = np.clip(guess + (1 - guess - lapse) * s, 1e-6, 1 - 1e-6)
        dl_dp = (p - y) / (p * (1 - p)) / len(y)    # d(-mean log-lik)/dp
        ds = dl_dp * (1 - guess - lapse) * s * (1 - s)

        g_thr = np.zeros_like(thr)
        np.add.at(g_thr, (gi, bi), ds * slope)
        g_ls = np.zeros_like(log_slope)
        np.add.at(g_ls, gi, ds * (thr[gi, bi] - lv) * slope)
        g_lapse = np.sum(dl_dp * -s) * 0.2 * sigmoid(lapse_logit) * (1 - sigmoid(lapse_logit))
        grads = [g_thr, g_ls, np.array(g_lapse)]

        for k, (prm, gr) in enumerate(zip(params, grads)):
            m[k] = 0.9 * m[k] + 0.1 * gr
            v[k] = 0.999 * v[k] + 0.001 * gr * gr
            prm -= lr * (m[k] / (1 - 0.9 ** step)) / (np.sqrt(v[k] / (1 - 0.999 ** step)) + 1e-8)

    slope = np.exp(log_slope)
    lapse = float(sigmoid(lapse_logit) * 0.2)
    s = sigmoid(slope[gi] * (thr[gi, bi] - lv))
    p = np.clip(guess + (1 - guess - lapse) * s, 1e-6, 1 - 1e-6)
    nll = float(-np.mean(y * np.log(p) + (1 - y) * np.log(1 - p)))
    return np.clip(thr, *CLIP), slope, lapse, nll


# ---- output --------------------------------------------------------------------
def write_header(results: dict):
    lines = [
        "#pragma once",
        "",
        "// GENERATED by tools/bots/train_bots.py - do not edit by hand; run the script.",
        "// The bots' perceptron weights (ADR 046): per bot, per exercise, a threshold",
        "// per skill bucket (the level where the bot is half-way between guessing and",
        "// sure), a slope per exercise, one lapse rate. Sources for the teacher the",
        "// weights were trained on: docs/research/2026-09-bot-hearing.md.",
        "",
        "namespace BotWeights",
        "{",
        f"    constexpr int maxBuckets = {MAX_BUCKETS};",
        f"    constexpr int bucketsOf[{len(GAMES)}] {{ " + ", ".join(str(len(BUCKETS[g])) for g in GAMES) + " };",
        "",
        "    struct Weights",
        "    {",
        f"        float threshold[{len(GAMES)}][{MAX_BUCKETS}];",
        f"        float slope[{len(GAMES)}];",
        "        float lapse;",
        "    };",
        "",
        "    // hound, cat, viper, owl, bat, elephant (BotListener::Bot order)",
        f"    constexpr Weights weights[{len(BOTS)}] {{",
    ]
    for bot in BOTS:
        thr, slope, lapse = results[bot]["threshold"], results[bot]["slope"], results[bot]["lapse"]
        lines.append(f"        {{ // {bot}")
        lines.append("            {")
        for gi, g in enumerate(GAMES):
            vals = [thr[gi][b] if b < len(BUCKETS[g]) else 0.0 for b in range(MAX_BUCKETS)]
            lines.append("                { " + ", ".join(f"{x:.2f}f" for x in vals) + f" }},   // {g}")
        lines.append("            },")
        lines.append("            { " + ", ".join(f"{x:.3f}f" for x in slope) + " },")
        lines.append(f"            {lapse:.4f}f")
        lines.append("        },")
    lines += ["    };", "}", ""]
    with open(OUT_HEADER, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def game_means(thr_row, g):
    return float(np.mean(thr_row[: len(BUCKETS[g])]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="fit and print, write nothing")
    ap.add_argument("--answers", help="CSV game,bucket,level,correct: fit one listener to real answers")
    ap.add_argument("--name", default="twin")
    ap.add_argument("--rounds", type=int, default=40000, help="simulated rounds per bot")
    args = ap.parse_args()

    if args.answers:
        rows = []
        with open(args.answers, newline="") as f:
            for r in csv.DictReader(f):
                g = r["game"] if not r["game"].isdigit() else GAMES[int(r["game"])]
                rows.append((GAMES.index(g), int(r["bucket"]), int(r["level"]), int(r["correct"])))
        thr, slope, lapse, nll = fit(rows)
        print(f"{args.name}: {len(rows)} rounds, lapse {lapse:.3f}, nll {nll:.3f}")
        for gi, g in enumerate(GAMES):
            print(f"  {g:6s} slope {slope[gi]:.2f}  " + " ".join(f"{x:4.1f}" for x in thr[gi][: len(BUCKETS[g])]))
        return

    rng = np.random.default_rng(46)
    results, report = {}, []
    for bot, (mean_t, slope_t, lapse_t, *_rest) in BOTS.items():
        teacher = teacher_thresholds(bot)
        rows = simulate(teacher, slope_t, lapse_t, args.rounds, rng)
        thr, slope, lapse, nll = fit(rows)
        err = np.mean([abs(thr[gi][b] - teacher[g][b]) for gi, g in enumerate(GAMES) for b in range(len(BUCKETS[g]))])
        results[bot] = {"threshold": thr.tolist(), "slope": slope.tolist(), "lapse": lapse,
                        "teacher": {g: teacher[g].tolist() for g in GAMES}, "nll": nll, "mean_abs_error": float(err)}
        report.append((bot, err, lapse, nll))

    print("bot       |err| thr  lapse   nll   " + "  ".join(f"{g:>5s}" for g in GAMES))
    for bot, err, lapse, nll in report:
        thr = np.array(results[bot]["threshold"])
        means = [game_means(thr[gi], g) for gi, g in enumerate(GAMES)]
        print(f"{bot:9s} {err:5.2f}   {lapse:.3f}  {nll:.3f}  " + "  ".join(f"{m:5.1f}" for m in means))
    print("\nband by bucket (" + ", ".join(BUCKETS['band']) + "):")
    for bot in BOTS:
        print(f"  {bot:9s} " + " ".join(f"{x:4.1f}" for x in results[bot]["threshold"][0][:7]))

    if args.check:
        return
    write_header(results)
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump({"games": GAMES, "buckets": BUCKETS, "bots": results, "invented": INVENTED,
                   "literature": LIT, "settings": {"spread": SPREAD, "clip": CLIP, "rounds": args.rounds}},
                  f, indent=1)
    print(f"\nwrote {os.path.relpath(OUT_HEADER, ROOT)} and {os.path.relpath(OUT_JSON, ROOT)}")


if __name__ == "__main__":
    sys.exit(main())
