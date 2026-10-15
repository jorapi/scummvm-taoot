# SPDX-License-Identifier: GPL-3.0-or-later
"""Create an invalid DFSV v3-v8 armed flag for transactional rejection testing."""
import hashlib
from pathlib import Path
import struct
import sys

source, destination = map(Path, sys.argv[1:])
data = bytearray(source.read_bytes())
assert data[:4] == b'DFSV' and struct.unpack_from('<I', data, 4)[0] in (3, 4, 5, 6, 7, 8)
assert struct.unpack_from('<I', data, 8)[0] == len(data) - 28
assert hashlib.md5(data[28:]).digest() == data[12:28]

def string(text):
    raw = text.encode('ascii')
    return struct.pack('<I', len(raw)) + raw

# This exact record is authored by Titanic's turbine-room setup.
record = string('steam1') + string('turb') + struct.pack('<iiiii', 8440, 13322, 3500, 200, 200)
start = data.find(record, 28)
assert start >= 28 and data.find(record, start + 1) == -1
armed = start + len(record) + 8 + 1  # count u64, paused byte, armed byte
assert data[armed] in (0, 1)
data[armed] = 2
data[12:28] = hashlib.md5(data[28:]).digest()
destination.write_bytes(data)
print('Invalid armed flag fixture written')
