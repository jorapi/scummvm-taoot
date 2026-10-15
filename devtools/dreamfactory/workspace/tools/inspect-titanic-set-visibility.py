# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked Titanic room visibility initialization evidence."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['full']
inspector.RELEASES['full'] = filename, digest, [
    (0x407630, 0x200, 'retail opensetfile stub and room initialization'),
]
inspector.RELEASES['demo'] = 'TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
    (0x408240, 0xc0, 'demo opensetfile passes room struct at 0x488fbc'),
    (0x4082d0, 0x370, 'demo room initialization through room pointer'),
    (0x408680, 0x100, 'demo room close through room pointer'),
    (0x4093d0, 0x46, 'demo setvisible setter at room struct +2'),
]
for edition in ('full', 'demo'):
    inspector.inspect(edition)
