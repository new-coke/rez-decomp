#!/usr/bin/env python3
"""Check the linked executable and 29 overlays against the recorded sha256.

  verify_build.py [--dir build/relink/ps2-us-1.01/exact]"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = "ps2-us-1.01"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--dir", type=Path, default=ROOT / "build/relink" / BUILD / "exact",
                    help="relink output to check (default build/relink/ps2-us-1.01/exact)")
    a = ap.parse_args(argv)
    builds = {b["id"]: b
              for b in json.loads((ROOT / "config/versions/builds.json").read_text())["builds"]}
    reg = json.loads((ROOT / "config/overlays" / f"{BUILD}.json").read_text())
    want = [("main", a.dir / builds[BUILD]["boot_executable"], builds[BUILD]["executable_sha256"])]
    want += [(o["module"], a.dir / "overlays" / o["path"].split("/")[-1], o["sha256"])
             for o in reg["overlays"]]
    good = 0
    for name, path, digest in want:
        if not path.is_file():
            print(f"{name:<9} {path.name:<13} MISSING")
            continue
        got = hashlib.sha256(path.read_bytes()).hexdigest()
        ok = got == digest
        good += ok
        verdict = "OK" if ok else f"DIFFERS (expected {digest})"
        print(f"{name:<9} {path.name:<13} {got} {verdict}")
    print(f"{good}/{len(want)} sha256 match")
    return 0 if good == len(want) else 1


if __name__ == "__main__":
    sys.exit(main())
