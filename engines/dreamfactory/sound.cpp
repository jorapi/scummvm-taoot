/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/sound.h"
#include "audio/audiostream.h"
namespace DreamFactory {
bool ThemeQueue::ready() {
	while (current < parts.size()) {
		const AudioClip &clip = *parts[current].clip;
		const uint64 end = uint64(clip.frames()) * 44100;
		if (phase < end) return true;
		const uint64 remaining = phase - end;
		const uint32 rate = clip.rate;
		++current;
		if (current == parts.size() && repeat >= 0) current = uint32(repeat);
		if (current == parts.size()) { phase = 0; return false; }
		phase = remaining * parts[current].clip->rate / rate;
	}
	return false;
}
void ThemeQueue::prune() {
	const uint32 keep = repeat < 0 ? current : MIN<uint32>(current, uint32(repeat));
	if (keep) parts.erase(parts.begin(), parts.begin() + keep);
	current -= keep; if (repeat >= 0) repeat -= keep;
}
class ThemeStream : public Audio::AudioStream {
public:
	ThemeStream(ThemeQueue *queue, bool stereo) : _queue(queue), _stereo(stereo) {}
	int readBuffer(int16 *buffer, const int count) override {
		Common::StackLock lock(_queue->mutex);
		int n = 0;
		const uint32 outputChannels = _stereo ? 2 : 1;
		while (n + int(outputChannels) <= count && _queue->ready()) {
			const AudioClip &clip = *_queue->parts[_queue->current].clip;
			const uint32 at = uint32(_queue->phase / 44100), fraction = uint32(_queue->phase % 44100);
			for (uint32 channel = 0; channel < outputChannels; ++channel) {
				const uint32 c = clip.channels == 2 ? channel : 0;
				const int32 a = clip.samples[at * clip.channels + c], b = clip.samples[MIN<uint32>(at + 1, clip.frames() - 1) * clip.channels + c];
				const int64 value = int64(a) * (44100 - fraction) + int64(b) * fraction;
				buffer[n++] = int16(value >= 0 ? (value + 22050) / 44100 : -((-value + 22049) / 44100));
			}
			_queue->phase += clip.rate;
		}
		return n;
	}
	bool isStereo() const override { return _stereo; }
	int getRate() const override { return 44100; }
	bool endOfData() const override { Common::StackLock lock(_queue->mutex); return !_queue->ready(); }
private:
	ThemeQueue *_queue;
	bool _stereo;
};
Audio::AudioStream *makeThemeStream(ThemeQueue *queue, bool stereo) { return new ThemeStream(queue, stereo); }
class PCMStream : public Audio::RewindableAudioStream {
public:
	explicit PCMStream(const AudioClip *clip, uint32 start = 0, SoundLoopControl *control = nullptr) : _clip(clip), _control(control), _position(MIN(start, clip->samples.size())) {}
	int readBuffer(int16 *buffer, const int count) override {
		if (count <= 0) return 0;
		if (_control) { Common::StackLock lock(_control->mutex); return read(buffer, count, _control->loop); }
		return read(buffer, count, false);
	}
	bool isStereo() const override { return false; }
	int getRate() const override { return int(_clip->rate); }
	bool endOfData() const override {
		if (_clip->samples.empty()) return true;
		if (_control) { Common::StackLock lock(_control->mutex); if (_control->loop) return false; }
		return _position >= _clip->samples.size();
	}
	bool rewind() override { _position = 0; return true; }
private:
	const AudioClip *_clip;
	SoundLoopControl *_control;
	uint32 _position;
	int read(int16 *buffer, int count, bool loop) {
		int total = 0;
		while (total < count && !_clip->samples.empty()) {
			if (_position == _clip->samples.size()) { if (!loop) break; _position = 0; }
			const uint32 length = MIN<uint32>(count - total, _clip->samples.size() - _position);
			memcpy(buffer + total, _clip->samples.data() + _position, length * sizeof(int16));
			_position += length; total += length;
		}
		return total;
	}
};
Audio::AudioStream *makePCMStream(const AudioClip *clip, bool loop, uint32 start, SoundLoopControl *control) {
	PCMStream *pcm = new PCMStream(clip, start, control);
	if (control) return pcm;
	return loop ? Audio::makeLoopingAudioStream(pcm, 0) : pcm;
}
Audio::AudioStream *SoundSystem::stream(Play &play) {
	play.control.reset(new SoundLoopControl()); play.control->loop = play.loop; play.wrapped = play.loop;
	return makePCMStream(play.clip.get(), play.loop, play.startSample, play.control.get());
}
Common::SharedPtr<AudioClip> SoundSystem::namedClip(Common::String name) {
	name.toLowercase(); if (name.hasSuffix(".wav")) name = name.substr(0, name.size() - 4);
	for (uint32 i = 0; i < _banks.size(); ++i) if (_banks[i]->data.singles.contains(name)) return decode(*_banks[i], _banks[i]->data.singles[name]);
	return Common::SharedPtr<AudioClip>();
}
bool SoundSystem::playClip(Common::String name, const Common::SharedPtr<AudioClip> &clip, Audio::SoundHandle &handle) {
	if (!_mixer || !clip || clip->samples.empty()) return false;
	prune(); name.toLowercase();
	for (uint32 i = 0; i < _plays.size(); ++i) if (!_plays[i].voice && !_plays[i].loop) stop(_plays[i]);
	Play play; play.name = name; play.clip = clip;
	play.positionalVolume = byte(CLIP<int32>(soundVolume(name), 0, 255));
	play.balance = (_titanicRetail || _titanicDemo) ? int8(CLIP<int32>(soundPan(name) - 128, -127, 127)) : 0;
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &play.handle, makePCMStream(play.clip.get()), -1, volume(play), play.balance);
	_plays.push_back(play); _slots[0] = play; handle = play.handle; return true;
}
bool SoundSystem::active(const Play &play) const { return _mixer && !play.name.empty() && _mixer->isSoundHandleActive(play.handle); }
SoundSystem::Bank *SoundSystem::soundBank(Common::String &name) const {
	name.toLowercase();
	if (name.hasSuffix(".wav")) name = name.substr(0, name.size() - 4);
	for (const auto &bank : _banks) if (bank->data.singles.contains(name)) return bank.get();
	return nullptr;
}
void SoundSystem::stop(Play &play) { if (_mixer && !play.name.empty()) _mixer->stopHandle(play.handle); play = Play(); }
void SoundSystem::prune() { for (uint32 i = 0; i < _plays.size();) { if (!active(_plays[i])) _plays.remove_at(i); else ++i; } }
void SoundSystem::reset() {
	haltSounds(); haltVoice(); haltTheme(); _banks.clear(); _loops.clear(); _volumes.clear(); _pans.clear(); _themeVolumes.clear();
	_waveVolume = 9; _themeVolume = 255;
	_titanicRetail = false;
	_titanicDemo = false;
}
SoundSystem::Bank *SoundSystem::find(const Common::String &name) const {
	for (const auto &bank : _banks) if (bank->key.equalsIgnoreCase(name) || bank->data.name.equalsIgnoreCase(name)) return bank.get();
	return nullptr;
}
SoundSystem::Bank *SoundSystem::findBankID(uint32 id) const {
	for (const auto &bank : _banks) if (bank->id == id) return bank.get();
	return nullptr;
}
bool SoundSystem::open(const Common::String &name, const Common::String &source) {
	Bytes bytes; Common::SharedPtr<Bank> bank(new Bank());
	if (!_resources.read(source.empty() ? name : source, bytes) || !bank->data.load(bytes)) return false;
	bank->source = _resources.sourceName(source.empty() ? name : source);
	bank->id = 1; while (findBankID(bank->id)) ++bank->id;
	bank->key = name; bank->key.toLowercase(); _banks.push_back(bank);
	// Demo 44cfde and Dust 403185 renumber descriptors on open, but close
	// leaves the remaining IDs intact. Active physical slots retain copies.
	if (hasCompositeSoundIDs()) for (uint32 i = 0; i < _banks.size(); ++i) _banks[i]->priorityBase = i << 16;
	return true;
}
void SoundSystem::close(const Common::String &name) {
	Bank *direct = find(name); if (!direct) return;
	for (uint32 i = 0; i < _banks.size();) {
		Bank &bank = *_banks[i];
		if (&bank != direct) { ++i; continue; }
		for (uint32 p = 0; p < _plays.size(); ++p) if (_plays[p].bankID == bank.id) stop(_plays[p]);
		bool usesTheme = _theme.bankID == bank.id;
		if (_themeQueue) {
			Common::StackLock lock(_themeQueue->mutex);
			_themeQueue->ready(); _themeQueue->prune();
			for (const auto &part : _themeQueue->parts) if (part.bankID == bank.id) usesTheme = true;
		}
		if (usesTheme) haltTheme();
		_banks.remove_at(i);
		break;
	}
	prune();
}
Common::SharedPtr<AudioClip> SoundSystem::decode(Bank &bank, uint32 container) {
	if (bank.clips.contains(container)) return bank.clips[container];
	Common::SharedPtr<AudioClip> clip(new AudioClip());
	if (!bank.data.clip(container, *clip) || clip->samples.empty()) return Common::SharedPtr<AudioClip>();
	bank.clips[container] = clip; return clip;
}
byte SoundSystem::volume(const Play &play) const { return byte((int64(play.positionalVolume) * _waveVolume) / 9); }
uint32 SoundSystem::effectSlots(uint32 id, SoundMode mode) const {
	const uint32 first = active(_slots[0]) ? _slots[0].soundID : 0;
	const uint32 second = active(_slots[1]) ? _slots[1].soundID : 0;
	const uint32 lowest = first < second ? 0 : 1;
	const uint32 lowID = lowest == 0 ? first : second;
	const uint32 otherID = lowest == 0 ? second : first;
	if (!_titanicRetail && mode == kBoth) return 1 << lowest;
	if (!_titanicRetail && mode == kDual) mode = kSingle;
	if (mode == kBoth) return 3;
	if (mode == kDual) return (id > first ? 1 : 0) | (id > second ? 2 : 0);
	if (mode == kSingle) return id > lowID && id != otherID ? 1 << lowest : 0;
	return id >= lowID ? 1 << (id == otherID ? 1 - lowest : lowest) : 0;
}
bool SoundSystem::playSound(const Common::String &name, SoundMode mode) {
	prune(); Common::String key = name; key.toLowercase();
	if (key.hasSuffix(".wav")) key = key.substr(0, key.size() - 4);
	const bool voice = mode == kVoice;
	const bool loop = soundLoop(key);
	Bank *owner = nullptr; uint32 container = 0;
	for (uint32 i = 0; i < _banks.size(); ++i) if (_banks[i]->data.singles.contains(key)) { owner = _banks[i].get(); container = owner->data.singles[key]; break; }
	if (!owner || !_mixer) return false;
	uint32 slots = 0;
	const uint32 id = owner->data.soundIDs[key] | owner->priorityBase;
	if (!voice) {
		// Titanic retail 44cc96 assigns a bank-local, one-based descriptor ID;
		// lazy loading preserves it (44ddde..44de0b). 425150/240/330/400
		// arbitrate two physical channels using this ID, even across banks.
		// Demo 44e670 and Dust 404290 add the bank ordinal in the upper word;
		// their dual and both wrappers use the early policy in effectSlots().
		slots = effectSlots(id, mode);
		// A rejected effect is a successful command, not a missing resource.
		if (!slots) return true;
	}
	const Common::SharedPtr<AudioClip> clip = decode(*owner, container); if (!clip) return false;
	if (voice) haltVoice();
	const int8 pan = (_titanicRetail || _titanicDemo) ? int8(CLIP<int32>(soundPan(key) - 128, -127, 127)) : 0;
	for (uint32 channel = 0; channel < 2; ++channel) {
		if (voice ? channel != 0 : !(slots & (1 << channel))) continue;
		if (!voice) stop(_slots[channel]);
		Play play; play.name = key; play.bank = owner->key; play.bankID = owner->id; play.voice = voice; play.loop = loop; play.soundID = id;
		play.clip = clip; play.balance = pan;
		play.positionalVolume = byte(CLIP<int32>(soundVolume(key), 0, 255));
		_mixer->playStream(voice ? Audio::Mixer::kSpeechSoundType : Audio::Mixer::kSFXSoundType, &play.handle, stream(play), -1, volume(play), pan);
		_plays.push_back(play);
		if (voice) _voice = play; else _slots[channel] = play;
	}
	prune();
	return true;
}
bool SoundSystem::playAmbient(Common::String name, double gain, double pan, Audio::SoundHandle &handle, SoundMode mode) {
	prune(); name.toLowercase(); if (name.hasSuffix(".wav")) name = name.substr(0, name.size() - 4);
	Bank *bank = nullptr; uint32 location = 0;
	for (uint32 i = 0; i < _banks.size(); ++i) if (_banks[i]->data.singles.contains(name)) { bank = _banks[i].get(); location = bank->data.singles[name]; break; }
	if (!bank || !_mixer) return false;
	const uint32 id = bank->data.soundIDs[name] | bank->priorityBase;
	if (_titanicRetail && mode == kSingle) {
		for (uint32 channel = 0; channel < 2; ++channel) {
			Play &slot = _slots[channel];
			if (!active(slot) || slot.name != name || slot.bankID != bank->id || slot.soundID != id) continue;
			handle = slot.handle; slot.positional = true;
			for (uint32 i = 0; i < _plays.size(); ++i) if (_plays[i].handle == handle) _plays[i].positional = true;
			positionAmbient(handle, gain, pan); return true;
		}
	}
	const uint32 slots = effectSlots(id, mode);
	if (!slots) { handle = Audio::SoundHandle(); return false; }
	const auto clip = decode(*bank, location); if (!clip) return false;
	// Older experimental saves could contain positional voices outside the
	// physical slots. Rebind a selected loop instead of retaining an orphan.
	if (_titanicRetail && mode == kSingle) stopAmbient(name);
	Play play; play.name = name; play.bank = bank->key; play.bankID = bank->id; play.positional = true; play.soundID = id;
	play.positionalVolume = byte(CLIP<double>(gain, 0, 1) * 255 + 0.5); play.loop = soundLoop(name);
	const int8 balance = int8(CLIP<double>(pan, -1, 1) * 127);
	play.clip = clip; play.balance = balance;
	const uint32 channel = slots & 1 ? 0 : 1;
	stop(_slots[channel]);
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &play.handle, stream(play), -1, volume(play), balance);
	_plays.push_back(play); _slots[channel] = play; handle = play.handle; return true;
}
void SoundSystem::positionAmbient(const Audio::SoundHandle &handle, double gain, double pan) {
	if (!_mixer) return;
	const byte level = byte(CLIP<double>(gain, 0, 1) * 255 + 0.5);
	const int8 balance = int8(CLIP<double>(pan, -1, 1) * 127);
	Common::String name;
	for (uint32 i = 0; i < _plays.size(); ++i)
		if (_plays[i].positional && _plays[i].handle == handle) { name = _plays[i].name; break; }
	if (name.empty()) return;
	auto update = [&](Play &play) {
		if (!play.positional || play.name != name) return;
		play.positionalVolume = level; play.balance = balance;
	};
	for (uint32 i = 0; i < _plays.size(); ++i) {
		Play &play = _plays[i];
		if (!play.positional || play.name != name || !active(play)) continue;
		update(play);
		_mixer->setChannelVolume(play.handle, volume(play));
		_mixer->setChannelBalance(play.handle, balance);
	}
	// Keep the channel descriptor in sync for save/load and currentsound().
	for (uint32 i = 0; i < 2; ++i) update(_slots[i]);
}
void SoundSystem::stopAmbient(Common::String name) {
	name.toLowercase();
	for (uint32 i = 0; i < _plays.size(); ++i)
		if (!_plays[i].voice && _plays[i].name == name) stop(_plays[i]);
	prune();
}
bool SoundSystem::playTheme(const Common::String &name) {
	Bank *bank = name.empty() ? nullptr : find(name);
	if (name.empty()) for (uint32 i = 0; i < _banks.size(); ++i) if (!_banks[i]->data.loops.empty()) { bank = _banks[i].get(); break; }
	if (!bank || !_mixer || bank->data.loops.empty()) return false;
	if (!bank->enhancementChecked) {
		bank->enhancementChecked = true;
		Common::Array<Common::SharedPtr<AudioClip> > replacement;
		int32 repeat = -1;
		bool preserveIntro = false;
		if (enhancements.music(bank->source, replacement, repeat, &preserveIntro)) {
			// The authored prefix can contain speech, not just musical lead-in.
			// BEDRAD1, for example, plays its BBC broadcast before the piano.
			// Replace the music bed without discarding or repeating that prefix.
			const uint32 prefix = preserveIntro ? bank->data.loopRepeat : 0;
			bool valid = prefix <= bank->data.loops.size();
			for (uint32 i = 0; valid && i < prefix; ++i) {
				const auto clip = decode(*bank, bank->data.loops[i]);
				if (!clip) valid = false;
				else bank->enhancedParts.push_back(clip);
			}
			if (valid) {
				bank->enhancedRepeat = repeat < 0 ? -1 : int32(bank->enhancedParts.size()) + repeat;
				for (const auto &clip : replacement) bank->enhancedParts.push_back(clip);
			} else bank->enhancedParts.clear();
		}
	}
	if (!bank->enhancedParts.empty()) return queueTheme(name.empty() ? bank->key : name, bank->key, bank->enhancedParts, bank->enhancedRepeat, byte(bank->themeVolume), bank->id);
	Common::Array<Common::SharedPtr<AudioClip> > clips;
	for (uint32 container : bank->data.loops) {
		const auto clip = decode(*bank, container); if (!clip) return false;
		clips.push_back(clip);
	}
	return queueTheme(name.empty() ? bank->key : name, bank->key, clips, bank->data.loopRepeat, byte(bank->themeVolume), bank->id);
}
bool SoundSystem::queueTheme(const Common::String &name, const Common::String &bank, const Common::Array<Common::SharedPtr<AudioClip> > &clips, int32 repeat, byte gain, uint32 bankID) {
	if (!_mixer || clips.empty() || repeat < -1 || repeat >= int32(clips.size())) return false;
	for (const auto &clip : clips) if (!clip || clip->samples.empty() || clip->rate < 1000 || clip->rate > 192000) return false;
	if (!_themeQueue) _themeQueue.reset(new ThemeQueue());
	{
		Common::StackLock lock(_themeQueue->mutex);
		const uint32 base = _themeQueue->parts.size();
		if (repeat >= 0) _themeQueue->repeat = int32(base) + repeat;
		for (const auto &clip : clips) { ThemePart part; part.clip = clip; part.bank = bank; part.bankID = bankID; _themeQueue->parts.push_back(part); }
		// Retain the current tail and repeat root. Consumed history is freed
		// on the main thread, never by an AudioStream destructor.
		_themeQueue->prune();
	}
	_theme.name = name; _theme.bank = bank; _theme.bankID = bankID; _theme.clip.reset(); _themeVolume = gain;
	if (!active(_theme)) startTheme();
	else _mixer->setChannelVolume(_theme.handle, themeGain());
	return true;
}
bool SoundSystem::playMovieTheme(const Common::String &name, const Common::Array<Common::SharedPtr<AudioClip> > &clips, uint32 repeat) {
	return queueTheme(name, "", clips, int32(repeat), 255);
}
void SoundSystem::startTheme() {
	if (_mixer && _themeQueue) _mixer->playStream(Audio::Mixer::kMusicSoundType, &_theme.handle, makeThemeStream(_themeQueue.get(), true), -1, themeGain());
}
bool SoundSystem::themeSnapshot(ThemeQueue &out) const {
	if (!_themeQueue || !active(_theme)) return false;
	Common::StackLock lock(_themeQueue->mutex);
	if (!_themeQueue->ready()) return false;
	_themeQueue->prune();
	out.parts = _themeQueue->parts; out.current = _themeQueue->current; out.phase = _themeQueue->phase; out.repeat = _themeQueue->repeat;
	return true;
}
bool SoundSystem::themeActive() const {
	if (!_themeQueue || !active(_theme)) return false;
	Common::StackLock lock(_themeQueue->mutex); return _themeQueue->ready();
}
void SoundSystem::haltSounds() {
	for (uint32 i = 0; i < _plays.size(); ++i) if (!_plays[i].voice) stop(_plays[i]);
	_slots[0] = _slots[1] = Play(); prune();
}
void SoundSystem::haltVoice() { for (uint32 i = 0; i < _plays.size(); ++i) if (_plays[i].voice) stop(_plays[i]); _voice = Play(); prune(); }
bool SoundSystem::playVoiceClip(const Common::String &name, const Common::SharedPtr<AudioClip> &clip, bool loop, Audio::SoundHandle *handle) {
	haltVoice();
	if (!_mixer || !clip || !clip->rate || clip->samples.empty()) return false;
	Play play; play.name = name; play.name.toLowercase(); play.voice = true;
	play.clip = clip; play.loop = loop;
	_mixer->playStream(Audio::Mixer::kSpeechSoundType, &play.handle, stream(play), -1, volume(play), 0);
	_plays.push_back(play); _voice = play; if (handle) *handle = play.handle; return true;
}
void SoundSystem::haltTheme() { stop(_theme); _themeQueue.reset(); }
bool SoundSystem::done(bool voice) const {
	for (uint32 i = 0; i < _plays.size(); ++i) if (_plays[i].voice == voice && active(_plays[i])) return false;
	return true;
}
Common::String SoundSystem::currentSound(uint32 channel) const { return channel >= 1 && channel <= 2 && active(_slots[channel - 1]) ? _slots[channel - 1].name : "none"; }
Common::String SoundSystem::currentVoice() const { return active(_voice) ? _voice.name : "none"; }
Common::String SoundSystem::currentTheme() const { return themeActive() ? _theme.name : "none"; }
void SoundSystem::soundLoop(Common::String name, bool loop) {
	Bank *bank = soundBank(name); if (!bank) return;
	bank->loops[name] = loop;
	auto update = [&](Play &play) {
		if (play.bankID != bank->id || play.name != name || !play.control) return;
		{ Common::StackLock lock(play.control->mutex); play.control->loop = loop; }
		play.loop = loop; play.wrapped = play.wrapped || loop;
	};
	for (auto &play : _plays) update(play);
	for (auto &slot : _slots) update(slot);
	update(_voice);
	prune();
}
bool SoundSystem::soundLoop(Common::String name) const { Bank *bank = soundBank(name); return bank && bank->loops.contains(name) && bank->loops[name]; }
int32 SoundSystem::soundVolume(Common::String name) const { Bank *bank = soundBank(name); return bank && bank->volumes.contains(name) ? bank->volumes[name] : 255; }
int32 SoundSystem::soundPan(Common::String name) const { Bank *bank = soundBank(name); return bank && bank->pans.contains(name) ? bank->pans[name] : 128; }
void SoundSystem::setSoundVolume(Common::String name, int32 value) {
	Bank *bank = soundBank(name); if (!bank) return;
	bank->volumes[name] = value;
	auto update = [&](Play &play) { if (!play.positional && play.bankID == bank->id && play.name == name) play.positionalVolume = byte(CLIP<int32>(value, 0, 255)); };
	for (auto &play : _plays) { update(play); if (_mixer && active(play)) _mixer->setChannelVolume(play.handle, volume(play)); }
	for (auto &slot : _slots) update(slot);
	update(_voice);
}
void SoundSystem::setSoundPan(Common::String name, int32 value) {
	Bank *bank = soundBank(name); if (!bank) return;
	bank->pans[name] = value;
	// Dust 403010 only records pan. Its PCM setup copies 0x27 descriptor
	// bytes (41ce90), excluding pan at +31, and its gains remain centered.
	if (!_titanicRetail && !_titanicDemo) return;
	auto update = [&](Play &play) {
		if (!play.positional && play.bankID == bank->id && play.name == name) {
			play.balance = int8(CLIP<int32>(value - 128, -127, 127));
			if (_mixer && active(play)) _mixer->setChannelBalance(play.handle, play.balance);
		}
	};
	for (auto &play : _plays) update(play);
	for (auto &slot : _slots) update(slot);
	update(_voice);
}
void SoundSystem::setWaveVolume(int32 value) {
	_waveVolume = CLIP<int32>(value, 0, 9);
	if (_mixer) for (uint32 i = 0; i < _plays.size(); ++i) if (active(_plays[i])) _mixer->setChannelVolume(_plays[i].handle, volume(_plays[i]));
	// The originals call waveOutSetVolume for the mixed device, not only the
	// SFX/voice channels. Apply that master locally to ScummVM music as well.
	if (active(_theme)) _mixer->setChannelVolume(_theme.handle, themeGain());
}
void SoundSystem::setThemeVolume(const Common::String &name, int32 value) {
	Bank *bank = find(name); if (!bank || !bank->data.themeRecords) return;
	bank->themeVolume = CLIP<int32>(value, 0, 255);
	if (active(_theme) && _theme.bankID == bank->id) {
		_themeVolume = bank->themeVolume; _mixer->setChannelVolume(_theme.handle, themeGain());
	}
}
int32 SoundSystem::themeVolume(const Common::String &name) const { Bank *bank = find(name); return bank && bank->data.themeRecords ? bank->themeVolume : 0; }
Common::Array<Common::String> SoundSystem::soundNames(const Common::String &name) const { Bank *bank = find(name); return bank ? bank->data.soundNames : Common::Array<Common::String>(); }
Common::Array<Common::String> SoundSystem::trackNames() const {
	Common::Array<Common::String> names;
	for (const auto &bank : _banks) names.push_back(bank->data.name);
	return names;
}
}
