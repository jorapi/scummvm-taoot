/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_NAVIGATION_H
#define DREAMFACTORY_NAVIGATION_H
#include "dreamfactory/runtime.h"
#include "dreamfactory/stage.h"
#include "dreamfactory/shop.h"
#include "dreamfactory/image.h"
#include "common/ustr.h"
namespace DreamFactory {
class Session;
struct MapPosition {
	Common::String room, scene, view, deck;
	int32 page = 0, x = -1, y = -1;
	bool valid() const { return !room.empty() && !scene.empty() && !view.empty() && room.size() <= 128 && scene.size() <= 128 && view.size() <= 128 && deck.size() <= 128 && ((page == 0 && x == -1 && y == -1) || (page > 0 && page <= 8 && x >= 0 && x < 512 && y >= 0 && y < 384)); }
	bool samePlace(const MapPosition &other) const { return room.equalsIgnoreCase(other.room) && scene.equalsIgnoreCase(other.scene) && page == other.page && x == other.x && y == other.y; }
};
struct MapVisit { MapPosition position; uint64 first = 0, last = 0; };
struct MapPassage { uint32 from = 0, to = 0; Common::String exitView; };
struct MapEncounter { uint32 visit = 0; Common::String identity, file, speaker; uint64 time = 0; };
class TravelHistory {
public:
	static const uint32 kMaxVisits = 4096, kMaxPassages = 8192;
	Common::Array<MapVisit> visits;
	Common::Array<MapPassage> passages;
	Common::Array<MapEncounter> encounters;
	uint32 find(const MapPosition &position) const;
	uint32 visit(const MapPosition &position, uint64 time);
	void passage(const MapPosition &from, const MapPosition &to, uint64 time);
	void meet(const MapPosition &position, const Common::String &file, const Common::String &speaker, uint64 time);
	Common::Array<uint32> route(uint32 from, uint32 to) const;
	bool valid() const;
};
class NavigationAtlas {
	bool _attempted = false, _available = false, _retail = false;
	Stage _stage;
	Shop _dots;
	ScriptInstance _main;
	Common::Array<ScriptInstance> _flats;
	int32 _picturePage = 0;
	Frame _picture;
public:
	bool load(Session &session);
	bool project(const Session &session, MapPosition &position) const;
	const Frame *picture(uint32 page);
	const byte *palette() const { return _stage.palette; }
	uint32 pages() const { return _available ? _stage.flats.size() : 0; }
	bool retail() const { return _retail; }
};
Common::U32String mapLocationName(const MapPosition &position);
Common::U32String mapPageName(uint32 page, bool retail);
Common::U32String navigationDirection(const Session &session, uint32 destination);
bool auditTravelHistory();
}
#endif
