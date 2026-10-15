/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_PUPPETS_H
#define DREAMFACTORY_PUPPETS_H
#include "dreamfactory/puppet.h"
#include "dreamfactory/audio.h"
#include "dreamfactory/resources.h"
#include "dreamfactory/runtime.h"
#include "common/ptr.h"
namespace DreamFactory {
struct PuppetChoice { Common::String text; int32 id = 0; };
struct LoadedPuppet {
	Common::String file, subtitle;
	Puppet data;
	Common::Array<Common::SharedPtr<ScriptInstance> > scripts;
	Common::SharedPtr<ScriptInstance> main;
	Common::Array<PuppetChoice> choices;
	Common::Array<PuppetChoice> lastChoices;
	int32 chosen = -1, pressed = -1, lastChosen = -1;
	bool pressInside = false;
	PuppetPose pose, defaultPose;
	int32 stance = 0, defaultStance = 0;
	bool visible = true, hasPose = false, hasDefaultPose = false;
	bool interrupted = false;
	bool subtitleBand = false;
	Common::Array<uint32> voiceQueue;
};
// Original row tracker: release in the pressed row, with a ten-tick floor.
class PuppetChoices {
public:
	void start(const Common::SharedPtr<LoadedPuppet> &puppet);
	void down(int32 row, uint64 tick);
	void move(int32 row);
	void up(int32 row);
	void tick(uint64 tick);
	void focus(int32 direction);
	void confirm(uint64 tick, int32 row = -1);
	bool hasFocus() const { return _puppet && _puppet->chosen >= 0; }
	void escape() { finish(-1); }
	bool active() const { return bool(_puppet); }
	uint64 generation() const { return _generation; }
	int32 answer() const { return _answer; }
	bool pressVisible(uint64 tick) const { return _puppet && _puppet->pressed >= 0 && (tick < _until || _puppet->pressInside); }
private:
	Common::SharedPtr<LoadedPuppet> _puppet;
	uint64 _until = 0;
	uint64 _generation = 0;
	int32 _answer = -1, _released = -1;
	int32 _focus = -1;
	void finish(int32 answer);
};
class PuppetSpeechHost {
public:
	virtual ~PuppetSpeechHost() {}
	virtual void puppetVoice(const Common::String &name, const Common::SharedPtr<AudioClip> &clip) = 0;
	virtual void puppetSilence() = 0;
};
// Modal speech clock; drawing reads the loaded puppet's current pose/subtitle.
// The owner stays alive if an enclosing conversation is replaced or closed.
class PuppetSpeech {
public:
	explicit PuppetSpeech(PuppetSpeechHost &host) : _host(host) {}
	bool start(const Common::SharedPtr<LoadedPuppet> &puppet, const Common::String &ident, uint64 now, bool queue = true, bool fast = false);
	void tick(uint64 now);
	void skip(bool sequence = false);
	void abandon();
	bool active() const { return bool(_puppet); }
	Common::String error;
private:
	PuppetSpeechHost &_host;
	Common::SharedPtr<LoadedPuppet> _puppet;
	Common::Array<PuppetPose> _poses;
	uint64 _start = 0, _deadline = 0;
	void finish();
};
class PuppetSystem {
public:
	PuppetSystem(Resources &resources, Runtime &runtime) : _resources(resources), _runtime(runtime) {}
	void reset() { current.reset(); }
	bool open(Common::String file);
	Common::String name() const;
	bool base(const Common::String &ident);
	void clear();
	void bevel(const Common::String &text, int32 id);
	Value send(const Common::String &script, const Common::String &handler, const Values &args, ScriptFrame &parent);
	Common::SharedPtr<LoadedPuppet> current;
private:
	Resources &_resources;
	Runtime &_runtime;
};
}
#endif
