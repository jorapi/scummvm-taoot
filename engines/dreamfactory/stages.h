/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_STAGES_H
#define DREAMFACTORY_STAGES_H
#include "dreamfactory/stage.h"
#include "dreamfactory/resources.h"
#include "dreamfactory/runtime.h"
#include "common/ptr.h"
namespace DreamFactory {
struct LoadedStage {
	Common::String source;
	Common::String name;
	Stage data;
	Common::SharedPtr<ScriptInstance> main;
	Common::Array<Common::SharedPtr<ScriptInstance> > flats;
	Common::Array<Common::Array<Common::SharedPtr<ScriptInstance> > > buttons;
	bool closing = false;
};
class StageHost {
public:
	virtual ~StageHost() {}
	virtual void flatChanged() = 0;
};
class StageSystem {
public:
	StageSystem(Resources &resources, Runtime &runtime, StageHost *host = nullptr) : _resources(resources), _runtime(runtime), _host(host) {}
	void reset() { current.reset(); flat = -1; ++_revision; if (_host) _host->flatChanged(); }
	bool open(Common::String name, ScriptFrame *parent = nullptr, bool callbacks = true, const Common::String &source = "");
	void close(ScriptFrame *parent = nullptr);
	bool go(const Common::String &name, ScriptFrame *parent = nullptr);
	int32 resolve(const Common::String &name) const;
	Common::String currentFlat() const;
	Value send(bool stage, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent, bool rootEvent = false);
	Value button(const Common::String &flat, const Common::String &name, const Common::String &handler, const Values &args, ScriptFrame &parent);
	bool running(const Common::String &name, const Common::String &handler) const;
	Common::SharedPtr<LoadedStage> current;
	int32 flat = -1;
	uint32 version = 4;
private:
	ScriptResult flatEvent(const Common::SharedPtr<LoadedStage> &loaded, int32 index, const Common::String &handler, ScriptFrame *parent);
	Resources &_resources;
	Runtime &_runtime;
	StageHost *_host;
	uint32 _revision = 0;
};
}
#endif
