// SPDX-License-Identifier: GPL-3.0-or-later
// Expected lookups are derived from physical source paths, not native lookup.
import fs from 'node:fs';
const edition=process.argv[2];
const root=`game-data/${edition}`;
const index=JSON.parse(fs.readFileSync(`${root}/resource-index.json`,'utf8'));
const cases=[];
let serial=0;
function add(prefix,query,paths,source,cd='-') {
  const slots=Array(8).fill('-'); paths.forEach((p,i)=>slots[i]=p);
  cases.push({name:`${prefix}-${String(++serial).padStart(4,'0')}`,query,slots,source,cd});
}
for(const [basename,variants] of Object.entries(index).sort()) {
  for(const v of variants) {
    const logical=v.path.replace(/^volumes\//,'');
    const dir=logical.slice(0,logical.lastIndexOf('/')+1).replaceAll('/',':');
    const cd=logical.startsWith('installed/') ? '-' : logical.split('/')[0];
    // Upper-case queries deliberately exercise case-insensitive virtual paths.
    add('direct',logical.replaceAll('/',':').toUpperCase(),[],v.path,cd);
    add('search',basename,[dir],v.path,cd);
  }
  if(variants.length>1) {
    const paths=variants.map(v=>v.path.replace(/^volumes\//,'').slice(0,-basename.length).replaceAll('/',':'));
    add('priority',basename,paths.slice(0,8),variants[0].path);
  }
}
add('missing','no-such-file.set',['installed:local:'],null);
add('traversal','installed:local:..:bootfile',[],null);
add('wrong-volume','missingdisc:data:BOOTFILE',[],null);
add('missing-cd','BOOTFILE',['installed:'],index.BOOTFILE.find(v=>v.path.startsWith('installed/'))?.path,'missingdisc');
const chunks=[];
const count=Buffer.alloc(4);count.writeUInt32LE(cases.length);chunks.push(count);
for(const c of cases) {
  for(const s of [c.name,c.cd,...c.slots,c.query]) {
    const bytes=Buffer.from(s,'latin1');if(bytes.length>255)throw new Error('Fixture string too long');
    chunks.push(Buffer.from([bytes.length]),bytes);
  }
  let size=0,hash=2166136261;
  if(c.source) {
    const file=`${root}/${c.source}`;size=fs.statSync(file).size;
    const fd=fs.openSync(file,'r');const buffer=Buffer.alloc(5000);
    const n=fs.readSync(fd,buffer,0,buffer.length,0);fs.closeSync(fd);
    for(const b of buffer.subarray(0,n))hash=Math.imul(hash^b,16777619)>>>0;
  }
  const mounted=c.cd==='missingdisc' ? '-' : c.cd;
  console.log(`RESOURCE ${c.name} cd=${mounted} path=${c.source??'-'} size=${size} fnv1a=${hash.toString(16).padStart(8,'0')}`);
}
fs.writeFileSync(`build/resource-tests-${edition}.bin`,Buffer.concat(chunks));
console.log(`RESOURCE PASS cases=${cases.length}`);
