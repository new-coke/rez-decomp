#!/usr/bin/env python3
"""Put the game files the build needs in place and check their hashes.

  extract.py --disc "Rez (USA).cue"             a bin/cue (or .bin, or .iso) disc image
  extract.py --exe SLUS_203.44 --cvm REZ.CVM    the two files, copied from the disc by hand
  extract.py --exe SLUS_203.44 --overlays DIR   the 29 overlay files (<NAME>.BIN) instead of REZ.CVM
  extract.py --check                            only check what is already in place"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from asset_analysis.iso9660 import Image  # noqa: E402

BUILD = "ps2-us-1.01"
BUILDS = {b["id"]: b
          for b in json.loads((ROOT / "config/versions/builds.json").read_text())["builds"]}
REG = json.loads((ROOT / "config/overlays" / f"{BUILD}.json").read_text())
GAME = ROOT / "game" / BUILD
EXE = GAME / BUILDS[BUILD]["boot_executable"]
CVM = GAME / "REZ.CVM"
OVL = ROOT / REG["extract_dir"]


def digest(path: Path, algorithm: str = "sha256") -> str:
    h = hashlib.new(algorithm)
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def disc_image(path: Path) -> Path:
    """The data file of a disc image: the BINARY file a .cue names, else the file itself."""
    if path.suffix.lower() != ".cue":
        return path
    m = re.search(r'^\s*FILE\s+"([^"]+)"\s+BINARY', path.read_text(errors="replace"), re.M | re.I)
    if not m:
        sys.exit(f"{path}: no FILE \"...\" BINARY line")
    return path.parent / m.group(1)


def place(data: bytes, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)


def copy(src: Path, dest: Path) -> None:
    if not src.is_file():
        sys.exit(f"{src}: not found")
    dest.parent.mkdir(parents=True, exist_ok=True)
    if src.resolve() != dest.resolve():
        shutil.copyfile(src, dest)


def check() -> int:
    """Compare every file in place with config/; print one line each, return the failure count."""
    bad = 0
    want = [("main", EXE, BUILDS[BUILD]["executable_sha256"])]
    want += [(o["module"], OVL / o["path"].split("/")[-1], o["sha256"]) for o in REG["overlays"]]
    for name, path, want_digest in want:
        if not path.is_file():
            print(f"MISSING  {name:<9} {path.relative_to(ROOT)}")
            bad += 1
        elif digest(path) != want_digest:
            print(f"BAD      {name:<9} {path.relative_to(ROOT)} (sha256 differs from config)")
            bad += 1
        else:
            print(f"ok       {name:<9} {path.relative_to(ROOT)}")
    return bad


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--disc", type=Path, help="disc image: .cue, .bin (2352-byte sectors) or .iso")
    ap.add_argument("--exe", type=Path, help="SLUS_203.44 from the disc")
    ap.add_argument("--cvm", type=Path, help="REZ.CVM from the disc")
    ap.add_argument("--overlays", type=Path, help="a folder with the 29 overlay files (<NAME>.BIN)")
    ap.add_argument("--check", action="store_true", help="only check the files already in place")
    a = ap.parse_args(argv)
    if not (a.check or a.disc or a.exe or a.cvm or a.overlays):
        ap.error("give --disc, or --exe with --cvm or --overlays (or --check)")

    if a.disc:
        img = disc_image(a.disc)
        if not img.is_file():
            sys.exit(f"{img}: not found")
        track = BUILDS[BUILD].get("redump_track_sha1")
        if track and img.suffix.lower() == ".bin":
            print("hashing the disc image ...", flush=True)
            got = digest(img, "sha1")
            print(f"disc image sha1 {got}: "
                  + ("matches the Redump record" if got == track else
                     f"differs from the Redump record {track} (the files are checked below)"))
        with Image(img) as d:
            place(d.read_file(EXE.name), EXE)
            place(d.read_file(CVM.name), CVM)
        print(f"extracted {EXE.relative_to(ROOT)} and {CVM.relative_to(ROOT)}")
    if a.exe:
        copy(a.exe, EXE)
    if a.cvm:
        copy(a.cvm, CVM)
    if a.overlays:
        for o in REG["overlays"]:
            name = o["path"].split("/")[-1]
            tries = (a.overlays / name, a.overlays / o["path"])
            src = next((p for p in tries if p.is_file()), None)
            if src is None:
                sys.exit(f"{a.overlays}: {name} not found (also tried {o['path']})")
            copy(src, OVL / name)
    elif (a.disc or a.cvm) and CVM.is_file():
        with Image(CVM) as v:
            for o in REG["overlays"]:
                place(v.read_file(o["path"]), OVL / o["path"].split("/")[-1])
        print(f"extracted {len(REG['overlays'])} overlays to {OVL.relative_to(ROOT)}/")

    bad = check()
    print("all files match config/" if not bad else f"{bad} file(s) missing or different")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
