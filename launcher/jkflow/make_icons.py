#!/usr/bin/env python3
"""JKPro icon0.png (512x512) and pic1.png (1920x1080) for the PKG's sce_sys.

The art is JKPro's own (launcher/src/jkpro_art.c: the glowing JKPRO logo with a
football as the O, on a night stadium); no game or publisher art. The PNGs in
launcher/jkflow/icons/ are rendered by `launcher/tests/run.sh make_jkpro_art`
(build/sce_sys-jkpro/) and committed; this script copies them, so the PKG build
needs no compiler.

usage: make_icons.py OUTDIR
"""
import shutil
import sys
from pathlib import Path

here = Path(__file__).resolve().parent / "icons"
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for name in ("icon0.png", "pic1.png"):
    shutil.copyfile(here / name, out / name)
print("icons written to", out)
