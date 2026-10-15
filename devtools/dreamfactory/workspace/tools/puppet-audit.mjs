// SPDX-License-Identifier: GPL-3.0-or-later
// Independent PUP metadata, lip-sync, sprite and PCM corpus oracle.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-puppet');fs.mkdirSync(dest,{recursive:true});
for(const file of fs.readdirSync('references/dreamrefactory/engine/src/df').filter(n=>n.endsWith('.ts'))){
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/df/${file}`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(`${dest}/${file.replace(/\.ts$/,'.mjs')}`,js.replace(/from (["'])(\.{1,2}\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
// Execute the reference's actual art compositor with deterministic, neutral
// screen/gamma adapters; canvas text is audited separately from the pixels.
fs.writeFileSync(`${dest}/screen.mjs`,'export const SCREEN_W=512,SCREEN_H=384;');
fs.writeFileSync(`${dest}/screen-gamma.mjs`,'export const displayPalette=p=>p, displayChannel=c=>c, screenGammaGeneration=()=>0;');
for(const file of ['fonts','puppet-view']){
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/web/${file}.ts`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(`${dest}/${file}.mjs`,js.replace(/@dreamfactory\/engine\/df\//g,'./').replace(/from (["'])(\.{1,2}\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {PuppetView}=await import(pathToFileURL(`${dest}/puppet-view.mjs`));
const {readPupFile,readAnimLogic}=await import(pathToFileURL(`${dest}/pup.mjs`));
const {decodeShpFrame}=await import(pathToFileURL(`${dest}/shp.mjs`));
const {decodeAudioContainer}=await import(pathToFileURL(`${dest}/audio.mjs`));
class Hash{
 hash=2166136261;
 byte(b){this.hash=Math.imul(this.hash^(b&255),16777619)>>>0;}
 number(n){for(let i=0;i<4;++i)this.byte(n>>>(8*i));}
 text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
 hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length?process.argv.slice(2):['demo','full','dust']){
 let files=0;
 for(const name of corpusFiles(edition).filter(n=>/\.pup$/i.test(n))){
  const bytes=new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`)), dv=new DataView(bytes.buffer);
  if(dv.getUint32(4,false)===bytes.length && dv.getUint32(4,true)!==bytes.length){console.log(`PUPPET SKIP ${name} big-endian`);continue;}
  const pup=readPupFile(bytes), meta=new Hash(), pixels=new Hash(), audio=new Hash();let frames=0,poses=0,samples=0;
  meta.number(new DataView(pup.file.containers[0].data.buffer,pup.file.containers[0].data.byteOffset).getUint32(2,true));meta.text(pup.pupName);meta.number(pup.bandLocation);
  for(let i=0;i<256;++i)for(let c=0;c<3;++c)meta.byte(pup.paletteRaw[i*8+3+c*2]);
  for(const timer of pup.idleTimers){meta.number(timer.minTicks);meta.number(timer.maxTicks);}
  for(const s of pup.scripts){meta.text(s.name);meta.number(s.location);}
  const sprite=ref=>{
   const data=pup.file.containers[ref]?.data;
   if(!ref || !data?.length){pixels.number(0);return;}
   const f=decodeShpFrame(data);++frames;
   for(const n of [f.width,f.height,f.posYraw,f.posXraw])pixels.number(n);
   for(const b of f.indexed)pixels.byte(b);for(const b of f.opaque)pixels.byte(b);
  };
  sprite(pup.bandLocation);
  for(const stance of pup.stances){meta.number(stance.location);for(const l of stance.layers){meta.number(l.anchorY);meta.number(l.anchorX);meta.number(l.frames.length);for(const ref of l.frames){meta.number(ref);sprite(ref);}}}
  for(const l of pup.dialogue.values()){
   meta.text(l.ident);meta.text(l.raw);for(const n of [l.stance,l.audioLocation,l.animLogicLocation,l.record])meta.number(n);
   const anim=readAnimLogic(pup,l.animLogicLocation);meta.number(anim.length);poses+=anim.length;
   for(const p of anim)for(const layer of p.layers)for(const n of [layer.frame,layer.y,layer.x])meta.number(n);
   const data=pup.file.containers[l.audioLocation]?.data;
   if(!l.audioLocation || !data?.length){audio.number(0);continue;}
   const clip=decodeAudioContainer(data);audio.number(clip.sampleRate);audio.number(clip.samples.length);samples+=clip.samples.length;
   for(const sample of clip.samples){const value=Math.round(sample*32768);audio.byte(value);audio.byte(value>>>8);}
  }
  const rendered=new Hash();let pictures=0;
  for(let stance=0;stance<pup.stances.length;++stance){
   const line=[...pup.dialogue.values()].find(l=>l.stance===stance);if(!line)continue;
   const poses=readAnimLogic(pup,line.animLogicLocation);if(!poses.length)continue;
   for(const pose of [poses[0],poses.at(-1)])for(const captions of [false,true]){
    const pixels=new Uint8Array(512*264),palette=new Uint8ClampedArray(256*4);
    for(let y=0;y<264;++y)for(let x=0;x<512;++x)pixels[y*512+x]=((x>>5)+(y>>4)+pictures*17)&255;
    for(let i=0;i<256;++i){for(let c=0;c<3;++c)palette[i*4+c]=(i*3+c*61+pictures*13)&255;palette[i*4+3]=255;}
    const state={name,pup,stanceIdx:stance,subtitle:captions?'caption':'',bevels:[]};
    const view=new PuppetView({puppet:state,puppetCtrl:{puppetFrame:()=>pose},subtitlesOn:()=>true});
    const rgba=new Uint8ClampedArray(512*384*4);for(let i=0;i<rgba.length;i+=4)rgba.set([17,31,47,255],i);
    view.composite(rgba,{pixels,width:512,height:264,palette});
    for(const b of rgba)rendered.byte(b);
    if(pictures===0 && /(?:^|\/)(DPENNY|PENNY1|MWIFE)\.PUP$/i.test(name))fs.writeFileSync(`build/puppet-art-${edition}.rgba`,rgba);
    ++pictures;
   }
  }
  console.log(`PUPPETART ${name} pictures=${pictures} rgba=${rendered.hex()}`);
  console.log(`PUPPET ${name} lines=${pup.dialogue.size} scripts=${pup.scripts.length} stances=${pup.stances.length} frames=${frames} poses=${poses} samples=${samples} meta=${meta.hex()} pixels=${pixels.hex()} audio=${audio.hex()}`);++files;
 }
 console.log(`PUPPET PASS files=${files} failures=0`);
}
