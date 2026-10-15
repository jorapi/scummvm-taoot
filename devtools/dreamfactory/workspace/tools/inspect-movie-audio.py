# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked evidence for original movie cue voice ownership."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('context', Path(__file__).with_name('inspect-stage-context.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES = {
    'full': (filename, digest, [
        (0x44ac20, 0x151, 'retail missing-cue early return, embedded loop flag and shared voice playback'),
        (0x448f4e, 0x70, 'retail preloaded cues also apply the loop flag'),
        (0x4492eb, 0xb0, 'retail segment cleanup unconditionally halts shared voice'),
        (0x4254b0, 0x60, 'retail voice playback selects physical channel zero'),
    ]),
    'demo': ('TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
        (0x44b2e0, 0x13a, 'demo movie cue resolution and shared voice playback without retail loop-flag dispatch'),
        (0x449b39, 0x50, 'demo segment cleanup halts shared voice'),
        (0x426530, 0x80, 'demo voice playback selects physical channel zero'),
    ]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe', [
        (0x419530, 0xbb, 'Dust embedded and lazy movie cues use shared voice'),
        (0x41935a, 0xb0, 'Dust segment cleanup halts voice before loading the next segment'),
        (0x4027d0, 0x5d, 'Dust halt argument four selects voice channel zero'),
        (0x402e40, 0x6a, 'Dust voice playback selects physical channel zero'),
    ]),
}
for edition in reader.RELEASES:
    reader.inspect(edition)
