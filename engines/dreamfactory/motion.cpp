/* SPDX-License-Identifier: GPL-3.0-or-later
 * Movement records, turn-before-move and callbacks follow
 * dreamREfactory/runtime/scheduler.ts and runtime/builtins/actors.ts.
 */
#include "dreamfactory/motion.h"
#include "common/textconsole.h"
#include <math.h>
namespace DreamFactory {
Common::SharedPtr<ActorWalk> MotionSystem::get(const Common::String &name) const {
	for (uint32 i = 0; i < walks.size(); ++i) if (walks[i]->actor->name.equalsIgnoreCase(name)) return walks[i];
	return Common::SharedPtr<ActorWalk>();
}
bool MotionSystem::contains(const Common::SharedPtr<ActorWalk> &walk) const {
	for (uint32 i = 0; i < walks.size(); ++i) if (walks[i] == walk) return true;
	return false;
}
void MotionSystem::remove(const Common::SharedPtr<ActorWalk> &walk) {
	for (uint32 i = 0; i < walks.size(); ++i) if (walks[i] == walk) { walks.remove_at(i); return; }
}
void MotionSystem::prune() {
	for (uint32 i = walks.size(); i > 0; --i) if (_casts.get(walks[i - 1]->actor->name) != walks[i - 1]->actor) walks.remove_at(i - 1);
}
void MotionSystem::stop(const Common::String &name) {
	const auto walk = get(name); if (!walk) return;
	remove(walk);
	if (walk->actor->pose == "walk") { walk->actor->pose = "stand"; walk->actor->step = 0; }
}
void MotionSystem::pause(const Common::String &name, bool paused) { const auto walk = get(name); if (walk) walk->paused = paused; }
void MotionSystem::start(const Common::String &name, int32 x, int32 y, int32 z, const Common::String &destination) {
	const auto actor = _casts.get(name); if (!actor) return;
	const auto previous = get(name); if (previous) remove(previous);
	if (walks.size() >= 16) { warning("DreamFactory walk table full; dropping %s", name.c_str()); return; }
	Common::SharedPtr<ActorWalk> walk(new ActorWalk()); walk->actor = actor;
	walk->sx = actor->x; walk->sy = actor->y; walk->sz = actor->z;
	walk->dx = double(x) - actor->x; walk->dy = double(y) - actor->y; walk->dz = double(z) - actor->z;
	walk->distance = MAX<double>(1, floor(sqrt(walk->dx * walk->dx + walk->dy * walk->dy + walk->dz * walk->dz)));
	walk->facing = int32(floor(atan2(walk->dy, walk->dx) * 256 / (2 * M_PI) + 0.5)) & 255;
	walk->destination = destination; walks.push_back(walk);
}
void MotionSystem::turn(const Common::String &name, int32 facing) {
	const auto actor = _casts.get(name); if (!actor) return;
	const bool dust = actor->cast && actor->cast->data.version == 1;
	// Dust also queues an already-facing turn (DF.EXE 0x410690), so its
	// completion callback must still run on the next movement pass.
	facing &= 255; if (!dust && (actor->deg & 255) == facing) return;
	if (const auto previous = get(name)) {
		if (!dust) { actor->deg = facing; return; }
		// Dust's turn builder replaces the actor's previous walk via
		// 0x4107db -> 0x40fed0 before installing its turn record.
		remove(previous);
	}
	if (walks.size() >= 16) { warning("DreamFactory walk table full; dropping turn %s", name.c_str()); return; }
	Common::SharedPtr<ActorWalk> walk(new ActorWalk()); walk->actor = actor;
	walk->facing = facing; walk->turnOnly = true; walk->destination = actor->star; walks.push_back(walk);
}
void MotionSystem::startPath(const Common::String &name, const StarPath &route, const Common::String &destination, bool resume) {
	const auto actor = _casts.get(name); if (!actor || route.points.size() < 2) return;
	Common::Array<StarPoint> points = route.points;
	if (route.a.equalsIgnoreCase(destination)) {
		for (uint32 i = 0; i < points.size() / 2; ++i) SWAP(points[i], points[points.size() - 1 - i]);
		for (uint32 i = points.size() - 1; i > 0; --i) points[i].distance = points[i - 1].distance;
		points[0].distance = 0;
	}
	uint32 first = 0;
	if (resume) {
		double best = HUGE_VAL;
		for (uint32 i = 0; i < points.size(); ++i) {
			const double x = double(actor->x) - points[i].x, y = double(actor->y) - points[i].y, z = double(actor->z) - points[i].z;
			const double distance = floor(sqrt(x * x + y * y + z * z));
			if (distance < best) { best = distance; first = i; }
		}
		if (first == points.size() - 1) --first;
		points[first].x = actor->x; points[first].y = actor->y; points[first].z = actor->z;
	}
	const auto previous = get(name); if (previous) remove(previous);
	if (walks.size() >= 16) { warning("DreamFactory walk table full; dropping %s", name.c_str()); return; }
	Common::SharedPtr<ActorWalk> walk(new ActorWalk()); walk->actor = actor;
	double cumulative = 0;
	for (uint32 i = first; i < points.size(); ++i) {
		if (i > first) {
			const double x = double(points[i].x) - points[i - 1].x, y = double(points[i].y) - points[i - 1].y, z = double(points[i].z) - points[i - 1].z;
			cumulative += resume ? floor(sqrt(x * x + y * y + z * z)) : points[i].distance;
		}
		MotionPoint point; point.x = points[i].x; point.y = points[i].y; point.z = points[i].z; point.cumulative = cumulative; walk->path.push_back(point);
	}
	const MotionPoint &a = walk->path[0], &b = walk->path[1];
	walk->facing = int32(floor(atan2(b.y - a.y, b.x - a.x) * 256 / (2 * M_PI) + 0.5)) & 255;
	walk->distance = MAX<double>(1, floor(cumulative)); walk->destination = destination; walks.push_back(walk);
}
bool MotionSystem::service() {
	if (_servicing) return true;
	_servicing = true; const bool consumed = _runtime.eventConsumed;
	ScriptFrame context = _runtime.eventFrame("walk", "endwalk");
	const auto snapshot = walks; Common::Array<Common::SharedPtr<ActorWalk> > arrived, turned;
	for (uint32 i = 0; i < snapshot.size() && _runtime.current(context); ++i) {
		const auto walk = snapshot[i]; const auto actor = walk->actor;
		if (!contains(walk)) continue;
		if (_casts.get(actor->name) != actor) { remove(walk); continue; }
		if (walk->paused) continue;
		if (walk->turning) {
			const int32 diff = (walk->facing - actor->deg) & 255;
			const int64 step = MAX<int64>(1, actor->turn < 0 ? -int64(actor->turn) : actor->turn);
			actor->deg = diff <= step || 256 - diff <= step ? walk->facing : (actor->deg + (diff < 128 ? int32(step) : -int32(step))) & 255;
			if (actor->deg == walk->facing) {
				walk->turning = false;
				if (walk->turnOnly) {
					remove(walk);
					// DF.EXE 0x410c39 removes a turn-only record before
					// dispatching endturn. Ned's livery greeting relies on it.
					if (actor->cast && actor->cast->data.version == 1) turned.push_back(walk);
					continue;
				}
				turned.push_back(walk);
			}
			continue;
		}
		walk->progress = MIN<double>(walk->distance, walk->progress + MAX<int32>(1, actor->speed));
		const double fraction = walk->progress / walk->distance;
		if (!walk->path.empty()) {
			const double at = fraction * walk->path.back().cumulative; uint32 leg = 1;
			while (leg < walk->path.size() - 1 && walk->path[leg].cumulative < at) ++leg;
			const MotionPoint &a = walk->path[leg - 1], &b = walk->path[leg];
			const double length = b.cumulative - a.cumulative, u = length > 0 ? CLIP<double>((at - a.cumulative) / length, 0, 1) : 1;
			actor->x = int32(floor(a.x + (b.x - a.x) * u + 0.5));
			actor->y = int32(floor(a.y + (b.y - a.y) * u + 0.5));
			actor->z = int32(floor(a.z + (b.z - a.z) * u + 0.5));
			actor->deg = int32(floor(atan2(b.y - a.y, b.x - a.x) * 256 / (2 * M_PI) + 0.5)) & 255;
		} else {
			actor->x = int32(floor(walk->sx + walk->dx * fraction + 0.5));
			actor->y = int32(floor(walk->sy + walk->dy * fraction + 0.5));
			actor->z = int32(floor(walk->sz + walk->dz * fraction + 0.5));
		}
		if (fraction >= 1) {
			remove(walk);
			if (actor->pose == "walk") { actor->pose = "stand"; actor->step = 0; }
			actor->star = walk->destination; arrived.push_back(walk);
		}
	}
	// Callbacks can start movement and wait for it with forceupdate (Georgia
	// entering B70 does this). Only protect the update pass against re-entry;
	// keeping the guard through script callbacks deadlocks their new movement.
	_servicing = false;
	for (uint32 i = 0; i < turned.size() && _runtime.current(context); ++i) {
		const auto actor = turned[i]->actor;
		if ((!turned[i]->turnOnly && !contains(turned[i])) || _casts.get(actor->name) != actor) continue;
		ScriptFrame turning = _runtime.eventFrame("walk", "endturn");
		_casts.send(true, actor->name, "endturn", Values(), turning, true);
	}
	for (uint32 i = 0; i < arrived.size() && _runtime.current(context); ++i) {
		const auto actor = arrived[i]->actor;
		if (_casts.get(actor->name) != actor || !actor->script->script.handlers.contains("endwalk")) continue;
		ScriptFrame arrival = _runtime.eventFrame("walk", "endwalk");
		_casts.send(true, actor->name, "endwalk", Values(), arrival, true);
	}
	_runtime.eventConsumed = consumed; _servicing = false; return _runtime.current(context);
}
}
