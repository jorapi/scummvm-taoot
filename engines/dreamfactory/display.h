/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_DISPLAY_H
#define DREAMFACTORY_DISPLAY_H
#include "dreamfactory/archive.h"
class OSystem;
namespace DreamFactory {
// One backing picture for indexed rooms/movies and RGBA stage/conversation composition. Fades
// always read the unmodified picture, never the previous darkened ramp step.
class Display {
public:
	explicit Display(OSystem *system) : _system(system) { reset(); }
	void reset();
	bool rgb() const;
	bool puppet() const { return _puppet; }
	void palette(const byte *palette, bool update = true);
	const byte *palette() const { return _palette; }
	void indexed(const Bytes &pixels, uint32 width, uint32 height, int32 x, int32 y, const byte *palette, bool black = false);
	bool rgba(const Bytes &pixels, uint32 width, uint32 height, bool black = false, bool puppet = true);
	static bool scaleRGBA(const Bytes &pixels, uint32 width, uint32 height, Bytes &screen);
	void fade(uint64 numerator, uint64 denominator);
	void clear();
	void restore(bool black = false);
	bool capture(Bytes &rgba) const;
	bool transition(const Bytes &from, const Common::String &effect, uint32 step, uint32 steps, bool lowResolution = false);
	// Compose the next backing picture without exposing it before a reveal.
	void hold(bool held) { _held = held; }
	const Bytes &picture() const { return _picture; }
private:
	OSystem *_system;
	Bytes _indices, _picture, _transfer, _saved, _savedIndices;
	byte _palette[768] = {};
	byte _savedPalette[768] = {};
	bool _puppet = false;
	bool _held = false;
	bool _indexed = true, _savedIndexed = true;
	bool promote();
	void colorize();
	void paint(uint64 numerator = 1, uint64 denominator = 1, const Bytes *picture = nullptr);
};
}
#endif
