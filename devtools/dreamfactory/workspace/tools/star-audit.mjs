// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the unmodified reference decoder after transpiling its TS modules.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
const source = path.resolve('references/dreamrefactory/engine/src/df');
const dest = path.resolve('build/reference-star');
fs.mkdirSync(dest, { recursive: true });
for (const name of ['byte-order', 'binary', 'version', 'container', 'set', 'set-v1', 'image']) {
  const text = fs.readFileSync(path.join(source, name + '.ts'), 'utf8');
  const js = ts.transpileModule(text, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 } }).outputText;
  fs.writeFileSync(path.join(dest, name + '.mjs'), js.replace(/from "(\.\/[^".]+(?:-[^".]+)?)"/g, 'from "$1.mjs"'));
}
const { readSetFile } = await import(pathToFileURL(path.join(dest, 'set.mjs')));
const { readSetFileV1 } = await import(pathToFileURL(path.join(dest, 'set-v1.mjs')));

const {readStarPath} = await import(pathToFileURL(path.join(dest, 'set.mjs')));
const {corpusFiles} = await import('./audit-files.mjs');
class Hash {
  hash=2166136261;
  byte(n){this.hash=Math.imul(this.hash^(n&255),16777619)>>>0;}
  number(n){for(let i=0;i<4;++i)this.byte(n>>>(8*i));}
  text(s){for(const c of s)this.byte(c.charCodeAt(0));this.byte(0);}
  hex(){return this.hash.toString(16).padStart(8,'0');}
}
for(const edition of process.argv.slice(2).length?process.argv.slice(2):['demo','full','dust']) {
  let files=0;
  for(const name of corpusFiles(edition).filter(n=>/\.set$/i.test(n))) {
    const bytes=new Uint8Array(fs.readFileSync(`game-data/${edition}/${name}`));
    const set=edition==='dust'?readSetFileV1(bytes):readSetFile(bytes);
    const hash=new Hash();let points=0;
    for(const star of set.actors){hash.text(star.identifier);for(const n of [star.positionX,star.positionZ,star.positionY,star.rotation8])hash.number(n);}
    for(const path of set.starPaths){hash.text(path.a);hash.text(path.b);hash.number(path.container);const route=readStarPath(set.file.containers,path.container,edition==='dust'?1:4);hash.number(route.length);points+=route.length;for(const point of route)for(const n of [point.x,point.z,point.y,point.fromPrev])hash.number(n);}
    console.log(`STAR ${name} stars=${set.actors.length} paths=${set.starPaths.length} points=${points} meta=${hash.hex()}`);++files;
  }
  console.log(`STAR PASS files=${files}`);
}
