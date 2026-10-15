// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-movie');fs.mkdirSync(dest,{recursive:true});
for(const name of ['binary','byte-order','version','container','audio','banks','snd','mov','mov-v1','image']) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const load=name=>import(pathToFileURL(path.join(dest,name+'.mjs')));
const {readContainerFile}=await load('container');const {readMovFile}=await load('mov');
const {readMovFileV1,movFileFromV1,compositeFrameV1}=await load('mov-v1');
const {FrameBuffer,decodeFrame}=await load('image');const {decodeAudioContainer}=await load('audio');
class Hash {
  hash=2166136261;
  byte(n){this.hash=Math.imul(this.hash^(n&255),16777619)>>>0;}
  number(n){for(let i=0;i<4;++i)this.byte(n>>>(8*i));}
  text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
  hex(){return this.hash.toString(16).padStart(8,'0');}
}
const action=(hash,a)=>{hash.number(a.type);hash.text(a.sound);hash.text(a.event);hash.text(a.target);};
for(const edition of process.argv.slice(2).length?process.argv.slice(2):['demo','full','dust']) {
  let count=0,skipped=0;
  for(const name of corpusFiles(edition).filter(n=>/\.mov$/i.test(n))) {
    const bytes=new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`));
    const envelope=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
    if(bytes.length>=32 && envelope.getUint32(4,false)===bytes.length && envelope.getUint32(4,true)!==bytes.length){console.log(`MOVIE SKIP ${name} big-endian`);++skipped;continue;}
    const head=readContainerFile(bytes).containers[0].data;
    const version=new DataView(head.buffer,head.byteOffset,head.byteLength).getUint32(2,true);
    if(version===0){console.log(`MOVIE SKIP ${name} version=0`);++skipped;continue;}
    const movie=version===1?movFileFromV1(readMovFileV1(bytes)):readMovFile(bytes);
    // The reference infers v1 beds from unused chunks. DF.EXE 418e0c..418f21
    // instead loads header+28 records after header+26 singles and follows the
    // header+2110 order. Keep the independent frame/image decoder, but use the
    // original loader's soundtrack directory for this part of the oracle.
    if(version===1)for(const seg of movie.segments){
      const h=movie.file.containers[seg.bias].data,v=new DataView(h.buffer,h.byteOffset,h.byteLength);
      const singles=v.getUint16(26,true),records=v.getUint16(28,true),length=v.getUint16(52,true);
      if(length>64)throw new Error(`Invalid v1 bed length: ${name}`);
      seg.audioChunks=Array.from({length},(_,i)=>{
        const index=v.getUint16(2110+i*2,true);
        if(!index||index>records)throw new Error(`Invalid v1 bed index: ${name}`);
        return seg.bias+singles+index;
      });
      seg.audioLoops=seg.audioChunks.length>0;seg.bedRecords=records;seg.bedRepeat=v.getUint32(2238,true);
    }
    if(version===4)for(const seg of movie.segments){
      const h=movie.file.containers[seg.bias].data,v=new DataView(h.buffer,h.byteOffset,h.byteLength),loc=v.getUint32(100,true);
      seg.bedRecords=0;seg.bedRepeat=0;
      if(loc){const t=movie.file.containers[seg.bias+loc].data,b=new DataView(t.buffer,t.byteOffset,t.byteLength);seg.bedRecords=b.getUint32(266,true);
        if(seg.audioChunks.length)seg.bedRepeat=Math.max(0,Math.min(b.getInt32(0,true),seg.bedRecords-1,seg.audioChunks.length-1));}
    }
    const meta=new Hash(),pixels=new Hash(),audio=new Hash(),used=new Set();let frames=0,regions=0;
    meta.number(version);
    for(const seg of movie.segments) {
      for(const n of [seg.bias,seg.width,seg.height,seg.originX,seg.originY,seg.flags,seg.minHoldTicks,+seg.keySkips,+seg.audioLoops])meta.number(n);
      meta.number(seg.bedRecords);meta.number(seg.bedRepeat);
      meta.text(seg.actionFrame1);meta.text(seg.actionFrame2);
      const palette=new Uint8Array(768);for(let i=0;i<256;++i)for(let c=0;c<3;++c)palette[i*3+c]=seg.paletteRaw[i*8+3+c*2];
      palette.fill(0,0,3);palette.fill(255,765);for(const b of palette)meta.byte(b);
      meta.number(seg.frames.length);const buffer=new FrameBuffer();let shown=null;
      for(const frame of seg.frames) {
        ++frames;action(meta,frame);
        for(const n of [frame.locationFrame,frame.locationClickRegion,frame.record,frame.width,frame.height])meta.number(n);
        meta.text(frame.name);for(const n of [frame.holdTicks,+frame.waitsForVoice,+frame.holdsDeadline,+frame.playsThroughRegions,frame.regions.length])meta.number(n);
        for(const region of frame.regions) {++regions;action(meta,region);for(const n of [region.y0,region.x0,region.y1,region.x1,region.record])meta.number(n);}
        const image=decodeFrame(movie.file.containers[frame.locationFrame].data,buffer);
        const indexed=buffer.pixels.slice(0,image.width*image.height);if(version===1)compositeFrameV1(indexed,shown);shown=indexed;
        pixels.number(image.width);pixels.number(image.height);for(const b of indexed)pixels.byte(b);
      }
      meta.number(seg.audioChunks.length);for(const c of seg.audioChunks){meta.number(c);used.add(c);}
      meta.number(seg.sounds.size);for(const [name,c] of seg.sounds){meta.text(name);meta.number(c);meta.text(seg.soundFollows.get(name)??'');used.add(c);}
      meta.number(seg.cues.length);for(const cue of seg.cues){meta.number(cue.tick);meta.text(cue.target);}
    }
    for(const c of [...used].sort((a,b)=>a-b)) {
      const clip=decodeAudioContainer(movie.file.containers[c].data);
      audio.number(c);audio.number(clip.sampleRate);audio.number(clip.samples.length);
      for(const v of clip.samples){const n=Math.round(v*32768);audio.byte(n);audio.byte(n>>8);}
    }
    console.log(`MOVIE ${name} segments=${movie.segments.length} frames=${frames} regions=${regions} chunks=${used.size} meta=${meta.hex()} pixels=${pixels.hex()} audio=${audio.hex()}`);++count;
  }
  console.log(`MOVIE PASS files=${count} skipped=${skipped} failures=0`);
}
