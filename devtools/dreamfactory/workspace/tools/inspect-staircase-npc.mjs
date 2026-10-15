// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only reproduction using the reference SET decoder and actor renderer.
// Run room/cast audits first to prepare build/reference-room and reference-cast.
// This tests a representative crowd sprite at the three authored landing stars;
// it does not identify the randomly selected NPC seen in a particular save.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {readSetFile} from '../build/reference-room/set.mjs';
import {decodeFrame, FrameBuffer} from '../build/reference-room/image.mjs';
import {readCstFile} from '../build/reference-cast/df/cst.mjs';
import {LoadedCast, ActorInstance, ActorRuntime} from '../build/reference-cast/runtime/actors.mjs';

const file = 'game-data/full/volumes/titanic2/DATA/GSTAIR2.SET';
const bytes = fs.readFileSync(file);
const set = readSetFile(new Uint8Array(bytes));
const road = set.transitions.find(r => r.transitionName === 'Road58');
const frame = road.frameRegisters[0].frames[0];
const scene = set.scenes.find(s => s.views.some(v => v.viewID === road.viewIDstart));
const view = scene.views.findIndex(v => v.viewID === road.viewIDstart);
const stand = scene.turns[1].frames.find(f => f.motionInfo > 0 && f.viewID === view);
assert.equal(frame.posY16, 7645);
assert.equal(stand.posY16, 5976);
const image = new FrameBuffer();
assert.equal(decodeFrame(set.file.containers[frame.frameContainerLoc].data, image).hasZ, true);
const cast = new LoadedCast('extra', readCstFile(new Uint8Array(fs.readFileSync('game-data/full/EXTRA.CST'))));
const renderer = new ActorRuntime();
const palette = new Uint8ClampedArray(1024).fill(255);
const results = [];
for (const star of set.actors.filter(s => /^ex\.a\.[123]$/.test(s.identifier))) {
  const actor = new ActorInstance(cast.cst.members.find(m => m.name === 'bruce1'), cast);
  Object.assign(actor, {visible: true, scale: 8300, worldX: star.positionX,
    worldY: star.positionZ, worldZ: star.positionY, deg: 0, poseName: 'stand', zclip: -1500});
  renderer.actors.clear();
  renderer.actors.set('probe', actor);
  function visiblePixels(height) {
    const rgba = new Uint8ClampedArray(512 * 264 * 4);
    renderer.composite(rgba, 512, 264, palette,
      {x: frame.posX16, y: frame.posZ16, z: height, deg: frame.axisX8,
        f: 256, cx: 256, cy: 132, clipW: 512, clipH: 264},
      {z: image.zPixels, w: image.width, h: image.height,
        scale: set.zFarMax / set.zLevelCount, levels: set.zLevelCount});
    let count = 0;
    for (let i = 0; i < rgba.length; i += 4) if (rgba[i]) ++count;
    return count;
  }
  const before = visiblePixels(frame.posY16), after = visiblePixels(stand.posY16);
  assert(before > 1000);
  assert.equal(after, 0);
  results.push({star: star.identifier, representativeActor: 'bruce1', beforePixels: before, afterPixels: after});
}
assert.equal(results.length, 3);
const report = {scope: 'Reference-renderer reproduction; Android runtime confirmation pending.',
  source: file, sha256: crypto.createHash('sha256').update(bytes).digest('hex'),
  road: 'Road58', departure: `${scene.sceneName}/${scene.views[view].viewName}`,
  rawHeight: frame.posY16, correctedHeight: stand.posY16, results};
fs.writeFileSync('build/staircase-npc-reference.json', JSON.stringify(report, null, 2) + '\n');
console.log('Staircase NPC reference reproduction PASS', results);
