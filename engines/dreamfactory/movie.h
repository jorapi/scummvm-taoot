/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_MOVIE_H
#define DREAMFACTORY_MOVIE_H
#include "dreamfactory/archive.h"
#include "dreamfactory/image.h"
#include "common/hashmap.h"
#include "common/ptr.h"
namespace DreamFactory {
struct MovieAction {
	int32 type = 6;
	Common::String sound, event, target;
};
struct MovieRegion : MovieAction {
	int16 top = 0, left = 0, bottom = 0, right = 0;
	uint32 record = 0;
};
struct MovieFrame : MovieAction {
	uint32 image = 0, logic = 0, record = 0;
	uint16 width = 0, height = 0;
	Common::String name;
	int32 hold = 0;
	bool waitVoice = false, holdDeadline = false, playThrough = false;
	Common::Array<MovieRegion> regions;
};
struct MovieSound { Common::String name, follow; uint32 container = 0; bool loop = false; };
struct MovieCue { int32 tick = 0; Common::String target; };
struct MovieSegment {
	uint32 bias = 0, flags = 0;
	uint16 width = 0, height = 0;
	int16 x = 0, y = 0;
	int32 minHold = 0;
	bool keySkips = false, audioLoops = false;
	uint32 bedRecords = 0, bedRepeat = 0;
	Common::String action1, action2;
	byte palette[768] = {};
	Common::Array<MovieFrame> frames;
	Common::Array<uint32> bed;
	Common::Array<MovieSound> sounds;
	Common::Array<MovieCue> cues;
	int32 findFrame(const Common::String &name) const;
	void addSound(const MovieSound &sound);
};
class Movie {
public:
	bool load(const Bytes &data);
	// Decode a segment in authored order, retaining raw delta state separately
	// from Dust's transparency composite. Random playback jumps use these images.
	bool decode(uint32 segment, Common::Array<Frame> &frames) const;
	Archive archive;
	Common::String source;
	uint32 version = 0;
	Common::Array<MovieSegment> segments;
private:
	bool readV4(MovieSegment &segment, uint32 &next);
	bool readV1(MovieSegment &segment, uint32 &next);
};
// Platform operations are separate from the modal state machine so authored
// actions can be checked with a deterministic clock and audio completion.
class MovieHost {
public:
	virtual ~MovieHost() {}
	virtual Common::SharedPtr<Movie> loadMovie(const Common::String &name) = 0;
	virtual bool movieSegment(const Movie &movie, uint32 segment) = 0;
	virtual void movieFrame(uint32 frame) = 0;
	virtual bool movieSound(const Common::String &name) = 0;
	virtual bool movieVoiceDone() const = 0;
	virtual void movieFinished(bool dismissed, bool interactive, bool keepInput) = 0;
};
class MoviePlayer {
public:
	explicit MoviePlayer(MovieHost &host) : _host(host) {}
	bool start(const Common::String &name, uint64 now);
	void tick(uint64 now);
	void click(int32 x, int32 y, uint64 now); // Native movie canvas coordinates.
	void key(bool escape);
	void abandon() { finish(true); }
	void reset() { finish(true); _returns.clear(); _actions = 0; error.clear(); }
	bool active() const { return bool(_movie); }
	uint32 actions() const { return _actions; }
	uint32 frame() const { return _frame; }
	uint32 segment() const { return _segment; }
	Common::String error;
private:
	struct Return { Common::SharedPtr<Movie> movie; uint32 segment, frame; };
	MovieHost &_host;
	Common::SharedPtr<Movie> _movie;
	Common::Array<Return> _returns;
	Common::Array<bool> _cues;
	uint32 _segment = 0, _frame = 0, _actions = 0;
	uint64 _now = 0, _segmentStart = 0, _deadline = 0;
	int32 _soundJump = -1;
	bool _interactive = false, _autoSegment = false;
	bool enterSegment(uint32 segment, uint32 frame = 0);
	void enterFrame(uint32 frame, const Common::String &clickSound = Common::String());
	void playSound(const Common::String &name);
	void action(const MovieAction &action, const Common::String &clickSound = Common::String());
	void endSegment();
	void finish(bool dismissed = false);
	void fail(const Common::String &message);
};
}
#endif
