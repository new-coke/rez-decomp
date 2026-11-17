"""Decode the address each relocation of an object encodes in the original, and check it
against the symbol or section bytes it names."""

from __future__ import annotations

import collections
import re
import struct
from pathlib import Path
from typing import Callable

# An overlay's own symbols carry its module as a prefix with a double underscore
# (segalogo__func_00af4b80), because overlays share load addresses.
ADDR_NAME = re.compile(r"^(?:[a-z0-9_]+?__)?(?:FUN|func|sub|D|DAT|jtbl|PTR)_([0-9a-fA-F]{8})$")
R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16, R_MIPS_LITERAL = 2, 4, 5, 6, 7, 8
R_SH_DIR32 = 1


def load_symbol_file(path: str | Path) -> dict[str, int]:
    """{name: address} from a splat-style symbol file (`name = 0xADDR;` lines)."""
    out = {}
    for line in Path(path).read_text().splitlines():
        m = re.match(r"\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def resolve(name: str, symbols: dict[str, int]) -> int | None:
    """A symbol's address: from `symbols`, else from an address-encoded name, else None."""
    if name in symbols:
        return symbols[name]
    m = ADDR_NAME.match(name)
    return int(m.group(1), 16) if m else None


def _sext16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


def reginfo_gp(blob: bytes, info: dict) -> int | None:
    """The gp value an ELF records in .reginfo (ri_gp_value), or None without .reginfo."""
    sec = next((s for s in info["sections"] if s["name"] == ".reginfo"), None)
    if sec is None or sec["size"] < 24:
        return None
    fmt = ">I" if info.get("endianness") == "big" else "<I"
    return struct.unpack_from(fmt, blob, sec["offset"] + 20)[0]


def decode(relocs, target, cand, arch, big, gp: tuple[int, int] | None = None):
    """Yields (reloc, encoded_target_value, object_addend) or (reloc, None, why).

    `gp` is (target gp, object gp0) for GPREL16 and LITERAL. Against a section symbol the
    assembler subtracted gp0 from the addend, so it is added back."""
    fmt = ">I" if big else "<I"

    def word(buf, off):
        return struct.unpack_from(fmt, buf, off)[0] if 0 <= off and off + 4 <= len(buf) else None

    # Keep relocation-table order: GNU as emits each HI16 right before its LO16, which is
    # what pairs them when several %hi/%lo of one section symbol are interleaved in the code.
    last_hi: dict[str, tuple[int, int]] = {}  # symbol -> (target hi, object hi)
    for i, r in enumerate(relocs):
        tw, cw = word(target, r["offset"]), word(cand, r["offset"])
        if tw is None or cw is None:
            yield r, None, "outside function"
            continue
        if arch == "MIPS":
            t = r["type"]
            if t == R_MIPS_26:
                yield r, (tw & 0x3FFFFFF) << 2, (cw & 0x3FFFFFF) << 2
            elif t == R_MIPS_HI16:
                last_hi[r["symbol"]] = (tw & 0xFFFF, cw & 0xFFFF)
                lo = next((x for x in relocs[i + 1:]
                           if x["type"] == R_MIPS_LO16 and x["symbol"] == r["symbol"]), None)
                if lo is None:          # a %hi moved after its %lo (e.g. into a delay slot)
                    lo = next((x for x in reversed(relocs[:i])
                               if x["type"] == R_MIPS_LO16 and x["symbol"] == r["symbol"]), None)
                if lo is None:
                    yield r, None, "HI16 without matching LO16"
                    continue
                tl, cl = word(target, lo["offset"]), word(cand, lo["offset"])
                if tl is None or cl is None:
                    yield r, None, "paired LO16 outside function"
                    continue
                yield r, ((tw & 0xFFFF) << 16) + _sext16(tl & 0xFFFF), \
                    ((cw & 0xFFFF) << 16) + _sext16(cl & 0xFFFF)
            elif t == R_MIPS_LO16:
                hi = last_hi.get(r["symbol"])
                if hi is None:          # its %hi comes later in the table
                    h = next((x for x in relocs[i + 1:]
                              if x["type"] == R_MIPS_HI16 and x["symbol"] == r["symbol"]), None)
                    th, ch = ((word(target, h["offset"]), word(cand, h["offset"])) if h
                              else (None, None))
                    if th is not None and ch is not None:
                        hi = (th & 0xFFFF, ch & 0xFFFF)
                if hi is None:
                    yield r, None, "LO16 without preceding HI16"
                    continue
                yield r, (hi[0] << 16) + _sext16(tw & 0xFFFF), (hi[1] << 16) + _sext16(cw & 0xFFFF)
            elif t == R_MIPS_32:
                yield r, tw, cw
            elif t in (R_MIPS_GPREL16, R_MIPS_LITERAL):
                if gp is None:
                    yield r, None, "gp-relative relocation without a known gp value"
                    continue
                gp0 = gp[1] if r["symbol"].startswith("<section ") else 0
                yield r, gp[0] + _sext16(tw & 0xFFFF), gp0 + _sext16(cw & 0xFFFF)
            else:
                yield r, None, f"relocation type {t} not checked"
        elif arch == "SuperH" and r["type"] == R_SH_DIR32:
            yield r, tw, (r.get("addend") or 0) + cw
        else:
            yield r, None, f"relocation type {r['type']} not checked"


# MIPS load opcodes of 4 bytes or more -> bytes loaded (lw, lwu, ld, lq, lwc1, ldc1, lqc2)
MIPS_LOAD_WIDTH = {0x23: 4, 0x27: 4, 0x37: 8, 0x1E: 16, 0x31: 4, 0x35: 8, 0x36: 16}


def _literal_length(data: bytes, start: int, starts=(), align: int = 8) -> int:
    """Length of the string literal at `start` with its terminator: it ends at a NUL followed
    by padding to the next `align` boundary, or at the next referenced offset (`starts`)."""
    i = start
    while True:
        nul = data.find(b"\0", i)
        if nul < 0 or nul + 1 >= len(data):
            return (len(data) if nul < 0 else nul + 1) - start
        end = min(-(-(nul + 1) // align) * align, len(data))
        nxt = nul + 1
        while nxt < end and data[nxt] == 0:
            nxt += 1
        if nxt == end and (end == len(data) or data[end] == 0 or end in starts or end > nul + 1):
            return nul + 1 - start
        if nxt == nul + 1 and nxt in starts:
            return nul + 1 - start
        i = nul + 1


def _printable(bs: bytes) -> bool:
    return all(32 <= c < 127 or c in (9, 10) for c in bs)


def _section_content_ok(data: bytes, addend: int, value: int, read_target, sec_relocs,
                        func_obj_offset: int, target_addr: int, symbols, big: bool,
                        section: str = "", depth: int = 2, starts=(), target_starts=(),
                        func_size: int | None = None, text_funcs: dict[int, str] | None = None,
                        width: int | None = None):
    """(ok, description): the object's section bytes at `addend` against the original's at
    `value`.

    Relocated words (jump tables, pointers in constant aggregates) are compared by the
    address they must hold: against .text relative to the function, against this section
    relative to `value`, against a named symbol through the symbol map. A run of relocated
    words is compared in full, else 16 bytes or a string. With func_size and text_funcs, a
    pointer into .text is this function's jump table, a function pointer, or another
    function's table, which is checked with that function."""
    fmt = ">I" if big else "<I"
    rel_at = {r["offset"]: r for r in (sec_relocs or [])}

    def text_kind(o: int) -> str | None:
        r = rel_at.get(o)
        if r is None or r["symbol"] != "<section .text>" or func_size is None:
            return None
        inplace = struct.unpack_from(fmt, data, o)[0]
        if func_obj_offset <= inplace < func_obj_offset + func_size:
            return "local"
        if inplace in (text_funcs or {}):
            return "func"
        return "foreign"

    first = text_kind(addend)
    n = 0                                      # a run of relocated words: a table
    while addend + 4 * n in rel_at and addend + 4 * n + 4 <= len(data):
        kind_n = text_kind(addend + 4 * n)
        if n and (kind_n == "foreign" or (first == "local" and kind_n != "local")):
            break
        n += 1
    limit = len(data)                          # never compare into another function's table
    for o in sorted(rel_at):
        if o >= addend + 4 * n and text_kind(o) == "foreign":
            limit = o
            break
    if n == 0:                                 # a constant ends where the next one starts
        later = [s for s in starts if s > addend]
        if later:
            limit = min(limit, min(later))
    length = max(4 * n, min(16, limit - addend))
    if n == 0 and width:                       # a constant read by a load: only what it loads
        length = min(length, width)
    kind = f"jump table of {n} entries" if n > 1 else None
    window = range(addend, addend + length)
    relocated = [o for o in rel_at if o in window or o + 3 in window]
    want = data[addend:addend + length]
    if not relocated and b"\0" in want and not width:   # strings: the whole literal, both sides
        want = data[addend:addend + min(_literal_length(data, addend, starts), limit - addend)]
        length = len(want)
        # text right after the terminator continues the literal (NUL-separated arguments)
        after = read_target(value + length, 16) or b""
        run = after.split(b"\0")[0]
        continues = len(run) >= 2 and b"\0" in after and _printable(run)
        if continues and len(want) > 1 and _printable(want[:-1]) and length % 8 and \
                value + length not in target_starts:
            return False, f"the original's literal continues after byte {length}"
    got = read_target(value, length)
    if got is None:
        return False, "outside the original"
    section_base = value - addend              # where this object section sits in the original
    for o in relocated:
        if o < addend or o + 4 > addend + length:
            return False, "relocation straddles the compared range"
        r = rel_at[o]
        if r["type"] != R_MIPS_32:
            return False, f"relocation type {r['type']} in data"
        inplace = struct.unpack_from(fmt, data, o)[0]
        named = (text_funcs or {}).get(inplace) if r["symbol"] == "<section .text>" else None
        if named and resolve(named, symbols) is not None and text_kind(o) == "func":
            expect = resolve(named, symbols)  # a function pointer: by name, like a call
        elif r["symbol"] == "<section .text>":
            expect = target_addr + inplace - func_obj_offset
        elif section and r["symbol"] == f"<section {section}>":
            expect = section_base + inplace
            # and what the pointer points to
            if depth > 0 and inplace < len(data):
                ok, what = _section_content_ok(data, inplace, expect, read_target, sec_relocs,
                                               func_obj_offset, target_addr, symbols, big,
                                               section, depth - 1, func_size=func_size,
                                               text_funcs=text_funcs)
                if not ok:
                    return False, f"pointee of word at +{o - addend}: {what}"
        else:
            sym = resolve(r["symbol"], symbols)
            if sym is None:
                return False, f"data relocation against {r['symbol']} not resolvable"
            expect = sym + inplace
        k = o - addend
        if struct.unpack_from(fmt, got, k)[0] != expect & 0xFFFFFFFF:
            return False, f"relocated word at +{k} differs"
    masked = set()
    for o in relocated:
        masked.update(range(o - addend, o - addend + 4))
    if any(got[i] != want[i] for i in range(length) if i not in masked):
        return False, "bytes differ"
    return True, kind or want.decode("latin-1", "replace")


def verify(relocs: list[dict], target: bytes, target_addr: int, cand: bytes, arch: str,
           big: bool, symbols: dict[str, int],
           read_target: Callable[[int, int], bytes | None] | None = None,
           obj_section: Callable[[str], bytes | None] | None = None,
           obj_section_relocs: Callable[[str], list[dict]] | None = None,
           func_obj_offset: int = 0, gp: tuple[int, int] | None = None,
           text_funcs: dict[int, str] | None = None,
           object_starts: dict[str, set[int]] | None = None) -> dict:
    """text_funcs maps .text offsets to the functions defined there, so calls into the
    object's own .text are checked by name. object_starts adds the section offsets the
    object's other functions reference."""
    checked, mismatched, unverified = [], [], []
    decoded = list(decode(relocs, target, cand, arch, big, gp))
    starts = collections.defaultdict(set)      # referenced offsets per section: literal starts
    for name, offs in (object_starts or {}).items():
        starts[name] |= offs
    tstarts = collections.defaultdict(set)     # the same references' addresses in the original
    for r, value, addend in decoded:
        if value is not None and r["symbol"].startswith("<section "):
            starts[r["symbol"]].add(addend)
            tstarts[r["symbol"]].add(value & 0xFFFFFFFF)
    # widths of the loads that read a section constant (MIPS %lo loads, 4 bytes or more); an
    # address taken (addiu) or a narrower access leaves the extent to the content rules
    widths: dict[tuple[str, int], int | None] = {}
    if arch == "MIPS":
        fmt = ">I" if big else "<I"
        for r, value, addend in decoded:
            if value is None or r["type"] != R_MIPS_LO16 or not r["symbol"].startswith("<section "):
                continue
            off = r["offset"] & ~3
            op = struct.unpack_from(fmt, cand, off)[0] >> 26 if off + 4 <= len(cand) else None
            w = MIPS_LOAD_WIDTH.get(op)
            key = (r["symbol"], addend)
            widths[key] = None if key in widths and widths[key] != w else w
    for r, value, addend in decoded:
        entry = {"offset": f"0x{target_addr + r['offset']:08x}", "symbol": r["symbol"],
                 "type": r["type"]}
        if value is None:
            entry["why"] = addend
            unverified.append(entry)
            continue
        value &= 0xFFFFFFFF
        entry["target_value"] = f"0x{value:08x}"
        sym = resolve(r["symbol"], symbols)
        if sym is not None:
            ok = value == ((sym + addend) & 0xFFFFFFFF)
            entry["expected"] = f"0x{(sym + addend) & 0xFFFFFFFF:08x}"
        elif r["symbol"] == "<section .text>":
            # a call or jump into this object's own .text (recursion, static functions)
            named = (text_funcs or {}).get(addend)
            at = resolve(named, symbols) if named else None
            if at is not None:
                expect = at
                entry["via"] = named
            else:
                expect = (target_addr - func_obj_offset + addend) & 0xFFFFFFFF
            ok = value == expect
            entry["expected"] = f"0x{expect:08x}"
        elif r["symbol"].startswith("<section ") and read_target and obj_section:
            data = obj_section(r["symbol"][9:-1])
            if not data or addend >= len(data):
                entry["why"] = "section content unavailable"
                unverified.append(entry)
                continue
            name = r["symbol"][9:-1]
            ok, what = _section_content_ok(
                data, addend, value, read_target,
                obj_section_relocs(name) if obj_section_relocs else None,
                func_obj_offset, target_addr, symbols, big, name, starts=starts[r["symbol"]],
                target_starts=tstarts[r["symbol"]], func_size=len(cand), text_funcs=text_funcs,
                width=widths.get((r["symbol"], addend)))
            entry["content"] = what
        else:
            entry["why"] = "symbol address unknown"
            unverified.append(entry)
            continue
        (checked if ok else mismatched).append(entry)
    return {"checked": len(checked), "mismatched": mismatched, "unverified": unverified}
