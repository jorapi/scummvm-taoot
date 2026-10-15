// SPDX-License-Identifier: GPL-3.0-or-later
// Evidence index, not a proof of reachability or permission to restore a scene.
import fs from 'node:fs';
import path from 'node:path';
import {readContainerFile} from '../build/reference-script/df/container.mjs';
import {decodeScript} from '../build/reference-script/df/script.mjs';
const prior=JSON.parse(fs.readFileSync('build/enhancements/content-inventory.json','utf8'));
const results=[];
for(const previous of prior.results){
 const edition=previous.edition, root=`game-data/${edition}`;
 const index=JSON.parse(fs.readFileSync(`${root}/resource-index.json`,'utf8'));
 const strings=[]; let decodedScripts=0;
 for(const [name,variants] of Object.entries(index)){
  if(!/\.(set|stg|flt|cst|pup|shp|trk|sfx|snd)$/i.test(name)&&name!=='BOOTFILE')continue;
  for(const variant of variants){
   let archive;try{archive=readContainerFile(new Uint8Array(fs.readFileSync(`${root}/${variant.path}`)));}catch{continue;}
   for(let container=0;container<archive.containers.length;++container){
    let tokens;try{tokens=decodeScript(archive.containers[container].data);}catch{continue;}
    if(!tokens.some(t=>t.kind==='op'&&t.id===4001))continue;
    ++decodedScripts;
    for(const token of tokens)if(typeof token.value==='string')strings.push({source:variant.path,container,text:token.value});
   }
  }
 }
 const executables=[];
 for(const [name,variants] of Object.entries(index))if(/\.exe$/i.test(name))for(const variant of variants){
  const data=fs.readFileSync(`${root}/${variant.path}`);
  executables.push({source:variant.path,ascii:data.toString('latin1').toUpperCase(),utf16:data.toString('utf16le').toUpperCase()});
 }
 const candidates=[];
 for(const candidate of previous.resourcesWithoutExactScriptLiteral){
  const filename=candidate.name.toUpperCase(),stem=filename.replace(/\.[^.]+$/,'');
  const references=strings.filter(s=>{
   const value=s.text.replaceAll(':','/').replaceAll('\\','/').toUpperCase();
   const leaf=value.split('/').at(-1);
   return leaf===filename||(stem.length>=3&&leaf===stem);
  }).map(s=>({...s,text:s.text.slice(0,200)}));
  const native=executables.filter(e=>e.ascii.includes(filename)||e.utf16.includes(filename)).map(e=>e.source);
  candidates.push({...candidate,scriptEvidence:references,nativeFilenameEvidence:native,
   classification:references.length?'Extensionless/path-qualified script evidence found':native.length?'Filename embedded in original executable; inspect native caller':'Unresolved candidate; dynamic/native references still possible'});
 }
 results.push({edition,decodedScripts,scriptEvidenceFound:candidates.filter(c=>c.scriptEvidence.length).length,
  nativeFilenameEvidenceFound:candidates.filter(c=>c.nativeFilenameEvidence.length).length,
  unresolved:candidates.filter(c=>!c.scriptEvidence.length&&!c.nativeFilenameEvidence.length).length,candidates});
}
const report={scope:'Content research only; no verified cut-content restoration or gameplay patches.',
 limitations:['String matches are evidence for investigation, not an executable reachability proof.',
 'Native filenames may occur in unused code; computed filenames and table indices remain unresolved.',
 'An unresolved candidate can still be normal gameplay, help, a rare branch or an alternate ending.'],results};
fs.writeFileSync('build/enhancements/content-triage.json',JSON.stringify(report,null,2)+'\n');
for(const r of results)console.log(`${r.edition}: ${r.scriptEvidenceFound} script leads, ${r.nativeFilenameEvidenceFound} native filename leads, ${r.unresolved} unresolved; zero verified restorations`);
