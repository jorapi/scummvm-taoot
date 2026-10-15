/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_STARS_H
#define DREAMFACTORY_STARS_H
#include "dreamfactory/archive.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
namespace DreamFactory {
struct Star {
	Common::String name;
	int32 x = 0, y = 0, z = 0, facing = 0;
};
struct StarPoint { int32 x = 0, y = 0, z = 0, distance = 0; };
struct StarPath {
	Common::String a, b;
	uint32 container = 0;
	Common::Array<StarPoint> points;
};
class StarTable {
public:
	uint32 version = 0;
	bool load(const Archive &archive, uint32 version);
	const Star *find(const Common::String &name) const;
	Common::Array<Star> stars;
	Common::Array<StarPath> paths;
};
// Current-room placements take precedence over positions remembered from
// rooms visited earlier. Dust scripts also place actors from outside a room.
class StarRegistry {
public:
	void reset() { current = StarTable(); known.clear(); knownPaths.clear(); }
	void bind(const StarTable &table);
	const Star *find(Common::String name) const;
	const StarPath *path(const Common::String &from, const Common::String &to) const;
	StarTable current;
private:
	friend class SaveState;
	Common::HashMap<Common::String, Star> known;
	Common::Array<StarPath> knownPaths;
};
}
#endif
