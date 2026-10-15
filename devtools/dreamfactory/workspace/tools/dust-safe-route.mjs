// SPDX-License-Identifier: GPL-3.0-or-later
// Plan physical pointer arcs from CRACK.PRP's artwork and authored limiter.
// No native save, VM variables or game resources are modified.
import fs from 'node:fs';
import {readShpFile, decodeShpFrame} from '../build/reference-shop/df/shp.mjs';
const shop = readShpFile(new Uint8Array(fs.readFileSync('game-data/dust/CRACK.PRP')));
const state = shop.groups.find(g => g.name.toLowerCase() === 'spin').states[0];
function grab(degree) {
  const index = state.degrees.indexOf(degree);
  if (index < 0) throw new Error('Missing dial degree');
  const f = decodeShpFrame(shop.file.containers[state.frames[index]].data);
  for (let y=0;y<384;y+=4) for (let x=0;x<512;x+=4) {
    const sx=x-256+f.posXraw, sy=y-128+f.posYraw;
    if (sx>=0 && sy>=0 && sx<f.width && sy<f.height && f.opaque[sy*f.width+sx]) return [x,y];
  }
  throw new Error('No physical dial grab point');
}
function bearing(x,y) {
  if (!y) return x<0?128:0;
  if (!x) return y<0?192:64;
  const ax=Math.abs(x), ay=Math.abs(y);
  let a=ax===ay?32:Math.floor(Math.atan(Math.floor(Math.min(ax,ay)*512/Math.max(ax,ay))/512)*128/Math.PI);
  if (ax!==ay && ay>ax) a=64-a;
  if (x<0) a=128-a;
  if (y<0) a=-a;
  return a&255;
}
const position=(x,y)=>Math.trunc(((bearing(x-256,y-128)+64)&255)*50/256);
function simulate(current,right,delta,steps) {
  const start=grab(current), initial=Math.atan2(start[1]-128,start[0]-256);
  let previous=position(...start);
  for (let i=1;i<=steps;i++) {
    const a=initial+i*delta*Math.PI/128;
    const next=position(Math.floor(256+24*Math.cos(a)+.5),Math.floor(128+24*Math.sin(a)+.5));
    let change=previous-next;
    if ((right && change < -24) || (!right && change > 24)) change=next-previous;
    const result=current+change;
    if (!((right && current<=result) || (!right && current>=result))) current=(result+100)%50;
    previous=next;
  }
  return current;
}
for (const [current,target,right] of [[0,8,true],[8,23,false],[23,41,true]]) {
  let chosen;
  for (let steps=4;steps<=64 && !chosen;steps++) for (let d=1;d<=16;d++) {
    const delta=right?d:-d;
    if (simulate(current,right,delta,steps)===target) { chosen={delta,steps}; break; }
  }
  if (!chosen) throw new Error('No physical arc reaches the next dial number');
  console.log(`dragarc prop spin 256 128 ${chosen.delta} ${chosen.steps}`);
  console.log(`expectpropdeg spin ${target}`);
}
console.log('expectglobal combo 08,23,41,');
