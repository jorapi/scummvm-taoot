// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-stage');
fs.mkdirSync(dest,{recursive:true});
for (const name of ['byte-order','binary','version','container','stg','image']) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {readStgFile,readStgRegions}=await import(pathToFileURL(path.join(dest,'stg.mjs')));
const {decodeFrame,FrameBuffer}=await import(pathToFileURL(path.join(dest,'image.mjs')));
class Hash {
  hash=2166136261;
  byte(b){this.hash=Math.imul(this.hash^b,16777619)>>>0;}
  number(n){for(let i=0;i<4;++i)this.byte((n>>>(8*i))&255);}
  text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
  hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length ? process.argv.slice(2) : ['demo','full','dust']) {
  let count=0;
  for(const name of corpusFiles(edition).filter(n=>/\.(stg|flt)$/i.test(n))) {
    const stage=readStgFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`)));
    const meta=new Hash(),pixels=new Hash(),art=new Hash();let regions=0,frames=0;
    meta.number(stage.version);meta.number(stage.screen.width);meta.number(stage.screen.height);meta.number(stage.mainScriptLocation);meta.text(stage.refName);
    const palette=new Uint8Array(768);
    for(let i=0;i<256;++i)for(let c=0;c<3;++c)palette[i*3+c]=stage.paletteRaw[i*8+3+c*2];
    palette.fill(0,0,3);palette.fill(255,765);
    for(const b of palette)meta.byte(b);
    for(const flat of stage.flats) {
      for(const n of [flat.condition,flat.locationScript,flat.locationFrame,flat.locationClickLogic,flat.width,flat.height])meta.number(n);
      meta.text(flat.name);
      const buttons=flat.locationClickLogic ? readStgRegions(stage.file.containers[flat.locationClickLogic].data,stage.version) : [];
      for(const r of buttons){++regions;for(const n of [r.top,r.left,r.bottom,r.right,r.script])meta.number(n);meta.text(r.name);}
      if(flat.locationFrame) {
        const buffer=new FrameBuffer();
        const frame=decodeFrame(stage.file.containers[flat.locationFrame].data,buffer);
        pixels.number(frame.width);pixels.number(frame.height);
        for(const b of buffer.pixels.subarray(0,frame.width*frame.height))pixels.byte(b);
        // ScreenDirector paints the flat first, then the SET at the origin.
        // Use decoded authored art and independently colored synthetic room data.
        for(let variant=0;variant<3;++variant) {
          const rgba=new Uint8Array(512*384*4);for(let i=3;i<rgba.length;i+=4)rgba[i]=255;
          for(let y=0;y<Math.min(384,frame.height);++y)for(let x=0;x<Math.min(512,frame.width);++x) {
            const index=buffer.pixels[y*frame.width+x];
            for(let c=0;c<3;++c)rgba[(y*512+x)*4+c]=Math.floor(palette[index*3+c]*(variant===2?127:255)/255);
          }
          if(variant)for(let y=0;y<264;++y)for(let x=0;x<512;++x) {
            const index=(x+y*3)&255;
            for(let c=0;c<3;++c)rgba[(y*512+x)*4+c]=Math.floor((((index*3+c)*7+13)&255)*(variant===2?63:255)/255);
          }
          for(const b of rgba)art.byte(b);
        }
        ++frames;
      }
    }
    console.log(`STAGEART ${name} pictures=${frames*3} rgba=${art.hex()}`);
    console.log(`STAGE ${name} flats=${stage.flats.length} regions=${regions} frames=${frames} meta=${meta.hex()} pixels=${pixels.hex()}`);
    ++count;
  }
  console.log(`STAGE PASS files=${count} failures=0`);
}
