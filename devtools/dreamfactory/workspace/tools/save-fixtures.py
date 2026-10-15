#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Produce invalid native checkpoints for transactional load rejection tests."""
import argparse
import hashlib
from pathlib import Path
import struct


def fixtures(source, destination):
    data = bytearray(source.read_bytes())
    assert data[:4] == b'DFSV' and struct.unpack_from('<I', data, 8)[0] == len(data) - 28
    destination.mkdir(parents=True, exist_ok=True)

    def emit(name, value, digest=False):
        value = bytearray(value)
        if digest:
            struct.pack_into('<I', value, 8, len(value) - 28)
            value[12:28] = hashlib.md5(value[28:]).digest()
        (destination / (name + '.dfsv')).write_bytes(value)

    bad = data.copy(); bad[0] ^= 1; emit('magic', bad)
    bad = data.copy(); struct.pack_into('<I', bad, 4, 0xffffffff); emit('version', bad)
    bad = data.copy(); struct.pack_into('<I', bad, 8, 0xffffffff); emit('size', bad)
    bad = data.copy(); bad[-1] ^= 1; emit('checksum', bad)
    emit('truncated', data[:-1])
    emit('truncated-payload', data[:-1], True)
    # The initial string identifies the exact BOOTFILE, before any assets load.
    bad = data.copy(); bad[32] = ord('z'); emit('edition', bad, True)
    # Skip BOOT signature, engine version, eight resource paths and current CD.
    pos = 28
    length, = struct.unpack_from('<I', data, pos); pos += 4 + length + 4
    for _ in range(9):
        length, = struct.unpack_from('<I', data, pos); pos += 4 + length
    bad = data.copy(); struct.pack_into('<I', bad, pos, 0xffffffff)
    emit('global-count', bad, True)
    # Keep the logical room name valid while making its canonical source absent.
    needle = b'titanic1/DATA/BEDSIT1.SET'
    if needle in data:
        emit('missing-asset', data.replace(needle, b'titanic1/DATA/XXXXXXX.SET'), True)
    print(f'{destination}: {len(list(destination.glob("*.dfsv")))} invalid checkpoints')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    fixtures(args.source, args.destination)
