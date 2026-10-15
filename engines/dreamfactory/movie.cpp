/* SPDX-License-Identifier: GPL-3.0-or-later
 * Movie layouts adapted from dreamREfactory/df/mov.ts and mov-v1.ts.
 * Only the supplied Windows DreamFactory 1 and 4 formats are supported.
 */
#include "dreamfactory/movie.h"
namespace DreamFactory {
int32 MovieSegment::findFrame(const Common::String &name) const {
	for (uint32 i = 0; i < frames.size(); ++i) if (frames[i].name.equalsIgnoreCase(name)) return int32(i);
	return -1;
}
void MovieSegment::addSound(const MovieSound &sound) {
	for (uint32 i = 0; i < sounds.size(); ++i) if (sounds[i].name == sound.name) {
		sounds[i].container = sound.container; sounds[i].loop = sound.loop; if (!sound.follow.empty()) sounds[i].follow = sound.follow; return;
	}
	sounds.push_back(sound);
}
static Common::String optionalName(const Bytes &data, uint32 at, uint32 maximum = 15) {
	if (at >= data.size() || data[at] > maximum || data[at] > data.size() - at - 1) return Common::String();
	Reader r(data); return r.name(at, data[at] + 1);
}
static void moviePalette(const Bytes &head, uint32 at, byte *palette) {
	for (uint32 i = 0; i < 256; ++i) for (uint32 c = 0; c < 3; ++c) palette[i * 3 + c] = head[at + i * 8 + 3 + c * 2];
	palette[0] = palette[1] = palette[2] = 0; palette[765] = palette[766] = palette[767] = 255;
}
bool Movie::load(const Bytes &data) {
	segments.clear(); version = 0;
	if (!archive.load(data)) return false;
	Reader r(archive.get(0), 2); version = r.u32();
	if (!r.good() || (version != 1 && version != 4)) return false;
	Common::HashMap<uint32, bool> seen; uint32 bias = 0;
	for (;;) {
		if (seen.contains(bias) || !archive.has(bias)) return false;
		seen[bias] = true; MovieSegment segment; segment.bias = bias; uint32 next = 0;
		if (!(version == 1 ? readV1(segment, next) : readV4(segment, next))) return false;
		segments.push_back(segment); if (!next) break; bias = next;
	}
	if (version == 1) {
		Common::HashMap<uint32, bool> images;
		for (uint32 s = 0; s < segments.size(); ++s) for (uint32 f = 0; f < segments[s].frames.size(); ++f) images[segments[s].frames[f].image] = true;
		for (uint32 c = 1; c < archive.size(); ++c) {
			if (images.contains(c) || seen.contains(c) || !archive.has(c)) continue;
			const Bytes &chunk = archive.get(c); Reader a(chunk);
			if (chunk.size() < 48 || a.u32() != 0x10000) return false;
			int32 owner = -1; uint32 highest = 0;
			for (uint32 s = 0; s < segments.size(); ++s) if (segments[s].bias < c && (owner < 0 || segments[s].bias > highest)) { owner = s; highest = segments[s].bias; }
			if (owner < 0) return false;
			MovieSound sound; sound.name = Common::String::format("%u", c); sound.container = c; segments[owner].addSound(sound);
		}
	}
	return !segments.empty();
}
bool Movie::readV4(MovieSegment &segment, uint32 &next) {
	const uint32 bias = segment.bias; const Bytes &head = archive.get(bias); Reader r(head, 2);
	if (head.size() < 2172 || r.u32() != 4) return false;
	r.seek(24); segment.flags = r.u32(); segment.keySkips = segment.flags & 1; segment.minHold = int32(r.u32());
	r.seek(36); segment.x = int16(r.u16()); segment.y = int16(r.u16()); r.seek(44); next = r.u32();
	segment.action1 = optionalName(head, 64); segment.action2 = optionalName(head, 80);
	r.seek(96); const uint32 singles = r.u32(), loops = r.u32(), cues = r.u32();
	moviePalette(head, 108, segment.palette);
	r.seek(2160); segment.height = r.u16(); segment.width = r.u16(); r.u32(); const uint32 count = r.u32();
	if (!segment.width || !segment.height || segment.width > 640 || segment.height > 480 || count > (head.size() - 2172) / 42 || !count) return false;
	for (uint32 f = 0; f < count; ++f) {
		MovieFrame frame; frame.record = 2172 + f * 42; r.seek(frame.record + 8);
		frame.height = r.u16(); frame.width = r.u16(); const uint32 picture = r.u32(), logic = r.u32();
		if (picture > UINT32_MAX - bias || logic > UINT32_MAX - bias) return false;
		frame.image = picture + bias; frame.logic = logic ? logic + bias : 0; frame.name = optionalName(head, frame.record + 26);
		if (!archive.has(frame.image) || (frame.logic && !archive.has(frame.logic))) return false;
		const Bytes &data = archive.get(frame.logic);
		if (frame.logic && data.size() >= 66) {
			Reader l(data); frame.type = int16(l.u16()); frame.hold = int32(l.u32()); const byte flags = l.u8();
			frame.waitVoice = flags & 1; frame.playThrough = flags & 4; frame.holdDeadline = flags & 8;
			frame.sound = optionalName(data, 18); frame.event = optionalName(data, 34); frame.target = optionalName(data, 50);
			if (data.size() >= 1094) {
				l.seek(1090); const uint32 regions = l.u32(); if (regions > (data.size() - 1094) / 64) return false;
				for (uint32 g = 0; g < regions; ++g) {
					MovieRegion region; region.record = 1094 + g * 64; l.seek(region.record); region.type = int16(l.u16()); l.seek(region.record + 8);
					region.top = int16(l.u16()); region.left = int16(l.u16()); region.bottom = int16(l.u16()); region.right = int16(l.u16());
					region.sound = optionalName(data, region.record + 16); region.event = optionalName(data, region.record + 32); region.target = optionalName(data, region.record + 48); frame.regions.push_back(region);
				}
			}
			if (!l.good()) return false;
		}
		segment.frames.push_back(frame);
	}
	if (loops) {
		if (loops > UINT32_MAX - bias || !archive.has(loops + bias)) return false;
		const Bytes &table = archive.get(loops + bias); Reader l(table, 4);
		if (table.size() >= 270) {
			const uint32 length = l.u16(); if (length > 130) return false;
			Common::Array<uint32> order; for (uint32 i = 0; i < length; ++i) order.push_back(l.u16());
			l.seek(266); const uint32 records = l.u32(); if (records > (table.size() - 270) / 26) return false;
			segment.bedRecords = records;
			Common::Array<uint32> chunks;
			for (uint32 i = 0; i < records; ++i) { l.seek(270 + i * 26 + 4); const uint32 loc = l.u32(); if (loc > UINT32_MAX - bias || !archive.has(loc + bias)) return false; chunks.push_back(loc + bias); }
			for (uint32 i = 0; i < order.size(); ++i) if (order[i] > 0 && order[i] <= chunks.size()) segment.bed.push_back(chunks[order[i] - 1]);
			Reader root(table); const int32 authored = int32(root.u32());
			if (!segment.bed.empty()) segment.bedRepeat = uint32(CLIP<int32>(authored, 0, MIN<uint32>(records - 1, segment.bed.size() - 1)));
			if (!l.good()) return false;
		}
	}
	segment.audioLoops = !segment.bed.empty();
	if (singles) {
		if (singles > UINT32_MAX - bias || !archive.has(singles + bias)) return false;
		const Bytes &table = archive.get(singles + bias); Reader l(table, 4); if (table.size() < 8) return false;
		const uint32 records = l.u32(); if (records > (table.size() - 8) / 42) return false;
		for (uint32 i = 0; i < records; ++i) {
			const uint32 at = 8 + i * 42; l.seek(at); const uint32 flags = l.u32(), loc = l.u32(); if (loc > UINT32_MAX - bias || !archive.has(loc + bias)) return false;
			MovieSound sound; sound.name = optionalName(table, at + 10); sound.name.toLowercase(); sound.follow = optionalName(table, at + 26); sound.container = loc + bias; sound.loop = flags & 2;
			if (!sound.name.empty()) segment.addSound(sound);
		}
		if (!l.good()) return false;
	}
	if (cues) {
		if (cues > UINT32_MAX - bias || !archive.has(cues + bias)) return false;
		const Bytes &table = archive.get(cues + bias); Reader l(table); if (table.size() < 4) return false;
		const uint32 records = l.u32(); if (records > (table.size() - 4) / 28) return false;
		for (uint32 i = 0; i < records; ++i) { l.seek(4 + i * 28); MovieCue cue; cue.tick = int32(l.u32()); cue.target = optionalName(table, 16 + i * 28); segment.cues.push_back(cue); }
		if (!l.good()) return false;
	}
	return r.good();
}
static Common::String numberedTarget(int32 index, uint32 count) { return Common::String::format("%u", CLIP<int32>(index, 0, count - 1) + 1); }
bool Movie::readV1(MovieSegment &segment, uint32 &next) {
	const uint32 bias = segment.bias; const Bytes &head = archive.get(bias); Reader r(head, 2);
	if (head.size() < 2242 || r.u32() != 1) return false;
	r.seek(24); const uint32 count = r.u16(); if (!count || count > (head.size() - 2242) / 80) return false;
	// DF.EXE 418e0c loads bed records after the preloaded one-shots;
	// 418e83 then copies them in the header's authored order, including repeats.
	// Unreferenced audio elsewhere in the segment is not a soundtrack record.
	const uint32 singles = r.u16(), beds = r.u16();
	segment.bedRecords = beds;
	if (uint64(bias) + singles + beds >= archive.size()) return false;
	r.seek(52); const uint32 bedLength = r.u16(); if (bedLength > 64) return false;
	r.seek(2110);
	for (uint32 i = 0; i < bedLength; ++i) {
		const uint32 index = r.u16();
		if (!index || index > beds || !archive.has(bias + singles + index)) return false;
		segment.bed.push_back(bias + singles + index);
	}
	r.seek(2238); segment.bedRepeat = r.u32();
	if (!segment.bed.empty() && segment.bedRepeat >= segment.bed.size()) return false;
	segment.audioLoops = !segment.bed.empty();
	r.seek(34); segment.height = r.u16(); segment.width = r.u16(); segment.minHold = int16(r.u16()); if (!segment.minHold) segment.minHold = 3;
	if (!segment.width || !segment.height || segment.width > 640 || segment.height > 480) return false;
	r.seek(46); const int16 a1 = int16(r.u16()), a2 = int16(r.u16());
	if (a1 >= 0) segment.action1 = Common::String::format("%d", a1 + 1);
	if (a2 >= 0) segment.action2 = Common::String::format("%d", a2 + 1);
	r.seek(54); next = r.u16(); moviePalette(head, 62, segment.palette); segment.keySkips = true;
	for (uint32 f = 0; f < count; ++f) {
		MovieFrame frame; frame.record = 2242 + f * 80; frame.width = segment.width; frame.height = segment.height; frame.name = Common::String::format("%u", f + 1);
		r.seek(frame.record); const uint32 regions = r.u16(); frame.hold = int32(r.u32()); const uint32 legacy = r.u16();
		r.seek(frame.record + 22); const int32 action = int16(r.u16()), target = int16(r.u16()); const uint32 flags = r.u16(), image = r.u16();
		if (!image || image > UINT32_MAX - bias || !archive.has(image + bias)) return false;
		frame.image = image + bias; frame.waitVoice = (legacy & 1) || (flags & 1); frame.playThrough = regions == 0 || (flags & 4);
		r.seek(frame.record + 32); const int64 sound = int32(r.u32()); const int32 regionAt = int16(r.u16());
		if (sound) { const uint64 loc = (sound < 0 ? -sound : sound) + bias; if (loc > UINT32_MAX || !archive.has(uint32(loc))) return false; frame.sound = Common::String::format("%u", uint32(loc)); }
		frame.type = action >= 1 && action <= 5 ? action : 6;
		if (action == 1 && f + 1 < count) frame.type = 3;
		if (action == 2) frame.target = numberedTarget(target, count);
		if (action == 3) {
			frame.event = optionalName(head, frame.record + 48, 31);
			for (uint32 c = 0; c < frame.event.size(); ++c) if (byte(frame.event[c]) < 32 || byte(frame.event[c]) > 126) { frame.event.clear(); break; }
		}
		if (regions && regionAt > 0) {
			Reader h(head, regionAt);
			for (uint32 g = 0; g < regions; ++g) {
				MovieRegion region; region.record = h.pos(); const int32 rawType = int16(h.u16()); region.type = rawType < 0 ? -rawType : rawType;
				const uint32 length = region.type == 1 || region.type == 5 ? 14 : region.type == 2 ? 16 : region.type == 3 ? 46 : region.type == 4 ? 48 : 0;
				if (!length || region.record > head.size() || length > head.size() - region.record) return false;
				region.top = int16(h.u16()); region.left = int16(h.u16()); region.bottom = int16(h.u16()); region.right = int16(h.u16()); const int32 soundRef = int16(h.u16()); h.u16();
				if (soundRef > 0) { if (!archive.has(bias + soundRef)) return false; region.sound = Common::String::format("%u", bias + soundRef); }
				if (region.type == 2) region.target = numberedTarget(int16(h.u16()), count);
				h.seek(region.record + length); frame.regions.push_back(region);
			}
			if (!h.good()) return false;
		}
		segment.frames.push_back(frame);
	}
	return r.good();
}
bool Movie::decode(uint32 segment, Common::Array<Frame> &out) const {
	if (segment >= segments.size()) return false;
	Common::Array<Frame> decoded; Frame raw; uint64 retained = 0;
	for (uint32 f = 0; f < segments[segment].frames.size(); ++f) {
		if (!decodeFrame(archive.get(segments[segment].frames[f].image), raw)) return false;
		retained += raw.pixels.size(); if (retained > 256 * 1024 * 1024) return false;
		Frame shown = raw;
		if (version == 1 && !decoded.empty() && decoded.back().pixels.size() == shown.pixels.size()) for (uint32 i = 0; i < shown.pixels.size(); ++i)
			if (shown.pixels[i] == 0 || shown.pixels[i] == 255) shown.pixels[i] = decoded.back().pixels[i];
		decoded.push_back(shown);
	}
	out.swap(decoded); return true;
}
bool MoviePlayer::start(const Common::String &name, uint64 now) {
	if (active()) finish(true);
	error.clear(); _returns.clear(); _actions = 0; _now = now; _interactive = false;
	_movie = _host.loadMovie(name);
	if (!_movie) { fail("Could not load movie: " + name); return false; }
	return enterSegment(0);
}
void MoviePlayer::fail(const Common::String &message) { error = message; finish(true); }
bool MoviePlayer::enterSegment(uint32 segment, uint32 frame) {
	if (!_movie || segment >= _movie->segments.size() || frame >= _movie->segments[segment].frames.size()) {
		fail("Invalid movie segment or frame"); return false;
	}
	_segment = segment; _segmentStart = _now; _deadline = _now; _soundJump = -1;
	const MovieSegment &seg = _movie->segments[_segment];
	_cues.clear(); _cues.resize(seg.cues.size());
	for (uint32 c = 0; c < _cues.size(); ++c) _cues[c] = false;
	_autoSegment = _movie->segments.size() > 1 || seg.audioLoops;
	for (uint32 f = 0; f < seg.frames.size(); ++f) {
		if (!seg.frames[f].regions.empty()) { _interactive = true; _autoSegment = true; }
		if (seg.frames[f].type == 6 || seg.frames[f].type == 7 ||
			(_movie->version == 1 && seg.frames[f].type == 2 && seg.findFrame(seg.frames[f].target) > int32(f))) _autoSegment = true;
	}
	if (!_host.movieSegment(*_movie, _segment)) { fail("Could not decode movie segment"); return false; }
	enterFrame(frame); return true;
}
void MoviePlayer::playSound(const Common::String &name) {
	if (name.empty()) return;
	const MovieSegment &seg = _movie->segments[_segment];
	const MovieSound *sound = nullptr;
	for (const auto &entry : seg.sounds) if (entry.name.equalsIgnoreCase(name)) { sound = &entry; break; }
	// Native 44ac20 returns before changing voice or follow state on no match.
	if (!sound) return;
	_soundJump = -1;
	if (!_host.movieSound(name)) { fail("Could not play movie sound: " + name); return; }
	if (!sound->follow.empty()) _soundJump = seg.findFrame(sound->follow);
}
void MoviePlayer::enterFrame(uint32 frame, const Common::String &clickSound) {
	_frame = frame;
	const MovieSegment &seg = _movie->segments[_segment]; const MovieFrame &f = seg.frames[frame];
	if (!seg.action1.empty() && f.name.equalsIgnoreCase(seg.action1)) _actions |= 1;
	if (!seg.action2.empty() && f.name.equalsIgnoreCase(seg.action2)) _actions |= 2;
	// The hold unit is 1/60 second. Round up so a fractional millisecond never
	// advances the picture before its authored deadline.
	const uint64 hold = (uint64(MAX<int32>(0, MAX(f.hold, seg.minHold))) * 50 + 2) / 3;
	_deadline = (f.holdDeadline ? _deadline : _now) + hold;
	_host.movieFrame(frame);
	if (!f.sound.empty() && !f.sound.equalsIgnoreCase(clickSound)) playSound(f.sound);
}
void MoviePlayer::endSegment() {
	if (_segment + 1 < _movie->segments.size()) enterSegment(_segment + 1);
	else finish();
}
void MoviePlayer::action(const MovieAction &a, const Common::String &clickSound) {
	// Copy before a chain releases the source movie and its action storage.
	const int32 type = a.type; const Common::String target = a.target, event = a.event;
	const MovieSegment &seg = _movie->segments[_segment];
	switch (type) {
	case 2: {
		const int32 frame = seg.findFrame(target);
		if (frame < 0) fail("Movie frame not found: " + target);
		else enterFrame(uint32(frame), clickSound);
		break;
	}
	case 3: case 4: {
		if (type == 4) {
			const int32 frame = seg.findFrame(target);
			if (frame < 0 || _returns.size() >= 5) { fail("Invalid movie return target or call depth"); break; }
			Return ret; ret.movie = _movie; ret.segment = _segment; ret.frame = uint32(frame); _returns.push_back(ret);
		}
		if (event.empty()) { finish(); break; }
		Common::SharedPtr<Movie> next = _host.loadMovie(event);
		if (!next) { fail("Could not load chained movie: " + event); break; }
		_movie = next; enterSegment(0); break;
	}
	case 5:
		if (_returns.empty()) finish();
		else {
			const Return ret = _returns.back(); _returns.pop_back(); _movie = ret.movie; enterSegment(ret.segment, ret.frame);
		}
		break;
	case 6:
		if (_frame + 1 < seg.frames.size()) enterFrame(_frame + 1, clickSound); else endSegment();
		break;
	case 7: if (_frame) enterFrame(_frame - 1, clickSound); break;
	default: endSegment(); break;
	}
}
void MoviePlayer::tick(uint64 now) {
	if (!active()) return;
	_now = MAX(now, _now);
	const MovieSegment &seg = _movie->segments[_segment];
	// Timed cues and sound follows can release both a voice wait and a modal
	// click frame. Process one transition per poll, including zero-hold films.
	for (uint32 c = 0; c < seg.cues.size(); ++c) if (!_cues[c] &&
		(_now - _segmentStart) * 3 >= uint64(MAX<int32>(0, seg.cues[c].tick)) * 50) {
		_cues[c] = true; const int32 target = seg.findFrame(seg.cues[c].target);
		// The demo's OPEN.MOV contains a cue naming a frame absent from
		// this segment. Like the reference player, consume that
		// cue without interrupting the film or delaying other due cues.
		if (target < 0) continue;
		enterFrame(uint32(target));
		return;
	}
	if (_soundJump >= 0 && _host.movieVoiceDone()) { const uint32 frame = _soundJump; _soundJump = -1; enterFrame(frame); return; }
	const MovieFrame &frame = seg.frames[_frame];
	if (!_autoSegment) return;
	if (!frame.regions.empty() && !frame.playThrough) return;
	if (_now < _deadline || (frame.waitVoice && !_host.movieVoiceDone())) return;
	action(frame);
}
void MoviePlayer::click(int32 x, int32 y, uint64 now) {
	if (!active()) return;
	_now = MAX(now, _now); const MovieSegment &seg = _movie->segments[_segment];
	if (seg.flags & 8) { finish(true); return; }
	const MovieFrame &frame = seg.frames[_frame];
	x -= seg.x; y -= seg.y;
	for (uint32 r = 0; r < frame.regions.size(); ++r) {
		const MovieRegion &region = frame.regions[r];
		if (x >= region.left && x < region.right && y >= region.top && y < region.bottom) {
			const Common::String sound = region.sound;
			if (!sound.empty()) playSound(sound);
			if (active()) action(region, sound);
			return;
		}
	}
	// Outside clicks on a region frame are consumed. A region-less close-up
	// still permits the player to execute its current frame action directly.
	if (frame.regions.empty()) action(frame);
}
void MoviePlayer::key(bool escape) {
	if (active() && ((_movie->segments[_segment].flags & 8) || (escape && _movie->segments[_segment].keySkips))) finish(true);
}
void MoviePlayer::finish(bool dismissed) {
	if (!_movie) return;
	const bool keepInput = _segment < _movie->segments.size() && (_movie->segments[_segment].flags & 8);
	_movie.reset(); _returns.clear(); _soundJump = -1;
	_host.movieFinished(dismissed, _interactive, keepInput);
}
}
