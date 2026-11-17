"""Check that every reference in a link follows its target, and list the address-like words
that stay constant."""

from __future__ import annotations

import bisect
import struct
from pathlib import Path

from objmatch.relocs import R_MIPS_26, decode

from . import elf32, mips
from .main import rel_dicts, reginfo_gp

LABEL = "rz_"


def anchors(syms: dict[str, int], prefix: str = LABEL) -> list[tuple[int, int]]:
    """(original address, linked address) of every `<prefix><8 hex digits>` label, sorted."""
    out = []
    for n, v in syms.items():
        if n.startswith(prefix) and len(n) == len(prefix) + 8:
            try:
                out.append((int(n[3:], 16), v))
            except ValueError:
                pass
    return sorted(out)


def mapper(anch: list[tuple[int, int]]):
    """Original address -> linked: the move of the nearest anchor at or below it."""
    olds = [a for a, _ in anch]

    def move(a: int) -> int:
        i = bisect.bisect_right(olds, a) - 1
        if i < 0:
            return a
        return anch[i][1] + a - anch[i][0]
    return move


class Image:
    def __init__(self, linked: dict):
        self.secs = sorted((a, size, data) for a, size, data in linked.values() if data)

    def read(self, a: int, n: int) -> bytes:
        for b, size, data in self.secs:
            if b <= a and a + n <= b + size:
                return data[a - b:a - b + n]
        raise ValueError(f"0x{a:08x} not in the linked image")

    def word(self, a: int) -> int:
        return struct.unpack("<I", self.read(a, 4))[0]


def check(orig, lay, linked, syms, code_refs, ptrs, move=None) -> dict:
    """Decode every reference in the original and the linked image; report those whose new
    value is not the move of the old, and unrelocated words that look like addresses."""
    move = move or mapper(anchors(syms))
    new = Image(linked)
    gp = syms["_gp"]
    checked, wrong, suspects = 0, [], []

    def expect(kind: str, site: int, old: int, got: int, is_gp: bool = False):
        nonlocal checked
        checked += 1
        want = gp if is_gp else move(old)     # `_gp` moves with .lit4, not with what is there
        if orig.internal(old) and want != got:
            wrong.append(f"0x{site:08x} {kind}: 0x{old:08x} should become 0x{want:08x}, "
                         f"is 0x{got:08x}")

    cache: dict[str, elf32.Elf] = {}
    for c in lay.chunks:
        if c["kind"] != "obj" or not c.get("matching", True):
            continue
        if c["obj"] not in cache:
            cache[c["obj"]] = elf32.read(Path(c["obj"]).read_bytes())
        e = cache[c["obj"]]
        sec = e.section(c["section"])
        data = e.data(sec)
        rels = rel_dicts(e, sec)
        at = syms[c["label"]]
        relocated = {r["offset"] & ~3 for r in rels}
        for i in range(0, len(data), 4):
            if i in relocated:
                continue
            w = struct.unpack_from("<I", data, i)[0]
            ow = orig.word(c["start"] + i)
            if ow >> 26 == 15 and orig.internal((ow & 0xFFFF) << 16):
                suspects.append((c["start"] + i, f"lui 0x{ow & 0xFFFF:04x} in compiled code "
                                                 "without relocation"))
            elif ow >> 26 in (2, 3) and w == ow:
                suspects.append((c["start"] + i, "jump in compiled code without relocation"))
        old = list(decode(rels, orig.read(c["start"], len(data)), data, "MIPS", False,
                          (orig.gp, reginfo_gp(e))))
        nw = list(decode(rels, new.read(at, len(data)), data, "MIPS", False,
                         (gp, reginfo_gp(e))))
        for (rel, ov, _), (_, nv, _) in zip(old, nw):
            if ov is None or nv is None:
                continue
            site = c["start"] + rel["offset"]
            if rel["type"] == R_MIPS_26:
                ov |= (site + 4) & 0xF0000000
                nv |= (at + rel["offset"] + 4) & 0xF0000000
            expect(f"compiled {mips.RELOC_NAMES.get(rel['type'], rel['type'])}", site,
                   ov & 0xFFFFFFFF, nv & 0xFFFFFFFF, rel["symbol"] == "_gp")
    for site, rtype, target, lui in code_refs:
        ns = move(site)
        w = new.word(ns)
        if rtype == mips.R_MIPS_26:
            got = ((ns + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
        elif rtype == mips.R_MIPS_PC16:
            got = ns + 4 + (mips.sext16(w & 0xFFFF) << 2)
        elif rtype == mips.R_MIPS_GPREL16:
            got = gp + mips.sext16(w & 0xFFFF)
        elif rtype == mips.R_MIPS_LO16 and lui is not None:
            got = ((new.word(move(lui)) & 0xFFFF) << 16) + mips.sext16(w & 0xFFFF)
        else:
            continue
        expect(f"extracted {mips.RELOC_NAMES[rtype]}", site, target, got & 0xFFFFFFFF)
    for site, target in ptrs.items():
        expect("data pointer", site, target, new.word(move(site)))
    return {"checked": checked, "wrong": wrong[:2000], "wrong_count": len(wrong),
            "suspects": [f"0x{a:08x}: {why}" for a, why in suspects]}
