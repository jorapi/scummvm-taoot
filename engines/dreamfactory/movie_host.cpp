/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/movie_host.h"
#include "dreamfactory/controls.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "common/textconsole.h"
#include "graphics/paletteman.h"
namespace DreamFactory {
MovieSystem::MovieSystem(Resources &resources, SoundSystem &sound, Display &display, Common::Array<Common::Event> &pending, OSystem *system) :
	_resources(resources), _sound(sound), _display(display), _pending(pending), _system(system), _player(*this) {}
MovieSystem::~MovieSystem() { reset(); }
void MovieSystem::stopEvent() { if (_system) _system->getMixer()->stopHandle(_event); }
bool MovieSystem::eventDone() const { return !_system || !_system->getMixer()->isSoundHandleActive(_event); }
bool MovieSystem::bedActive() const { return _hasBed && _sound.themeActive(); }
void MovieSystem::reset() {
	_player.reset(); stopEvent();
	if (_hasBed) _sound.haltTheme();
	_hasBed = false;
	_images.clear(); _canvas.clear(); _scaled.clear(); _movie = nullptr; _name.clear();
	_quit = false; displayedFrames = 0;
	_bedClip.reset(); _bedLoop = false; _bedStart = 0;
}
void MovieSystem::flushInput() {
	_pending.clear(); if (!_system) return;
	Common::Event event;
	while (_system->getEventManager()->pollEvent(event)) {
		if (event.type == Common::EVENT_QUIT || event.type == Common::EVENT_RETURN_TO_LAUNCHER) { _quit = true; _quitEvent = event; }
	}
}
Common::SharedPtr<Movie> MovieSystem::loadMovie(const Common::String &name) {
	Bytes bytes; Common::SharedPtr<Movie> movie(new Movie());
	Common::String path = name;
	if (!path.hasSuffixIgnoreCase(".mov")) path += ".mov";
	if (!_resources.read(path, bytes) || !movie->load(bytes)) return Common::SharedPtr<Movie>();
	movie->source = path;
	debug("MOVIE PLAY %s segments=%u", path.c_str(), movie->segments.size()); return movie;
}
bool MovieSystem::movieSegment(const Movie &movie, uint32 segment) {
	if (!_system) return false;
	if (_movie) { stopEvent(); _sound.haltVoice(); }
	const bool newFile = _movie != &movie;
	if (newFile && _hasBed) { _sound.haltTheme(); _hasBed = false; }
	if (!movie.decode(segment, _images)) return false;
	_movie = &movie; _segment = segment;
	_controlRegion = -1;
	_name = movie.source;
	const MovieSegment &seg = movie.segments[segment];
	const uint32 width = movie.version == 1 ? 640 : 512, height = movie.version == 1 ? 480 : 384;
	_canvas.resize(width * height); memset(_canvas.data(), 0, _canvas.size());
	_scaled.resize(640 * 480);
	_display.palette(seg.palette, false);
	if (newFile || seg.bedRecords) {
		// First submission queues behind existing game music. A later segment
		// with new records replaces the movie bed; zero-record segments inherit.
		if (!newFile) _sound.haltTheme();
		_hasBed = seg.bedRecords != 0;
		if (!seg.bed.empty()) {
			Common::HashMap<uint32, Common::SharedPtr<AudioClip> > decoded;
			Common::Array<Common::SharedPtr<AudioClip> > clips;
			for (uint32 container : seg.bed) {
				if (!decoded.contains(container)) {
					Common::SharedPtr<AudioClip> clip(new AudioClip());
					if (!decodeAudio(movie.archive.get(container), *clip) || clip->samples.empty()) return false;
					decoded[container] = clip;
				}
				clips.push_back(decoded[container]);
			}
			if (!_sound.playMovieTheme(movie.source, clips, seg.bedRepeat)) return false;
		}
	}
	flushInput(); return true;
}
bool MovieSystem::compositeHotspotHints(Bytes &rgba, const MovieSegment &seg, uint32 frame, uint32 width, uint32 height) {
	// Mark only held, authored click regions. Playback and click geometry stay
	// unchanged; annotations never enter the indexed delta-decoding canvas.
	if (!width || !height || rgba.size() != 640 * 480 * 4 || frame >= seg.frames.size() ||
		(seg.flags & 8) || seg.frames[frame].playThrough) return false;
	bool marked = false;
	for (const MovieRegion &region : seg.frames[frame].regions) {
		const int32 left = MAX<int32>(0, seg.x + region.left), top = MAX<int32>(0, seg.y + region.top);
		const int32 right = MIN<int32>(width, seg.x + region.right), bottom = MIN<int32>(height, seg.y + region.bottom);
		if (left >= right || top >= bottom) continue;
		const int32 cx = (left + (right - left) / 2) * 640 / width;
		const int32 cy = (top + (bottom - top) / 2) * 480 / height;
		for (int32 dy = -5; dy <= 5; ++dy) for (int32 dx = -5; dx <= 5; ++dx) {
			const int32 edge = MAX(ABS(dx), ABS(dy)), x = cx + dx, y = cy + dy;
			if (edge < 3 || x < 0 || y < 0 || x >= 640 || y >= 480) continue;
			byte *pixel = rgba.data() + (y * 640 + x) * 4;
			pixel[0] = edge == 4 ? 255 : 0; pixel[1] = edge == 4 ? 230 : 0;
			pixel[2] = 0; pixel[3] = 255;
		}
		marked = true;
	}
	return marked;
}
void MovieSystem::movieFrame(uint32 frame) {
	if (!_movie || frame >= _images.size()) return;
	if (frame != _controlFrame) { _controlRegion = -1; _controlFrame = frame; }
	const MovieSegment &seg = _movie->segments[_segment]; const Frame &image = _images[frame];
	const int32 width = _movie->version == 1 ? 640 : 512, height = _movie->version == 1 ? 480 : 384;
	for (int32 y = MAX<int32>(0, seg.y); y < MIN<int32>(height, seg.y + image.height); ++y)
		for (int32 x = MAX<int32>(0, seg.x); x < MIN<int32>(width, seg.x + image.width); ++x)
			_canvas[y * width + x] = image.pixels[(y - seg.y) * image.width + x - seg.x];
	for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) _scaled[y * 640 + x] = _canvas[(y * height / 480) * width + x * width / 640];
	++displayedFrames;
	if ((_showHints || _controlRegion >= 0) && !seg.frames[frame].regions.empty()) {
		Bytes rgba; rgba.resize(640 * 480 * 4);
		for (uint32 i = 0; i < _scaled.size(); ++i) {
			memcpy(rgba.data() + i * 4, seg.palette + _scaled[i] * 3, 3); rgba[i * 4 + 3] = 255;
		}
		bool marked = _showHints && compositeHotspotHints(rgba, seg, frame, width, height);
		if (_controlRegion >= 0 && uint32(_controlRegion) < seg.frames[frame].regions.size()) {
			const MovieRegion &r = seg.frames[frame].regions[_controlRegion];
			controlMarker(rgba, 640, 480, (seg.x + (r.left + r.right) / 2) * 640 / width, (seg.y + (r.top + r.bottom) / 2) * 480 / height); marked = true;
		}
		if (marked && _display.rgba(rgba, 640, 480, false, false)) return;
	}
	_display.indexed(_scaled, 640, 480, 0, 0, seg.palette);
}
bool MovieSystem::movieSound(const Common::String &name) {
	if (!_movie || !_system) return false;
	Common::SharedPtr<AudioClip> clip; bool loop = false; const MovieSegment &seg = _movie->segments[_segment];
	for (uint32 i = 0; i < seg.sounds.size(); ++i) if (seg.sounds[i].name.equalsIgnoreCase(name)) {
		clip.reset(new AudioClip()); if (!decodeAudio(_movie->archive.get(seg.sounds[i].container), *clip)) return false;
		loop = _sound.isTitanicRetail() && seg.sounds[i].loop; break;
	}
	// PENOTE can name absent cues while puppet speech occupies shared voice.
	// Native 44ac20 leaves it alone on no table match; malformed clips fail.
	if (!clip) return true;
	if (clip->samples.empty()) return false;
	return _sound.playVoiceClip(name, clip, loop, &_event);
}
void MovieSystem::movieFinished(bool dismissed, bool interactive, bool keepInput) {
	// The player has already released its movie. Event sources polled by the
	// cleanup must never observe the borrowed pointer after that release.
	_movie = nullptr;
	if (_hasBed) _sound.haltTheme();
	_hasBed = false;
	stopEvent(); _sound.haltVoice(); // Native cleanup always releases channel 0.
	if (!keepInput) flushInput();
	_images.clear(); _canvas.clear(); _scaled.clear(); _movie = nullptr;
}
bool MovieSystem::play(const Common::String &name, Runtime &runtime, ScriptFrame &frame) {
	if (!_system) { runtime.fail("Movie playback requires a display/audio host"); return false; }
	_quit = false; displayedFrames = 0; uint32 last = _system->getMillis(); uint64 now = 0;
	if (!_player.start(name, now)) { runtime.fail(_player.error); return false; }
	last = _system->getMillis(); // Loading is outside the film's authored clock.
	uint32 traceAt = last;
	while (_player.active() && runtime.current(frame) && !_quit) {
		Common::Event event;
		while (_player.active() && _system->getEventManager()->pollEvent(event)) {
			if (event.type == Common::EVENT_QUIT || event.type == Common::EVENT_RETURN_TO_LAUNCHER) { _quit = true; _quitEvent = event; break; }
			if ((event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START || event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_END) && event.customType >= kPadUp && event.customType <= kPadRepeat) {
				if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_END || event.customType == kPadRepeat) continue;
				const MovieSegment &seg = _movie->segments[_segment];
				const auto &regions = seg.frames[_player.frame()].regions;
				if (event.customType == kPadInteract) {
					if (!regions.empty()) {
						const MovieRegion &r = regions[_controlRegion >= 0 ? _controlRegion : 0];
						_player.click(seg.x + (r.left + r.right) / 2, seg.y + (r.top + r.bottom) / 2, now);
					} else _player.click(0, 0, now);
				} else if (!regions.empty() && !seg.frames[_player.frame()].playThrough) {
					const int32 direction = event.customType == kPadPrevious || event.customType == kPadUp || event.customType == kPadLeft ? -1 : 1;
					_controlRegion = _controlRegion < 0 ? (direction < 0 ? regions.size() - 1 : 0) : (_controlRegion + direction + int32(regions.size())) % regions.size();
					movieFrame(_player.frame());
				}
				continue;
			}
			if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == 1) {
				if (ConfMan.hasKey("dreamfactory_hotspot_hints") && ConfMan.getBool("dreamfactory_hotspot_hints")) {
					_showHints = !_showHints;
					movieFrame(_player.frame());
				}
			} else if (event.type == Common::EVENT_KEYDOWN) _player.key(event.kbd.keycode == Common::KEYCODE_ESCAPE);
			else if (event.type == Common::EVENT_LBUTTONDOWN) {
				const uint32 w = _movie && _movie->version == 1 ? 640 : 512, h = w == 640 ? 480 : 384;
				_player.click(event.mouse.x * int32(w) / 640, event.mouse.y * int32(h) / 480, now);
			}
		}
		const uint32 sample = _system->getMillis(); now += uint32(sample - last); last = sample;
		if (uint32(sample - traceAt) >= 5000) {
			debug(1, "MOVIE STATUS %s segment=%u frame=%u drawn=%u clock=%llu voiceDone=%u voiceMillis=%u", _name.c_str(), _segment, _player.frame(), displayedFrames, (unsigned long long)now, movieVoiceDone() ? 1 : 0, _system->getMixer()->getSoundElapsedTime(_event));
			traceAt = sample;
		}
		if (!_quit) _player.tick(now);
		// Held interactive frames still need software-cursor redraws. Event
		// polling moves the cursor, but it does not present the dirty rectangle.
		_system->updateScreen();
		last = _system->getMillis(); // Segment reloads may decode many megabytes.
		runtime.yielded();
		if (_player.active()) _system->delayMillis(5);
	}
	if (_quit || !runtime.current(frame)) {
		_player.abandon(); stopEvent(); _sound.haltSounds(); _sound.haltVoice(); _sound.haltTheme();
		if (_quit) { _pending.push_back(_quitEvent); runtime.abandon(); }
		return false;
	}
	if (!_player.error.empty()) { runtime.fail(_player.error); return false; }
	return true;
}
}
