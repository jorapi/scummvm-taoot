# SPDX-License-Identifier: GPL-3.0-or-later
"""Read selected globals from a native DFSV checkpoint without modifying it."""
import hashlib
import json
from pathlib import Path
import struct
import sys


def read_globals(path):
    data = Path(path).read_bytes()
    if len(data) < 28 or data[:4] != b'DFSV':
        raise ValueError('Not a native DFSV checkpoint')
    if struct.unpack_from('<I', data, 4)[0] not in (1, 2, 3, 4, 5, 6, 7, 8, 9) or struct.unpack_from('<I', data, 8)[0] != len(data) - 28:
        raise ValueError('Unsupported checkpoint envelope')
    if hashlib.md5(data[28:]).digest() != data[12:28]:
        raise ValueError('Checkpoint checksum mismatch')
    pos = 28

    def u32():
        nonlocal pos
        if pos + 4 > len(data):
            raise ValueError('Truncated checkpoint')
        value = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        return value

    def string():
        nonlocal pos
        size = u32()
        if size > 1024 * 1024 or pos + size > len(data):
            raise ValueError('Invalid checkpoint string')
        value = data[pos:pos + size].decode('utf-8', errors='replace')
        pos += size
        return value

    string()  # BOOTFILE identity
    u32()     # DreamFactory version
    for _ in range(9):
        string()  # Resource paths
    count = u32()
    if count > 10000:
        raise ValueError('Invalid global count')
    globals_ = {}
    for _ in range(count):
        key = string()
        if pos >= len(data):
            raise ValueError('Truncated checkpoint value')
        kind = data[pos]
        pos += 1
        if kind == 1:
            value = string()
        elif kind == 0:
            value = u32()
            if value >= 2**31:
                value -= 2**32
        else:
            raise ValueError('Invalid checkpoint value kind')
        globals_[key] = value
    return globals_


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit('Usage: read-checkpoint-globals.py CHECKPOINT GLOBAL [GLOBAL ...]')
    values = read_globals(sys.argv[1])
    print(json.dumps({key: values.get(key) for key in sys.argv[2:]}))
