# SPDX-License-Identifier: GPL-3.0-or-later
"""Hash-checked original live sound controls; never executes the game binaries."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('context', Path(__file__).with_name('inspect-stage-context.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
filename, digest, _ = reader.RELEASES['full']
reader.RELEASES = {
    'full': (filename, digest, [
        (0x425640, 0xe5, 'retail pan updates active buffers'),
        (0x425740, 0x99, 'retail loop setter updates physical channels 0-2'),
        (0x401d40, 0x50, 'retail queues loop enable/disable without stopping playback'),
        (0x4254b0, 0x60, 'retail bank voice copies descriptor levels and loop flag'),
    ]),
    'demo': ('TI.EXE', '70fe81036870218315696f4e5d176ce24103d93b5a82d27b45cca961bc849a63', [
        (0x426720, 0x128, 'demo pan updates active buffers'),
        (0x426860, 0x7a, 'demo loop setter updates physical channels 0-2'),
        (0x401af0, 0x50, 'demo queues loop enable/disable without stopping playback'),
        (0x426530, 0x80, 'demo bank voice copies descriptor levels and loop flag'),
    ]),
    'dust': ('DF.EXE', 'd8a22e266c7307e550bd7284d2743947765303348a6b33cda48534c467aa95fe', [
        (0x403950, 0x90, 'Dust pan script wrapper resolves the bank descriptor'),
        (0x4039e0, 0x40, 'Dust pan setter dispatch ends after storing descriptor pan'),
        (0x403010, 0xe, 'Dust pan stores the descriptor field without updating gains'),
        (0x41ce90, 0x26, 'Dust PCM setup copies only 0x27 bytes, excluding descriptor pan at +31'),
        (0x402ee0, 0x9d, 'Dust volume gains use equal left/right levels, without descriptor pan'),
        (0x403030, 0x7a, 'Dust loop setter updates physical channels 0-2'),
        (0x41d150, 0x43, 'Dust queues loop enable/disable'),
        (0x41d496, 0x26, 'Dust services pending loop change during active playback'),
        (0x41d8c0, 0x48, 'Dust clears repeat link but retains the current PCM buffer'),
        (0x402e40, 0x6a, 'Dust bank voice copies descriptor levels and loop flag'),
    ]),
}
for edition in reader.RELEASES:
    reader.inspect(edition)
