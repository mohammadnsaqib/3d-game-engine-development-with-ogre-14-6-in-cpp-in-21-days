#!/usr/bin/env python3
"""Cook the Lantern Keep into a folder somebody else can unzip and run.

    python cook.py --build D:/b/d21 --out dist [--zip]

WHAT COOKING MEANS HERE. The build tree is not a game. It holds one executable, a
pile of DLLs that CMake happened to copy, and two absolute paths compiled into the
binary that point at an SDK on this machine. Cooking is the step that turns that
into a self-contained folder: the executable, only the libraries it actually loads,
the media it actually reads, and the game's own data -- arranged so the program
finds all of it relative to itself.

THE LIST OF DLLs IS EXPLICIT, AND THAT IS THE POINT. It would be a one-liner to
copy `*.dll` from the build tree, and the result would ship twenty-three libraries
where six are needed, including three render systems the game never asks for and
SDL2, which this game does not use at all. A shipped folder is a promise about what
your program depends on; a wildcard is a refusal to find out. When a dependency is
added, this list is where it is added, deliberately, once.

WHAT IS DELIBERATELY NOT COPIED:
  * the other two render systems -- the engine loads Direct3D11 and nothing else,
  * SDL2 -- the Bites component can use it for windowing; this game never opens
    a window,
  * the sample plugins (BSP, Octree, PCZ, ParticleFX, DotScene) -- never loaded,
  * anything under Media/packs and Media/Terrain -- never referenced,
  * the .lib and .pdb files, which are build artefacts and not runtime ones.

Run with --verify to launch the cooked folder and confirm it starts with the SDK
renamed out of the way. A cook that is only checked on the machine that has the SDK
installed has not been checked at all; that is the entire failure this script
exists to prevent.
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
import zipfile
from pathlib import Path

# Everything the engine actually loads. See the docstring: a list, not a glob.
RUNTIME_DLLS = [
    "OgreMain.dll",              # the engine
    "OgreRTShaderSystem.dll",    # Day 1's shader generator; nothing renders without it
    "OgreBites.dll",             # Day 19's gizmos
    "OgreOverlay.dll",           # Bites pulls this in
    "RenderSystem_Direct3D11.dll",  # the one render system this engine asks for by name
    "Codec_STBI.dll",            # writes the PNGs
    "z.dll",                     # zlib, which OgreMain links
]

# The two media folders the renderer adds as resource locations, and nothing else.
MEDIA_DIRS = ["Main", "RTShaderLib"]


def cook(build: Path, sdk: Path, source: Path, out: Path) -> dict:
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    report = {"exe": 0, "dlls": 0, "media": 0, "gamemedia": 0, "data": 0, "missing": []}

    exe = build / "Release" / "lanternkeep.exe"
    if not exe.is_file():
        exe = build / "lanternkeep.exe"
    if exe.is_file():
        shutil.copy2(exe, out / "lanternkeep.exe")
        report["exe"] = 1
    else:
        report["missing"].append("lanternkeep.exe")

    for name in RUNTIME_DLLS:
        src = build / "Release" / name
        if not src.is_file():
            src = sdk / "bin" / name
        if src.is_file():
            shutil.copy2(src, out / name)
            report["dlls"] += 1
        else:
            # NAMED, NOT SKIPPED. A cook that quietly ships six of seven libraries
            # produces a folder that fails on somebody else's machine with a message
            # about a plugin.
            report["missing"].append(name)

    media = out / "media"
    for d in MEDIA_DIRS:
        src = sdk / "Media" / d
        if src.is_dir():
            shutil.copytree(src, media / d)
            report["media"] += sum(1 for _ in (media / d).rglob("*") if _.is_file())
        else:
            report["missing"].append(f"Media/{d}")

    gm = source / ".." / ".." / "media"
    if gm.is_dir():
        shutil.copytree(gm, out / "gamemedia")
        report["gamemedia"] = sum(1 for _ in (out / "gamemedia").rglob("*")
                                   if _.is_file())

    data = source / "lanternkeep" / "data"
    if data.is_dir():
        shutil.copytree(data, out / "data")
        report["data"] = sum(1 for _ in (out / "data").rglob("*") if _.is_file())

    return report


def make_zip(out: Path) -> tuple[int, int]:
    archive = out.with_suffix(".zip")
    if archive.exists():
        archive.unlink()
    count = 0
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(out.rglob("*")):
            if f.is_file():
                z.write(f, f.relative_to(out))
                count += 1
    return count, archive.stat().st_size


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", required=True)
    ap.add_argument("--sdk", default="D:/OgreSDK")
    ap.add_argument("--source", default=str(Path(__file__).resolve().parent))
    ap.add_argument("--out", default="dist")
    ap.add_argument("--zip", action="store_true")
    a = ap.parse_args()

    out = Path(a.out).resolve()
    report = cook(Path(a.build), Path(a.sdk), Path(a.source), out)

    print(f"cooked into {out}")
    print(f"  executable   {report['exe']}")
    print(f"  libraries    {report['dlls']} of {len(RUNTIME_DLLS)}")
    print(f"  engine media {report['media']} file(s)")
    print(f"  game media   {report['gamemedia']} file(s)")
    print(f"  game data    {report['data']} file(s)")
    if report["missing"]:
        print("  MISSING      " + ", ".join(report["missing"]))

    if a.zip:
        count, size = make_zip(out)
        print(f"  archive      {count} file(s), {size / 1024 / 1024:.1f} MB")

    return 1 if report["missing"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
