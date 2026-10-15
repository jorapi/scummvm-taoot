/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_PROPS_H
#define DREAMFACTORY_PROPS_H
#include "dreamfactory/shop.h"
#include "dreamfactory/geometry.h"
#include "dreamfactory/resources.h"
#include "dreamfactory/runtime.h"
#include "common/ptr.h"
namespace DreamFactory {
struct LoadedShop {
	Common::String source;
	Common::String name;
	Shop data;
	Common::SharedPtr<ScriptInstance> main;
	Common::Array<Common::SharedPtr<ScriptInstance> > scripts;
	bool closing = false;
	bool persistent = false;
	const Sprite *frame(uint32 location);
	Common::HashMap<uint32, Sprite> frameCache;
};
struct Prop {
	Common::String name, set, view, star;
	Common::SharedPtr<LoadedShop> shop;
	Common::SharedPtr<ScriptInstance> script;
	uint32 group = 0, step = 0;
	Common::Array<uint32> order;
	bool directional = false, locked = false, variants = false, animating = false;
	uint64 degreeEvent = 0, lastTick = 0;
	bool copy = false, visible = false, hidden = false, worldSpace = false, screenPlaced = false, starPending = false;
	int32 x = 0, y = 0, z = 0, anchorX = 256, anchorY = 192, deg = 0;
	int32 scale = 0, speed = 0, dist = 0, zclip = 0;
	Value owner = Value(Common::String("none")), value;
	const PropState *state() const;
	uint32 frameIndex() const;
	uint32 frameCount() const;
	void selectView(Common::String name, uint64 event);
	void selectDegree(int32 value, uint64 event);
	void becomeWorld();
	bool contains(int32 x, int32 y) const;
};
struct WorldPropEntry { Common::SharedPtr<Prop> prop; ProjectedPoint point; };
class PropSystem {
public:
	PropSystem(Resources &resources, Runtime &runtime) : _resources(resources), _runtime(runtime) {}
	void reset() { props.clear(); shops.clear(); }
	bool open(const Common::String &name, ScriptFrame *parent = nullptr, bool callbacks = true, const Common::String &source = "");
	void close(const Common::String &name, ScriptFrame *parent = nullptr);
	Common::SharedPtr<Prop> get(const Common::String &name) const;
	Common::SharedPtr<LoadedShop> findShop(const Common::String &name) const;
	void instance(const Common::String &src, const Common::String &dst);
	void remove(const Common::String &name);
	bool warm(const Common::String &name, bool wholeShop = false);
	Value send(bool prop, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent = false);
	bool running(const Common::String &name, const Common::String &handler) const;
	void animate(uint64 now);
	Common::Array<WorldPropEntry> worldDrawList(const WorldCamera &camera, const Common::String &set) const;
	bool compositeWorldOne(const WorldPropEntry &entry, Bytes &rgba, uint32 width, uint32 height, const byte *palette, const WorldCamera &camera, const Occlusion &occlusion);
	bool compositeWorld(Bytes &rgba, uint32 width, uint32 height, const byte *palette, const WorldCamera &camera, const Common::String &set, const Occlusion &occlusion);
	Common::SharedPtr<Prop> hitWorld(int32 x, int32 y, const WorldCamera &camera, const Common::String &set, const Occlusion &occlusion);
	bool composite(Bytes &rgba, uint32 width, uint32 height, const byte *palette, bool persistentOnly = false);
	Common::SharedPtr<Prop> hitTest(int32 x, int32 y, bool persistentOnly = false);
	Common::Array<Common::SharedPtr<LoadedShop> > shops;
	Common::Array<Common::SharedPtr<Prop> > props;
private:
	Resources &_resources;
	Runtime &_runtime;
	Common::Array<Common::SharedPtr<Prop> > drawList(bool persistentOnly) const;
};
}
#endif
