// SPDX-License-Identifier: GPL-3.0-or-later
// Plan physical navigation through an authored retail smokestack layout.
// No game state is written. The layout number comes from the native entry log.
import fs from 'node:fs';
import { readSetFile } from '../build/reference-room/set.mjs';
import { decodeScript, scriptToText } from '../build/reference-room-fixtures/script.mjs';

const maze = Number(process.argv[2]);
if (!Number.isInteger(maze) || maze < 1 || maze > 4)
  throw new Error('Usage: smokestack-route.mjs MAZE_NUMBER (1..4)');
const sets = [1, 2, 3].map(n => readSetFile(new Uint8Array(fs.readFileSync(
  `game-data/full/volumes/titanic2/DATA/SMSTACK${n}.SET`))));
const scripts = sets.map(s => s.file.containers.flatMap(c => {
  try { const t = scriptToText(decodeScript(c.data)); return t.includes('code ') ? [t] : []; }
  catch { return []; }
}).join('\n'));
const setup = scripts[1].match(new RegExp(
  `case ${maze}\\n\\t\\tstackmax = (\\d+)\\n\\t\\tswitch stacklevel\\n([\\s\\S]*?)\\t\\tendswitch`));
if (!setup) throw new Error('Cannot read authored maze layout');
const top = Number(setup[1]), blocks = new Map();
for (const m of setup[2].matchAll(/case (\d+)\n\t\t\tblocks = "([\d,]*)"/g))
  blocks.set(Number(m[1]), new Set(m[2].split(',').filter(Boolean)));
if (blocks.size !== top - 2) throw new Error('Incomplete authored floor layout');
const barriers = new Map();
const blocked = scripts[1].split('code pathblocked ')[1]?.split('code setupblocks ')[0];
if (!blocked) throw new Error('Cannot read authored barrier rules');
for (const m of blocked.matchAll(/case "(view\d+)"\n\t+if substring \(blocks, "(\d+)"\)/g))
  barriers.set(m[1].toLowerCase(), m[2]);
if (barriers.size !== 16) throw new Error('Incomplete authored barrier rules');

const rooms = sets.map(set => {
  const views = new Map(), edges = new Map();
  for (const scene of set.scenes) {
    for (const view of scene.views) {
      views.set(view.viewID, view.viewName.toLowerCase());
      edges.set(view.viewID, []);
    }
    for (const [direction, turn] of scene.turns.entries()) {
      if (direction > 1) break;
      const ring = turn.frames.filter(f => f.motionInfo > 0 && f.viewID >= 0)
        .map(f => scene.views[f.viewID].viewID);
      ring.forEach((id, i) => edges.get(id).push({
        id: ring[(i + 1) % ring.length], action: `key ${direction ? 'leftarrow' : 'rightarrow'}`
      }));
    }
  }
  for (const road of set.transitions) {
    if (road.frameRegisters[0]?.frames.length)
      edges.get(road.viewIDstart).push({ id: road.viewIDend, action: 'key uparrow' });
    if (road.frameRegisters[1]?.frames.length)
      edges.get(road.viewIDend).push({ id: road.viewIDstart, action: 'key uparrow' });
  }
  return { views, edges };
});
const roomAt = level => level === 1 ? 0 : level === top ? 2 : 1;
function idAt(level, name) {
  const matches = [...rooms[roomAt(level)].views].filter(([, n]) => n === name);
  if (matches.length !== 1) throw new Error(`Ambiguous floor ${level} view ${name}`);
  return matches[0][0];
}
// Stair endpoints are read from the per-scene authored key handlers. The
// repeated middle room has separate up/down views for its four staircases.
const up = scripts.map(text => new Map([...text.matchAll(
  /code keydown \(arg\)([\s\S]*?)endcode/g
)].flatMap(m => {
  const view = m[1].match(/currentview \(\) = "(view\d+)" & arg = "uparrow"/);
  const target = m[1].match(/changeset \("smstack[23]", "[^"]+", "(view\d+)"\)/);
  return view && target ? [[view[1], target[1]]] : [];
})));
if (up[0].size !== 4 || up[1].size !== 4) throw new Error('Incomplete authored up stairs');
const down = new Map([
  ['view50', ['view42', 'view78']], ['view54', ['view58', 'view72']],
  ['view56', ['view59', 'view68']], ['view43', ['view57', 'view79']]
]);
const start = { level: 1, id: idAt(1, 'view40'), path: [] };
const queue = [start], seen = new Set([`${start.level}:${start.id}`]);
for (let cursor = 0; cursor < queue.length; ++cursor) {
  const node = queue[cursor], r = roomAt(node.level), room = rooms[r], name = room.views.get(node.id);
  if (node.level === top && name === 'view53') {
    for (const step of node.path) {
      console.log(step.action);
      console.log(`expectset smstack${roomAt(step.level) + 1}`);
      console.log(`expectglobal stacklevel ${step.level}`);
      console.log(`expectview ${rooms[roomAt(step.level)].views.get(step.id)}`);
    }
    process.exit(0);
  }
  const next = [];
  for (const edge of room.edges.get(node.id)) {
    if (edge.action === 'key uparrow') {
      if (r === 1 && blocks.get(node.level)?.has(barriers.get(name))) continue;
      if (up[r].has(name)) continue;
    }
    next.push({ ...edge, level: node.level });
  }
  if (node.level < top && up[r].has(name))
    next.push({ level: node.level + 1, id: idAt(node.level + 1, up[r].get(name)), action: 'key uparrow' });
  if (node.level > 1 && down.has(name)) {
    const level = r === 2 ? 1 : node.level - 1;
    const target = r === 2 ? 'view42' : down.get(name)[level === 1 ? 0 : 1];
    next.push({ level, id: idAt(level, target), action: 'key downarrow' });
  }
  for (const edge of next) {
    const key = `${edge.level}:${edge.id}`;
    if (seen.has(key)) continue;
    seen.add(key); queue.push({ ...edge, path: [...node.path, edge] });
  }
}
throw new Error(`No route through maze ${maze}`);
