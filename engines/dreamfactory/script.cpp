/* SPDX-License-Identifier: GPL-3.0-or-later
 * Grammar adapted from dreamREfactory's parser.ts; see README.dreamfactory.
 */
#include "dreamfactory/script.h"
#include "dreamfactory/opcodes.h"
namespace DreamFactory {
const char *opcodeName(uint16 opcode) {
	for (uint i = 0; i < ARRAYSIZE(kOpcodeNames); ++i) if (kOpcodeNames[i].id == opcode) return kOpcodeNames[i].name;
	return nullptr;
}
bool decodeScript(const Bytes &data, ScriptTokens &tokens, Common::String &error) {
	tokens.clear(); error.clear();
	for (uint32 pos = 0; pos <= data.size() && data.size() - pos >= 8; pos += 8) {
		Reader r(data, pos); ScriptToken t; t.opcode = r.u16(); t.number = r.u32();
		if (!t.opcode) return true;
		if (t.opcode == 3 || t.opcode == 5) {
			if (t.number >= data.size() - pos) { error = "String reference outside script"; return false; }
			uint32 at = pos + t.number; Reader text(data, at); uint32 length = text.u8();
			if (length > data.size() - at - 1) { error = "Truncated script string"; return false; }
			for (uint32 i = 0; i < length; ++i) t.text += char(text.u8());
		} else if (t.opcode != 4 && t.opcode != 6 && !opcodeName(t.opcode)) {
			error = Common::String::format("Unknown script token %u at %u", t.opcode, pos); return false;
		}
		if (t.opcode != 1) tokens.push_back(t);
	}
	error = "Unterminated script token stream";
	return false;
}

class ScriptParser {
public:
	ScriptParser(const ScriptTokens &tokens, Script &script, Common::String &error) : _tokens(tokens), _script(script), _error(error), _pos(0), _depth(0) {}
	bool parse();
private:
	const ScriptTokens &_tokens;
	Script &_script;
	Common::String &_error;
	uint32 _pos, _depth;
	uint16 at() const { return _pos < _tokens.size() ? _tokens[_pos].opcode : 0; }
	void fail(const char *message) { if (_error.empty()) _error = Common::String::format("%s at token %u", message, _pos); }
	bool expect(uint16 op) { if (at() == op) { ++_pos; return true; } fail("Unexpected token"); return false; }
	void breaks() { while (at() == 6) ++_pos; }
	bool boundary() const { return !at() || at() == 4001 || at() == 4004 || at() == 4011 || at() == 4010; }
	void close(uint16 op) { if (at() == op) ++_pos; else if (!boundary()) fail("Missing block closer"); }
	Common::String identifier();
	uint32 expression(uint32 minPrecedence = 1);
	uint32 primary();
	uint32 append(const Expression &e) { _script.expressions.push_back(e); return _script.expressions.size() - 1; }
	Statement statement();
	Common::Array<Statement> block(uint16 closer, uint16 other = 0);
};

static uint32 precedence(uint16 op) {
	if (op == 8006) return 1;
	if (op == 8005) return 2;
	if (op >= 8008 && op <= 8013) return 3;
	if (op == 8007) return 4;
	if (op == 8001 || op == 8002) return 5;
	if (op == 8003 || op == 8004) return 6;
	return 0;
}
Common::String ScriptParser::identifier() {
	if (at() != 5) { fail("Expected identifier"); return Common::String(); }
	Common::String name = _tokens[_pos++].text; name.toLowercase(); return name;
}
uint32 ScriptParser::expression(uint32 minPrecedence) {
	if (++_depth > 128) { fail("Expression nesting limit"); --_depth; return 0; }
	uint32 left = primary();
	while (_error.empty() && precedence(at()) >= minPrecedence) {
		Expression e; e.kind = Expression::kBinary; e.opcode = at();
		uint32 p = precedence(at()); ++_pos;
		e.args.push_back(left); e.args.push_back(expression(p + 1)); left = append(e);
	}
	--_depth; return left;
}
uint32 ScriptParser::primary() {
	if (_pos >= _tokens.size()) { fail("Expected expression"); return 0; }
	ScriptToken t = _tokens[_pos++]; Expression e; e.opcode = t.opcode;
	if (t.opcode == 8002 || t.opcode == 4023) {
		e.kind = Expression::kUnary; e.args.push_back(expression(7)); return append(e);
	}
	if (t.opcode == 4018) { uint32 result = expression(); expect(4019); return result; }
	if (t.opcode == 4) { e.kind = Expression::kNumber; e.number = int32(t.number); }
	else if (t.opcode == 3) { e.kind = Expression::kString; e.name = t.text; }
	else if (t.opcode == 4021 || t.opcode == 4022) { e.kind = Expression::kNumber; e.number = t.opcode == 4021 ? 1 : 0; }
	else if (t.opcode == 4026) e.kind = Expression::kMe;
	else if (t.opcode == 4027) e.kind = Expression::kTarget;
	else if (t.opcode == 5 || (t.opcode >= 4001 && t.opcode < 8000) || t.opcode >= 12000) {
		e.name = t.opcode == 5 ? t.text : opcodeName(t.opcode); e.name.toLowercase();
		e.kind = t.opcode == 5 ? Expression::kVariable : Expression::kCall;
		if (at() == 4018) {
			e.kind = Expression::kCall; ++_pos;
			while (at() && at() != 4019 && _error.empty()) {
				e.args.push_back(expression());
				if (at() == 4020) ++_pos;
			}
			expect(4019);
		}
	} else {
		if (_error.empty()) _error = Common::String::format("Unexpected expression token %u at token %u", t.opcode, _pos - 1);
	}
	return append(e);
}
Common::Array<Statement> ScriptParser::block(uint16 closer, uint16 other) {
	Common::Array<Statement> result; breaks();
	while (!boundary() && at() != closer && (!other || at() != other) && _error.empty()) {
		result.push_back(statement()); breaks();
	}
	return result;
}
Statement ScriptParser::statement() {
	Statement s;
	if (++_depth > 128) { fail("Statement nesting limit"); --_depth; return s; }
	uint16 op = at();
	if (op == 4002 || op == 4003 || op == 4028 || op == 4029) {
		++_pos; s.kind = Statement::kDeclaration; s.opcode = op;
		bool wrapped = at() == 4018; if (wrapped) ++_pos;
		s.names.push_back(identifier());
		while (at() == 4020 && _error.empty()) { ++_pos; s.names.push_back(identifier()); }
		if (wrapped) expect(4019);
	} else if (op == 4005 || op == 4025) {
		++_pos; s.kind = op == 4005 ? Statement::kExit : Statement::kPass;
		if (at() == 4018) { ++_pos; expect(4019); }
	} else if (op == 4024) {
		++_pos; s.kind = Statement::kReturn;
		if (at() && at() != 6 && !boundary()) s.expressions.push_back(expression());
	} else if (op == 4006) {
		++_pos; s.kind = Statement::kIf; s.expressions.push_back(expression());
		s.body = block(4007, 4008);
		if (at() == 4008) { ++_pos; s.alternate = block(4007); }
		close(4007);
	} else if (op == 4009) {
		++_pos; s.kind = Statement::kSwitch; s.expressions.push_back(expression()); block(4010);
		while (at() == 4011 && _error.empty()) {
			++_pos; Statement arm; arm.kind = Statement::kCase; arm.expressions.push_back(expression()); arm.body = block(4010);
			s.body.push_back(arm);
		}
		close(4010);
	} else if (op == 4016) {
		++_pos; s.kind = Statement::kWhile; s.expressions.push_back(expression()); s.body = block(4017); close(4017);
	} else if (op == 4012) {
		++_pos; s.kind = Statement::kFor; s.name = identifier(); expect(8008); s.expressions.push_back(expression());
		expect(4013); s.expressions.push_back(expression());
		if (at() == 4014) { ++_pos; s.expressions.push_back(expression()); }
		s.body = block(4015); close(4015);
	} else if (op == 5 && _pos + 1 < _tokens.size() && _tokens[_pos + 1].opcode == 8008) {
		s.kind = Statement::kAssign; s.name = identifier(); ++_pos; s.expressions.push_back(expression());
	} else if (op == 8004 || (op == 5 && (_pos + 1 >= _tokens.size() || _tokens[_pos + 1].opcode != 4018))) {
		// Shipped comment lines and misspelled bare labels are inert.
		while (at() && at() != 6) ++_pos;
	} else {
		s.kind = Statement::kCall; uint32 e = expression(); s.expressions.push_back(e);
		if (_error.empty() && _script.expressions[e].kind != Expression::kCall) fail("Expected call statement");
	}
	--_depth; return s;
}
bool ScriptParser::parse() {
	bool hasHandlers = false;
	for (uint32 i = 0; i < _tokens.size(); ++i) if (_tokens[i].opcode == 4001) hasHandlers = true;
	breaks();
	while (at() && _error.empty()) {
		if (at() == 4001) {
			++_pos; Handler h; h.name = identifier(); expect(4018);
			while (at() && at() != 4019 && _error.empty()) { h.params.push_back(identifier()); if (at() == 4020) ++_pos; }
			expect(4019); h.body = block(4004);
			if (at() == 4004) ++_pos;
			_script.handlers[h.name] = h;
		} else {
			uint32 start = _pos; Statement s = statement();
			if (!_error.empty() && hasHandlers) {
				// Dead compile remnants outside handlers occur in the originals.
				_error.clear(); _pos = start;
				while (at() && at() != 4001) ++_pos;
			} else _script.topLevel.push_back(s);
		}
		breaks();
	}
	return _error.empty();
}
bool parseScript(const ScriptTokens &tokens, Script &script, Common::String &error) {
	error.clear(); Script parsed;
	if (!ScriptParser(tokens, parsed, error).parse()) return false;
	script = parsed; return true;
}
bool loadScript(const Bytes &data, Script &script, Common::String &error) {
	ScriptTokens tokens;
	return decodeScript(data, tokens, error) && parseScript(tokens, script, error);
}
}
