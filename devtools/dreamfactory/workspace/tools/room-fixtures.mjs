// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic SETs and compiled script integration tests; no game data.
import fs from 'node:fs';import path from 'node:path';import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-room-fixtures');fs.mkdirSync(dest,{recursive:true});
for(const name of ['binary','byte-order','container','opcodes','text','script','script-asm']) {
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(path.join(dest,'container.mjs'))),{compileScript}=await import(pathToFileURL(path.join(dest,'script-asm.mjs')));
const u16=(b,p,n)=>new DataView(b.buffer).setUint16(p,n,true),u32=(b,p,n)=>new DataView(b.buffer).setUint32(p,n,true);
const pstr=(b,p,s)=>{b[p]=s.length;for(let i=0;i<s.length;++i)b[p+i+1]=s.charCodeAt(i);};
const envelope=parts=>writeContainerFile({header:{fourCC:65536,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),containers:parts.map((data,id)=>({id,data}))});
const image=color=>new Uint8Array([2,0,4,0,4,color,color,color,color,4,color,color,color,color]);
const main=compileScript(`code openset()
global log, openedvisible, hideopened
openedvisible = setvisible()
if hideopened
setvisible(false)
endif
log = log @ "S"
passcode
endcode
code closeset()
global log
log = log @ "T"
closesetfile()
passcode
endcode
code openscene()
global log
log = log @ "M"
passcode
endcode
code closescene()
global log
log = log @ "m"
passcode
endcode
code shared(v)
return me @ "/" @ target @ "/" @ v
endcode
code passed()
return me @ "/" @ target
endcode`);
const sceneA=compileScript(`code openscene()
global log, redirect
log = log @ "A"
if redirect
redirect = false
currentview("north")
endif
passcode
endcode
code closescene()
global log, replace
log = log @ "a"
if replace
replace = false
opensetfile("second.set")
endif
passcode
endcode
code own(v)
return me @ "/" @ target @ "/" @ v
endcode
code helper()
return shared("helper")
endcode
code stagecall()
return stagelib()
endcode
code passed()
passcode
endcode
code pulse()
global pulses
pulses = pulses + 1
endcode
code keydown(arg)
global keys
keys = keys @ "A"
if arg = "stop"
exitcode
endif
endcode
code removeown()
closesetfile()
return me
endcode`);
const sceneB=compileScript(`code openscene()
global log
log = log @ "D"
passcode
endcode
code closescene()
global log
log = log @ "d"
passcode
endcode`);
for(const edition of ['demo','full','dust']) {
 const dir=`build/room-session-${edition}`,v1=edition==='dust',version=v1?1:4;fs.mkdirSync(dir,{recursive:true});
 const boot=new Uint8Array(128);u32(boot,2,version);
 fs.writeFileSync(dir+'/BOOTFILE',envelope([boot,compileScript(`code boot()
endcode
code openset()
global log
log = log @ "B"
passcode
endcode
code openscene()
global log
log = log @ "b"
passcode
endcode
code closescene()
global log
log = log @ "c"
passcode
endcode
code closeset()
global log
log = log @ "t"
passcode
endcode
code keydown(arg)
global keys
keys = keys @ "R"
sendtoscene(currentscene(), keydown(arg))
endcode`),compileScript(`code keydown(arg)
global keys
keys = keys @ "L"
endcode`)]));
 const head=new Uint8Array(v1?0x1b94+68:0xa2e);u32(head,2,version);
 for(let i=0;i<256;++i){const at=(v1?0x50:0xf2)+i*8;head[at+3]=i;head[at+5]=255-i;head[at+7]=(i*2)&255;}
 const stars=new Uint8Array(v1?28:8);
 const parts=[head,main,sceneA,sceneB,compileScript('code own(v)\nreturn me @ "/" @ target @ "/" @ v\nendcode'),stars];
 if(v1){
  u16(head,0x18,64);u16(head,0x1a,160);u16(head,0x1e,6);u16(head,0x22,5);u16(head,0x26,2);u16(head,0x28,1);u16(head,0x2a,8);u16(head,0x2c,4);u16(head,0x34,3);u32(head,0x1b78,1);pstr(head,0x1b7c,'town');u32(head,0x1b90,2);
  for(let i=0;i<2;++i){const at=0x1b94+i*32;u16(head,at+6,i);u16(head,at+12,i);pstr(head,at+16,i?'Scene B1':'Scene A1');u32(head,at+32,2+i);}
  // Script refs follow the names; the final ref occupies the four-byte tail.
  // In the native/ref table the name is +16, coordinates at +4/+6.
  const moves=[],right={1:3,3:2,2:4,4:1},left={1:4,4:2,2:3,3:1};
  for(let x=0;x<2;++x)for(const f of [1,2,3,4])for(const d of [right,left])moves.push([x,0,f,x,0,d[f]]);
  moves.push([0,0,3,1,0,3],[1,0,4,0,0,4]);
  const reg=new Uint8Array(moves.length*28);parts.push(reg);u32(head,8,moves.length);
  moves.forEach((m,i)=>{for(let j=0;j<6;++j)u16(reg,i*28+j*2,m[j]);u32(reg,i*28+24,parts.length);for(let f=0;f<5;++f)parts.push(image(20+i));});
 }else{
  u32(head,0x54,14);u32(head,0x58,5);u32(head,0x5c,1);u32(head,0x60,6);u32(head,0x64,2);pstr(head,0x70,'town');u16(head,0x84,8);u16(head,0x86,4);pstr(head,0xa0e,'Scene A1');pstr(head,0xa1e,'east');
  const scenes=new Uint8Array(84);parts.push(scenes);
  for(let i=0;i<2;++i){const at=i*42;u16(scenes,at+4,128+i*256);u16(scenes,at+6,128);u32(scenes,at+10,7+i);u32(scenes,at+14,9);u32(scenes,at+18,10);u32(scenes,at+22,2+i);pstr(scenes,at+26,i?'Scene B1':'Scene A1');const vs=new Uint8Array(144);u32(vs,48,2);for(let v=0;v<2;++v){u16(vs,52+v*46+8,v?192:0);u32(vs,52+v*46+22,100+i*100+v);u32(vs,52+v*46+26,i||v?0:11);pstr(vs,52+v*46+30,v?'north':'east');}parts.push(vs);}
  for(let d=0;d<2;++d){const ring=new Uint8Array(132);u32(ring,4,2);for(let v=0;v<2;++v){u16(ring,12+v*60+32,-10-v);u16(ring,12+v*60+34,20+v);u16(ring,12+v*60+36,30+v);u16(ring,12+v*60+38,v?192:0);u32(ring,12+v*60+40,1);u32(ring,12+v*60+44,12+v);u32(ring,12+v*60+56,v);}parts.push(ring);}
  const paintings=new Uint8Array(44);u32(paintings,0,1);u16(paintings,16,10);u16(paintings,18,20);u16(paintings,20,30);u16(paintings,22,40);u32(paintings,24,4);pstr(paintings,28,'door');parts.push(paintings,image(21),new Uint8Array([2,0,4,0,40,40]));
  const roads=new Uint8Array(24),info=new Uint8Array(14);u32(roads,4,1);u32(roads,8,15);u32(roads,12,16);u32(roads,16,17);u32(info,6,100);u32(info,10,200);parts.push(roads,info);
  for(let d=0;d<2;++d){const walk=new Uint8Array(72);u32(walk,4,1);u32(walk,56,12+d);u32(walk,68,0xffffffff);parts.push(walk);}
 }
 fs.writeFileSync(dir+'/FIRST.SET',envelope(parts));
 const entry=parts.map(x=>new Uint8Array(x));
 if(v1){u16(entry[0],0x30,1);u16(entry[0],0x34,1);}
 else {pstr(entry[0],0xa0e,'Scene B1');pstr(entry[0],0xa1e,'north');}
 fs.writeFileSync(dir+'/ENTRY.SET',envelope(entry));
 for(const kind of ['MAIN','SCENE','COUNT','NAME',...(!v1?['PAINT']:[])]) {
  const bad=parts.map(x=>new Uint8Array(x));
  if(kind==='MAIN')u32(bad[0],v1?0x1b78:0x5c,99999);
  if(kind==='SCENE')u32(bad[v1?0:6],v1?0x1b94+32:22,99999);
  if(kind==='COUNT'){if(v1)u16(bad[0],0x26,63);else u32(bad[0],0x64,99999);}
  if(kind==='NAME')bad[v1?0:6][v1?0x1b94+16:26]=16;
  if(kind==='PAINT')u32(bad[11],24,99999);
  fs.writeFileSync(`${dir}/BAD${kind}.SET`,envelope(bad));
 }
 if(!v1){const empty=parts.map(x=>new Uint8Array(x));for(const at of [9,10])for(const v of [0,1])u32(empty[at],12+v*60+44,0);fs.writeFileSync(dir+'/VOID.SET',envelope(empty));}
 const second=parts.map(x=>new Uint8Array(x));pstr(second[0],v1?0x1b7c:0x70,'nighttown');fs.writeFileSync(dir+'/SECOND.SET',envelope(second));
 const stageAt=v1?2100:2120,stride=v1?28:46,stage=new Uint8Array(stageAt+4+stride);u32(stage,2,version);u16(stage,v1?28:40,8);u16(stage,v1?30:42,4);u32(stage,v1?32:44,1);pstr(stage,v1?2084:2104,'library');u32(stage,stageAt,1);pstr(stage,stageAt+4+(v1?12:30),'first');
 fs.writeFileSync(dir+(v1?'/LIBRARY.FLT':'/LIBRARY.STG'),envelope([stage,compileScript('code stagelib()\nclosestagefile()\nreturn me @ "/" @ target\nendcode')]));
 const script=`code test()
global log, redirect, replace, pulses, keys
log = ""
if currentset() != "none" | currentdeg() != -1
return 1
endif
opensetfile("entry.set")
if currentscene() != "Scene B1" | currentview() != "north"
return 17
endif
closesetfile()
opensetfile("entry.set","Scene A1")
if currentscene() != "Scene A1" | currentview() != "north"
return 18
endif
closesetfile()
opensetfile("entry.set","Scene A1","east")
if currentscene() != "Scene A1" | currentview() != "east"
return 19
endif
closesetfile()
log = ""
opensetfile("first.set")
if currentset() != "town" | currentscene() != "Scene A1" | currentview() != "east" | log != "SBAMb"
return 2
endif
if countscenes() != 2 | indextoscene(2) != "Scene B1" | countviews("Scene A1") < 2 | playerxyz(1) != 128 | scenexyz("Scene B1", 1) != 384
return 3
endif
${v1?`if cameraxyz(1) != 64 | cameraxyz(2) != 128 | cameraxyz(3) != 160 | playerxyz(3) != 0 | camerahi() != 160
return 20
endif
if sendtopostfx(0, 1) != "Scene B1" | sendtopostfx(9, 9) != "none" | not sendtoserverfx("Scene B1") | actorexists("Scene B1") != 0 | propexists("Scene B1") != 1
return 16
endif`:''}
if sendtoscenefx("Scene A1", own("argument")) != "scene a1/Scene A1/argument" | sendtoscenefx("Scene A1", helper()) != "scene a1/Scene A1/helper" | sendtoscenefx("Scene A1", passed()) != "town/Scene A1"
return 4
endif
log = ""
currentview("north")
if currentview() != "north" | currentdeg() != 192 | log != "amcAMb"
return 5
endif
log = ""
currentscene("right")
if currentview() != "east" | log != "amcAMb"
return 6
endif
if not roadahead("Scene A1", "east")
return 7
endif
log = ""
currentscene("strait")
if currentscene() != "Scene B1" | log != "amcDMb"
return 8
endif
log = ""
currentscene("Scene A1")
if currentscene() != "Scene A1" | log != "dmcAMb"
return 9
endif
pulses = 0
makeloop("scene", "Scene A1", "pulse", 2)
forceupdate()
forceupdate()
if pulses != 1
return 10
endif
log = ""
redirect = true
currentview("east")
if currentview() != "north" | log != "amcAMb"
return 11
endif
log = ""
replace = true
closesetfile()
if currentset() != "nighttown"
return 12
endif
if sendtoscenefx("Scene A1", removeown()) != "scene a1" | currentset() != "none"
return 13
endif
return 12345
endcode
code quitmove()
global aftermove
currentscene("right")
aftermove = 1
endcode
code holdblack()
clut("black")
visualeffect(plain,0)
currentscene("left")
endcode
code revealroom()
blacktoscreen("set",1)
endcode`;
 fs.writeFileSync(dir+'/test.dfscript',compileScript(script));fs.writeFileSync(dir+'/test.txt',script);
}
console.log('Synthetic room lifecycle/navigation fixtures written');
