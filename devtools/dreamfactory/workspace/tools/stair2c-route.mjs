// SPDX-License-Identifier: GPL-3.0-or-later
// Physical second-class staircase routing, including its repeated deck state.
import fs from 'node:fs';
import { readSetFile } from '../build/reference-room/set.mjs';
const [fromDeckArg, fromView, toDeckArg, toView] = process.argv.slice(2);
const deckValue = value => value === 'landing' ? '' : value;
const fromDeck = deckValue(fromDeckArg), toDeck = deckValue(toDeckArg);
if (!['view28', 'view64'].includes(toView?.toLowerCase())) throw new Error('Target must be the bottom or boat-deck exit');
const descending = toView.toLowerCase() === 'view28';
for (const d of [fromDeck, toDeck])
  if (!['bd', 'a', 'b', 'c', 'd', 'e', 'f', ''].includes(d)) throw new Error('Invalid staircase deck');
const set = readSetFile(new Uint8Array(fs.readFileSync('game-data/full/volumes/titanic1/DATA/STAIR2C.SET')));
const names = new Map(), ids = new Map(), edges = new Map();
for (const scene of set.scenes) {
  for (const view of scene.views) {
    names.set(view.viewID, view.viewName); ids.set(view.viewName.toLowerCase(), view.viewID); edges.set(view.viewID, []);
  }
  for (const [direction, turn] of scene.turns.entries()) {
    if (direction > 1) break;
    const ring = turn.frames.filter(f => f.motionInfo > 0 && f.viewID >= 0).map(f => scene.views[f.viewID].viewID);
    ring.forEach((id, i) => edges.get(id).push({id: ring[(i + 1) % ring.length], action: `key ${direction ? 'leftarrow' : 'rightarrow'}`}));
  }
}
for (const road of set.transitions) {
  if (road.frameRegisters[0]?.frames.length) edges.get(road.viewIDstart).push({id: road.viewIDend, action: 'key uparrow'});
  if (road.frameRegisters[1]?.frames.length) edges.get(road.viewIDend).push({id: road.viewIDstart, action: 'key uparrow'});
}
const find = name => { const id = ids.get(name.toLowerCase()); if (id === undefined) throw new Error(`Unknown view ${name}`); return id; };
// STAIR2C's main key handler resets the departure view before passing the
// arrow to the room walker. The two landing scenes also turn twice per key.
const upDeck = {b:'a', c:'b', d:'c', e:'d', f:'e', a:'', '':''};
const downDeck = {a:'b', b:'c', c:'d', d:'e', e:'', f:'', '':''};
const initial = {deck: fromDeck, id: find(fromView), path: []}, queue = [initial], seen = new Set();
for (let cursor = 0; cursor < queue.length; ++cursor) {
  const node = queue[cursor], key = `${node.deck}:${node.id}`;
  if (seen.has(key)) continue;
  seen.add(key);
  if (node.deck === toDeck && node.id === find(toView)) {
    for (const step of node.path) {
      console.log(step.action);
      console.log(`expectglobal savedeck ${step.deck}`);
      console.log(`expectview ${names.get(step.id)}`);
    }
    process.exit(0);
  }
  const name = names.get(node.id).toLowerCase(), next = [];
  for (const edge of edges.get(node.id)) {
    let deck = node.deck, id = edge.id;
    if (edge.action === 'key uparrow') {
      // Do not reverse at the boundary of the repeated landing. Its script
      // clears savedeck there; using that transient state to jump straight
      // to the bottom would bypass the intermediate decks and their gates.
      if (descending && (name === 'view51' || name === 'view15')) continue;
      if (!descending && (name === 'view18' || name === 'view63')) continue;
      let source = node.id;
      if (name === 'view63') deck = 'a';
      if (name === 'view15') deck = 'e';
      if (name === 'view18' || name === 'view51') {
        deck = (name === 'view18' ? downDeck : upDeck)[deck];
        if (deck === undefined) continue;
        if (deck !== '') source = find(name === 'view18' ? 'view54' : 'view15');
        const road = edges.get(source).find(e => e.action === 'key uparrow');
        if (!road) throw new Error('Missing authored reset road');
        id = road.id;
      }
    } else if (/^view(4[6-9]|5[0-3])$/.test(name)) {
      const second = edges.get(id).find(e => e.action === edge.action);
      if (!second) throw new Error('Missing authored double turn');
      id = second.id;
    }
    next.push({deck, id, action: edge.action});
  }
  for (const step of next) queue.push({...step, path: [...node.path, step]});
}
throw new Error('No staircase route');
