// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic movies and compiled host tests; no game assets are copied.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-movie-fixtures');fs.mkdirSync(dest,{recursive:true});
for(const name of ['binary','byte-order','container','opcodes','text','script','script-asm']) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(path.join(dest,'container.mjs')));
const {compileScript}=await import(pathToFileURL(path.join(dest,'script-asm.mjs')));
const u16=(b,p,n)=>new DataView(b.buffer).setUint16(p,n,true),u32=(b,p,n)=>new DataView(b.buffer).setUint32(p,n,true);
const pstr=(b,p,s)=>{b[p]=s.length;for(let i=0;i<s.length;++i)b[p+i+1]=s.charCodeAt(i);};
function envelope(parts){return writeContainerFile({header:{fourCC:65536,fileSize:0,containerCount:parts.length,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),order:'le',containers:parts.map((data,id)=>({id,data}))});}
function image(color){const b=new Uint8Array(14);u16(b,0,2);u16(b,2,4);b.set([4,color,color,color,color,4,color,color,color,color],4);return b;}
function audio(samples){const b=new Uint8Array(48+samples);u32(b,0,65536);u16(b,26,2);u32(b,28,1000);u32(b,36,samples*2);u32(b,44,48);b.fill(128,48);return b;}
function palette(b,at){for(let i=0;i<256;++i){b[at+i*8+3]=i;b[at+i*8+5]=255-i;b[at+i*8+7]=i;}}
function head4(frames,bias=0,next=0,bed=false){const b=new Uint8Array(2172+frames*42);u32(b,2,4);u32(b,24,1);u32(b,28,3);u16(b,36,2);u16(b,38,3);u32(b,44,next);pstr(b,64,'f1');pstr(b,80,'f2');u32(b,96,bed?7:0);u32(b,100,bed?9:0);palette(b,108);u16(b,2160,6);u16(b,2162,8);u32(b,2168,frames);for(let i=0;i<frames;++i){const p=2172+i*42;u16(b,p+8,2);u16(b,p+10,4);u32(b,p+12,i+1);u32(b,p+16,frames+i+1);pstr(b,p+26,'f'+i);}return b;}
function logic(type,sound='',wait=false,region=false){const b=new Uint8Array(region?1158:66);u16(b,0,type);u32(b,2,3);b[6]=wait?1:0;pstr(b,18,sound);if(region){u32(b,1090,1);u16(b,1094,1);u16(b,1106,20);u16(b,1108,20);}return b;}
function singles(){const b=new Uint8Array(50);u32(b,4,1);u32(b,12,8);pstr(b,18,'line');return b;}
function loops(){const b=new Uint8Array(296);u16(b,4,2);u16(b,6,1);u16(b,8,1);u32(b,266,1);u32(b,274,10);return b;}
for(const edition of ['demo','full','dust']) {
 const dir=`build/movie-session-${edition}`;fs.mkdirSync(dir,{recursive:true});const version=edition==='dust'?1:4;
 const boot=new Uint8Array(128);u32(boot,2,version);fs.writeFileSync(dir+'/BOOTFILE',envelope([boot,compileScript('code boot()\nendcode')]));
 const names=['first','second','absent-cue'];
 if(version===1) {
  const h=new Uint8Array(186+24*names.length);u32(h,2,1);u16(h,24,names.length);pstr(h,158,'fx');names.forEach((n,i)=>pstr(h,186+24*i,n));
  fs.writeFileSync(dir+'/FX.SND',envelope([h,...names.map(()=>audio(5000))]));
 } else {
  const h=new Uint8Array(52),s=new Uint8Array(8+26*names.length);u32(h,2,4);u32(h,32,1);pstr(h,36,'fx');u32(s,4,names.length);
  names.forEach((n,i)=>{u32(s,12+26*i,2+i);pstr(s,18+26*i,n);});
  fs.writeFileSync(dir+'/FX.SND',envelope([h,s,...names.map(()=>audio(5000))]));
 }
 const a=head4(3,0,11,true),b=head4(2);pstr(b,80,'f1');
 fs.writeFileSync(dir+'/AUTO.MOV',envelope([a,image(21),image(22),image(23),logic(6,'line'),logic(6,'',true),logic(1),singles(),audio(300),loops(),audio(100),b,image(24),image(25),logic(6),logic(1)]));
 const tailA=head4(3,0,11,true),tailB=head4(2);u32(tailA,2168,1);u32(tailB,2168,1);
 fs.writeFileSync(dir+'/SEGTAIL.MOV',envelope([tailA,image(21),image(22),image(23),logic(1,'line'),logic(1),logic(1),singles(),audio(1000),loops(),audio(100),tailB,image(24),image(25),logic(1,'',true),logic(1)]));
 // Natural and interactive movie exits both stop the shared cue voice.
 for(const [name,interactive] of [['CARRY',false],['CLICK',true],['QUIET',false]]){
   const h=head4(3,0,0,true);u32(h,2168,1);pstr(h,64,'f0');pstr(h,80,'');
   // An absent cue preserves current voice/follow state; voice waits are global.
   fs.writeFileSync(`${dir}/${name}.MOV`,envelope([h,image(31),image(32),image(33),logic(1,name==='QUIET'?'absent-cue':'line',name==='QUIET',interactive),logic(1),logic(1),singles(),audio(1000),loops(),audio(100)]));
 }
 const loopHead=head4(3,0,0,true);u32(loopHead,2168,1);pstr(loopHead,64,'f0');pstr(loopHead,80,'');
 const loopSounds=singles();u32(loopSounds,8,3);
 fs.writeFileSync(dir+'/LOOP.MOV',envelope([loopHead,image(31),image(32),image(33),logic(1,'line',false,true),logic(1),logic(1),loopSounds,audio(1000),loops(),audio(100)]));
 const silent=head4(1);u32(silent,2168,1);pstr(silent,64,'f0');pstr(silent,80,'');
 fs.writeFileSync(dir+'/SILENT.MOV',envelope([silent,image(31),logic(1)]));
 const d=new Uint8Array(2242+3*80);u32(d,2,1);u16(d,24,3);u16(d,26,1);u16(d,28,1);u16(d,52,2);u16(d,2110,1);u16(d,2112,1);u16(d,34,2);u16(d,36,4);u16(d,38,3);u16(d,46,1);u16(d,48,2);palette(d,62);
 for(let i=0;i<3;++i){const p=2242+i*80;u32(d,p+2,3);u16(d,p+22,i===2?1:2);u16(d,p+24,i+1);u16(d,p+26,i===1?1:0);u16(d,p+28,i+3);if(i===0)u32(d,p+32,1);}
 // Unreferenced trailing audio must not become part of the authored bed.
 fs.writeFileSync(dir+'/DUST.MOV',envelope([d,audio(300),audio(100),image(41),image(42),image(43),audio(700)]));
 // Close-up controller fixture: region zero exits; a different region visits frame 1.
 const controlHead=head4(2);pstr(controlHead,64,'f1');pstr(controlHead,80,'');
 const controlLogic=new Uint8Array(1094+2*64);u16(controlLogic,0,1);u32(controlLogic,2,3);u32(controlLogic,1090,2);
 for(let i=0;i<2;++i){const at=1094+i*64;u16(controlLogic,at,i?2:1);u16(controlLogic,at+8,i?20:300);u16(controlLogic,at+10,i?20:400);u16(controlLogic,at+12,i?100:350);u16(controlLogic,at+14,i?100:480);if(i)pstr(controlLogic,at+48,'f1');}
 fs.writeFileSync(dir+'/CONTROL.MOV',envelope([controlHead,image(51),image(52),controlLogic,logic(1)]));
 const controlV1=new Uint8Array(2242+2*80+30);u32(controlV1,2,1);u16(controlV1,24,2);u16(controlV1,34,2);u16(controlV1,36,4);u16(controlV1,38,3);u16(controlV1,46,1);u16(controlV1,48,65535);palette(controlV1,62);
 const regionAt=2242+2*80;
 for(let i=0;i<2;++i){const at=2242+i*80;u16(controlV1,at,i?0:2);u32(controlV1,at+2,3);u16(controlV1,at+22,1);u16(controlV1,at+28,i+1);if(!i)u16(controlV1,at+36,regionAt);}
 u16(controlV1,regionAt,1);u16(controlV1,regionAt+2,300);u16(controlV1,regionAt+4,400);u16(controlV1,regionAt+6,350);u16(controlV1,regionAt+8,480);
 u16(controlV1,regionAt+14,2);u16(controlV1,regionAt+16,20);u16(controlV1,regionAt+18,20);u16(controlV1,regionAt+20,100);u16(controlV1,regionAt+22,100);u16(controlV1,regionAt+28,1);
 fs.writeFileSync(dir+'/CONTROL1.MOV',envelope([controlV1,image(51),image(52)]));
 const script=`code control()
playmovie("control.mov")
if not actionframe(1)
return 11
endif
return 12345
endcode
code control1()
playmovie("control1.mov")
if not actionframe(1)
return 12
endif
return 12345
endcode
code auto()
before = tick()
playmovie("auto.mov")
if tick() - before < 24 | not actionframe(1) | not actionframe(2) | actionframe(0) | actionframe(3)
return 1
endif
return 12345
endcode
code dust()
before = tick()
playmovie("dust.mov")
if tick() - before < 21 | not actionframe(1) | not actionframe(2)
return 2
endif
return 12345
endcode
code carry()
playmovie("carry.mov")
if not actionframe(1) | actionframe(2)
return 3
endif
return 12345
endcode
code click()
before = tick()
playmovie("click.mov")
if tick() - before < 6 | not actionframe(1) | actionframe(2)
return 4
endif
return 12345
endcode
code quiet()
before = tick()
playmovie("quiet.mov")
if tick() - before > 20
return 5
endif
return 12345
endcode
code segmenttail()
before = tick()
playmovie("segtail.mov")
if tick() - before > 20
return 10
endif
return 12345
endcode
code voicewait()
before = tick()
playmovie("quiet.mov")
if tick() - before < 45 | not voicedone()
return 8
endif
return 12345
endcode
code loopmovie()
playmovie("loop.mov")
return 12345
endcode
code silent()
playmovie("silent.mov")
return 12345
endcode
code quitmovie()
global afterquit
playmovie("click.mov")
afterquit = 1
return 12345
endcode`;
 fs.writeFileSync(dir+'/test.dfscript',compileScript(script));fs.writeFileSync(dir+'/test.txt',script+'\n');
}
for(const edition of ['demo','full','dust']) {
 const dir=`build/movie-session-${edition}`;
 const name=edition==='full'?'PROZAC':edition==='dust'?'YUNNIEND':'POSTDEMO';
 fs.copyFileSync(`${dir}/QUIET.MOV`,`${dir}/${name}.MOV`);
 fs.writeFileSync(`${dir}/ending.dfscript`,compileScript(`code ending()\nplaymovie("${name.toLowerCase()}.mov")\nreturn 12345\nendcode`));
}
console.log('Synthetic movie session fixtures written');
