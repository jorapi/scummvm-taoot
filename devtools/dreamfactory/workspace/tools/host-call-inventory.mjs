// SPDX-License-Identifier: GPL-3.0-or-later
// Static triage only: finding an opcode in C++ does not prove its semantics.
import fs from 'node:fs';
import {corpusFiles} from './audit-files.mjs';
import {readContainerFile} from '../build/reference-script/df/container.mjs';
import {decodeScript} from '../build/reference-script/df/script.mjs';

const source = fs.readFileSync('scummvm/engines/dreamfactory/session.cpp', 'utf8');
const mentioned = new Set([...source.matchAll(/(?:case\s+|call\.opcode\s*(?:==|!=)\s*)(\d+)/g)].map(m => Number(m[1])));
const report = {scope: 'All script containers in supplied volumes and installed resources. Missing IDs are candidates for review; source mentions do not establish implementation or reachability.', editions: {}};
for (const edition of process.argv.slice(2).length ? process.argv.slice(2) : ['demo','full','dust']) {
  const calls = new Map(), effects = new Map();
  let scripts = 0;
  for (const file of corpusFiles(edition)) {
    let archive;
    try { archive = readContainerFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${file}`))); } catch { continue; }
    for (const container of archive.containers) {
      let tokens;
      try { tokens = decodeScript(container.data); } catch { continue; }
      if (!tokens.some(t => t.kind === 'op' && t.id === 4001)) continue;
      ++scripts;
      for (let index = 0; index < tokens.length; ++index) {
        const token = tokens[index];
        if (token.kind !== 'op' || token.id < 12000) continue;
        if (token.id >= 24000) {
          if (!effects.has(token.id)) effects.set(token.id, {id: token.id, decodedName: token.name, occurrences: []});
          effects.get(token.id).occurrences.push({file, container: container.id, token: index});
          continue;
        }
        if (!calls.has(token.id)) calls.set(token.id, {id: token.id, decodedName: token.name, sourceMention: mentioned.has(token.id), occurrences: []});
        calls.get(token.id).occurrences.push({file, container: container.id, token: index});
      }
    }
  }
  const entries = [...calls.values()].sort((a,b) => a.id - b.id);
  const missing = entries.filter(c => !c.sourceMention);
  report.editions[edition] = {scripts, calls: entries, effects: [...effects.values()].sort((a,b) => a.id - b.id)};
  console.log(`${edition}: ${scripts} scripts, ${entries.length} host IDs, ${missing.length} IDs absent from Session dispatch`);
  for (const call of missing) console.log(`  ${call.id} ${call.decodedName}: ${call.occurrences.length} occurrences; ${call.occurrences[0].file} container=${call.occurrences[0].container}`);
  console.log(`  Visual effects: ${[...effects.values()].map(e => `${e.decodedName}=${e.occurrences.length}`).join(', ')}`);
}
fs.mkdirSync('build', {recursive:true});
fs.writeFileSync('build/host-call-inventory.json', JSON.stringify(report, null, 2) + '\n');
