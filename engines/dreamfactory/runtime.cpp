/* SPDX-License-Identifier: GPL-3.0-or-later
 * Language semantics follow dreamREfactory/runtime/interp.ts.
 */
#include "dreamfactory/runtime.h"
namespace DreamFactory {
int32 Value::number() const { return isString ? int32(strtol(string.c_str(), nullptr, 10)) : integer; }
Common::String Value::text() const { return isString ? string : Common::String::format("%d", integer); }
bool Value::equals(const Value &other) const {
	return !isString && !other.isString ? integer == other.integer : text().equalsIgnoreCase(other.text());
}
Value Runtime::get(Common::String name, const ScriptFrame &frame) const {
	name.toLowercase();
	Variables::const_iterator local = frame.locals.find(name);
	if (local != frame.locals.end()) return local->_value;
	Variables::const_iterator global = globals.find(name);
	return global == globals.end() ? Value() : global->_value;
}
void Runtime::set(Common::String name, const Value &value, ScriptFrame &frame) {
	name.toLowercase();
	if (frame.locals.contains(name) || !globals.contains(name)) frame.locals[name] = value;
	else globals[name] = value;
}
bool Runtime::isRunning(const ScriptInstance &instance, const Common::String &handler) const {
	for (uint32 i = 0; i < _frames.size(); ++i)
		if (_frames[i]->instance == &instance && _frames[i]->handler.equalsIgnoreCase(handler)) return true;
	return false;
}
bool Runtime::isRunning(const Common::String &handler) const {
	for (uint32 i = 0; i < _frames.size(); ++i)
		if (_frames[i]->handler.equalsIgnoreCase(handler)) return true;
	return false;
}
bool Runtime::tick(ScriptFrame &frame) {
	if (frame.epoch != _epoch || !error.empty()) return false;
	if (!_fuel) { fail("Script instruction budget exceeded without yielding"); return false; }
	--_fuel; return true;
}
ScriptResult Runtime::run(ScriptInstance &instance, Common::String handler, const Values &args, const Common::String &target, ScriptFrame *parent, CallContext context) {
	handler.toLowercase(); ScriptResult result;
	if (parent && parent->epoch != _epoch) { result.signal = ScriptResult::kAbandoned; return result; }
	if (parent && !error.empty()) { result.signal = ScriptResult::kError; return result; }
	if (_frames.empty()) { error.clear(); _fuel = 100000; eventConsumed = false; }
	Common::HashMap<Common::String, Handler>::const_iterator found = instance.script.handlers.find(handler);
	if (found == instance.script.handlers.end()) { result.signal = ScriptResult::kPass; return result; }
	if (_frames.size() >= 64) { fail("Script call depth exceeded"); result.signal = ScriptResult::kError; return result; }
	ScriptFrame frame; frame.instance = &instance; frame.handler = handler; frame.dispatch = parent && context != kLifecycle ? parent->dispatch : handler;
	// Stage containment has a separate context at each link. In particular,
	// a button calling a flat routine must arm flat timers with the flat's me.
	const bool inheritMe = parent && context == kInherit && !(instance.ownsContext && parent->instance != &instance);
	frame.me = inheritMe ? parent->me : instance.name; frame.target = target; frame.epoch = _epoch;
	frame.event = ++_eventSequence;
	const Handler &h = found->_value;
	for (uint32 i = 0; i < h.params.size(); ++i) frame.locals[h.params[i]] = i < args.size() ? args[i] : Value();
	_frames.push_back(&frame);
	result = execute(h.body, frame); result.handled = true;
	_frames.pop_back();
	return result;
}
Values Runtime::arguments(const Expression &expression, ScriptFrame &frame) {
	Values values;
	for (uint32 i = 0; i < expression.args.size() && error.empty() && frame.epoch == _epoch; ++i) values.push_back(evaluate(expression.args[i], frame));
	return values;
}
Value Runtime::call(const Expression &expression, ScriptFrame &frame) {
	ScriptInstance &instance = *frame.instance;
	if (expression.opcode == 5 && instance.script.handlers.contains(expression.name)) {
		Values args = arguments(expression, frame);
		return run(instance, expression.name, args, frame.target, &frame).value;
	}
	Value result;
	if (_host && _host->invoke(*this, expression, frame, result)) return result;
	if (expression.opcode == 5) {
		Values args = arguments(expression, frame);
		uint32 depth = 0;
		for (ScriptInstance *p = instance.parent; p && depth++ < 64; p = p->parent)
			if (p->script.handlers.contains(expression.name)) return run(*p, expression.name, args, frame.target, &frame).value;
		if (_host && _host->resolveLibrary(*this, expression.name, args, frame, result)) return result;
		for (uint32 i = 0; i < fallbacks.size(); ++i)
			if (fallbacks[i]->script.handlers.contains(expression.name)) return run(*fallbacks[i], expression.name, args, frame.target, &frame).value;
	}
	fail(Common::String::format("Unimplemented script call %s (opcode %u)", expression.name.c_str(), expression.opcode));
	return Value();
}
Value Runtime::evaluate(uint32 index, ScriptFrame &frame) {
	if (!tick(frame)) return Value();
	if (index >= frame.instance->script.expressions.size()) { fail("Expression index outside script"); return Value(); }
	const Expression &e = frame.instance->script.expressions[index];
	switch (e.kind) {
	case Expression::kNumber: return Value(e.number);
	case Expression::kString: return Value(e.name);
	case Expression::kVariable: return get(e.name, frame);
	case Expression::kMe: return Value(frame.me);
	case Expression::kTarget: return Value(frame.target);
	case Expression::kCall: return call(e, frame);
	case Expression::kUnary: {
		Value v = evaluate(e.args[0], frame);
		return Value(e.opcode == 4023 ? !v.truth() : int32(0u - uint32(v.number())));
	}
	case Expression::kBinary: {
		Value a = evaluate(e.args[0], frame);
		if (e.opcode == 8005 && !a.truth()) return Value(0);
		if (e.opcode == 8006 && a.truth()) return Value(1);
		Value b = evaluate(e.args[1], frame);
		int32 x = a.number(), y = b.number();
		switch (e.opcode) {
		case 8001: return Value(int32(uint32(x) + uint32(y)));
		case 8002: return Value(int32(uint32(x) - uint32(y)));
		case 8003: return Value(int32(uint32(uint64(uint32(x)) * uint32(y))));
		case 8004: if (!y) { fail("Division by zero"); return Value(); } return Value(int32(int64(x) / y));
		case 8005: case 8006: return Value(b.truth() ? 1 : 0);
		case 8007: return Value(a.text() + b.text());
		case 8008: return Value(a.equals(b) ? 1 : 0);
		case 8009: return Value(a.equals(b) ? 0 : 1);
		case 8010: return Value(x > y ? 1 : 0);
		case 8011: return Value(x < y ? 1 : 0);
		case 8012: return Value(x >= y ? 1 : 0);
		case 8013: return Value(x <= y ? 1 : 0);
		}
	}
	}
	fail("Invalid script expression"); return Value();
}
ScriptResult Runtime::execute(const Common::Array<Statement> &body, ScriptFrame &frame) {
	ScriptResult result;
	for (uint32 i = 0; i < body.size(); ++i) {
		if (!tick(frame)) { result.signal = frame.epoch != _epoch ? ScriptResult::kAbandoned : ScriptResult::kError; return result; }
		result = statement(body[i], frame);
		if (!error.empty()) result.signal = ScriptResult::kError;
		if (result.signal != ScriptResult::kNormal) return result;
	}
	return result;
}
ScriptResult Runtime::statement(const Statement &s, ScriptFrame &frame) {
	ScriptResult result;
	switch (s.kind) {
	case Statement::kNoop: break;
	case Statement::kDeclaration:
		for (uint32 i = 0; i < s.names.size(); ++i) {
			const Common::String &name = s.names[i];
			if (s.opcode == 4029) globals.erase(name);
			else if (s.opcode == 4002) { if (!globals.contains(name)) globals[name] = Value(); }
			else if (!frame.locals.contains(name)) frame.locals[name] = Value();
		}
		break;
	case Statement::kAssign: {
		Value value = evaluate(s.expressions[0], frame);
		// A load/restart inside the expression invalidates this entire chain.
		// Never write its result back into the replacement game's globals.
		if (frame.epoch == _epoch && error.empty()) set(s.name, value, frame);
		break;
	}
	case Statement::kCall: evaluate(s.expressions[0], frame); break;
	case Statement::kIf: return execute(evaluate(s.expressions[0], frame).truth() ? s.body : s.alternate, frame);
	case Statement::kSwitch: {
		Value subject = evaluate(s.expressions[0], frame);
		for (uint32 i = 0; i < s.body.size(); ++i) {
			if (!subject.equals(evaluate(s.body[i].expressions[0], frame))) continue;
			while (i + 1 < s.body.size() && s.body[i].body.empty()) ++i;
			return execute(s.body[i].body, frame);
		}
		break;
	}
	case Statement::kWhile:
		while (tick(frame) && evaluate(s.expressions[0], frame).truth()) {
			result = execute(s.body, frame); if (result.signal != ScriptResult::kNormal) return result;
		}
		break;
	case Statement::kFor: {
		int32 start = evaluate(s.expressions[0], frame).number(), end = evaluate(s.expressions[1], frame).number();
		int32 step = s.expressions.size() > 2 ? evaluate(s.expressions[2], frame).number() : 1;
		if (!step) { fail("For loop step is zero"); break; }
		for (int64 i = start; (step > 0 ? i <= end : i >= end) && tick(frame); i += step) {
			set(s.name, Value(int32(i)), frame); result = execute(s.body, frame);
			if (result.signal != ScriptResult::kNormal) return result;
		}
		break;
	}
	case Statement::kExit:
		if (frame.handler == frame.dispatch) eventConsumed = true;
		result.signal = ScriptResult::kExit; break;
	case Statement::kPass: result.signal = ScriptResult::kPass; break;
	case Statement::kReturn:
		result.signal = ScriptResult::kReturn;
		if (!s.expressions.empty()) result.value = evaluate(s.expressions[0], frame);
		break;
	case Statement::kCase: fail("Case executed outside switch"); break;
	}
	if (frame.epoch != _epoch) result.signal = ScriptResult::kAbandoned;
	return result;
}
}
