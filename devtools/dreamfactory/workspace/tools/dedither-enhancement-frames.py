#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Export edge-aware de-dithered copies, at original dimensions and filenames.

Dependencies: numpy, Pillow (also loaded from dependencies/image-processing).
No AI, sharpening, resizing, random noise, or modification of original assets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
LOCAL_DEPS = ROOT / 'dependencies/image-processing'
if LOCAL_DEPS.is_dir():
    sys.path.insert(0, str(LOCAL_DEPS))
import numpy as np
from PIL import Image


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dedither(rgb, spatial_sigma, range_sigma):
    """Joint bilateral filter with a lightly smoothed RGB edge guide.

    The guide suppresses one-pixel checkerboard variation so those dots do not
    protect themselves as edges. Range weights still limit cross-edge mixing.
    Work in encoded RGB: palette dithering approximates these displayed colors.
    """
    height, width, _ = rgb.shape
    padded = np.pad(rgb, ((1, 1), (1, 1), (0, 0)), mode='reflect')
    guide = np.zeros_like(rgb)
    kernel = (1, 2, 1)
    for y in range(3):
        for x in range(3):
            guide += padded[y:y + height, x:x + width] * (kernel[y] * kernel[x] / 16)
    radius = 2
    guide_pad = np.pad(guide, ((radius, radius), (radius, radius), (0, 0)), mode='reflect')
    source_pad = np.pad(rgb, ((radius, radius), (radius, radius), (0, 0)), mode='reflect')
    total = np.zeros_like(rgb)
    weight_total = np.zeros((height, width), dtype=np.float32)
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            y, x = dy + radius, dx + radius
            neighbor_guide = guide_pad[y:y + height, x:x + width]
            distance = np.mean((neighbor_guide - guide) ** 2, axis=2)
            weights = np.exp(-distance / (2 * range_sigma ** 2)
                             - (dx * dx + dy * dy) / (2 * spatial_sigma ** 2))
            total += source_pad[y:y + height, x:x + width] * weights[:, :, None]
            weight_total += weights
    return np.clip(np.rint(total / weight_total[:, :, None]), 0, 255).astype(np.uint8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', nargs='?', type=Path, default=ROOT / 'build/enhancements/visual-pilot')
    parser.add_argument('--spatial-sigma', type=float, default=1.15)
    parser.add_argument('--range-sigma', type=float, default=24.0, help='RGB range tolerance in 0-255 units')
    parser.add_argument('--sequence', help='Export only a named sequence for preview')
    args = parser.parse_args()
    if not (0.1 <= args.spatial_sigma <= 3 and 1 <= args.range_sigma <= 80):
        parser.error('spatial sigma must be 0.1..3; range sigma must be 1..80')
    directory = args.directory.resolve()
    manifest = json.loads((directory / 'manifest.json').read_text())
    assets = manifest['assets']
    if args.sequence:
        sequence = next((s for s in manifest['sequences'] if s['name'] == args.sequence), None)
        if sequence is None:
            parser.error('Unknown sequence')
        assets = [manifest['assets'][i] for i in sorted({f['asset'] for f in sequence['frames']})]
    output = directory / 'dedithered'
    output.mkdir(exist_ok=True)
    records = []
    for index, asset in enumerate(assets):
        name = asset['file']
        if Path(name).name != name or not name.lower().endswith('.png'):
            raise ValueError('Unsafe manifest filename')
        source, target = directory / 'original' / name, output / name
        with Image.open(source) as image:
            image.load()
            if image.size != (asset['width'], asset['height']):
                raise ValueError('Source dimensions differ from manifest: ' + name)
            rgba = np.asarray(image.convert('RGBA')).copy()
        if hashlib.sha256(rgba.tobytes()).hexdigest() != asset['decodedSHA256']:
            raise ValueError('Original pixels differ from export manifest: ' + name)
        result = dedither(rgba[:, :, :3].astype(np.float32), args.spatial_sigma, args.range_sigma)
        rgba[:, :, :3] = result
        temporary = target.with_suffix('.part')
        Image.fromarray(rgba).save(temporary, format='PNG')
        with Image.open(temporary) as checked:
            checked.load()
            if checked.size != (asset['width'], asset['height']):
                raise ValueError('Output dimensions changed')
        temporary.replace(target)
        records.append(dict(file=name, sourceSHA256=digest(source), outputSHA256=digest(target),
                            width=asset['width'], height=asset['height']))
        if (index + 1) % 25 == 0 or index + 1 == len(assets):
            print(f'{index + 1}/{len(assets)} exported', flush=True)
    report = dict(algorithm='5x5 joint bilateral, 3x3 binomial RGB guide, reflected borders',
                  spatialSigma=args.spatial_sigma, rangeSigma=args.range_sigma,
                  dimensions='unchanged', alpha='preserved', sequence=args.sequence,
                  manifestSHA256=digest(directory / 'manifest.json'), assets=records,
                  limitations='May soften fine texture and narrow lines; review moving sequences before upscaling.')
    (output / 'dedither-report.json').write_text(json.dumps(report, indent=2) + '\n')
    template = (ROOT / 'scummvm/devtools/dreamfactory/enhancement-compare.html').read_text()
    template = template.replace('/* MANIFEST */ null', json.dumps(manifest).replace('<', '\\u003c'))
    template = template.replace("'enhanced/'", "'dedithered/'")
    template = template.replace('Left of the slider is the enhanced image.', 'Left of the slider is the de-dithered image.')
    template = template.replace('Enhanced / Original', 'De-dithered / Original')
    template = template.replace('>Enhanced</button>', '>De-dithered</button>')
    template = template.replace('Put 2× PNG outputs in <code>enhanced/</code> using the original filenames.',
                                'Same-size cleaned PNGs are in <code>dedithered/</code>.')
    template = template.replace('checking enhanced image', 'checking de-dithered image').replace(' enhanced`', ' de-dithered`')
    (directory / 'compare-dedithered.html').write_text(template)
    print('Output:', output, flush=True)


if __name__ == '__main__':
    main()
