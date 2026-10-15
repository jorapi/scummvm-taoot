/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_MOVIE_HOST_H
#define DREAMFACTORY_MOVIE_HOST_H
#include "dreamfactory/movie.h"
#include "dreamfactory/sound.h"
#include "dreamfactory/runtime.h"
#include "dreamfactory/display.h"
#include "common/events.h"
class OSystem;
namespace DreamFactory {
class CursorSystem;
// Owns the modal picture and bed; cues use SoundSystem's shared voice channel.
class MovieSystem : private MovieHost {
public:
	MovieSystem(Resources &resources, SoundSystem &sound, Display &display, Common::Array<Common::Event> &pending, OSystem *system);
	~MovieSystem();
	bool play(const Common::String &name, Runtime &runtime, ScriptFrame &frame);
	void reset();
	void setHotspotHints(bool enabled) { _showHints = enabled; }
	void setCursors(CursorSystem *cursors) { _cursors = cursors; }
	bool hotspotHints() const { return _showHints; }
	static bool compositeHotspotHints(Bytes &rgba, const MovieSegment &segment, uint32 frame, uint32 width, uint32 height);
	uint32 actions() const { return _player.actions(); }
	bool eventDone() const;
	bool bedActive() const;
	bool active() const { return _player.active(); }
	Common::String currentMovie() const { return _name; }
	uint32 canvasWidth() const { return _movie && _movie->version == 1 ? 640 : 512; }
	const MovieSegment *currentSegment() const { return _player.active() && _movie && _segment < _movie->segments.size() ? &_movie->segments[_segment] : nullptr; }
	uint32 currentFrame() const { return _player.frame(); }
	uint32 displayedFrames = 0;
private:
	friend class SaveState;
	Resources &_resources;
	SoundSystem &_sound;
	Display &_display;
	Common::Array<Common::Event> &_pending;
	OSystem *_system;
	CursorSystem *_cursors = nullptr;
	MoviePlayer _player;
	const Movie *_movie = nullptr;
	Common::String _name;
	uint32 _segment = 0;
	Common::Array<Frame> _images;
	Bytes _canvas, _scaled;
	Audio::SoundHandle _event;
	bool _hasBed = false;
	// Read only when upgrading old saves that used a separate movie mixer.
	Common::SharedPtr<AudioClip> _bedClip;
	bool _bedLoop = false;
	uint32 _bedStart = 0;
	bool _showHints = false;
	int32 _controlRegion = -1;
	uint32 _controlFrame = 0;
	bool _quit = false;
	Common::Event _quitEvent;
	void flushInput();
	void stopEvent();
	Common::SharedPtr<Movie> loadMovie(const Common::String &name) override;
	bool movieSegment(const Movie &movie, uint32 segment) override;
	void movieFrame(uint32 frame) override;
	bool movieSound(const Common::String &name) override;
	bool movieVoiceDone() const override { return _sound.done(true); }
	void movieFinished(bool dismissed, bool interactive, bool keepInput) override;
};
}
#endif
