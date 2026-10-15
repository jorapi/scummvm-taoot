// SPDX-License-Identifier: GPL-3.0-or-later
// Audit SET script/scene/hotspot metadata against the unmodified readers.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-room');fs.mkdirSync(dest,{recursive:true});
for(const name of ['byte-order','binary','version','container','set','set-v1','image']) {
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {readSetFile}=await import(pathToFileURL(path.join(dest,'set.mjs')));
const {readSetFileV1}=await import(pathToFileURL(path.join(dest,'set-v1.mjs')));
const {decodeFrame,FrameBuffer}=await import(pathToFileURL(path.join(dest,'image.mjs')));
class Hash {
 hash=2166136261;
 number(n){for(let i=0;i<4;++i)this.hash=Math.imul(this.hash^((n>>>(8*i))&255),16777619)>>>0;}
 text(s){for(const c of s)this.hash=Math.imul(this.hash^c.charCodeAt(0),16777619)>>>0;this.hash=Math.imul(this.hash,16777619)>>>0;}
 hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length?process.argv.slice(2):['demo','full','dust']) {
 let files=0;
 for(const name of corpusFiles(edition).filter(n=>/\.set$/i.test(n))) {
  const v1=edition==='dust', bytes=new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`)), set=v1?readSetFileV1(bytes):readSetFile(bytes), h=new Hash();let paints=0;
  h.text(set.setName);h.number(set.mainScript);
  for(const s of set.scenes){h.text(v1?s.name:s.sceneName);h.number(v1?s.scriptLocation:s.locationScript);for(const n of v1?[s.x*256+128,s.z*256+128,0,s.build?1:0]:[s.xAxisMap,s.zAxisMap,s.yAxisMap,0])h.number(n);
   if(!v1) for(const v of s.views){h.text(v.viewName);h.number(v.viewID);h.number(v.rotation8);for(const p of v.objects){++paints;h.text(p.identifier);for(const n of [p.locationScript,p.rotation8,p.startRegionX,p.startRegionY,p.endRegionX,p.endRegionY])h.number(n);}}
  }
  const camera=new Hash();let cameraFrames=0;
  for(const n of [set.viewPortWidth,set.viewPortHeight,v1?24:set.zLevelCount,v1?1536:set.zFarMax,v1?set.cameraSetback:0,v1?set.eyeHeight:0])camera.number(n);
  if(!v1) {
   for(const scene of set.scenes) for(const view of scene.views)camera.number(Math.round(view.cameraHeight*512));
   const sequences=[...set.scenes.flatMap(s=>s.turns),...set.transitions.flatMap(t=>t.frameRegisters)];
   for(const seq of sequences)for(const f of seq.frames){for(const n of [f.posX16,f.posZ16,f.posY16,f.axisX8])camera.number(n);++cameraFrames;}
  }
  const sequences=[];
  if(v1) {
   sequences.push(...set.transitions.map(t=>t.frames));
   const views=new Map();
   for(const t of set.transitions) {
    const key=`${t.from.x},${t.from.z}`;
    if(!views.has(key))views.set(key,new Map());
    const directions=views.get(key);
    if(!directions.has(t.from.facing))directions.set(t.from.facing,-1);
    if(t.departureStill>=0)directions.set(t.from.facing,t.departureStill);
   }
   for(const scene of set.scenes)for(const still of views.get(`${scene.x},${scene.z}`)?.values()??[])if(still>=0)sequences.push([still]);
  }else{
   for(const scene of set.scenes)for(const turn of scene.turns)sequences.push(turn.frames.map(f=>f.frameContainerLoc));
   for(const road of set.transitions)for(const walk of road.frameRegisters)sequences.push(walk.frames.map(f=>f.frameContainerLoc));
  }
  let depthHash=2166136261,depthFrames=0,imageFrames=0;
  for(const sequence of sequences){
   const buffer=new FrameBuffer();
   for(const id of sequence){
    if(!id)continue;
    const frame=decodeFrame(set.file.containers[id].data,buffer);++imageFrames;
    if(frame.hasZ){++depthFrames;for(let p=0;p<frame.width*frame.height;++p)depthHash=Math.imul(depthHash^buffer.zPixels[p],16777619)>>>0;}
   }
  }
  console.log(`ROOMDEPTH ${name} images=${imageFrames} planes=${depthFrames} fnv1a=${depthHash.toString(16).padStart(8,'0')}`);
  console.log(`ROOMCAM ${name} frames=${cameraFrames} meta=${camera.hex()}`);
  console.log(`ROOM ${name} scenes=${set.scenes.length} paintings=${paints} meta=${h.hex()}`);++files;
 }
 console.log(`ROOM PASS files=${files}`);
}
