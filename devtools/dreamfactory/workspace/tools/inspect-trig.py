# SPDX-License-Identifier: GPL-3.0-or-later
"""Read Titanic's original fixed-point vector tables without executing TI.EXE."""
import hashlib
import math
from pathlib import Path
import struct


def tables(path, expected_hash):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_hash:
        raise ValueError(f"Unexpected original executable: {path}")
    pe = struct.unpack_from("<I", data, 60)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Expected PE executable")
    optional = pe + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x10b:
        raise ValueError("Expected PE32 optional header")
    section_start = optional + struct.unpack_from("<H", data, pe + 20)[0]
    sections = [struct.unpack_from("<IIII", data, section_start + i * 40 + 8)
                for i in range(struct.unpack_from("<H", data, pe + 6)[0])]

    def raw(rva):
        for virtual_size, address, size, offset in sections:
            if address <= rva < address + max(virtual_size, size):
                delta = rva - address
                if delta >= size:
                    raise ValueError("Resource points outside raw section")
                return offset + delta
        raise ValueError(f"Unmapped RVA {rva:x}")

    root = raw(struct.unpack_from("<I", data, optional + 112)[0])

    def entries(offset):
        named, numeric = struct.unpack_from("<HH", data, root + offset + 12)
        for index in range(named + numeric):
            key, value = struct.unpack_from("<II", data, root + offset + 16 + index * 8)
            if key & 0x80000000:
                string = root + (key & 0x7fffffff)
                length = struct.unpack_from("<H", data, string)[0]
                key = data[string + 2:string + 2 + length * 2].decode("utf-16le")
            yield key, value

    found = {}
    for kind, directory in entries(0):
        if kind != "RAW":
            continue
        for name, languages in entries(directory & 0x7fffffff):
            if name not in ("TRIG1", "TRIG2"):
                continue
            for language, entry in entries(languages & 0x7fffffff):
                address, size = struct.unpack_from("<II", data, root + entry)
                if size != 512:
                    raise ValueError(f"Unexpected {name} size {size}")
                content = data[raw(address):raw(address) + size]
                values = struct.unpack("<256h", content)
                trig = math.sin if name == "TRIG1" else math.cos
                differing = [i for i, value in enumerate(values)
                             if value != math.floor(16384 * trig(i * math.pi / 128) + 0.5)]
                print(f"{path.parent.parent.name} {name} lang={language} "
                      f"sha256={hashlib.sha256(content).hexdigest()} "
                      f"rounded-table-differences={len(differing)} headings={differing}")
                found[name] = content
    if set(found) != {"TRIG1", "TRIG2"}:
        raise ValueError(f"Missing trig tables in {path}")
    return found


root = Path(__file__).resolve().parent.parent
retail = tables(root / "game-data/full/installed/TI.EXE",
                "09e4c9c6536660c303fd26f0fa01dc91e2583eb78b81243f8fb1b27ef761011f")
demo = tables(root / "game-data/demo/installed/TI.EXE",
              "70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63")
if retail != demo:
    raise ValueError("Retail and demo trig resources differ")
print("PASS: retail/demo tables match; current rounded sine/cosine is an approximation")
