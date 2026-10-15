/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ACTORS_H
#define DREAMFACTORY_ACTORS_H
#include "dreamfactory/cast.h"
#include "dreamfactory/geometry.h"
#include "dreamfactory/resources.h"
#include "dreamfactory/runtime.h"
#include "common/ptr.h"
namespace DreamFactory {
struct LoadedCast {
	Common::String source;
	Common::String name;
	Cast data;
	Common::HashMap<uint32, Sprite> frameCache;
	const Sprite *frame(uint32 location);
	Common::SharedPtr<ScriptInstance> main;
	Common::Array<Common::SharedPtr<ScriptInstance> > scripts;
};
struct Actor {
	Common::String name, set, star, pose = "stand";
	Common::SharedPtr<LoadedCast> cast;
	Common::SharedPtr<ScriptInstance> script;
	uint32 member = 0, step = 0;
	bool copy = false, visible = false, worldSpace = true, starPending = false;
	int32 x = 0, y = 0, z = 0, anchorX = 0, anchorY = 0, deg = 0;
	int32 scale = 0, speed = 0, turn = 16, zclip = 0, dist = 0;
	int16 hitWidth = 0, hitHeight = 0;
	const CastPose *poseData() const;
	const CastFrame *frameRecord(const WorldCamera *camera) const;
	int32 referenceScale() const;
	Value owner = Value(Common::String("none")), value;
};
struct ActorDrawEntry { Common::SharedPtr<Actor> actor; ProjectedPoint point; };
class CastSystem {
public:
	CastSystem(Resources &resources, Runtime &runtime) : _resources(resources), _runtime(runtime) {}
	void animate();
	Common::Array<ActorDrawEntry> drawList(const WorldCamera &camera, const Common::String &set) const;
	bool compositeOne(const ActorDrawEntry &entry, Bytes &rgba, uint32 width, uint32 height, const byte *palette, const WorldCamera &camera, const Occlusion &occlusion);
	bool compositeScreen(Bytes &rgba, uint32 width, uint32 height, const byte *palette, const Common::String &set);
	Common::SharedPtr<Actor> hitTest(int32 x, int32 y, const WorldCamera &camera, const Common::String &set, const Occlusion &occlusion);
	bool onScreen(const Actor &actor, const WorldCamera &camera, const Common::String &set);
	void reset() { actors.clear(); casts.clear(); }
	bool open(const Common::String &name, ScriptFrame *parent = nullptr, bool callbacks = true, const Common::String &source = "");
	void close(const Common::String &name);
	Common::SharedPtr<Actor> get(const Common::String &name) const;
	void instance(const Common::String &src, const Common::String &dst);
	void remove(const Common::String &name);
	bool warm(const Common::String &name, bool wholeCast = false);
	Value send(bool actor, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent = false);
	bool running(const Common::String &name, const Common::String &handler) const;
	Common::Array<Common::SharedPtr<LoadedCast> > casts;
	Common::Array<Common::SharedPtr<Actor> > actors;
private:
	Resources &_resources;
	Runtime &_runtime;
};
}
#endif
