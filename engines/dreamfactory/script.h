/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SCRIPT_H
#define DREAMFACTORY_SCRIPT_H
#include "dreamfactory/archive.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
namespace DreamFactory {
struct ScriptToken {
	uint16 opcode = 0;
	uint32 number = 0;
	Common::String text;
};
typedef Common::Array<ScriptToken> ScriptTokens;
bool decodeScript(const Bytes &data, ScriptTokens &tokens, Common::String &error);
const char *opcodeName(uint16 opcode);

struct Expression {
	enum Kind { kNumber, kString, kVariable, kCall, kBinary, kUnary, kMe, kTarget } kind = kNumber;
	uint16 opcode = 0;
	int32 number = 0;
	Common::String name;
	Common::Array<uint32> args;
};
struct Statement {
	enum Kind { kNoop, kDeclaration, kAssign, kCall, kIf, kSwitch, kCase, kWhile, kFor, kExit, kPass, kReturn } kind = kNoop;
	uint16 opcode = 0;
	Common::String name;
	Common::Array<Common::String> names;
	Common::Array<uint32> expressions;
	Common::Array<Statement> body, alternate;
};
struct Handler {
	Common::String name;
	Common::Array<Common::String> params;
	Common::Array<Statement> body;
};
struct Script {
	Common::Array<Expression> expressions;
	Common::HashMap<Common::String, Handler> handlers;
	Common::Array<Statement> topLevel;
};
bool parseScript(const ScriptTokens &tokens, Script &script, Common::String &error);
bool loadScript(const Bytes &data, Script &script, Common::String &error);
}
#endif
