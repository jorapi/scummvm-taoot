// SPDX-License-Identifier: GPL-3.0-or-later
// Conservative research inventory. Absence of a literal filename reference is
// never proof of cut content: scripts compose names and native loaders use tables.
import fs from 'node:fs';
import {readContainerFile} from '../build/reference-script/df/container.mjs';
import {decodeScript} from '../build/reference-script/df/script.mjs';
const results = [];
for (const edition of ['full', 'demo', 'dust']) {
  const index = JSON.parse(fs.readFileSync(`game-data/${edition}/resource-index.json`, 'utf8'));
  const references = new Set(); let scripts = 0;
  for (const [name, variants] of Object.entries(index)) {
    if (!/\.(set|stg|cst|pup|shp|trk|sfx|snd)$/i.test(name) && name !== 'BOOTFILE') continue;
    for (const variant of variants) {
      let file;
      try { file = readContainerFile(new Uint8Array(fs.readFileSync(`game-data/${edition}/${variant.path}`))); }
      catch { continue; }
      for (const container of file.containers) {
        try {
          const tokens = decodeScript(container.data);
          if (!tokens.some(t => t.kind === 'op' && t.id === 4001)) continue;
          ++scripts;
          for (const token of tokens) if (typeof token.value === 'string') references.add(token.value.toUpperCase());
        } catch { /* Most containers hold images, audio or tables. */ }
      }
    }
  }
  results.push({edition, uniqueResources: Object.keys(index).length, decodedScripts: scripts,
    literalReferenceCount: references.size,
    resourcesWithoutExactScriptLiteral: Object.entries(index).filter(([name]) =>
      /\.(mov|pup|jpg|pic)$/i.test(name) && !references.has(name)).map(([name, variants]) =>
      ({name, variants: variants.map(v => ({path: v.path, bytes: v.bytes, sha256: v.sha256}))}))});
}
fs.mkdirSync('build/enhancements', {recursive: true});
fs.writeFileSync('build/enhancements/content-inventory.json', JSON.stringify({
  scope: 'Research candidates only. No verified cut scenes or restored content.',
  limitations: ['Dynamic filenames and native table references are not resolved.',
    'Rare branches, alternate endings, demo assets and developer tools are not automatically cut content.',
    'Each candidate requires resource inspection and a reproducible reachability check.'], results}, null, 2) + '\n');
for (const result of results) console.log(result.edition, result.uniqueResources, 'resources;', result.decodedScripts, 'scripts;', result.resourcesWithoutExactScriptLiteral.length, 'literal-only research candidates');
