// SPDX-License-Identifier: GPL-3.0-or-later
// Export decoded backgrounds for a private Gigapixel comparison batch.
// No original archives are rewritten. Run the room audit to prepare readers.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import {readSetFile} from '../build/reference-room/set.mjs';
import {decodeFrame, FrameBuffer} from '../build/reference-room/image.mjs';

const output = path.resolve('build/enhancements/visual-pilot');
const originals = path.join(output, 'original');
fs.mkdirSync(originals, {recursive: true});
fs.mkdirSync(path.join(output, 'enhanced'), {recursive: true});
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let i = 0; i < 8; ++i) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, bytes) {
  const tag = Buffer.from(type), length = Buffer.alloc(4), crc = Buffer.alloc(4);
  length.writeUInt32BE(bytes.length); crc.writeUInt32BE(crc32(Buffer.concat([tag, bytes])));
  return Buffer.concat([length, tag, bytes, crc]);
}
function png(width, height, rgba) {
  const header = Buffer.alloc(13); header.writeUInt32BE(width); header.writeUInt32BE(height, 4);
  header[8] = 8; header[9] = 6;
  const rows = Buffer.alloc(height * (width * 4 + 1));
  for (let y = 0; y < height; ++y) rgba.copy(rows, y * (width * 4 + 1) + 1, y * width * 4, (y + 1) * width * 4);
  return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]), chunk('IHDR', header),
    chunk('IDAT', zlib.deflateSync(rows)), chunk('IEND', Buffer.alloc(0))]);
}
const report = {format: 1, scope: 'Background-only 2x visual pilot; original gameplay masks remain authoritative.',
  bootMD5: crypto.createHash('md5').update(fs.readFileSync('game-data/full/BOOTFILE')).digest('hex'),
  expectedScale: 2, assets: [], sequences: []};
const known = new Map();
for (const room of process.argv.slice(2).length ? process.argv.slice(2) : ['C73', 'GSTAIR2']) {
  if (!/^[a-z0-9_-]+$/i.test(room)) throw new Error('Invalid room name');
  const source = `game-data/full/volumes/titanic2/DATA/${room.toUpperCase()}.SET`;
  const bytes = fs.readFileSync(source), set = readSetFile(new Uint8Array(bytes));
  function sequence(name, frames) {
    const fb = new FrameBuffer(), ids = [];
    for (const frame of frames) {
      const container = frame.frameContainerLoc;
      if (!container) continue;
      const decoded = decodeFrame(set.file.containers[container].data, fb);
      const rgba = Buffer.alloc(fb.width * fb.height * 4);
      for (let p = 0; p < fb.width * fb.height; ++p) {
        const color = fb.pixels[p];
        for (let c = 0; c < 3; ++c) rgba[p * 4 + c] = color === 0 ? 0 : color === 255 ? 255 : set.paletteRaw[color * 8 + 3 + c * 2];
        rgba[p * 4 + 3] = 255;
      }
      // A decoded-content suffix keeps delta-context variants distinct.
      const file = `${room.toUpperCase()}-${container}-${hash(rgba).slice(0, 12)}.png`;
      if (!known.has(file)) {
        fs.writeFileSync(path.join(originals, file), png(fb.width, fb.height, rgba));
        known.set(file, report.assets.length);
        report.assets.push({file, source, sourceSHA256: hash(bytes), container,
          width: fb.width, height: fb.height, decodedSHA256: hash(rgba),
          depthSHA256: decoded.hasZ ? hash(fb.zPixels.subarray(0, fb.width * fb.height)) : null});
      }
      ids.push({asset: known.get(file), camera: {x: frame.posX16, y: frame.posZ16, z: frame.posY16, degree: frame.axisX8}, motion: frame.motionInfo});
    }
    if (ids.length) report.sequences.push({name: `${room}/${name}`, frames: ids});
  }
  for (const scene of set.scenes) scene.turns.slice(0, 2).forEach((ring, i) => sequence(`${scene.sceneName}/${i ? 'left' : 'right'}`, ring.frames));
  for (const road of set.transitions) road.frameRegisters.forEach((register, i) => sequence(`${road.transitionName}/${i}`, register.frames));
}
fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(report, null, 2) + '\n');
const template = fs.readFileSync('scummvm/devtools/dreamfactory/enhancement-compare.html', 'utf8');
fs.writeFileSync(path.join(output, 'compare.html'), template.replace('/* MANIFEST */ null', JSON.stringify(report).replaceAll('<', '\\u003c')));
fs.writeFileSync(path.join(output, 'README.txt'),
  'Private C73 / Grand Staircase visual pilot\n\n' +
  '1. Batch the PNG files in original/ through Gigapixel at exactly 2x.\n' +
  '2. Save PNG output in enhanced/ with exactly the same filenames.\n' +
  '3. Open compare.html to compare stills and play whole turn/walk sequences.\n' +
  '4. Run python tools/verify-enhancement-images.py to validate the batch.\n\n' +
  'This is an asset-production workbench, not a live HD graphics pack yet.\n' +
  'No original archives or depth/click masks are changed. Review every frame\n' +
  'for unstable textures, altered lettering and details before accepting it.\n');
console.log(`Visual pilot: ${report.assets.length} PNGs, ${report.sequences.length} sequences; ${output}`);
