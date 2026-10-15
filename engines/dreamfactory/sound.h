/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SOUND_H
#define DREAMFACTORY_SOUND_H
#include "dreamfactory/audio.h"
#include "dreamfactory/resources.h"
#include "dreamfactory/enhancements.h"
#include "audio/mixer.h"
#include "common/mutex.h"
namespace Audio { class AudioStream; }
namespace DreamFactory {
struct SoundLoopControl {
	Common::Mutex mutex;
	bool loop = false;
};
struct ThemePart {
	Common::SharedPtr<AudioClip> clip;
	Common::String bank;
	uint32 bankID = 0;
};
// Main-thread owners retain PCM. The mixer borrows this state and only moves
// its cursor under the mutex; it never changes shared-pointer references.
struct ThemeQueue {
	Common::Mutex mutex;
	Common::Array<ThemePart> parts;
	uint32 current = 0;
	uint64 phase = 0; // Source sample position, in units of 1/44100 sample.
	int32 repeat = -1;
	bool ready(); // Caller holds mutex.
	void prune(); // Main thread only; caller holds mutex.
};
Audio::AudioStream *makeThemeStream(ThemeQueue *queue, bool stereo = false);
// Borrow immutable PCM and optional controls. The main-thread owner must keep
// them alive until the mixer has stopped the handle; Common::SharedPtr's
// reference counts must not be modified by the mixer thread.
Audio::AudioStream *makePCMStream(const AudioClip *clip, bool loop = false, uint32 start = 0, SoundLoopControl *control = nullptr);
class SoundSystem {
public:
	SoundSystem(Resources &resources, Audio::Mixer *mixer) : _resources(resources), _mixer(mixer) {}
	~SoundSystem() { reset(); }
	void reset();
	EnhancementPack enhancements;
	bool open(const Common::String &name, const Common::String &source = "");
	void close(const Common::String &name);
	enum SoundMode { kSingle, kMultiple, kDual, kBoth, kVoice };
	bool playSound(const Common::String &name, SoundMode mode = kSingle);
	void setTitanicRetail(bool retail) { _titanicRetail = retail; if (retail) _titanicDemo = false; }
	bool isTitanicRetail() const { return _titanicRetail; }
	void setTitanicDemo(bool demo) { _titanicDemo = demo; }
	bool isTitanicDemo() const { return _titanicDemo; }
	bool hasCompositeSoundIDs() const { return !_titanicRetail; }
	Common::SharedPtr<AudioClip> namedClip(Common::String name);
	bool playClip(Common::String name, const Common::SharedPtr<AudioClip> &clip, Audio::SoundHandle &handle);
	bool playAmbient(Common::String name, double gain, double pan, Audio::SoundHandle &handle, SoundMode mode = kMultiple);
	void positionAmbient(const Audio::SoundHandle &handle, double gain, double pan);
	void stopAmbient(Common::String name);
	bool handleActive(const Audio::SoundHandle &handle) const { return _mixer && _mixer->isSoundHandleActive(handle); }
	void stopHandle(const Audio::SoundHandle &handle) { if (_mixer) _mixer->stopHandle(handle); }
	bool playTheme(const Common::String &name);
	bool playMovieTheme(const Common::String &name, const Common::Array<Common::SharedPtr<AudioClip> > &clips, uint32 repeat);
	bool themeActive() const;
	void haltSounds();
	void haltVoice();
	bool playVoiceClip(const Common::String &name, const Common::SharedPtr<AudioClip> &clip, bool loop = false, Audio::SoundHandle *handle = nullptr);
	void haltTheme();
	bool done(bool voice) const;
	Common::String currentSound(uint32 channel = 1) const;
	Common::String currentVoice() const;
	Common::String currentTheme() const;
	void soundLoop(Common::String name, bool loop);
	bool soundLoop(Common::String name) const;
	int32 soundVolume(Common::String name) const;
	int32 soundPan(Common::String name) const;
	void setSoundVolume(Common::String name, int32 value);
	void setSoundPan(Common::String name, int32 value);
	int32 waveVolume() const { return _waveVolume; }
	void setWaveVolume(int32 value);
	int32 themeVolume() const { return _themeVolume; }
	int32 themeVolume(const Common::String &name) const;
	void setThemeVolume(const Common::String &name, int32 value);
	Common::Array<Common::String> soundNames(const Common::String &bank) const;
	Common::Array<Common::String> trackNames() const;
private:
	friend class SaveState;
	friend class SoundAudit;
	struct Bank {
		Common::String key, source;
		AudioBank data;
		uint32 id = 0;
		int32 themeVolume = 255;
		uint32 priorityBase = 0;
		Common::HashMap<Common::String, bool> loops;
		Common::HashMap<Common::String, int32> volumes, pans;
		Common::HashMap<uint32, Common::SharedPtr<AudioClip> > clips;
		Common::SharedPtr<AudioClip> theme;
		bool enhancementChecked = false;
		Common::Array<Common::SharedPtr<AudioClip> > enhancedParts;
		int32 enhancedRepeat = -1;
	};
	struct Play {
		Common::SharedPtr<AudioClip> clip;
		Common::SharedPtr<SoundLoopControl> control;
		Audio::SoundHandle handle;
		Common::String name, bank;
		uint32 bankID = 0;
		bool voice = false, loop = false, positional = false;
		bool wrapped = false; // Transient: elapsed mixer time has passed loop boundaries.
		byte positionalVolume = 255;
		uint32 soundID = 0;
		uint32 startSample = 0;
		int8 balance = 0;
	};
	Resources &_resources;
	Audio::Mixer *_mixer;
	Common::Array<Common::SharedPtr<Bank> > _banks;
	Common::Array<Play> _plays;
	Play _slots[2], _voice, _theme;
	Common::SharedPtr<ThemeQueue> _themeQueue;
	// Only populated while decoding legacy name-global controls (v1-v6).
	Common::HashMap<Common::String, bool> _loops;
	Common::HashMap<Common::String, int32> _volumes, _pans;
	Common::HashMap<Common::String, int32> _themeVolumes;
	int32 _waveVolume = 9, _themeVolume = 255;
	bool _titanicRetail = false;
	bool _titanicDemo = false;
	Bank *find(const Common::String &name) const;
	Bank *findBankID(uint32 id) const;
	Bank *soundBank(Common::String &name) const;
	Common::SharedPtr<AudioClip> decode(Bank &bank, uint32 container);
	bool active(const Play &play) const;
	void stop(Play &play);
	void prune();
	byte volume(const Play &play) const;
	byte themeGain() const { return byte((int64(_themeVolume) * _waveVolume) / 9); }
	uint32 effectSlots(uint32 id, SoundMode mode) const;
	Audio::AudioStream *stream(Play &play);
	bool queueTheme(const Common::String &name, const Common::String &bank, const Common::Array<Common::SharedPtr<AudioClip> > &clips, int32 repeat, byte gain, uint32 bankID = 0);
	void startTheme();
	bool themeSnapshot(ThemeQueue &out) const;
};
}
#endif
