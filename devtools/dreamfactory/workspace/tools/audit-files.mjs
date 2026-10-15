// SPDX-License-Identifier: GPL-3.0-or-later
import fs from 'node:fs';
import path from 'node:path';
export function corpusFiles(edition) {
  const root=path.resolve('game-data',edition), files=[];
  function visit(dir,depth=0) {
    if(depth>16)throw new Error(`Corpus directory nesting: ${dir}`);
    for(const entry of fs.readdirSync(dir,{withFileTypes:true})) {
      const file=path.join(dir,entry.name);
      if(entry.isDirectory())visit(file,depth+1);
      else if(entry.isFile())files.push(path.relative(root,file).replaceAll('\\','/'));
    }
  }
  if(fs.existsSync(path.join(root,'volumes'))) {
    visit(path.join(root,'volumes'));
    if(fs.existsSync(path.join(root,'installed')))visit(path.join(root,'installed'));
  }else visit(root);
  return files.sort();
}
