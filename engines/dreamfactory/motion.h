/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_MOTION_H
#define DREAMFACTORY_MOTION_H
#include "dreamfactory/actors.h"
#include "dreamfactory/stars.h"
namespace DreamFactory {
struct MotionPoint { double x = 0, y = 0, z = 0, cumulative = 0; };
struct ActorWalk {
	Common::SharedPtr<Actor> actor;
	double sx = 0, sy = 0, sz = 0, dx = 0, dy = 0, dz = 0, distance = 1, progress = 0;
	int32 facing = 0;
	bool turning = true, turnOnly = false, paused = false;
	Common::String destination;
	Common::Array<MotionPoint> path;
};
// The shared v1/v4 movement service. One record per actor, serviced before
// coarse script loops, including while a script is waiting for movement.
class MotionSystem {
public:
	MotionSystem(CastSystem &casts, Runtime &runtime) : _casts(casts), _runtime(runtime) {}
	void reset() { walks.clear(); }
	void start(const Common::String &name, int32 x, int32 y, int32 z, const Common::String &destination = "custom");
	void turn(const Common::String &name, int32 facing);
	void startPath(const Common::String &name, const StarPath &path, const Common::String &destination, bool resume);
	void prune();
	void stop(const Common::String &name);
	void pause(const Common::String &name, bool paused);
	Common::SharedPtr<ActorWalk> get(const Common::String &name) const;
	bool service();
	Common::Array<Common::SharedPtr<ActorWalk> > walks;
private:
	CastSystem &_casts;
	Runtime &_runtime;
	bool _servicing = false;
	bool contains(const Common::SharedPtr<ActorWalk> &walk) const;
	void remove(const Common::SharedPtr<ActorWalk> &walk);
};
}
#endif
