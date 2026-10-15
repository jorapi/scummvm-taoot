/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_CAST_H
#define DREAMFACTORY_CAST_H
#include "dreamfactory/archive.h"
namespace DreamFactory {
struct Sprite {
	uint16 width = 0, height = 0;
	int16 x = 0, y = 0;
	Bytes pixels, opaque;
};
// SHP/CST transparent run stream; decoding failure preserves the old sprite.
bool decodeSprite(const Bytes &data, Sprite &sprite);
struct CastFrame {
	uint32 location = 0, record = 0;
	int16 direction = 0, angle = 0, refScale = 0;
};
struct CastPose {
	Common::String name;
	uint32 location = 0, record = 0, frameCount = 0;
	Common::Array<Common::Array<CastFrame> > steps;
	Common::Array<uint32> play;
};
struct CastMember {
	Common::String name;
	uint32 logic = 0, script = 0;
	Common::Array<CastPose> poses;
};
class Cast {
public:
	bool load(const Bytes &data);
	Archive archive;
	uint32 version = 0, mainScript = 0;
	byte palette[768] = {};
	Common::Array<CastMember> members;
};
}
#endif
