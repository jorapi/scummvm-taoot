// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only contact sheet for manually researched document/interface replacements.
import fs from 'node:fs';
import crypto from 'node:crypto';
import {readStgFile} from '../build/reference-stage/stg.mjs';
import {FrameBuffer,decodeFrame} from '../build/reference-stage/image.mjs';
const output='build/enhancements/artwork-catalog';fs.mkdirSync(output,{recursive:true});
const entries=[];
const escape=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('"','&quot;');
for(const edition of ['full','demo','dust']){
 const index=JSON.parse(fs.readFileSync(`game-data/${edition}/resource-index.json`,'utf8'));
 fs.mkdirSync(`${output}/${edition}`,{recursive:true});
 for(const [name,variants] of Object.entries(index)){
  if(!/\.(stg|flt)$/i.test(name))continue;
  const variant=variants[0];let stage;
  try{stage=readStgFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${variant.path}`)));}catch{continue;}
  for(const flat of stage.flats){
   if(!flat.locationFrame)continue;
   const fb=new FrameBuffer();try{decodeFrame(stage.file.containers[flat.locationFrame].data,fb);}catch{continue;}
   if(!fb.width||!fb.height||fb.width>640||fb.height>480)continue;
   const stride=Math.ceil(fb.width*3/4)*4,bmp=Buffer.alloc(54+stride*fb.height);
   bmp.write('BM');bmp.writeUInt32LE(bmp.length,2);bmp.writeUInt32LE(54,10);bmp.writeUInt32LE(40,14);
   bmp.writeUInt32LE(fb.width,18);bmp.writeUInt32LE(fb.height,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(24,28);
   for(let y=0;y<fb.height;++y)for(let x=0;x<fb.width;++x){
    const color=fb.pixels[y*fb.width+x],at=54+(fb.height-1-y)*stride+x*3;
    for(let c=0;c<3;++c)bmp[at+2-c]=color===0?0:color===255?255:stage.paletteRaw[color*8+3+c*2];
   }
   const file=`${edition}/${name.replace(/\.[^.]+$/,'')}-${flat.locationFrame}.bmp`;
   fs.writeFileSync(`${output}/${file}`,bmp);
   entries.push({edition,source:variant.path.replace(/^volumes\//,''),sourceSHA256:variant.sha256,flat:flat.name,
    container:flat.locationFrame,width:fb.width,height:fb.height,file,
    original_md5:crypto.createHash('md5').update(fb.pixels.subarray(0,fb.width*fb.height)).digest('hex'),
    provenance:'Original game artwork; no external original identified or substituted',eligibleInGame:'Static non-matte screen with no visible screen sprites or script text only'});
  }
 }
}
fs.writeFileSync(`${output}/catalog.json`,JSON.stringify({scope:'Private research catalog; no artwork replacements installed.',entries},null,2)+'\n');
const html=`<!doctype html><meta charset="utf-8"><title>DreamFactory artwork research</title><style>body{background:#17202b;color:#eee;font:16px system-ui;padding:24px}main{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:18px}img{width:100%;object-fit:contain;background:black}article{border:1px solid #567;padding:12px}code{overflow-wrap:anywhere}</style><h1>Artwork research catalog</h1><p>Original assets only. Identify an image and establish reliable provenance before replacing it. Game scripts, hotspots and animated scenes remain authoritative.</p><main>${entries.map(e=>`<article><a href="${escape(e.file)}"><img loading="lazy" src="${escape(e.file)}" alt="${escape(e.flat)}"></a><p>${escape(e.edition)} · ${escape(e.source)} · ${escape(e.flat)}</p><code>original_md5=${e.original_md5}</code></article>`).join('')}</main>`;
fs.writeFileSync(`${output}/index.html`,html);
console.log(`Artwork research: ${entries.length} original flat images exported; no replacement claims.`);
