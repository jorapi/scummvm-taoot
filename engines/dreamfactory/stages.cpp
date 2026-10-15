/* SPDX-License-Identifier: GPL-3.0-or-later
 * Stage and flat lifecycle/containment follow runtime/stage.ts.
 */
#include "dreamfactory/stages.h"
namespace DreamFactory {
static Common::SharedPtr<ScriptInstance> stageScript(const Bytes &data, Common::String name, Runtime &runtime) {
	Common::SharedPtr<ScriptInstance> script(new ScriptInstance()); name.toLowercase(); script->name = name; script->ownsContext = true;
	ScriptTokens tokens; Common::String error;
	if (!decodeScript(data, tokens, error)) return script;
	bool handlers = false; for (uint32 i = 0; i < tokens.size(); ++i) if (tokens[i].opcode == 4001) handlers = true;
	if (handlers && !parseScript(tokens, script->script, error)) runtime.fail("Stage script " + name + ": " + error);
	return script;
}
int32 StageSystem::resolve(const Common::String &name) const {
	if (!current) return -1;
	const int32 named = current->data.findFlat(name); if (named >= 0) return named;
	if (name.empty()) return -1;
	uint32 index = 0;
	for (uint32 i = 0; i < name.size(); ++i) { if (name[i] < '0' || name[i] > '9' || index > 100000) return -1; index = index * 10 + name[i] - '0'; }
	return index > 0 && index <= current->data.flats.size() ? int32(index - 1) : -1;
}
Common::String StageSystem::currentFlat() const { return current && flat >= 0 ? current->data.flats[flat].name : Common::String("none"); }
ScriptResult StageSystem::flatEvent(const Common::SharedPtr<LoadedStage> &loaded, int32 index, const Common::String &handler, ScriptFrame *parent) {
	// Lifecycle messages use the same flat -> stage containment as explicit
	// messages (DF.EXE 0x416ed0). YUNNIBOX.FLT keeps its shared open/close
	// handlers in the stage, while the picture flats have empty scripts.
	ScriptResult result = _runtime.run(*loaded->flats[index], handler, Values(), loaded->data.flats[index].name, parent, Runtime::kLifecycle);
	if (result.signal == ScriptResult::kPass)
		result = _runtime.run(*loaded->main, handler, Values(), loaded->data.flats[index].name, parent, Runtime::kLifecycle);
	return result;
}
bool StageSystem::open(Common::String name, ScriptFrame *parent, bool callbacks, const Common::String &source) {
	name.toLowercase(); if (!name.hasSuffix(".stg") && !name.hasSuffix(".flt")) name += version == 1 ? ".flt" : ".stg";
	if (current && current->name == name) return true;
	const auto previous = current; if (callbacks) close(parent); else { current.reset(); flat = -1; ++_revision; }
	if (parent && !_runtime.current(*parent)) return false;
	// A close callback may replace the stage. Its navigation wins.
	if (current && current != previous) return false;
	Bytes bytes; if (!_resources.read(source.empty() ? name : source, bytes)) return false;
	Common::SharedPtr<LoadedStage> loaded(new LoadedStage()); loaded->name = name; loaded->source = _resources.sourceName(source.empty() ? name : source);
	if (!loaded->data.load(bytes)) { _runtime.fail("Invalid stage: " + name); return false; }
	loaded->main = stageScript(loaded->data.archive.get(loaded->data.mainScript), name, _runtime);
	for (uint32 f = 0; f < loaded->data.flats.size(); ++f) {
		const Flat &data = loaded->data.flats[f];
		auto script = stageScript(loaded->data.archive.get(data.script), data.name, _runtime); script->parent = loaded->main.get(); loaded->flats.push_back(script);
		Common::Array<Common::SharedPtr<ScriptInstance> > buttons;
		for (uint32 b = 0; b < data.regions.size(); ++b) {
			auto button = stageScript(loaded->data.archive.get(data.regions[b].script), data.regions[b].name, _runtime); button->parent = script.get(); buttons.push_back(button);
		}
		loaded->buttons.push_back(buttons);
	}
	if (!_runtime.error.empty()) return false;
	current = loaded; flat = loaded->data.version == 1 && !loaded->data.flats.empty() ? 0 : -1; const uint32 revision = ++_revision;
	if (!callbacks) { flat = loaded->data.flats.empty() ? -1 : 0; return true; }
	const ScriptResult result = _runtime.run(*loaded->main, "openstage", Values(), Common::String(), parent, Runtime::kLifecycle);
	if (result.signal == ScriptResult::kAbandoned || !_runtime.error.empty()) return false;
	if (loaded->data.version == 1) {
		// DF.EXE 0x4158cb selects flat zero before openstage. 0x415d30
		// then opens that flat if the callback retained the file and index.
		// SCORP.FLT arms its initial flat timer from inside openstage.
		if (current == loaded && flat == 0) {
			if (_host) _host->flatChanged();
			const ScriptResult opened = flatEvent(loaded, 0, "openflat", parent);
			if (opened.signal == ScriptResult::kAbandoned) return false;
		}
	} else if (current == loaded && _revision == revision && flat < 0 && !loaded->data.flats.empty()) go(loaded->data.flats[0].name, parent);
	return _runtime.error.empty();
}
void StageSystem::close(ScriptFrame *parent) {
	const auto loaded = current; if (!loaded || loaded->closing) return;
	loaded->closing = true;
	const ScriptResult result = _runtime.run(*loaded->main, "closestage", Values(), Common::String(), parent, Runtime::kLifecycle);
	if (result.signal == ScriptResult::kAbandoned || (parent && !_runtime.current(*parent))) { loaded->closing = false; return; }
	if (current != loaded) { loaded->closing = false; return; }
	const int32 outgoing = flat; flat = -1; const uint32 revision = ++_revision;
	if (outgoing >= 0) {
		const ScriptResult closed = flatEvent(loaded, outgoing, "closeflat", parent);
		if (closed.signal == ScriptResult::kAbandoned || !_runtime.error.empty()) { loaded->closing = false; return; }
	}
	loaded->closing = false;
	if (current == loaded && _revision == revision && (!parent || _runtime.current(*parent))) reset();
}
bool StageSystem::go(const Common::String &name, ScriptFrame *parent) {
	const int32 target = resolve(name); if (target < 0) { _runtime.fail("Flat not found: " + name); return false; }
	const auto loaded = current; const uint32 revision = ++_revision;
	const int32 outgoing = flat;
	// Clear first so a closeflat callback can navigate without closing itself
	// recursively. The revision guard preserves that callback's destination.
	flat = -1;
	if (outgoing >= 0) {
		const ScriptResult result = flatEvent(loaded, outgoing, "closeflat", parent);
		if (result.signal == ScriptResult::kAbandoned || !_runtime.error.empty()) return false;
	}
	if (current != loaded || _revision != revision) return false;
	flat = target;
	if (_host) _host->flatChanged();
	const ScriptResult result = flatEvent(loaded, target, "openflat", parent);
	return result.signal != ScriptResult::kAbandoned && _runtime.error.empty();
}
bool StageSystem::running(const Common::String &name, const Common::String &nameOfHandler) const {
	Common::String handler = nameOfHandler; handler.toLowercase();
	if (!current) return false; const int32 index = resolve(name);
	for (const ScriptInstance *p = index >= 0 ? current->flats[index].get() : current->main.get(); p; p = p->parent) if (p->script.handlers.contains(handler)) return _runtime.isRunning(*p, handler);
	for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) if (_runtime.fallbacks[b]->script.handlers.contains(handler)) return _runtime.isRunning(*_runtime.fallbacks[b], handler);
	return false;
}
Value StageSystem::send(bool stage, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent) {
	const auto loaded = current; if (!loaded) return Value();
	Common::Array<ScriptInstance *> chain; const int32 index = stage ? -1 : resolve(name);
	if (!stage && index < 0) return Value();
	ScriptInstance *own = stage ? loaded->main.get() : loaded->flats[index].get();
	if (own->script.handlers.empty()) own = loaded->main.get();
	chain.push_back(own); if (own != loaded->main.get()) chain.push_back(loaded->main.get());
	if (rootEvent || parent.dispatch != handler) for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b)
		if (!_runtime.isRunning(*_runtime.fallbacks[b], handler)) chain.push_back(_runtime.fallbacks[b]);
	Value value;
	for (uint32 i = 0; i < chain.size(); ++i) if (chain[i]->script.handlers.contains(handler)) {
		ScriptFrame addressed = parent; addressed.me = own->name; if (rootEvent) addressed.dispatch = handler;
		const ScriptResult result = _runtime.run(*chain[i], handler, args, stage ? parent.me : name, &addressed); value = result.value;
		if (result.signal == ScriptResult::kError || result.signal == ScriptResult::kAbandoned || (result.handled && result.signal != ScriptResult::kPass)) break;
	}
	return value;
}
Value StageSystem::button(const Common::String &flatName, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent) {
	const auto loaded = current; const int32 index = resolve(flatName); if (!loaded || index < 0) return Value();
	int32 button = -1; for (uint32 b = 0; b < loaded->data.flats[index].regions.size(); ++b) if (loaded->data.flats[index].regions[b].name.equalsIgnoreCase(name)) { button = b; break; }
	if (button < 0) return Value();
	Common::Array<ScriptInstance *> chain; chain.push_back(loaded->buttons[index][button].get()); chain.push_back(loaded->flats[index].get()); chain.push_back(loaded->main.get());
	for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) if (!_runtime.isRunning(*_runtime.fallbacks[b], handler)) chain.push_back(_runtime.fallbacks[b]);
	for (uint32 i = 0; i < chain.size(); ++i) if (chain[i]->script.handlers.contains(handler)) {
		ScriptFrame addressed = parent; addressed.me = name;
		const ScriptResult result = _runtime.run(*chain[i], handler, args, name, &addressed); return result.value;
	}
	return Value();
}
}
