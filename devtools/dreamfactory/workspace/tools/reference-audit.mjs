// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the unmodified reference decoder after transpiling its TS modules.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const source = path.resolve('references/dreamrefactory/engine/src/df');
const dest = path.resolve('build/reference');
fs.mkdirSync(dest, { recursive: true });
for (const name of ['byte-order', 'binary', 'version', 'container', 'set', 'set-v1', 'image']) {
  const text = fs.readFileSync(path.join(source, name + '.ts'), 'utf8');
  const js = ts.transpileModule(text, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 } }).outputText;
  fs.writeFileSync(path.join(dest, name + '.mjs'), js.replace(/from "(\.\/[^".]+(?:-[^".]+)?)"/g, 'from "$1.mjs"'));
}
const { readSetFile } = await import(pathToFileURL(path.join(dest, 'set.mjs')));
const { readSetFileV1 } = await import(pathToFileURL(path.join(dest, 'set-v1.mjs')));
const { decodeFrame, FrameBuffer } = await import(pathToFileURL(path.join(dest, 'image.mjs')));
for (const edition of process.argv.slice(2).length ? process.argv.slice(2) : ['demo', 'full', 'dust']) {
  const folder = path.resolve('game-data', edition);
  let total = 0;
  for (const file of fs.readdirSync(folder).filter(f => /\.set$/i.test(f)).sort()) {
    const data = new Uint8Array(fs.readFileSync(path.join(folder, file)));
    const set = edition === 'dust' ? readSetFileV1(data) : readSetFile(data);
    const sequences = [];
    if (edition === 'dust') {
      sequences.push(...set.transitions.map(t => t.frames));
      // Match authored scene-table order and per-cell departure stills.
      const scenes = new Map();
      for (const t of set.transitions) {
        const key = `${t.from.x},${t.from.z}`;
        if (!scenes.has(key)) scenes.set(key, new Map());
        const views = scenes.get(key);
        if (!views.has(t.from.facing)) views.set(t.from.facing, -1);
        if (t.departureStill >= 0) views.set(t.from.facing, t.departureStill);
      }
      for (const scene of set.scenes) { const views = scenes.get(`${scene.x},${scene.z}`); if (views) for (const still of views.values()) if (still >= 0) sequences.push([still]); }
    } else {
      for (const scene of set.scenes) for (const turn of scene.turns) sequences.push(turn.frames.map(f => f.frameContainerLoc));
      for (const road of set.transitions) for (const walk of road.frameRegisters) sequences.push(walk.frames.map(f => f.frameContainerLoc));
    }
    let hash = 2166136261, depthHash = 2166136261, depthCount = 0, count = 0;
    for (const sequence of sequences) {
      const buffer = new FrameBuffer();
      for (const id of sequence) {
        if (!id) continue; // 0 means no image, rather than container 0's header.
        const frame = decodeFrame(set.file.containers[id].data, buffer);
        for (let p = 0; p < frame.width * frame.height; ++p) hash = Math.imul(hash ^ buffer.pixels[p], 16777619) >>> 0;
        if (frame.hasZ) {
          const data = set.file.containers[id].data, view = new DataView(data.buffer,data.byteOffset,data.byteLength);
          for(let y=0;y<frame.height;++y) {
            const row=frame.zOffset+view.getUint16(frame.zOffset+y*2,true), runs=data[row];
            let width=0;for(let r=0;r<runs;++r) width+=data[row+1+r*2];
            if(width!==frame.width) throw new Error(`${file}/${id}: depth row ${y} width ${width} != ${frame.width}`);
          }
          ++depthCount;
          for(let p=0;p<frame.width*frame.height;++p) depthHash=Math.imul(depthHash^buffer.zPixels[p],16777619)>>>0;
        }
        ++count;
      }
    }
    console.log(`VERIFY ${file} frames=${count} fnv1a=${hash.toString(16).padStart(8, '0')}`);
    console.log(`DEPTH ${file} frames=${depthCount} fnv1a=${depthHash.toString(16).padStart(8, '0')}`);
    total += count;
  }
  console.log(`VERIFY PASS edition=${edition} frames=${total}`);
}
