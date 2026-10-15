# SPDX-License-Identifier: GPL-3.0-or-later
"""Hash-checked movie/theme queue evidence; never executes original binaries."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('context', Path(__file__).with_name('inspect-stage-context.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES = {
    'full': (filename, digest, [
        (0x448b53, 0x30, 'retail starts each movie with previous-bed count minus one'),
        (0x448d46, 0x80, 'retail later empty-bed segments inherit; new beds clear previous movie music'),
        (0x448dc6, 0x147, 'retail copies authored bed order including repeats and links its repeat boundary'),
        (0x44939e, 0x3b, 'retail continues to another segment before final theme cleanup'),
        (0x449538, 0x3e, 'retail final movie cleanup halts theme channel only when a bed exists'),
        (0x425080, 0xd0, 'retail theme playback submits buffers to channel three'),
        (0x44caf8, 0xaa, 'retail bank themes preserve authored order and clamp the repeat root by record count'),
    ]),
    'demo': ('TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
        (0x426170, 0xd0, 'demo theme playback submits buffers to channel three'),
    ]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe', [
        (0x418c57, 0x25, 'Dust starts each movie with previous-bed count minus one'),
        (0x418d98, 0x84, 'Dust inherits a later empty bed and otherwise replaces previous movie music'),
        (0x418e0c, 0x115, 'Dust bed count, chunks after one-shots, authored order and repeat boundary'),
        (0x418f21, 0x22, 'Dust movie beds call the normal theme playback routine'),
        (0x41945e, 0x30, 'Dust next segments retain the bed; final cleanup stops theme channel'),
        (0x402ac0, 0xc4, 'Dust theme playback submits buffers to physical channel three'),
        (0x403308, 0x5a, 'Dust banks copy header+30 authored theme order and repeat from the first copied descriptor'),
        (0x402605, 0x9c, 'Dust movie PCM descriptors initialize theme gains to full volume'),
        (0x41d010, 0x26, 'Dust theme buffer submission'),
        (0x41d040, 0x88, 'Dust active theme submission appends to its existing buffer chain'),
        (0x4207d0, 0x72, 'Dust pending PCM nodes transfer in order and the last marked node becomes the repeat root'),
    ]),
}
for edition in reader.RELEASES:
    reader.inspect(edition)
