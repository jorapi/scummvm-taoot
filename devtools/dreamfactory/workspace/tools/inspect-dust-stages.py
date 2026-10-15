# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only evidence for Dust selecting its initial flat before openstage."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
filename, digest, _ = inspector.RELEASES['dust']
inspector.RELEASES['dust'] = filename, digest, [
    (0x415850, 0xa0, 'openstagefile: load, select flat zero, dispatch lifecycle'),
    (0x417410, 0xfd, 'select flat and publish current index'),
    (0x415d30, 0x133, 'openstage followed by openflat if file/index retained'),
    (0x415ac0, 0xe9, 'gotoflat: close, select, open even for the same index'),
    (0x415bb0, 0xc0, 'openflat callback dispatch'),
]
inspector.inspect('dust')
