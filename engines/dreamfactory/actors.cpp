/* SPDX-License-Identifier: GPL-3.0-or-later
 * Lifecycle, instance defaults and dispatch follow dreamREfactory's
 * runtime/actors.ts, session-resources.ts and session-events.ts.
 */
#include "dreamfactory/actors.h"
namespace DreamFactory {
bool CastSystem::warm(const Common::String &name, bool wholeCast) {
	Common::String key = name;
	if (wholeCast && key.hasSuffixIgnoreCase(".cst")) key = key.substr(0, key.size() - 4);
	for (const auto &actor : actors) {
		if (!actor->cast || actor->member >= actor->cast->data.members.size()) continue;
		Common::String castName = actor->cast->name;
		if (castName.hasSuffixIgnoreCase(".cst")) castName = castName.substr(0, castName.size() - 4);
		if (!(wholeCast ? castName.equalsIgnoreCase(key) : actor->name.equalsIgnoreCase(key))) continue;
		// DF.EXE 0x428680 visits every pose and picture in the member.
		// Copies share this immutable artwork cache, without changing pose,
		// playback step, placement, visibility or the animation clock.
		for (const auto &pose : actor->cast->data.members[actor->member].poses)
			for (const auto &step : pose.steps)
				for (const auto &picture : step) if (!actor->cast->frame(picture.location)) {
					_runtime.fail("Cannot preload actor sprite: " + actor->name); return false;
				}
	}
	return true;
}
const Sprite *LoadedCast::frame(uint32 location) {
	if (!frameCache.contains(location)) { Sprite sprite; if (!decodeSprite(data.archive.get(location), sprite)) return nullptr; frameCache[location] = sprite; }
	return &frameCache[location];
}
const CastPose *Actor::poseData() const {
	if (!cast || member >= cast->data.members.size()) return nullptr;
	const auto &poses = cast->data.members[member].poses;
	for (uint32 p = 0; p < poses.size(); ++p) if (poses[p].name.equalsIgnoreCase(pose)) return &poses[p];
	return poses.empty() ? nullptr : &poses[0];
}
const CastFrame *Actor::frameRecord(const WorldCamera *camera) const {
	const auto data = poseData(); if (!data || data->play.empty()) return nullptr;
	const uint32 index = data->play[step % data->play.size()]; if (index >= data->steps.size() || data->steps[index].empty()) return nullptr;
	const auto &frames = data->steps[index];
	const uint32 relative = camera ? (uint32(deg) - uint32(worldBearing(double(camera->x) - x, double(camera->y) - y))) & 255 : uint32(deg) & 255;
	uint32 best = 0, distance = 257;
	for (uint32 f = 0; f < frames.size(); ++f) {
		const uint32 diff = (uint32(frames[f].angle) - relative) & 255, apart = MIN(diff, 256 - diff);
		if (apart < distance) { best = f; distance = apart; }
	}
	return &frames[best];
}
int32 Actor::referenceScale() const {
	const auto data = poseData();
	return data && !data->steps.empty() && !data->steps[0].empty() && data->steps[0][0].refScale ? data->steps[0][0].refScale : 96;
}
void CastSystem::animate() {
	for (const auto &actor : actors) {
		const auto pose = actor->poseData(); if (!pose) continue;
		if (!pose->play.empty()) actor->step = (actor->step + 1) % pose->play.size();
		else if (pose->steps.empty() && !actor->step) actor->step = 1;
	}
}
Common::Array<ActorDrawEntry> CastSystem::drawList(const WorldCamera &camera, const Common::String &set) const {
	Common::Array<ActorDrawEntry> list;
	for (const auto &actor : actors) {
		if (!actor->visible || !actor->worldSpace || actor->scale <= 0 || (!actor->set.empty() && !actor->set.equalsIgnoreCase(set))) continue;
		ActorDrawEntry entry; entry.actor = actor; if (!projectPoint(camera, actor->x, actor->y, actor->z, entry.point)) continue;
		uint32 at = list.size(); list.push_back(entry);
		while (at && list[at - 1].point.depth < entry.point.depth) { list[at] = list[at - 1]; --at; }
		list[at] = entry;
	}
	return list;
}
struct ActorRect {
	const Sprite *sprite = nullptr;
	double scale = 1, x = 0, y = 0, width = 0, height = 0;
	bool texel(int32 px, int32 py, uint32 &source) const {
		if (!sprite || scale <= 0 || px < x || py < y || px >= x + width || py >= y + height) return false;
		const uint32 sx = uint32(MIN<double>(sprite->width - 1, floor((px - x) / scale))), sy = uint32(MIN<double>(sprite->height - 1, floor((py - y) / scale)));
		source = sy * sprite->width + sx; return sprite->opaque[source] != 0;
	}
};
static ActorRect actorRect(const Actor &actor, const WorldCamera *camera, const ProjectedPoint *point) {
	ActorRect rect; const auto frame = actor.frameRecord(camera); if (!frame) return rect;
	rect.sprite = actor.cast->frame(frame->location); if (!rect.sprite || !rect.sprite->width || !rect.sprite->height) return rect;
	rect.scale = point ? double(actor.scale) * actor.referenceScale() / (1000.0 * point->depth) : 1;
	if (rect.scale <= 0) return rect;
	rect.x = (point ? point->x : actor.anchorX) - floor(rect.sprite->x * rect.scale + 0.5);
	rect.y = (point ? point->y : actor.anchorY) - floor(rect.sprite->y * rect.scale + 0.5);
	rect.width = MAX<double>(1, floor(rect.sprite->width * rect.scale + 0.5)); rect.height = MAX<double>(1, floor(rect.sprite->height * rect.scale + 0.5)); return rect;
}
static bool drawActorRect(const ActorRect &rect, Bytes &rgba, uint32 width, uint32 height, const byte *palette, uint32 clipW, uint32 clipH, const Occlusion &occlusion, double depth) {
	if (!width || !height || width > 4096 || height > 4096 || rgba.size() != width * height * 4) return false;
	if (!rect.sprite || rect.scale <= 0 || !rect.sprite->width || !rect.sprite->height) return true;
	const int32 left = int32(CLIP<double>(rect.x, 0, MIN(width, clipW))), top = int32(CLIP<double>(rect.y, 0, MIN(height, clipH)));
	const double right = MIN<double>(MIN(width, clipW), rect.x + rect.width), bottom = MIN<double>(MIN(height, clipH), rect.y + rect.height);
	for (int32 y = top; y < bottom; ++y) for (int32 x = left; x < right; ++x) {
		uint32 source; if (!rect.texel(x, y, source) || occlusion.hides(x, y, depth)) continue;
		memcpy(rgba.data() + (y * width + x) * 4, palette + rect.sprite->pixels[source] * 3, 3);
	}
	return true;
}
bool CastSystem::compositeOne(const ActorDrawEntry &entry, Bytes &rgba, uint32 width, uint32 height, const byte *palette, const WorldCamera &camera, const Occlusion &occlusion) {
	return drawActorRect(actorRect(*entry.actor, &camera, &entry.point), rgba, width, height, palette, camera.width, camera.height, occlusion, double(entry.point.depth) - entry.actor->zclip + occlusion.bias);
}
static Common::Array<Common::SharedPtr<Actor> > screenActors(const Common::Array<Common::SharedPtr<Actor> > &actors, const Common::String &set) {
	Common::Array<Common::SharedPtr<Actor> > list;
	for (const auto &actor : actors) {
		if (!actor->visible || actor->worldSpace || (!actor->set.empty() && !actor->set.equalsIgnoreCase(set))) continue;
		uint32 at = list.size(); list.push_back(actor); while (at && list[at - 1]->dist < actor->dist) { list[at] = list[at - 1]; --at; } list[at] = actor;
	}
	return list;
}
bool CastSystem::compositeScreen(Bytes &rgba, uint32 width, uint32 height, const byte *palette, const Common::String &set) {
	for (const auto &actor : screenActors(actors, set)) if (!drawActorRect(actorRect(*actor, nullptr, nullptr), rgba, width, height, palette, width, height, Occlusion(), 0)) return false;
	return true;
}
Common::SharedPtr<Actor> CastSystem::hitTest(int32 x, int32 y, const WorldCamera &camera, const Common::String &set, const Occlusion &occlusion) {
	const auto screen = screenActors(actors, set);
	for (uint32 i = screen.size(); i > 0; --i) { uint32 source; if (actorRect(*screen[i - 1], nullptr, nullptr).texel(x, y, source)) return screen[i - 1]; }
	if (x < 0 || y < 0 || uint32(x) >= camera.width || uint32(y) >= camera.height) return Common::SharedPtr<Actor>();
	const auto list = drawList(camera, set);
	for (uint32 i = list.size(); i > 0; --i) {
		const auto &entry = list[i - 1]; uint32 source;
		if (actorRect(*entry.actor, &camera, &entry.point).texel(x, y, source) && !occlusion.hides(x, y, double(entry.point.depth) - entry.actor->zclip + occlusion.bias)) return entry.actor;
	}
	return Common::SharedPtr<Actor>();
}
bool CastSystem::onScreen(const Actor &actor, const WorldCamera &camera, const Common::String &set) {
	if (!actor.visible || (!actor.set.empty() && !actor.set.equalsIgnoreCase(set))) return false;
	ProjectedPoint point; if (actor.worldSpace && (actor.scale <= 0 || !projectPoint(camera, actor.x, actor.y, actor.z, point))) return false;
	const ActorRect rect = actorRect(actor, actor.worldSpace ? &camera : nullptr, actor.worldSpace ? &point : nullptr);
	return rect.sprite && rect.scale > 0 && rect.width > 0 && rect.height > 0 && rect.x < camera.width && rect.y < camera.height && rect.x + rect.width > 0 && rect.y + rect.height > 0;
}
static Common::String lower(Common::String name) { name.toLowercase(); return name; }
static Common::SharedPtr<ScriptInstance> castScript(const Bytes &data, const Common::String &name, Runtime &runtime) {
	Common::SharedPtr<ScriptInstance> script(new ScriptInstance()); script->name = name;
	ScriptTokens tokens; Common::String error;
	// Authoring stubs carry no executable handlers (e.g. the demo's smeth).
	if (!decodeScript(data, tokens, error)) return script;
	bool handlers = false; for (uint32 i = 0; i < tokens.size(); ++i) if (tokens[i].opcode == 4001) handlers = true;
	if (handlers && !parseScript(tokens, script->script, error)) runtime.fail("Cast script " + name + ": " + error);
	return script;
}
Common::SharedPtr<Actor> CastSystem::get(const Common::String &name) const {
	for (uint32 i = 0; i < actors.size(); ++i) if (actors[i]->name.equalsIgnoreCase(name)) return actors[i];
	return Common::SharedPtr<Actor>();
}
bool CastSystem::open(const Common::String &name, ScriptFrame *parent, bool callbacks, const Common::String &source) {
	const Common::String key = lower(name);
	for (uint32 i = 0; i < casts.size(); ++i) if (casts[i]->name == key) return true;
	Bytes bytes; if (!_resources.read(source.empty() ? name : source, bytes)) return false;
	Common::SharedPtr<LoadedCast> cast(new LoadedCast()); cast->name = key; cast->source = _resources.sourceName(source.empty() ? name : source);
	if (!cast->data.load(bytes)) { _runtime.fail("Invalid cast directory: " + name); return false; }
	cast->main = castScript(cast->data.archive.get(cast->data.mainScript), key, _runtime);
	for (uint32 i = 0; i < cast->data.members.size(); ++i) {
		const CastMember &member = cast->data.members[i];
		auto script = castScript(cast->data.archive.get(member.script), member.name, _runtime);
		script->parent = cast->main.get(); cast->scripts.push_back(script);
	}
	if (!_runtime.error.empty()) return false;
	casts.push_back(cast);
	for (uint32 i = 0; i < cast->data.members.size(); ++i) {
		const CastMember &member = cast->data.members[i]; if (get(member.name)) continue;
		Common::SharedPtr<Actor> actor(new Actor()); actor->name = member.name; actor->cast = cast; actor->member = i; actor->script = cast->scripts[i]; actors.push_back(actor);
	}
	if (!callbacks) return true;
	const ScriptResult opened = _runtime.run(*cast->main, "opencast", Values(), Common::String(), parent, Runtime::kLifecycle);
	if (opened.signal == ScriptResult::kAbandoned || opened.signal == ScriptResult::kError) return false;
	// Snapshot the owning cast: openactor may add actors, open another cast or
	// close this one. Script storage stays alive until the call chain unwinds.
	for (uint32 i = 0; i < cast->scripts.size() && _runtime.error.empty(); ++i) {
		bool loaded = false; for (uint32 c = 0; c < casts.size(); ++c) if (casts[c] == cast) loaded = true;
		if (!loaded) break;
		const auto &script = cast->scripts[i];
		const ScriptResult result = _runtime.run(*script, "openactor", Values(), Common::String(), parent, Runtime::kLifecycle);
		if (result.signal == ScriptResult::kAbandoned) break;
	}
	return _runtime.error.empty();
}
void CastSystem::close(const Common::String &name) {
	for (uint32 c = 0; c < casts.size(); ++c) if (casts[c]->name.equalsIgnoreCase(name)) {
		const auto cast = casts[c];
		for (uint32 a = actors.size(); a > 0; --a) if (actors[a - 1]->cast == cast) actors.remove_at(a - 1);
		casts.remove_at(c); return;
	}
}
void CastSystem::instance(const Common::String &src, const Common::String &dst) {
	const auto from = get(src); if (!from || dst.empty() || get(dst)) return;
	Common::SharedPtr<Actor> actor(new Actor()); actor->name = lower(dst); actor->cast = from->cast; actor->member = from->member; actor->copy = true;
	actor->script.reset(new ScriptInstance(*from->script)); actor->script->name = actor->name;
	actors.push_back(actor);
}
void CastSystem::remove(const Common::String &name) {
	for (uint32 i = 0; i < actors.size(); ++i) if (actors[i]->name.equalsIgnoreCase(name)) { actors.remove_at(i); return; }
}
static Common::String stem(Common::String name) { if (name.hasSuffixIgnoreCase(".cst")) name = name.substr(0, name.size() - 4); return lower(name); }
bool CastSystem::running(const Common::String &name, const Common::String &nameOfHandler) const {
	Common::String handler = nameOfHandler; handler.toLowercase();
	Common::SharedPtr<ScriptInstance> script; const auto actor = get(name);
	if (actor) script = actor->script;
	else for (uint32 c = 0; c < casts.size() && !script; ++c) for (uint32 m = 0; m < casts[c]->scripts.size(); ++m)
		if (casts[c]->scripts[m]->name.equalsIgnoreCase(name)) { script = casts[c]->scripts[m]; break; }
	for (const ScriptInstance *p = script.get(); p; p = p->parent) if (p->script.handlers.contains(handler)) return _runtime.isRunning(*p, handler);
	for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) if (_runtime.fallbacks[b]->script.handlers.contains(handler)) return _runtime.isRunning(*_runtime.fallbacks[b], handler);
	return false;
}
Value CastSystem::send(bool actorTarget, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent) {
	Common::SharedPtr<LoadedCast> cast; Common::SharedPtr<ScriptInstance> own;
	if (actorTarget) {
		const auto actor = get(name);
		if (actor) { cast = actor->cast; own = actor->script; }
		// A deleted original member retains its script; copies do not.
		else for (uint32 c = 0; c < casts.size() && !own; ++c) for (uint32 m = 0; m < casts[c]->scripts.size(); ++m)
			if (casts[c]->scripts[m]->name.equalsIgnoreCase(name)) { cast = casts[c]; own = cast->scripts[m]; break; }
	} else {
		for (uint32 c = 0; c < casts.size(); ++c) if (stem(casts[c]->name) == stem(name)) { cast = casts[c]; own = cast->main; break; }
	}
	if (!cast) return Value();
	if (own && own->script.handlers.empty()) {
		own.reset();
		if (cast->main->script.handlers.contains(handler)) own = cast->main;
	}
	Common::Array<ScriptInstance *> chain;
	if (own) chain.push_back(own.get());
	if (actorTarget && own && own != cast->main) chain.push_back(cast->main.get());
	// Actor defaults live in BOOTFILE. Keep their ownership stable in Session.
	if (actorTarget && (rootEvent || !own || parent.dispatch != handler)) for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) {
		ScriptInstance *boot = _runtime.fallbacks[b];
		if (boot->script.handlers.contains(handler) && !_runtime.isRunning(*boot, handler)) chain.push_back(boot);
	}
	Value value; const Common::String target = actorTarget ? name : parent.me;
	for (uint32 i = 0; i < chain.size(); ++i) {
		if (!chain[i]->script.handlers.contains(handler)) continue;
		// Containment helpers answer on behalf of the original script. A stub
		// resolves directly on its cast/boot instead, matching the demo's smeth.
		ScriptFrame addressed = parent; addressed.me = own ? own->name : chain[i]->name;
		if (rootEvent) addressed.dispatch = handler;
		const ScriptResult result = _runtime.run(*chain[i], handler, args, target, &addressed);
		value = result.value;
		if (result.signal == ScriptResult::kError || result.signal == ScriptResult::kAbandoned || (result.handled && result.signal != ScriptResult::kPass)) break;
	}
	return value;
}
}
