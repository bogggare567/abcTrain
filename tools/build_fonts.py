#!/usr/bin/env python3
#
# Rebuilds assets/fonts/ from Google Fonts upstream. Needs fonttools:
#
#     python3 -m venv .venv && .venv/bin/pip install fonttools brotli
#     curl the six Barlow TTFs and Roboto[wdth,wght].ttf next to this file
#     .venv/bin/python tools/build_fonts.py
#
# See assets/fonts/README.md for why the two families are split by script.

import os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools.subset import Subsetter, Options

OUT = "/Users/bogdankorablev/Desktop/work/abcTrain/repo/assets/fonts"
os.makedirs(OUT, exist_ok=True)

# Latin the app can actually show: ASCII + latin-1 + latin-ext (Polish, Czech,
# Turkish...) + the punctuation, currency and arrows the UI draws.
LATIN = ("U+0000-00FF,U+0100-024F,U+0259,U+02BB-02BC,U+02C6,U+02DA,U+02DC,"
         "U+0300-036F,U+2000-206F,U+2070-209F,U+20A0-20BF,U+2122,U+2190-21BB,"
         "U+2212,U+2215,U+2500-257F,U+25A0-25FF,U+2713,U+FEFF,U+FFFD")
# Cyrillic including the Ukrainian letters (ґ є і ї) the uk table needs.
CYR = "U+0400-052F,U+2116,U+20B4,U+0020,U+00A0"

def rename(f, family, style):
    full = family if style in ("Regular",) else f"{family} {style}"
    ps = full.replace(" ", "")
    name = f['name']
    for rec in list(name.names):
        name.removeNames(rec.nameID, rec.platformID, rec.platEncID, rec.langID)
    for pid, eid, lid in ((3, 1, 0x409), (1, 0, 0)):
        name.setName(family, 1, pid, eid, lid)
        name.setName(style if style in ("Regular", "Bold") else "Regular", 2, pid, eid, lid)
        name.setName(f"{full}:abcTrain", 3, pid, eid, lid)
        name.setName(full, 4, pid, eid, lid)
        name.setName("1.000", 5, pid, eid, lid)
        name.setName(ps, 6, pid, eid, lid)
        name.setName(family, 16, pid, eid, lid)
        name.setName(style, 17, pid, eid, lid)

def cut(font, unicodes, family, style, out):
    opts = Options()
    opts.layout_features = ['*']
    opts.name_IDs = ['*']
    opts.name_legacy = True
    opts.notdef_outline = True
    opts.recalc_bounds = True
    opts.drop_tables += ['DSIG']
    s = Subsetter(options=opts)
    s.populate(unicodes=[c for r in unicodes.split(',')
                         for c in (range(int(r.split('-')[0][2:], 16),
                                         int(r.split('-')[-1][2:], 16) + 1))])
    s.subset(font)
    rename(font, family, style)
    font.save(out)
    return os.path.getsize(out)

jobs = []
for src, fam, sty in [
    ("Barlow-Regular.ttf",                "Barlow",              "Regular"),
    ("Barlow-Medium.ttf",                 "Barlow",              "Medium"),
    ("BarlowSemiCondensed-Regular.ttf",   "Barlow SemiCondensed","Regular"),
    ("BarlowSemiCondensed-Medium.ttf",    "Barlow SemiCondensed","Medium"),
    ("BarlowCondensed-SemiBold.ttf",      "Barlow Condensed",    "SemiBold"),
    ("BarlowCondensed-Bold.ttf",          "Barlow Condensed",    "Bold"),
]:
    jobs.append((TTFont(src), LATIN, fam, sty, f"{OUT}/{fam.replace(' ','')}-{sty}.ttf"))

# The Cyrillic companion, pinned out of one Roboto variable font: the same
# three widths, so a Russian word beside an English one is the same shape.
for wdth, wght, fam, sty in [
    (100.0, 400, "abcTrain Cyr",               "Regular"),
    (100.0, 500, "abcTrain Cyr",               "Medium"),
    ( 87.5, 400, "abcTrain Cyr SemiCondensed", "Regular"),
    ( 87.5, 500, "abcTrain Cyr SemiCondensed", "Medium"),
    ( 75.0, 600, "abcTrain Cyr Condensed",     "SemiBold"),
    ( 75.0, 700, "abcTrain Cyr Condensed",     "Bold"),
]:
    vf = TTFont("RobotoVF.ttf")
    inst = instancer.instantiateVariableFont(vf, {"wdth": wdth, "wght": wght}, inplace=True)
    jobs.append((inst, CYR, fam, sty, f"{OUT}/{fam.replace(' ','')}-{sty}.ttf"))

total = 0
for font, uni, fam, sty, out in jobs:
    n = cut(font, uni, fam, sty, out)
    total += n
    print(f"{os.path.basename(out):44s} {n/1024:7.1f} KB")
print(f"{'TOTAL':44s} {total/1024:7.1f} KB")
