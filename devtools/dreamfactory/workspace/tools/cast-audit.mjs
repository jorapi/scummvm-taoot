// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-cast');
for(const dir of ['df','runtime']) {
 fs.mkdirSync(`${dest}/${dir}`,{recursive:true});
 for(const file of fs.readdirSync(`references/dreamrefactory/engine/src/${dir}`).filter(n=>n.endsWith('.ts'))) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/${dir}/${file}`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(`${dest}/${dir}/${file.replace(/\.ts$/,'.mjs')}`,js.replace(/from (["'])(\.{1,2}\/[^"']+)\1/g,'from $1$2.mjs$1'));
 }
}
const {readCstFile}=await import(pathToFileURL(`${dest}/df/cst.mjs`));
const {decodeShpFrame}=await import(pathToFileURL(`${dest}/df/shp.mjs`));
const {writeContainerFile}=await import(pathToFileURL(`${dest}/df/container.mjs`));
const {compileScript}=await import(pathToFileURL(`${dest}/df/script-asm.mjs`));
const {LoadedCast,ActorInstance,ActorRuntime}=await import(pathToFileURL(`${dest}/runtime/actors.mjs`));
class Hash {
  hash=2166136261;
  byte(b){this.hash=Math.imul(this.hash^b,16777619)>>>0;}
  number(n){for(let i=0;i<4;++i)this.byte((n>>>(8*i))&255);}
  text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
  hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length ? process.argv.slice(2) : ['demo','full','dust']) {
  let count=0;
  for(const name of corpusFiles(edition).filter(n=>/\.cst$/i.test(n))) {
    const cast=readCstFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`)));
    const head=cast.file.containers[0].data;
    const version=new DataView(head.buffer,head.byteOffset,head.byteLength).getUint32(2,true);
    const meta=new Hash(),pixels=new Hash(),selection=new Hash(),art=new Hash();let poses=0,frames=0,pictures=0;
    const loaded=new LoadedCast(name,cast),runtime=new ActorRuntime(),palette=new Uint8ClampedArray(1024);
    for(let i=0;i<256;++i){for(let c=0;c<3;++c)palette[i*4+c]=cast.paletteRaw[i*8+3+c*2];palette[i*4+3]=255;}
    meta.number(version);meta.number(cast.mainScriptLocation);
    for(let i=0;i<256;++i)for(let c=0;c<3;++c)meta.byte(cast.paletteRaw[i*8+3+c*2]);
    for(const member of cast.members) {
      meta.text(member.name);meta.number(member.logicLocation);meta.number(member.scriptLocation);meta.number(member.poses.length);
      for(const pose of member.poses) {
        ++poses;meta.text(pose.name);
        for(const n of [pose.location,pose.record,pose.frameCount,pose.steps.length])meta.number(n);
        for(const step of pose.steps) {
          meta.number(step.length);
          for(const frame of step) {
            ++frames;
            for(const n of [frame.location,frame.record,frame.direction,frame.angle,frame.refScale])meta.number(n);
            const sprite=decodeShpFrame(cast.file.containers[frame.location].data);
            for(const n of [sprite.width,sprite.height,sprite.posYraw,sprite.posXraw])pixels.number(n);
            for(const b of sprite.indexed)pixels.byte(b);
            for(const b of sprite.opaque)pixels.byte(b);
          }
        }
        meta.number(pose.play.length);for(const s of pose.play)meta.number(s);
        const actor=new ActorInstance(member,loaded);actor.poseName=pose.name;actor.worldX=160;actor.worldY=20;
        const probe={x:0,y:0,z:0,deg:0,f:48,cx:48,cy:36,clipW:96,clipH:72};
        const locations=new Map();for(const step of pose.steps)for(const f of step)locations.set(loaded.frame(f.location),f.location);
        for(let step=0;step<Math.max(1,pose.play.length);++step)for(let degree=0;degree<256;++degree){actor.step=step;actor.deg=degree;const frame=runtime.frameFor(actor,probe);selection.number(frame?locations.get(frame):0);selection.number(runtime.refScale(actor));}
        actor.visible=true;actor.scale=1500;actor.deg=73;
        for(const step of [0,Math.floor(pose.play.length/2),Math.max(0,pose.play.length-1)])for(const heading of [0,37,128,255]) {
          actor.step=step;const angle=heading*Math.PI/128;
          actor.worldX=13+Math.round(160*Math.cos(angle))+Math.round(20*Math.sin(angle));actor.worldY=-27+Math.round(160*Math.sin(angle))-Math.round(20*Math.cos(angle));actor.worldZ=35;
          const cam={x:13,y:-27,z:45,deg:heading,f:48,cx:48,cy:36,clipW:96,clipH:72};
          const z=Uint8Array.from({length:96*72},(_,i)=>(Math.floor(i/96)*7+(i%96)*3)%16),occ={z,w:96,h:72,scale:32,levels:16,groundBias:edition==='dust'?64:0};
          runtime.actors.clear();runtime.actors.set('probe',actor);
          const rgba=new Uint8ClampedArray(96*72*4);for(let i=0;i<rgba.length;i+=4)rgba.set([19,31,47,255],i);
          runtime.composite(rgba,96,72,palette,cam,occ);for(const b of rgba)art.byte(b);++pictures;
        }

        actor.worldSpace=false;actor.scale=0;
        for(const step of [0,Math.max(0,pose.play.length-1)])for(const degree of [0,73]) {
          actor.step=step;actor.deg=degree;const f=runtime.frameFor(actor,null);actor.anchorX=(f?.posXraw??0)-3;actor.anchorY=(f?.posYraw??0)+2;
          runtime.actors.clear();runtime.actors.set('probe',actor);
          const rgba=new Uint8ClampedArray(96*72*4);for(let i=0;i<rgba.length;i+=4)rgba.set([19,31,47,255],i);
          runtime.compositeScreen(rgba,96,72,palette);for(const b of rgba)art.byte(b);++pictures;
        }

      }
    }
    console.log(`ACTORART ${name} pictures=${pictures} selection=${selection.hex()} rgba=${art.hex()}`);
    console.log(`CAST ${name} members=${cast.members.length} poses=${poses} frames=${frames} meta=${meta.hex()} pixels=${pixels.hex()}`);++count;
  }
  console.log(`CAST PASS files=${count} failures=0`);
  writeSessionFixture(edition);
}
function writeSessionFixture(edition) {
  const version=edition==='dust'?1:4;
  const dir=`build/cast-session-${edition}`;fs.mkdirSync(dir,{recursive:true});
  const u32=(data,at,n)=>new DataView(data.buffer).setUint32(at,n,true);
  const pstr=(data,at,s)=>{data[at]=s.length;for(let i=0;i<s.length;++i)data[at+1+i]=s.charCodeAt(i);};
  const head=new Uint8Array(2396);u32(head,2,version);u32(head,2340,1);u32(head,2360,2);u32(head,2364,2);u32(head,2380,5);
  const logic=new Uint8Array(126);u32(logic,38,3);pstr(logic,42,'source');u32(logic,90,1);u32(logic,94,4);pstr(logic,110,'stand');
  const stub=logic.slice();u32(stub,38,6);pstr(stub,42,'stub');
  const pose=new Uint8Array(118);
  const castMain=`code opencast()
global opened
opened = opened + 1
endcode
code passed()
return me @ "/" @ target
endcode
code shared()
return me @ "/" @ target
endcode`;
  const member=`code openactor()
global actoropened
actoropened = actoropened + 1
actorinstance(me,"copy")
actorowner(me,"opened")
endcode
code own(v)
actorvalue(me,v)
return me @ "/" @ target @ "/" @ helper()
endcode
code helper()
return me
endcode
code passed()
passcode
endcode
code removeown()
actordelete(me)
return me
endcode
code closeown()
closecastfile("codex.cst")
return me @ "/" @ target
endcode
code pulse()
global pulses
pulses = pulses + 1
actorvalue(me,target)
if pulses < 3
makeloop("actor",me,"pulse",2)
endif
endcode
code endturn()
global motionturns, motionturnonly
motionturns = motionturns + 1
if iswalk(me)
actorpose(me,"walk")
else
motionturnonly = motionturnonly + 1
endif
endcode
code endwalk()
global motionarrivals, motionrestart, motioncallbackwait
local waitstep
motionarrivals = motionarrivals + 1
if motionrestart
motionrestart = false
walktoxyz(me,-10,0,0)
for waitstep = 1 to 64
if iswalk(me)
forceupdate()
endif
endfor
motioncallbackwait = iswalk(me)
endif
endcode
code reenter()
global entered, maxentered
entered = entered + 1
if entered > maxentered
maxentered = entered
endif
makeloop("actor",me,"reenter",1)
forceupdate()
stoploop("actor",me)
entered = entered - 1
endcode`;
  const envelope=containers=>writeContainerFile({header:{fourCC:0x48454144,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),containers:containers.map((data,id)=>({id,data}))});
  fs.writeFileSync(`${dir}/CODEX.CST`,envelope([head,compileScript(castMain),logic,compileScript(member),pose,stub,new Uint8Array(8)]));
  const warmPose = (frames) => {
    const data = new Uint8Array(118 + frames.length*44); u32(data,114,frames.length);
    frames.forEach((frame,i)=>u32(data,118+i*44,frame)); return data;
  };
  const warmStub=stub.slice();u32(warmStub,38,0);u32(warmStub,94,8);
  const art=color=>new Uint8Array([2,0,2,0,0,0,0,0,2,0,10,color,2,0,10,color]);
  const warmParts=[head,compileScript(castMain),logic,compileScript(member),warmPose([6,7]),warmStub,art(61),art(62),warmPose([9]),art(63)];
  fs.writeFileSync(`${dir}/WARM.CST`,envelope(warmParts));
  const broken=warmParts.slice();broken[7]=new Uint8Array(1);fs.writeFileSync(`${dir}/BADWARM.CST`,envelope(broken));
  fs.writeFileSync(`${dir}/warm.dfscript`,compileScript('code one()\nactorwarm("SOURCE")\nreturn 12345\nendcode\ncode all()\ncastwarm("WARM")\nreturn 12345\nendcode'));
  const v1=version===1, starHead=new Uint8Array(v1?36:96), first=v1?28:8, primary=v1?0:4, secondary=v1?26:30;
  u32(starHead,2,version);if(v1)new DataView(starHead.buffer).setUint16(0x22,1,true);else u32(starHead,0x58,1);
  const starReg=new Uint8Array(first+(v1?50:54));u32(starReg,v1?24:0,1);
  const u16=(d,a,n)=>new DataView(d.buffer).setInt16(a,n,true);
  for(const [o,xyz,deg,name] of [[first+primary,[-10,20,30],64,'town.a'],[first+secondary,[40,50,60],224,'town.b']]){u16(starReg,o,deg);xyz.forEach((n,i)=>u16(starReg,o+2+i*2,n));pstr(starReg,o+8,name);}
  u16(starReg,first+(v1?24:28),2);
  const route=new Uint8Array((v1?16:20)+24);u32(route,v1?0:8,3);
  for(const [i,point] of [[0,[-10,20,30,0]],[1,[-10,50,30,30]],[2,[40,50,60,58]]])point.forEach((n,j)=>u16(route,(v1?16:20)+i*8+j*2,n));
  fs.writeFileSync(`${dir}/stars.set`,envelope([starHead,starReg,route]));
  const bootHead=new Uint8Array(6);u32(bootHead,2,version);
  const boot=`code fallback()
return me @ "/" @ target
endcode`;
  fs.writeFileSync(`${dir}/BOOTFILE`,envelope([bootHead,compileScript(boot)]));
  const checks=[];
  const check=(condition)=>checks.push(`if ${condition}\nreturn ${checks.length+1}\nendif`);
  checks.push('global opened, actoropened, curattention, pulses, entered, maxentered');
  // The original v1/v4 integer square root truncates; the reference rounds.
  check('calcdist(makepoint(0,0),makepoint(2,2)) != 2 | calcdist(makepoint(-3,-4),makepoint(0,0)) != 5');
  check('calcdist(makepoint(-32768,0),makepoint(32767,0)) != 65535 | calcdist(makepoint(-32768,-32768),makepoint(32767,32767)) != 65533');
  check('calcdist(makepoint(-28,138),makepoint(384,128)) != 412');
  check('not opencastfile("codex.cst")');
  check('opened != 1 | actoropened != 1 | countcasts() != 1 | countactors() != 3');
  checks.push('opencastfile("CODEX.CST")');check('opened != 1 | actoropened != 1 | countactors() != 3');
  if(version===1){
    // The shared opcode is called currentcd in the assembler's v4 name table.
    check('currentcd("player",1) != 0 | currentcd("source",2) != 0');
    checks.push('currentcd("PLAYER",100,50)\ncurrentcd("SOURCE",1,0)');
    check('currentcd("player",1) != 100 | currentcd("player",2) != 50 | currentcd("source",1) != 1 | currentcd("source",2) != 0');
    checks.push('currentcd("copy",65535,32768)');
    check('currentcd("copy",1) != -1 | currentcd("copy",2) != -32768 | currentcd("source",1) != 1');
    checks.push('currentcd("source",0,0)\ncurrentcd("player",0,0)');
  } else {
    check('currentcd() != "" | camerahi() != 0');
    checks.push('camerahi(-96)');check('camerahi() != -96');
    checks.push('camerahi(65535)');check('camerahi() != -1');
    checks.push('camerahi(0)');
  }
  checks.push('startframe = frame()\nstarttick = tick()\nforceupdate()');
  check('frame() != startframe + 1 | tick() != starttick + 3');
  checks.push('makeloop("ACTOR","COPY","pulse",2)');check('not isloop("actor","copy") | countloops() != 1 | indextoloop(1) != "copy" | indextoloop(0) != ""');
  checks.push('pauseloop("actor","all")\nforceupdate()\nforceupdate()');check('pulses != 0');
  checks.push('pauseloop("actor","copy",false)\nforceupdate()');check('pulses != 0');
  checks.push('forceupdate()');check('pulses != 1 | actorvalue("copy") != "copy" | not isloop("actor","copy")');
  checks.push('forceupdate()\nforceupdate()\nforceupdate()\nforceupdate()');check('pulses != 3 | countloops() != 0');
  checks.push('pulses = 0\nmakeloop("actor","copy","pulse",2)\nstarttick = tick()\ndelay(6)');
  check('pulses != 0 | tick() - starttick != 6');
  checks.push('forceupdate()');check('pulses != 1');
  checks.push('stoploop("actor","all")\nmakeloop("actor","copy","reenter",1)\nforceupdate()');
  check('maxentered != 1 | entered != 0 | countloops() != 0');
  checks.push('actorvalue("copy",0)\nframerate(100)');check('framerate() != 60');
  checks.push('framerate(-1)');check('framerate() != 0');
  checks.push('framerate(3)');
  check('actorowner("source") != "opened" | actorowner("copy") != "none" | actorturn("copy") != 16');
  check('sendtoactorfx("copy",own(73)) != "copy/copy/copy" | actorvalue("copy") != 73 | actorvalue("source") != 0');
  check('sendtoactorfx("copy",own(me)) != "copy/copy/copy" | actorvalue("copy") != "actor-test"');
  check('sendtoactor("copy",passed()) != "copy/copy"');
  check('sendtoactor("copy",shared()) != "copy/copy"');
  check('sendtoactor("stub",shared()) != "codex.cst/stub"');
  check('sendtocastfx("codex",shared()) != "codex.cst/actor-test"');
  check('sendtoactor("copy",fallback()) != "copy/copy"');
  check('sendtoactor("stub",fallback()) != "boot/stub"');
  checks.push('actorinstance("copy","source")');check('actorowner("source") != "opened"');
  checks.push('actorvisible("copy",true)\ncurattention = "copy"\nactorhide("copy")');check('actorvisible("copy") | curattention != ""');
  checks.push('actorxyz("copy",-10,22,3)');check('actorxyz("copy",1) != -10 | actorxyz("copy",2) != 22 | actorxyz("copy",3) != 3 | not actoris3d("copy")');
  checks.push('actorxy("copy",12,34)');check('actorxy("copy",1) != 12 | actorxy("copy",2) != 34 | actoris3d("copy")');
  checks.push('actordeg("copy",258)\nactorpose("copy","WALK")\nactorset("copy","TOWN")\nactorscale("copy",1000)\nactorspeed("copy",20)\nactorzclip("copy",7)');
  check('actordeg("copy") != 2 | actorpose("copy") != "walk" | actorset("copy") != "town" | actorscale("copy") != 1000 | actorspeed("copy") != 20 | actorzclip("copy") != 7');
  check('indextoactor(1) != "source" | indextoactor(2) != "stub" | indextoactor(3) != "copy" | indextocast(1) != "codex.cst" | indextoactor(0) != ""');
  check('sendtoactor("copy",removeown()) != "copy" | countactors() != 2 | actorexists("copy")');
  checks.push('actorinstance("source","copy")');check('actorvalue("copy") != 0 | actorowner("copy") != "none"');
  if(version===1)check('currentcd("copy",1) != 0 | currentcd("copy",2) != 0');
  checks.push('actordelete("source")');check('sendtoactor("source",helper()) != "source"');
  check('sendtoactor("copy",closeown()) != "copy/copy" | countcasts() != 0 | countactors() != 0');
  checks.push('opencastfile("codex.cst")');check('opened != 2 | actoropened != 2 | countactors() != 3');
  checks.push('closecastfile("codex.cst")');
  check('opencastfile("missing.cst") | countcasts() != 0 | countactors() != 0');
  const script=`code test()\n${checks.join('\n')}\nreturn 12345\nendcode\ncode own(v)\nreturn "wrong-local-call"\nendcode
code invalidhitaxis()
return currentcd("player",3)
endcode
code missinghitactor()
return currentcd("missing",1)
endcode
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
endcode
code motiontest()
global motionturns, motionarrivals, motionrestart, motioncallbackwait, motionturnonly
opencastfile("codex.cst")
actorxyz("copy",0,0,0)
actordeg("copy",0)
actorturn("copy",16)
actorspeed("copy",10)
walktoxyz("copy",40,0,0)
if not iswalk("copy") | countwalks() != 1 | indextowalk(1) != "copy" | walkdest("copy") != "custom"
return 1
endif
forceupdate()
if actorxyz("copy",1) != 0 | motionturns != 1 | actorpose("copy") != "walk"
return 2
endif
pausewalk("copy")
forceupdate()
forceupdate()
if actorxyz("copy",1) != 0 | not iswalk("copy")
return 3
endif
pausewalk("copy",false)
delay(6)
if actorxyz("copy",1) != 20 | motionarrivals != 0
return 4
endif
${v1 ? `turntodeg("copy",128)
if countwalks() != 1 | walkdest("copy") != "walktoxyz"
return 19
endif
while iswalk("copy")
forceupdate()
endwhile
if actorxyz("copy",1) != 20 | motionarrivals != 0 | motionturns != 2 | motionturnonly != 1
return 20
endif
walktoxyz("copy",40,0,0)
while iswalk("copy")
forceupdate()
endwhile
if motionturns != 3 | motionarrivals != 1
return 21
endif
motionturns = 1
motionturnonly = 0` : `turntodeg("copy",128)
if countwalks() != 1 | walkdest("copy") != "custom" | actordeg("copy") != 128
return 16
endif`}
while iswalk("copy")
forceupdate()
endwhile
if actorxyz("copy",1) != 40 | motionarrivals != 1 | actorpose("copy") != "stand" | walkdest("copy") != "None"
return 5
endif
actordeg("copy",0)
turntodeg("copy",128)
if not iswalk("copy")
return 6
endif
forceupdate()
if actordeg("copy") != 240 | actorxyz("copy",1) != 40
return 7
endif
while iswalk("copy")
forceupdate()
endwhile
if actordeg("copy") != 128 | motionarrivals != 1 | motionturns != ${v1 ? 2 : 1} | motionturnonly != ${v1 ? 1 : 0}
return 8
endif
turntodeg("copy",128)
if ${v1 ? 'not iswalk("copy")' : 'iswalk("copy")'}
return 9
endif
forceupdate()
if iswalk("copy") | motionturnonly != ${v1 ? 2 : 0} | motionarrivals != 1
return 18
endif
walktoxyz("copy",100,0,0)
forceupdate()
stopwalk("COPY")
if iswalk("copy") | countwalks() != 0 | motionarrivals != 1
return 10
endif
actordeg("copy",0)
actorxyz("copy",0,0,0)
motionrestart = true
walktoxyz("copy",10,0,0)
while iswalk("copy")
forceupdate()
endwhile
if actorxyz("copy",1) != -10 | motionarrivals != 3 | motionturns != ${v1 ? 5 : 3}
return 11
endif
if motioncallbackwait
return 17
endif
walktoxyz("copy",100,0,0)
walktoxyz("copy",10,0,0)
if countwalks() != 1
return 12
endif
stopwalk("copy")
walktoxyz("missing",1,2,3)
turntodeg("missing",32)
if countwalks() != 0 | iswalk("missing") | indextowalk(1) != ""
return 13
endif
walktoxyz("copy",100,0,0)
actordelete("copy")
if countwalks() != 0 | iswalk("copy")
return 14
endif
actorinstance("source","copy")
walktoxyz("copy",100,0,0)
closecastfile("codex.cst")
if countwalks() != 0
return 15
endif
opencastfile("codex.cst")
for n = 1 to 17
who = "walker" @ numtostring(n)
actorinstance("source",who)
walktoxyz(who,100,0,0)
endfor
if countwalks() != 16 | indextowalk(17) != "" | iswalk("walker17")
return 17
endif
closecastfile("codex.cst")
opencastfile("codex.cst")
walktoxyz("copy",100,0,0)
stopwalk("copy")
return 12345
endcode`;
  const starScript=`
code preparestars()
actorxyz("copy",1,2,3)
actordeg("copy",9)
actorset("copy","town")
actorstar("copy","town.a")
if actorstar("copy") != "town.a" | actorxyz("copy",1) != 1
return 1
endif
return 12345
endcode
code starstest()
if actorxyz("copy",1) != -10 | actorxyz("copy",2) != 20 | actorxyz("copy",3) != 30 | actordeg("copy") != 9
return 1
endif
actorxy("copy",1,2)
actorstar("copy","TOWN.B")
if actorxyz("copy",1) != 40 | actorxyz("copy",2) != 50 | actorxyz("copy",3) != 60 | actordeg("copy") != 224 | not actoris3d("copy")
return 2
endif
actorstar("copy","town.a")
actorspeed("copy",10)
actorturn("copy",128)
walktostar("copy","town.b")
if actorstar("copy") != "${version===1?'walkonpath':'defer'}" | walkdest("copy") != "town.b" | not iswalk("copy")
return 3
endif
while iswalk("copy")
forceupdate()
endwhile
if actorstar("copy") != "town.b" | actorxyz("copy",1) != 40 | actorxyz("copy",2) != 50 | actorxyz("copy",3) != 60
return 4
endif
walktostar("copy","-8, -9,2")
while iswalk("copy")
forceupdate()
endwhile
if actorstar("copy") != "custom" | actorxyz("copy",1) != -8 | actorxyz("copy",2) != -9 | actorxyz("copy",3) != 2
return 5
endif
walktostar("copy","notfound")
if iswalk("copy") | actorstar("copy") != "custom"
return 6
endif
actorstar("copy","unseen.pending")
actorxyz("copy",4,5,6)
return 12345
endcode
code pathtest()
actorstar("copy","town.a")
actorspeed("copy",10)
actorturn("copy",128)
walkonpath("copy","town.a","town.b")
if actorstar("copy") != "walkonpath" | not iswalk("copy")
return 1
endif
forceupdate()
forceupdate()
forceupdate()
forceupdate()
if actorxyz("copy",1) != -10 | actorxyz("copy",2) != 50 | actorxyz("copy",3) != 30
return 2
endif
while iswalk("copy")
forceupdate()
endwhile
if actorstar("copy") != "town.b"
return 3
endif
walkonpath("copy","town.b","town.a")
forceupdate()
forceupdate()
forceupdate()
forceupdate()
if actorxyz("copy",1) != 14 | actorxyz("copy",2) != 50 | actorxyz("copy",3) != 44
return 4
endif
stopwalk("copy")
actorxyz("copy",-10,35,30)
walkonpath("copy","resume","town.b")
forceupdate()
forceupdate()
if actorxyz("copy",1) != -10 | actorxyz("copy",2) != 45 | actorxyz("copy",3) != 30
return 5
endif
while iswalk("copy")
forceupdate()
endwhile
if actorstar("copy") != "town.b" | actorxyz("copy",1) != 40 | actorxyz("copy",2) != 50 | actorxyz("copy",3) != 60
return 6
endif
return 12345
endcode
code resumelast()
actorxyz("copy",39,50,60)
walkonpath("copy","resume","town.b")
forceupdate()
forceupdate()
if iswalk("copy") | actorxyz("copy",1) != 40 | actorxyz("copy",2) != 50 | actorxyz("copy",3) != 60
return 1
endif
return 12345
endcode
code starhistory()
actorstar("copy","town.a")
if actorxyz("copy",1) != -10 | actorxyz("copy",2) != 20 | actorxyz("copy",3) != 30 | actordeg("copy") != 64
return 1
endif
return 12345
endcode`;
  fs.writeFileSync(`${dir}/test.dfscript`,compileScript(script+starScript+`
code orderprobe()
opencastfile("codex.cst")
actordist("source",-2)
return actordist("source") = 32000 & actordist("missing") = 32000
endcode
`));fs.writeFileSync(`${dir}/test.txt`,script+'\n');
}
