/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/session.h"
#include "common/algorithm.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/serializer.h"
#include "common/system.h"
#include <math.h>

namespace DreamFactory {
// A ScummVM-owned format. Originals' TI/DF save files are a separate format.
// Every variable-length field is bounded before allocation. Loading happens in
// a detached session; neither bad bytes nor missing assets touch the live game.
class StateIO {
public:
	StateIO(Common::SeekableReadStream *in, Common::WriteStream *out) : s(in, out), loading(in != nullptr), input(in) {}
	Common::Serializer s;
	bool loading, good = true;
	Common::SeekableReadStream *input;
	bool valid() const { return !s.err() && (!input || !input->eos()); }
	uint32 slots[3] = {}, movieEvent = 0;
	Common::Array<uint32> cricketPlays;
	uint32 formatVersion = 8;
	void u32(uint32 &v) { if (good) s.syncAsUint32LE(v); good = good && valid(); }
	void i32(int32 &v) { if (good) s.syncAsSint32LE(v); good = good && valid(); }
	void i16(int16 &v) { if (good) s.syncAsSint16LE(v); good = good && valid(); }
	void u64(uint64 &v) { uint32 lo = uint32(v), hi = uint32(v >> 32); u32(lo); u32(hi); if (loading) v = uint64(lo) | (uint64(hi) << 32); }
	void real(double &v) { if (good) s.syncAsDoubleLE(v); good = good && valid() && isfinite(v); }
	void flag(bool &v) { byte b = v ? 1 : 0; if (good) s.syncAsByte(b); good = good && valid() && b <= 1; if (loading) v = b != 0; }
	uint32 count(uint32 n, uint32 maximum) { u32(n); if (n > maximum) good = false; return good ? n : 0; }
	void str(Common::String &v) {
		const uint32 n = count(v.size(), 4096);
		if (loading) { v.clear(); for (uint32 i = 0; good && i < n; ++i) { byte b = 0; s.syncAsByte(b); good = valid() && b != 0; if (good) v += char(b); } }
		else if (good && n) { s.syncBytes(reinterpret_cast<byte *>(const_cast<char *>(v.c_str())), n); good = valid(); }
	}
	void bytes(Bytes &v, uint32 maximum = 4096) { uint32 n = count(v.size(), maximum); if (loading) v.resize(n); if (good && n) s.syncBytes(v.data(), n); good = good && valid(); }
	void value(Value &v) { flag(v.isString); if (v.isString) str(v.string); else i32(v.integer); }
	template<class T, class F> void array(Common::Array<T> &v, uint32 maximum, F sync) {
		uint32 n = count(v.size(), maximum); if (loading) v.resize(n);
		for (uint32 i = 0; good && i < n; ++i) sync(v[i]);
	}
	template<class T, class F> void map(Common::HashMap<Common::String, T> &v, F sync) {
		Common::Array<Common::String> keys;
		if (!loading) { for (auto it = v.begin(); it != v.end(); ++it) keys.push_back(it->_key); Common::sort(keys.begin(), keys.end()); }
		uint32 n = count(keys.size(), 16384); if (loading) v.clear();
		for (uint32 i = 0; good && i < n; ++i) {
			Common::String key = loading ? Common::String() : keys[i]; str(key);
			if (loading && v.contains(key)) { good = false; break; }
			sync(v[key]);
		}
	}
};

class SaveState {
public:
	static bool sync(Session &g, StateIO &io);
	static void commit(Session &g, Session &prepared, const StateIO &state);
	static bool stereoTheme(SoundSystem &sound) {
		if (!sound._themeQueue) return false;
		Common::StackLock lock(sound._themeQueue->mutex);
		for (const auto &part : sound._themeQueue->parts) if (part.clip && part.clip->channels == 2) return true;
		return false;
	}
private:
	static void clip(StateIO &io, Common::SharedPtr<AudioClip> &clip) {
		bool has = bool(clip); io.flag(has); if (!has) { if (io.loading) clip.reset(); return; }
		if (io.loading) clip.reset(new AudioClip());
		io.u32(clip->rate);
		if (io.formatVersion >= 9) io.u32(clip->channels);
		io.array(clip->samples, 32 * 1024 * 1024, [&](int16 &v) { io.i16(v); });
		if (clip->rate < 1000 || clip->rate > 192000 || clip->samples.empty() || (clip->channels != 1 && clip->channels != 2) || clip->samples.size() % clip->channels) io.good = false;
	}
	static uint32 sample(Audio::Mixer *mixer, const Audio::SoundHandle &handle, const Common::SharedPtr<AudioClip> &clip, uint32 start, bool loop) {
		if (!mixer || !clip || clip->samples.empty()) return start;
		uint64 n = start + uint64(mixer->getSoundElapsedTime(handle)) * clip->rate / 1000;
		return loop ? uint32(n % clip->samples.size()) : uint32(MIN<uint64>(n, clip->samples.size()));
	}
	static void play(StateIO &io, SoundSystem &sound, SoundSystem::Play &p) {
		io.str(p.name); io.str(p.bank); if (io.formatVersion >= 7) io.u32(p.bankID);
		io.flag(p.voice); io.flag(p.loop); io.flag(p.positional);
		io.u32(p.soundID); io.s.syncAsByte(p.positionalVolume); io.s.syncAsSByte(p.balance);
		uint32 offset = io.loading ? 0 : sample(sound._mixer, p.handle, p.clip, p.startSample, p.loop || p.wrapped);
		io.u32(offset); if (io.loading) p.startSample = offset;
		clip(io, p.clip);
		if (!p.clip || offset > p.clip->samples.size()) io.good = false;
	}
	static void bankReference(StateIO &io, SoundSystem &sound, const Common::String &name, uint32 &id) {
		if (io.loading && io.formatVersion < 7) {
			id = 0; for (const auto &bank : sound._banks) if (bank->key.equalsIgnoreCase(name)) { id = bank->id; break; }
		}
		const auto bank = id ? sound.findBankID(id) : nullptr;
		if (name.empty() ? id != 0 : (!bank || !bank->key.equalsIgnoreCase(name))) io.good = false;
	}
	template<class T> static void soundControls(StateIO &io, const AudioBank &data, Common::HashMap<Common::String, T> &controls) {
		Common::Array<Common::String> retired;
		for (const auto &v : controls) if (!data.singles.contains(v._key)) {
			if (io.loading && io.formatVersion < 8 && data.version == 1 && data.legacyMusic.contains(v._key)) retired.push_back(v._key);
			else io.good = false;
		}
		for (const auto &key : retired) controls.erase(key);
	}
	static void theme(StateIO &io, SoundSystem &sound) {
		ThemeQueue snapshot;
		bool has = !io.loading && sound.themeSnapshot(snapshot); io.flag(has);
		if (!has) return;
		io.str(sound._theme.name); io.str(sound._theme.bank);
		if (io.formatVersion >= 7) io.u32(sound._theme.bankID);
		bankReference(io, sound, sound._theme.bank, sound._theme.bankID);
		Common::Array<Common::SharedPtr<AudioClip> > pool;
		Common::Array<uint32> indices;
		if (!io.loading) for (const auto &part : snapshot.parts) {
			uint32 index = 0; while (index < pool.size() && pool[index] != part.clip) ++index;
			if (index == pool.size()) pool.push_back(part.clip);
			indices.push_back(index);
		}
		io.array(pool, 1024, [&](Common::SharedPtr<AudioClip> &pcm) { clip(io, pcm); if (!pcm) io.good = false; });
		const uint32 n = io.count(snapshot.parts.size(), 16384);
		if (io.loading) snapshot.parts.resize(n);
		for (uint32 i = 0; io.good && i < n; ++i) {
			uint32 index = io.loading ? 0 : indices[i]; io.u32(index); io.str(snapshot.parts[i].bank);
			if (io.formatVersion >= 7) io.u32(snapshot.parts[i].bankID);
			if (index >= pool.size()) { io.good = false; break; }
			if (io.loading) snapshot.parts[i].clip = pool[index];
			bankReference(io, sound, snapshot.parts[i].bank, snapshot.parts[i].bankID);
		}
		io.u32(snapshot.current); io.u64(snapshot.phase); io.i32(snapshot.repeat);
		if (sound._theme.name.empty() || snapshot.current >= n || snapshot.repeat < -1 || snapshot.repeat >= int32(n)) io.good = false;
		if (io.good && snapshot.phase >= uint64(snapshot.parts[snapshot.current].clip->frames()) * 44100) io.good = false;
		if (io.loading && io.good) {
			sound._themeQueue.reset(new ThemeQueue());
			sound._themeQueue->parts = snapshot.parts; sound._themeQueue->current = snapshot.current;
			sound._themeQueue->phase = snapshot.phase; sound._themeQueue->repeat = snapshot.repeat;
		}
	}
	static void star(StateIO &io, Star &v) { io.str(v.name); io.i32(v.x); io.i32(v.y); io.i32(v.z); io.i32(v.facing); }
	static void path(StateIO &io, StarPath &p) {
		io.str(p.a); io.str(p.b); io.u32(p.container);
		io.array(p.points, 65536, [&](StarPoint &v) { io.i32(v.x); io.i32(v.y); io.i32(v.z); io.i32(v.distance); });
	}
};

bool SaveState::sync(Session &g, StateIO &io) {
	Common::String signature = g._bootSignature; io.str(signature);
	uint32 version = g.version; io.u32(version);
	if (signature != g._bootSignature || version != g.version) return false;
	for (uint32 i = 1; i < 9; ++i) { Common::String path = g.resources.path(i); io.str(path); if (io.loading && !g.resources.setPath(i, path)) io.good = false; }
	Common::String cd = g.resources.currentCD(); io.str(cd); if (io.loading && !cd.empty() && !g.resources.mount(cd)) io.good = false;
	io.map(g.runtime.globals, [&](Value &v) { io.value(v); });
	io.u64(g._time); io.u64(g.runtime._eventSequence);
	uint32 seed = g._random.getSeed(); io.u32(seed); if (io.loading) g._random.setSeed(seed);
	io.i32(g.frameRate); io.i32(g.cameraHi); io.flag(g.menuVisible); io.flag(g.keyAborts); io.flag(g.puppetGrab);
	io.value(g.lastResult); io.i16(g.playerHitbox.x); io.i16(g.playerHitbox.y);
	uint32 params = io.count(g.puppetParams.size(), 256);
	if (io.loading) g.puppetParams.clear();
	Common::Array<int32> paramKeys; if (!io.loading) { for (auto it = g.puppetParams.begin(); it != g.puppetParams.end(); ++it) paramKeys.push_back(it->_key); Common::sort(paramKeys.begin(), paramKeys.end()); }
	for (uint32 i = 0; io.good && i < params; ++i) { int32 key = io.loading ? 0 : paramKeys[i]; io.i32(key); int32 value = io.loading ? 0 : g.puppetParams[key]; io.i32(value); if (io.loading) { if (g.puppetParams.contains(key)) io.good = false; g.puppetParams[key] = value; } }
	io.str(g.cursors.name); io.i32(g.cursors.depth);
	io.str(g._paletteKind); io.bytes(g._roomPalette); io.bytes(g._stagePalette); io.bytes(g._puppetPalette); io.bytes(g._puppetArtPalette);
	for (const Bytes *palette : { &g._roomPalette, &g._stagePalette, &g._puppetPalette, &g._puppetArtPalette }) if (!palette->empty() && palette->size() != 768) io.good = false;
	io.s.syncBytes(g.displayPalette, 768);
	io.array(g.textOverlay, 4096, [&](Session::TextEntry &t) { io.str(t.text); io.i32(t.x); io.i32(t.y); io.i32(t.color); io.i32(t.size); });

	// Construct immutable asset/script owners first. The saved arrays below
	// retain removed originals, runtime copies, overrides and insertion order.
	uint32 n = io.count(g.castSystem.casts.size(), 256);
	for (uint32 i = 0; io.good && i < n; ++i) {
		Common::String name = io.loading ? Common::String() : g.castSystem.casts[i]->name;
		Common::String source = io.loading ? Common::String() : g.castSystem.casts[i]->source;
		io.str(name); io.str(source);
		if (io.loading && (source.empty() || !g.castSystem.open(name, nullptr, false, source) || g.castSystem.casts.size() != i + 1)) io.good = false;
	}
	if (io.loading) g.castSystem.actors.clear();
	io.array(g.castSystem.actors, 4096, [&](Common::SharedPtr<Actor> &ptr) {
		if (io.loading) ptr.reset(new Actor()); Actor &a = *ptr;
		uint32 index = 0;
		if (!io.loading) { while (index < g.castSystem.casts.size() && g.castSystem.casts[index] != a.cast) ++index; }
		io.u32(index); io.u32(a.member); io.str(a.name); io.flag(a.copy);
		if (index >= g.castSystem.casts.size() || a.member >= g.castSystem.casts[index]->scripts.size()) { io.good = false; return; }
		if (io.loading) { a.cast = g.castSystem.casts[index]; a.script = a.cast->scripts[a.member]; if (a.copy) { a.script.reset(new ScriptInstance(*a.script)); a.script->name = a.name; } }
		io.str(a.set); io.str(a.star); io.str(a.pose); io.u32(a.step); io.flag(a.visible); io.flag(a.worldSpace); io.flag(a.starPending);
		io.i32(a.x); io.i32(a.y); io.i32(a.z); io.i32(a.anchorX); io.i32(a.anchorY); io.i32(a.deg); io.i32(a.scale);
		io.i32(a.speed); io.i32(a.turn); io.i32(a.zclip); io.i32(a.dist); io.i16(a.hitWidth); io.i16(a.hitHeight); io.value(a.owner); io.value(a.value);
	});
	n = io.count(g.propSystem.shops.size(), 256);
	for (uint32 i = 0; io.good && i < n; ++i) {
		Common::String name = io.loading ? Common::String() : g.propSystem.shops[i]->name;
		Common::String source = io.loading ? Common::String() : g.propSystem.shops[i]->source;
		io.str(name); io.str(source);
		if (io.loading && (source.empty() || !g.propSystem.open(name, nullptr, false, source) || g.propSystem.shops.size() != i + 1)) { io.good = false; break; }
		io.flag(g.propSystem.shops[i]->persistent);
	}
	if (io.loading) g.propSystem.props.clear();
	io.array(g.propSystem.props, 4096, [&](Common::SharedPtr<Prop> &ptr) {
		if (io.loading) ptr.reset(new Prop()); Prop &p = *ptr;
		uint32 index = 0; if (!io.loading) { while (index < g.propSystem.shops.size() && g.propSystem.shops[index] != p.shop) ++index; }
		io.u32(index); io.u32(p.group); io.str(p.name); io.flag(p.copy);
		if (index >= g.propSystem.shops.size() || p.group >= g.propSystem.shops[index]->scripts.size()) { io.good = false; return; }
		if (io.loading) { p.shop = g.propSystem.shops[index]; p.script = p.shop->scripts[p.group]; if (p.copy) { p.script.reset(new ScriptInstance(*p.script)); p.script->name = p.name; } }
		io.str(p.set); io.str(p.view); io.str(p.star); io.u32(p.step);
		io.array(p.order, 65536, [&](uint32 &v) { io.u32(v); });
		io.flag(p.directional); io.flag(p.locked); io.flag(p.variants); io.flag(p.animating); io.u64(p.degreeEvent); io.u64(p.lastTick);
		io.flag(p.visible); io.flag(p.hidden); io.flag(p.worldSpace); io.flag(p.screenPlaced); io.flag(p.starPending);
		io.i32(p.x); io.i32(p.y); io.i32(p.z); io.i32(p.anchorX); io.i32(p.anchorY); io.i32(p.deg); io.i32(p.scale); io.i32(p.speed); io.i32(p.dist); io.i32(p.zclip); io.value(p.owner); io.value(p.value);
		const PropState *state = p.state();
		if (state) for (uint32 v : p.order) if (v >= state->pictures.size()) io.good = false;
	});

	Common::String stage = g.stageSystem.current ? g.stageSystem.current->name : Common::String();
	Common::String stageSource = g.stageSystem.current ? g.stageSystem.current->source : Common::String();
	Common::String flat = g.stageSystem.currentFlat(); io.str(stage); io.str(stageSource); io.str(flat);
	if (io.loading && !stage.empty()) {
		if (stageSource.empty() || !g.stageSystem.open(stage, nullptr, false, stageSource)) io.good = false;
		g.stageSystem.flat = g.stageSystem.resolve(flat); if (g.stageSystem.flat < 0) io.good = false;
	}
	Common::String room = g.roomSystem.current ? g.roomSystem.current->file : Common::String();
	Common::String roomSource = g.roomSystem.current ? g.roomSystem.current->source : Common::String();
	Common::String scene = g.roomSystem.sceneName(), view = g.roomSystem.viewName();
	io.str(room); io.str(roomSource); io.str(scene); io.str(view); bool visible = g.roomSystem.visible; io.flag(visible);
	if (io.loading && !room.empty()) {
		if (roomSource.empty() || !g.roomSystem.open(room, scene, view, nullptr, false, roomSource) || g.roomSystem.sceneName() != scene || g.roomSystem.viewName() != view) io.good = false;
	}
	if (io.loading) { g.roomSystem.visible = visible; if (g.roomSystem.current) g.stars.current = g.roomSystem.current->data.stars; }
	io.i32(g.roomSystem._lastFacing); io.flag(g.roomSystem._hasFacing);
	io.map(g.stars.known, [&](Star &v) { star(io, v); });
	io.array(g.stars.knownPaths, 65536, [&](StarPath &v) { path(io, v); });

	io.u64(g.scheduler._time); io.u64(g.scheduler._service); io.u64(g.scheduler._display); io.u32(g.scheduler.frame);
	io.array(g.scheduler.loops, 32, [&](Common::SharedPtr<GameLoop> &ptr) {
		if (io.loading) ptr.reset(new GameLoop()); GameLoop &l = *ptr;
		io.str(l.kind); io.str(l.name); io.str(l.handler); io.i32(l.count); io.i32(l.period); io.flag(l.paused);
		if (l.period < 1 || l.count < 0) io.good = false;
	});
	io.array(g.motion.walks, 4096, [&](Common::SharedPtr<ActorWalk> &ptr) {
		if (io.loading) ptr.reset(new ActorWalk()); ActorWalk &w = *ptr;
		Common::String name = w.actor ? w.actor->name : Common::String(); io.str(name);
		if (io.loading) w.actor = g.castSystem.get(name); if (!w.actor) io.good = false;
		io.real(w.sx); io.real(w.sy); io.real(w.sz); io.real(w.dx); io.real(w.dy); io.real(w.dz); io.real(w.distance); io.real(w.progress);
		io.i32(w.facing); io.flag(w.turning); io.flag(w.turnOnly); io.flag(w.paused); io.str(w.destination);
		io.array(w.path, 65536, [&](MotionPoint &v) { io.real(v.x); io.real(v.y); io.real(v.z); io.real(v.cumulative); });
		if (w.distance <= 0 || w.progress < 0) io.good = false;
	});
	io.str(g.crickets.room); io.i32(g.crickets._x); io.i32(g.crickets._y); io.i32(g.crickets._facing); io.flag(g.crickets._listener);
	seed = g.crickets._random.getSeed(); io.u32(seed); if (io.loading) g.crickets._random.setSeed(seed);
	// Audio handle references are indices in the following active-play table.
	Common::Array<SoundSystem::Play> plays;
	if (!io.loading) for (const auto &p : g.sound._plays) if (g.sound.active(p)) plays.push_back(p);
	io.array(plays, 256, [&](SoundSystem::Play &p) { play(io, g.sound, p); });
	auto reference = [&](Audio::SoundHandle &handle) {
		uint32 index = plays.size();
		if (!io.loading) for (uint32 i = 0; i < plays.size(); ++i) if (plays[i].handle == handle) { index = i; break; }
		io.u32(index); if (index > plays.size()) io.good = false;
		if (io.loading && index < plays.size()) handle = plays[index].handle;
		return index;
	};
	// Detached handles are assigned distinct dummy values by indexing the
	// saved play records separately; commit reconstructs their mixer handles.
	uint32 slot[3]; for (uint32 i = 0; i < 3; ++i) {
		SoundSystem::Play &p = i < 2 ? g.sound._slots[i] : g.sound._voice;
		slot[i] = reference(p.handle);
		if (io.loading) p = slot[i] < plays.size() ? plays[slot[i]] : SoundSystem::Play();
	}
	io.array(g.crickets.crickets, 16, [&](Common::SharedPtr<Cricket> &ptr) {
		if (io.loading) ptr.reset(new Cricket()); Cricket &c = *ptr;
		io.str(c.name); io.str(c.set); io.i32(c.x); io.i32(c.y); io.i32(c.radius); io.i32(c.base); io.i32(c.jitter);
		uint64 count = uint64(c.count); io.u64(count); if (io.loading) c.count = int64(count); io.flag(c.paused);
		if (io.formatVersion >= 3) io.flag(c.armed);
		else if (io.loading) c.armed = true; // Older saves stored an already-started countdown.
		uint32 index = reference(c.handle); if (io.loading) { c.playing = index < plays.size(); if (c.playing) c.handle = plays[index].handle; }
		if (c.radius < 1) io.good = false;
		if (io.loading) io.cricketPlays.push_back(index);
	});
	if (io.loading) { g.sound._plays = plays; for (uint32 i = 0; i < 3; ++i) io.slots[i] = slot[i]; }
	io.map(g.sound._loops, [&](bool &v) { io.flag(v); });
	io.map(g.sound._volumes, [&](int32 &v) { io.i32(v); }); io.map(g.sound._pans, [&](int32 &v) { io.i32(v); }); io.map(g.sound._themeVolumes, [&](int32 &v) { io.i32(v); });
	io.i32(g.sound._waveVolume); io.i32(g.sound._themeVolume);
	if (g.sound._waveVolume < 0 || g.sound._waveVolume > 9 || g.sound._themeVolume < 0 || g.sound._themeVolume > 255) io.good = false;
	// The legacy policy field remains readable, but edition rules belong to
	// the running engine/data. Older saves can predate the correct policy.
	bool savedRetail = g.sound._titanicRetail; io.flag(savedRetail);
	n = io.count(g.sound._banks.size(), 256);
	for (uint32 i = 0; io.good && i < n; ++i) {
		Common::String name = io.loading ? Common::String() : g.sound._banks[i]->key;
		Common::String source = io.loading ? Common::String() : g.sound._banks[i]->source;
		io.str(name); io.str(source); if (io.loading && (source.empty() || !g.sound.open(name, source) || g.sound._banks.size() != i + 1)) io.good = false;
	}
	// Read after all opens: another early-engine bank renumbers descriptors.
	bool legacyDustPriorities = io.loading && g.version == 1 && io.formatVersion < 5;
	if (io.formatVersion >= 4) for (uint32 i = 0; io.good && i < n; ++i) {
		uint32 base = g.sound._banks[i]->priorityBase; io.u32(base);
		if ((base & 0xffff) || base > 0xff0000 || (g.sound._titanicRetail && base)) io.good = false;
		if (base) legacyDustPriorities = false;
		if (io.loading) g.sound._banks[i]->priorityBase = base;
	}
	if (io.formatVersion >= 5) for (uint32 i = 0; io.good && i < n; ++i) {
		SoundSystem::Bank &bank = *g.sound._banks[i];
		io.map(bank.loops, [&](bool &v) { io.flag(v); });
		io.map(bank.volumes, [&](int32 &v) { io.i32(v); });
		io.map(bank.pans, [&](int32 &v) { io.i32(v); });
		soundControls(io, bank.data, bank.loops); soundControls(io, bank.data, bank.volumes); soundControls(io, bank.data, bank.pans);
	}
	if (io.formatVersion >= 7) for (uint32 i = 0; io.good && i < n; ++i) {
		SoundSystem::Bank &bank = *g.sound._banks[i];
		io.u32(bank.id); io.i32(bank.themeVolume);
		if (!bank.id || bank.themeVolume < 0 || bank.themeVolume > 255) io.good = false;
		for (uint32 j = 0; j < i; ++j) if (g.sound._banks[j]->id == bank.id) io.good = false;
	}
	if (!io.good) return false;
	if (io.loading && io.formatVersion < 7) for (const auto &v : g.sound._themeVolumes) {
		SoundSystem::Bank *bank = nullptr;
		for (const auto &candidate : g.sound._banks) if (candidate->key.equalsIgnoreCase(v._key)) { bank = candidate.get(); break; }
		if (!bank) bank = g.sound.find(v._key);
		if (bank && bank->data.themeRecords) bank->themeVolume = CLIP<int32>(v._value, 0, 255);
	}
	if (io.loading) g.sound._themeVolumes.clear();
	for (auto &p : g.sound._plays) bankReference(io, g.sound, p.bank, p.bankID);
	if (!io.good) return false;
	if (io.loading && io.formatVersion < 8 && g.version == 1) for (auto &p : g.sound._plays) {
		const auto bank = g.sound.findBankID(p.bankID);
		if (!bank || !bank->data.legacyMusic.contains(p.name)) continue;
		// Old directory lookups could put music on an effect/voice channel,
		// including a looping high-priority slot that no native effect replaces.
		if (!bank->data.singles.contains(p.name) || ((p.soundID & 0xffff) && (p.soundID & 0xffff) != bank->data.soundIDs[p.name])) p = SoundSystem::Play();
	}
	if (io.loading && io.formatVersion < 5) {
		for (const auto &v : g.sound._loops) { Common::String key = v._key; auto bank = g.sound.soundBank(key); if (bank) bank->loops[key] = v._value; }
		for (const auto &v : g.sound._volumes) { Common::String key = v._key; auto bank = g.sound.soundBank(key); if (bank) bank->volumes[key] = v._value; }
		for (const auto &v : g.sound._pans) { Common::String key = v._key; auto bank = g.sound.soundBank(key); if (bank) bank->pans[key] = v._value; }
		for (auto &p : g.sound._plays) if (!p.positional && !p.voice) {
			p.positionalVolume = byte(CLIP<int32>(g.sound.soundVolume(p.name), 0, 255));
			p.balance = int8(CLIP<int32>(g.sound.soundPan(p.name) - 128, -127, 127));
		}
	}
	if (io.loading) { g.sound._loops.clear(); g.sound._volumes.clear(); g.sound._pans.clear(); }
	if (legacyDustPriorities) for (const auto &p : g.sound._plays)
		if (!p.voice && (p.soundID & 0xffff0000)) legacyDustPriorities = false;
	// Initial v4 Dust saves recorded zero for every bank. Two real early-engine
	// banks cannot share base zero, so this unambiguously identifies the old
	// approximation. A lone zero-base bank already has correct local IDs.
	if (legacyDustPriorities) for (uint32 i = 0; i < n; ++i) g.sound._banks[i]->priorityBase = i << 16;
	if (io.loading && g.sound.hasCompositeSoundIDs() && (io.formatVersion < 4 || legacyDustPriorities)) {
		for (auto &p : g.sound._plays) {
			SoundSystem::Bank *bank = g.sound.find(p.bank);
			if (bank && !p.voice) p.soundID = (p.soundID & 0xffff) | bank->priorityBase;
		}
		for (uint32 i = 0; i < 2; ++i) if (slot[i] < g.sound._plays.size()) g.sound._slots[i] = g.sound._plays[slot[i]];
	}
	// Experimental Dust saves could hold unbounded voices outside their two
	// recorded slots. Keep the physical slots and speech when upgrading them.
	if (legacyDustPriorities) for (uint32 i = 0; i < g.sound._plays.size(); ++i)
		if (!g.sound._plays[i].voice && !g.sound._plays[i].bank.empty() && i != slot[0] && i != slot[1]) g.sound._plays[i] = SoundSystem::Play();
	if (io.loading) for (uint32 i = 0; i < 3; ++i) {
		SoundSystem::Play &p = i < 2 ? g.sound._slots[i] : g.sound._voice;
		p = slot[i] < g.sound._plays.size() ? g.sound._plays[slot[i]] : SoundSystem::Play();
	}
	if (io.formatVersion >= 6) theme(io, g.sound);
	else { bool hasTheme = g.sound.active(g.sound._theme); io.flag(hasTheme); if (hasTheme) { play(io, g.sound, g.sound._theme); bankReference(io, g.sound, g.sound._theme.bank, g.sound._theme.bankID); } }
	bool bed = false; io.flag(bed);
	if (io.formatVersion >= 6 && bed) io.good = false;
	if (bed) {
		clip(io, g.movies._bedClip); io.flag(g.movies._bedLoop);
		uint32 offset = 0;
		io.u32(offset); if (io.loading) g.movies._bedStart = offset;
		if (!g.movies._bedClip || offset > g.movies._bedClip->samples.size()) io.good = false;
	}
	uint32 event = reference(g.movies._event); if (io.loading) io.movieEvent = event;
	if (io.formatVersion >= 2) {
		for (auto &ballSlot : g.balls.balls) {
			bool active = bool(ballSlot); io.flag(active);
			if (!active) { if (io.loading) ballSlot.reset(); continue; }
			if (io.loading) ballSlot.reset(new Ball()); Ball &b = *ballSlot;
			io.str(b.name); io.i16(b.x); io.i16(b.y); io.i16(b.z);
			io.i16(b.vx); io.i16(b.vy); io.i16(b.vz);
			io.i16(b.gravity); io.i16(b.bounce); io.i16(b.frames); io.i16(b.paused);
			if (g.version != 1 || b.name.empty()) io.good = false;
		}
		for (uint32 i = 0; i < g.balls.balls.size(); ++i) if (g.balls.balls[i])
			for (uint32 j = 0; j < i; ++j) if (g.balls.balls[j] && g.balls.balls[j]->name.equalsIgnoreCase(g.balls.balls[i]->name)) io.good = false;
	}
	if (io.formatVersion >= 10) {
		io.array(g.journal.entries, 16384, [&](JournalEntry &entry) {
			io.str(entry.speaker); io.str(entry.room); io.str(entry.file); io.str(entry.ident); io.str(entry.text);
			io.u64(entry.time); io.flag(entry.bookmarked); io.i32(entry.selected);
			io.array(entry.choices, 5, [&](Common::String &choice) { io.str(choice); });
			if (entry.selected < -1 || (entry.selected >= 0 && uint32(entry.selected) >= entry.choices.size())) io.good = false;
		});
	}
	if (io.formatVersion >= 11) {
		if (!io.loading) {
			g._savedRouteCursor = g.routes.graph.cursor(); g._savedRouteChoice = g.routes.graph.choice();
			g._savedRouteToken = g.routes.graph.cursorToken(g._savedRouteCursor);
		}
		io.u32(g._savedRouteCursor); io.i32(g._savedRouteChoice); io.str(g._savedRouteToken);
		if (g._savedRouteCursor > RouteGraph::kMaxNodes || g._savedRouteChoice < -1 || g._savedRouteChoice > 4 ||
			(!g._savedRouteCursor && (g._savedRouteChoice != -1 || !g._savedRouteToken.empty())) ||
			(g._savedRouteCursor && g._savedRouteToken.size() != 32)) io.good = false;
	}
	return io.good && !io.s.err() && g.runtime.error.empty();
}

void SaveState::commit(Session &g, Session &p, const StateIO &state) {
	// Invalidate the load command's old frames before replacing script owners.
	g.runtime.abandon(); g.pendingEvents.clear(); g.puppetChoices.escape(); g.puppetSpeech.abandon(); g.puppetSystem.reset();
	g.movies.reset(); g.crickets.reset(); g.sound.reset(); g.motion.reset(); g.balls.reset();
	g.resources = p.resources; g.bootScripts = p.bootScripts; g.runtime.fallbacks = p.runtime.fallbacks; g.runtime.globals = p.runtime.globals;
	g.runtime._eventSequence = p.runtime._eventSequence; g.runtime.error.clear();
	g.castSystem.casts = p.castSystem.casts; g.castSystem.actors = p.castSystem.actors;
	g.propSystem.shops = p.propSystem.shops; g.propSystem.props = p.propSystem.props;
	g.stageSystem.current = p.stageSystem.current; g.stageSystem.flat = p.stageSystem.flat;
	g.roomSystem.current = p.roomSystem.current; g.roomSystem.scene = p.roomSystem.scene; g.roomSystem.view = p.roomSystem.view;
	g.roomSystem.frame = p.roomSystem.frame; g.roomSystem.camera = p.roomSystem.camera; g.roomSystem.visible = p.roomSystem.visible;
	g.roomSystem.moving = g.roomSystem.turning = false; g.roomSystem._lastFacing = p.roomSystem._lastFacing; g.roomSystem._hasFacing = p.roomSystem._hasFacing; ++g.roomSystem._revision;
	g.stars = p.stars; g.motion.walks = p.motion.walks;
	g.scheduler.loops = p.scheduler.loops; g.scheduler.frame = p.scheduler.frame; g.scheduler._time = p.scheduler._time; g.scheduler._service = p.scheduler._service; g.scheduler._display = p.scheduler._display;
	g._time = p._time; g._lastMillis = g._system ? g._system->getMillis() : 0; g._random.setSeed(p._random.getSeed());
	g.frameRate = p.frameRate; g.cameraHi = p.cameraHi; g.menuVisible = p.menuVisible; g.keyAborts = p.keyAborts; g.puppetGrab = p.puppetGrab;
	g.journal = p.journal;
	g._routePending.clear(); g._routeOwner.reset(); g._routeResumeScene = 0; g._routeRevisiting = false;
	if (p._savedRouteToken != g.routes.graph.cursorToken(p._savedRouteCursor) || !g.routes.graph.restoreCursor(p._savedRouteCursor, p._savedRouteChoice))
		g.routes.graph.restoreCursor(0, -1);
	g.lastResult = p.lastResult; g.playerHitbox = p.playerHitbox; g.puppetParams = p.puppetParams;
	g._paletteKind = p._paletteKind; g._roomPalette = p._roomPalette; g._stagePalette = p._stagePalette; g._puppetPalette = p._puppetPalette; g._puppetArtPalette = p._puppetArtPalette; memcpy(g.displayPalette, p.displayPalette, 768); g.textOverlay = p.textOverlay;
	g._paletteBlack = g.screenBlack = g.pointerDown = g._stopped = g._scriptQuit = false; g._pointerButtons = 0; g._controlFocused = g._controlHeld = g._touchHeld = false;
	g._inventoryRequested = g._inventoryShown = false; g._inventoryOfferItem.clear();
	g._recoveryRequested = false;
	g._fastForward = g._speechKnown = g._speechReplay = false;
	g._captionTop = 224; g._replayButtonTop = 245;
	g._inventoryClicks.clear(); g._inventoryClickDown = g._inventoryReopen = false; g._inventoryClickDue = 0;
	g._stageView.reset(); g._puppetView.reset(); g.cursors.depth = p.cursors.depth; g.cursors.set(p.cursors.name);
	g.sound._banks = p.sound._banks; g.sound._loops = p.sound._loops; g.sound._volumes = p.sound._volumes; g.sound._pans = p.sound._pans; g.sound._themeVolumes = p.sound._themeVolumes;
	g.sound._waveVolume = p.sound._waveVolume; g.sound._themeVolume = p.sound._themeVolume; g.sound._titanicRetail = p.sound._titanicRetail;
	g.sound._titanicDemo = p.sound._titanicDemo;
	g.sound._plays = p.sound._plays;
	Audio::Mixer *mixer = g.sound._mixer;
	if (mixer) for (auto &play : g.sound._plays) {
		if (play.name.empty()) continue;
		if (g.version == 1 && !play.positional) play.balance = 0;
		mixer->playStream(play.voice ? Audio::Mixer::kSpeechSoundType : Audio::Mixer::kSFXSoundType, &play.handle, g.sound.stream(play), -1, g.sound.volume(play), play.balance);
	}
	for (uint32 i = 0; i < 3; ++i) { uint32 index = state.slots[i]; SoundSystem::Play &slot = i < 2 ? g.sound._slots[i] : g.sound._voice; slot = index < g.sound._plays.size() ? g.sound._plays[index] : SoundSystem::Play(); }
	g.balls.balls = p.balls.balls;
	g.crickets.crickets = p.crickets.crickets; g.crickets.room = p.crickets.room; g.crickets._x = p.crickets._x; g.crickets._y = p.crickets._y; g.crickets._facing = p.crickets._facing; g.crickets._listener = p.crickets._listener; g.crickets._random.setSeed(p.crickets._random.getSeed());
	for (uint32 i = 0; i < g.crickets.crickets.size(); ++i) { const uint32 index = state.cricketPlays[i]; if (index < g.sound._plays.size()) g.crickets.crickets[i]->handle = g.sound._plays[index].handle; }
	if (p.sound._themeQueue) {
		g.sound._theme = p.sound._theme; g.sound._themeQueue = p.sound._themeQueue;
	} else if (p.sound._theme.clip && mixer) {
		// v1-v5 stored one joined looping theme and an elapsed sample offset.
		g.sound._theme = p.sound._theme; g.sound._themeQueue.reset(new ThemeQueue());
		ThemePart part; part.clip = p.sound._theme.clip; part.bank = p.sound._theme.bank; part.bankID = p.sound._theme.bankID;
		g.sound._themeQueue->parts.push_back(part); g.sound._themeQueue->repeat = 0;
		g.sound._themeQueue->phase = uint64(p.sound._theme.startSample) * 44100;
		g.sound._theme.clip.reset();
	}
	if (p.movies._bedClip && mixer) {
		if (!g.sound._themeQueue) g.sound._themeQueue.reset(new ThemeQueue());
		ThemePart part; part.clip = p.movies._bedClip;
		if (p.movies._bedLoop) g.sound._themeQueue->repeat = g.sound._themeQueue->parts.size();
		g.sound._themeQueue->parts.push_back(part); g.sound._theme.name = "legacy-movie"; g.sound._theme.bank.clear(); g.sound._theme.bankID = 0; g.sound._themeVolume = 255;
		// The old movie bed is the appended final part; its offset must not
		// replace a still-playing game theme's cursor.
		if (g.sound._themeQueue->parts.size() == 1) g.sound._themeQueue->phase = uint64(p.movies._bedStart) * 44100;
	}
	if (g.sound._themeQueue) g.sound.startTheme();
	if (state.movieEvent < g.sound._plays.size()) g.movies._event = g.sound._plays[state.movieEvent].handle;
	// Prepared ambient records are shared; do not let its destructor stop newly
	// installed handles. Immutable banks/clips remain shared safely.
	p.crickets.crickets.clear(); p.sound._plays.clear(); p.sound._theme = SoundSystem::Play(); p.sound._themeQueue.reset();
	g.presentWorld();
}

bool Session::canAutosave() const {
	// A title/menu/document flat is not a useful recovery point. Require an
	// ordinary navigable world view in addition to the manual-save guards.
	return canSave() && roomSystem.current && roomSystem.visible;
}
bool Session::canSave() const {
	return !_dialogActive && !runtime.busy() && !pointerDown && !_paletteBlack && !screenBlack && !puppetSystem.current && !movies.active() && !roomSystem.moving && !roomSystem.turning && stageSystem.current;
}
bool Session::saveState(Bytes &bytes, Common::String &error) {
	error.clear(); bytes.clear();
	if (puppetSystem.current || puppetSpeech.active() || puppetChoices.active() || movies.active() || roomSystem.moving || roomSystem.turning || !stageSystem.current) { error = "The game is in a modal interaction"; return false; }
	Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES); StateIO io(nullptr, &payload);
	// Empty journals retain v8/v9 compatibility. History-bearing saves use v10.
	// Stereo PCM remains embedded, independent of the enhancement pack.
	if (SaveState::stereoTheme(sound)) io.formatVersion = 9;
	if (!journal.entries.empty()) io.formatVersion = 10;
	if (routes.graph.cursor()) io.formatVersion = 11;
	if (!SaveState::sync(*this, io) || payload.size() > 128 * 1024 * 1024) { error = "Cannot encode game state"; return false; }
	Common::MemoryReadStream content(payload.getData(), uint32(payload.size())); byte digest[16]; Common::computeStreamMD5(content, digest);
	Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES); out.writeUint32BE(MKTAG('D','F','S','V')); out.writeUint32LE(io.formatVersion); out.writeUint32LE(uint32(payload.size())); out.write(digest, 16); out.write(payload.getData(), uint32(payload.size()));
	bytes.resize(uint32(out.size())); memcpy(bytes.data(), out.getData(), uint32(out.size())); return true;
}
bool Session::loadState(const Bytes &bytes, Common::String &error) {
	error.clear();
	if (bytes.size() < 28 || bytes.size() > 128 * 1024 * 1024 + 28) { error = "Invalid save size"; return false; }
	Common::MemoryReadStream in(bytes.data(), bytes.size());
	const uint32 magic = in.readUint32BE(), formatVersion = in.readUint32LE(), payloadSize = in.readUint32LE();
	if (magic != MKTAG('D','F','S','V') || (formatVersion < 1 || formatVersion > 11) || payloadSize != bytes.size() - 28) { error = "Unsupported DreamFactory save"; return false; }
	byte expected[16], digest[16]; in.read(expected, 16); Common::computeStreamMD5(in, digest); in.seek(28);
	if (memcmp(expected, digest, 16)) { error = "Save checksum mismatch"; return false; }
	Session prepared;
	if (!prepared.init(_root)) { error = prepared.runtime.error; return false; }
	prepared.sound.setTitanicRetail(sound.isTitanicRetail());
	prepared.sound.setTitanicDemo(sound.isTitanicDemo());
	StateIO io(&in, nullptr);
	io.formatVersion = formatVersion;
	if (!SaveState::sync(prepared, io) || in.pos() != in.size()) { error = "Save data or required game resources are incompatible"; return false; }
	// Decode the visible picture and sprites before committing. Renderer errors
	// must reject the save while the original game is still intact.
	if (!prepared.stageSystem.current || prepared.stageSystem.flat < 0) { error = "Save has no interface"; return false; }
	const auto stage = prepared.stageSystem.current;
	const Frame *flat = prepared._stageView.frame(stage, prepared.stageSystem.flat);
	const Room *room = prepared.roomSystem.current ? &prepared.roomSystem.current->data : nullptr;
	const Frame *frame = room && prepared.roomSystem.visible ? &prepared.roomSystem.frame : nullptr;
	if (!flat && stage->data.flats[prepared.stageSystem.flat].image) { error = "Invalid saved stage artwork"; return false; }
	byte palette[768]; memcpy(palette, prepared._stagePalette.empty() ? stage->data.palette : prepared._stagePalette.data(), 768);
	const byte *roomPalette = room ? (prepared._roomPalette.empty() ? room->palette : prepared._roomPalette.data()) : nullptr;
	Bytes picture;
	if (!StageView::compose(flat, palette, frame, roomPalette, picture, false)) { error = "Invalid saved picture"; return false; }
	if (frame) memcpy(palette, roomPalette, 128 * 3);
	prepared.compositeSprites(picture, palette, room, frame);
	if (!prepared.runtime.error.empty()) { error = prepared.runtime.error; return false; }
	SaveState::commit(*this, prepared, io);
	if (runtime.error.empty()) for (const auto &entry : journal.entries) if (entry.choices.empty()) heard.mark(entry.file, entry.ident, entry.text);
	return runtime.error.empty();
}
}
