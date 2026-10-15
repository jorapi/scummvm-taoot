/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/balls.h"
namespace DreamFactory {
bool BallSystem::make(const Common::String &name, int32 vx, int32 vy, int32 vz, int32 gravity, int32 bounce, int32 frames) {
	const auto prop = _props.get(name);
	if (!prop) { _runtime.fail("makeball prop not found: " + name); return false; }
	stop(name);
	for (auto &slot : balls) if (!slot) {
		slot.reset(new Ball()); Ball &b = *slot; b.name = prop->name;
		b.x = int16(prop->x); b.y = int16(prop->y); b.z = int16(prop->z);
		b.vx = int16(vx); b.vy = int16(vy); b.vz = int16(vz);
		b.gravity = int16(gravity); b.bounce = int16(bounce); b.frames = int16(frames);
		return true;
	}
	return false;
}
void BallSystem::stop(const Common::String &name) {
	for (auto &slot : balls) if (slot && (name.equalsIgnoreCase("all") || slot->name.equalsIgnoreCase(name))) slot.reset();
}
void BallSystem::pause(const Common::String &name, bool paused) {
	for (auto &slot : balls) if (slot && (name.equalsIgnoreCase("all") || slot->name.equalsIgnoreCase(name)))
		slot->paused = paused ? int16(slot->paused + 1) : MAX<int16>(0, int16(slot->paused - 1));
}
bool BallSystem::has(const Common::String &name) const {
	for (const auto &slot : balls) if (slot && slot->name.equalsIgnoreCase(name)) return true;
	return false;
}
Common::String BallSystem::index(int32 n) const {
	if (n > 0) for (const auto &slot : balls) if (slot && --n == 0) return slot->name;
	return Common::String();
}
int32 BallSystem::count() const { int32 n = 0; for (const auto &slot : balls) if (slot) ++n; return n; }
bool BallSystem::building(int16 x, int16 y) const {
	if (!_rooms.current) return false;
	for (const Scene &scene : _rooms.current->data.scenes)
		if ((scene.x >> 8) == (x >> 8) && (scene.y >> 8) == (y >> 8)) return scene.build;
	return false;
}
// Original 0x411780/0x4118e0 use overlapping per-axis swept bounds,
// rather than intersecting a ray with the visible sprite or a sphere.
static bool crosses(int16 a, int16 b, int32 center, int32 half) {
	return !(a < center - half && b < center - half) && !(a > center + half && b > center + half);
}
static bool hits(const Ball &before, const Ball &after, int32 x, int32 y, int32 z, int16 width, int16 height) {
	return width > 0 && height > 0 && crosses(before.x, after.x, x, width) && crosses(before.y, after.y, y, width) && crosses(before.z, after.z, z, height);
}
bool BallSystem::service() {
	if (_servicing) return true;
	_servicing = true;
	const bool consumed = _runtime.eventConsumed;
	ScriptFrame context = _runtime.eventFrame("ball", "endball");
	struct End { Common::SharedPtr<Prop> prop; Common::String reason; };
	Common::Array<End> ended;
	const auto snapshot = balls;
	for (uint32 i = 0; i < snapshot.size() && _runtime.current(context); ++i) {
		const auto slot = snapshot[i]; if (!slot || balls[i] != slot || slot->paused) continue;
		const auto prop = _props.get(slot->name); if (!prop) continue;
		Ball &b = *slot; Common::String reason; b.frames = int16(b.frames - 1);
		if (b.frames < 0) reason = "frames";
		else {
			const Ball before = b;
			b.x = int16(b.x + b.vx); b.y = int16(b.y + b.vy); b.z = int16(b.z + b.vz);
			if (b.gravity) b.vz = int16(b.vz - 2);
			if (b.bounce) {
				if (b.z <= 0) { b.z = 0; b.vz = int16(-(b.vz / 2)); if (b.vz >= -2 && b.vz <= 2) b.vz = 0; }
				if (building(b.x, b.y)) {
					if ((before.x >> 8) != (b.x >> 8)) { b.x = int16(((before.x >> 8) + (b.x >> 8) + 1) * 128); b.vx = int16(-(b.vx / 2)); }
					if ((before.y >> 8) != (b.y >> 8)) { b.y = int16(((before.y >> 8) + (b.y >> 8) + 1) * 128); b.vy = int16(-(b.vy / 2)); }
				}
			} else if (b.z <= 0 || building(b.x, b.y)) reason = "building";
			if (reason.empty()) {
				for (const auto &actor : _casts.actors) {
					// v1 actorhitbox's first argument is the vertical extent,
					// despite the older host field names (0x426244/0x4117c1).
					if (actor->visible && actor->set.equalsIgnoreCase(_rooms.name()) && hits(before, b, actor->x, actor->y, actor->z, actor->hitHeight, actor->hitWidth)) { reason = actor->name; break; }
				}
				if (reason.empty() && _rooms.current && hits(before, b, _rooms.camera.x, _rooms.camera.y, 0, _playerHitbox.y, _playerHitbox.x)) reason = "player";
			}
			prop->x = b.x; prop->y = b.y; prop->z = b.z;
		}
		if (!reason.empty()) { balls[i].reset(); End e; e.prop = prop; e.reason = reason; ended.push_back(e); }
	}
	// Remove completed records before callbacks. A callback may replace a ball
	// and wait for its completion through forceupdate, just like endwalk.
	_servicing = false;
	for (const End &e : ended) {
		if (!_runtime.current(context)) break;
		if (_props.get(e.prop->name) != e.prop) continue;
		Values args; args.push_back(Value(e.reason));
		ScriptFrame event = _runtime.eventFrame("ball", "endball");
		_props.send(true, e.prop->name, "endball", args, event, true);
	}
	_runtime.eventConsumed = consumed;
	return _runtime.current(context);
}
}
