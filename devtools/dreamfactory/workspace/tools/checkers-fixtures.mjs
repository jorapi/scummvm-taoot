// SPDX-License-Identifier: GPL-3.0-or-later
// Independent oracle from the unchanged, pinned reference implementation.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import ts from '../dependencies/node_modules/typescript/lib/typescript.js';
fs.mkdirSync('build/reference-checkers', {recursive:true});
const source=fs.readFileSync('references/dreamrefactory/engine/src/runtime/checkers.ts','utf8');
fs.writeFileSync('build/reference-checkers/checkers.mjs', ts.transpileModule(source, {compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText);
const {checkMove,legalMoves}=await import(pathToFileURL(path.resolve('build/reference-checkers/checkers.mjs')));
let seed=17431;
const pick=n=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed%n;};
const initial=()=>Int8Array.from({length:64},(_,i)=>((Math.floor(i/8)+i%8)%2===1 ? Math.floor(i/8)<3?1:Math.floor(i/8)>4?-1:0:0));
const apply=(board,h)=>{
 const b=Int8Array.from(board), p=b[h.row*8+h.col], r=h.row+h.dRow,c=h.col+h.dCol;
 b[h.row*8+h.col]=0;b[r*8+c]=(Math.abs(p)===2||r===(p>0?7:0)?2:1)*Math.sign(p);
 if(Math.abs(h.dRow)===2)b[(h.row+h.dRow/2)*8+h.col+h.dCol/2]=0;
 return b;
};
let cases=[];
for(let game=0;game<3;game++){
 let b=initial();
 for(let turn=0;turn<24;turn++){
  for(const side of [0,1]){
   const depth=side===0?1+turn%5:0;
   cases.push(`${depth}|${side}|${checkMove([...b].join(' '),depth,side)||'-'}|${[...b].join(' ')}`);
   const moves=legalMoves(b,side); if(!moves.length){b=initial();continue;}
   for(const hop of moves[pick(moves.length)])b=apply(b,hop);
  }
 }
}
fs.mkdirSync('build/script-tests',{recursive:true});
fs.writeFileSync('build/script-tests/checkers.cases',cases.join('\n')+'\n');
console.log(`CHECKERS reference fixtures=${cases.length}`);
