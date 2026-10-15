# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only evidence for DreamFactory's two physical sound-effect slots.

Reuse the hash-guarded PE reader from the context investigation. No original
executable is run. Selection compares a descriptor priority ID, rather than
allocating an additional mixer voice for every multiplesound call.
"""
import importlib.util
from pathlib import Path

path = Path(__file__).with_name('inspect-stage-context.py')
spec = importlib.util.spec_from_file_location('original_context_reader', path)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES = {
    'full': (filename, digest, [
        (0x43c9c2, 0x49, 'single/multiple/dual script dispatch'),
        (0x44cf70, 0xbd, 'wrappers resolve the sound descriptor'),
        (0x424a50, 0x13e, 'loaded descriptor word zero and PCM fields'),
        (0x44cc56, 0x4d, 'bank construction overwrites word zero with record index plus one'),
        (0x44ddbe, 0x50, 'lazy loading preserves the bank-local sound ID'),
        (0x425510, 0x1b, 'descriptor ID getter and setter'),
        (0x424c90, 0x30, 'completed-channel polling clears descriptor word zero'),
        (0x425150, 0x1d2, 'single/multiple arbitration between slots 1 and 2'),
        (0x425330, 0xcd, 'dual sound can replace both slots'),
        (0x425400, 0xa4, 'both sound unconditionally replaces both slots'),
        (0x401860, 0x106, 'physical-channel setup replaces the old buffer chain'),
    ]),
    'demo': ('TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
        (0x425be0, 0x140, 'demo new descriptor defaults: loop off, volume 255 and pan 128'),
        (0x426240, 0x300, 'demo routines differ: 426440 aliases single; 426450 replaces lowest'),
        (0x415b80, 0x110, 'bank loading assigns a one-based record ordinal at descriptor +39'),
        (0x44e3f0, 0x130, 'lazy loading preserves descriptor priority'),
        (0x44cf10, 0xe0, 'bank open appends and renumbers all descriptor priorities'),
        (0x44d3d0, 0x100, 'bank close compacts the table without renumbering priorities'),
        (0x44e670, 0x90, 'priority becomes bank ordinal shifted 16 OR local ordinal'),
        (0x4265b0, 0x20, 'descriptor priority getter and setter'),
    ]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe', [
        (0x432052, 0x82, 'Dust script single/multiple/dual/both/voice dispatch'),
        (0x4035d0, 0x100, 'Dust wrappers resolve and call the corresponding playback routine'),
        (0x402b90, 0x2b0, 'Dust single/multiple arbitration; dual aliases single; both replaces lowest'),
        (0x402550, 0x170, 'Dust descriptor ID initially equals the sample container index'),
        (0x4030b0, 0xe3, 'Dust bank open appends then renumbers'),
        (0x40329a, 0x33, 'Dust one-shot containers and local IDs start at one'),
        (0x4033a0, 0x110, 'Dust close compacts banks without renumbering'),
        (0x404290, 0x90, 'Dust priority uses bank ordinal shifted 16 OR local ordinal'),
        (0x4026c0, 0x30, 'Dust completed-slot polling clears descriptor IDs'),
        (0x41ce50, 0xd0, 'Dust physical playback replaces the old buffer chain'),
    ])
}
for edition in reader.RELEASES:
    reader.inspect(edition)
