/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_RUNTIME_H
#define DREAMFACTORY_RUNTIME_H
#include "dreamfactory/script.h"
namespace DreamFactory {
struct Value {
	bool isString = false;
	int32 integer = 0;
	Common::String string;
	Value(int32 n = 0) : integer(n) {}
	Value(const Common::String &s) : isString(true), string(s) {}
	bool truth() const { return isString ? !string.empty() : integer != 0; }
	int32 number() const;
	Common::String text() const;
	bool equals(const Value &other) const;
};
typedef Common::Array<Value> Values;
typedef Common::HashMap<Common::String, Value> Variables;
struct ScriptInstance {
	Common::String name;
	Script script;
	ScriptInstance *parent = nullptr;
	bool ownsContext = false;
};
struct ScriptFrame {
	ScriptInstance *instance = nullptr;
	Variables locals;
	Common::String me, target, handler, dispatch;
	uint32 epoch = 0;
	uint64 event = 0;
};
struct ScriptResult {
	enum Signal { kNormal, kExit, kPass, kReturn, kAbandoned, kError } signal = kNormal;
	Value value;
	bool handled = false;
};
class Runtime;
class ScriptHost {
public:
	virtual ~ScriptHost() {}
	// Arguments remain expressions so sendto* can evaluate its nested call
	// against the target object, rather than accidentally against the caller.
	virtual bool invoke(Runtime &runtime, const Expression &call, ScriptFrame &frame, Value &result) = 0;
	virtual bool resolveLibrary(Runtime &runtime, const Common::String &name, const Values &args, ScriptFrame &frame, Value &result) { return false; }
};
class Runtime {
public:
	enum CallContext { kInherit, kAddressed, kLifecycle };
	explicit Runtime(ScriptHost *host = nullptr) : _host(host) {}
	Variables globals;
	Common::Array<ScriptInstance *> fallbacks;
	Common::String error;
	bool eventConsumed = false;
	ScriptResult run(ScriptInstance &instance, Common::String handler, const Values &args = Values(), const Common::String &target = Common::String(), ScriptFrame *parent = nullptr, CallContext context = kInherit);
	Value evaluate(uint32 expression, ScriptFrame &frame);
	Values arguments(const Expression &call, ScriptFrame &frame);
	Value get(Common::String name, const ScriptFrame &frame) const;
	void set(Common::String name, const Value &value, ScriptFrame &frame);
	void abandon() { ++_epoch; }
	void yielded() { _fuel = 100000; }
	bool isRunning(const ScriptInstance &instance, const Common::String &handler) const;
	bool isRunning(const Common::String &handler) const;
	bool current(const ScriptFrame &frame) const { return frame.epoch == _epoch && error.empty(); }
	bool busy() const { return !_frames.empty(); }
	ScriptFrame eventFrame(const Common::String &me, const Common::String &dispatch) const { ScriptFrame frame; frame.me = me; frame.dispatch = dispatch; frame.epoch = _epoch; return frame; }
	void fail(const Common::String &message) { if (error.empty()) error = message; }
private:
	friend class SaveState;
	ScriptHost *_host;
	uint32 _epoch = 0, _fuel = 100000;
	uint64 _eventSequence = 0;
	Common::Array<ScriptFrame *> _frames;
	bool tick(ScriptFrame &frame);
	ScriptResult execute(const Common::Array<Statement> &body, ScriptFrame &frame);
	ScriptResult statement(const Statement &statement, ScriptFrame &frame);
	Value call(const Expression &expression, ScriptFrame &frame);
};
}
#endif
