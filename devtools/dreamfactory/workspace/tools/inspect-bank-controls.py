# SPDX-License-Identifier: GPL-3.0-or-later
"""Hash-checked bank lifetime and theme controls; never runs game binaries."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('context', Path(__file__).with_name('inspect-stage-context.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES = {
    'full': (filename, digest, [
        (0x44c880, 0xdc, 'retail open allocates and appends a bank'),
        (0x44cd10, 0xf9, 'retail close removes the first matched bank instance'),
        (0x44de60, 0x75, 'retail lookup selects the first matching internal name'),
        (0x44d260, 0xf3, 'retail theme setter changes stored and ordered music descriptors'),
        (0x44d630, 0x95, 'retail theme getter reads first stored music record; no music returns zero'),
        (0x425530, 0xf5, 'retail volume updates only physical channels with matching PCM identity'),
        (0x44da20, 0x140, 'retail track enumeration includes all open bank instances'),
    ]),
    'demo': ('TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
        (0x44cf10, 0xdc, 'demo open allocates and appends then renumbers descriptors'),
        (0x44d3d0, 0xf9, 'demo close removes the first matched bank instance'),
        (0x44e520, 0x75, 'demo lookup selects the first matching internal name'),
        (0x44d920, 0xf3, 'demo theme setter changes stored and ordered music descriptors'),
        (0x44dcf0, 0x95, 'demo theme getter reads first stored music record; no music returns zero'),
        (0x4265d0, 0x138, 'demo volume updates only physical channels with matching PCM identity'),
        (0x44e0e0, 0x140, 'demo track enumeration includes all open bank instances'),
    ]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe', [
        (0x4030b0, 0xe3, 'Dust open allocates and appends then renumbers descriptors'),
        (0x42d5f0, 0x70, 'Dust archive open consumes an inactive cached handle or allocates a new one'),
        (0x4033a0, 0x10f, 'Dust close removes the first matched bank instance'),
        (0x404150, 0x71, 'Dust lookup selects the first matching internal name'),
        (0x403840, 0x104, 'Dust theme setter changes stored and ordered music descriptors'),
        (0x403b20, 0x95, 'Dust theme getter reads first stored music record; no music returns zero'),
        (0x402ed0, 0x126, 'Dust volume updates only physical channels with matching PCM identity'),
        (0x403e70, 0xd7, 'Dust track enumeration includes all open bank instances'),
        (0x403212, 0x13a, 'Dust loads one-shots and music into separate descriptor arrays'),
        (0x403d40, 0x130, 'Dust sound count/index use only the one-shot array'),
        (0x404070, 0xe0, 'Dust named sound lookup walks only each bank one-shot array'),
    ]),
}
for edition in reader.RELEASES:
    reader.inspect(edition)
