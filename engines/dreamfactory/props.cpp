/* SPDX-License-Identifier: GPL-3.0-or-later
 * Lifecycle, instance defaults and dispatch follow dreamREfactory's
 * runtime/props.ts, session-resources.ts and session-events.ts.
 */
#include "dreamfactory/props.h"
namespace DreamFactory {
static Common::String lower(Common::String name) { name.toLowercase(); return name; }
const PropState *Prop::state() const {
	if (!shop || group >= shop->data.groups.size()) return nullptr;
	const auto &states = shop->data.groups[group].states;
	for (uint32 i = 0; i < states.size(); ++i) if (states[i].name.equalsIgnoreCase(view)) return &states[i];
	return view.empty() && !states.empty() ? &states[0] : nullptr;
}
const Sprite *LoadedShop::frame(uint32 location) {
	if (!frameCache.contains(location)) { Sprite image; if (!data.sprite(location, image)) return nullptr; frameCache[location] = image; }
	return &frameCache[location];
}
bool PropSystem::warm(const Common::String &name, bool wholeShop) {
	const auto shop = wholeShop ? findShop(name) : Common::SharedPtr<LoadedShop>();
	if (wholeShop && !shop) return true;
	// DF.EXE 0x4131b0 warms active props in the shop, including copies.
	// Decode all states without selecting a frame or touching animation state.
	for (const auto &prop : props) {
		if (!(wholeShop ? prop->shop == shop : prop->name.equalsIgnoreCase(name))) continue;
		if (!prop->shop || prop->group >= prop->shop->data.groups.size()) continue;
		for (const auto &state : prop->shop->data.groups[prop->group].states)
			for (const auto &picture : state.pictures) if (!prop->shop->frame(picture.location)) {
				_runtime.fail("Cannot preload prop sprite: " + prop->name); return false;
			}
	}
	return true;
}
static uint32 degreeFrame(const PropState &state, int32 degree) {
	const uint32 target = uint32(degree) & 255; uint32 best = 0, distance = 257;
	for (uint32 i = 0; i < state.pictures.size(); ++i) {
		const uint32 diff = (uint32(state.pictures[i].degree) - target) & 255, d = MIN(diff, 256 - diff);
		if (d < distance) { best = i; distance = d; }
	}
	return best;
}
static Common::Array<uint32> variantFrames(const PropState &state, int32 degree) {
	Common::Array<uint32> result; uint32 sizes[8] = {}, groups = 0, size = 0;
	if (state.pictures.size() < 3) return result;
	for (uint32 i = 0; i < state.pictures.size(); ++i) {
		const int32 d = state.pictures[i].degree; if (d < 0 || d >= 8) return result;
		++sizes[d];
	}
	for (uint32 d = 0; d < 8; ++d) if (sizes[d]) { if (size && size != sizes[d]) return result; size = sizes[d]; ++groups; }
	if (groups < 2 || size < 2) return result;
	const int32 selected = state.pictures[degreeFrame(state, degree)].degree;
	for (uint32 i = 0; i < state.pictures.size(); ++i) if (state.pictures[i].degree == selected) result.push_back(i);
	return result;
}
static Common::Array<uint32> playOrder(const PropState &state, int32 degree) {
	const auto variant = variantFrames(state, degree);
	if (state.play.empty()) return variant;
	if (variant.empty()) return state.play;
	Common::Array<uint32> grouped; bool positions = true, contained = true;
	for (uint32 p = 0; p < state.play.size(); ++p) {
		const uint32 entry = state.play[p]; bool found = false, present = false;
		for (uint32 i = 0; i < variant.size(); ++i) {
			if (variant[i] == entry) present = true;
			if (!found && state.pictures[variant[i]].group == int32(entry)) { grouped.push_back(variant[i]); found = true; }
		}
		if (entry >= variant.size()) positions = false;
		if (!present) contained = false;
	}
	if (grouped.size() == state.play.size()) return grouped;
	if (positions) { grouped.clear(); for (uint32 p = 0; p < state.play.size(); ++p) grouped.push_back(variant[state.play[p]]); return grouped; }
	return contained ? state.play : variant;
}
uint32 Prop::frameCount() const { const auto s = state(); return !order.empty() ? order.size() : s ? s->pictures.size() : 0; }
uint32 Prop::frameIndex() const {
	const auto s = state(); if (!s || s->pictures.empty()) return 0;
	if (order.empty() && !s->animated) return degreeFrame(*s, deg);
	const uint32 index = MIN(step, frameCount() - 1); return order.empty() ? index : order[index];
}
void Prop::selectView(Common::String name, uint64 event) {
	name.toLowercase(); view = name; lastTick = 0; const auto s = state(); order.clear(); step = 0; locked = animating = false;
	if (!s || s->pictures.empty()) return;
	const auto variant = variantFrames(*s, deg);
	if (!s->animated || (variants && variant.empty() && (degreeEvent == event || s->pictures.size() <= 2))) {
		step = degreeFrame(*s, deg); locked = true;
	} else { order = playOrder(*s, deg); animating = frameCount() > 1; }
}
void Prop::selectDegree(int32 value, uint64 event) {
	deg = value; if (worldSpace) { directional = true; return; }
	variants = true; degreeEvent = event; const auto s = state();
	if (!s || s->pictures.empty()) return;
	if (animating && !variantFrames(*s, deg).empty()) { order = playOrder(*s, deg); return; }
	order.clear(); step = degreeFrame(*s, deg); locked = true;
}
void Prop::becomeWorld() {
	worldSpace = true; locked = variants = false; order.clear();
	if (shop && shop->data.version == 1 && !scale) scale = 1000;
}
static Common::String stem(Common::String name) {
	if (name.hasSuffixIgnoreCase(".shp") || name.hasSuffixIgnoreCase(".prp")) name = name.substr(0, name.size() - 4);
	return lower(name);
}
Common::SharedPtr<LoadedShop> PropSystem::findShop(const Common::String &name) const {
	for (uint32 i = 0; i < shops.size(); ++i) if (shops[i]->name.equalsIgnoreCase(name)) return shops[i];
	for (uint32 i = 0; i < shops.size(); ++i) if (stem(shops[i]->name) == stem(name) || shops[i]->data.name.equalsIgnoreCase(name)) return shops[i];
	return Common::SharedPtr<LoadedShop>();
}
static Common::SharedPtr<ScriptInstance> shopScript(const Bytes &data, const Common::String &name, Runtime &runtime) {
	Common::SharedPtr<ScriptInstance> script(new ScriptInstance()); script->name = name;
	ScriptTokens tokens; Common::String error;
	// Authoring stubs carry no executable handlers.
	if (!decodeScript(data, tokens, error)) return script;
	bool handlers = false; for (uint32 i = 0; i < tokens.size(); ++i) if (tokens[i].opcode == 4001) handlers = true;
	if (handlers && !parseScript(tokens, script->script, error)) runtime.fail("Shop script " + name + ": " + error);
	return script;
}
Common::SharedPtr<Prop> PropSystem::get(const Common::String &name) const {
	for (uint32 i = 0; i < props.size(); ++i) if (props[i]->name.equalsIgnoreCase(name)) return props[i];
	return Common::SharedPtr<Prop>();
}
bool PropSystem::open(const Common::String &name, ScriptFrame *parent, bool callbacks, const Common::String &source) {
	Common::String key = lower(name);
	if (!key.hasSuffix(".shp") && !key.hasSuffix(".prp")) key += ".shp";
	for (uint32 i = 0; i < shops.size(); ++i) if (shops[i]->name == key) {
		const auto shop = shops[i];
		const ScriptResult result = _runtime.run(*shop->main, "openshop", Values(), Common::String(), parent, Runtime::kLifecycle);
		return result.signal != ScriptResult::kAbandoned && _runtime.error.empty();
	}
	Bytes bytes; if (!_resources.read(source.empty() ? key : source, bytes)) return false;
	Common::SharedPtr<LoadedShop> shop(new LoadedShop()); shop->name = key; shop->source = _resources.sourceName(source.empty() ? key : source);
	shop->persistent = key == "house.shp" || key == "inven.shp" || key == "house.prp" || key == "inven.prp";
	if (!shop->data.load(bytes)) { _runtime.fail("Invalid shop directory: " + name); return false; }
	shop->main = shopScript(shop->data.archive.get(shop->data.mainScript), key, _runtime);
	for (uint32 i = 0; i < shop->data.groups.size(); ++i) {
		const PropGroup &group = shop->data.groups[i];
		auto script = shopScript(shop->data.archive.get(group.script), lower(group.name), _runtime);
		script->parent = shop->main.get(); shop->scripts.push_back(script);
	}
	if (!_runtime.error.empty()) return false;
	shops.push_back(shop);
	for (uint32 i = 0; i < shop->data.groups.size(); ++i) {
		const PropGroup &group = shop->data.groups[i];
		Common::SharedPtr<Prop> prop(new Prop()); prop->name = lower(group.name); prop->shop = shop; prop->group = i; prop->script = shop->scripts[i];
		bool replaced = false;
		for (uint32 p = 0; p < props.size(); ++p) if (props[p]->name == prop->name) { props[p] = prop; replaced = true; break; }
		if (!replaced) props.push_back(prop);
	}
	if (!callbacks) return true;
	// Snapshot the owning shop: openprop may add props, open another shop or
	// close this one. Script storage stays alive until the call chain unwinds.
	for (uint32 i = 0; i < shop->scripts.size() && _runtime.error.empty(); ++i) {
		bool loaded = false; for (uint32 c = 0; c < shops.size(); ++c) if (shops[c] == shop) loaded = true;
		if (!loaded) break;
		const auto &script = shop->scripts[i];
		const ScriptResult result = _runtime.run(*script, "openprop", Values(), Common::String(), parent, Runtime::kLifecycle);
		if (result.signal == ScriptResult::kAbandoned || result.signal == ScriptResult::kError) return false;
	}
	bool loaded = false; for (uint32 c = 0; c < shops.size(); ++c) if (shops[c] == shop) loaded = true;
	if (loaded && _runtime.error.empty()) {
		const ScriptResult result = _runtime.run(*shop->main, "openshop", Values(), Common::String(), parent, Runtime::kLifecycle);
		if (result.signal == ScriptResult::kAbandoned) return false;
	}
	return _runtime.error.empty();
}
void PropSystem::close(const Common::String &name, ScriptFrame *parent) {
	const auto shop = findShop(name); if (!shop || shop->closing) return;
	shop->closing = true;
	const ScriptResult result = _runtime.run(*shop->main, "closeshop", Values(), Common::String(), parent, Runtime::kLifecycle);
	if (result.signal == ScriptResult::kAbandoned) { shop->closing = false; return; }
	for (uint32 c = 0; c < shops.size(); ++c) if (shops[c] == shop) {
		for (uint32 a = props.size(); a > 0; --a) if (props[a - 1]->shop == shop) props.remove_at(a - 1);
		shops.remove_at(c); return;
	}
}
void PropSystem::instance(const Common::String &src, const Common::String &dst) {
	const auto from = get(src); if (!from || dst.empty() || get(dst)) return;
	Common::SharedPtr<Prop> prop(new Prop()); prop->name = lower(dst); prop->shop = from->shop; prop->group = from->group; prop->copy = true;
	prop->visible = from->visible; prop->view = from->view; prop->deg = from->deg; prop->dist = from->dist; prop->worldSpace = from->worldSpace; prop->directional = from->directional; prop->set = from->set;
	prop->script.reset(new ScriptInstance(*from->script)); prop->script->name = prop->name;
	props.push_back(prop);
}
void PropSystem::remove(const Common::String &name) {
	for (uint32 i = 0; i < props.size(); ++i) if (props[i]->name.equalsIgnoreCase(name)) { props.remove_at(i); return; }
}
void PropSystem::animate(uint64 now) {
	for (uint32 i = 0; i < props.size(); ++i) {
		const auto &prop = props[i]; const auto state = prop->state(); if (prop->locked || !state) continue;
		const bool round = prop->shop->data.version == 4 && state->play.size() > 1;
		if (!prop->visible && !round) continue;
		if (!prop->animating) { if (!round) continue; prop->order = playOrder(*state, prop->deg); prop->step = 0; prop->lastTick = 0; prop->animating = true; }
		const uint32 count = prop->frameCount();
		if (count < 2 || (!round && prop->step >= count - 1)) { prop->animating = false; continue; }
		if (!prop->lastTick) prop->lastTick = now;
		if (now - prop->lastTick < 50) continue;
		prop->lastTick = now;
		if (round) prop->step = (prop->step + 1) % count;
		else if (++prop->step >= count - 1) prop->animating = false;
	}
}
Common::Array<Common::SharedPtr<Prop> > PropSystem::drawList(bool persistentOnly) const {
	Common::Array<Common::SharedPtr<Prop> > result;
	for (uint32 i = 0; i < props.size(); ++i) {
		const auto &prop = props[i]; const auto state = prop->state();
		if (!prop->visible || prop->hidden || prop->worldSpace || !state || state->pictures.empty() || (persistentOnly && !prop->shop->persistent)) continue;
		uint32 at = result.size(); result.push_back(prop);
		while (at && result[at - 1]->dist < prop->dist) { result[at] = result[at - 1]; --at; }
		result[at] = prop;
	}
	return result;
}
bool PropSystem::composite(Bytes &rgba, uint32 width, uint32 height, const byte *palette, bool persistentOnly) {
	if (!width || !height || width > 4096 || height > 4096 || rgba.size() != width * height * 4) return false;
	const auto list = drawList(persistentOnly);
	for (uint32 i = 0; i < list.size(); ++i) {
		const auto &prop = list[i]; const Sprite *sprite = prop->shop->frame(prop->state()->pictures[prop->frameIndex()].location);
		if (!sprite) return false;
		const int64 dx = int64(prop->anchorX) - sprite->x, dy = int64(prop->anchorY) - sprite->y;
		for (int32 y = 0; y < sprite->height; ++y) for (int32 x = 0; x < sprite->width; ++x) {
			const int64 xx = dx + x, yy = dy + y; const uint32 source = y * sprite->width + x;
			if (xx < 0 || yy < 0 || xx >= width || yy >= height || !sprite->opaque[source]) continue;
			memcpy(rgba.data() + (yy * width + xx) * 4, palette + sprite->pixels[source] * 3, 3);
		}
	}
	return true;
}
Common::SharedPtr<Prop> PropSystem::hitTest(int32 x, int32 y, bool persistentOnly) {
	const auto list = drawList(persistentOnly);
	for (uint32 i = list.size(); i > 0; --i) {
		const auto &prop = list[i - 1]; if (prop->contains(x, y)) return prop;
	}
	return Common::SharedPtr<Prop>();
}
bool Prop::contains(int32 x, int32 y) const {
	const auto s = state(); if (!s || s->pictures.empty()) return false;
	const Sprite *sprite = shop->frame(s->pictures[frameIndex()].location); if (!sprite) return false;
	const int64 xx = int64(x) - anchorX + sprite->x, yy = int64(y) - anchorY + sprite->y;
	return xx >= 0 && yy >= 0 && xx < sprite->width && yy < sprite->height && sprite->opaque[yy * sprite->width + xx];
}
Common::Array<WorldPropEntry> PropSystem::worldDrawList(const WorldCamera &camera, const Common::String &set) const {
	Common::Array<WorldPropEntry> result;
	for (uint32 i = 0; i < props.size(); ++i) {
		const auto &prop = props[i]; const auto state = prop->state();
		if (!prop->visible || prop->hidden || !prop->worldSpace || !state || state->pictures.empty() || prop->scale <= 0 || (!prop->set.empty() && !prop->set.equalsIgnoreCase(set))) continue;
		WorldPropEntry entry; entry.prop = prop;
		if (!projectPoint(camera, prop->x, prop->y, prop->z, entry.point)) continue;
		uint32 at = result.size(); result.push_back(entry);
		while (at && result[at - 1].point.depth < entry.point.depth) { result[at] = result[at - 1]; --at; }
		result[at] = entry;
	}
	return result;
}
struct WorldPropRect {
	const Sprite *sprite = nullptr;
	double scale = 0, x = 0, y = 0, width = 0, height = 0;
	bool texel(int32 px, int32 py, uint32 &source) const {
		if (!sprite || scale <= 0 || px < x || py < y || px >= x + width || py >= y + height) return false;
		const uint32 sx = uint32(MIN<double>(sprite->width - 1, floor((px - x) / scale))), sy = uint32(MIN<double>(sprite->height - 1, floor((py - y) / scale)));
		source = sy * sprite->width + sx;
		return sprite->opaque[source] != 0;
	}
};
static WorldPropRect worldPropRect(const WorldPropEntry &entry, const WorldCamera &camera) {
	WorldPropRect rect; const auto &prop = entry.prop; const auto state = prop->state();
	uint32 index = prop->frameIndex();
	if (prop->directional && state->pictures.size() > 1) index = degreeFrame(*state, int32(uint32(prop->deg) - uint32(worldBearing(double(camera.x) - prop->x, double(camera.y) - prop->y))));
	const PropPicture &picture = state->pictures[index]; rect.sprite = prop->shop->frame(picture.location);
	if (!rect.sprite || !rect.sprite->width || !rect.sprite->height) return rect;
	rect.scale = double(prop->scale) * picture.refScale / (1000.0 * entry.point.depth);
	if (rect.scale <= 0) return rect;
	rect.x = entry.point.x - floor(rect.sprite->x * rect.scale + 0.5);
	rect.y = entry.point.y - floor(rect.sprite->y * rect.scale + 0.5);
	rect.width = MAX<double>(1, floor(rect.sprite->width * rect.scale + 0.5));
	rect.height = MAX<double>(1, floor(rect.sprite->height * rect.scale + 0.5));
	return rect;
}
bool PropSystem::compositeWorldOne(const WorldPropEntry &entry, Bytes &rgba, uint32 width, uint32 height, const byte *palette, const WorldCamera &camera, const Occlusion &occlusion) {
	if (!width || !height || width > 4096 || height > 4096 || rgba.size() != width * height * 4) return false;
	const WorldPropRect rect = worldPropRect(entry, camera);
	if (!rect.sprite) return false;
	if (rect.scale <= 0 || !rect.sprite->width || !rect.sprite->height) return true;
	const int32 left = int32(CLIP<double>(rect.x, 0, MIN(width, camera.width))), top = int32(CLIP<double>(rect.y, 0, MIN(height, camera.height)));
	const double right = MIN<double>(MIN(width, camera.width), rect.x + rect.width), bottom = MIN<double>(MIN(height, camera.height), rect.y + rect.height);
	const double depth = double(entry.point.depth) - entry.prop->zclip + occlusion.bias;
	for (int32 y = top; y < bottom; ++y) for (int32 x = left; x < right; ++x) {
		uint32 source;
		if (!rect.texel(x, y, source) || occlusion.hides(x, y, depth)) continue;
		memcpy(rgba.data() + (y * width + x) * 4, palette + rect.sprite->pixels[source] * 3, 3);
	}
	return true;
}
bool PropSystem::compositeWorld(Bytes &rgba, uint32 width, uint32 height, const byte *palette, const WorldCamera &camera, const Common::String &set, const Occlusion &occlusion) {
	for (const auto &entry : worldDrawList(camera, set)) if (!compositeWorldOne(entry, rgba, width, height, palette, camera, occlusion)) return false;
	return true;
}
Common::SharedPtr<Prop> PropSystem::hitWorld(int32 x, int32 y, const WorldCamera &camera, const Common::String &set, const Occlusion &occlusion) {
	if (x < 0 || y < 0 || uint32(x) >= camera.width || uint32(y) >= camera.height) return Common::SharedPtr<Prop>();
	const auto list = worldDrawList(camera, set);
	for (uint32 i = list.size(); i > 0; --i) {
		const auto &entry = list[i - 1]; uint32 source;
		if (worldPropRect(entry, camera).texel(x, y, source) && !occlusion.hides(x, y, double(entry.point.depth) - entry.prop->zclip + occlusion.bias)) return entry.prop;
	}
	return Common::SharedPtr<Prop>();
}
bool PropSystem::running(const Common::String &name, const Common::String &nameOfHandler) const {
	Common::String handler = nameOfHandler; handler.toLowercase();
	Common::SharedPtr<ScriptInstance> script; const auto prop = get(name);
	if (prop) script = prop->script;
	else for (uint32 c = 0; c < shops.size() && !script; ++c) for (uint32 m = 0; m < shops[c]->scripts.size(); ++m)
		if (shops[c]->scripts[m]->name.equalsIgnoreCase(name)) { script = shops[c]->scripts[m]; break; }
	for (const ScriptInstance *p = script.get(); p; p = p->parent) if (p->script.handlers.contains(handler)) return _runtime.isRunning(*p, handler);
	for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) if (_runtime.fallbacks[b]->script.handlers.contains(handler)) return _runtime.isRunning(*_runtime.fallbacks[b], handler);
	return false;
}
Value PropSystem::send(bool propTarget, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent) {
	Common::SharedPtr<LoadedShop> shop; Common::SharedPtr<ScriptInstance> own;
	if (propTarget) {
		const auto prop = get(name);
		if (prop) { shop = prop->shop; own = prop->script; }
		// A deleted original group retains its script; copies do not.
		else for (uint32 c = 0; c < shops.size() && !own; ++c) for (uint32 m = 0; m < shops[c]->scripts.size(); ++m)
			if (shops[c]->scripts[m]->name.equalsIgnoreCase(name)) { shop = shops[c]; own = shop->scripts[m]; break; }
	} else {
		shop = findShop(name); if (shop) own = shop->main;
	}
	if (!shop) return Value();
	if (own && own->script.handlers.empty()) {
		own.reset();
		if (shop->main->script.handlers.contains(handler)) own = shop->main;
	}
	Common::Array<ScriptInstance *> chain;
	if (own) chain.push_back(own.get());
	if (propTarget && own && own != shop->main) chain.push_back(shop->main.get());
	// Prop defaults live in BOOTFILE. Keep their ownership stable in Session.
	if (propTarget && (rootEvent || !own || parent.dispatch != handler)) for (uint32 b = 0; b < _runtime.fallbacks.size(); ++b) {
		ScriptInstance *boot = _runtime.fallbacks[b];
		if (boot->script.handlers.contains(handler) && !_runtime.isRunning(*boot, handler)) chain.push_back(boot);
	}
	Value value; const Common::String target = propTarget ? name : parent.me;
	for (uint32 i = 0; i < chain.size(); ++i) {
		if (!chain[i]->script.handlers.contains(handler)) continue;
		// Containment helpers answer on behalf of the original script. A stub
		// resolves directly on its shop/boot instead.
		ScriptFrame addressed = parent; addressed.me = own ? own->name : chain[i]->name;
		if (rootEvent) addressed.dispatch = handler;
		const ScriptResult result = _runtime.run(*chain[i], handler, args, target, &addressed);
		value = result.value;
		if (result.signal == ScriptResult::kError || result.signal == ScriptResult::kAbandoned || (result.handled && result.signal != ScriptResult::kPass)) break;
	}
	return value;
}
}
