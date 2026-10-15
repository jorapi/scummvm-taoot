/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_PUPPET_H
#define DREAMFACTORY_PUPPET_H
#include "dreamfactory/archive.h"
namespace DreamFactory {
struct PuppetLine {
	Common::String ident, text;
	int16 stance = 0;
	uint32 audio = 0, animation = 0, record = 0;
};
struct PuppetLayer {
	int16 x = 0, y = 0;
	Common::Array<uint32> frames;
};
struct PuppetStance {
	uint32 location = 0;
	PuppetLayer layers[11];
};
struct PuppetPose {
	struct Layer { int16 frame = -1, y = 0, x = 0; } layers[11];
};
struct PuppetScript { Common::String name; uint32 location = 0; };
class Puppet {
public:
	bool load(const Bytes &bytes);
	bool animation(uint32 location, Common::Array<PuppetPose> &poses) const;
	int32 line(const Common::String &name, bool textFallback = false) const;
	int32 textLine(const Common::String &text) const;
	Archive archive;
	uint32 version = 0, band = 0;
	Common::String name;
	byte palette[768] = {};
	int32 idleMin[4] = {}, idleMax[4] = {};
	Common::Array<PuppetLine> lines;
	Common::Array<PuppetScript> scripts;
	Common::Array<PuppetStance> stances;
private:
	bool stance(uint32 location);
};
}
#endif
