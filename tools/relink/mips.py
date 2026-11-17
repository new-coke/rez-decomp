"""Find the address references in raw EE (MIPS R5900) code: j/jal, lui with its %lo users,
gp-relative accesses and branches out of the chunk, each to be emitted as a relocation."""

from __future__ import annotations

from dataclasses import dataclass

R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16, R_MIPS_PC16 = 2, 4, 5, 6, 7, 10

# I-type opcodes whose 16-bit immediate is an address offset from rs (the %lo users)
LO_OPS = {9, 25, 26, 27, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
          49, 53, 54, 55, 57, 61, 62, 63}
# I-type opcodes that write rt
WRITES_RT = {8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 30, 32, 33, 34, 35, 36, 37, 38, 39, 55}
BRANCH_OPS = {4, 5, 6, 7, 20, 21, 22, 23}
REGIMM_BRANCH = {0, 1, 2, 3, 16, 17, 18, 19}
# registers a call may change (at, v0-v1, a0-a7, t0-t3, t8-t9, ra): a lui's value in one of them
# does not survive a jal/jalr, so a later use of the register is not its %lo user
CALL_CLOBBERED = {1, *range(2, 16), 24, 25, 31}
# a lui's value spilled to off($sp) is followed as SLOT + off until the slot is overwritten
SLOT = 0x10000
# sq, sb, sh, swl, sw, sdl, sdr, swr, swc1, sqc2, sd
STORE_OPS = {31, 40, 41, 42, 43, 44, 45, 46, 57, 62, 63}


@dataclass
class Ref:
    index: int          # word index in the chunk
    type: int           # relocation type
    target: int         # the address the original encodes
    pair: int | None = None     # for a %lo: the index of its lui


def sext16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


def branch_target(w: int, pc: int) -> int | None:
    """The target of a conditional or unconditional PC-relative branch, else None."""
    op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
    if op in BRANCH_OPS or (op == 1 and rt in REGIMM_BRANCH) or (op in (16, 17, 18) and rs == 8):
        return (pc + 4 + (sext16(w & 0xFFFF) << 2)) & 0xFFFFFFFF
    return None


def written_gpr(w: int) -> int | None:
    """The general register an instruction writes (None for none or an FPU/VU register)."""
    op, rs, rt, rd = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    if op == 0:
        funct = w & 63
        if funct in (8, 12, 13, 15) or (24 <= funct <= 27 and rd == 0):
            return None     # jr, syscall, break, sync; mult/div without a destination
        return rd
    if op == 28:        # MMI
        return rd
    if op in WRITES_RT:
        return rt
    if op == 3:
        return 31
    if op == 1 and rt in (16, 17, 18, 19):
        return 31
    if op in (16, 17, 18) and rs in (0, 1, 2):
        return rt       # mfc/dmfc/cfc (qmfc2 for COP2)
    return None


def is_likely(w: int) -> bool:
    """beql/bnel/blezl/bgtzl, bltzl/bgezl(al), bc0/1/2 f/t likely."""
    op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
    return (op in (20, 21, 22, 23) or (op == 1 and rt in (2, 3, 18, 19))
            or (op in (16, 17, 18) and rs == 8 and rt & 2 != 0))


def is_call(w: int) -> bool:
    """jal, jalr, bal/bgezal-type and their likely forms."""
    op = w >> 26
    return op == 3 or (op == 0 and w & 63 == 9) or (op == 1 and (w >> 16) & 31 in (16, 17, 18, 19))


def copy_of(w: int, r: int) -> int | None:
    """The register a `move` of r, or an addu of r and another register, writes."""
    if w >> 26 != 0 or w & 63 not in (0x21, 0x25, 0x2D):
        return None
    rs, rt, rd = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    if not rd or (rs != r and rt != r) or rs == rt:
        return None
    if w & 63 == 0x25 and (rs if rt == r else rt) != 0:
        return None                     # or with a non-zero register is not an address
    return rd


def _successors(words: list[int], j: int, base: int) -> list[int]:
    """Indices that can execute after instruction j, delay slots included."""
    if j > 0:
        p = words[j - 1]
        pop, pfunct = p >> 26, p & 63
        bt = branch_target(p, base + 4 * (j - 1))
        if bt is not None:
            out = [(bt - base) // 4]
            # `b` (beq $0,$0) never falls through; a likely branch's delay slot runs only when
            # the branch is taken
            if not (pop == 4 and (p >> 16) & 0x3FF == 0) and not is_likely(p):
                out.append(j + 1)
            return out
        if pop == 2:                                          # j
            return [((((base + 4 * j) & 0xF0000000) | ((p & 0x3FFFFFF) << 2)) - base) // 4]
        if pop == 0 and pfunct == 8:                          # jr
            return []
    if j < len(words) and is_likely(words[j]):
        return [j + 1, j + 2]           # the delay slot (taken) or past it (not taken)
    return [j + 1]


def _is_switch(w: int) -> bool:
    """`jr rX` with rX other than $ra: a jump through a table (gcc's switch)."""
    return w >> 26 == 0 and w & 63 == 8 and (w >> 21) & 31 != 31


def _case_starts(words: list[int], base: int) -> list[int]:
    """Instructions that control flow from the chunk's start does not reach and no other
    unreached code leads to: switch cases and dead padding."""
    seen: set = set()
    todo = [0]
    while todo:
        j = todo.pop()
        if j in seen or not 0 <= j < len(words):
            continue
        seen.add(j)
        todo.extend(_successors(words, j, base))
    entered = {k for i in range(len(words)) if i not in seen
               for k in _successors(words, i, base)}
    return [j for j in range(len(words)) if j not in seen and j not in entered]


def symbolize(words: list[int], base: int, internal, gp: int, chunk_end: int | None = None,
              limit: int = 4096):
    """(refs, suspects) for the code words at `base`. `internal(addr)` says whether an address
    lies in the image; `chunk_end` bounds the chunk, for cross-chunk branches.

    Each lui is followed along every path (delay slots, copies, stack spills, switch jumps)
    until its register is rewritten; every instruction that uses it as a base is a %lo user.
    gcc hoists luis into saved registers and delay slots, so a linear scan misses pairs."""
    n = len(words)
    end = chunk_end if chunk_end is not None else base + 4 * n
    cases = None

    def successors(j: int) -> list[int]:
        # a switch's `jr` continues at its cases, which a value live across it reaches
        nonlocal cases
        if j > 0 and _is_switch(words[j - 1]):
            if cases is None:
                cases = _case_starts(words, base)
            return cases
        return _successors(words, j, base)

    refs: list[Ref] = []
    suspects: list[tuple[int, str]] = []
    lo_seen: dict[int, tuple[int, int]] = {}      # user index -> (target, lui index)
    for i, w in enumerate(words):
        pc = base + 4 * i
        op, rs = w >> 26, (w >> 21) & 31
        if op in (2, 3):
            t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if internal(t):
                refs.append(Ref(i, R_MIPS_26, t))
        bt = branch_target(w, pc)
        if bt is not None and not base <= bt < end:
            refs.append(Ref(i, R_MIPS_PC16, bt))
        if op in LO_OPS and rs == 28:
            t = (gp + sext16(w & 0xFFFF)) & 0xFFFFFFFF
            if internal(t):
                refs.append(Ref(i, R_MIPS_GPREL16, t))
    for i, w in enumerate(words):
        if w >> 26 != 15 or not (w >> 16) & 31:
            continue
        reg, himm = (w >> 16) & 31, w & 0xFFFF
        first, const = None, None
        # (index, register), copies and indexed sums included
        todo = [(j, reg) for j in successors(i)]
        seen: set = set()
        while todo and len(seen) < limit:
            j, r = todo.pop()
            if (j, r) in seen or not 0 <= j < n:
                continue
            seen.add((j, r))
            u = words[j]
            uop, urs, urt = u >> 26, (u >> 21) & 31, (u >> 16) & 31
            nxt = successors(j)
            if r >= SLOT:                   # the value sits in a stack slot
                if urs == 29 and u & 0xFFFF == r - SLOT:
                    if uop in STORE_OPS:
                        continue            # the slot is overwritten
                    if uop in (30, 35, 55) and urt:     # lq/lw/ld: reloaded into a register
                        todo.extend((k, urt) for k in nxt)
                todo.extend((k, r) for k in nxt)
                continue
            if uop in (31, 43, 63) and urs == 29 and urt == r:      # sq/sw/sd r, off($sp)
                todo.extend((k, SLOT + (u & 0xFFFF)) for k in nxt)
            if uop == 15 and urt == r:
                continue                    # another lui of the register takes over
            if uop in LO_OPS and urs == r:
                t = ((himm << 16) + sext16(u & 0xFFFF)) & 0xFFFFFFFF
                if internal(t):
                    if lo_seen.setdefault(j, (t, i))[0] != t:
                        suspects.append((base + 4 * j, "%lo user of luis with different %hi"))
                    if first is None or j < first[0]:
                        first = (j, t)
            elif uop == 13 and urs == r and internal((himm << 16) | (u & 0xFFFF)):
                const = (himm << 16) | (u & 0xFFFF)
            call = j > 0 and is_call(words[j - 1])      # j is a call's delay slot
            dst = copy_of(u, r)
            if dst is not None and dst != r and not (call and dst in CALL_CLOBBERED):
                todo.extend((k, dst) for k in nxt)
            if written_gpr(u) == r and dst != r:
                continue
            if call and r in CALL_CLOBBERED:
                continue                    # the call clobbers r
            todo.extend((k, r) for k in nxt)
        if first is not None:
            refs.append(Ref(i, R_MIPS_HI16, first[1]))
        elif const is not None:
            suspects.append((base + 4 * i, f"lui/ori constant 0x{const:08x}"))
        else:
            v = himm << 16
            if internal(v) or internal(v - 0x8000) or internal(v + 0x7FFF):
                suspects.append((base + 4 * i, f"lui 0x{himm:04x} without %lo user"))
    for j, (t, li) in lo_seen.items():
        refs.append(Ref(j, R_MIPS_LO16, t, li))
    refs.sort(key=lambda r: (r.index, r.type))
    return refs, suspects


def encode(word: int, rtype: int, addend: int) -> int:
    """The word with its relocation field holding `addend` the way a REL object stores it."""
    if rtype == R_MIPS_26:
        return (word & 0xFC000000) | ((addend >> 2) & 0x3FFFFFF)
    if rtype == R_MIPS_HI16:
        return (word & 0xFFFF0000) | (((addend + 0x8000) >> 16) & 0xFFFF)
    if rtype in (R_MIPS_LO16, R_MIPS_GPREL16):
        return (word & 0xFFFF0000) | (addend & 0xFFFF)
    if rtype == R_MIPS_PC16:
        return (word & 0xFFFF0000) | (((addend - 4) >> 2) & 0xFFFF)
    if rtype == R_MIPS_32:
        return addend & 0xFFFFFFFF
    raise ValueError(f"relocation type {rtype}")


RELOC_NAMES = {R_MIPS_32: "R_MIPS_32", R_MIPS_26: "R_MIPS_26", R_MIPS_HI16: "R_MIPS_HI16",
               R_MIPS_LO16: "R_MIPS_LO16", R_MIPS_GPREL16: "R_MIPS_GPREL16",
               R_MIPS_PC16: "R_MIPS_PC16"}
