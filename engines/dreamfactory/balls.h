/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_BALLS_H
#define DREAMFACTORY_BALLS_H
#include "dreamfactory/props.h"
#include "dreamfactory/rooms.h"
#include "dreamfactory/actors.h"
namespace DreamFactory {
struct Ball {
	Common::String name;
	int16 x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
	int16 gravity = 0, bounce = 0, frames = 0, paused = 0;
};
// Dust's v1 makeball family shares opcode numbers with v4 ambient crickets.
// DF.EXE 0x411050/0x4112b0: sixteen slots, signed-word physics at 20 Hz.
class BallSystem {
public:
	BallSystem(PropSystem &props, CastSystem &casts, RoomSystem &rooms, Runtime &runtime, Common::Point &playerHitbox) : _props(props), _casts(casts), _rooms(rooms), _runtime(runtime), _playerHitbox(playerHitbox) { reset(); }
	void reset() { balls.clear(); balls.resize(16); }
	bool make(const Common::String &name, int32 vx, int32 vy, int32 vz, int32 gravity, int32 bounce, int32 frames);
	void stop(const Common::String &name);
	void pause(const Common::String &name, bool paused);
	bool has(const Common::String &name) const;
	Common::String index(int32 n) const;
	int32 count() const;
	bool service();
	Common::Array<Common::SharedPtr<Ball> > balls;
private:
	PropSystem &_props;
	CastSystem &_casts;
	RoomSystem &_rooms;
	Runtime &_runtime;
	Common::Point &_playerHitbox;
	bool _servicing = false;
	bool building(int16 x, int16 y) const;
};
}
#endif
