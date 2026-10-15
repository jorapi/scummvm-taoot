// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-stage-session');
fs.mkdirSync(dest,{recursive:true});
for(const name of ['byte-order','binary','version','container','opcodes','text','script','script-asm']) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(path.join(dest,'container.mjs')));
const {compileScript}=await import(pathToFileURL(path.join(dest,'script-asm.mjs')));

for(const edition of ['demo','full','dust']) {
  const version=edition==='dust'?1:4, v1=version===1, ext=v1?'flt':'stg';
  const dir=`build/stage-session-${edition}`;fs.mkdirSync(dir,{recursive:true});
  const u32=(d,a,n)=>new DataView(d.buffer).setUint32(a,n,true);
  const u16=(d,a,n)=>new DataView(d.buffer).setUint16(a,n,true);
  const pstr=(d,a,s)=>{d[a]=s.length;for(let i=0;i<s.length;++i)d[a+1+i]=s.charCodeAt(i);};
  const envelope=containers=>writeContainerFile({header:{fourCC:0x48454144,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),containers:containers.map((data,id)=>({id,data}))});
  const routeShop=new Uint8Array(2380), routeProp=new Uint8Array(158);
  u32(routeShop,2,version);u32(routeShop,2340,1);pstr(routeShop,2344,'route-items');u32(routeShop,2360,1);u32(routeShop,2364,2);
  pstr(routeProp,42,'artifact');u32(routeProp,90,2);
  u32(routeProp,94,3);pstr(routeProp,110,'intact');u32(routeProp,126,3);pstr(routeProp,142,'used');
  fs.writeFileSync(`${dir}/ROUTE-ITEMS.SHP`,envelope([routeShop,compileScript('code openshop()\nendcode'),routeProp,new Uint8Array(118)]));
  const at=v1?2100:2120,stride=v1?28:46,head=new Uint8Array(at+4+stride*2);
  u32(head,2,version);u16(head,v1?28:40,320);u16(head,v1?30:42,200);u32(head,v1?32:44,1);pstr(head,v1?2084:2104,'internal');u32(head,at,2);
  for(let i=0;i<2;++i){const p=at+4+stride*i;u32(head,p+(v1?0:6),2+i);u32(head,p+(v1?8:14),i?0:4);pstr(head,p+(v1?12:30),i?'second':'first');}
  const regionAt=v1?0:1028,regions=new Uint8Array(regionAt+4+32*2);u32(regions,regionAt,2);
  for(let i=0;i<2;++i){let p=regionAt+4+i*32;u16(regions,p+4,10);u16(regions,p+6,20);u16(regions,p+8,30);u16(regions,p+10,40);u32(regions,p+12,5);pstr(regions,p+16,i?'upper':'lower');}
  const main=`code openstage()
global log, initialflat
initialflat = currentflat()
log = log @ "S"
gotoflat("first")
endcode
code closestage()
global log
log = log @ "C"
closestagefile()
endcode
code keydown(arg)
global seen
seen = "stage:" @ arg
endcode
code shared()
return me @ "/" @ target
endcode
code removeown()
closestagefile()
return me
endcode`;
  const first=`code openflat()
global log
log = log @ "A"
endcode
code closeflat()
global log, redirect
log = log @ "a"
if redirect
redirect = false
gotoflat("second")
endif
endcode
code keydown(arg)
global seen
seen = "flat:" @ arg
endcode
code own(v)
return me @ "/" @ target @ "/" @ v
endcode
code passed()
passcode
endcode
code pulse()
global pulses
pulses = pulses + 1
endcode
code arm()
makeloop("flat", me, "contextpulse", 2)
return me @ "/" @ target
endcode
code contextpulse()
global contextpulses
contextpulses = contextpulses + 1
endcode`;
  const second=`code openflat()
global log
log = log @ "B"
endcode
code closeflat()
global log
log = log @ "b"
endcode`;
  fs.writeFileSync(`${dir}/CODEX.${ext.toUpperCase()}`,envelope([head,compileScript(main),compileScript(first),compileScript(second),regions,compileScript('code own(v)\nreturn me @ "/" @ target @ "/" @ v\nendcode\ncode armcall()\nreturn arm()\nendcode')]));
  // Lifecycle containment: missing/passed flat handlers reach the stage;
  // a normally handled event stops before the shared stage handler.
  const commonHead = new Uint8Array(at + 4 + stride * 3); commonHead.set(head); u32(commonHead, at, 3);
  pstr(commonHead, v1 ? 2084 : 2104, 'common');
  for (let i=0;i<3;++i) {
    const p=at+4+stride*i; u32(commonHead,p+(v1?0:6),2+i); u32(commonHead,p+(v1?8:14),0);
    pstr(commonHead,p+(v1?12:30),['empty','passing','own'][i]);
  }
  fs.writeFileSync(`${dir}/COMMON.${ext.toUpperCase()}`,envelope([commonHead,
    compileScript('code openflat()\nglobal commonlog, openme, opentarget\ncommonlog = commonlog @ "M"\nopenme = me\nopentarget = target\nendcode\ncode closeflat()\nglobal commonlog, closename\ncommonlog = commonlog @ "m"\nclosename = target\nendcode'),
    compileScript(''),
    compileScript('code openflat()\nglobal commonlog\ncommonlog = commonlog @ "F"\npasscode\nendcode\ncode closeflat()\nglobal commonlog\ncommonlog = commonlog @ "C"\npasscode\nendcode'),
    compileScript('code openflat()\nglobal commonlog\ncommonlog = commonlog @ "O"\nendcode\ncode closeflat()\nglobal commonlog\ncommonlog = commonlog @ "c"\nendcode')
  ]));
  const artHead = new Uint8Array(at + 4 + stride * 2);
  u32(artHead,2,version);u16(artHead,v1?28:40,512);u16(artHead,v1?30:42,384);u32(artHead,v1?32:44,1);pstr(artHead,v1?2084:2104,'display');u32(artHead,at,2);
  for(let i=0;i<256;++i)for(let c=0;c<3;++c)artHead[(v1?36:56)+i*8+3+c*2]=(i*(c+2)+c*19)%256;
  for(let i=0;i<2;++i){const p=at+4+i*stride;u32(artHead,p+(v1?4:10),i+2);pstr(artHead,p+(v1?12:30),i?'second':'first');}
  const art=(variant,matte=false)=>{const b=new Uint8Array(4+384*513);u16(b,0,384);u16(b,2,512);for(let y=0;y<384;++y){b[4+y*513]=4;for(let x=0;x<512;++x)b[5+y*513+x]=matte&&y<264?253:2+((x*3+y*5+variant*31)%250);}return b;};
  fs.writeFileSync(`${dir}/DISPLAY.${ext.toUpperCase()}`,envelope([artHead,compileScript('code openstage()\nendcode'),art(0),art(1)]));
  fs.writeFileSync(`${dir}/MATTE.${ext.toUpperCase()}`,envelope([artHead,compileScript('code openstage()\nendcode'),art(0,true),art(1,true)]));
  const bootHead=new Uint8Array(6);u32(bootHead,2,version);
  fs.writeFileSync(`${dir}/BOOTFILE`,envelope([bootHead,compileScript(`code passed()
return me @ "/" @ target
endcode
code idle()
global idles
idles = idles + 1
forceupdate()
endcode
code keydown(arg)
global seen
seen = seen @ arg
endcode
code keyrepeat(arg)
global seen
seen = seen @ "repeat:" @ arg
endcode
code mousedown(pt)
global clickpoint, clickname, clicktype
clickpoint = pt
clickname = hittest(pt)
clicktype = result()
endcode
code angles(a,b)
return calcdeg(a,b)
endcode
code pointerprobe()
global sampledpoint, sampledbutton
sampledpoint = mouse()
sampledbutton = button()
endcode
code flush()
flushevents()
endcode`)]));
  // Read-only oracle: both shipped engines have the same 512-entry atan table.
  const exe=fs.readFileSync(`game-data/${edition}/installed/${v1?'DF':'TI'}.EXE`);
  const pe=exe.readUInt32LE(60),base=exe.readUInt32LE(pe+52),sec=pe+24+exe.readUInt16LE(pe+20);
  const va=v1?0x4455c8:edition==='demo'?0x45bad8:0x45dce0;let table;
  for(let i=0;i<exe.readUInt16LE(pe+6);++i){const p=sec+i*40,rva=exe.readUInt32LE(p+12),size=Math.max(exe.readUInt32LE(p+8),exe.readUInt32LE(p+16));if(va>=base+rva&&va<base+rva+size){const o=exe.readUInt32LE(p+20)+va-base-rva;table=exe.subarray(o,o+512);}}
  if(!table||table.length!==512)throw new Error('Missing original angle table');
  const records=[]; const packed=(x,y)=>((x&65535)<<16)|(y&65535);
  const expect=(x,y)=>{if(!y)return x<0?128:0;if(!x)return y<0?192:64;const ax=Math.abs(x),ay=Math.abs(y);let a=ax===ay?32:table[Math.trunc(Math.min(ax,ay)*512/Math.max(ax,ay))];if(ax!==ay&&ay>ax)a=64-a;if(x<0)a=128-a;if(y<0)a=-a;return a&255;};
  for(let n=0;n<=512;++n)for(const [x,y] of [[512,n],[n,512],[-512,n],[-n,512],[-512,-n],[-n,-512],[512,-n],[n,-512]])records.push([0,packed(x,y),expect(x,y)]);
  records.push([packed(32760,0),packed(-32760,0),0],[packed(0,32760),packed(0,-32760),64],[packed(-10,-20),packed(-5,80),63]);
  const angleData=Buffer.alloc(records.length*12);records.forEach((r,i)=>r.forEach((n,j)=>angleData.writeUInt32LE(n>>>0,i*12+j*4)));fs.writeFileSync(`${dir}/angles.bin`,angleData);
  const checks=[];const check=s=>checks.push(`if ${s}\nreturn ${checks.length+1}\nendif`);
  checks.push('global log, redirect, pulses\nlog = ""');
  check('not openstagefile("codex")');
  const opened=v1?'SaAA':'SA';
  check(`log != "${opened}" | currentflat() != "first" | currentstage() != "internal" | initialflat != "${v1?'first':'none'}"`);
  check(`result() != "codex.${ext}"`);
  check('countflats() != 2 | indextoflat(2) != "second" | flattoindex("FIRST") != 1 | flattoindex("bad") != 0');
  check('not pointinstage(makepoint(-1,-1)) | not pointinbutton("first","upper",makepoint(40,30)) | pointinbutton("first","upper",makepoint(41,30))');
  check('countbuttons("first") != 2 | indextobutton("first",2) != "upper"');
  check('sendtoflatfx("first",own(7)) != "first/first/7"');
  check(`sendtostagefx(passed()) != "codex.${ext}/stage-test"`);
  check('sendtoflatfx("first",passed()) != "first/first"');
  check('sendtobuttonfx("first","upper",own(9)) != "upper/upper/9"');
  check(`sendtobuttonfx("first","upper",shared()) != "codex.${ext}/upper"`);
  check('sendtobuttonfx("first","upper",armcall()) != "first/upper"');
  checks.push('forceupdate()\nforceupdate()');check('contextpulses != 1');
  checks.push('openstagefile("codex")');check(`log != "${opened}"`);
  checks.push('makeloop("flat","first","pulse",2)\nforceupdate()\nforceupdate()');check('pulses != 1');
  checks.push('redirect = true\ngotoflat("first")');check(`currentflat() != "second" | log != "${opened}aB"`);
  check(`sendtostagefx(removeown()) != "codex.${ext}"`);check(`log != "${opened}aBCb" | currentflat() != "none" | currentstage() != "none" | stagevisible()`);
  checks.push('global commonlog\ncommonlog = ""\nopenstagefile("common")');
  check(`commonlog != "M" | openme != "common.${ext}" | opentarget != "empty"`);
  checks.push('gotoflat("passing")');
  check(`commonlog != "MmFM" | openme != "common.${ext}" | opentarget != "passing" | closename != "empty"`);
  checks.push('gotoflat("own")');check('commonlog != "MmFMCmO" | closename != "passing"');
  checks.push('closestagefile()');check('commonlog != "MmFMCmOc" | closename != "passing"');
  const display = `
code display()
openstagefile("display")
forceupdate()
endcode
code second()
gotoflat("second")
forceupdate()
endcode
code hide()
setvisible(false)
forceupdate()
endcode
code show()
setvisible(true)
forceupdate()
endcode
code dimstage()
mixclut("stage","black",0,255,128)
forceupdate()
endcode
code dimroom()
mixclut("set","black",0,255,192)
forceupdate()
endcode
code resetstage()
clut("stage")
forceupdate()
endcode
code resetroom()
clut("set")
forceupdate()
endcode
code dark()
screentoblack("stage",2)
endcode
code black()
clut("black")
forceupdate()
endcode
code reveal()
blacktoscreen("stage",2)
endcode
code matte()
openstagefile("matte")
forceupdate()
endcode
code refresh()
forceupdate()
endcode
code textprobe()
global measured
drawstring("Cash $123",makepoint(10,320),250,12)
drawstring("Other",makepoint(200,320),250,12)
measured = stringwidth("Cash $123",250,12)
forceupdate()
endcode
code textreplace()
drawstring("$9",makepoint(10,320),250,12)
forceupdate()
endcode
code texterase()
propvisible("textclear",true)
forceupdate()
propvisible("textclear",false)
forceupdate()
endcode`;
  fs.writeFileSync(`${dir}/test.dfscript`,compileScript('code test()\n'+checks.join('\n')+'\nreturn 12345\nendcode\n'+display));
}
console.log('Synthetic stage lifecycle fixtures written');
