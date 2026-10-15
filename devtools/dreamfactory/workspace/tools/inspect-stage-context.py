# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only disassembly of original DreamFactory stage message contexts.

Requires capstone in dependencies/python. Does not execute the game binaries.
The addresses below apply only to the SHA-256-identified supplied releases.
"""
import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'dependencies/python'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

RELEASES = {
    'full': ('TI.EXE', '09e4c9c6536660c303fd26f0fa01dc91e2583eb78b81243f8fb1b27ef761011f',
             [(0x4464a0, 0x380, 'button/flat/stage context construction'),
              (0x447710, 0x110, 'handler lookup advances context by 0x4e'),
              (0x40647b, 0x6a, 'me reads context+0x1e; target reads context+0x3e')]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe',
             [(0x416a80, 0x2ac, 'button/flat/stage context construction')]),
}


def inspect(edition):
    filename, expected, ranges = RELEASES[edition]
    data = (ROOT / 'game-data' / edition / 'installed' / filename).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected:
        raise ValueError(f'{edition}: executable differs from researched release ({digest})')
    pe = struct.unpack_from('<I', data, 60)[0]
    base = struct.unpack_from('<I', data, pe + 52)[0]
    sec = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for i in range(struct.unpack_from('<H', data, pe + 6)[0]):
        _, va, size, offset = struct.unpack_from('<IIII', data, sec + 40 * i + 8)
        sections.append((base + va, size, offset))

    def raw(address, size):
        for start, length, offset in sections:
            if start <= address and address + size <= start + length:
                return offset + address - start
        raise ValueError(f'Address outside file-backed section: {address:x}')

    print(edition, filename, digest)
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    for address, size, description in ranges:
        print(f'\n{address:08x}: {description}')
        offset = raw(address, size)
        for instruction in decoder.disasm(data[offset:offset + size], address):
            print(f'{instruction.address:08x} {instruction.mnemonic} {instruction.op_str}')


if __name__ == '__main__':
    editions = sys.argv[1:] or list(RELEASES)
    if any(edition not in RELEASES for edition in editions):
        raise SystemExit('Usage: inspect-stage-context.py [full|dust ...]')
    for edition in editions:
        inspect(edition)
