# SPDX-License-Identifier: GPL-3.0-or-later
"""Corrupt only DFSV v2-v8 projectile tails to test transactional rejection."""
import hashlib
from pathlib import Path
import struct
import sys

source, retail, destination = map(Path, sys.argv[1:])
data = source.read_bytes()
full = retail.read_bytes()
assert all(d[:4] == b'DFSV' and struct.unpack_from('<I', d, 4)[0] in (2, 3, 4, 5, 6, 7, 8)
           and hashlib.md5(d[28:]).digest() == d[12:28] for d in (data, full))
needle = struct.pack('<I', 10) + b'targbullet'
start = data.rfind(needle) - 1
assert start >= 28 and data[start] == 1
record = data[start:-15]
assert len(record) == 1 + len(needle) + 20 and data[-15:] == bytes(15)
assert full[-16:] == bytes(16)
destination.mkdir(parents=True, exist_ok=True)


def emit(name, value):
    value = bytearray(value)
    struct.pack_into('<I', value, 8, len(value) - 28)
    value[12:28] = hashlib.md5(value[28:]).digest()
    (destination / (name + '.dfsv')).write_bytes(value)


emit('duplicate-ball', data[:start] + record + record + bytes(14))
emit('bad-ball-flag', data[:-15] + bytes([2]) + bytes(14))
emit('retail-ball', full[:-16] + record + bytes(15))
print('Three checksummed projectile-tail rejection fixtures written')
