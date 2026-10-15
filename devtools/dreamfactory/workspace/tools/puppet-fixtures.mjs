// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic v1/v4 puppet reader and compiled session fixtures; no game assets.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-puppet-fixtures');fs.mkdirSync(dest,{recursive:true});
for(const name of ['byte-order','binary','container','opcodes','text','script','script-asm']){
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(`${dest}/${name}.mjs`,js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(`${dest}/container.mjs`));
const {compileScript}=await import(pathToFileURL(`${dest}/script-asm.mjs`));
const u32=(d,a,n)=>new DataView(d.buffer).setUint32(a,n,true),u16=(d,a,n)=>new DataView(d.buffer).setInt16(a,n,true);
const pstr=(d,a,s)=>{d[a]=s.length;for(let i=0;i<s.length;++i)d[a+i+1]=s.charCodeAt(i);};
const envelope=parts=>writeContainerFile({header:{fourCC:0x48454144,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),containers:parts.map((data,id)=>({id,data}))});
for(const edition of ['demo','full','dust']){
 const version=edition==='dust'?1:4,dir=`build/puppet-session-${edition}`;fs.mkdirSync(dir,{recursive:true});
 fs.copyFileSync('build/runtime-data/LiberationSans-Regular.ttf',`${dir}/FONT.TTF`);
 for(const movie of ['AUTO','DUST'])fs.copyFileSync(`build/movie-session-${edition}/${movie}.MOV`,`${dir}/${movie}.MOV`);
 const portrait={demo:'DPENNY',full:'PENNY1',dust:'MWIFE'}[edition];
 const picture=corpusFiles(edition).find(p=>p.toUpperCase().endsWith('/'+portrait+'.PUP'));
 if(!picture)throw new Error(`Missing supplied portrait: ${edition}`);
 fs.copyFileSync(`game-data/${edition}/${picture}`,`${dir}/PORTRAIT.PUP`);
 fs.copyFileSync(`game-data/${edition}/${picture}`,`${dir}/${portrait}.PUP`);
 const head=new Uint8Array(2160+2*312);u32(head,2,version);u32(head,0x85a,4);pstr(head,0x85e,'inside');u16(head,2158,2);
 for(let i=0;i<256;++i)for(let c=0;c<3;++c)head[58+i*8+3+c*2]=(i+c)&255;
 for(let i=0;i<4;++i){u32(head,0x83a+4*i,10+i);u32(head,0x84a+4*i,20+i);}
 for(let i=0;i<2;++i){const at=2160+i*312;u16(head,at,version===1?0:i);u32(head,at+12,9+i);pstr(head,at+24,i?'Other words':'Hello there');pstr(head,at+280,`line.${i}`);}
 const scripts=new Uint8Array(104);u16(scripts,22,2);u32(scripts,24,5);pstr(scripts,32,'Boot Script');u32(scripts,64,6);pstr(scripts,72,'Branch');
 const stance=new Uint8Array(2904);for(let i=0;i<11;++i){u16(stance,22+i*262,i===0?1:0);u16(stance,24+i*262,-i);u16(stance,26+i*262,i);u32(stance,28+i*262,4);}
 const directory=new Uint8Array(1610);u32(directory,22,7);u32(directory,26,8);u32(directory,30,0x23450000);
 const sprite=new Uint8Array([1,0,1,0,0,0,0,0,2,0,6,0]);
 const anim=(x,y)=>{const d=new Uint8Array(82);for(let i=0;i<11;++i){u16(d,16+i*6,i===0?0:-1);u16(d,18+i*6,y);u16(d,20+i*6,x);}return d;};
 const main=compileScript(`code boot()
global boots
boots = boots + 1
endcode
code helper()
return me
endcode
code passed()
return me @ "/" @ target
endcode`);
 const branch=compileScript(`code own(v)
return me @ "/" @ target @ "/" @ v @ "/" @ helper()
endcode
code passed()
passcode
endcode
code closeown()
closepuppetfile()
return me @ "/" @ target
endcode
code replaceown()
openpuppetfile("alternate.pup")
return me @ "/" @ target
endcode`);
 const parts=[head,new Uint8Array(8),scripts,version===1?stance:directory,sprite,main,branch,stance,stance,anim(15,22),anim(100,-5)];
 fs.writeFileSync(`${dir}/CODEX.PUP`,envelope(parts));
 const speech=parts.map(p=>p.slice());
 const poses=new Uint8Array(82*3);for(let i=0;i<3;++i)poses.set(anim(10+i*10,22),i*82);
 speech[9]=poses;u32(speech[0],2160+8,11);
 const voice=new Uint8Array(48+200);u32(voice,0,65536);u16(voice,26,2);u32(voice,28,1000);u32(voice,36,400);u32(voice,44,48);voice.fill(128,48);speech.push(voice);
 fs.writeFileSync(`${dir}/SPEECH.PUP`,envelope(speech));

 const idle=speech.map(p=>p.slice());pstr(idle[0],2160+312+280,'idle 1');u32(idle[0],2160+312+8,11);u32(idle[0],0x83a,1);u32(idle[0],0x84a,1);fs.writeFileSync(`${dir}/IDLE.PUP`,envelope(idle));
 const art=parts.map(p=>p.slice());
 const smallSprite=(color=5)=>{const d=new Uint8Array(19);u16(d,0,2);u16(d,2,2);u16(d,4,-1);u16(d,6,-1);u16(d,8,3);d.set([6,color,5],10);u16(d,13,4);d.set([7,0,7,255],15);return d;};
 art.push(smallSprite(),new Uint8Array([1,0,1,0,0,0,0,0,2,0,6,9]));
 for(const id of version===1?[3]:[7,8]){u16(art[id],22+262,1);u32(art[id],28+262,11);u16(art[id],22+524,1);u32(art[id],28+524,12);}
 art[4]=smallSprite();u16(art[4],4,60);u16(art[4],6,256);
 art[9]=anim(0,0);u16(art[9],16+6,0);u16(art[9],16+12,0);u16(art[9],18+12,2);u16(art[9],20+12,2);
 fs.writeFileSync(`${dir}/RENDER.PUP`,envelope(art));
 const other=art.map(p=>p.slice());other[11]=smallSprite(6);fs.writeFileSync(`${dir}/RENDEROTHER.PUP`,envelope(other));
 const malformed=art.map(p=>p.slice());malformed[11]=smallSprite().slice(0,9);fs.writeFileSync(`${dir}/RENDERBAD.PUP`,envelope(malformed));
 const alternate=parts.map(p=>p.slice());pstr(alternate[0],0x85e,'alternate');fs.writeFileSync(`${dir}/ALTERNATE.PUP`,envelope(alternate));
 const bad=(name,change)=>{const p=parts.map(d=>d.slice());change(p);fs.writeFileSync(`${dir}/${name}.pup`,envelope(p));};
 bad('bad-head',p=>p[0]=p[0].slice(0,2159));bad('bad-version',p=>u32(p[0],2,5));bad('bad-lines',p=>u16(p[0],2158,3));
 bad('bad-band',p=>u32(p[0],0x85a,999));bad('bad-script',p=>u32(p[2],24,999));bad('bad-name',p=>p[0][0x85e]=16);
 bad('bad-layer',p=>u16(p[version===1?3:7],22,33));bad('bad-frame',p=>u32(p[version===1?3:7],28,999));
 bad('bad-stance',p=>u16(p[0],2160,version===1?1:2));bad('bad-animation',p=>p[9]=p[9].slice(0,81));
 const boot=new Uint8Array(6);u32(boot,2,version);fs.writeFileSync(`${dir}/BOOTFILE`,envelope([boot,compileScript(`code boot()
endcode
code mousedown(point)
global routecounter, multiroute
routecounter = routecounter + 1
openpuppetfile("speech")
puppetspeak("line.0")
closepuppetfile()
if multiroute
openpuppetfile("alternate.pup")
puppetspeak("line.0")
closepuppetfile()
endif
endcode`)]));
 const v1=version===1,flatAt=v1?2100:2120,stride=v1?28:46;
 const routeStage=new Uint8Array(flatAt+4+stride);
 u32(routeStage,2,version);u16(routeStage,v1?28:40,512);u16(routeStage,v1?30:42,384);u32(routeStage,v1?32:44,1);
 pstr(routeStage,v1?2084:2104,'route');u32(routeStage,flatAt,1);
 u32(routeStage,flatAt+4+(v1?4:10),2);pstr(routeStage,flatAt+4+(v1?12:30),'first');
 const routeArt=new Uint8Array(4+384*513);u16(routeArt,0,384);u16(routeArt,2,512);
 for(let y=0;y<384;++y){routeArt[4+y*513]=4;routeArt.fill(0,5+y*513,5+y*513+512);}
 fs.writeFileSync(`${dir}/ROUTE.${v1?'FLT':'STG'}`,envelope([routeStage,compileScript('code openstage()\nendcode'),routeArt]));
 const test=`code test()
global boots
if currentpuppet() != "none" | countpuppets() != 0 | puppetvisible() != 0 | countbevels() != 0
return 1
endif
if not openpuppetfile("CODEX") | currentpuppet() != "inside" | countpuppets() != 2 | boots != 0
return 2
endif
if indextopuppet(1) != "boot script" | indextopuppet(2) != "branch" | indextopuppet(0) != "" | indextopuppet(3) != ""
return 3
endif
if sendtopuppetfx("BrAnCh",own(me)) != "branch/puppet-test/puppet-test/branch"
return 4
endif
if sendtopuppetfx("branch",passed()) != "boot script/puppet-test"
return 5
endif
sendtopuppet("boot script",boot())
if boots != 1
return 6
endif
puppetvisible(false)
if puppetvisible() | currentpuppet() != "inside" | countpuppets() != 2
return 7
endif
puppetbase("LINE.1")
puppetbevel("first",101)
puppetbevel("second",102)
puppetbevel("third",103)
puppetbevel("fourth",104)
if countbevels() != 4
return 9
endif
puppetscramble()
puppetbevel("last",105)
puppetbevel("overflow",106)
if countbevels() != 5
return 10
endif
if openpuppetfile("missing") | currentpuppet() != "inside"
return 8
endif
return 12345
endcode
code clearbase()
puppetclear()
if countbevels() != 0
error()
endif
puppetbase("")
endcode
code unknownbase()
puppetbase("unknown")
endcode
code replaceown()
return sendtopuppetfx("branch",replaceown())
endcode
code closeown()
return sendtopuppetfx("branch",closeown())
endcode
code speak()
puppetspeak("line.0")
return 42
endcode
code skiprun()
puppetspeak("line.0")
puppetspeak("line.1")
return 43
endcode
code quitspeech()
global afterspeech
afterspeech = 0
puppetspeak("line.0")
afterspeech = 1
endcode
code choices()
puppetclear()
puppetbevel("Hello there",701)
puppetbevel("answer",902)
return puppetevent(-1)
endcode
code accesschoices()
puppetclear()
puppetbevel("I would like to hear the complete account of what happened earlier this evening, including the details that might help us understand where everyone went and why they made those decisions.",701)
puppetbevel("A short reply.",902)
return puppetevent(-1)
endcode
code waitchoices()
return puppetevent(-1)
endcode
code quitchoices()
global afterchoice
afterchoice = 0
puppetevent(-1)
afterchoice = 1
endcode
code displayopen()
openpuppetfile("render")
puppetbevel("A visible answer.",701)
forceupdate()
return 1
endcode
code displayportrait()
openpuppetfile("${portrait}")
puppetbevel("What can you tell me?",701)
forceupdate()
return 1
endcode
code actualspeak(line)
puppetspeak(line)
return 1
endcode
code displayhide()
puppetvisible(false)
return 1
endcode
code displayshow()
puppetvisible(true)
return 1
endcode
code displayclose()
closepuppetfile()
return 1
endcode
code displayblack()
clut("black")
forceupdate()
return 1
endcode
code displayreveal()
blacktoscreen("puppet",2)
return 1
endcode
code displayfade()
screentoblack("current",2)
return 1
endcode
code displaymix()
mixclut("puppet","black",5,5,255)
return 1
endcode
code displayreset()
clut("puppet")
return 1
endcode
code displaydimroom()
mixclut("set","black",7,7,255)
return 1
endcode
code displayresetroom()
clut("set")
return 1
endcode
code displaymovie()
playmovie("auto.mov")
return 1
endcode
code displaydustmovie()
playmovie("dust.mov")
return 1
endcode
code displayforce()
forceupdate()
return 1
endcode
code badopen()
openpuppetfile("bad-lines.pup")
endcode`;
 fs.writeFileSync(`${dir}/test.dfscript`,compileScript(test));fs.writeFileSync(`${dir}/test.txt`,test);
}
console.log('Synthetic puppet reader/session fixtures written');
