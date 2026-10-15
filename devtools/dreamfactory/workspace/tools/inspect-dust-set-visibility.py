# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked evidence for Dust room visibility lifecycle."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['dust']
inspector.RELEASES['dust'] = filename, digest, [
    (0x432156, 0x34, 'opensetfile and closesetfile opcode targets'),
    (0x4327bc, 0x1a, 'setvisible setter opcode target'),
    (0x421ab0, 0x360, 'opensetfile handler'),
    (0x421e10, 0x200, 'closesetfile handler'),
    (0x423200, 0x100, 'setvisible setter'),
]
inspector.inspect('dust')
