# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only iris disassembly of the supplied, SHA-256-checked executables.

Titanic computes one aperture/strip per requested step. Dust instead uses
six closing strips or sixteen opening apertures and divides its delay among
those fixed passes. Both use rectangular copies, not an elliptical mask.
Requires the existing Capstone installation in dependencies/python.
"""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'stage_context', Path(__file__).with_name('inspect-stage-context.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)

ranges = {
    'full': [(0x43b830, 0x240, 'iris close/open geometry and 60 Hz waits')],
    'dust': [(0x40ef90, 0x230, 'six close passes / sixteen open passes'),
             (0x438240, 0x36, 'delay converts ticks to milliseconds')],
}
for edition, selected in ranges.items():
    filename, digest, _ = inspector.RELEASES[edition]
    inspector.RELEASES[edition] = filename, digest, selected
    inspector.inspect(edition)
