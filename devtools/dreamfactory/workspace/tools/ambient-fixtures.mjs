// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic ambient bank and compiled API checks; contains no game assets.
import fs from 'node:fs';
import crypto from 'node:crypto';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-ambient-fixtures');fs.mkdirSync(dest,{recursive:true});
for(const name of ['binary','byte-order','container','opcodes','text','script','script-asm']) {
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(path.join(dest,'container.mjs')));
const {compileScript}=await import(pathToFileURL(path.join(dest,'script-asm.mjs')));
const u16=(b,p,n)=>new DataView(b.buffer).setUint16(p,n,true),u32=(b,p,n)=>new DataView(b.buffer).setUint32(p,n,true);
const pstr=(b,p,s)=>{b[p]=s.length;for(let i=0;i<s.length;++i)b[p+i+1]=s.charCodeAt(i);};
function envelope(parts){return writeContainerFile({header:{fourCC:65536,fileSize:0,containerCount:parts.length,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),order:'le',containers:parts.map((data,id)=>({id,data}))});}
function audio(samples=2000){const b=new Uint8Array(48+samples);u32(b,0,65536);u16(b,26,2);u32(b,28,1000);u32(b,36,samples*2);u32(b,44,48);b.fill(128,48);return b;}
const script=`code test()
stopcricket()
makecricket("FIRST", 0, 0, 0, 2, 0)
makecricket("second", 0, 0, 20, 3, 0)
if countcrickets() != 2 | not iscricket("First") | indextocricket(1) != "first" | indextocricket(0) != "" | indextocricket(3) != ""
return 1
endif
makecricket("first", 1, 2, 30, 4, 0)
if countcrickets() != 2 | indextocricket(1) != "first" | indextocricket(2) != "second"
return 2
endif
pausecricket("ALL", 1)
stopcricket("SECOND")
if countcrickets() != 1 | iscricket("second")
return 3
endif
makecricket("third",0,0,20,3,0)
stopcricket("first")
if countcrickets() != 1 | indextocricket(1) != "third"
return 4
endif
makecricket("fourth",0,0,20,3,0)
if countcrickets() != 2 | indextocricket(1) != "fourth" | indextocricket(2) != "third"
return 5
endif
stopcricket()
return 12345
endcode`;
// v1 uses the same opcode numbers for projectiles, not ambient sounds.
const ballScript=`code test()
global ballwhy, ballends, ballactive, ballrestart
ballwhy = ""
ballends = 0
ballrestart = 0
stopcricket()
makecricket("FIRST", 1, 2, 3, 0, 0, 60)
makecricket("second", 0, 0, 0, 0, 0, 60)
if countcrickets() != 2 | not iscricket("First") | indextocricket(1) != "first"
return 1
endif
makecricket("first", 4, 5, 6, 0, 0, 60)
if countcrickets() != 2 | indextocricket(1) != "first" | indextocricket(2) != "second"
return 2
endif
pausecricket("ALL", 1)
pausecricket("first", 1)
pausecricket("all", 0)
stopcricket("SECOND")
if countcrickets() != 1 | iscricket("second")
return 3
endif
stopcricket()
return 12345
endcode
code endball(reason)
global ballwhy, ballends, ballactive, ballrestart
ballwhy = reason
ballends = ballends + 1
ballactive = iscricket(me)
if ballrestart
ballrestart = 0
makecricket(me, 0, 0, 0, 0, 0, 2)
endif
endcode`;
const themeScript=`
code themecontrols()
opentrackfile("music.snd")
opentrackfile("music2.snd")
themevol("music",81)
themevol("music2",49)
playtheme("music")
themevol("music2",151)
wavevolume(4)
if themevol("music") != 81 | themevol("music2") != 151 | currenttheme() != "music"
return 31
endif
wavevolume(9)
halttheme()
opentrackfile("music.snd")
if counttracks() != 3 | indextotrack(1) != "music" | indextotrack(2) != "music2" | indextotrack(3) != "music"
return 32
endif
closetrackfile("music.snd")
if counttracks() != 2 | themevol("music") != 255
return 33
endif
closetrackfile("music.snd")
closetrackfile("music2.snd")
return 12345
endcode
code sounddirectory()
opentrackfile("music.snd")
if countsounds("music") != 2 | indextosound("music",1) != "cue" | indextosound("music",2) != "cue"
return 34
endif
voicesound("cue")
haltvoice()
closetrackfile("music")
return 12345
endcode
`;
for(const edition of ['demo','full','dust']) {
 const dir=`build/ambient-session-${edition}`;fs.mkdirSync(dir,{recursive:true});
 const boot=new Uint8Array(128);u32(boot,2,edition==='dust'?1:4);
 fs.writeFileSync(dir+'/BOOTFILE',envelope([boot,compileScript('code boot()\nendcode')]));
 const head=new Uint8Array(52);u32(head,2,4);u32(head,32,1);pstr(head,36,'ambient');
 const singles=new Uint8Array(112);u32(singles,4,4);u32(singles,12,2);pstr(singles,18,'chirp');u32(singles,38,3);pstr(singles,44,'other');u32(singles,64,4);pstr(singles,70,'third');u32(singles,90,5);pstr(singles,96,'effect');
 function bank(name, names) {
  if(edition==='dust') {
   const h=new Uint8Array(186+names.length*24);u32(h,2,1);u16(h,24,names.length);pstr(h,158,name);
   names.forEach((n,i)=>pstr(h,186+i*24,n));
   return envelope([h,...names.map(()=>audio())]);
  }
  const h=head.slice();pstr(h,36,name);
  if(names.length===4)return envelope([h,singles,audio(),audio(),audio(),audio()]);
  const s=new Uint8Array(34);u32(s,4,1);u32(s,12,2);pstr(s,18,names[0]);
  return envelope([h,s,audio()]);
 }
 fs.writeFileSync(dir+'/AMBIENT.SND',bank('ambient',['chirp','other','third','effect']));
 fs.writeFileSync(dir+'/DUP.SND',bank('dup',['chirp']));
 if(edition==='dust'){
  const h=new Uint8Array(186+4*24);u32(h,2,1);u16(h,24,2);u16(h,26,2);u16(h,28,4);pstr(h,158,'music');
  [2,1,2,2].forEach((n,i)=>u16(h,30+i*2,n));['cue','cue','a','b'].forEach((n,i)=>pstr(h,186+i*24,n));
  fs.writeFileSync(dir+'/MUSIC.SND',envelope([h,audio(100),audio(200),audio(100),audio(100)]));
  const other=h.slice();pstr(other,158,'music2');fs.writeFileSync(dir+'/MUSIC2.SND',envelope([other,audio(100),audio(200),audio(100),audio(100)]));
 }else{
  const h=head.slice();u32(h,28,1);u32(h,32,0);pstr(h,36,'music');const t=new Uint8Array(270+2*26);u32(t,0,1);u16(t,4,4);
  [2,1,2,2].forEach((n,i)=>u16(t,6+i*2,n));u32(t,266,2);u32(t,274,2);u32(t,300,3);
  fs.writeFileSync(dir+'/MUSIC.SND',envelope([h,t,audio(100),audio(100)]));
  const other=h.slice();pstr(other,36,'music2');fs.writeFileSync(dir+'/MUSIC2.SND',envelope([other,t,audio(100),audio(100)]));
 }
 for(const name of ['aux','late']) {
  fs.writeFileSync(dir+'/'+name.toUpperCase()+'.SND',bank(name,[name]));
 }
 {
  const v1=edition==='dust', at=v1?2100:2120, pos=at+4;
  const stage=new Uint8Array(pos+(v1?28:46));u32(stage,2,v1?1:4);
  u16(stage,v1?28:40,512);u16(stage,v1?30:42,384);u32(stage,v1?32:44,1);pstr(stage,v1?2084:2104,'blank');u32(stage,at,1);
  u32(stage,pos+(v1?0:6),1);u32(stage,pos+(v1?4:10),2);pstr(stage,pos+(v1?12:30),'blank');
  const pixel=new Uint8Array([1,0,1,0,4,1]);
  fs.writeFileSync(dir+'/BLANK.STG',envelope([stage,compileScript('code unused()\nendcode'),pixel]));
 }
 fs.writeFileSync(dir+'/test.dfscript',compileScript((edition==='dust'?ballScript:script)+themeScript));
 fs.mkdirSync(dir+'/enhancements',{recursive:true});
 const wav=Buffer.alloc(44+2000*4);
 wav.write('RIFF',0);wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);
 wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(2,22);
 wav.writeUInt32LE(1000,24);wav.writeUInt32LE(4000,28);wav.writeUInt16LE(4,32);wav.writeUInt16LE(16,34);
 wav.write('data',36);wav.writeUInt32LE(8000,40);
 for(let i=0;i<2000;++i){wav.writeInt16LE(i+1,44+i*4);wav.writeInt16LE(-i-1,46+i*4);}
 fs.writeFileSync(dir+'/enhancements/stereo.wav',wav);
 fs.writeFileSync(dir+'/enhancements/broken.wav',Buffer.from('bad wave'));
 fs.writeFileSync(dir+'/enhancements/pack.ini',`[pack]\nformat=1\nboot_md5=00000000000000000000000000000000\n[music-valid]\nsource=flat/MUSIC.SND\nfile=stereo.wav\nloop_start=400\nloop_end=1800\n[music-escape]\nsource=escape\nfile=../stereo.wav\n[music-invalid]\nsource=invalid\nfile=broken.wav\n[music-range]\nsource=range\nfile=stereo.wav\nloop_start=1900\nloop_end=1800\n`);
 const bmp=Buffer.alloc(62);bmp.write('BM');bmp.writeUInt32LE(62,2);bmp.writeUInt32LE(54,10);bmp.writeUInt32LE(40,14);bmp.writeUInt32LE(2,18);bmp.writeUInt32LE(1,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(24,28);bmp.writeUInt32LE(8,34);Buffer.from([30,20,10,60,50,40]).copy(bmp,54);
 fs.writeFileSync(dir+'/enhancements/art.bmp',bmp);
 const originalMD5=crypto.createHash('md5').update(Buffer.from([1])).digest('hex');
 let pack=fs.readFileSync(dir+'/enhancements/pack.ini','utf8');pack=pack.replace('[music-valid]\n','[music-valid]\npreserve_original_intro=true\n');fs.writeFileSync(dir+'/enhancements/pack.ini',pack);
 fs.appendFileSync(dir+'/enhancements/pack.ini','\n[music-legacy-radio]\nsource=titanic1/DATA/BEDRAD1.TRK\nfile=stereo.wav\n[music-radio-override]\nsource=titanic2/DATA/BEDRAD1.TRK\nfile=stereo.wav\npreserve_original_intro=false\n[music-plain]\nsource=plain\nfile=stereo.wav\n[music-intro-invalid]\nsource=intro-invalid\nfile=stereo.wav\npreserve_original_intro=invalid\n');
 fs.appendFileSync(dir+'/enhancements/pack.ini',`\n[art-valid]\nsource=flat/blank.stg\nflat=blank\nfile=art.bmp\noriginal_md5=${originalMD5}\nx=10\ny=20\n[art-bounds]\nsource=flat/blank.stg\nflat=bounds\nfile=art.bmp\noriginal_md5=${originalMD5}\nx=639\n[art-escape]\nsource=flat/blank.stg\nflat=escape\nfile=../art.bmp\noriginal_md5=${originalMD5}\n`);
}
console.log('Synthetic ambient fixtures written');
