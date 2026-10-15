/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ENHANCEMENTS_H
#define DREAMFACTORY_ENHANCEMENTS_H
#include "dreamfactory/audio.h"
#include "common/fs.h"
#include "common/formats/ini-file.h"
namespace DreamFactory {
// Optional, edition-bound assets live outside the original resource archives.
class EnhancementPack {
public:
	bool init(const Common::FSNode &directory, const Common::String &bootMD5);
	bool music(const Common::String &source, Common::Array<Common::SharedPtr<AudioClip> > &parts, int32 &repeat, bool *preserveIntro = nullptr) const;
	bool artwork(const Common::String &source, const Common::String &flat, const Bytes &originalPixels, Bytes &screen);
	bool active() const { return _active; }
	void clear() { _active = false; _manifest.clear(); _artKey.clear(); _artPixels.clear(); }
private:
	Common::FSNode _root;
	Common::INIFile _manifest;
	bool _active = false;
	Common::String _artKey;
	Bytes _artPixels;
	uint32 _artX = 0, _artY = 0, _artWidth = 0, _artHeight = 0;
	bool file(const Common::String &relative, Common::FSNode &node) const;
};
}
#endif
