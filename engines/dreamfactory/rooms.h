/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ROOMS_H
#define DREAMFACTORY_ROOMS_H
#include "dreamfactory/room.h"
#include "dreamfactory/geometry.h"
#include "dreamfactory/stages.h"
#include "dreamfactory/image.h"
namespace DreamFactory {
struct LoadedRoom {
	Common::String source;
	Common::String file;
	Room data;
	Common::SharedPtr<ScriptInstance> main;
	Common::Array<Common::SharedPtr<ScriptInstance> > scenes;
	Common::Array<Common::Array<Common::Array<Common::SharedPtr<ScriptInstance> > > > paintings;
	bool closing = false;
	uint32 lifecycle = 0;
};
class RoomHost {
public:
	virtual ~RoomHost() {}
	virtual void bindRoom(const Room *room) = 0;
	virtual void roomListener(const Scene &scene, const View &view) = 0;
	virtual void presentRoom(const Room &room, const Frame &frame) = 0;
	virtual bool waitRoomFrame(ScriptFrame *parent) = 0;
};
class RoomSystem {
public:
	RoomSystem(Resources &resources, Runtime &runtime, StageSystem &stages, RoomHost &host) : _resources(resources), _runtime(runtime), _stages(stages), _host(host) {}
	void reset();
	bool open(Common::String file, const Common::String &scene = "", const Common::String &view = "", ScriptFrame *parent = nullptr, bool callbacks = true, const Common::String &source = "");
	void close(ScriptFrame *parent = nullptr);
	bool jump(const Common::String &scene, const Common::String &view, ScriptFrame *parent = nullptr);
	bool move(int direction, ScriptFrame *parent = nullptr);
	int32 sceneIndex(const Common::String &name) const;
	int32 viewIndex(int32 scene, const Common::String &name) const;
	Common::String name() const;
	Common::String sceneName() const;
	Common::String viewName() const;
	Value send(bool set, const Common::String &scene, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent = false);
	Value painting(const Common::String &scene, const Common::String &view, const Common::String &paint, const Common::String &handler, const Values &args, ScriptFrame &parent);
	bool running(const Common::String &scene, const Common::String &handler) const;
	bool roadAhead(int32 scene, int32 view) const;
	Common::SharedPtr<LoadedRoom> current;
	int32 scene = -1, view = -1;
	bool visible = true, moving = false, turning = false;
	Frame frame;
	CameraPose camera;
	bool worldCamera(WorldCamera &out, int32 heightBias) const;
private:
	friend class SaveState;
	Resources &_resources;
	Runtime &_runtime;
	StageSystem &_stages;
	RoomHost &_host;
	uint32 _revision = 0;
	int32 _lastFacing = 0;
	bool _hasFacing = false;
	bool position(int32 scene, int32 view, bool notify = true);
	bool animate(const Sequence &sequence, ScriptFrame *parent, uint32 revision);
	bool lifecycle(const Common::SharedPtr<LoadedRoom> &room, const Common::String &handler, int32 scene, ScriptFrame *parent);
	Value dispatch(const Common::SharedPtr<LoadedRoom> &room, Common::Array<ScriptInstance *> chain, const Common::String &handler, const Values &args, const Common::String &target, ScriptFrame *parent, bool ownContext, bool boot);
};
}
#endif
