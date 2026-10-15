/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SCHEDULER_H
#define DREAMFACTORY_SCHEDULER_H
#include "common/array.h"
#include "common/ptr.h"
#include "common/str.h"
namespace DreamFactory {
struct GameLoop {
	Common::String kind, name, handler;
	int32 count = 1, period = 1;
	bool paused = false;
};
class LoopHost {
public:
	virtual ~LoopHost() {}
	virtual void serviceCrickets() {}
	virtual void serviceAnimation(uint64 now) {}
	virtual bool serviceMotion() { return true; }
	virtual bool canFireLoop(const GameLoop &loop) = 0;
	virtual bool fireLoop(const GameLoop &loop) = 0;
};
class Scheduler {
public:
	void reset(uint64 now = 0);
	bool make(Common::String kind, Common::String name, Common::String handler, int32 period);
	void stop(Common::String kind, Common::String name);
	void pause(Common::String kind, Common::String name, bool paused);
	bool has(Common::String kind, Common::String name) const;
	// No catch-up burst after a stall. Busy scripts count timers down to zero;
	// only an idle pass or an explicit cooperative yield may dispatch them.
	void advance(uint64 now, int32 frameTicks, bool dispatch, LoopHost &host);
	uint32 frame = 0;
	Common::Array<Common::SharedPtr<GameLoop> > loops;
	static uint64 ticks(uint64 ms) { return ms * 3 / 50; }
private:
	friend class SaveState;
	uint64 _time = 0, _service = 0, _display = 0;
	bool contains(const Common::SharedPtr<GameLoop> &loop) const;
	void service(bool smooth, bool countdown, bool dispatch, LoopHost &host);
};
}
#endif
