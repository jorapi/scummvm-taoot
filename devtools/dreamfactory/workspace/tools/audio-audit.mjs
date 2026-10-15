// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
const dest=path.resolve('build/reference-audio');
fs.mkdirSync(dest,{recursive:true});
for(const name of ['binary','byte-order','version','container','audio','banks','snd','opcodes','text','script','script-asm']) {
  const source=fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8');
  const js=ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
  fs.writeFileSync(path.join(dest,name+'.mjs'),js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {readContainerFile}=await import(pathToFileURL(path.join(dest,'container.mjs')));
const {readAudioBank:readReferenceBank}=await import(pathToFileURL(path.join(dest,'banks.mjs')));
function readAudioBank(file){
  const bank=readReferenceBank(file),h=file.containers[0].data,v=new DataView(h.buffer,h.byteOffset,h.byteLength);
  bank.soundNames=[...bank.singles.keys()];
  // Original Dust 403308..40335f copies the header+30 order, including
  // repeats, rather than assuming that every stored bed record plays once.
  if(v.getUint32(2,true)===1){
    const singles=v.getUint16(24,true),records=v.getUint16(26,true),length=v.getUint16(28,true);
    if(length>64)throw new Error('Invalid Dust bank theme order');
    bank.loopChunks=Array.from({length},(_,i)=>v.getUint16(30+i*2,true)).filter(n=>n>0&&n<=records).map(n=>singles+n);
    // 403d40 reads only header+24 one-shots; theme records are not effects.
    bank.singles=new Map();bank.soundNames=[];
    for(let i=0;i<singles;i++){
      const at=186+i*24,name=Buffer.from(h.subarray(at+1,at+1+h[at])).toString('latin1').replace(/^.*\//,'').toLowerCase();
      bank.soundNames.push(name);
      if(!bank.singles.has(name))bank.singles.set(name,{identifier:name,containerLoc:i+1,follow:''});
    }
  }
  return bank;
}
const {decodeAudioContainer,resampleTo}=await import(pathToFileURL(path.join(dest,'audio.mjs')));
const {compileScript}=await import(pathToFileURL(path.join(dest,'script-asm.mjs')));
class Hash {
  hash=2166136261;
  byte(n){this.hash=Math.imul(this.hash^(n&255),16777619)>>>0;}
  number(n){for(let i=0;i<4;++i)this.byte(n>>>(8*i));}
  text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
  hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length ? process.argv.slice(2) : ['demo','full','dust']) {
  let banks=0;
  for(const name of corpusFiles(edition).filter(n=>/\.(trk|sfx|11k|snd)$/i.test(n))) {
    const file=readContainerFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`)));
    const bank=readAudioBank(file),meta=new Hash(),pcm=new Hash(),theme=new Hash();
    meta.number(new DataView(file.containers[0].data.buffer,file.containers[0].data.byteOffset).getUint32(2,true));meta.text(bank.trackName);
    meta.number(bank.loopChunks.length);
    for(const c of bank.loopChunks)meta.number(c);
    meta.number(bank.soundNames.length);
    for(const id of bank.soundNames){meta.text(id);meta.number(bank.singles.get(id).containerLoc);}
    const used=[...new Set([...bank.loopChunks,...[...bank.singles.values()].map(c=>c.containerLoc)])].sort((a,b)=>a-b);
    let samples=0;
    for(const id of used) {
      const clip=decodeAudioContainer(file.containers[id].data);
      pcm.number(id);pcm.number(clip.sampleRate);pcm.number(clip.samples.length);
      for(const v of clip.samples){const n=Math.round(v*32768);pcm.byte(n);pcm.byte(n>>8);}
      samples+=clip.samples.length;
    }
    if(bank.loopChunks.length) {
      const parts=bank.loopChunks.map(id=>decodeAudioContainer(file.containers[id].data));
      const rate=Math.max(...parts.map(c=>c.sampleRate));
      const resampled=parts.map(c=>resampleTo(c.samples,c.sampleRate,rate));
      theme.number(rate);theme.number(resampled.reduce((n,c)=>n+c.length,0));
      for(const part of resampled)for(const v of part){const n=Math.round(v*32768);theme.byte(n);theme.byte(n>>8);}
    }
    console.log(`AUDIO ${name} loops=${bank.loopChunks.length} sounds=${bank.soundNames.length} chunks=${used.length} samples=${samples} meta=${meta.hex()} pcm=${pcm.hex()} theme=${theme.hex()}`);
    ++banks;
  }
  console.log(`AUDIO PASS banks=${banks} failures=0`);
  const manifest=JSON.parse(fs.readFileSync(`game-data/${edition}/resource-index.json`,'utf8'));
  const themeFile=edition==='dust'?'TOWN.SND':edition==='full'?'BEDRAD1.TRK':'DEMO.TRK';
  const themeBank=readAudioBank(readContainerFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${manifest[themeFile][0].path}`))));
  const library=edition==='dust'?'UNILIB.SND':'UNILIB.TRK';
  const libraryBank=readAudioBank(readContainerFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${manifest[library][0].path}`))));
  const prefix=edition==='dust'?'dust:data:':edition==='full'?'titanic1:data:':'3pacdemo:titanic:data:';
  const q=JSON.stringify;
  let aliasCase='';
  for(const [filename,variants] of Object.entries(manifest).sort()) {
    if(!/\.(snd|trk|11k|sfx)$/i.test(filename))continue;
    const source=variants[0].path;
    const bank=readAudioBank(readContainerFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${source}`))));
    if(!bank.loopChunks.length || bank.trackName.toLowerCase()===filename.toLowerCase())continue;
    const logical=source.replace(/^volumes\//,'').replaceAll('/',':');
    aliasCase=`opentrackfile(${q(logical)})
playtheme(${q(bank.trackName)})
if currenttheme() != ${q(bank.trackName)}
return 21
endif
closetrackfile(${q(bank.trackName)})
if currenttheme() != "none" | counttracks() != 0
return 22
endif`;
    break;
  }
  // Original retail 425150/240/330/400: two replaceable SFX slots. Hold
  // clips in loops so assertions cannot race natural PCM completion.
  const slotCases=edition!=='full'?'':`
local secondclip, thirdclip, repeats
secondclip = indextosound(${q(library)},2)
thirdclip = indextosound(${q(library)},3)
soundloop(clipname,true)
soundloop(secondclip,true)
soundloop(thirdclip,true)
singlesound(clipname)
if currentsound(1) != "none" | currentsound(2) != clipname
return 30
endif
singlesound(clipname)
singlesound(secondclip)
if currentsound(1) != secondclip | currentsound(2) != clipname
return 31
endif
singlesound(clipname)
singlesound(thirdclip)
singlesound(clipname)
if currentsound(1) != secondclip | currentsound(2) != thirdclip
return 32
endif
multiplesound(secondclip)
multiplesound(thirdclip)
multiplesound(clipname)
if currentsound(1) != secondclip | currentsound(2) != thirdclip
return 33
endif
dualsound(secondclip)
if currentsound(1) != secondclip | currentsound(2) != thirdclip
return 34
endif
dualsound(thirdclip)
if currentsound(1) != thirdclip | currentsound(2) != thirdclip
return 35
endif
bothsound(clipname)
if currentsound(1) != clipname | currentsound(2) != clipname
return 36
endif
dualsound(secondclip)
if currentsound(1) != secondclip | currentsound(2) != secondclip
return 37
endif
repeats = 0
while repeats < 100
multiplesound(thirdclip)
repeats = repeats + 1
endwhile
if currentsound(1) != secondclip | currentsound(2) != thirdclip
return 38
endif
haltsound()
soundloop(clipname,false)
soundloop(secondclip,false)
soundloop(thirdclip,false)
`;
  // Execute these calls in the real native session/mixer, including yielding
  // voice completion and closing a bank that owns a still-looping theme.
  const script=`code test()
path(1,path(0) @ "local:")
path(2,${q(prefix)})
opentrackfile(${q(library)})
if counttracks() != 1 | indextotrack(1) != ${q(libraryBank.trackName)}
return 23
endif
local clipname
clipname = indextosound(${q(library)},1)
if clipname = ""
return 10
endif
soundvol(clipname,180)
soundpan(clipname,64)
wavevolume(4)
if wavevolume() != 4 | soundvol(clipname) != 180 | soundpan(clipname) != 64
return 20
endif
wavevolume(9)
voicesound(clipname)
while not voicedone()
endwhile
if currentvoice() != "none"
return 11
endif
soundloop(clipname,true)
multiplesound(clipname)
voicesound(clipname)
if currentsound(2) != clipname
return 12
endif
soundloop(clipname,false)
while not sounddone() | not voicedone()
endwhile
singlesound(clipname)
haltsound()
if currentsound(1) != "none"
return 14
endif
${slotCases}
closetrackfile(${q(library)})
if countsounds(${q(library)}) != 0
return 15
endif
opentrackfile(${q(themeFile)})
themevol(${q(themeBank.trackName)},123)
playtheme(${q(themeBank.trackName)})
if currenttheme() != ${q(themeBank.trackName)}
return 16
endif
if themevol(${q(themeBank.trackName)}) != 123
return 17
endif
halttheme()
if currenttheme() != "none"
return 18
endif
playtheme(${q(themeBank.trackName)})
closetrackfile(${q(themeBank.trackName)})
if currenttheme() != "none"
return 19
endif
${aliasCase}
opentrackfile(${q(library)})
opentrackfile(${q(library)})
if counttracks() != 2 | indextotrack(1) != ${q(libraryBank.trackName)} | indextotrack(2) != ${q(libraryBank.trackName)}
return 24
endif
closetrackfile(${q(library)})
if counttracks() != 1 | countsounds(${q(library)}) != ${libraryBank.soundNames.length}
return 25
endif
closetrackfile(${q(library)})
if counttracks() != 0
return 26
endif
return 12345
endcode`;
  fs.writeFileSync(`build/audio-tests-${edition}.dfscript`,compileScript(script));
  fs.writeFileSync(`build/audio-tests-${edition}.txt`,script+'\n');
}
