#!/usr/bin/env python3
#
# Rebuilds assets/fonts/ from Google Fonts upstream.
#
#     python3 -m venv .venv && .venv/bin/pip install fonttools brotli pillow
#     .venv/bin/python tools/build_fonts.py            # downloads what it needs
#
# See assets/fonts/README.md for why there are two families in here and
# why each is cut down to one script.
#
# The interesting part is the *matching*. Barlow has no Cyrillic, so the
# Cyrillic is carried by a companion - and a companion picked by eye is a
# companion that looks almost right in the one word you tested it on. Each
# Barlow cut is measured (the width-to-cap-height ratio of H/O/N, and the
# ink coverage of the same three letters, which is a good proxy for stem
# weight) and the companion is pinned to whichever point on its own
# variable axes lands closest.
#
# That search produced two different companions, and that is not an
# inconsistency: Barlow Condensed and Barlow are already two different
# designs, so matching each of them separately is the consistent thing to
# do. Noto Sans reaches Barlow and Barlow SemiCondensed almost exactly.
# It cannot get narrow enough for Barlow Condensed - its narrowest is 15%
# too wide - where Oswald lands within 3% on width and 1% on weight.
#
# Vertical metrics are then forced to match Barlow's exactly (ascent = 1
# em, descent = 0.2 em, cap height = 0.7 em). Without that step the
# companion draws 4% larger at the same requested height, because JUCE
# scales a typeface by its own ascent+descent and the two families do not
# agree about those.

import io
import os
import sys
import urllib.request

from fontTools.ttLib import TTFont
from fontTools.ttLib.scaleUpem import scale_upem
from fontTools.varLib import instancer
from fontTools.subset import Subsetter, Options

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
OUT = os.path.join(REPO, "assets", "fonts")
CACHE = os.path.join(HERE, ".fontcache")

GF = "https://raw.githubusercontent.com/google/fonts/main"

SOURCES = {
    "Barlow-Regular.ttf":              "ofl/barlow/Barlow-Regular.ttf",
    "Barlow-Medium.ttf":               "ofl/barlow/Barlow-Medium.ttf",
    "BarlowSemiCondensed-Regular.ttf": "ofl/barlowsemicondensed/BarlowSemiCondensed-Regular.ttf",
    "BarlowSemiCondensed-Medium.ttf":  "ofl/barlowsemicondensed/BarlowSemiCondensed-Medium.ttf",
    "BarlowCondensed-SemiBold.ttf":    "ofl/barlowcondensed/BarlowCondensed-SemiBold.ttf",
    "BarlowCondensed-Bold.ttf":        "ofl/barlowcondensed/BarlowCondensed-Bold.ttf",
    "NotoSansVF.ttf":                  "ofl/notosans/NotoSans%5Bwdth%2Cwght%5D.ttf",
    "OswaldVF.ttf":                    "ofl/oswald/Oswald%5Bwght%5D.ttf",
    "OFL-Barlow.txt":                  "ofl/barlow/OFL.txt",
    "OFL-NotoSans.txt":                "ofl/notosans/OFL.txt",
    "OFL-Oswald.txt":                  "ofl/oswald/OFL.txt",
}

# Latin the app can actually show: ASCII, Latin-1 and Latin Extended-A
# (Polish, Czech, Turkish), plus the punctuation, currency and arrows the
# interface draws.
LATIN = [(0x0000, 0x00FF), (0x0100, 0x024F), (0x0259, 0x0259),
         (0x02BB, 0x02BC), (0x02C6, 0x02C6), (0x02DA, 0x02DA), (0x02DC, 0x02DC),
         (0x2000, 0x206F), (0x20A0, 0x20BF), (0x2122, 0x2122),
         (0x2190, 0x2193), (0x2212, 0x2212), (0x2215, 0x2215), (0x2713, 0x2713),
         (0xFEFF, 0xFEFF), (0xFFFD, 0xFFFD)]
# Deliberately no combining marks (U+0300-036F): every accented letter this
# app can show is a precomposed codepoint in Latin Extended-A, and keeping
# the marks drags in the whole GPOS mark-attachment machinery for glyphs
# nothing ever asks for - 28 KB per face, which is most of the difference
# between a 350 KB font payload and a 700 KB one.

# Cyrillic including the Ukrainian letters (ґ є і ї) the uk table needs.
CYRILLIC = [(0x0020, 0x0020), (0x00A0, 0x00A0),
            (0x0400, 0x052F), (0x2116, 0x2116), (0x20B4, 0x20B4)]

# Barlow's own vertical proportions, which every companion is forced onto.
UPM = 1000
ASCENT = 1000
DESCENT = -200
CAP_HEIGHT = 700

# The result of the measured search described above. Each entry is
# (family, style, source, axes) - see match_companions() for how these
# numbers were arrived at, and how to check them again.
COMPANIONS = [
    ("abcTrain Cyr",               "Regular",  "NotoSansVF.ttf", {"wdth": 80.0,  "wght": 350}),
    ("abcTrain Cyr",               "Medium",   "NotoSansVF.ttf", {"wdth": 80.0,  "wght": 450}),
    ("abcTrain Cyr SemiCondensed", "Regular",  "NotoSansVF.ttf", {"wdth": 70.0,  "wght": 350}),
    ("abcTrain Cyr SemiCondensed", "Medium",   "NotoSansVF.ttf", {"wdth": 62.5,  "wght": 500}),
    ("abcTrain Cyr Condensed",     "SemiBold", "OswaldVF.ttf",   {"wght": 500}),
    ("abcTrain Cyr Condensed",     "Bold",     "OswaldVF.ttf",   {"wght": 600}),
]

LATIN_FACES = [
    ("Barlow",               "Regular",  "Barlow-Regular.ttf"),
    ("Barlow",               "Medium",   "Barlow-Medium.ttf"),
    ("Barlow SemiCondensed", "Regular",  "BarlowSemiCondensed-Regular.ttf"),
    ("Barlow SemiCondensed", "Medium",   "BarlowSemiCondensed-Medium.ttf"),
    ("Barlow Condensed",     "SemiBold", "BarlowCondensed-SemiBold.ttf"),
    ("Barlow Condensed",     "Bold",     "BarlowCondensed-Bold.ttf"),
]


def fetch(name):
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, name)

    if not os.path.exists(path):
        url = GF + "/" + SOURCES[name]
        print("  fetching", name)
        with urllib.request.urlopen(url, timeout=120) as r, open(path, "wb") as f:
            f.write(r.read())

    return path


def rename(font, family, style):
    full = family if style == "Regular" else family + " " + style
    postscript = full.replace(" ", "")
    name = font["name"]

    for record in list(name.names):
        name.removeNames(record.nameID, record.platformID, record.platEncID, record.langID)

    for pid, eid, lid in ((3, 1, 0x409), (1, 0, 0)):
        name.setName(family, 1, pid, eid, lid)
        # Only Regular and Bold are legal legacy subfamilies; the real one
        # goes in nameID 17, which is what a modern shaper reads.
        name.setName(style if style in ("Regular", "Bold") else "Regular", 2, pid, eid, lid)
        name.setName(full + ":abcTrain", 3, pid, eid, lid)
        name.setName(full, 4, pid, eid, lid)
        name.setName("1.000", 5, pid, eid, lid)
        name.setName(postscript, 6, pid, eid, lid)
        name.setName(family, 16, pid, eid, lid)
        name.setName(style, 17, pid, eid, lid)


def normalise_metrics(font):
    """Force Barlow's own vertical proportions onto a companion.

    JUCE scales a typeface by its own ascent + descent, so two families
    that disagree about those draw at different sizes for one requested
    height. Barlow is 1.0 em up and 0.2 em down with a 0.7 em cap; every
    companion is scaled and relabelled to match, which is what makes a
    Russian word and an English one in the same line the same size.
    """
    scale_upem(font, UPM)

    cap = font["OS/2"].sCapHeight
    if cap and cap != CAP_HEIGHT:
        # scale_upem is the only uniform transform fontTools offers, so the
        # scaling is done by asking for the em that makes the cap come out
        # right and then relabelling the em back to 1000. The coordinates
        # are what actually moved; unitsPerEm is only how they are read.
        scale_upem(font, max(1, round(UPM * CAP_HEIGHT / cap)))
        font["head"].unitsPerEm = UPM

    os2 = font["OS/2"]
    hhea = font["hhea"]

    hhea.ascent, hhea.descent, hhea.lineGap = ASCENT, DESCENT, 0
    os2.sTypoAscender, os2.sTypoDescender, os2.sTypoLineGap = ASCENT, DESCENT, 0
    os2.usWinAscent, os2.usWinDescent = ASCENT, -DESCENT
    os2.sCapHeight = CAP_HEIGHT
    os2.fsSelection |= 1 << 7          # USE_TYPO_METRICS


def cut(font, ranges, family, style, out_path):
    options = Options()
    options.layout_features = ["*"]
    options.name_IDs = ["*"]
    options.name_legacy = True
    options.notdef_outline = True
    options.recalc_bounds = True
    options.drop_tables += ["DSIG"]

    subsetter = Subsetter(options=options)
    subsetter.populate(unicodes=[c for lo, hi in ranges for c in range(lo, hi + 1)])
    subsetter.subset(font)

    rename(font, family, style)
    font.save(out_path)
    return os.path.getsize(out_path)


def main():
    os.makedirs(OUT, exist_ok=True)
    total = 0

    print("Latin (Barlow):")
    for family, style, source in LATIN_FACES:
        font = TTFont(fetch(source))
        path = os.path.join(OUT, family.replace(" ", "") + "-" + style + ".ttf")
        size = cut(font, LATIN, family, style, path)
        total += size
        print("  %-44s %7.1f KB" % (os.path.basename(path), size / 1024))

    print("Cyrillic companion (matched to each Barlow cut):")
    for family, style, source, axes in COMPANIONS:
        font = instancer.instantiateVariableFont(TTFont(fetch(source)), axes, inplace=True)
        normalise_metrics(font)
        path = os.path.join(OUT, family.replace(" ", "") + "-" + style + ".ttf")
        size = cut(font, CYRILLIC, family, style, path)
        total += size
        print("  %-44s %7.1f KB  (%s %s)" % (os.path.basename(path), size / 1024,
                                              source.replace("VF.ttf", ""),
                                              " ".join("%s=%g" % kv for kv in axes.items())))

    for licence in ("OFL-Barlow.txt", "OFL-NotoSans.txt", "OFL-Oswald.txt"):
        with open(fetch(licence), "rb") as src, open(os.path.join(OUT, licence), "wb") as dst:
            dst.write(src.read())

    print("  %-44s %7.1f KB" % ("TOTAL", total / 1024))


if __name__ == "__main__":
    sys.exit(main())
