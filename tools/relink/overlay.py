"""Link the 29 code overlays with ld, each at its load address against the linked main
executable (see tools/README.md)."""

from __future__ import annotations

import bisect
import collections
import hashlib
import json
import struct
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from objmatch.probe import build_symbols, load_candidates
from objmatch.relocs import R_MIPS_26, R_MIPS_LITERAL, resolve

from . import elf32, mips, verify
from .main import (BIN, DISCARD, R_MIPS_GPREL16, ROOT, Layout, Original, _analyze_job, builds,
                   code_chunks, compile_one, data_evidence, emit_data, emit_text,
                   into_reconstructed, link_object, not_linked, object_name, plan_text, records,
                   run, scan_data, shadow_script, shadows, split_reconstructed, xbuild)

SECTIONS = (".head", ".text", ".data", ".bss")
SCAN = (".head", ".data")
PREFIX = "ro_"
# the room each load address has before the next buffer main's .bss holds there (main symbols
# gx_vertex_buffer at 0xbf6680 and heap_base_mem at 0xea0680)
REGION_END = {0x00AF6680: 0x00BF6680, 0x00CA0680: 0x00EA0680}


def registry(build: str) -> dict:
    reg = json.loads((ROOT / "config/overlays" / f"{build}.json").read_text())
    d = ROOT / reg.get("extract_dir", "build/overlays")
    return {o["module"]: dict(o, file=d / o["path"].split("/")[-1]) for o in reg["overlays"]}


class OverlayImage:
    """The main executable with the overlay's file at its load address. Sections `.head`,
    `.text`, `.data`, and a `.bss` from the end of the file to `bss_end`."""

    def __init__(self, main: Original, blob: bytes, load: int, code: tuple[int, int],
                 bss_end: int | None = None):
        self.main, self.blob, self.load = main, blob, load
        self.end = load + len(blob)
        self.bss_end = max(bss_end or 0, self.end)
        self.gp = main.gp
        self.lo, self.hi = load, self.bss_end
        bounds = [(".head", load, code[0], 3, 1), (".text", code[0], code[1], 6, 1),
                  (".data", code[1], self.end, 3, 1), (".bss", self.end, self.bss_end, 3,
                                                       elf32.SHT_NOBITS)]
        self.order = [elf32.Section(i + 1, n, t, fl, a, a - load, b - a, 0, 0, 4, 0, 0)
                      for i, (n, a, b, fl, t) in enumerate(bounds)]
        self.secs = {s.name: s for s in self.order}
        self.starts = [s.addr for s in self.order]

    def own(self, a: int) -> bool:
        return self.load <= a <= self.hi

    def internal(self, a: int) -> bool:
        return self.own(a) or self.main.internal(a)

    def section_of(self, a: int):
        if not self.own(a):
            return self.main.section_of(a)
        return self.order[bisect.bisect_right(self.starts, a) - 1]

    def read(self, a: int, n: int) -> bytes:
        if self.own(a):
            if a >= self.end and a + n <= self.hi:
                return bytes(n)
            if a + n > self.end:
                raise ValueError(f"0x{a:08x}+{n} runs past the overlay")
            return self.blob[a - self.load:a - self.load + n]
        return self.main.read(a, n)

    def word(self, a: int) -> int:
        return struct.unpack("<I", self.read(a, 4))[0]


def bss_extent(blob: bytes, load: int, code: tuple[int, int], gp: int) -> int:
    """The end of the overlay's own .bss: the highest address past the file, inside its slot of
    main's .bss, that its code refers to."""
    end, slot = load + len(blob), REGION_END.get(load, load + len(blob))
    words = list(struct.unpack_from(f"<{(code[1] - code[0]) // 4}I", blob, code[0] - load))
    refs, _ = mips.symbolize(words, code[0], lambda a: load <= a < slot, gp)
    top = max((r.target for r in refs if end <= r.target < slot), default=None)
    return end if top is None else min((top + 4 + 15) & ~15, slot)


class OverlayLayout(Layout):
    """Labels `ro_<address>` for the overlay's own addresses; main's labels for the rest."""

    prefix = PREFIX

    def __init__(self, img: OverlayImage, main_labels: list[int]):
        super().__init__(img)
        self.main_labels = main_labels

    def symbol(self, a: int) -> tuple[str, int]:
        if self.orig.own(a):
            return super().symbol(a)
        return main_symbol(self.main_labels, a)


def main_symbol(labels: list[int], a: int) -> tuple[str, int]:
    i = bisect.bisect_right(labels, a) - 1
    if i < 0:
        raise ValueError(f"0x{a:08x} lies below main's first label")
    return f"rz_{labels[i]:08x}", a - labels[i]


def _expr(sym: str, add: int) -> str:
    return f"{sym} + 0x{add:x}" if add else sym


class MainLink:
    """The original executable, its inventory and symbols, and main.elf's labels."""

    def __init__(self, build: str, out: Path, elf_path: Path):
        b = builds()
        self.orig = Original(ROOT / "game" / build / b[build]["boot_executable"])
        self.inv = json.loads((ROOT / f"config/inventory/{build}.json").read_text())["functions"]
        le = elf32.read(elf_path.read_bytes())
        syms = {s.name: s.value for s in le.symbols if s.name}
        self.anchors = verify.anchors(syms)
        self.labels = [a for a, _ in self.anchors]
        self.move = verify.mapper(self.anchors)
        self.syms = {n: v for n, v in syms.items() if n.startswith("rz_") or n == "_gp"}
        self.just = out / "work" / "main_rz.elf"
        self.just.parent.mkdir(parents=True, exist_ok=True)
        r = run([f"{BIN}objcopy", "--strip-all", "--wildcard", "--keep-symbol=rz_*",
                 "--keep-symbol=_gp", elf_path, self.just])
        if r.returncode:
            raise SystemExit(f"objcopy main.elf: {r.stderr}")
        self.cross_exe, self.cross_reg = (xbuild.overlay_exe(b, Original) if xbuild
                                          else (None, {}))


# Generated data keeps the original's address modulo this (main's largest section alignment),
# so variables after code that changed size keep their alignment.
KEEP_ALIGN = 128


def keep_align(orig: int) -> str:
    """A statement advancing `.` to the next address congruent to `orig` modulo KEEP_ALIGN.
    Correct only in `.ovl`, which starts at its load address."""
    r = orig % KEEP_ALIGN
    return f". = ALIGN(. - {r:#x}, {KEEP_ALIGN}) + {r:#x};"


def overlay_script(lay: OverlayLayout, data_objs: dict, shadow_list: list, aliases: dict,
                   load: str, real: list | None, data_start: int, bss_start: int) -> str:
    L = ["SECTIONS", "{", f"  .ovl ({load}) : {{", f'    "{data_objs[".head"]}"(.head)']
    for c in lay.chunks:
        if c["kind"] == "obj":
            if c["align"] > 4:
                L.append(f"    . = ALIGN({c['align']});")
            L.append(f'    {c["label"]} = .; "{c["obj"]}"({c["section"]})')
        else:
            L.append(f'    "{data_objs[".text"]}"(.text.{c["label"]})')
    L.append(f'    {keep_align(data_start)}')
    L.append(f'    "{data_objs[".data"]}"(.data)')
    nobits = []
    for obj, sec, align, sym in real or []:
        al = f". = ALIGN({align}); " if align > 1 else ""
        line = f'    {al}{sym} = .; "{obj}"({sec})'
        (nobits if sec.startswith((".bss", ".sbss")) else L).append(line)
    L.append("  }")
    # after the file: the overlay's own .bss, and what the reconstructed code adds to it
    # (an explicit address: the section's own alignment, from COMMON symbols, must not move it)
    r = bss_start % KEEP_ALIGN
    L.append(f'  .ovlbss (ALIGN(. - {r:#x}, {KEEP_ALIGN}) + {r:#x}) (NOLOAD) : {{\n'
             f'    "{data_objs[".bss"]}"(.bss)')
    if real is not None:
        L += nobits + ["    *(.scommon) *(COMMON)"]
    L.append("  }")
    L += shadow_script(lay, shadow_list)
    L.append("  /DISCARD/ : { " + " ".join(f"*({d}*)" for d in DISCARD) + " }")
    L.append("}")
    L += [f"{n} = {e};" for n, e in sorted(aliases.items())]
    return "\n".join(L) + "\n"


def build_overlay(a, ov: dict, ml: MainLink, cands: dict) -> dict:
    module = ov["module"]
    bid = f"{a.build}--{module}"
    out = a.out / "overlays"
    work = out / "work" / module
    for d in ("lobj", "gen"):
        (work / d).mkdir(parents=True, exist_ok=True)
    report: dict = {"module": module, "build": bid}
    blob = Path(ov["file"]).read_bytes()
    if hashlib.sha256(blob).hexdigest() != ov["sha256"]:
        raise SystemExit(f"{ov['file']} is not the recorded file")
    load = int(ov["load_address"], 16)
    code = tuple(int(x, 16) for x in ov["code"])
    img = OverlayImage(ml.orig, blob, load, code, bss_extent(blob, load, code, ml.orig.gp))
    inv, groups = records(bid, a.all_c, a.c_only, a.c_skip)
    symbols = {f["name"]: int(f["address"], 16) for f in ml.inv}
    symbols.update({f["name"]: int(f["address"], 16) for f in inv})
    symbols.update(build_symbols(bid))

    jobs = [(src, cands[comp]["command"], flags, str(a.objdir / object_name(src, comp, flags)))
            for (src, comp, flags) in groups]
    with ProcessPoolExecutor(a.jobs) as ex:
        compiled = list(ex.map(compile_one, jobs))
        report["compile_failed"] = {f"{j[0]} {j[2]}": err for j, (_, err) in zip(jobs, compiled)
                                    if err}
        ajobs = [(obj, groups[k], img, symbols)
                 for k, (obj, err) in zip(groups, compiled) if not err]
        analyses = list(ex.map(_analyze_job, ajobs))
    report["analysis_errors"] = [x for x in analyses if "error" in x]
    analyses = [x for x in analyses if "error" not in x]
    report["placement_problems"] = [p for x in analyses for p in x["problems"]]
    if a.all_c:
        analyses, report["reconstructed_units"] = split_reconstructed(analyses, work / "robj")
    lay = OverlayLayout(img, ml.labels)
    report["text_problems"] = plan_text(lay, analyses, inv)
    report["matching_not_linked"] = not_linked(lay, analyses, inv)
    kept = {(c["obj"], c["section"]) for c in lay.chunks if c["kind"] == "obj"}
    symbolized, code_refs, code_suspects = code_chunks(lay)
    for name in SECTIONS:
        lay.labels.add(img.secs[name].addr)
    evidence, covered, ev_stats = data_evidence(
        lay, [x for x in analyses if not x.get("recon_copy")])
    funcs = sorted((int(f["address"], 16), int(f["address"], 16) + f["size"])
                   for f in inv + ml.inv)
    known = set(symbols.values()) | {t for _, _, t, _ in code_refs} | set(evidence.values())
    for x in analyses:
        known |= {v for _, t, v in x["refs"] if t != R_MIPS_26}
    known |= {s for s, _ in funcs}
    xb = xbuild.overlay_check(a, module, img, blob, load, inv, analyses, ml) if xbuild else None
    cross = xb.cross if xb else None
    ptrs, data_suspects, ptr_stats = scan_data(lay, evidence, covered, known, funcs, cross,
                                               scan=SCAN, raw=())
    if a.all_c:
        report["into_reconstructed"] = into_reconstructed(lay, ptrs, code_refs)
    for _, _, t, _ in code_refs:
        lay.symbol(t)
    outside = {p: t for p, t in ptrs.items() if not img.internal(t)}
    for p, t in outside.items():     # a relocated word of a unit whose original holds no address
        data_suspects.append((p, f"word 0x{ptrs.pop(p):08x}: relocated in the source, not an "
                                 "address in the original; left as is"))
    for t in ptrs.values():
        lay.symbol(t)
    real: list | None = [] if a.all_c else None
    shadow_list, sh_stats = shadows(lay, analyses, symbols, kept, real, place=img.own)
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
            addr = next(iter(values[n]))
        elif n not in symbols and xb:
            hit, x = xb.translate(n, addr)
            if hit:
                addr = x
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
    for lab in sorted(lay.labels):
        for p in (lab - 1, lab - 2, lab - 3):
            if p in ptrs:
                data_suspects.append((p, f"word 0x{ptrs.pop(p):08x}: a label lies inside it, "
                                         "left as is"))
    gen = work / "gen"
    emit_text(lay, symbolized, gen / "text.s")
    data_objs = {".text": str(gen / "text.o")}
    for name in SCAN + (".bss",):
        emit_data(lay, name, ptrs, gen / f"{name[1:]}.s")
        data_objs[name] = str(gen / f"{name[1:]}.o")
    for o in data_objs.values():
        r = run([f"{BIN}as", "-EL", "-march=r5900", "-mabi=eabi", "-G0", "-no-pad-sections",
                 str(Path(o).with_suffix(".s")), "-o", o])
        if r.returncode:
            raise SystemExit(f"{module}: assembler failed:\n{r.stderr[-2000:]}")
    strip = collections.defaultdict(list)
    for obj, name, *_ in shadow_list:
        strip[obj].append(name)
    objs = sorted({x["obj"] for x in analyses})
    linked_objs = {o: link_object((o, strip.get(o, []), str(work / "lobj" / Path(o).name),
                                   edits.get(o, []))) for o in objs}
    for c in lay.chunks:
        if c["kind"] == "obj":
            c["obj"] = linked_objs[c["obj"]]
    shadow_list = [(linked_objs[o], *rest) for o, *rest in shadow_list]
    if real is not None:
        real = [(linked_objs[o], *rest) for o, *rest in real]
    load_expr = _expr(*main_symbol(ml.labels, load))
    script = overlay_script(lay, data_objs, shadow_list, aliases, load_expr, real, code[1],
                            img.end)
    (work / "overlay.ld").write_text(script)
    elf_path = work / "overlay.elf"
    ld = run([f"{BIN}ld", "-EL", "-T", work / "overlay.ld", "-o", elf_path,
              "--no-check-sections", "-z", "muldefs", "-Map", work / "overlay.map",
              f"--just-symbols={ml.just}",
              *data_objs.values(), *sorted(set(linked_objs.values()))])
    report["ld"] = ld.stderr[-4000:]
    report["ld_errors"] = sorted({ln.split(": ", 1)[-1] for ln in ld.stderr.splitlines()
                                  if "undefined reference" in ln or "truncated" in ln
                                  or "exceeds" in ln or "error" in ln.lower()})
    report["stats"] = {
        "text_chunks": len(lay.chunks),
        "compiled_chunks": sum(c["kind"] == "obj" for c in lay.chunks),
        "compiled_bytes": sum(c["size"] for c in lay.chunks if c["kind"] == "obj"),
        "extracted_bytes": sum(c["size"] for c in lay.chunks if c["kind"] == "asm"),
        "code_relocations": len(code_refs), "data_pointers": len(ptrs),
        "data_pointers_into_main": sum(not img.own(t) for t in ptrs.values()),
        "code_relocations_into_main": sum(not img.own(t) for _, _, t, _ in code_refs),
        "labels": len(lay.labels), "aliases": len(aliases),
        "shadows": dict(sh_stats), "data_evidence": dict(ev_stats),
        "pointer_scan": dict(ptr_stats)}
    if ld.returncode or not elf_path.is_file():
        report["ok"] = False
        return report
    le = elf32.read(elf_path.read_bytes())
    s = le.section(".ovl")
    data = le.data(s)
    bss = le.section(".ovlbss")
    # every label of the overlay moves with its load address unless code changed size (--all-c)
    shift = s.addr - load
    report["labels_not_moved_with_load"] = sum(
        1 for x in le.symbols if x.name.startswith(PREFIX) and len(x.name) == len(PREFIX) + 8
        and x.value - int(x.name[3:], 16) != shift)
    dest = out / Path(ov["file"]).name
    dest.write_bytes(data)
    got = hashlib.sha256(data).hexdigest()
    end = s.addr + s.size + (bss.size if bss is not None else 0)
    room = REGION_END.get(load)
    report.update(file=str(dest), sha256=got, expected=ov["sha256"],
                  sha256_match=got == ov["sha256"], size=len(data), original_size=len(blob),
                  load=f"0x{s.addr:08x}", bss_end_original=f"0x{img.bss_end:08x}",
                  load_follows_main=s.addr == ml.move(load), end=f"0x{end:08x}",
                  fits=room is None or end - s.addr <= room - load, ok=True)
    osyms = {x.name: x.value for x in le.symbols if x.name}
    syms = dict(ml.syms, **osyms)
    omove = verify.mapper(verify.anchors(osyms, PREFIX))

    def move(x: int) -> int:
        return omove(x) if img.own(x) else ml.move(x)
    report["verify"] = verify.check(img, lay, {".ovl": (s.addr, s.size, data)}, syms, code_refs,
                                    ptrs, move=move)
    report["hard_coded"] = sorted({f"0x{x:08x}: {why}" for x, why in code_suspects + data_suspects}
                                  | set(report["verify"].pop("suspects")))
    return report


def build_all(a, ml: MainLink, modules: list[str] | None) -> dict:
    reg = registry(a.build)
    cands = {c["id"]: c for c in load_candidates(ROOT / "config/compilers.json", "ps2")}
    out = a.out / "overlays"
    out.mkdir(parents=True, exist_ok=True)
    summary = {}
    for module in modules or list(reg):
        r = build_overlay(a, reg[module], ml, cands)
        (out / "work" / module / "report.json").write_text(json.dumps(r, indent=1) + "\n")
        v = r.get("verify", {})
        summary[module] = {k: r.get(k) for k in (
            "ok", "sha256_match", "size", "original_size", "load", "load_follows_main",
            "bss_end_original", "end", "fits", "labels_not_moved_with_load")}
        summary[module].update(checked=v.get("checked"), wrong=v.get("wrong_count"),
                               hard_coded=len(r.get("hard_coded", [])),
                               unresolved=len(r.get("unresolved_symbols", [])),
                               ld_errors=len(r.get("ld_errors", [])),
                               compile_failed=len(r.get("compile_failed", {})),
                               not_linked=len(r.get("matching_not_linked", [])))
        print(f"{module:10} {'ok' if r.get('ok') else 'LINK FAILED'} "
              f"sha256 {'MATCH' if r.get('sha256_match') else 'differs'} "
              f"checked {v.get('checked')} wrong {v.get('wrong_count')} "
              f"hard-coded {len(r.get('hard_coded', []))} ld errors {len(r.get('ld_errors', []))} "
              f"not linked from source {len(r.get('matching_not_linked', []))}")
    (out / "report.json").write_text(json.dumps(summary, indent=1) + "\n")
    return summary
