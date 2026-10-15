# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked Titanic positional-audio evidence; never runs TI.EXE."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('context', Path(__file__).with_name('inspect-stage-context.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES['full'] = filename, digest, [
    (0x444430, 0x298, 'retail cricket service: integer distance/volume, live descriptor updates and radius stop'),
    (0x444790, 0x67, 'retail folded bearing pan'),
    (0x4446d0, 0xbd, 'retail two-loop priority selection'),
    (0x4443e0, 0x41, 'retail make replaces name then scans for first inactive slot'),
    (0x442770, 0xc0, 'retail stop clears matching table slots without compaction'),
    (0x444800, 0x80, 'retail count/index enumerate active slots in table order'),
    (0x444b70, 0x200, 'retail integer-ratio bearing and shortest angle'),
    (0x435950, 0x53, 'retail integer square root'),
    (0x425540, 0xe5, 'retail volume setter updates active physical buffers'),
    (0x425640, 0xe5, 'retail pan setter updates active physical buffers'),
    (0x425730, 0xc, 'retail loop flag getter, not a playback-active query'),
    (0x435810, 0x1b, 'retail random jitter is 1 through bound, or zero for nonpositive'),
]
reader.RELEASES['demo'] = 'TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
    (0x444cb0, 0x40, 'demo first-free cricket allocation'),
    (0x444cf0, 0x298, 'demo cricket service: same integer distance/volume and live updates'),
    (0x444f90, 0x67, 'demo folded bearing pan'),
    (0x445000, 0x80, 'demo active cricket count/index'),
]
for edition in ('full', 'demo'):
    reader.inspect(edition)
