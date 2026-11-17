"""Compile a function with a recorded or candidate compiler and compare it with the
original."""

from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SHARED_INCLUDE = ROOT / "src" / "include"   # types_*.h, sdk_compat.h, stdarg.h


def header_dirs(own: Path) -> list[Path]:
    """Where a source's headers are: its own directory, then src/include if it exists."""
    shared = SHARED_INCLUDE.is_dir() and SHARED_INCLUDE.resolve() != Path(own).resolve()
    return [Path(own)] + ([SHARED_INCLUDE] if shared else [])


def include_flags(own) -> list[str]:
    """-I flags of a decompiled source whose directory is `own` (as the caller spells it)."""
    return [f"-I{own}"] + [f"-I{d}" for d in header_dirs(Path(own))[1:]]


def load_candidates(path: str | Path, platform: str | None = None) -> list[dict]:
    """The compilers of a compilers.json (optionally one platform's), {tools} filled in."""
    doc = json.loads(Path(path).read_text())
    tools = os.environ.get("REZ_TOOLS_DIR", str(ROOT / ".tools"))
    out = []
    for c in doc["candidates"]:
        if platform and c["platform"] != platform:
            continue
        cmd = [part.replace("{tools}", tools) for part in c["command"]]
        out.append({**c, "command": cmd})
    return out


def build_symbols(build: str | None) -> dict[str, int]:
    """config/symbols/<build>.txt, if present. An overlay build (<parent>--<module>) also gets
    its parent executable's symbols, which its calls and data references reach."""
    from .relocs import load_symbol_file

    out: dict[str, int] = {}
    if not build:
        return out
    for b in ([build.split("--")[0]] if "--" in build else []) + [build]:
        path = ROOT / "config" / "symbols" / f"{b}.txt"
        if path.is_file():
            out.update(load_symbol_file(path))
    return out


def probe(target, address: int, source, symbol: str, candidates: list[dict],
          size: int | None = None, load_address: int | None = None,
          extra_flags: str | list[str] = "", symbols: dict[str, int] | None = None) -> list[dict]:
    """Compile `source` with every candidate and match `symbol` at `address`; best first."""
    from binary_inspector.util import FormatError

    from .match import match_function

    results = []
    extra = shlex.split(extra_flags) if isinstance(extra_flags, str) else list(extra_flags)
    with tempfile.TemporaryDirectory() as tmp:
        for cand in candidates:
            exe = cand["command"][0]
            if shutil.which(exe) is None and not Path(exe).is_file():
                results.append({"compiler": cand["id"], "flags": None,
                                "error": f"compiler not installed: {exe}"})
                continue
            for i, flags in enumerate(cand["flag_sets"]):
                obj = Path(tmp) / f"{len(results)}_{i}.o"
                cmd = [*cand["command"], *shlex.split(flags), *extra,
                       *include_flags(Path(source).resolve().parent), str(source), "-o", str(obj)]
                proc = subprocess.run(cmd, capture_output=True, text=True)
                entry = {"compiler": cand["id"], "flags": flags}
                if proc.returncode != 0:
                    entry["error"] = proc.stderr.strip()[-500:]
                else:
                    try:
                        r = match_function(target, address, obj, symbol, size, load_address,
                                           symbols)
                        rel = r["relocations"]
                        entry.update(exact=r["exact"], score=r["score"],
                                     units=f"{r['candidate_units']}/{r['target_units']}",
                                     code_exact=r["code_exact"],
                                     relocations=f"{rel['checked']} ok, "
                                                 f"{len(rel['mismatched'])} wrong, "
                                                 f"{len(rel['unverified'])} unverified")
                    except FormatError as e:
                        entry["error"] = str(e)
                results.append(entry)
    return sorted(results, key=lambda e: (not e.get("exact", False), -e.get("score", -1)))


def consistent_candidates(per_function: dict[str, list[dict]]) -> list[tuple[str, str]]:
    """(compiler, flags) pairs that match every probed function exactly."""
    sets = [{(r["compiler"], r["flags"]) for r in results if r.get("exact")}
            for results in per_function.values()]
    return sorted(set.intersection(*sets)) if sets else []


def verify_matches(inventory, target, compilers_path,
                   load_address: int | None = None) -> list[dict]:
    """Recompile every matching record with its recorded compiler and flags and compare."""
    cands = {c["id"]: c for c in load_candidates(compilers_path)}
    out = []
    for rec in sorted(inventory.records.values(), key=lambda r: r.addr):
        if rec.status != "matching":
            continue
        m = rec.match
        cand = cands.get(m.compiler)
        if cand is None:
            out.append({"address": rec.address, "name": rec.name, "ok": False,
                        "error": f"compiler {m.compiler} not in {compilers_path}"})
            continue
        res = probe(target, rec.addr, ROOT / rec.source_file, rec.name,
                    [{**cand, "flag_sets": [m.flags]}], rec.size, load_address,
                    symbols=build_symbols(inventory.build))[0]
        out.append({"address": rec.address, "name": rec.name, "ok": bool(res.get("exact")),
                    **{k: res[k] for k in ("score", "relocations", "error") if k in res}})
    return out
