/* SPDX-License-Identifier: GPL-3.0-or-later
 * Rules, move encoding and search adapt dreamREfactory/runtime/checkers.ts.
 * The original CHECKERS.PRP validates moves, draws pieces and owns game state.
 */
#include "dreamfactory/checkers.h"
#include "common/array.h"
#include "common/random.h"
#include "common/tokenizer.h"
#include "common/textconsole.h"

namespace DreamFactory {
namespace {
struct Board { int8 cells[64] = {}; int at(int row, int col) const { return cells[row * 8 + col]; } };
struct Hop { int row, col, dr, dc; };
typedef Common::Array<Hop> Chain;
typedef Common::Array<Chain> Moves;
static bool on(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
static int sign(int n) { return n > 0 ? 1 : n < 0 ? -1 : 0; }
static bool parse(const Common::String &text, Board &b) {
	Common::StringTokenizer words(text, " \t\r\n\f\v");
	for (int i = 0; i < 64; ++i) {
		if (words.empty()) return false;
		const Common::String word = words.nextToken(); char *end = nullptr;
		const long n = strtol(word.c_str(), &end, 10);
		if (end == word.c_str() || *end || n < -2 || n > 2) return false;
		b.cells[i] = int8(n);
	}
	return words.empty();
}
static Board apply(const Board &b, const Hop &h) {
	Board next = b;
	const int piece = b.at(h.row, h.col), row = h.row + h.dr, col = h.col + h.dc;
	const bool king = ABS(piece) == 2 || row == (piece > 0 ? 7 : 0);
	next.cells[row * 8 + col] = int8((king ? 2 : 1) * sign(piece));
	next.cells[h.row * 8 + h.col] = 0;
	if (ABS(h.dr) == 2) next.cells[(h.row + h.dr / 2) * 8 + h.col + h.dc / 2] = 0;
	return next;
}
static Board apply(const Board &b, const Chain &chain) {
	Board next = b;
	for (uint32 i = 0; i < chain.size(); ++i) next = apply(next, chain[i]);
	return next;
}
static int ways(int piece, Hop *out) {
	const int f = sign(piece);
	out[0] = {0, 0, f, 1}; out[1] = {0, 0, f, -1};
	if (ABS(piece) != 2) return 2;
	out[2] = {0, 0, -f, 1}; out[3] = {0, 0, -f, -1}; return 4;
}
static void chains(const Board &b, int row, int col, const Chain &prefix, Moves &out) {
	Hop directions[4]; const int piece = b.at(row, col), count = ways(piece, directions);
	bool continued = false;
	for (int i = 0; i < count; ++i) {
		const int dr = directions[i].dr, dc = directions[i].dc;
		if (!on(row + 2 * dr, col + 2 * dc) || b.at(row + 2 * dr, col + 2 * dc)) continue;
		const int over = b.at(row + dr, col + dc);
		if (!over || sign(over) == sign(piece)) continue;
		const Hop hop = { row, col, dr * 2, dc * 2 };
		Chain next = prefix; next.push_back(hop);
		// Promotion occurs before continuing, including backward king jumps.
		chains(apply(b, hop), row + hop.dr, col + hop.dc, next, out);
		continued = true;
	}
	if (!continued && !prefix.empty()) out.push_back(prefix);
}
static Moves legal(const Board &b, int side) {
	Moves jumps, steps; const int mine = side == 0 ? 1 : -1;
	for (int row = 0; row < 8; ++row) for (int col = 0; col < 8; ++col) {
		const int piece = b.at(row, col); if (sign(piece) != mine) continue;
		chains(b, row, col, Chain(), jumps);
		Hop dirs[4]; const int count = ways(piece, dirs);
		for (int i = 0; i < count; ++i) if (on(row + dirs[i].dr, col + dirs[i].dc) && !b.at(row + dirs[i].dr, col + dirs[i].dc)) {
			Chain step; step.push_back({ row, col, dirs[i].dr, dirs[i].dc }); steps.push_back(step);
		}
	}
	return jumps.empty() ? steps : jumps;
}
static int evaluate(const Board &b) {
	int score = 0;
	for (int row = 0; row < 8; ++row) for (int col = 0; col < 8; ++col) {
		const int p = b.at(row, col); if (!p) continue;
		score += sign(p) * (ABS(p) == 2 ? 175 : 100);
		if (ABS(p) == 1) score += sign(p) * (p > 0 ? row : 7 - row);
	}
	return score;
}
static int search(const Board &b, int side, int depth, int alpha, int beta) {
	const Moves moves = legal(b, side);
	if (moves.empty()) return side == 0 ? -100000 : 100000;
	if (depth <= 0) return evaluate(b);
	int best = side == 0 ? -1000000 : 1000000;
	for (uint32 i = 0; i < moves.size(); ++i) {
		const int got = search(apply(b, moves[i]), side == 0 ? 1 : 0, depth - 1, alpha, beta);
		if (side == 0) { best = MAX(best, got); alpha = MAX(alpha, got); }
		else { best = MIN(best, got); beta = MIN(beta, got); }
		if (beta <= alpha) break;
	}
	return best;
}
static Common::String text(const Hop &h) {
	int code = 8;
	if (h.dr == -2 && h.dc == 2) code = 1;
	else if (h.dr == 2 && h.dc == 2) code = 2;
	else if (h.dr == 2 && h.dc == -2) code = 3;
	else if (h.dr == -2 && h.dc == -2) code = 4;
	else if (h.dr == -1 && h.dc == 1) code = 5;
	else if (h.dr == 1 && h.dc == 1) code = 6;
	else if (h.dr == 1 && h.dc == -1) code = 7;
	return Common::String::format("%d%d%d", h.row, h.col, code);
}
static Common::String boardText(const Board &b) {
	Common::String out;
	for (int i = 0; i < 64; ++i) { if (i) out += ' '; out += Common::String::format("%d", b.cells[i]); }
	return out;
}
}
Common::String checkersMove(const Common::String &board, int32 depth, int32 side, Common::RandomSource *random) {
	Board b; if (!parse(board, b)) return Common::String();
	const Moves moves = legal(b, side); if (moves.empty()) return Common::String();
	Common::String out;
	if (side != 0) {
		for (uint32 i = 0; i < moves.size(); ++i) { if (i) out += ','; out += text(moves[i][0]); }
		return out;
	}
	const int plies = CLIP<int32>(depth, 1, 8);
	int best = -1000000; Common::Array<uint32> choices;
	for (uint32 i = 0; i < moves.size(); ++i) {
		const int got = search(apply(b, moves[i]), 1, plies - 1, -1000000, 1000000);
		if (got > best) { best = got; choices.clear(); choices.push_back(i); }
		else if (got == best) choices.push_back(i);
	}
	const Chain &chosen = moves[choices[random ? random->getRandomNumber(choices.size() - 1) : 0]];
	for (uint32 i = 0; i < chosen.size(); ++i) { if (i) out += ','; out += text(chosen[i]); }
	return out;
}
bool checkersSelfTest() {
	Board b;
	if (!checkersMove("", 3, 0).empty() || !checkersMove("0 0 0", 3, 0).empty()) return false;
	b.cells[3 * 8 + 3] = 1; b.cells[4 * 8 + 4] = -1;
	if (checkersMove(boardText(b), 3, 0) != "332") return false;
	b = Board(); b.cells[2 * 8 + 2] = 1; b.cells[3 * 8 + 3] = b.cells[5 * 8 + 5] = -1;
	if (checkersMove(boardText(b), 3, 0) != "222,442") return false;
	b = Board(); b.cells[5 * 8 + 3] = 1; b.cells[6 * 8 + 4] = b.cells[6 * 8 + 6] = -1;
	if (checkersMove(boardText(b), 3, 0) != "532,751") return false;
	b = Board(); b.cells[7 * 8 + 7] = 1;
	if (!checkersMove(boardText(b), 3, 0).empty()) return false;
	b = Board(); b.cells[3 * 8 + 3] = 2; b.cells[2 * 8 + 2] = -1;
	if (checkersMove(boardText(b), 3, 0) != "334") return false;
	b = Board(); b.cells[5 * 8 + 3] = b.cells[5 * 8 + 5] = -1; b.cells[4 * 8 + 4] = b.cells[4 * 8 + 6] = 1;
	if (checkersMove(boardText(b), 0, 1) != "531,551,554") return false;
	b = Board(); b.cells[1 * 8 + 3] = 1;
	if (checkersMove(boardText(b), 2, 0) != "136") return false;
	warning("CHECKERS PASS forced capture multi-jump promotion backward king player alternatives no moves malformed board search");
	return true;
}
}
