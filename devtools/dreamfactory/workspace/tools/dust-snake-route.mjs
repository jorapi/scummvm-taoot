// SPDX-License-Identifier: GPL-3.0-or-later
// Read the native rotor dump and plan physical drags from SNAKE.PRP's coupling.
// No game resources, native saves, VM state or inventory are modified.
import fs from 'node:fs';
const log = fs.readFileSync(process.argv[2] ?? 'build/dust-snake-puzzle-entry-native.log', 'utf8');
const degrees = Array(7).fill(null);
for (const m of log.matchAll(/BOOT PROP name=stop([1-7]) deg=(\d+)/g)) degrees[Number(m[1])-1] = Number(m[2]);
if (degrees.some(d => d === null || d < 0 || d >= 36 || d % 6)) throw new Error('Expected seven native rotor positions in multiples of six');
const points = [[256,217],[181,175],[181,91],[255,47],[330,91],[330,176],[256,131]];
const mod = (n, base) => (n % base + base) % base;
const matrix = points.map((p,row) => points.map((q,col) => {
  if (row === col) return 1;
  const d = Math.floor(Math.hypot(p[0]-q[0], p[1]-q[1]));
  return d > 150 ? 3 : d > 100 && d < 150 ? 2 : d > 50 && d < 100 ? 1 : 0;
}));
const goal = degrees.map(d => mod(-d/6,6));
let best = null, cost = Infinity;
for (let packed=0;packed<6**7;++packed) {
  let n=packed, candidate=[], weight=0;
  for (let i=0;i<7;++i) { const x=n%6; n=Math.floor(n/6); candidate.push(x); weight+=Math.min(x,6-x); }
  if (weight >= cost) continue;
  if (matrix.every((row,i) => mod(row.reduce((sum,a,j)=>sum+a*candidate[j],0),6) === goal[i])) {
    best=candidate; cost=weight;
  }
}
if (!best) throw new Error('Native rotor arrangement is unreachable under the authored coupling');
const state = degrees.slice();
for (let col=0;col<7;++col) {
  const moves = best[col] <= 3 ? best[col] : best[col]-6;
  const sign = Math.sign(moves);
  for (let move=0;move<Math.abs(moves);++move) {
    console.log(`dragarc prop stop${col+1} ${points[col][0]} ${points[col][1]} ${sign*16} 3`);
    for (let row=0;row<7;++row) {
      state[row]=mod(state[row]+sign*6*matrix[row][col],36);
      console.log(`expectpropdeg stop${row+1} ${state[row]}`);
    }
  }
}
if (state.some(d => d)) throw new Error('Planned physical moves do not solve the arrangement');
