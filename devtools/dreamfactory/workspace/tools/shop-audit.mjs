// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-shop');fs.mkdirSync(dest,{recursive:true});
for(const dir of ['df','runtime']) {
 fs.mkdirSync(`${dest}/${dir}`,{recursive:true});
 for(const file of fs.readdirSync(`references/dreamrefactory/engine/src/${dir}`).filter(n=>n.endsWith('.ts'))) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/${dir}/${file}`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(`${dest}/${dir}/${file.replace(/\.ts$/,'.mjs')}`,js.replace(/from (["'])(\.{1,2}\/[^"']+)\1/g,'from $1$2.mjs$1'));
 }
}
const {readShpFile,decodeShpFrame}=await import(pathToFileURL(`${dest}/df/shp.mjs`));
const {LoadedShop,PropInstance,PropRuntime,frameIndexForDegree,degVariantFrames,playSequence,isDegreeSelector}=await import(pathToFileURL(`${dest}/runtime/props.mjs`));
const {projectPoint,bearing}=await import(pathToFileURL(`${dest}/runtime/geometry.mjs`));
class Hash {
 hash=2166136261;
 byte(b){this.hash=Math.imul(this.hash^(b&255),16777619)>>>0;}
 number(n){for(let i=0;i<4;++i)this.byte(n>>>(8*i));}
 text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
 hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length?process.argv.slice(2):['demo','full','dust']) {
 const projection=new Hash(),bearings=new Hash(),coords=[-2147483648,-32768,0,32767,2147483647];
 for(let degree=0;degree<256;++degree)for(const x of coords)for(const y of coords)for(const z of [-32768,0,32767]) {
  const p=projectPoint({x:13,y:-27,z:45,deg:degree,f:48,cx:48,cy:36,clipW:96,clipH:72},x,y,z);
  projection.number(p?1:0);if(p)for(const n of [p.x,p.y,p.depth])projection.number(n);
 }
 for(const x of coords)for(const y of coords)bearings.number(bearing(x,y));
 console.log(`GEOMETRY projection=${projection.hex()} bearing=${bearings.hex()}`);
 let count=0;
 for(const name of corpusFiles(edition).filter(n=>/\.(shp|prp)$/i.test(n))) {
  const bytes=new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`)),shop=readShpFile(bytes),meta=new Hash(),pixels=new Hash(),selection=new Hash(),art=new Hash(),worldArt=new Hash();let states=0,frames=0,pictures=0,worldPictures=0;
  const loaded=new LoadedShop(name,shop),runtime=new PropRuntime(),palette=new Uint8ClampedArray(1024);for(let i=0;i<256;++i){for(let c=0;c<3;++c)palette[i*4+c]=shop.paletteRaw[i*8+3+c*2];palette[i*4+3]=255;}
  meta.number(new DataView(shop.file.containers[0].data.buffer,shop.file.containers[0].data.byteOffset).getUint32(2,true));
  meta.number(shop.mainScriptLocation);meta.text(shop.refName);for(let i=0;i<256;++i)for(let c=0;c<3;++c)meta.byte(shop.paletteRaw[i*8+3+c*2]);meta.number(shop.groups.length);
  for(const group of shop.groups){meta.text(group.name);meta.number(group.location);meta.number(group.scriptContainerLocation);meta.number(group.states.length);
   for(const state of group.states){++states;meta.text(state.identifier);meta.number(state.location);meta.number(state.record);meta.number(+state.animated);meta.number(state.playOrder?.length??0);for(const p of state.playOrder??[])meta.number(p);meta.number(state.frames.length);
    if(state.frames.length)for(const degree of [-1,0,1,2,7,64,128,255,256]) {
      const p=new PropInstance(group,loaded);p.stateName=state.identifier.toLowerCase();p.deg=degree;
      selection.number(p.currentFrameIdx(state));
      p.frameOrder=isDegreeSelector(state)?null:playSequence(state,degVariantFrames(state,degree));
      selection.number(p.frameCount(state));
      for(let i=0;i<p.frameCount(state);++i){p.frameIdx=i;selection.number(p.currentFrameIdx(state));}
    }
    if(state.frames.length && (state===group.states[0] || state===group.states.at(-1)))for(const degree of [0,1]) {
      const p=new PropInstance(group,loaded);p.stateName=state.identifier.toLowerCase();p.deg=degree;p.visible=true;
      p.frameOrder=isDegreeSelector(state)?null:playSequence(state,degVariantFrames(state,degree));p.frameIdx=p.frameCount(state)-1;
      const frame=loaded.frame(p.currentFrame(state));p.anchorX=frame.posXraw-3;p.anchorY=frame.posYraw+2;
      runtime.props.clear();runtime.props.set('probe',p);
      const rgba=new Uint8ClampedArray(96*72*4);for(let i=0;i<rgba.length;i+=4)rgba.set([19,31,47,255],i);
      runtime.composite(rgba,96,72,palette);for(const b of rgba)art.byte(b);++pictures;
    }
    if(state.frames.length && (state===group.states[0] || state===group.states.at(-1)))for(const heading of [0,37,128,255])for(const directional of [false,true]) {
      const p=new PropInstance(group,loaded);p.stateName=state.identifier.toLowerCase();p.deg=73;p.visible=true;p.worldSpace=true;p.directional=directional;p.scale=1500;
      const angle=heading*Math.PI/128;
      p.worldX=13+Math.round(160*Math.cos(angle))+Math.round(20*Math.sin(angle));p.worldY=-27+Math.round(160*Math.sin(angle))-Math.round(20*Math.cos(angle));p.worldZ=35;p.zclip=directional?75:0;
      const cam={x:13,y:-27,z:45,deg:heading,f:48,cx:48,cy:36,clipW:96,clipH:72};
      const z=Uint8Array.from({length:96*72},(_,i)=>(Math.floor(i/96)*7+(i%96)*3)%16),occ={z,w:96,h:72,scale:32,levels:16,groundBias:edition==='dust'?64:0};
      runtime.props.clear();runtime.props.set('probe',p);
      const rgba=new Uint8ClampedArray(96*72*4);for(let i=0;i<rgba.length;i+=4)rgba.set([19,31,47,255],i);
      runtime.composite(rgba,96,72,palette,cam,false,occ);for(const b of rgba)worldArt.byte(b);++worldPictures;
    }
    for(let f=0;f<state.frames.length;++f){++frames;for(const n of [state.frames[f],state.records[f],state.groups[f],state.degrees[f],state.refScales[f]])meta.number(n);
     const sprite=decodeShpFrame(shop.file.containers[state.frames[f]].data);for(const n of [sprite.width,sprite.height,sprite.posXraw,sprite.posYraw])pixels.number(n);for(const b of sprite.indexed)pixels.byte(b);for(const b of sprite.opaque)pixels.byte(b);
    }
   }
  }
  console.log(`PROPWORLD ${name} pictures=${worldPictures} rgba=${worldArt.hex()}`);
  console.log(`PROPART ${name} pictures=${pictures} selection=${selection.hex()} rgba=${art.hex()}`);
  console.log(`SHOP ${name} props=${shop.groups.length} states=${states} frames=${frames} meta=${meta.hex()} pixels=${pixels.hex()}`);++count;
 }
 console.log(`SHOP PASS files=${count} failures=0`);
}
