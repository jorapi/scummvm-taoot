/* SPDX-License-Identifier: GPL-3.0-or-later
 * Room binding and lifecycle follow runtime/setscripts.ts and web/viewer.ts.
 */
#include "dreamfactory/rooms.h"
namespace DreamFactory {
static Common::SharedPtr<ScriptInstance> roomScript(const Room &room, uint32 location, Common::String name, Runtime &runtime) {
	auto instance = Common::SharedPtr<ScriptInstance>(new ScriptInstance()); name.toLowercase(); instance->name = name;
	if (!location) return instance;
	ScriptTokens tokens; Common::String error;
	if (!decodeScript(room.archive.get(location), tokens, error)) { runtime.fail("Room script " + name + ": " + error); return instance; }
	if (!parseScript(tokens, instance->script, error)) runtime.fail("Room script " + name + ": " + error);
	return instance;
}
void RoomSystem::reset() {
	current.reset(); scene = view = -1; moving = turning = false; visible = true; frame = Frame(); camera = CameraPose(); ++_revision; _hasFacing = false; _host.bindRoom(nullptr);
}
Common::String RoomSystem::name() const { return current ? current->main->name : Common::String("none"); }
Common::String RoomSystem::sceneName() const { return current && scene >= 0 ? current->data.scenes[scene].name : Common::String("none"); }
Common::String RoomSystem::viewName() const {
	if (moving) return Common::String("moving");
	return current && scene >= 0 && view >= 0 ? current->data.scenes[scene].views[view].name : Common::String("none");
}
int32 RoomSystem::sceneIndex(const Common::String &name) const {
	if (current) for (uint32 i = 0; i < current->data.scenes.size(); ++i) if (current->data.scenes[i].name.equalsIgnoreCase(name)) return i;
	return -1;
}
int32 RoomSystem::viewIndex(int32 index, const Common::String &name) const {
	if (current && index >= 0 && uint32(index) < current->data.scenes.size()) for (uint32 i = 0; i < current->data.scenes[index].views.size(); ++i) if (current->data.scenes[index].views[i].name.equalsIgnoreCase(name)) return i;
	return -1;
}
Value RoomSystem::dispatch(const Common::SharedPtr<LoadedRoom> &loaded, Common::Array<ScriptInstance *> chain, const Common::String &handler, const Values &args, const Common::String &target, ScriptFrame *parent, bool ownContext, bool boot) {
	// Keep the stage alive if a handler closes it while its chain is in flight.
	const auto stage = _stages.current;
	if (stage) chain.push_back(stage->main.get());
	const bool key = handler == "keydown" || handler == "keyrepeat";
	if (boot && (ownContext || key || !parent || parent->dispatch != handler)) for (uint32 i = 0; i < _runtime.fallbacks.size(); ++i) if (!_runtime.isRunning(*_runtime.fallbacks[i], handler)) chain.push_back(_runtime.fallbacks[i]);
	Value value;
	for (uint32 i = 0; i < chain.size(); ++i) {
		if (!chain[i] || !chain[i]->script.handlers.contains(handler)) continue;
		_runtime.eventConsumed = false;
		const ScriptResult result = _runtime.run(*chain[i], handler, args, target, parent, ownContext ? Runtime::kLifecycle : Runtime::kAddressed); value = result.value;
		if (result.signal == ScriptResult::kAbandoned || result.signal == ScriptResult::kError || result.signal == ScriptResult::kExit || (!key && result.handled && result.signal != ScriptResult::kPass)) break;
	}
	return value;
}
bool RoomSystem::lifecycle(const Common::SharedPtr<LoadedRoom> &loaded, const Common::String &handler, int32 index, ScriptFrame *parent) {
	Common::Array<ScriptInstance *> chain; if (index >= 0) chain.push_back(loaded->scenes[index].get()); chain.push_back(loaded->main.get());
	++loaded->lifecycle;
	// Lifecycle dispatch is a fresh event, even when the initiating command
	// happens to be inside another event with the same name.
	ScriptFrame context = parent ? *parent : _runtime.eventFrame("", ""); context.dispatch.clear();
	dispatch(loaded, chain, handler, Values(), "", &context, true, true); --loaded->lifecycle;
	return _runtime.current(context);
}
bool RoomSystem::open(Common::String file, const Common::String &sceneName, const Common::String &viewName, ScriptFrame *parent, bool callbacks, const Common::String &source) {
	file.toLowercase(); if (!file.hasSuffix(".set")) file += ".set";
	const auto previous = current; if (callbacks) close(parent); else reset(); if (parent && !_runtime.current(*parent)) return false;
	if (current && current != previous) return false; // navigation from a departing callback wins
	Bytes bytes; if (!_resources.read(source.empty() ? file : source, bytes)) { _runtime.fail("Cannot open room: " + file); return false; }
	auto loaded = Common::SharedPtr<LoadedRoom>(new LoadedRoom()); loaded->file = file; loaded->source = _resources.sourceName(source.empty() ? file : source);
	if (!loaded->data.load(bytes)) { _runtime.fail("Invalid room: " + file); return false; }
	const Room &room = loaded->data;
	loaded->main = roomScript(room, room.mainScript, room.name.empty() ? file.substr(0, file.size() - 4) : room.name, _runtime);
	for (uint32 s = 0; s < room.scenes.size(); ++s) {
		auto script = roomScript(room, room.scenes[s].script, room.scenes[s].name, _runtime); script->parent = loaded->main.get(); loaded->scenes.push_back(script);
		Common::Array<Common::Array<Common::SharedPtr<ScriptInstance> > > views;
		for (uint32 v = 0; v < room.scenes[s].views.size(); ++v) {
			Common::Array<Common::SharedPtr<ScriptInstance> > paints;
			for (uint32 p = 0; p < room.scenes[s].views[v].paintings.size(); ++p) {
				const Painting &paint = room.scenes[s].views[v].paintings[p]; auto inst = roomScript(room, paint.script, paint.name, _runtime); inst->parent = loaded->main.get(); paints.push_back(inst);
			}
			views.push_back(paints);
		}
		loaded->paintings.push_back(views);
	}
	if (!_runtime.error.empty()) return false;
	current = loaded; scene = view = -1; moving = turning = false; ++_revision;
	// Each original loader initializes the new room's visibility before callbacks:
	// DF.EXE 0x421b80, retail TI.EXE 0x4077e5, demo TI.EXE 0x4083c9.
	// A hidden overlay or old room must not keep the arriving room hidden.
	visible = true;
	if (callbacks) _host.bindRoom(&room);
	int32 s = sceneIndex(sceneName.empty() ? room.defaultScene : sceneName);
	if (s < 0 || room.scenes[s].views.empty()) for (uint32 i = 0; i < room.scenes.size(); ++i) if (!room.scenes[i].views.empty()) { s = i; break; }
	int32 v = viewIndex(s, viewName.empty() ? room.defaultView : viewName);
	if (s < 0 || !position(s, v, callbacks)) return false;
	if (!callbacks) return true;
	if (!lifecycle(loaded, "openset", -1, parent) || current != loaded) return _runtime.error.empty();
	return lifecycle(loaded, "openscene", scene, parent);
}
void RoomSystem::close(ScriptFrame *parent) {
	const auto loaded = current; if (!loaded || loaded->closing) return;
	loaded->closing = true; const uint32 revision = ++_revision;
	if (scene >= 0 && !lifecycle(loaded, "closescene", scene, parent)) { loaded->closing = false; return; }
	if (current != loaded || revision != _revision) { loaded->closing = false; return; }
	if (!lifecycle(loaded, "closeset", -1, parent)) { loaded->closing = false; return; }
	loaded->closing = false;
	if (current == loaded && revision == _revision) { current.reset(); scene = view = -1; moving = turning = false; frame = Frame(); camera = CameraPose(); ++_revision; _host.bindRoom(nullptr); }
}
bool RoomSystem::worldCamera(WorldCamera &out, int32 heightBias) const {
	if (!current || scene < 0 || view < 0) return false;
	const Room &room = current->data;
	const double angle = (camera.degree & 255) * M_PI / 128;
	out.x = camera.x - int32(floor(room.cameraSetback * cos(angle) + 0.5));
	out.y = camera.y - int32(floor(room.cameraSetback * sin(angle) + 0.5));
	out.z = camera.z + heightBias; out.degree = camera.degree;
	out.width = room.width; out.height = room.height;
	out.focal = room.version == 1 ? 310 : double(MAX(room.width, room.height)) / 2;
	return true;
}
bool RoomSystem::position(int32 s, int32 v, bool notify) {
	if (!current || s < 0 || uint32(s) >= current->data.scenes.size()) return false;
	const Scene &data = current->data.scenes[s]; if (data.views.empty()) return false;
	if (v < 0 || uint32(v) >= data.views.size()) {
		v = 0;
		if (_hasFacing) { int32 best = 257; for (uint32 i = 0; i < data.views.size(); ++i) { const int32 d = (data.views[i].facing - _lastFacing) & 255, distance = MIN<int32>(d, 256 - d); if (distance < best) { best = distance; v = i; } } }
	}
	scene = s; view = v; moving = turning = false; _lastFacing = data.views[v].facing & 255; _hasFacing = true;
	if (notify) _host.roomListener(data, data.views[v]);
	if (!current->data.stand(s, v, frame, &camera)) { _runtime.fail("Invalid room standpoint image"); return false; }
	if (notify && visible) _host.presentRoom(current->data, frame);
	return true;
}
bool RoomSystem::jump(const Common::String &sceneName, const Common::String &viewName, ScriptFrame *parent) {
	const int32 s = sceneName.empty() ? scene : sceneIndex(sceneName); if (s < 0 || !current || current->data.scenes[s].views.empty()) return false;
	const auto loaded = current; const int32 v = viewName.empty() && s == scene ? view : viewIndex(s, viewName);
	const uint32 revision = ++_revision;
	if (!loaded->lifecycle && scene >= 0 && !lifecycle(loaded, "closescene", scene, parent)) return false;
	if (current != loaded || revision != _revision || !position(s, v)) return false;
	return loaded->lifecycle || lifecycle(loaded, "openscene", scene, parent);
}
bool RoomSystem::animate(const Sequence &sequence, ScriptFrame *parent, uint32 revision) {
	const auto loaded = current;
	for (uint32 i = 0; i < sequence.size(); ++i) {
		if (current != loaded || revision != _revision || (parent && !_runtime.current(*parent))) return false;
		camera = sequence[i].camera;
		if (sequence[i].image) {
			if (!decodeFrame(loaded->data.archive.get(sequence[i].image), frame)) { _runtime.fail("Invalid room motion image"); return false; }
			if (visible) _host.presentRoom(loaded->data, frame);
		}
		if (!_host.waitRoomFrame(parent)) return false;
	}
	return current == loaded && revision == _revision;
}
bool RoomSystem::move(int direction, ScriptFrame *parent) {
	if (!current || moving || scene < 0 || view < 0 || direction < 0 || direction > 2) return false;
	const auto loaded = current; const Room &room = loaded->data; int32 s = scene, v = -1; Sequence sequence;
	const int32 id = room.scenes[scene].views[view].id;
	if (room.version == 1) {
		for (uint32 i = 0; i < room.moves.size(); ++i) if (room.moves[i].from == id && room.moves[i].direction == direction) {
			for (uint32 a = 0; a < room.scenes.size(); ++a) for (uint32 b = 0; b < room.scenes[a].views.size(); ++b) if (room.scenes[a].views[b].id == room.moves[i].to) { s = a; v = b; sequence = room.moves[i].frames; }
			break;
		}
	} else if (direction < 2) {
		const Sequence &ring = room.scenes[scene].turns[direction];
		for (uint32 i = 0; i < ring.size(); ++i) if (ring[i].view == view && ring[i].motion > 0) {
			for (uint32 n = 1; n <= ring.size(); ++n) { const MotionFrame &f = ring[(i + n) % ring.size()]; sequence.push_back(f); if (f.view >= 0 && f.motion > 0) { v = f.view; break; } } break;
		}
	} else {
		for (uint32 i = 0; i < room.roads.size(); ++i) {
			const Road &road = room.roads[i]; const int32 dir = road.from == id ? 0 : road.to == id ? 1 : -1; if (dir < 0 || road.walks[dir].empty()) continue;
			room.roadSequence(road, dir, sequence, s, v);
			break;
		}
	}
	if (v < 0 || sequence.empty()) return false;
	const uint32 revision = ++_revision;
	if (!lifecycle(loaded, "closescene", scene, parent) || current != loaded || revision != _revision) return false;
	frame = Frame(); camera = CameraPose();
	if (room.version == 4 && direction < 2) {
		const Sequence &ring = room.scenes[scene].turns[direction];
		for (uint32 i = 0; i < ring.size(); ++i) {
			if (ring[i].image && !decodeFrame(room.archive.get(ring[i].image), frame)) { _runtime.fail("Invalid room turn prelude"); return false; }
			if (ring[i].view == view && ring[i].motion > 0) break;
		}
	}
	moving = true; turning = direction < 2;
	if (!animate(sequence, parent, revision)) { if (current == loaded && revision == _revision) moving = turning = false; return false; }
	if (!position(s, v)) return false;
	return lifecycle(loaded, "openscene", scene, parent);
}
bool RoomSystem::roadAhead(int32 s, int32 v) const {
	if (!current || s < 0 || v < 0) return false;
	const Room &room = current->data; const int32 id = room.scenes[s].views[v].id;
	if (room.version == 1) { for (uint32 i = 0; i < room.moves.size(); ++i) if (room.moves[i].from == id && room.moves[i].direction == 2) return true; }
	else for (uint32 i = 0; i < room.roads.size(); ++i) if (room.roads[i].from == id || room.roads[i].to == id) return true;
	return false;
}
Value RoomSystem::send(bool set, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent) {
	const auto loaded = current; if (!loaded) return Value(); const int32 s = set ? -1 : sceneIndex(name); if (!set && s < 0) return Value();
	Common::Array<ScriptInstance *> chain; if (s >= 0) chain.push_back(loaded->scenes[s].get()); chain.push_back(loaded->main.get());
	ScriptFrame context = parent; if (rootEvent) context.dispatch = handler;
	return dispatch(loaded, chain, handler, args, set ? parent.me : name, &context, false, handler == "keydown" || handler == "keyrepeat");
}
Value RoomSystem::painting(const Common::String &sceneName, const Common::String &viewName, const Common::String &paintName, const Common::String &handler, const Values &args, ScriptFrame &parent) {
	const auto loaded = current; const int32 s = sceneIndex(sceneName), v = viewIndex(s, viewName); if (!loaded || s < 0 || v < 0) return Value();
	const auto &paints = loaded->data.scenes[s].views[v].paintings;
	for (uint32 p = 0; p < paints.size(); ++p) if (paints[p].name.equalsIgnoreCase(paintName)) {
		Common::Array<ScriptInstance *> chain; chain.push_back(loaded->paintings[s][v][p].get()); chain.push_back(loaded->scenes[s].get()); chain.push_back(loaded->main.get());
		return dispatch(loaded, chain, handler, args, paintName, &parent, false, false);
	}
	return Value();
}
bool RoomSystem::running(const Common::String &name, const Common::String &nameOfHandler) const {
	if (!current) return false;
	Common::String handler = nameOfHandler; handler.toLowercase();
	Common::Array<ScriptInstance *> chain; const int32 s = sceneIndex(name);
	if (s >= 0) chain.push_back(current->scenes[s].get());
	chain.push_back(current->main.get());
	if (_stages.current) chain.push_back(_stages.current->main.get());
	for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) chain.push_back(_runtime.fallbacks[b]);
	for (uint32 i = 0; i < chain.size(); ++i) if (chain[i]->script.handlers.contains(handler)) return _runtime.isRunning(*chain[i], handler);
	return false;
}
}
