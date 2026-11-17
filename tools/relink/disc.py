"""Write rebuilt files into copies of REZ.CVM and the disc image.

    python scripts/build_disc.py --from build/relink/ps2-us-1.01/exact [--out DIR]"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
USER, RAW = 2048, 2352
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
DATA, EOF = 0x08, 0x89          # subheader submodes: data; data + end of record + end of file
CVM_HEADER = 0x1800

_ECC_F = [0] * 256
_ECC_B = [0] * 256
_EDC = [0] * 256
for _i in range(256):
    _j = ((_i << 1) ^ (0x11D if _i & 0x80 else 0)) & 0xFF
    _ECC_F[_i] = _j
    _ECC_B[_i ^ _j] = _i
    _e = _i
    for _ in range(8):
        _e = (_e >> 1) ^ (0xD8018001 if _e & 1 else 0)
    _EDC[_i] = _e


def edc(data: bytes) -> int:
    """The CD-ROM EDC (CRC-32, polynomial 0xD8018001 reflected) of `data`."""
    e = 0
    for b in data:
        e = (e >> 8) ^ _EDC[(e ^ b) & 0xFF]
    return e


def _ecc(sector: bytearray, majors: int, minors: int, mult: int, inc: int, dest: int):
    """Reed-Solomon parity over sector[12:] (the P or Q vectors), written at sector[dest:]."""
    size = majors * minors
    for major in range(majors):
        index = (major >> 1) * mult + (major & 1)
        a = b = 0
        for _ in range(minors):
            t = sector[12 + index]
            index += inc
            if index >= size:
                index -= size
            a ^= t
            b ^= t
            a = _ECC_F[a]
        a = _ECC_B[_ECC_F[a] ^ b]
        sector[dest + major] = a
        sector[dest + major + majors] = a ^ b


def msf(lba: int) -> bytes:
    """The BCD minute:second:frame address of `lba` (LBA 0 is 00:02:00)."""
    f = lba + 150
    bcd = [f // (75 * 60), f // 75 % 60, f % 75]
    return bytes(((x // 10) << 4) | (x % 10) for x in bcd)


def encode_sector(lba: int, user: bytes, submode: int = DATA) -> bytes:
    """A complete Mode 2 Form 1 sector: the ECC is computed with the header's address as zero."""
    assert len(user) == USER
    s = bytearray(RAW)
    s[0:12] = SYNC
    sub = bytes([0, 0, submode, 0])
    s[16:24] = sub + sub
    s[24:24 + USER] = user
    struct.pack_into("<I", s, 0x818, edc(bytes(s[16:0x818])))
    _ecc(s, 86, 24, 2, 86, 0x81C)        # P
    _ecc(s, 52, 43, 86, 88, 0x8C8)       # Q
    s[12:16] = msf(lba) + b"\x02"
    return bytes(s)


class Volume:
    """An ISO 9660 volume in a file: 2048-byte sectors after `header` bytes, or raw sectors."""

    def __init__(self, path: Path, raw: bool = False, header: int = 0):
        self.path, self.raw, self.header = Path(path), raw, header
        self.f = self.path.open("r+b")
        self.ssize = RAW if raw else USER
        pvd = self.read(16)
        if pvd[1:6] != b"CD001":
            raise ValueError(f"{path}: no ISO 9660 volume")
        self.blocks = struct.unpack_from("<I", pvd, 80)[0]
        self.root = (struct.unpack_from("<I", pvd, 156 + 2)[0],
                     struct.unpack_from("<I", pvd, 156 + 10)[0])

    def close(self):
        self.f.close()

    def read(self, lba: int) -> bytes:
        self.f.seek(self.header + lba * self.ssize + (24 if self.raw else 0))
        d = self.f.read(USER)
        if len(d) != USER:
            raise ValueError(f"sector {lba} is past the end of {self.path.name}")
        return d

    def write(self, lba: int, data: bytes, submode: int | None = None):
        """A sector's user data; a raw sector keeps its submode unless one is given."""
        data = data.ljust(USER, b"\0")
        if self.raw and submode is None:
            self.f.seek(lba * RAW + 18)
            sub = self.f.read(1)
            submode = sub[0] if sub else DATA
        self.f.seek(self.header + lba * self.ssize)
        self.f.write(encode_sector(lba, data, submode) if self.raw else data)

    def copy(self, src: int, dst: int):
        """Sector `src` to `dst`. A raw sector's EDC and ECC do not cover its address (Mode 2), so
        only the address changes."""
        if not self.raw:
            self.write(dst, self.read(src))
            return
        self.f.seek(src * RAW)
        s = bytearray(self.f.read(RAW))
        s[12:15] = msf(dst)
        self.f.seek(dst * RAW)
        self.f.write(s)

    def files(self) -> dict[str, dict]:
        """{path: {lba, size, record: (sector, offset), dir}} of every file and directory below
        the root, upper case, without `;1`."""
        out = {}

        def walk(lba: int, size: int, parent: str):
            for k in range((size + USER - 1) // USER):
                buf = self.read(lba + k)
                off = 0
                while off < USER and buf[off]:
                    n = buf[off]
                    name = buf[off + 33:off + 33 + buf[off + 32]]
                    e_lba, e_size = struct.unpack_from("<I", buf, off + 2)[0], \
                        struct.unpack_from("<I", buf, off + 10)[0]
                    if name not in (b"\0", b"\1"):
                        p = f"{parent}/{name.decode('latin-1').split(';')[0]}".upper()
                        is_dir = bool(buf[off + 25] & 2)
                        out[p] = {"lba": e_lba, "size": e_size, "record": (lba + k, off),
                                  "dir": is_dir}
                        if is_dir:
                            walk(e_lba, e_size, p)
                    off += n
        walk(*self.root, "")
        return out

    def set_record(self, rec: tuple[int, int], lba: int, size: int):
        """Point the directory record at `rec` (sector, offset) at `lba`, `size` (both-endian)."""
        sector, off = rec
        buf = bytearray(self.read(sector))
        struct.pack_into("<I", buf, off + 2, lba)
        struct.pack_into(">I", buf, off + 6, lba)
        struct.pack_into("<I", buf, off + 10, size)
        struct.pack_into(">I", buf, off + 14, size)
        self.write(sector, bytes(buf))

    def set_blocks(self, blocks: int):
        """Set the volume size in the primary volume descriptor."""
        buf = bytearray(self.read(16))
        struct.pack_into("<I", buf, 80, blocks)
        struct.pack_into(">I", buf, 84, blocks)
        self.write(16, bytes(buf))
        self.blocks = blocks

    def write_file(self, lba: int, data: bytes, sectors: int, old: bytes | None = None,
                   old_lba: int | None = None):
        """`data` at `lba`, zeroes in the rest of `sectors`. Sectors whose contents stay are left
        or copied from `old` at `old_lba`."""
        n = max((len(data) + USER - 1) // USER, 1)
        old_n = (len(old) + USER - 1) // USER if old is not None else 0
        old_lba = lba if old_lba is None else old_lba
        for k in range(max(n, sectors)):
            chunk = data[k * USER:(k + 1) * USER].ljust(USER, b"\0")
            last = k == n - 1
            if k < old_n - 1 and not last and old[k * USER:(k + 1) * USER] == chunk:
                if old_lba != lba:
                    self.copy(old_lba + k, lba + k)
                continue
            self.write(lba + k, chunk, EOF if last else DATA)


def replace(vol: Volume, path: str, data: bytes) -> dict:
    """Put `data` in place of the file `path`, in place if it fits, else at the end of the
    volume. Returns what was done."""
    files = vol.files()
    key = "/" + path.strip("/").upper()
    f = files[key]
    if f["dir"]:
        raise IsADirectoryError(path)
    starts = sorted({x["lba"] for x in files.values()} | {vol.root[0], vol.blocks})
    nxt = next(s for s in starts if s > f["lba"]) if f["size"] else f["lba"]
    room = (nxt - f["lba"]) * USER
    old_sectors = (f["size"] + USER - 1) // USER
    old = b"".join(vol.read(f["lba"] + k) for k in range(old_sectors))[:f["size"]]
    if len(data) <= room:
        vol.write_file(f["lba"], data, old_sectors, old)
        vol.set_record(f["record"], f["lba"], len(data))
        return {"path": key, "lba": f["lba"], "size": len(data), "room": room, "moved": False}
    lba = vol.blocks
    n = (len(data) + USER - 1) // USER
    vol.write_file(lba, data, n, old, f["lba"])
    vol.set_record(f["record"], lba, len(data))
    vol.set_blocks(lba + n)
    return {"path": key, "lba": lba, "size": len(data), "room": room, "moved": True,
            "from_lba": f["lba"]}


def cvm_fix_header(path: Path, iso_blocks: int):
    """The CRI header's sizes after the CVM's volume grew: the file size (0x20), the ZONE's size
    (0x808, the file size less 0x80c) and its data size (0x834, the volume's bytes)."""
    with path.open("r+b") as f:
        size = CVM_HEADER + iso_blocks * USER
        f.truncate(size)
        h = bytearray(f.read(CVM_HEADER))
        assert h[:4] == b"CVMH" and h[0x800:0x804] == b"ZONE"
        struct.pack_into(">I", h, 0x20, size)
        struct.pack_into(">I", h, 0x808, size - 0x80C)
        struct.pack_into(">I", h, 0x834, iso_blocks * USER)
        f.seek(0)
        f.write(h)


def _map_symbols(path: Path, names: list[str]) -> dict[str, int]:
    """Addresses of `names` in a GNU ld map file."""
    found = {}
    for m in re.finditer(r"^\s+0x([0-9a-f]+)\s+(\S+)\s*$", path.read_text(), re.M):
        if m.group(2) in names:
            found[m.group(2)] = int(m.group(1), 16)
    missing = set(names) - set(found)
    if missing:
        raise SystemExit(f"{path}: no symbol {', '.join(sorted(missing))}")
    return found


def skip_memcard_prompt_patch(src: Path, data: bytes) -> bytes:
    """TEST ONLY (--test-skip-memcard-prompt): AUTOLOAD.BIN whose task starts at the health
    warning, so the memory-card load and its prompts never run."""
    names = ("init", "exec", "disp", "release", "exec_warning")
    sym = _map_symbols(src / "overlays/work/autoload/overlay.map",
                       [f"autoload__autoload_{n}" for n in names])

    def a(n: str) -> int:
        return sym[f"autoload__autoload_{n}"]

    desc = struct.pack("<4I", a("init"), a("exec"), a("disp"), a("release"))
    hits = [m.start() for m in re.finditer(re.escape(desc), data)]
    if len(hits) != 1:
        raise SystemExit(f"AUTOLOAD: {len(hits)} task descriptors found, expected 1")
    out = bytearray(data)
    out[hits[0] + 4:hits[0] + 8] = struct.pack("<I", a("exec_warning"))
    return bytes(out)


def build(src: Path, out: Path, build_id: str = "ps2-us-1.01",
          skip_memcard_prompt: bool = False, autoplay: int | None = None,
          autoplay_modes: tuple[str, ...] = ()) -> dict:
    """Write REZ.CVM and the disc image into `out`, read them back, return the report."""
    reg = json.loads((ROOT / "config/overlays" / f"{build_id}.json").read_text())
    builds = json.loads((ROOT / "config/versions/builds.json").read_text())["builds"]
    b = {x["id"]: x for x in builds}
    game = ROOT / "game" / build_id
    disc_in = next((game / "disc").glob("*.bin"), None)
    if disc_in is None:
        raise SystemExit(f"game/{build_id}/disc/: no .bin; copy your raw disc image there")
    cvm_in = game / "REZ.CVM"
    out.mkdir(parents=True, exist_ok=True)
    report: dict = {"from": str(src), "cvm": [], "disc": []}
    cvm = out / "REZ.CVM"
    shutil.copyfile(cvm_in, cvm)
    vol = Volume(cvm, header=CVM_HEADER)
    blocks0 = vol.blocks
    for ov in reg["overlays"]:
        p = src / "overlays" / ov["path"].split("/")[-1]
        if not p.is_file():
            report["cvm"].append({"path": ov["path"], "missing": True})
            continue
        data = p.read_bytes()
        if skip_memcard_prompt and p.name == "AUTOLOAD.BIN":
            data = skip_memcard_prompt_patch(src, data)
            report["test_skip_memcard_prompt"] = True
        report["cvm"].append(replace(vol, ov["path"], data))
    vol.close()
    if vol.blocks != blocks0:
        cvm_fix_header(cvm, vol.blocks)
    disc = out / disc_in.name
    shutil.copyfile(disc_in, disc)
    dv = Volume(disc, raw=True)
    # the EDC/ECC computed here must reproduce the original's sectors
    for lba in (16, 947, dv.blocks - 1):
        dv.f.seek(lba * RAW)
        orig = dv.f.read(RAW)
        if encode_sector(lba, orig[24:24 + USER], orig[18]) != orig:
            raise SystemExit(f"EDC/ECC of sector {lba} does not reproduce the original")
    exe = src / b[build_id]["boot_executable"]
    exe_bytes = exe.read_bytes() if exe.is_file() else None
    if autoplay is not None and exe_bytes is not None:
        from relink.autoplay import autoplay_patch
        exe_bytes, report["test_autoplay"] = autoplay_patch(src, exe_bytes, autoplay,
                                                            autoplay_modes)
    report["disc"].append(replace(dv, "REZ.CVM", cvm.read_bytes()))
    if exe_bytes is not None:
        report["disc"].append(replace(dv, exe.name, exe_bytes))
    dv.close()
    cue = disc.with_suffix(".cue")
    cue.write_text(f'FILE "{disc.name}" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
    sys.path.insert(0, str(ROOT / "tools"))
    from asset_analysis.iso9660 import Image
    ok = True
    with Image(disc) as d:
        ok &= d.read_file("REZ.CVM") == cvm.read_bytes()
        if exe_bytes is not None:
            ok &= d.read_file(exe.name) == exe_bytes
    with Image(cvm) as c:
        for ov in reg["overlays"]:
            p = src / "overlays" / ov["path"].split("/")[-1]
            if p.is_file() and not (skip_memcard_prompt and p.name == "AUTOLOAD.BIN"):
                ok &= c.read_file(ov["path"]) == p.read_bytes()
    report.update(read_back_ok=ok, image=str(disc),
                  sha256=hashlib.sha256(disc.read_bytes()).hexdigest())
    (out / "report.json").write_text(json.dumps(report, indent=1) + "\n")
    return report


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        prog="build_disc.py",
        description=__doc__.split("\n\n")[0] + " Reads the disc image from "
        "game/ps2-us-1.01/disc/*.bin.")
    ap.add_argument("--from", dest="src", type=Path, required=True,
                    help="a relink output directory (its SLUS_203.44 and overlays/*.BIN)")
    ap.add_argument("--out", type=Path, default=None, help="default: build/disc/<name of --from>")
    ap.add_argument("--build", default="ps2-us-1.01", help=argparse.SUPPRESS)
    ap.add_argument("--test-skip-memcard-prompt", action="store_true",
                    help="TEST ONLY: patch AUTOLOAD.BIN so the boot skips the memory-card load and "
                         "its prompts (no input needed to reach the title and its attract demo); "
                         "default --out build/disc/<name>-autotest")
    if (Path(__file__).parent / "autoplay.py").is_file():
        ap.add_argument("--test-autoplay", nargs="?", const="0", default=None,
                        metavar="AREA[,MODE...]",
                        help="TEST ONLY: add the autoplay bot (tools/relink/autoplay_bot.c) to "
                             "the executable: it gives the controller input from inside the "
                             "game, plays and keeps the player from dying; with AREA (1-5) it "
                             "plays that area, otherwise every area in turn. Modes: 'mortal' (no "
                             "invincibility, stops firing after a while: damage, death, game "
                             "over), 'card' (keeps the memory-card load at boot and answers its "
                             "prompts). Implies --test-skip-memcard-prompt unless 'card'; "
                             "default --out build/disc/<name>-autoplay[AREA][-MODE...]")
    a = ap.parse_args(argv)
    src = a.src.resolve()
    area, modes = None, ()
    if getattr(a, "test_autoplay", None) is not None:
        from relink.autoplay import parse_spec
        try:
            area, modes = parse_spec(a.test_autoplay)
        except ValueError as e:
            ap.error(f"--test-autoplay: {e}")
        a.test_skip_memcard_prompt = "card" not in modes
        tag = src.name + "-autoplay" + (str(area) if area else "") + "".join("-" + m for m in modes)
    else:
        tag = src.name + ("-autotest" if a.test_skip_memcard_prompt else "")
    out = (a.out or ROOT / "build/disc" / tag).resolve()
    r = build(src, out, a.build, a.test_skip_memcard_prompt, area, modes)
    moved = [x["path"] for x in r["cvm"] + r["disc"] if x.get("moved")]
    print(f"{r['image']}: {sum(not x.get('missing') for x in r['cvm'])} overlays, "
          f"moved to the end of their volume: {', '.join(moved) or 'none'}; "
          f"read back {'OK' if r['read_back_ok'] else 'FAILED'}")
    return 0 if r["read_back_ok"] else 1
