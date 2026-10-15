/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_CRICKETS_H
#define DREAMFACTORY_CRICKETS_H
#include "dreamfactory/sound.h"
#include "common/random.h"
namespace DreamFactory {
struct Cricket {
	Common::String name, set;
	int32 x = 0, y = 0, radius = 1, base = 0, jitter = -1;
	int64 count = 0;
	bool paused = false, playing = false, armed = false;
	Audio::SoundHandle handle;
};
class CricketSystem {
public:
	explicit CricketSystem(SoundSystem &sound) : _sound(sound), _random("dreamfactory-ambient") { reset(); }
	~CricketSystem() { reset(); }
	void reset();
	bool make(Common::String name, int32 x, int32 y, int32 radius, int32 base, int32 jitter);
	void stop(const Common::String &name);
	void pause(const Common::String &name, bool paused);
	bool has(const Common::String &name) const;
	uint32 count() const;
	Common::String index(int32 number) const;
	void service();
	void setRoom(Common::String name);
	void listener(int32 x, int32 y, int32 facing) { _x = x; _y = y; _facing = facing; _listener = true; }
	void clearListener() { _listener = false; }
	Common::Array<Common::SharedPtr<Cricket> > crickets;
	Common::String room = "none";
private:
	friend class SaveState;
	SoundSystem &_sound;
	Common::RandomSource _random;
	int32 _x = 0, _y = 0, _facing = 0;
	bool _listener = false;
	int64 countdown(const Cricket &cricket);
	void silenceAbsent();
	void spatial(const Cricket &cricket, double &gain, double &pan) const;
	void retire(uint32 slot);
};
}
#endif
