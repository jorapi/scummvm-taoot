/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SHOP_H
#define DREAMFACTORY_SHOP_H
#include "dreamfactory/cast.h"
namespace DreamFactory {
struct PropPicture {
	uint32 location = 0, record = 0;
	int16 group = 0, degree = 0, refScale = 96;
};
struct PropState {
	Common::String name;
	uint32 location = 0, record = 0;
	Common::Array<PropPicture> pictures;
	Common::Array<uint32> play;
	bool animated = false;
};
struct PropGroup {
	Common::String name;
	uint32 location = 0, script = 0;
	Common::Array<PropState> states;
};
class Shop {
public:
	bool load(const Bytes &bytes);
	bool sprite(uint32 location, Sprite &out) const;
	Archive archive;
	uint32 version = 0, mainScript = 0;
	Common::String name;
	byte palette[768] = {};
	Common::Array<PropGroup> groups;
private:
	void orient(PropGroup &group);
};
}
#endif
