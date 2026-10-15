# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked evidence for Dust flat lifecycle containment."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['dust']
inspector.RELEASES['dust'] = filename, digest, [
    (0x416dd0, 0x350, 'flat message builds flat and stage script contexts'),
    (0x415bb0, 0xc0, 'openflat dispatch uses the flat message chain'),
]
inspector.inspect('dust')
