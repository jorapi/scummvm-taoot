# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only, hash-checked evidence for Dust's turn records and callbacks.

The turn builder queues an already-facing turn and replaces the previous
walk. Completion removes a turn-only record before dispatching endturn.
No original game executable is run.
"""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['dust']
inspector.RELEASES['dust'] = filename, digest, [
    (0x410690, 0x181, 'turn builder and previous-walk removal'),
    (0x40fed0, 0xb0, 'remove walk by actor name'),
    (0x410b80, 0x135, 'turn service: remove before endturn dispatch'),
]
inspector.inspect('dust')
