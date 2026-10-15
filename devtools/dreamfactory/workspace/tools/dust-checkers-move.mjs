// SPDX-License-Identifier: GPL-3.0-or-later
// Read an earned board and emit physical drags for a legal player turn.
import {checkMove,parseBoard,legalMoves} from '../build/reference-checkers/checkers.mjs';
const [text,depthText='6',strategy='best']=process.argv.slice(2);
const board=parseBoard(text??'');if(!board)throw new Error('Expected an earned 64-cell board');
const reflected=new Int8Array(64);
for(let r=0;r<8;r++)for(let c=0;c<8;c++)reflected[(7-r)*8+c]=-board[r*8+c];
const delta={1:[-2,2],2:[2,2],3:[2,-2],4:[-2,-2],5:[-1,1],6:[1,1],7:[1,-1],8:[-1,-1]};
const reflectedText=[...reflected].join(' ');
let choices=0;
let encoded=checkMove(reflectedText,Number(depthText),0,n=>{choices=n;return 0;});
if(!encoded)throw new Error('The player has no legal move');
// Among equally rated legal turns, approach the opponent rather than repeatedly
// selecting the first king shuffle. This affects only the physical test player.
function pursuit(move){
 const b=Int8Array.from(reflected);
 let promotions=0;
 for(const t of move.split(',')){
  const r=Number(t[0]),c=Number(t[1]),[dr,dc]=delta[t[2]],p=b[r*8+c];
  b[r*8+c]=0;b[(r+dr)*8+c+dc]=(Math.abs(p)===2||r+dr===7?2:1);
  if(p===1&&r+dr===7)promotions++;
  if(Math.abs(dr)===2)b[(r+dr/2)*8+c+dc/2]=0;
 }
 const mine=[],other=[];
 for(let i=0;i<64;i++){if(b[i]>0)mine.push(i);else if(b[i]<0)other.push(i);}
 if(!other.length)return -100000;
 // Crown a stranded man before pursuing with a king when minimax rates both
 // turns equally. Squared distances then bring distant pieces into the chase,
 // instead of repeatedly moving the nearest king while the others stay behind.
 return mine.reduce((s,i)=>{const d=Math.min(...other.map(j=>Math.max(Math.abs((i>>3)-(j>>3)),Math.abs(i%8-j%8))));return s+d*d;},0)-promotions*4096;
}
if(strategy==='lose'){
 // Exercise a legitimate losing path by choosing legal moves that offer the
 // opponent the largest immediate capture. Compulsory player jumps still apply.
 const moves=legalMoves(reflected,0);
 let risk=-1;
 for(const chain of moves){
  const b=Int8Array.from(reflected);
  for(const h of chain){
   const p=b[h.row*8+h.col],r=h.row+h.dRow,c=h.col+h.dCol;
   b[h.row*8+h.col]=0;b[r*8+c]=(Math.abs(p)===2||r===7?2:1);
   if(Math.abs(h.dRow)===2)b[(h.row+h.dRow/2)*8+h.col+h.dCol/2]=0;
  }
  const capture=Math.max(0,...legalMoves(b,1).map(reply=>Math.abs(reply[0].dRow)===2?reply.length:0));
  if(capture>risk){
   risk=capture;
   encoded=chain.map(h=>`${h.row}${h.col}${Object.keys(delta).find(k=>delta[k][0]===h.dRow&&delta[k][1]===h.dCol)}`).join(',');
  }
 }
}else if([...board].some(p=>p===-2)){
 let best=pursuit(encoded);
 for(let i=1;i<choices;i++){
  const candidate=checkMove(reflectedText,Number(depthText),0,()=>i),score=pursuit(candidate);
  if(score<best){best=score;encoded=candidate;}
 }
}
for(const token of encoded.split(',')){
 const row=7-Number(token[0]),col=Number(token[1]),[rr,dc]=delta[token[2]],dr=-rr;
 const index=row*8+col;if(board[index]>=0)throw new Error('Planner selected an opponent piece');
 const ordinal=[...board.slice(0,index+1)].filter(p=>p<0).length;
 const r=row+dr,c=col+dc,p=board[index];
 console.log(`dragpoint prop me${ordinal} ${153+c*29} ${31+r*29}`);
 board[index]=0;board[r*8+c]=(Math.abs(p)===2||r===0?-2:-1);
 if(Math.abs(dr)===2)board[(row+dr/2)*8+col+dc/2]=0;
}
