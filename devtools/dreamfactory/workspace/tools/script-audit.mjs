// SPDX-License-Identifier: GPL-3.0-or-later
// Independent oracle: transpile and execute the reference implementation unchanged.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
import {corpusFiles} from './audit-files.mjs';
import './checkers-fixtures.mjs';
const dest = path.resolve('build/reference-script');
for (const [dir, names] of Object.entries({df: ['byte-order','binary','version','container','opcodes','text','script','script-asm'], runtime: ['ast','caseless','parser','interp','geometry','point'], 'runtime/builtins':['core','helpers','pointer']})) {
  fs.mkdirSync(path.join(dest,dir), {recursive:true});
  for (const name of names) {
    const source = fs.readFileSync(`references/dreamrefactory/engine/src/${dir}/${name}.ts`,'utf8');
    const js = ts.transpileModule(source, {compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
    fs.writeFileSync(path.join(dest,dir,`${name}.mjs`),js.replace(/from (["'])(\.{1,2}\/[^"']+)\1/g, 'from $1$2.mjs$1'));
  }
}
const load = file => import(pathToFileURL(path.join(dest,file+'.mjs')));
const {readContainerFile} = await load('df/container');
const {decodeScript} = await load('df/script');
const {compileScript} = await load('df/script-asm');
const {parseScript} = await load('runtime/parser');
const {Interpreter,ScriptInstance} = await load('runtime/interp');
const {registerCoreBuiltins} = await load('runtime/builtins/core');
const {registerHelperBuiltins} = await load('runtime/builtins/helpers');
const {registerPointerBuiltins} = await load('runtime/builtins/pointer');
const fixtures = JSON.parse(fs.readFileSync('tests/scripts.json','utf8'));
fs.mkdirSync('build/script-tests',{recursive:true});
for (const [name,source] of Object.entries(fixtures)) {
  const bytes = compileScript(source);
  fs.writeFileSync(`build/script-tests/${name}.dfscript`,bytes);
  const vm = new Interpreter();
  if(name.startsWith('host-')) {
    const session={pathSlots:Array(9).fill(''),lastResult:0};
    session.pathSlots[0]=session.pathSlots[1]='installed:';
    const ctx={session,interp:vm,r:vm.register.bind(vm)};
    registerCoreBuiltins(vm);registerHelperBuiltins(ctx);registerPointerBuiltins(ctx);
  }
  let value = 0;
  try { value = (await vm.runHandler(new ScriptInstance('test',parseScript(decodeScript(bytes))),'test',[],{me:'test',target:''})).value; }
  catch { value = 'ERROR'; }
  console.log(`TEST ${name}.dfscript value=${value} string=${value === 'ERROR' ? 0 : typeof value === 'string' ? 1 : 0} consumed=${vm.eventConsumed ? 1 : 0}`);
}
function count(body) {
  return body.reduce((n,s) => n+1+count(s.body??[])+count(s.else_??[])+(s.cases??[]).reduce((m,c)=>m+count(c.body),0),0);
}
for (const edition of process.argv.slice(2).length ? process.argv.slice(2) : ['demo','full','dust']) {
  let scripts=0,handlers=0,failures=0;
  for (const name of corpusFiles(edition)) {
    const filePath = `game-data/${edition}/${name}`;
    if (!fs.statSync(filePath).isFile()) continue;
    let archive;
    try { archive=readContainerFile(new Uint8Array(fs.readFileSync(filePath))); } catch { continue; }
    let ns=0,nh=0,nst=0;
    for (const c of archive.containers) {
      let tokens;
      try { tokens=decodeScript(c.data); } catch { continue; }
      if (!tokens.some(t=>t.kind==='op' && t.id===4001)) continue;
      try {
        const script=parseScript(tokens); ++ns; nh+=script.codes.size; nst+=count(script.topLevel);
        for (const handler of script.codes.values()) nst+=count(handler.body);
      } catch(e) { ++failures; console.log(`SCRIPT FAIL ${name} container=${c.id} ${e.message}`); }
    }
    if(ns) console.log(`SCRIPT ${name} scripts=${ns} handlers=${nh} statements=${nst}`);
    scripts+=ns;handlers+=nh;
  }
  console.log(`SCRIPT ${failures ? 'FAIL':'PASS'} scripts=${scripts} handlers=${handlers} failures=${failures}`);
}
