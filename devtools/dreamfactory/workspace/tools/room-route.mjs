// SPDX-License-Identifier: GPL-3.0-or-later
// Find a v4 room route using authored turn rings and road endpoints.
// Usage: node tools/room-route.mjs full TURK View116 View119
// Run tools/room-audit.mjs first to prepare the primary format reader.
import fs from 'node:fs';
import { readSetFile } from '../build/reference-room/set.mjs';

const [edition, room, startName, endName, volume = '2'] = process.argv.slice(2);
if (!['full', 'demo'].includes(edition) || !room || !startName || !endName)
  throw new Error('Usage: room-route.mjs full|demo ROOM START_VIEW END_VIEW [1|2]');
if (volume !== '1' && volume !== '2') throw new Error('Invalid retail volume');
if (!/^[a-z0-9_-]+$/i.test(room)) throw new Error('Invalid room name');
// The flat convenience aliases can name the CD1 tour variant. Retail story
// navigation uses CD2 DATA; the sinking chapter switches to CD1. Common
// rooms absent from that disc retain their flat compatibility alias.
const retailPath = `game-data/full/volumes/titanic${volume}/DATA/${room.toUpperCase()}.SET`;
const source = edition === 'full' && fs.existsSync(retailPath) ? retailPath : `game-data/${edition}/${room.toUpperCase()}.SET`;
const set = readSetFile(new Uint8Array(fs.readFileSync(source)));
const edges = new Map(), views = new Map();
for (const scene of set.scenes) {
  for (const view of scene.views) {
    views.set(view.viewID, { scene: scene.sceneName, name: view.viewName });
    edges.set(view.viewID, []);
  }
  for (const [direction, turn] of scene.turns.entries()) {
    if (direction > 1) break;
    const ring = turn.frames.filter(f => f.motionInfo > 0 && f.viewID >= 0)
      .map(f => scene.views[f.viewID].viewID);
    ring.forEach((id, index) => edges.get(id).push({
      id: ring[(index + 1) % ring.length], action: `key ${direction ? 'leftarrow' : 'rightarrow'}`
    }));
  }
}
for (const road of set.transitions) {
  if (road.frameRegisters[0]?.frames.length)
    edges.get(road.viewIDstart).push({ id: road.viewIDend, action: 'key uparrow' });
  if (road.frameRegisters[1]?.frames.length)
    edges.get(road.viewIDend).push({ id: road.viewIDstart, action: 'key uparrow' });
}
function find(name) {
  const matches = [...views].filter(([, v]) => v.name.toLowerCase() === name.toLowerCase());
  if (matches.length !== 1) throw new Error(`Expected one view named ${name}, found ${matches.length}`);
  return matches[0][0];
}
const start = find(startName), end = find(endName), queue = [{ id: start, path: [] }], seen = new Set();
for (let index = 0; index < queue.length; ++index) {
  const { id, path } = queue[index];
  if (seen.has(id)) continue;
  seen.add(id);
  if (id === end) {
    for (const step of path) {
      const view = views.get(step.id);
      console.log(`${step.action} # ${view.scene}/${view.name}`);
    }
    process.exit(0);
  }
  for (const next of edges.get(id)) queue.push({ id: next.id, path: [...path, next] });
}
throw new Error(`No authored route from ${startName} to ${endName}`);
