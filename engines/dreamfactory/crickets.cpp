/* SPDX-License-Identifier: GPL-3.0-or-later
 * Range timers/spatialization follow TI.EXE 0x444430/0x444790; retail loop
 * priority follows 0x4446d0. The independent RNG follows dreamREfactory.
 */
#include "dreamfactory/crickets.h"
#include "dreamfactory/geometry.h"
#include <math.h>
namespace DreamFactory {
void CricketSystem::reset() {
	stop("all"); room = "none"; _listener = false; _x = _y = _facing = 0;
	_random.setSeed(0x9e3779b9);
}
int64 CricketSystem::countdown(const Cricket &cricket) {
	return int64(cricket.base) + (cricket.jitter > 0 ? 1 + int64(_random.getRandomNumber(uint32(cricket.jitter) - 1)) : 0);
}
bool CricketSystem::make(Common::String name, int32 x, int32 y, int32 radius, int32 base, int32 jitter) {
	if (name.empty()) return false;
	stop(name);
	uint32 slot = 0;
	while (slot < crickets.size() && !crickets[slot]->name.empty()) ++slot;
	if (slot >= 16) return false;
	Common::SharedPtr<Cricket> cricket(new Cricket()); name.toLowercase(); cricket->name = name; cricket->set = room;
	cricket->x = x; cricket->y = y; cricket->radius = MAX<int32>(1, radius); cricket->base = base; cricket->jitter = jitter;
	cricket->count = jitter < 0 ? base : 0;
	if (slot == crickets.size()) crickets.push_back(cricket); else crickets[slot] = cricket;
	return true;
}
void CricketSystem::retire(uint32 slot) {
	crickets[slot].reset(new Cricket());
	// Keep interior holes so replacing/recreating a sound uses native slot
	// order, including after saving. Trailing holes carry no ordering state.
	while (!crickets.empty() && crickets.back()->name.empty()) crickets.pop_back();
}
void CricketSystem::stop(const Common::String &name) {
	if (name.equalsIgnoreCase("all")) {
		// A rejected detached save may have decoded only part of the table.
		for (const auto &c : crickets) if (c && !c->name.empty()) _sound.stopAmbient(c->name);
		crickets.clear(); return;
	}
	for (uint32 i = 0; i < crickets.size(); ++i) if (crickets[i] && !crickets[i]->name.empty() && crickets[i]->name.equalsIgnoreCase(name)) {
		_sound.stopAmbient(crickets[i]->name); retire(i); return;
	}
}
void CricketSystem::pause(const Common::String &name, bool paused) {
	for (const auto &c : crickets) if (!c->name.empty() && (name.equalsIgnoreCase("all") || c->name.equalsIgnoreCase(name))) c->paused = paused;
}
bool CricketSystem::has(const Common::String &name) const {
	for (const auto &c : crickets) if (!c->name.empty() && c->name.equalsIgnoreCase(name)) return true;
	return false;
}
uint32 CricketSystem::count() const {
	uint32 number = 0;
	for (const auto &c : crickets) if (!c->name.empty()) ++number;
	return number;
}
Common::String CricketSystem::index(int32 number) const {
	if (number > 0) for (const auto &c : crickets) if (!c->name.empty() && --number == 0) return c->name;
	return Common::String();
}
void CricketSystem::setRoom(Common::String name) { name.toLowercase(); room = name; silenceAbsent(); }
void CricketSystem::silenceAbsent() {
	for (uint32 i = 0; i < crickets.size(); ++i) {
		Cricket &c = *crickets[i];
		if (c.name.empty()) continue;
		if (c.set.empty() || c.set.equalsIgnoreCase(room)) continue;
		_sound.stopAmbient(c.name);
		c.playing = false; c.armed = false;
	}
}
void CricketSystem::spatial(const Cricket &c, double &gain, double &pan) const {
	gain = 1; pan = 0;
	if (_listener) {
		const int64 dx = int64(c.x) - _x, dy = int64(c.y) - _y;
		const uint32 distance = uint32(sqrt(double(uint32(dx * dx + dy * dy))));
		const int32 level = distance < uint32(c.radius) ? 255 - int32(int64(distance) * 255 / c.radius) : 0;
		gain = double(level) / 255;
		const int32 from = int32((uint32(uint16(_x)) << 16) | uint16(_y));
		const int32 to = int32((uint32(uint16(c.x)) << 16) | uint16(c.y));
		const int32 facing = _facing & 255;
		const int32 difference = ABS(facing - 128);
		int32 bearing = (packedBearing(from, to) + (facing > 128 ? -difference : difference)) & 255;
		if (bearing > 192) bearing = 384 - bearing;
		if (bearing < 64) bearing = 128 - bearing;
		pan = double(CLIP<int32>(bearing * 2 - 128, 0, 255) - 128) / 127;
	}
}
void CricketSystem::service() {
	silenceAbsent();
	const bool retail = _sound.isTitanicRetail();
	Common::Array<Common::SharedPtr<Cricket> > candidates;
	Common::SharedPtr<Cricket> selected[2];
	double highest[2] = { -1, -1 };
	if (retail) for (const auto &c : crickets) {
		if (c->name.empty() || c->paused || c->jitter < 0 || (!c->set.empty() && !c->set.equalsIgnoreCase(room)) || !_sound.soundLoop(c->name)) continue;
		double gain, pan; spatial(*c, gain, pan);
		if (gain <= 0 || !_sound.namedClip(c->name)) continue;
		candidates.push_back(c);
		// TI.EXE 0x4446d0 selects by raw positional volume, before master
		// scaling. A strict comparison keeps the earlier table entry on ties.
		if (gain > highest[0]) { selected[1] = selected[0]; highest[1] = highest[0]; selected[0] = c; highest[0] = gain; }
		else if (gain > highest[1]) { selected[1] = c; highest[1] = gain; }
	}
	const auto scheduled = crickets;
	for (const auto &entry : scheduled) {
		Cricket &c = *entry; if (c.name.empty() || c.paused) continue;
		if (!c.set.empty() && !c.set.equalsIgnoreCase(room)) continue;
		double gain, pan; spatial(c, gain, pan);
		if (c.jitter < 0) {
			if (c.count > 0) --c.count;
			if (c.count > 0) continue;
			if (gain > 0) _sound.playAmbient(c.name, gain, pan, c.handle);
			for (uint32 i = 0; i < crickets.size(); ++i) if (crickets[i] == entry) { retire(i); break; }
			continue;
		}
		if (gain <= 0) {
			_sound.stopAmbient(c.name);
			c.playing = false; c.armed = false; continue;
		}
		c.playing = c.playing && _sound.handleActive(c.handle);
		if (c.playing) _sound.positionAmbient(c.handle, gain, pan);
		// Looping PCM starts on range entry; timed one-shots arm there and
		// count only audible, unpaused service passes (TI.EXE 0x44462d).
		if (_sound.soundLoop(c.name) && !_sound.isTitanicDemo()) {
			if (!retail && !c.playing) c.playing = _sound.playAmbient(c.name, gain, pan, c.handle);
			c.armed = true; continue;
		}
		if (!c.armed) {
			c.armed = true;
			if (_sound.isTitanicDemo() && _sound.soundLoop(c.name)) {
				c.playing = _sound.playAmbient(c.name, gain, pan, c.handle);
				c.count = 0x7fffffff; // Demo 444f22: start once per range entry.
			} else c.count = countdown(c);
			continue;
		}
		if (c.count > 0) --c.count;
		if (c.count > 0) continue;
		c.playing = _sound.playAmbient(c.name, gain, pan, c.handle);
		c.count = countdown(c);
	}
	// The retail priority pass follows all timed/one-shot service. Selected
	// loops then compete with ordinary effects through the same physical slots.
	for (const auto &c : candidates) if (c != selected[0] && c != selected[1]) {
		_sound.stopAmbient(c->name); c->playing = false;
	}
	for (const auto &c : selected) if (c) {
		double gain, pan; spatial(*c, gain, pan);
		c->playing = _sound.playAmbient(c->name, gain, pan, c->handle, SoundSystem::kSingle);
	}
}
}
