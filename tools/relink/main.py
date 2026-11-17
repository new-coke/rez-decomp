"""Link the main executable with ld from a generated linker script (see tools/README.md).

    python scripts/relink.py [--overlays] [--out DIR] [--pad ADDR:N ...] [--all-c] [--jobs N]"""

from __future__ import annotations

import argparse
import bisect
import collections
import hashlib
import json
import os
import shlex
import shutil
import struct
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from objmatch.probe import build_symbols, header_dirs, include_flags, load_candidates
from objmatch.relocs import R_MIPS_26, R_MIPS_LITERAL, decode, resolve

from . import elf32, mips
from .mips import R_MIPS_32, R_MIPS_GPREL16, RELOC_NAMES, encode

try:
    from . import xbuild            # private; absent from the public release
except ImportError:
    xbuild = None

ROOT = Path(__file__).resolve().parents[2]
BIN = Path(os.environ.get("REZ_TOOLS_DIR", ROOT / ".tools")) / "ps2/binutils/mips-ps2-decompals-"
TEXT_SECTIONS = (".text",)
DATA_SECTIONS = (".vutext", ".data", ".rodata", ".lit4", ".sdata", ".sbss", ".bss")
SCAN_POINTERS = {".data", ".rodata", ".lit4", ".sdata"}   # .vutext is VU microcode
SCRATCH = 0x40000000
DISCARD = (".reginfo", ".mdebug", ".pdr", ".comment", ".gnu.attributes", ".MIPS.abiflags")
DEFAULT_COMPILER = ("ee-gcc2.9-991111-01", "-O2 -G0 -DVERSION_US")
SHF_ALLOC, SHF_EXECINSTR = 2, 4
STB_GLOBAL, STB_WEAK = 1, 2


def builds() -> dict:
    reg = json.loads((ROOT / "config/versions/builds.json").read_text())
    return {b["id"]: b for b in reg["builds"]}


class Original:
    def __init__(self, path: Path):
        self.blob = path.read_bytes()
        self.elf = elf32.read(self.blob)
        self.secs = {s.name: s for s in self.elf.sections if s.name}
        ri = self.secs[".reginfo"]
        self.gp = struct.unpack_from("<I", self.blob, ri.offset + 20)[0]
        self.lo = self.secs[".text"].addr
        last = self.secs[".bss"]
        self.hi = last.addr + last.size              # inclusive: an end pointer is internal
        self.order = [self.secs[n] for n in TEXT_SECTIONS + DATA_SECTIONS]
        self.starts = [s.addr for s in self.order]

    def internal(self, a: int) -> bool:
        return self.lo <= a <= self.hi

    def section_of(self, a: int):
        """The section holding `a`, or None. An address at a section's end belongs to it only
        when no section starts there."""
        i = bisect.bisect_right(self.starts, a) - 1
        if i < 0:
            return None
        s = self.order[i]
        if a < s.addr + s.size or (a == s.addr + s.size and (i + 1 == len(self.order)
                                                             or self.order[i + 1].addr != a)):
            return s
        return None

    def read(self, a: int, n: int) -> bytes:
        s = self.section_of(a)
        if s is None or a + n > s.addr + s.size:
            raise ValueError(f"0x{a:08x}+{n} is not inside one section")
        if s.type == elf32.SHT_NOBITS:
            return bytes(n)
        return self.blob[s.offset + a - s.addr:][:n]

    def word(self, a: int) -> int:
        return struct.unpack("<I", self.read(a, 4))[0]


def compile_one(job: tuple) -> tuple[str, str]:
    """(object path, error). Skipped when the object is newer than the source and its headers."""
    src, command, flags, obj = job
    srcp = ROOT / src
    newest = max([srcp.stat().st_mtime] + [h.stat().st_mtime for d in header_dirs(srcp.parent)
                                           for h in d.glob("*.h")])
    objp = Path(obj)
    if objp.exists() and objp.stat().st_mtime > newest:
        return obj, ""
    cmd = [*command, *shlex.split(flags), "-ffunction-sections", *include_flags(srcp.parent),
           str(srcp), "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return obj, ("" if r.returncode == 0 else (r.stderr.strip()[-400:] or "failed"))


def object_name(src: str, compiler: str, flags: str) -> str:
    tag = hashlib.sha1(f"{src} {compiler} {flags} fs".encode()).hexdigest()[:8]
    return f"{Path(src).stem}-{tag}.o"


def reginfo_gp(e: elf32.Elf) -> int:
    s = e.section(".reginfo")
    return struct.unpack_from("<I", e.blob, s.offset + 20)[0] if s and s.size >= 24 else 0


def rel_dicts(e: elf32.Elf, sec: elf32.Section) -> list[dict]:
    """The section's relocations in objmatch's form. Section symbols are named `<section .x>`
    because objmatch's decoder adds gp0 to gp-relative addends against sections only."""
    def name(i: int) -> str:
        sym = e.symbols[i]
        return f"<section {e.symbol_name(i)}>" if sym.type == elf32.STT_SECTION else sym.name
    return [{"offset": r.offset, "type": r.type, "symbol": name(r.sym), "sym": r.sym}
            for r in e.relocs.get(sec.index, [])]


def analyze(obj: str, recs: list[dict], orig: Original, symbols: dict) -> dict:
    """Where the object's text sections go, its data sections' original addresses, and the
    values of the undefined symbols its code uses."""
    e = elf32.read(Path(obj).read_bytes())
    gp0 = reginfo_gp(e)
    funcs = {}
    for s in e.symbols:
        if s.type == elf32.STT_FUNC and 0 < s.shndx < len(e.sections):
            funcs.setdefault(s.name, s)
    text: dict[int, int] = {}          # section index -> address
    problems, placed_recs = [], []
    by_sec = collections.defaultdict(list)
    sizes: dict[int, int] = {}
    reconstructed: set[int] = set()    # sections holding a non-matching record (--all-c)
    for r in recs:
        addr = int(r["address"], 16)
        sym = funcs.get(r["name"]) or funcs.get(f"func_{addr:08x}")
        if sym is None:
            problems.append(f"{r['address']}: no symbol {r['name']}")
            continue
        base = addr - sym.value
        if text.setdefault(sym.shndx, base) != base:
            problems.append(f"{r['address']}: section {e.sections[sym.shndx].name} has two bases")
            continue
        placed_recs.append(r)
        by_sec[sym.shndx].append(addr)
        sizes[sym.shndx] = sizes.get(sym.shndx, 0) + r["size"]
        if r["status"] != "matching":
            reconstructed.add(sym.shndx)
    bases: dict[int, set] = collections.defaultdict(set)
    undefined: dict[str, set] = collections.defaultdict(set)
    refs = []      # (site, type, value) for the verification of shifted builds
    into = []      # (text section, offset, type, symbol's section, original target)
    for idx, base in text.items():
        if idx in reconstructed:       # its code is not the original's: proves nothing
            continue
        sec = e.sections[idx]
        data = e.data(sec)
        try:
            tgt = orig.read(base, len(data))
        except ValueError:
            problems.append(f"{sec.name}: 0x{base:08x}+{len(data)} outside the image")
            continue
        for rel, value, addend in decode(rel_dicts(e, sec), tgt, data, "MIPS", False,
                                         (orig.gp, gp0)):
            if value is None:
                continue
            refs.append((base + rel["offset"], rel["type"], value & 0xFFFFFFFF))
            if rel["type"] == R_MIPS_26:
                continue
            s = e.symbols[rel["sym"]]
            into.append((idx, rel["offset"], rel["type"], s.shndx, value & 0xFFFFFFFF))
            if s.type == elf32.STT_SECTION:
                if s.shndx not in text:
                    bases[s.shndx].add((value - addend) & 0xFFFFFFFF)
            elif 0 < s.shndx < len(e.sections) and s.shndx not in text:
                bases[s.shndx].add((value - addend - s.value) & 0xFFFFFFFF)
            elif s.shndx in (elf32.SHN_UNDEF, elf32.SHN_COMMON, elf32.SHN_MIPS_SCOMMON):
                undefined[s.name].add((value - addend) & 0xFFFFFFFF)
    for s in e.symbols:
        if s.name and s.type != elf32.STT_SECTION and 0 < s.shndx < len(e.sections) \
                and s.shndx not in text and s.name in symbols and s.bind == STB_GLOBAL:
            bases[s.shndx].add((symbols[s.name] - s.value) & 0xFFFFFFFF)
    sections = []
    with_symbols = {s.shndx for s in e.symbols
                    if s.name and s.type != elf32.STT_SECTION and s.bind in (STB_GLOBAL, STB_WEAK)}
    for sec in e.sections:
        if not sec.name or not sec.flags & SHF_ALLOC or sec.name.startswith(DISCARD):
            continue
        sym = next((s for s in e.symbols
                    if s.shndx == sec.index and s.type == elf32.STT_FUNC), None)
        sections.append({"index": sec.index, "name": sec.name, "size": sec.size,
                         "align": max(sec.align, 1), "nobits": sec.type == elf32.SHT_NOBITS,
                         "text": bool(sec.flags & SHF_EXECINSTR),
                         "func": sym.name if sym else None,
                         "address": text.get(sec.index), "records": by_sec.get(sec.index, []),
                         "span": sizes.get(sec.index), "matching": sec.index not in reconstructed,
                         "bases": sorted(bases.get(sec.index, ())),
                         "symbols": sec.index in with_symbols})
    defined = {s.name for s in e.symbols if s.name and s.bind in (STB_GLOBAL, STB_WEAK)
               and 0 < s.shndx < len(e.sections)}
    common = {s.name for s in e.symbols if s.shndx in (elf32.SHN_COMMON, elf32.SHN_MIPS_SCOMMON)}
    undef = {s.name for s in e.symbols if s.name and s.shndx == elf32.SHN_UNDEF}
    # a section whose relocations disagree on its base: retarget each to the original's address
    split = {i for i, b in bases.items() if len(b) > 1}
    retarget = [(i, off, t, v) for i, off, t, shndx, v in into if shndx in split]
    return {"obj": obj, "sections": sections, "retarget": retarget, "problems": problems,
            "records": [r["address"] for r in placed_recs], "defined": sorted(defined),
            "common": sorted(common), "undefined": sorted(undef),
            "values": {k: sorted(v) for k, v in undefined.items()}, "refs": refs}


def _analyze_job(args):
    try:
        return analyze(*args)
    except Exception as ex:   # noqa: BLE001 - reported per object
        return {"obj": args[0], "error": repr(ex)}


class Layout:
    """Text chunks in address order, data labels, and the symbol for any image address."""

    prefix = "rz_"      # label names: <prefix><original address>

    def __init__(self, orig: Original):
        self.orig = orig
        self.chunks: list[dict] = []
        self.labels: set[int] = set()
        t = orig.secs[".text"]
        self.text = (t.addr, t.addr + t.size)

    def finish(self):
        self.chunks.sort(key=lambda c: c["start"])
        self.cstarts = [c["start"] for c in self.chunks]

    def chunk_at(self, a: int) -> dict:
        return self.chunks[bisect.bisect_right(self.cstarts, a) - 1]

    def symbol(self, a: int) -> tuple[str, int]:
        """(symbol, addend) that names image address `a`."""
        if self.text[0] <= a < self.text[1]:
            c = self.chunk_at(a)
            return c["label"], a - c["start"]
        s = self.orig.section_of(a)
        if s is None:       # between two sections: from the end of the previous one
            prev = max((x for x in self.orig.order if x.addr + x.size <= a),
                       key=lambda x: x.addr)
            end = prev.addr + prev.size
            self.labels.add(end)
            return f"{self.prefix}{end:08x}", a - end
        self.labels.add(a)
        return f"{self.prefix}{a:08x}", 0

    def expr(self, a: int) -> str:
        if not self.orig.internal(a):
            return f"0x{a:08x}"
        s, add = self.symbol(a)
        return f"{s} + 0x{add:x}" if add else s


def plan_text(lay: Layout, analyses: list[dict], inventory: list[dict]) -> list[str]:
    """Fill `lay.chunks` with compiled and extracted chunks; return the placement problems."""
    problems = []
    lo, hi = lay.text
    placed = []
    for an in analyses:
        for s in an["sections"]:
            if s["address"] is not None and s["text"]:
                size = s["size"] if s.get("matching", True) else s["span"]
                placed.append((s["address"], size, s["align"], an["obj"], s["name"],
                               set(s["records"]), s.get("matching", True)))
    placed.sort()
    starts = sorted(int(f["address"], 16) for f in inventory)
    kept, end = [], lo
    for a, size, align, obj, name, recs, matching in placed:
        i = bisect.bisect_right(starts, a)
        while i < len(starts) and starts[i] in recs:
            i += 1
        nxt = starts[i] if i < len(starts) else hi
        if a < end or (a % align and matching) or a + size > hi:
            problems.append(f"0x{a:08x} {Path(obj).name}({name}): cannot place "
                            f"(overlap, alignment {align} or end)")
            continue
        if a + size > nxt:
            problems.append(f"0x{a:08x} {Path(obj).name}({name}): runs into 0x{nxt:08x}")
            continue
        kept.append((a, size, align, obj, name, matching))
        end = a + size
    for a, size, align, obj, name, matching in kept:
        lay.chunks.append({"start": a, "size": size, "kind": "obj", "obj": obj, "section": name,
                           "align": align, "label": f"{lay.prefix}{a:08x}", "matching": matching})
    covered = [(a, a + size) for a, size, *_ in kept] + [(hi, hi)]
    pos = lo
    for a, b in covered:
        if pos < a:
            cuts = [pos] + [s for s in starts if pos < s < a] + [a]
            for x, y in zip(cuts, cuts[1:]):
                lay.chunks.append({"start": x, "size": y - x, "kind": "asm",
                                   "label": f"{lay.prefix}{x:08x}"})
        pos = max(pos, b)
    lay.finish()
    return problems


def not_linked(lay: Layout, analyses: list[dict], inventory: list[dict]) -> list[str]:
    """Matching records whose code is not in a compiled chunk. They would fall back to the
    original's bytes and still match, so the build fails on them."""
    kept = {(c["obj"], c["section"]) for c in lay.chunks if c["kind"] == "obj"}
    linked = {r for an in analyses for s in an["sections"] if (an["obj"], s["name"]) in kept
              for r in s["records"]}
    return [f"{f['address']} {f['name']}" for f in inventory
            if f["status"] == "matching" and int(f["address"], 16) not in linked]


def data_evidence(lay: Layout, analyses: list[dict]) -> tuple[dict, list, dict]:
    """Pointer words proved by compiled units' data relocations: ({address: target}, covered
    ranges, stats). A section counts only when its unrelocated bytes equal the original's."""
    orig = lay.orig
    ptrs, covered = {}, []
    stats = collections.Counter()
    by_obj = collections.defaultdict(list)
    for an in analyses:
        for s in an["sections"]:
            if not s["text"] and not s["nobits"] and len(s["bases"]) == 1 and s["size"]:
                by_obj[an["obj"]].append(s)
    for obj, secs in by_obj.items():
        e = elf32.read(Path(obj).read_bytes())
        for s in secs:
            base = s["bases"][0]
            sec = e.sections[s["index"]]
            data = e.data(sec)
            try:
                tgt = orig.read(base, len(data))
            except ValueError:
                stats["outside"] += 1
                continue
            rels = e.relocs.get(sec.index, [])
            if any(r.type != R_MIPS_32 for r in rels):
                stats["other_relocs"] += 1
                continue
            ours, theirs = bytearray(data), bytearray(tgt)
            for r in rels:
                ours[r.offset:r.offset + 4] = theirs[r.offset:r.offset + 4] = bytes(4)
            if ours != theirs:
                stats["content_differs"] += 1
                continue
            stats["sections"] += 1
            covered.append((base, base + len(data)))
            for r in rels:
                ptrs[base + r.offset] = struct.unpack_from("<I", tgt, r.offset)[0]
    return ptrs, covered, stats


def _printable_start(orig: Original, v: int) -> bool:
    """`v` starts a NUL-terminated string of at least two printable characters."""
    try:
        b = orig.read(v - 1, 3)
    except ValueError:
        return False
    return b[0] == 0 and all(0x20 <= c < 0x7F or c in (9, 10) for c in b[1:3])


def classify(orig: Original, v: int, prev: int, nxt: int, known: set,
             funcs: list[tuple[int, int]]) -> tuple[str, str] | None:
    """(strength, reason) for taking data word `v` for an address, or None. Integers, bitmaps
    and 16.16 fixed-point numbers share the image's range, so a value needs a reason.

    Strong: a known target, a jump table (two neighbouring pointers into one function), or a
    string start. Weak: a 4-aligned target outside .text that is not a string's tail; kept
    only next to stronger evidence or in a run of weak ones. A multiple of 0x10000 is never
    weak, and strong only when a neighbour points into the same section."""
    if v & 0xFFFF == 0:
        sec = orig.section_of(v)
        if not any(orig.internal(x) and orig.section_of(x) is sec for x in (prev, nxt)):
            return None
    if v in known:
        return "strong", "known target"
    i = bisect.bisect_right(funcs, (v, 1 << 32)) - 1
    if 0 <= i and funcs[i][0] <= v < funcs[i][1]:
        f = funcs[i]
        if v % 4 == 0 and any(f[0] <= x < f[1] for x in (prev, nxt)):
            return "strong", "jump table"
        return None
    s = orig.section_of(v)
    if s is None or s.name == ".text":
        return None
    if s.type != elf32.SHT_NOBITS and _printable_start(orig, v):
        return "strong", "string"
    if v % 4 == 0 and v & 0xFFFF and not _string_tail(v, prev):
        return "weak", f"aligned address in {s.name}, next to a pointer"
    return None


def _printable(b: bytes) -> bool:
    return all(0x20 <= c < 0x7F for c in b)


def _string_tail(v: int, prev: int) -> bool:
    """The word ends a string the previous word continues ("ptm\0" after "ult.")."""
    return (v >> 24 == 0 and _printable(v.to_bytes(4, "little")[:3])
            and _printable(prev.to_bytes(4, "little")))


def _bitmap(v: int) -> bool:
    """Every hex digit is 0-3: the font and icon bitmaps."""
    return all((v >> s) & 0xF <= 3 for s in range(0, 32, 4))


def scan_data(lay: Layout, evidence: dict, covered: list, known: set, funcs: list,
              cross=None, scan=SCAN_POINTERS, raw=(".vutext",)) -> tuple:
    """({address: target}, suspects, stats) for the data words taken for addresses: by
    compiled units' relocations, else `cross` (a second build's verdicts) where present, else
    `classify`."""
    orig = lay.orig
    cov = sorted(covered)
    cstarts = [a for a, _ in cov]
    ptrs, suspects = dict(evidence), []
    stats = collections.Counter(evidence=len(evidence))

    def in_cov(a):
        i = bisect.bisect_right(cstarts, a) - 1
        return i >= 0 and a < cov[i][1]

    for name in sorted(set(scan) | set(raw)):
        s = orig.secs[name]
        data = orig.blob[s.offset:s.offset + s.size]
        n = s.size // 4
        words = struct.unpack_from(f"<{n}I", data)
        why, verdict = {}, {}
        for k, v in enumerate(words):
            if orig.internal(v):
                why[k] = classify(orig, v, words[k - 1] if k else 0,
                                  words[k + 1] if k + 1 < n else 0, known, funcs)
                if cross is not None and name in scan:
                    verdict[k] = cross.judge(s.addr + 4 * k, v)
        strong = ({k for k, w in why.items() if w and w[0] == "strong"}
                  | {k for k, d in verdict.items() if d == "pointer"}
                  | {(a - s.addr) // 4 for a in evidence if s.addr <= a < s.addr + s.size})
        weak = {k for k, w in why.items() if w and w[0] == "weak" and not _bitmap(words[k])}
        for k, w in why.items():
            if w and w[0] == "weak" and not (any(k + d in strong for d in (-2, -1, 1, 2))
                                             or (k in weak and (k - 1 in weak or k + 1 in weak))):
                why[k] = None
        for k, w in why.items():
            a, v = s.addr + 4 * k, words[k]
            d = verdict.get(k)
            if in_cov(a) and name in scan:
                truth = "pointer" if a in evidence else "integer"
                stats[f"calibration_{truth}_heuristic_{'accepted' if w else 'rejected'}"] += 1
                if d:
                    stats[f"calibration_{truth}_cross_says_{d}"] += 1
                continue
            if d == "pointer":
                ptrs[a] = v
                stats["cross: moved with its target"] += 1
            elif d == "integer":
                stats["cross: same value, an integer"] += 1
            elif name not in scan:
                stats["vutext_in_range"] += 1
                suspects.append((a, f"{name} word 0x{v:08x} (VU microcode, not converted)"))
            elif w:
                ptrs[a] = v
                stats[f"heuristic: {w[1]}"] += 1
            else:
                stats["heuristic: left as integer"] += 1
                suspects.append((a, f"{name} word 0x{v:08x}: in the image's range, taken for "
                                    "an integer"))
    return ptrs, suspects, stats


def _words_lines(words: list[int]) -> list[str]:
    return [" .word " + ", ".join(f"0x{w:08x}" for w in words[i:i + 4])
            for i in range(0, len(words), 4)]


def code_chunks(lay: Layout) -> tuple[dict, list, list]:
    """Symbolize every extracted text chunk: ({start: (words, refs)}, all refs, suspects)."""
    orig = lay.orig
    out, flat, suspects = {}, [], []
    for c in lay.chunks:
        if c["kind"] != "asm":
            continue
        data = orig.read(c["start"], c["size"])
        words = list(struct.unpack(f"<{c['size'] // 4}I", data))
        refs, sus = mips.symbolize(words, c["start"], orig.internal, orig.gp,
                                   c["start"] + c["size"])
        out[c["start"]] = (words, refs)
        suspects += sus
        for r in refs:
            lui = c["start"] + 4 * r.pair if r.pair is not None else None
            flat.append((c["start"] + 4 * r.index, r.type, r.target, lui))
    return out, flat, suspects


def emit_text(lay: Layout, symbolized: dict, path: Path):
    lines = ["# Generated by rez-relink from the user's executable. Do not commit.",
             ".set noreorder", ".set noat"]
    for c in lay.chunks:
        if c["kind"] != "asm":
            continue
        words, refs = symbolized[c["start"]]
        lab = c["label"]
        lines += [f'.section .text.{lab},"ax",@progbits', ".balign 4", f".globl {lab}", f"{lab}:"]
        byidx = {r.index: r for r in refs}
        run: list[int] = []
        for i, w in enumerate(words):
            r = byidx.get(i)
            if r is None:
                run.append(w)
                continue
            lines += _words_lines(run)
            run = []
            sym, add = lay.symbol(r.target)
            lines += [f" .reloc ., {RELOC_NAMES[r.type]}, {sym}",
                      f" .word 0x{encode(w, r.type, add):08x}"]
        lines += _words_lines(run)
    path.write_text("\n".join(lines) + "\n")


SECTION_FLAGS = {".vutext": '"ax",@progbits', ".data": '"aw",@progbits', ".rodata": '"a",@progbits',
                 ".lit4": '"aw",@progbits', ".sdata": '"aw",@progbits', ".sbss": '"aw",@nobits',
                 ".bss": '"aw",@nobits', ".head": '"aw",@progbits'}


def emit_data(lay: Layout, name: str, ptrs: dict, path: Path, pads: dict | None = None):
    """One data section as assembly: its bytes, a label at every referenced address, each
    pointer as `.word symbol+offset`. `pads` {address: n} puts n zero bytes before a label."""
    orig = lay.orig
    s = orig.secs[name]
    nobits = s.type == elf32.SHT_NOBITS
    data = b"" if nobits else orig.blob[s.offset:s.offset + s.size]
    # a label at the end belongs here only when no section starts there
    lab = sorted(a for a in lay.labels if s.addr <= a < s.addr + s.size
                 or (a == s.addr + s.size and orig.section_of(a) is s))
    ptr = sorted(a for a in ptrs if s.addr <= a < s.addr + s.size)
    events = sorted({(a, 0) for a in lab} | {(a, 1) for a in ptr})
    lines = ["# Generated by rez-relink from the user's executable. Do not commit.",
             f".section {name},{SECTION_FLAGS[name]}", ".balign 1"]
    pos = s.addr

    def raw(a: int, b: int):
        if nobits:
            if b > a:
                lines.append(f" .space 0x{b - a:x}")
            return
        while a < b and a % 4:
            lines.append(f" .byte 0x{data[a - s.addr]:02x}")
            a += 1
        n = (b - a) // 4
        if n:
            lines.extend(_words_lines(list(struct.unpack_from(f"<{n}I", data, a - s.addr))))
            a += 4 * n
        while a < b:
            lines.append(f" .byte 0x{data[a - s.addr]:02x}")
            a += 1

    for a, kind in events:
        if a < pos:
            continue
        raw(pos, a)
        pos = a
        if kind == 0:
            if pads and a in pads:
                lines.append(f" .space 0x{pads[a]:x}")
            lines += [f".globl {lay.prefix}{a:08x}", f"{lay.prefix}{a:08x}:"]
        else:
            lines.append(f" .word {lay.expr(ptrs[a])}")
            pos = a + 4
    raw(pos, s.addr + s.size)
    path.write_text("\n".join(lines) + "\n")


def section_aligns(orig: Original) -> dict[str, str]:
    """The ALIGN(n) that places each data section where the original has it."""
    out = {}
    for prev, s in zip(orig.order, orig.order[1:]):
        end = prev.addr + prev.size
        for n in sorted({max(s.align, 1), 16, 32, 64, 128}):
            if (end + n - 1) // n * n == s.addr:
                out[s.name] = f"ALIGN({n})"
                break
        else:
            raise ValueError(f"{s.name}: no alignment puts it at 0x{s.addr:08x}")
    return out


READONLY = (".rodata", ".rdata", ".lit4", ".lit8")


def into_reconstructed(lay: Layout, ptrs: dict, code_refs: list) -> list[str]:
    """References from extracted code or data into the middle of a reconstructed function."""
    out = []
    refs = [(s, t, "data word") for s, t in ptrs.items()] + \
        [(s, t, "extracted code") for s, _, t, _ in code_refs]
    for site, t, kind in sorted(refs):
        if not lay.text[0] <= t < lay.text[1]:
            continue
        c = lay.chunk_at(t)
        if c["kind"] == "obj" and not c.get("matching", True) and t != c["start"]:
            out.append(f"0x{site:08x} {kind} -> 0x{t:08x} ({c['label']} + 0x{t - c['start']:x})")
    return out


def split_reconstructed(analyses: list[dict], dest: Path) -> tuple[list[dict], dict]:
    """--all-c: give reconstructed code the data its own compiler emitted. A unit with both
    kinds is linked from two copies of its object: the first links the matching functions,
    the second (`<obj>.recon.o`) the reconstructed ones with their read-only data as real
    data and their writable data shadowed over the first copy's."""
    dest.mkdir(parents=True, exist_ok=True)
    out, stats = [], collections.Counter()
    for an in analyses:
        recon = [s for s in an["sections"]
                 if s["text"] and s["address"] is not None and not s["matching"]]
        out.append(an)
        if not recon:
            continue
        if not any(s["text"] and s["address"] is not None and s["matching"]
                   for s in an["sections"]):
            stats["units_reconstructed_only"] += 1
            for s in an["sections"]:
                if not s["text"] and s["name"].startswith(READONLY) and s["size"]:
                    s.update(bases=[], real=True)
            continue
        stats["units_split"] += 1
        cp = dest / (Path(an["obj"]).stem + ".recon.o")
        shutil.copyfile(an["obj"], cp)
        rc = dict(an, obj=str(cp), retarget=[], refs=[], values={}, problems=[], sections=[],
                  recon_copy=True)
        for s in an["sections"]:
            t = dict(s)
            if s["text"]:
                if s["address"] is not None and s["matching"]:
                    t.update(address=None, shadow_at=s["address"])
            elif s["name"].startswith(READONLY):
                t.update(bases=[], real=True)
            else:
                t["twin"] = (an["obj"], s["name"])
            rc["sections"].append(t)
        for s in recon:
            s.update(address=None, shadow_at=s["address"])
        out.append(rc)
    return out, stats


def shadows(lay: Layout, analyses: list[dict], symbols: dict, kept: set,
            real: list | None = None, place=None) -> tuple[list, dict]:
    """(shadow list, stats): each compiled section not linked as code, overlaid (NOLOAD) where
    the original holds its contents. `real` sections are linked as data (--all-c); a `twin`
    lies over the section it copies."""
    place = place or lay.orig.internal
    out, stats = [], collections.Counter()
    fate: dict[tuple, object] = {}       # (obj, section) -> address, real symbol or None
    for an in analyses:
        for s in an["sections"]:
            if (an["obj"], s["name"]) in kept:
                continue
            if not s["size"] and not s["symbols"]:
                continue
            key = (an["obj"], s["name"])
            if s.get("real") and real is not None:
                fate[key] = f"rzreal_{len(real)}"
                real.append((an["obj"], s["name"], s["align"], fate[key]))
                stats["linked_as_data_reconstructed"] += 1
                continue
            if s.get("twin") in fate:
                twin = fate[s["twin"]]
                if twin is not None:
                    out.append((an["obj"], s["name"], twin, s["size"]))
                    stats["twin"] += 1
                    continue
            addr = None
            if s.get("shadow_at") is not None:
                addr = s["shadow_at"]
                stats["text_at_its_chunk"] += 1
            elif len(s["bases"]) == 1:
                addr = s["bases"][0]
                stats["by_relocation"] += 1
            elif len(s["bases"]) > 1:
                stats["conflicting"] += 1
            elif s["func"] and resolve(s["func"], symbols) is not None:
                addr = resolve(s["func"], symbols)
                stats["by_name"] += 1
            if (addr is None or not place(addr)) and real is not None \
                    and not s["text"] and s["size"]:
                fate[key] = f"rzreal_{len(real)}"
                real.append((an["obj"], s["name"], s["align"], fate[key]))
                stats["linked_as_data"] += 1
                continue
            if addr is None or not place(addr):
                addr = None
                stats["scratch" if s["size"] else "scratch_empty"] += 1
                if s["size"] and s["symbols"]:
                    stats["scratch_with_symbols"] += 1
            fate[key] = addr
            out.append((an["obj"], s["name"], addr, s["size"]))
    for _, _, addr, _ in out:
        if isinstance(addr, int):
            lay.symbol(addr)
    return out, stats


OUTPUT_OF = {".rodata": ".rodata", ".rdata": ".rodata", ".data": ".data", ".sdata": ".sdata",
             ".sbss": ".sbss", ".bss": ".bss", ".lit4": ".lit4", ".lit8": ".lit4",
             ".vutext": ".vutext"}


def linker_script(lay: Layout, data_objs: dict, shadow_list: list, aliases: dict, pads: dict,
                  gp_offset: int, real: list | None = None) -> str:
    orig = lay.orig
    aligns = section_aligns(orig)
    L = ["SECTIONS", "{", f"  .text 0x{lay.text[0]:08x} : {{"]
    for c in lay.chunks:
        if c["kind"] == "obj":
            if c["align"] > 4:
                L.append(f"    . = ALIGN({c['align']});")
            L.append(f'    {c["label"]} = .; "{c["obj"]}"({c["section"]})')
        else:
            L.append(f'    "{data_objs[".text"]}"(.text.{c["label"]})')
        end = c["start"] + c["size"]
        if end in pads:
            L.append(f"    . += 0x{pads[end]:x};")
    L.append("  }")
    extra = collections.defaultdict(list)
    for obj, sec, align, sym in real or []:
        al = f". = ALIGN({align}); " if align > 1 else ""
        extra[OUTPUT_OF.get(sec, ".data")].append(f'{al}{sym} = .; "{obj}"({sec})')
    if real is not None:
        extra[".sbss"].append("*(.scommon)")
        extra[".bss"].append("*(COMMON)")
    for name in DATA_SECTIONS:
        pad = f" . += 0x{pads[name]:x};" if name in pads else ""
        more = "".join(f"\n    {x}" for x in extra[name])
        L.append(f'  {name} {aligns[name]} : {{{pad} "{data_objs[name]}"({name}){more} }}')
    L += shadow_script(lay, shadow_list)
    L.append("  /DISCARD/ : { " + " ".join(f"*({d}*)" for d in DISCARD) + " }")
    L.append("}")
    # the original's gp is .lit4's start + 0x7ff0 (the small-data area begins with .lit4)
    L.append(f"_gp = {lay.expr(orig.secs['.lit4'].addr)} + 0x{gp_offset:x};")
    L += [f"{n} = {e};" for n, e in sorted(aliases.items())]
    return "\n".join(L) + "\n"


def shadow_script(lay: Layout, shadow_list: list) -> list[str]:
    """The NOLOAD output sections that hold the shadows."""
    L = []
    # as few output sections as overlaps allow; one per shadow in .text (--all-c)
    groups: list[list] = []
    for obj, name, addr, size in sorted((x for x in shadow_list if isinstance(x[2], int)),
                                        key=lambda x: (x[2], x[3])):
        if not groups or addr < groups[-1][-1][0] + groups[-1][-1][1] \
                or lay.text[0] <= addr < lay.text[1]:
            groups.append([])
        groups[-1].append((addr, size, obj, name))
    for i, g in enumerate(groups):
        L.append(f"  .shadow.{i} ({lay.expr(g[0][0])}) (NOLOAD) : {{")
        L += [f'    . = {lay.expr(addr)}; "{obj}"({name})' for addr, _, obj, name in g]
        L.append("  }")
    # twins of sections linked as real data (--all-c): over the real copy
    for i, (obj, name, sym, _) in enumerate(x for x in shadow_list if isinstance(x[2], str)):
        L.append(f'  .shadow.twin.{i} ({sym}) (NOLOAD) : {{ "{obj}"({name}) }}')
    scratch = [x for x in shadow_list if x[2] is None]
    if scratch:
        L.append(f"  .shadow.scratch 0x{SCRATCH:08x} (NOLOAD) : {{")
        L += [f'    "{obj}"({name})' for obj, name, *_ in scratch]
        L.append("  }")
    L.append(f"  .shadow.common 0x{SCRATCH - 0x1000000:08x} (NOLOAD) : {{ *(COMMON) *(.scommon) }}")
    return L


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True)


def records(build: str, all_c: bool, only: set | None = None,
            skip: set | None = None) -> tuple[list[dict], dict]:
    """(inventory, compile groups). With `all_c`, non-matching records with a source join
    their file's group, filtered by `only` and `skip`."""
    inv = json.loads((ROOT / f"config/inventory/{build}.json").read_text())["functions"]
    groups = collections.defaultdict(list)
    for f in inv:
        if f.get("source_file") and f["status"] == "matching":
            groups[(f["source_file"], f["match"]["compiler"], f["match"]["flags"])].append(f)
    if all_c:
        def rank(k):
            fl = shlex.split(k[2])
            return ("-G0" in fl, "-DVERSION_US" in fl, len(groups[k]))
        by_file = {k[0]: k for k in sorted(groups, key=rank)}
        for f in inv:
            if f.get("source_file") and f["status"] != "matching":
                addr = int(f["address"], 16)
                if (only is not None and addr not in only) or (skip and addr in skip):
                    continue
                m = f.get("match") or {}
                key = by_file.get(f["source_file"]) or (
                    f["source_file"], m.get("compiler") or DEFAULT_COMPILER[0],
                    m.get("flags") or DEFAULT_COMPILER[1])
                groups[key].append(f)
    return inv, groups


def all_symbols(build: str, inv: list[dict]) -> dict[str, int]:
    out = {f["name"]: int(f["address"], 16) for f in inv}
    out.update(build_symbols(build))
    return out


def link_object(job: tuple) -> str:
    """The object to link: relocations retargeted, shadowed sections' relocations removed."""
    obj, secs, dest, edits = job
    if not secs and not edits:
        return obj
    src = obj
    if edits:
        Path(dest).write_bytes(elf32.retarget(Path(obj).read_bytes(), edits, encode))
        src = dest
    if not secs:
        return dest
    # a shadow lies where the original has its contents, whatever the section's alignment says
    r = run([f"{BIN}objcopy", *[f"--remove-relocations={s}" for s in secs],
             *[f"--set-section-alignment={s}=1" for s in secs], src, dest])
    if r.returncode:
        raise RuntimeError(r.stderr)
    return dest


def build(a) -> dict:
    out = a.out
    work = out / "work"
    for d in ("lobj", "gen"):
        (work / d).mkdir(parents=True, exist_ok=True)
    reg = builds()
    b = reg[a.build]
    exe = ROOT / "game" / a.build / b["boot_executable"]
    orig = Original(exe)
    if hashlib.sha256(orig.blob).hexdigest() != b["executable_sha256"]:
        raise SystemExit(f"{exe} is not the recorded file")
    inv, groups = records(a.build, a.all_c, a.c_only, a.c_skip)
    symbols = all_symbols(a.build, inv)
    cands = {c["id"]: c for c in load_candidates(ROOT / "config/compilers.json", "ps2")}
    report: dict = {"build": a.build, "all_c": a.all_c,
                    "pads": {k if isinstance(k, str) else f"0x{k:08x}": v
                             for k, v in a.pads.items()}}

    a.objdir.mkdir(parents=True, exist_ok=True)
    jobs = [(src, cands[comp]["command"], flags, str(a.objdir / object_name(src, comp, flags)))
            for (src, comp, flags) in groups]
    with ProcessPoolExecutor(a.jobs) as ex:
        compiled = list(ex.map(compile_one, jobs, chunksize=8))
    failed = {f"{j[0]} {j[2]}": err for j, (_, err) in zip(jobs, compiled) if err}
    report["compile_failed"] = failed
    ajobs = [(obj, groups[k], orig, symbols)
             for k, (obj, err) in zip(groups, compiled) if not err]
    with ProcessPoolExecutor(a.jobs) as ex:
        analyses = list(ex.map(_analyze_job, ajobs, chunksize=16))
    report["analysis_errors"] = [x for x in analyses if "error" in x]
    analyses = [x for x in analyses if "error" not in x]
    report["placement_problems"] = [p for x in analyses for p in x["problems"]]
    if a.all_c:
        analyses, report["reconstructed_units"] = split_reconstructed(analyses, work / "robj")
    lay = Layout(orig)
    report["text_problems"] = plan_text(lay, analyses, inv)
    report["matching_not_linked"] = not_linked(lay, analyses, inv)
    kept = {(c["obj"], c["section"]) for c in lay.chunks if c["kind"] == "obj"}
    symbolized, code_refs, code_suspects = code_chunks(lay)
    for name in DATA_SECTIONS:     # every data section moves with the label at its start
        lay.labels.add(orig.secs[name].addr)
    evidence, covered, ev_stats = data_evidence(
        lay, [x for x in analyses if not x.get("recon_copy")])
    known = set(symbols.values()) | {t for _, _, t, _ in code_refs} | set(evidence.values())
    for x in analyses:
        known |= {v for _, t, v in x["refs"] if t != R_MIPS_26}
    funcs = sorted((int(f["address"], 16), int(f["address"], 16) + f["size"]) for f in inv)
    known |= {a for a, _ in funcs}
    xb = xbuild.main_check(a, orig, inv, analyses, reg, Original) if xbuild else None
    cross = xb.cross if xb else None
    ptrs, data_suspects, ptr_stats = scan_data(lay, evidence, covered, known, funcs, cross)
    if a.all_c:
        report["into_reconstructed"] = into_reconstructed(lay, ptrs, code_refs)
    for _, _, t, _ in code_refs:
        lay.symbol(t)
    outside = {p: t for p, t in ptrs.items() if not orig.internal(t)}
    for p, t in outside.items():     # a relocated word of a unit whose original holds no address
        data_suspects.append((p, f"word 0x{ptrs.pop(p):08x}: relocated in the source, not an "
                                 "address in the original; left as is"))
    for t in ptrs.values():
        lay.symbol(t)
    real: list | None = [] if a.all_c else None
    shadow_list, sh_stats = shadows(lay, analyses, symbols, kept, real)
    defined = set().union(*(x["defined"] for x in analyses))
    wanted = set().union(*(set(x["undefined"]) | set(x["common"]) for x in analyses))
    values = collections.defaultdict(set)
    for x in analyses:
        for k, v in x["values"].items():
            values[k] |= set(v)
    aliases, unresolved = {}, []
    for n in sorted(wanted - defined - {"_gp"}):
        addr = resolve(n, symbols)
        if n not in symbols and len(values.get(n, ())) == 1:
            addr = next(iter(values[n]))       # matching code's relocations say where
        elif n not in symbols and xb:
            addr = xb.translate(n, addr)[1]
        if addr is None:
            unresolved.append(n)
            continue
        aliases[n] = lay.expr(addr)
    report["unresolved_symbols"] = unresolved
    if xb:
        xb.report(report)
    edits = {}
    for x in analyses:
        if x["retarget"]:
            ed = []
            for i, off, t, v in x["retarget"]:
                sym, add = lay.symbol(v)
                ed.append((i, off, R_MIPS_GPREL16 if t == R_MIPS_LITERAL else t, sym, add))
            edits[x["obj"]] = ed
    report["stats_retargeted"] = sum(map(len, edits.values()))
    # a label inside a pointer word (a misaligned target) keeps that word raw
    for lab in sorted(lay.labels):
        for p in (lab - 1, lab - 2, lab - 3):
            if p in ptrs:
                data_suspects.append((p, f"word 0x{ptrs.pop(p):08x}: a label lies inside it, "
                                         "left as is"))
    gen = work / "gen"
    emit_text(lay, symbolized, gen / "text.s")
    data_objs = {".text": str(gen / "text.o")}
    for name in DATA_SECTIONS:
        emit_data(lay, name, ptrs, gen / f"{name[1:]}.s", a.pads)
        data_objs[name] = str(gen / f"{name[1:]}.o")
    asm = [run([f"{BIN}as", "-EL", "-march=r5900", "-mabi=eabi", "-G0", "-no-pad-sections",
                str(Path(o).with_suffix(".s")), "-o", o]) for o in data_objs.values()]
    bad = [r.stderr[-2000:] for r in asm if r.returncode]
    if bad:
        raise SystemExit("assembler failed:\n" + "\n".join(bad))
    strip = collections.defaultdict(list)
    for obj, name, *_ in shadow_list:
        strip[obj].append(name)
    objs = sorted({x["obj"] for x in analyses})
    with ProcessPoolExecutor(a.jobs) as ex:
        linked_objs = dict(zip(objs, ex.map(link_object, [
            (o, strip.get(o, []), str(work / "lobj" / Path(o).name), edits.get(o, []))
            for o in objs], chunksize=16)))
    for c in lay.chunks:
        if c["kind"] == "obj":
            c["obj"] = linked_objs[c["obj"]]
    shadow_list = [(linked_objs[o], *rest) for o, *rest in shadow_list]
    if real is not None:
        real = [(linked_objs[o], *rest) for o, *rest in real]
    script = linker_script(lay, data_objs, shadow_list, aliases, a.pads,
                           orig.gp - orig.secs[".lit4"].addr, real)
    (work / "main.ld").write_text(script)
    elf_path = work / "main.elf"
    ld = run([f"{BIN}ld", "-EL", "-T", work / "main.ld", "-o", elf_path, "--no-check-sections",
              "-z", "muldefs", "-Map", work / "main.map",
              *data_objs.values(),
              *sorted(set(linked_objs.values()))])
    report["ld"] = ld.stderr[-4000:]
    report.update(stats={
        "text_chunks": len(lay.chunks),
        "compiled_chunks": sum(c["kind"] == "obj" for c in lay.chunks),
        "compiled_bytes": sum(c["size"] for c in lay.chunks if c["kind"] == "obj"),
        "extracted_bytes": sum(c["size"] for c in lay.chunks if c["kind"] == "asm"),
        "code_relocations": len(code_refs), "data_pointers": len(ptrs),
        "labels": len(lay.labels), "aliases": len(aliases),
        "shadows": dict(sh_stats), "data_evidence": dict(ev_stats),
        "pointer_scan": dict(ptr_stats)})
    report["ld_errors"] = sorted({ln.split(": ", 1)[-1] for ln in ld.stderr.splitlines()
                                  if "undefined reference" in ln or "truncated" in ln
                                  or "exceeds" in ln or "error" in ln.lower()})
    if ld.returncode or not elf_path.is_file():
        report["ok"] = False
        return report
    from . import pack, verify
    le = elf32.read(elf_path.read_bytes())
    linked = {}
    for name in pack.LOADED + (".sbss", ".bss", ".vubss"):
        s = le.section(name)
        if s is None:       # empty sections ld drops: where they would be
            prev = linked[{".vudata": ".data", ".vubss": ".bss"}[name]]
            linked[name] = (prev[0] + prev[1], 0, b"")
        else:
            linked[name] = (s.addr, s.size, None if s.type == elf32.SHT_NOBITS else le.data(s))
    syms = {s.name: s.value for s in le.symbols if s.name}
    move = verify.mapper(verify.anchors(syms))
    blob = pack.pack(orig.blob, linked, syms["_gp"], move(orig.elf.entry))
    dest = out / b["boot_executable"]
    dest.write_bytes(blob)
    got = hashlib.sha256(blob).hexdigest()
    report.update(file=str(dest), sha256=got, expected=b["executable_sha256"],
                  sha256_match=got == b["executable_sha256"], ok=True)
    report["verify"] = verify.check(orig, lay, linked, syms, code_refs, ptrs)
    report["hard_coded"] = sorted({f"0x{x:08x}: {why}" for x, why in code_suspects + data_suspects}
                                  | set(report["verify"].pop("suspects")))
    return report


def parse_pads(items: list[str]) -> dict:
    out = {}
    for it in items or []:
        where, n = it.split(":")
        out[where if where.startswith(".") else int(where, 16)] = int(n, 0)
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        prog="relink.py",
        description="Link the main executable, and with --overlays the 29 overlays, from source "
                    "with ld.")
    ap.add_argument("--build", default="ps2-us-1.01", help=argparse.SUPPRESS)
    ap.add_argument("--out", type=Path, default=None,
                    help="output directory (default build/relink/<build>/exact)")
    ap.add_argument("--objdir", type=Path, default=None,
                    help="where compiled objects are kept (shared between runs)")
    ap.add_argument("--pad", action="append", metavar="ADDR:N",
                    help="insert N zero bytes after the text chunk ending at ADDR, before the data "
                         "label at ADDR, or at the start of a data section (.data:N)")
    ap.add_argument("--all-c", action="store_true",
                    help="also link the C of every non-matching record instead of its bytes")
    ap.add_argument("--c-only", metavar="ADDR,...",
                    help="--all-c: only these non-matching functions from C (to bisect a fault)")
    ap.add_argument("--c-skip", metavar="ADDR,...",
                    help="--all-c: keep these non-matching functions' original bytes")
    if xbuild:
        xbuild.add_args(ap)
    ap.add_argument("--overlays", nargs="*", metavar="MODULE",
                    help="also link the overlays (all, or these modules) against this main link; "
                         "output in <out>/overlays/")
    ap.add_argument("--skip-main", action="store_true",
                    help="with --overlays: reuse the main link already in --out")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4,
                    help="parallel jobs (default: CPU count)")
    a = ap.parse_args(argv)
    a.pads = parse_pads(a.pad)
    for k in ("c_only", "c_skip"):
        v = getattr(a, k)
        setattr(a, k, {int(x, 16) for x in v.split(",") if x} if v is not None else None)
    a.all_c = a.all_c or a.c_only is not None or a.c_skip is not None
    tag = "allc" if a.all_c else ("pad" if a.pads else "exact")
    a.out = (a.out or ROOT / "build/relink" / a.build / tag).resolve()
    a.objdir = (a.objdir or ROOT / "build/relink" / a.build / "obj").resolve()
    a.out.mkdir(parents=True, exist_ok=True)
    if a.skip_main:
        if a.overlays is None:
            raise SystemExit("--skip-main needs --overlays")
        return overlays(a)
    report = build(a)
    (a.out / "report.json").write_text(json.dumps(report, indent=1) + "\n")
    st = report.get("stats", {})
    print(f"compiled {st.get('compiled_chunks')} chunks ({st.get('compiled_bytes')} bytes), "
          f"extracted {st.get('extracted_bytes')} bytes; {len(report['compile_failed'])} compile "
          f"failures, {len(report['unresolved_symbols'])} unresolved symbols, "
          f"{len(report['text_problems'])} placement problems, "
          f"{len(report['matching_not_linked'])} matching functions not linked from source")
    for x in report["matching_not_linked"][:20]:
        print(f"  not linked from source: {x}", file=sys.stderr)
    if not report.get("ok"):
        print("link FAILED:\n" + report.get("ld", ""), file=sys.stderr)
        return 1
    v = report["verify"]
    print(f"sha256 {'MATCH' if report['sha256_match'] else 'DIFFERS'} ({report['file']})")
    print(f"references checked {v['checked']}, not following their target {len(v['wrong'])}; "
          f"hard-coded addresses left {len(report['hard_coded'])}")
    rc = 0 if (report["sha256_match"] or a.pads or a.all_c) and not v["wrong"] \
        and not report["matching_not_linked"] else 1
    if a.overlays is not None:
        rc = overlays(a) or rc
    return rc


def overlays(a) -> int:
    from .overlay import MainLink, build_all
    ml = MainLink(a.build, a.out, a.out / "work" / "main.elf")
    summary = build_all(a, ml, a.overlays or None)
    bad = [m for m, r in summary.items() if not r["ok"] or r["wrong"] or r["not_linked"]
           or not (r["sha256_match"] or a.pads or a.all_c)]
    print(f"overlays: {sum(bool(r['sha256_match']) for r in summary.values())}/{len(summary)} "
          f"sha256 match, {sum(r['checked'] or 0 for r in summary.values())} references checked, "
          f"{sum(r['wrong'] or 0 for r in summary.values())} not following their target"
          + (f"; failed: {', '.join(bad)}" if bad else ""))
    return 1 if bad else 0
