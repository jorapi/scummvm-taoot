// SPDX-License-Identifier: GPL-3.0-or-later
// Plan ordinary navigation through MINE.SET's authored repeating cross-tunnels.
// The input coordinates come from an earned native checkpoint; nothing is edited.
const raw = process.argv.slice(2);
if (raw.length !== 2 || raw.some(s => !/^-?\d+$/.test(s))) throw new Error('Usage: dust-mine-route.mjs STANDX STANDY');
let [sx,sy] = raw.map(Number);
if ([sx,sy].some(n => !Number.isSafeInteger(n) || (n-640)%1024 || Math.abs(n-640)>32768)) throw new Error('Unexpected skeleton grid coordinates');
let x=2,y=4, facing=0, steps=0;
const headings=['north','east','south','west'];
const scene=()=>`Scene ${String.fromCharCode(65+x)}${y+1}`;
function turn(to) {
  let delta=(to-facing+4)%4;
  while(delta) {
    const right=delta!==3;
    console.log(`key ${right?'rightarrow':'leftarrow'}`);
    facing=(facing+(right?1:3))%4; delta=(to-facing+4)%4;
    console.log(`expectscene ${scene()}`);console.log(`expectview ${headings[facing]}`);
  }
}
function walk() {
  if (++steps>300) throw new Error('Repeating route exceeded its bounded length');
  let changed='';
  if(x===0&&y===2&&facing===3){x=3;sx+=1024;changed='standx';}
  else if(x===4&&y===2&&facing===1){x=1;sx-=1024;changed='standx';}
  else if(x===2&&y===0&&facing===0){y=3;sy+=1024;changed='standy';}
  else if(x===2&&y===4&&facing===2){y=1;sy-=1024;changed='standy';}
  else {x+=[0,1,0,-1][facing];y+=[-1,0,1,0][facing];}
  if(x<0||x>4||y<0||y>4||(x!==2&&y!==2)) throw new Error('Route left the authored cross-tunnels');
  console.log('key uparrow');console.log(`expectscene ${scene()}`);console.log(`expectview ${headings[facing]}`);
  if(changed) console.log(`expectglobal ${changed} ${changed==='standx'?sx:sy}`);
}
walk();walk(); // Entrance C5 -> central C3.
if(sx!==640){turn(sx>640?1:3);while(sx!==640)walk();while(x!==2)walk();}
console.log('expectglobal standx 640');
if(sy===640) throw new Error('Expected an initial distant destination on both axes');
turn(sy>640?2:0);while(sy!==640)walk();
if(x!==2||![1,3].includes(y)) throw new Error('Route did not approach the skeleton from an adjacent cell');
console.log('expectglobal standy 640');
console.log('expectactorvalue skeleton 1');
console.log('dumpactor skeleton');
