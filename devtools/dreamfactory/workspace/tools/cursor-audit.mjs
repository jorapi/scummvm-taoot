// SPDX-License-Identifier: GPL-3.0-or-later
// Compare ScummVM's executable resource reader with the independent PE reader.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {execFileSync} from 'node:child_process';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const dest=path.resolve('build/reference-cursor');fs.mkdirSync(dest,{recursive:true});
const transpile=(file,out)=>fs.writeFileSync(out,ts.transpileModule(fs.readFileSync(file,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText.replace(/from (["'])(\.\/[^"']+)\1/g,'from $1$2.mjs$1'));
transpile('references/dreamrefactory/tools/dumpcursors.ts',`${dest}/dump.mjs`);
transpile('references/dreamrefactory/engine/src/web/cursors.ts',`${dest}/pixels.mjs`);
for(const name of ['byte-order','binary','container','opcodes','text','script','script-asm'])transpile(`references/dreamrefactory/engine/src/df/${name}.ts`,`${dest}/${name}.mjs`);
const {cursorPixels}=await import(pathToFileURL(`${dest}/pixels.mjs`));
const {compileScript}=await import(pathToFileURL(`${dest}/script-asm.mjs`));
for(const edition of process.argv.slice(2).length?process.argv.slice(2):['demo','full','dust']){
 const exe=`game-data/${edition}/installed/${edition==='dust'?'DF':'TI'}.EXE`, out=`${dest}/${edition}.mjs`;
 execFileSync(process.execPath,[`${dest}/dump.mjs`,exe,'--out',`${dest}/${edition}.ts`,'--const','ART']);
 transpile(`${dest}/${edition}.ts`,out);
 const {ART}=await import(pathToFileURL(out));
 for(const [name,art] of Object.entries(ART)){
  const p=cursorPixels(art);let hash=2166136261;
  for(const b of p.rgba)hash=Math.imul(hash^b,16777619)>>>0;
  console.log(`CURSOR ${name} width=${p.width} height=${p.height} x=${art.hx} y=${art.hy} rgba=${hash.toString(16).padStart(8,'0')}`);
 }
 // Compile the public script API, rather than calling only CursorSystem directly.
 fs.writeFileSync(`build/cursor-tests-${edition}.dfscript`,compileScript(`code test()
hidecursor()
hidecursor()
showcursor()
cursor("ToUcH")
return 12345
endcode
code reveal()
showcursor()
cursor("NONE")
endcode
code fallback()
cursor("not-in-the-executable")
endcode`));
 console.log(`CURSOR PASS files=${Object.keys(ART).length} failures=0`);
}
