/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ENGINE_H
#define DREAMFACTORY_ENGINE_H
#include "engines/engine.h"
#include "dreamfactory/room.h"
#include "dreamfactory/image.h"
#include "dreamfactory/save_host.h"
#include "common/path.h"
namespace DreamFactory {
class Session;
class DreamFactoryEngine : public Engine, private SaveHost {
public:
	DreamFactoryEngine(OSystem *system) : Engine(system), _scene(0), _view(0), _set(0) {}
	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override { return f == kSupportsReturnToLauncher || f == kSupportsSavingDuringRuntime || f == kSupportsLoadingDuringRuntime || f == kSupportsSubtitleOptions; }
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canSaveAutosaveCurrently() override;
	void pauseEngineIntern(bool pause) override;
private:
	Session *_session = nullptr;
	bool _scriptSave = false;
	bool saveGame() override;
	bool loadGame() override;
	Room _room;
	Frame _frame;
	Common::Array<Common::Path> _sets;
	uint32 _scene, _view, _set;
	bool openSet(uint32 index);
	bool stand(uint32 scene, uint32 view);
	bool animate(const Sequence &frames);
	void turn(int dir);
	void walk();
	void moveV1(int direction);
	void draw();
};
}
#endif
