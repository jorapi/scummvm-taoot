# SPDX-License-Identifier: GPL-3.0-or-later
"""Hash-checked output-device volume evidence; never runs original binaries."""
import importlib.util
import struct
from pathlib import Path

spec = importlib.util.spec_from_file_location('context', Path(__file__).with_name('inspect-stage-context.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES = {
    'full': (filename, digest, [
        (0x4249b0, 0x19, 'retail wavevolume setter dispatch'),
        (0x4057d0, 0x11, 'retail dispatch through backend volume entry'),
        (0x4065d3, 0x3b, 'retail backend volume entry is 4072d0'),
        (0x4072d0, 0x80, 'retail scales device volume by n/9 and calls waveOutSetVolume'),
    ]),
    'demo': ('TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
        (0x425b40, 0x19, 'demo wavevolume setter dispatch'),
        (0x405770, 0x11, 'demo dispatch through backend volume entry'),
        (0x407f50, 0x80, 'demo scales device volume by n/9 and calls waveOutSetVolume'),
    ]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe', [
        (0x4024e0, 0x18, 'Dust wavevolume setter calls 421960'),
        (0x421960, 0x80, 'Dust scales device volume by n/9 and calls waveOutSetVolume'),
    ]),
}


def imports(edition):
    """Resolve IAT addresses to names instead of inferring an API from code."""
    filename, _, _ = reader.RELEASES[edition]
    data = (reader.ROOT / 'game-data' / edition / 'installed' / filename).read_bytes()
    pe = struct.unpack_from('<I', data, 60)[0]
    optional = pe + 24
    base = struct.unpack_from('<I', data, optional + 28)[0]
    sec = optional + struct.unpack_from('<H', data, pe + 20)[0]
    sections = [struct.unpack_from('<III', data, sec + 40 * i + 12)
                for i in range(struct.unpack_from('<H', data, pe + 6)[0])]

    def raw(rva):
        for start, length, offset in sections:
            if start <= rva < start + length:
                return offset + rva - start
        raise ValueError(f'RVA outside file-backed section: {rva:x}')

    at = raw(struct.unpack_from('<I', data, optional + 104)[0])
    found = set()
    while True:
        original, _, _, dll, thunk = struct.unpack_from('<5I', data, at)
        if not dll:
            break
        offset, index = raw(original or thunk), 0
        while True:
            target = struct.unpack_from('<I', data, offset + index * 4)[0]
            if not target:
                break
            if not target & 0x80000000:
                name_at = raw(target) + 2
                name = data[name_at:data.index(b'\0', name_at)].decode('ascii')
                if name in ('waveOutSetVolume', 'waveOutGetVolume'):
                    found.add(name)
                    print(f'{edition} IAT {base + thunk + index * 4:08x} {name}')
            index += 1
        at += 20
    if found != {'waveOutSetVolume', 'waveOutGetVolume'}:
        raise ValueError(f'{edition}: missing wave volume imports')


for edition in reader.RELEASES:
    reader.inspect(edition)  # Verifies the executable hash before reading imports.
    imports(edition)
