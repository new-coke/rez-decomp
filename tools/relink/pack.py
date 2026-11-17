"""Pack ld's output sections into a file laid out like the original PS2 executable, with the
original as the template for the headers."""

from __future__ import annotations

import struct

from . import elf32

LOADED = (".text", ".vutext", ".data", ".vudata", ".rodata", ".lit4", ".sdata")


def pack(orig: bytes, linked: dict[str, tuple[int, int, bytes | None]], gp: int,
         entry: int) -> bytes:
    """`linked` maps each section of the original's image (.text ... .vubss) to (address, size,
    contents or None for NOBITS)."""
    oe = elf32.read(orig)
    osec = {s.name: s for s in oe.sections if s.name}
    phoff, = struct.unpack_from("<I", orig, 28)
    p_type, p_off = struct.unpack_from("<II", orig, phoff)
    assert p_type == 1 and struct.unpack_from("<H", orig, 44)[0] == 1, "expects one PT_LOAD"
    text = linked[".text"][0]
    img_end = linked[".sdata"][0] + linked[".sdata"][1]
    img = bytearray(img_end - text)
    for n in LOADED:
        a, size, data = linked[n]
        if data:
            img[a - text:a - text + size] = data
    o_text = osec[".text"].addr
    o_img_end_off = p_off + osec[".sdata"].addr + osec[".sdata"].size - o_text
    delta = p_off + len(img) - o_img_end_off
    if delta % 4:
        img += bytes(4 - delta % 4)
        delta += 4 - delta % 4
    out = bytearray(orig[:p_off]) + img + bytearray(orig[o_img_end_off:])
    bss_end = linked[".bss"][0] + linked[".bss"][1]

    def tail(off: int) -> int:
        return off - o_img_end_off + p_off + len(img)

    struct.pack_into("<I", out, 24, entry)
    shoff = struct.unpack_from("<I", orig, 32)[0]
    struct.pack_into("<I", out, 32, tail(shoff))
    struct.pack_into("<IIIII", out, phoff + 4, p_off, text, text, len(img), bss_end - text)
    vutext = linked[".vutext"]
    for s in oe.sections:
        h = tail(s.header_offset)
        addr, off, size = s.addr, s.offset, s.size
        if s.name in linked and s.name in LOADED + (".sbss",):
            addr, size = linked[s.name][:2]
            off = p_off + addr - text
        elif s.name in (".bss", ".vubss"):
            addr, size = linked[s.name][:2]
            off = tail(s.offset)
        elif s.name == ".reginfo":
            addr, off = vutext[0] + vutext[1], tail(s.offset)
            struct.pack_into("<I", out, off + 20, gp)
        elif s.name.startswith(".DVP.ovlytab") or s.name.startswith(".DVP.ovlystrtab"):
            addr, off = bss_end, tail(s.offset)
        elif s.offset >= o_img_end_off:
            off = tail(s.offset)
        struct.pack_into("<III", out, h + 12, addr, off, size)
    # VU overlay table: (name address, copy address in .vutext, VU address) per entry
    tab, strtab = osec.get(".DVP.ovlytab"), osec.get(".DVP.ovlystrtab")
    if tab is not None and strtab is not None:
        for i in range(tab.size // 12):
            o = tail(tab.offset) + 12 * i
            name, lma = struct.unpack_from("<II", out, o)
            struct.pack_into("<II", out, o, bss_end + name - strtab.addr,
                             vutext[0] + lma - osec[".vutext"].addr)
    return bytes(out)
