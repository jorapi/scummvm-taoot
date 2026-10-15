// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-prop');
fs.mkdirSync(dest,{recursive:true});
for(const name of ['byte-order','binary','version','container','opcodes','text','script','script-asm']) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(path.join(dest,'container.mjs')));
const {compileScript}=await import(pathToFileURL(path.join(dest,'script-asm.mjs')));
for(const edition of ['demo','full','dust'])writeSessionFixture(edition);
console.log('Synthetic prop session fixtures written');
function writeSessionFixture(edition) {
  const version=edition==='dust'?1:4;
  const dir=`build/prop-session-${edition}`;fs.mkdirSync(dir,{recursive:true});
  const u32=(data,at,n)=>new DataView(data.buffer).setUint32(at,n,true);
  const pstr=(data,at,s)=>{data[at]=s.length;for(let i=0;i<s.length;++i)data[at+1+i]=s.charCodeAt(i);};
  const head=new Uint8Array(2396);u32(head,2,version);u32(head,2340,1);u32(head,2360,2);u32(head,2364,2);u32(head,2380,5);
  pstr(head,2344,'refshop');
  const logic=new Uint8Array(126);u32(logic,38,3);pstr(logic,42,'source');u32(logic,90,1);u32(logic,94,4);pstr(logic,110,'stand');
  const stub=logic.slice();u32(stub,38,6);pstr(stub,42,'stub');
  const pose=new Uint8Array(118);
  const shopMain=`code openshop()
global opened
if not propexists("copy")
opened = -100
endif
opened = opened + 1
endcode
code passed()
return me @ "/" @ target
endcode
code shared()
return me @ "/" @ target
endcode
code closeshop()
global closed
closed = closed + 1
closeshopfile("refshop")
endcode`;
  const member=`code openprop()
global propopened
propopened = propopened + 1
propinstance(me,"copy")
propowner(me,"opened")
endcode
code own(v)
propvalue(me,v)
return me @ "/" @ target @ "/" @ helper()
endcode
code helper()
return me
endcode
code passed()
passcode
endcode
code removeown()
propdelete(me)
return me
endcode
code closeown()
closeshopfile("codex.shp")
return me @ "/" @ target
endcode
code pulse()
global pulses
pulses = pulses + 1
propvalue(me,target)
if pulses < 3
makeloop("prop",me,"pulse",2)
endif
endcode
code idle()
global memberidles
memberidles = memberidles + 1
makeloop("prop",me,"idle",1)
forceupdate()
stoploop("prop",me)
endcode
code reenter()
global entered, maxentered
entered = entered + 1
if entered > maxentered
maxentered = entered
endif
makeloop("prop",me,"reenter",1)
forceupdate()
stoploop("prop",me)
entered = entered - 1
endcode`;
  const envelope=containers=>writeContainerFile({header:{fourCC:0x48454144,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),containers:containers.map((data,id)=>({id,data}))});
  fs.writeFileSync(`${dir}/CODEX.SHP`,envelope([head,compileScript(shopMain),logic,compileScript(member),pose,stub,new Uint8Array(8)]));
  const warmPose = frames => {
    const data = new Uint8Array(118 + frames.length * 44); u32(data,114,frames.length);
    frames.forEach((frame,i) => u32(data,118+i*44,frame)); return data;
  };
  const warmStub = stub.slice(); u32(warmStub,38,0); u32(warmStub,94,8);
  const art = color => new Uint8Array([2,0,2,0,0,0,0,0,2,0,10,color,2,0,10,color]);
  const warmParts = [head,compileScript(shopMain),logic,compileScript(member),warmPose([6,7]),warmStub,art(71),art(72),warmPose([9]),art(73)];
  fs.writeFileSync(`${dir}/WARM.SHP`,envelope(warmParts));
  const broken=warmParts.slice(); broken[7]=new Uint8Array([2,0,2,0,0,0,0,0]);
  fs.writeFileSync(`${dir}/BADWARM.SHP`,envelope(broken));
  fs.writeFileSync(`${dir}/warm.dfscript`,compileScript('code one()\npropwarm("SOURCE")\nreturn 12345\nendcode\ncode all()\nshopwarm("REFSHOP")\nreturn 12345\nendcode'));
  const bootHead=new Uint8Array(6);u32(bootHead,2,version);
  const boot=`code fallback()
return me @ "/" @ target
endcode
code idle()
global bootidles
bootidles = bootidles + 1
forceupdate()
forceupdate()
forceupdate()
endcode`;
  fs.writeFileSync(`${dir}/BOOTFILE`,envelope([bootHead,compileScript(boot),compileScript('code later(v)\nreturn me @ "/" @ target @ "/" @ v\nendcode')]));
  const checks=[];
  const check=(condition)=>checks.push(`if ${condition}\nreturn ${checks.length+1}\nendif`);
  checks.push('global opened, propopened, closed, curattention, pulses, entered, maxentered, memberidles, bootidles');
  check('not openshopfile("codex.shp")');
  check('opened != 1 | propopened != 1 | countshops() != 1 | countprops() != 3');
  check('propview("source") != "stand" | propxy("source",1) != 256 | propxy("source",2) != 192 | propis3d("source")');
  checks.push('openshopfile("CODEX.SHP")');check('opened != 2 | propopened != 1 | countprops() != 3');
  checks.push('startframe = frame()\nstarttick = tick()\nforceupdate()');
  check('frame() != startframe + 1 | tick() != starttick + 3');
  checks.push('makeloop("PROP","COPY","pulse",2)');check('not isloop("prop","copy") | countloops() != 1 | indextoloop(1) != "copy" | indextoloop(0) != ""');
  checks.push('pauseloop("prop","all")\nforceupdate()\nforceupdate()');check('pulses != 0');
  checks.push('pauseloop("prop","copy",false)\nforceupdate()');check('pulses != 0');
  checks.push('forceupdate()');check('pulses != 1 | propvalue("copy") != "copy" | not isloop("prop","copy")');
  checks.push('forceupdate()\nforceupdate()\nforceupdate()\nforceupdate()');check('pulses != 3 | countloops() != 0');
  checks.push('pulses = 0\nmakeloop("prop","copy","pulse",2)\nstarttick = tick()\ndelay(6)');
  check('pulses != 0 | tick() - starttick != 6');
  checks.push('forceupdate()');check('pulses != 1');
  checks.push('stoploop("prop","all")\nmakeloop("prop","copy","reenter",1)\nforceupdate()');
  check('maxentered != 1 | entered != 0 | countloops() != 0');
  checks.push('makeloop("prop","copy","IDLE",2)\nsendtoboot(idle())');
  check('memberidles != 1 | bootidles != 1 | countloops() != 0');
  check('sendtobootfx(later(me)) != "boot/prop-test/prop-test" | sendtobootfx(missing()) != 0');
  checks.push('propvalue("copy",0)\nframerate(100)');check('framerate() != 60');
  checks.push('framerate(-1)');check('framerate() != 0');
  checks.push('framerate(3)');
  check('propowner("source") != "opened" | propowner("copy") != "none" | propspeed("copy") != 0');
  check('sendtopropfx("copy",own(73)) != "copy/copy/copy" | propvalue("copy") != 73 | propvalue("source") != 0');
  check('sendtopropfx("copy",own(me)) != "copy/copy/copy" | propvalue("copy") != "prop-test"');
  check('sendtoprop("copy",passed()) != "copy/copy"');
  check('sendtoprop("copy",shared()) != "copy/copy"');
  check('sendtoprop("stub",shared()) != "codex.shp/stub"');
  check('sendtoshopfx("codex",shared()) != "codex.shp/prop-test"');
  check('sendtoprop("copy",fallback()) != "copy/copy"');
  check('sendtoprop("stub",fallback()) != "boot/stub"');
  checks.push('propinstance("copy","source")');check('propowner("source") != "opened"');
  checks.push('propvisible("copy",true)\nprophide("copy",true)');check('not propvisible("copy")');
  checks.push('propxyz("copy",-10,22,3)');check('propxyz("copy",1) != -10 | propxyz("copy",2) != 22 | propxyz("copy",3) != 3 | not propis3d("copy")');
  check('propxyz("copy",4) != makepoint(-10,22) | propxyz("copy",1) != -10');
  checks.push('propxy("copy",12,34)');check('propxy("copy",1) != 12 | propxy("copy",2) != 34 | propis3d("copy")');
  check('propxy("copy",3) != makepoint(12,34)');
  checks.push('propdeg("copy",258)\npropview("copy","WALK")\npropset("copy","TOWN")\npropscale("copy",1000)\npropspeed("copy",20)\npropzclip("copy",7)');
  check('propdeg("copy") != 258 | propview("copy") != "walk" | propset("copy") != "town" | propscale("copy") != 1000 | propspeed("copy") != 20 | propzclip("copy") != 7');
  check('indextoprop(1) != "source" | indextoprop(2) != "stub" | indextoprop(3) != "copy" | indextoshop(1) != "codex.shp" | indextoprop(0) != ""');
  check('sendtoprop("copy",removeown()) != "copy" | countprops() != 2 | propexists("copy")');
  checks.push('propvisible("source",true)\npropview("source","stand")\npropdeg("source",11)\npropdist("source",-50)\npropinstance("source","copy")');check('propvalue("copy") != 0 | propowner("copy") != "none" | not propvisible("copy") | propview("copy") != "stand" | propdeg("copy") != 11 | propdist("copy") != -50');
  checks.push('propdelete("source")');check('sendtoprop("source",helper()) != "source"');
  check('sendtoprop("copy",closeown()) != "copy/copy" | countshops() != 0 | countprops() != 0');
  checks.push('openshopfile("codex.shp")');check('opened != 3 | propopened != 2 | countprops() != 3');
  checks.push('closeshopfile("refshop")');check('closed != 2');
  check('openshopfile("missing.shp") | countshops() != 0 | countprops() != 0');
  const script=`code test()\n${checks.join('\n')}\nreturn 12345\nendcode\ncode own(v)\nreturn "wrong-local-call"\nendcode
code walltest()
before = tick()
delay(6)
if tick() - before < 6
return 1
endif
before = tick()
beforeframe = frame()
framerate(6)
forceupdate()
if tick() - before < 6 | frame() <= beforeframe
return 2
endif
return 12345
endcode`;
  const visibility=`
code visibility()
openshopfile("codex.shp")
propvisible("source",true)
propvisible("stub",false)
propvisible("copy",false)
prophide("all",true)
propvisible("copy",true)
return 12345
endcode
code release()
prophide("all",false)
prophide("stub",true)
return 12345
endcode
code preparestars()
propset("copy","town")
propstar("copy","TOWN.A")
propdeg("copy",9)
propstar("stub","placement-sentinel")
propstar("source","town.b")
propxyz("source",7,8,9)
propset("source","town")
return 12345
endcode
code placedstars()
if starxyz("TOWN.A") != -10 | starxyz("town.a",1) != -10 | starxyz("town.a",2) != 20 | starxyz("town.a",3) != 30
return 101
endif
if pointx(starxyz("town.a",4)) != -10 | pointy(starxyz("town.a",4)) != 20 | starxyz("town.a",0) != 0 | starxyz("missing",1) != 0
return 102
endif
if propstar("copy") != "town.a" | propxyz("copy",1) != -10 | propxyz("copy",2) != 20 | propxyz("copy",3) != 30 | propdeg("copy") != 9 | not propis3d("copy")
return 1
endif
if propxyz("source",1) != 7 | propxyz("source",2) != 8 | propxyz("source",3) != 9
return 2
endif
propstar("copy","TOWN.B")
if propstar("copy") != "town.b" | propxyz("copy",1) != 40 | propxyz("copy",2) != 50 | propxyz("copy",3) != 60 | propdeg("copy") != 224
return 3
endif
propscale("copy",777)
propstar("copy","town.a")
if propscale("copy") != 777
return 4
endif
propinstance("copy","fresh")
if propstar("fresh") != "" | propxyz("fresh",1) != 0 | propscale("fresh") != 0
return 5
endif
propstar("fresh","unseen.pending")
propxyz("fresh",4,5,6)
propstar("stub","plain-sentinel")
if propstar("missing") != "" | propstar("stub") != "plain-sentinel"
return 6
endif
return 12345
endcode
code rememberedstar()
propstar("copy","town.a")
if propxyz("copy",1) != -10 | propxyz("copy",2) != 20 | propxyz("copy",3) != 30 | propdeg("copy") != 64
return 1
endif
return 12345
endcode`;
  fs.writeFileSync(`${dir}/test.dfscript`,compileScript(script+visibility+'\ncode pointprobe(x,y)\nreturn pointinprop("probe",makepoint(x,y))\nendcode\n'));fs.writeFileSync(`${dir}/test.txt`,script+visibility+'\n');
}
