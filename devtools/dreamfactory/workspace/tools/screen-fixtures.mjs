// SPDX-License-Identifier: GPL-3.0-or-later
// Compiled palette and fade commands. No original game assets.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-screen');fs.mkdirSync(dest,{recursive:true});
for(const name of ['byte-order','binary','container','opcodes','text','script','script-asm']){
 const js=ts.transpileModule(fs.readFileSync(`references/dreamrefactory/engine/src/df/${name}.ts`,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
 fs.writeFileSync(`${dest}/${name}.mjs`,js.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
}
const {writeContainerFile}=await import(pathToFileURL(`${dest}/container.mjs`));
const {compileScript}=await import(pathToFileURL(`${dest}/script-asm.mjs`));
const script=`code source()
clut("SET")
endcode
code dim()
mixclut("set","black",1,2,128)
endcode
code fades()
global afterfade
clut("set")
screentoblack("CURRENT",6)
blacktoscreen("current",6)
afterfade = 1
return 12345
endcode
code dark()
screentoblack("set",3)
endcode
code stage()
clut("stage")
endcode
code black()
clut("black")
endcode
code clear()
blackscreen()
endcode
code canceltransition()
visualeffect(plain,0)
endcode
code animated()
visualeffect(venetian,12)
endcode
code transitions()
visualeffect(barndooropen,3)
visualeffect(barndoorclose,3)
visualeffect(wipeleft,3)
visualeffect(wiperight,3)
visualeffect(irisopen,3)
visualeffect(irisclose,3)
return 12345
endcode
code invalid()
blacktoscreen("missing",1)
endcode
code quitfade()
global afterquit
blacktoscreen("current",12)
afterquit = 1
endcode`;
for(const edition of ['demo','full','dust']){
 const dir=`build/screen-session-${edition}`;fs.mkdirSync(dir,{recursive:true});
 const boot=new Uint8Array(6);new DataView(boot.buffer).setUint32(2,edition==='dust'?1:4,true);
 fs.writeFileSync(dir+'/BOOTFILE',writeContainerFile({header:{fourCC:0x48454144,type:0,gapWhere:0},headerRaw:new Uint8Array(1024),containers:[boot,compileScript('code boot()\nendcode')].map((data,id)=>({id,data}))}));
 fs.writeFileSync(dir+'/test.dfscript',compileScript(script));
}
console.log('Synthetic palette/fade fixtures written');
