# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked Dust projectile evidence; never runs DF.EXE.

The v1 makeball opcode shares its number with v4 makecricket. Balls use a
16-slot table, signed-word positions/velocities, nested pause counts, gravity,
bounce and endball callbacks. Hitbox axis 1 is vertical, axis 2 horizontal.
"""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['dust']
inspector.RELEASES['dust'] = filename, digest, [
    (0x40fe00, 0xa0, 'motion service: walks then balls then loops'),
    (0x410000, 0x70, 'stop all or named ball'),
    (0x411050, 0x260, 'seven-argument makeball and slot insertion'),
    (0x4112b0, 0x4cc, 'physics and completion callback dispatch'),
    (0x411780, 0x260, 'swept per-axis actor and player collision'),
    (0x4119e0, 0x179, 'count/index balls and nested pause counters'),
    (0x40e6a0, 0x75, 'cell building test'),
    (0x4261a0, 0x103, 'hitbox setters: vertical then horizontal extent'),
    (0x4078c0, 0x1c0, 'camera XYZ versus ground-level player XYZ'),
    (0x406260, 0x27, 'camerahi reads room eye height'),
    (0x40dd90, 0x70, 'camera initialization with room eye height'),
    (0x40e004, 0xb6, 'camera setback applied after player position'),
]
inspector.inspect('dust')
