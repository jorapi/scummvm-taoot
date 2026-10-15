// SPDX-License-Identifier: GPL-3.0-or-later
// Plan physical Dust grid navigation from the room's authored transition register.
import fs from 'node:fs';
import { readSetFileV1 } from '../build/reference-room/set-v1.mjs';
const [room, fromName, fromView, toName, toView] = process.argv.slice(2);
if (!room || !/^[a-z0-9_-]+$/i.test(room)) throw new Error('Usage: dust-route.mjs ROOM FROM_CELL FROM_VIEW TO_CELL TO_VIEW');
const set = readSetFileV1(new Uint8Array(fs.readFileSync(`game-data/dust/${room.toUpperCase()}.SET`)));
const headings = [192, 64, 0, 128], names = ['north','south','east','west'];
const scene = name => {
  const exact = set.scenes.find(s => s.name.toLowerCase() === name?.toLowerCase());
  if (exact) return exact;
  const normalized = name?.toLowerCase().replace(/^scene\s+/, '');
  const found = set.scenes.find(s => s.name.toLowerCase() === `scene ${normalized}`);
  if (!found) throw new Error(`Unknown scene ${name}`);
  return found;
};
const facing = name => {
  const i = names.indexOf(name?.toLowerCase());
  if (i < 0) throw new Error(`Unknown direction ${name}`);
  return i + 1;
};
const key = s => `${s.x},${s.z},${s.facing}`;
const from = scene(fromName), to = scene(toName);
const start = { x:from.x,z:from.z,facing:facing(fromView) };
const goal = key({ x:to.x,z:to.z,facing:facing(toView) });
const edges = new Map();
for (const t of set.transitions) {
  const id = key(t.from);
  if (!edges.has(id)) edges.set(id, []);
  let action = 'key uparrow';
  if (t.kind === 'turn') {
    const delta = (headings[t.to.facing-1] - headings[t.from.facing-1] + 256) % 256;
    if (delta !== 64 && delta !== 192) throw new Error('Unsupported authored turn');
    action = `key ${delta === 64 ? 'rightarrow' : 'leftarrow'}`;
  }
  edges.get(id).push({ state:t.to, action });
}
const queue = [{ state:start,path:[] }], seen = new Set();
for (let i=0; i<queue.length; i++) {
  const {state,path} = queue[i], id=key(state);
  if (seen.has(id)) continue;
  seen.add(id);
  if (id === goal) {
    for (const step of path) {
      const s = set.scenes.find(s => s.x === step.state.x && s.z === step.state.z);
      if (!s) throw new Error('Transition references a missing scene');
      console.log(step.action);
      console.log(`expectscene ${s.name}`);
      console.log(`expectview ${names[step.state.facing-1]}`);
    }
    process.exit(0);
  }
  for (const step of edges.get(id) ?? []) queue.push({ state:step.state,path:[...path,step] });
}
throw new Error('No authored route between these viewpoints');
