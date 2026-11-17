"""A minimal little-endian ELF32 reader: sections, symbols (with their table index) and REL
relocations, enough for MIPS relocatable objects and the PS2 executable."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

SHT_SYMTAB, SHT_STRTAB, SHT_REL, SHT_NOBITS = 2, 3, 9, 8
SHN_UNDEF, SHN_ABS, SHN_COMMON = 0, 0xFFF1, 0xFFF2
SHN_MIPS_SCOMMON = 0xFF03
STT_SECTION, STT_FUNC = 3, 2


@dataclass
class Section:
    index: int
    name: str
    type: int
    flags: int
    addr: int
    offset: int
    size: int
    link: int
    info: int
    align: int
    entsize: int
    header_offset: int


@dataclass
class Symbol:
    index: int
    name: str
    value: int
    size: int
    bind: int
    type: int
    shndx: int


@dataclass
class Reloc:
    offset: int
    type: int
    sym: int


@dataclass
class Elf:
    blob: bytes
    sections: list[Section]
    symbols: list[Symbol] = field(default_factory=list)
    relocs: dict[int, list[Reloc]] = field(default_factory=dict)  # by target section index
    entry: int = 0

    def section(self, name: str) -> Section | None:
        return next((s for s in self.sections if s.name == name), None)

    def data(self, sec: Section) -> bytes:
        if sec.type == SHT_NOBITS:
            return bytes(sec.size)
        return self.blob[sec.offset:sec.offset + sec.size]

    def symbol_name(self, index: int) -> str:
        """A symbol's name; a section symbol is named after its section."""
        s = self.symbols[index]
        if s.type == STT_SECTION and s.shndx < len(self.sections):
            return self.sections[s.shndx].name
        return s.name


def _cstr(blob: bytes, off: int) -> str:
    return blob[off:blob.index(b"\0", off)].decode("latin-1")


def read(blob: bytes) -> Elf:
    """Parse a little-endian ELF32 file."""
    if blob[:4] != b"\x7fELF" or blob[4] != 1 or blob[5] != 1:
        raise ValueError("not a little-endian ELF32 file")
    entry, _phoff, shoff = struct.unpack_from("<III", blob, 24)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", blob, 46)
    raw = []
    for i in range(shnum):
        o = shoff + i * shentsize
        raw.append((o, struct.unpack_from("<IIIIIIIIII", blob, o)))
    stroff = raw[shstrndx][1][4] if shnum else 0
    secs = [Section(i, _cstr(blob, stroff + r[0]), r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
                    r[9], o) for i, (o, r) in enumerate(raw)]
    elf = Elf(blob, secs, entry=entry)
    symtab = next((s for s in secs if s.type == SHT_SYMTAB), None)
    if symtab:
        strtab = secs[symtab.link]
        for i in range(symtab.size // 16):
            name, value, size, info, _other, shndx = struct.unpack_from(
                "<IIIBBH", blob, symtab.offset + i * 16)
            elf.symbols.append(Symbol(i, _cstr(blob, strtab.offset + name), value, size,
                                      info >> 4, info & 15, shndx))
    for s in secs:
        if s.type == SHT_REL:
            elf.relocs[s.info] = [Reloc(*_rel(blob, s.offset + i * 8))
                                  for i in range(s.size // 8)]
    return elf


def _rel(blob: bytes, off: int) -> tuple[int, int, int]:
    r_offset, r_info = struct.unpack_from("<II", blob, off)
    return r_offset, r_info & 0xFF, r_info >> 8


def retarget(blob: bytes, edits: list[tuple[int, int, int, str, int]], encode) -> bytes:
    """A copy of a relocatable object where each edit (section, offset, type, symbol, addend)
    names a new undefined global. The new symbol and string tables are appended; nothing
    else moves."""
    e = read(blob)
    out = bytearray(blob)
    symtab = next(s for s in e.sections if s.type == SHT_SYMTAB)
    strtab = e.sections[symtab.link]
    strs = bytearray(e.data(strtab))
    syms = bytearray(e.data(symtab))
    index: dict[str, int] = {}
    for name in sorted({x[3] for x in edits}):
        index[name] = len(syms) // 16
        syms += struct.pack("<IIIBBH", len(strs), 0, 0, (1 << 4) | 0, 0, SHN_UNDEF)
        strs += name.encode() + b"\0"
    want = {(sec, off): (t, name, add) for sec, off, t, name, add in edits}
    for s in e.sections:
        if s.type != SHT_REL:
            continue
        for i in range(s.size // 8):
            o = s.offset + 8 * i
            r_offset, r_info = struct.unpack_from("<II", out, o)
            hit = want.get((s.info, r_offset))
            if hit is None:
                continue
            t, name, add = hit
            struct.pack_into("<II", out, o, r_offset, (index[name] << 8) | t)
            target = e.sections[s.info]
            w = struct.unpack_from("<I", out, target.offset + (r_offset & ~3))[0]
            struct.pack_into("<I", out, target.offset + (r_offset & ~3), encode(w, t, add))
    for sec, data in ((symtab, syms), (strtab, strs)):
        while len(out) % 4:
            out.append(0)
        struct.pack_into("<II", out, sec.header_offset + 16, len(out), len(data))
        out += data
    return bytes(out)
