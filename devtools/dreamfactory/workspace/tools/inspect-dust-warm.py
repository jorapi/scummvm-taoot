# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked evidence for Dust actor/cast sprite warming."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['dust']
inspector.RELEASES['dust'] = filename, digest, [
    (0x4259b0, 0x77, 'actorwarm resolves one member'),
    (0x425a30, 0xa4, 'castwarm visits active members belonging to the cast'),
    (0x428680, 0x12b, 'warm every pose and picture without changing actor state'),
    (0x4131b0, 0xb0, 'shopwarm visits active props belonging to the named shop'),
    (0x415720, 0x130, 'propwarm visits every state and picture in its group'),
]
inspector.inspect('dust')
