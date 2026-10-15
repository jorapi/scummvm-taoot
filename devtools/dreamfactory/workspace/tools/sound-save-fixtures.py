# SPDX-License-Identifier: GPL-3.0-or-later
"""Create invalid Dust DFSV v4-v8 sound-bank tables for rejection tests only."""
import hashlib
from pathlib import Path
import struct
import sys

source, destination = map(Path, sys.argv[1:])
data = bytearray(source.read_bytes())
assert data[:4] == b'DFSV' and struct.unpack_from('<I', data, 4)[0] in (4, 5, 6, 7, 8)
assert struct.unpack_from('<I', data, 8)[0] == len(data) - 28
assert hashlib.md5(data[28:]).digest() == data[12:28]
key, canonical = b'unilib.snd', b'installed/LOCAL/UNILIB.SND'
anchor = struct.pack('<I', len(key)) + key + struct.pack('<I', len(canonical)) + canonical
assert data.count(anchor) == 1
pos = data.index(anchor)
count, = struct.unpack_from('<I', data, pos - 4)
assert 2 <= count <= 256  # Exercise failure after one bank has already opened.
sources = []
for _ in range(count):
    for field in range(2):
        size, = struct.unpack_from('<I', data, pos)
        pos += 4
        assert 0 < size <= 4096 and pos + size <= len(data)
        if field == 1:
            sources.append((pos, size))
        pos += size
assert all(struct.unpack_from('<I', data, pos + 4 * i)[0] & 0xffff == 0 for i in range(count))
destination.mkdir(parents=True, exist_ok=True)


def emit(name, bad):
    struct.pack_into('<I', bad, 8, len(bad) - 28)
    bad[12:28] = hashlib.md5(bad[28:]).digest()
    (destination / (name + '.dfsv')).write_bytes(bad)


for index in range(min(count, 2)):
    bad = data.copy()
    start, size = sources[index]
    bad[start + size - 1] = ord('X')
    emit(f'missing-bank-{index}', bad)
for name, value in [('unaligned-priority', 1), ('oversized-priority', 0x1000000)]:
    bad = data.copy()
    struct.pack_into('<I', bad, pos, value)
    emit(name, bad)
total = min(count, 2) + 2
if struct.unpack_from('<I', data, 4)[0] >= 5:
    controls = pos + count * 4
    entries, = struct.unpack_from('<I', data, controls)
    assert entries <= 16384
    bad = data.copy()
    struct.pack_into('<I', bad, controls, entries + 1)
    key = b'nonexistent-sound'
    bad[controls + 4:controls + 4] = struct.pack('<I', len(key)) + key + b'\0'
    emit('unknown-control', bad)
    bad = data.copy()
    struct.pack_into('<I', bad, controls, 0xffffffff)
    emit('oversized-control-count', bad)
    total += 2
print(f'{destination}: {total} invalid sound-bank checkpoints')
