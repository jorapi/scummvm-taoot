/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_STAGE_H
#define DREAMFACTORY_STAGE_H
#include "dreamfactory/archive.h"
#include "common/rect.h"
namespace DreamFactory {
struct Region {
	Common::Rect rect;
	uint32 script = 0;
	Common::String name;
};
struct Flat {
	int32 condition = 0;
	uint32 script = 0, image = 0, clickLogic = 0;
	uint16 width = 0, height = 0;
	Common::String name;
	Common::Array<Region> regions;
};
// The shared 2D interface format: Dust FLT (v1), Titanic STG (v4).
class Stage {
public:
	bool load(const Bytes &data);
	int32 findFlat(const Common::String &name) const;
	int32 hitTest(uint32 flat, int16 x, int16 y) const;
	Archive archive;
	uint32 version = 0, mainScript = 0;
	uint16 width = 0, height = 0;
	Common::String name;
	byte palette[768];
	Common::Array<Flat> flats;
private:
	bool regions(uint32 container, Common::Array<Region> &out);
};
}
#endif
