#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate an exported Gigapixel batch's exact names, dimensions and PNG CRCs."""
import argparse
import json
from pathlib import Path
import struct
import zlib


def png_size(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not PNG'
    pos = 8
    size = None
    ended = False
    while pos < len(data):
        n = struct.unpack_from('>I', data, pos)[0]
        kind, payload = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        assert len(payload) == n
        crc = struct.unpack_from('>I', data, pos + 8 + n)[0]
        assert zlib.crc32(kind + payload) == crc, 'PNG CRC mismatch'
        if kind == b'IHDR':
            assert n == 13
            size = struct.unpack_from('>II', payload)
        pos += n + 12
        if kind == b'IEND':
            ended = True
            break
    assert ended and size and pos == len(data), 'incomplete PNG'
    return size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', nargs='?', type=Path, default=Path('build/enhancements/visual-pilot'))
    parser.add_argument('--originals', action='store_true', help='Validate original exports before processing')
    args = parser.parse_args()
    manifest = json.loads((args.directory / 'manifest.json').read_text())
    folder = args.directory / ('original' if args.originals else 'enhanced')
    scale = 1 if args.originals else manifest['expectedScale']
    missing, invalid = [], []
    for asset in manifest['assets']:
        p = folder / asset['file']
        if not p.is_file():
            missing.append(asset['file'])
            continue
        try:
            actual = png_size(p)
            assert actual == (asset['width'] * scale, asset['height'] * scale), f'wrong dimensions: {actual}'
        except (AssertionError, ValueError, struct.error) as error:
            invalid.append(dict(file=asset['file'], error=str(error)))
    expected = {asset['file'] for asset in manifest['assets']}
    unexpected = sorted(p.name for p in folder.glob('*.png') if p.name not in expected)
    report = dict(expected=len(expected), missing=missing, invalid=invalid, unexpected=unexpected,
                  passed=not (missing or invalid or unexpected), scale=scale)
    (args.directory / ('original-validation.json' if args.originals else 'enhanced-validation.json')).write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(expected)-len(missing)} / {len(expected)} images present; {len(invalid)} invalid; {len(unexpected)} unexpected.')
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
