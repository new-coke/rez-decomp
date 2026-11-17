"""Read files from ISO 9660 volumes: 2048-byte ISOs, raw 2352-byte CD images and CRI CVM
volumes (ISO 9660 behind a 0x1800-byte header)."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
RAW = 2352
USER = 2048
CVM_MAGIC = b"CVMH"
CVM_HEADER = 0x1800


class DiscError(ValueError):
    pass


@dataclass
class Entry:
    path: str
    lba: int
    size: int
    is_dir: bool


class Image:
    def __init__(self, path: str | Path, lba_base: int | None = None):
        """lba_base: absolute LBA of the image's first sector (None: detected)."""
        self.path = Path(path)
        self.lba_base = 0
        self.f = self.path.open("rb")
        size = self.path.stat().st_size
        head = self.f.read(16)
        if head[:4] == CVM_MAGIC:
            self.sector_size, self.data_offset = USER, CVM_HEADER
        elif head[:12] == SYNC and size % RAW == 0:
            self.sector_size = RAW
            self.data_offset = 16 if head[15] == 1 else 24  # Mode 1, or Mode 2 past its subheader
        elif size % USER == 0:
            self.sector_size, self.data_offset = USER, 0
        else:
            raise DiscError(f"{path}: not a 2048- or 2352-byte sector image")
        pvd = self.read_sector(16)
        if pvd[0] != 1 or pvd[1:6] != b"CD001":
            raise DiscError(f"{path}: no ISO 9660 primary volume descriptor at sector 16")
        self.volume_id = pvd[40:72].decode("latin-1").rstrip()
        self.volume_blocks = struct.unpack_from("<I", pvd, 80)[0]
        self.block_size = struct.unpack_from("<H", pvd, 128)[0]
        if self.block_size != USER:
            raise DiscError(f"unsupported logical block size {self.block_size}")
        self.root = self._record(pvd, 156, "")
        sectors = size // self.sector_size
        if lba_base is not None:
            self.lba_base = lba_base
        else:
            # the base under which the root's "." record points at the root
            for base in (0, 45150, 45000):
                rel = self.root.lba - base
                if 0 <= rel < sectors:
                    self.f.seek(rel * self.sector_size + self.data_offset)
                    rec = self.f.read(USER)
                    if len(rec) == USER and rec[0] >= 34 and \
                            struct.unpack_from("<I", rec, 2)[0] == self.root.lba:
                        self.lba_base = base
                        break
            else:
                raise DiscError(f"{path}: cannot place the root directory (LBA "
                                f"{self.root.lba}); pass lba_base")

    def close(self) -> None:
        self.f.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def read_sector(self, lba: int) -> bytes:
        self.f.seek(lba * self.sector_size + self.data_offset)
        data = self.f.read(USER)
        if len(data) != USER:
            raise DiscError(f"sector {lba} is past the end of the image")
        return data

    def read_extent(self, lba: int, size: int) -> bytes:
        out = bytearray()
        sector = lba - self.lba_base
        while len(out) < size:
            out += self.read_sector(sector)
            sector += 1
        return bytes(out[:size])

    @staticmethod
    def _record(buf: bytes, off: int, parent: str) -> Entry:
        length = buf[off]
        lba = struct.unpack_from("<I", buf, off + 2)[0]
        size = struct.unpack_from("<I", buf, off + 10)[0]
        flags = buf[off + 25]
        name_len = buf[off + 32]
        name = buf[off + 33:off + 33 + name_len].decode("latin-1").split(";")[0]
        path = f"{parent}/{name}" if name not in ("\x00", "\x01") else parent or "/"
        if length < 34:
            raise DiscError("malformed directory record")
        return Entry(path, lba, size, bool(flags & 2))

    def listdir(self, directory: Entry) -> list[Entry]:
        data = self.read_extent(directory.lba, directory.size)
        out, off = [], 0
        while off < len(data):
            length = data[off]
            if length == 0:  # records do not cross sector boundaries; skip padding
                off = (off // USER + 1) * USER
                continue
            e = self._record(data, off, directory.path.rstrip("/"))
            name_len = data[off + 32]
            if not (name_len == 1 and data[off + 33] in (0, 1)):
                out.append(e)
            off += length
        return out

    def walk(self, directory: Entry | None = None):
        directory = directory or self.root
        for e in self.listdir(directory):
            yield e
            if e.is_dir:
                yield from self.walk(e)

    def find(self, path: str) -> Entry:
        want = "/" + path.strip("/").upper()
        for e in self.walk():
            if e.path.upper() == want:
                return e
        raise FileNotFoundError(path)

    def read_file(self, path: str) -> bytes:
        e = self.find(path)
        if e.is_dir:
            raise IsADirectoryError(path)
        return self.read_extent(e.lba, e.size)


def parse_system_cnf(text: str) -> dict:
    """PS2 SYSTEM.CNF: KEY = value lines; BOOT2 = cdrom0:\\NAME;1 names the boot ELF."""
    out = {}
    for line in text.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    boot = out.get("BOOT2") or out.get("BOOT")
    if boot:
        name = boot.split(":", 1)[-1].lstrip("\\/").split(";")[0]
        out["boot_file"] = name.replace("\\", "/")
    return out
