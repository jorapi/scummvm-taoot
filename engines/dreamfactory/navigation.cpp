/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/navigation.h"
#include "dreamfactory/session.h"
#include "common/tokenizer.h"
#include "common/translation.h"
namespace DreamFactory {
uint32 TravelHistory::find(const MapPosition &position) const {
	for (uint32 i = 0; i < visits.size(); ++i) if (visits[i].position.samePlace(position)) return i + 1;
	return 0;
}
uint32 TravelHistory::visit(const MapPosition &position, uint64 time) {
	if (!position.valid()) return 0;
	const uint32 found = find(position);
	if (found) { visits[found - 1].position = position; visits[found - 1].last = time; return found; }
	if (visits.size() >= kMaxVisits) return 0;
	MapVisit visit; visit.position = position; visit.first = visit.last = time; visits.push_back(visit); return visits.size();
}
void TravelHistory::passage(const MapPosition &from, const MapPosition &to, uint64 time) {
	if (from.samePlace(to)) return;
	const uint32 a = visit(from, time), b = visit(to, time);
	if (!a || !b) return;
	for (const auto &edge : passages) if (edge.from == a && edge.to == b && edge.exitView.equalsIgnoreCase(from.view)) return;
	if (passages.size() < kMaxPassages) { MapPassage edge; edge.from = a; edge.to = b; edge.exitView = from.view; passages.push_back(edge); }
}
Common::Array<uint32> TravelHistory::route(uint32 from, uint32 to) const {
	Common::Array<uint32> result;
	if (!from || !to || from > visits.size() || to > visits.size() || from == to) return result;
	Common::Array<uint32> queue, previous; previous.resize(visits.size() + 1, 0); queue.push_back(from); previous[from] = uint32(-1);
	for (uint32 at = 0; at < queue.size() && !previous[to]; ++at) {
		for (uint32 i = 0; i < passages.size(); ++i) {
			const auto &edge = passages[i];
			if (edge.from != queue[at] || !edge.to || edge.to > visits.size() || previous[edge.to]) continue;
			previous[edge.to] = i + 1; queue.push_back(edge.to);
		}
	}
	if (!previous[to]) return result;
	for (uint32 cursor = to; cursor != from; cursor = passages[previous[cursor] - 1].from) result.insert_at(0, previous[cursor] - 1);
	return result;
}
void TravelHistory::meet(const MapPosition &position, const Common::String &file, const Common::String &speaker, uint64 time) {
	Common::String identity = file; identity.toLowercase();
	for (int32 i = int32(identity.size()) - 1; i >= 0; --i) if (identity[i] == ':' || identity[i] == '/' || identity[i] == '\\') { identity = identity.substr(i + 1); break; }
	if (identity.hasSuffix(".pup")) identity.erase(identity.size() - 4);
	while (!identity.empty() && identity.lastChar() >= '0' && identity.lastChar() <= '9') identity.erase(identity.size() - 1);
	if (identity.empty() || identity.size() > 128 || file.size() > 256 || speaker.size() > 128) return;
	const uint32 id = visit(position, time); if (!id) return;
	MapEncounter encounter; encounter.visit = id; encounter.identity = identity; encounter.file = file; encounter.speaker = speaker; encounter.time = time;
	for (auto &old : encounters) if (old.identity == identity) { old = encounter; return; }
	if (encounters.size() < 512) encounters.push_back(encounter);
}
bool TravelHistory::valid() const {
	if (visits.size() > kMaxVisits || passages.size() > kMaxPassages || encounters.size() > 512) return false;
	Common::HashMap<Common::String, bool> places, edges;
	for (uint32 i = 0; i < visits.size(); ++i) {
		const auto &visit = visits[i];
		if (!visit.position.valid() || visit.first > visit.last) return false;
		Common::String room = visit.position.room, scene = visit.position.scene; room.toLowercase(); scene.toLowercase();
		const Common::String key = Common::String::format("%u:", uint32(room.size())) + room + scene + Common::String::format(":%d:%d:%d", visit.position.page, visit.position.x, visit.position.y);
		if (places.contains(key)) return false; places[key] = true;
	}
	for (uint32 i = 0; i < passages.size(); ++i) {
		const auto &edge = passages[i];
		if (!edge.from || !edge.to || edge.from > visits.size() || edge.to > visits.size() || edge.from == edge.to || edge.exitView.empty() || edge.exitView.size() > 128) return false;
		Common::String view = edge.exitView; view.toLowercase(); const Common::String key = Common::String::format("%u:%u:", edge.from, edge.to) + view;
		if (edges.contains(key)) return false; edges[key] = true;
	}
	Common::HashMap<Common::String, bool> speakers;
	for (const auto &encounter : encounters) {
		if (!encounter.visit || encounter.visit > visits.size() || encounter.identity.empty() || encounter.identity.size() > 128 || encounter.file.empty() || encounter.file.size() > 256 || encounter.speaker.size() > 128 || speakers.contains(encounter.identity)) return false;
		speakers[encounter.identity] = true;
	}
	return true;
}
namespace {
bool parse(const Bytes &bytes, ScriptInstance &instance) {
	ScriptTokens tokens; Common::String error;
	return decodeScript(bytes, tokens, error) && parseScript(tokens, instance.script, error);
}
// The game's coordinate functions run in a detached VM. Only scalar queries
// and a captured marker are allowed; no live scripts or props are invoked.
class MapProbe : public ScriptHost {
	const Session &_session;
public:
	Common::Point point = Common::Point(-1, -1);
	explicit MapProbe(const Session &session) : _session(session) {}
	bool invoke(Runtime &vm, const Expression &call, ScriptFrame &frame, Value &result) override {
		switch (call.opcode) {
		case 20053: result = Value(_session.roomSystem.name()); return true;
		case 16029: if (call.args.empty()) { result = Value(_session.roomSystem.sceneName()); return true; } return false;
		case 20002: case 20003: case 20004: case 20025: case 20054: case 20063: case 16018: case 16015: break;
		default: return false;
		}
		const Values args = vm.arguments(call, frame); if (!vm.current(frame)) return true;
		const Value a = args.empty() ? Value() : args[0], b = args.size() > 1 ? args[1] : Value(); result = Value();
		switch (call.opcode) {
		case 20002: result = Value(int16(uint32(a.number()) >> 16)); break;
		case 20003: result = Value(int16(a.number())); break;
		case 20004: result = Value(int32((uint32(a.number()) << 16) | uint16(b.number()))); break;
		case 20025: result = Value(a.number()); break;
		case 20054: {
			Common::StringTokenizer words(a.text(), b.text()); int32 index = args.size() > 2 ? args[2].number() : 1;
			while (!words.empty()) { const Common::String word = words.nextToken(); if (--index == 0) { result = Value(word); break; } } break;
		}
		case 20063: {
			const auto room = _session.roomSystem.current; const int32 scene = _session.roomSystem.scene;
			if (room && scene >= 0 && uint32(scene) < room->data.scenes.size()) result = Value(a.number() == 1 ? room->data.scenes[scene].x : a.number() == 2 ? room->data.scenes[scene].y : 0); break;
		}
		case 16018: if (a.text() != "spot" || args.size() != 3) return false; point = Common::Point(b.number(), args[2].number()); break;
		case 16015: if (a.text() != "spot" || args.size() != 2) return false; break;
		}
		return true;
	}
};
}
bool NavigationAtlas::load(Session &session) {
	if (_attempted) return _available;
	_attempted = true;
	if (session.version != 4) return false;
	Bytes bytes;
	if (!session.resources.read("map.stg", bytes) || !_stage.load(bytes) || _stage.flats.empty() || _stage.flats.size() > 8) return false;
	_retail = session.sound.isTitanicRetail();
	if (_retail) {
		if (!parse(_stage.archive.get(_stage.mainScript), _main) || !_main.script.handlers.contains("currentpage")) return false;
		_flats.resize(_stage.flats.size());
		for (uint32 i = 0; i < _flats.size(); ++i) { if (!parse(_stage.archive.get(_stage.flats[i].script), _flats[i])) return false; _flats[i].parent = &_main; }
	} else {
		if (!session.resources.read("map.shp", bytes) || !_dots.load(bytes) || !parse(_dots.archive.get(_dots.mainScript), _main) || !_main.script.handlers.contains("dotflat")) return false;
	}
	_available = true; return true;
}
bool NavigationAtlas::project(const Session &session, MapPosition &position) const {
	position = MapPosition();
	if (!_available || !session.roomSystem.current || session.roomSystem.scene < 0) return false;
	MapProbe probe(session); Runtime vm(&probe); vm.globals = session.runtime.globals;
	ScriptInstance main = _main;
	const int32 page = vm.run(main, _retail ? "currentpage" : "dotflat").value.number();
	if (!vm.error.empty() || page < 1 || uint32(page) > _stage.flats.size()) return false;
	if (_retail) { ScriptInstance flat = _flats[page - 1]; flat.parent = &main; vm.run(flat, "posdot"); }
	else {
		const Common::String state = vm.run(main, "dotview").value.text(); const int32 degree = vm.run(main, "dotdeg").value.number();
		for (const auto &group : _dots.groups) if (group.name.equalsIgnoreCase("dots")) for (const auto &view : group.states) if (view.name.equalsIgnoreCase(state)) {
			const PropPicture *picture = nullptr; uint32 distance = 257;
			for (const auto &frame : view.pictures) { const uint32 diff = (uint32(frame.degree) - uint32(degree)) & 255, gap = MIN(diff, 256 - diff); if (gap < distance) { distance = gap; picture = &frame; } }
			Sprite sprite;
			if (!picture || !_dots.sprite(picture->location, sprite)) return false;
			int64 x = 0, y = 0, pixels = 0;
			for (uint32 row = 0; row < sprite.height; ++row) for (uint32 col = 0; col < sprite.width; ++col) if (sprite.opaque[row * sprite.width + col]) { x += col; y += row; ++pixels; }
			if (pixels) probe.point = Common::Point(256 - sprite.x + x / pixels, 192 - sprite.y + y / pixels);
		}
	}
	position.room = session.roomSystem.name(); position.scene = session.roomSystem.sceneName(); position.view = session.roomSystem.viewName();
	position.page = page; position.x = probe.point.x; position.y = probe.point.y;
	if (session.runtime.globals.contains("savedeck")) position.deck = session.runtime.globals["savedeck"].text();
	if (!vm.error.empty() || position.x < 0 || position.y < 0 || position.x >= 512 || position.y >= 384) { position.page = 0; position.x = position.y = -1; }
	return position.valid();
}
const Frame *NavigationAtlas::picture(uint32 page) {
	if (!_available || !page || page > _stage.flats.size()) return nullptr;
	if (_picturePage != int32(page)) { Frame image; if (!decodeFrame(_stage.archive.get(_stage.flats[page - 1].image), image)) return nullptr; _picture = image; _picturePage = page; }
	return &_picture;
}
Common::U32String mapPageName(uint32 page, bool retail) {
	if (!page) return _("Not charted");
	if (!retail) return Common::U32String::format(_("Demo map %u"), page);
	if (page == 1) return _("Boat deck");
	return page >= 2 && page <= 8 ? Common::U32String::format(_("Deck %c"), 'A' + page - 2) : _("Unknown deck");
}
Common::U32String mapLocationName(const MapPosition &position) {
	if (position.room.equalsIgnoreCase("boil") && position.deck.size() == 5 && position.deck.hasPrefix("boil") && position.deck[4] >= '1' && position.deck[4] <= '5') return Common::U32String::format(_("Boiler room %u"), position.deck[4] - '0');
	const struct { const char *id, *label; } names[] = {
		{"gym", _s("Gymnasium")}, {"wireless", _s("Wireless room")}, {"halla", _s("A-deck corridor")}, {"hallb", _s("B-deck corridor")}, {"hallc", _s("C-deck corridor")},
		{"gstair1", _s("Grand Staircase")}, {"gstair2", _s("Grand Staircase")}, {"gstair3", _s("Grand Staircase")}, {"vestport", _s("Port vestibule")}, {"veststbd", _s("Starboard vestibule")},
		{"c73", _s("Cabin C-73")}, {"c71", _s("Cabin")}, {"b59", _s("Photographer's cabin")}, {"b70", _s("Cabin B-70")}, {"deckbd", _s("Boat deck")}, {"deckbd2", _s("Boat deck")},
		{"scot1", _s("Scotland Road")}, {"scot2", _s("Scotland Road")}, {"scot3", _s("Scotland Road")}, {"control", _s("Control room")}, {"turb", _s("Turbine room")}, {"engine", _s("Engine room")},
		{"boil", _s("Boiler room")}, {"cargo", _s("Cargo hold")}, {"turk", _s("Turkish bath")}, {"squash", _s("Squash court")}, {"hallf3c", _s("Third-class corridor")}, {"hallf2c", _s("Second-class corridor")},
		{"stair2c", _s("Second-class stairs")}, {"stair1c1", _s("First-class stairs")}, {"stair1c2", _s("First-class stairs")}, {"sqhall", _s("Squash-court stairs")}, {"turkstrs", _s("Bath stairwell")},
		{"halld", _s("D-deck corridor")}, {"a14", _s("Cabin A-14")}, {"c59", _s("Cabin C-59")}, {"c78", _s("Cabin C-78")}, {"smoke", _s("Smoking room")}, {"lounge1c", _s("First-class lounge")},
		{"bridge", _s("Bridge")}, {"bedsit1", _s("London apartment")}, {"smstack1", _s("Funnel")}, {"smstack2", _s("Funnel")}, {"smstack3", _s("Funnel")}
	};
	for (const auto &name : names) if (position.room.equalsIgnoreCase(name.id)) return _(name.label);
	return _("Visited location");
}
Common::U32String navigationDirection(const Session &session, uint32 destination) {
	MapPosition current;
	if (!session.atlas.project(session, current)) return _("Your current position is not on this map.");
	const uint32 start = session.travel.find(current);
	if (start == destination && start) return _("You are already at this visited location.");
	const auto route = session.travel.route(start, destination);
	if (route.empty()) return _("No recorded walking route reaches this location from here. Only passages you have walked are used; map jumps are not walking routes.");
	const auto &edge = session.travel.passages[route[0]];
	const auto room = session.roomSystem.current; const int32 scene = session.roomSystem.scene;
	if (room && scene >= 0 && uint32(scene) < room->data.scenes.size()) {
		const auto &spot = room->data.scenes[scene];
		const int32 target = session.roomSystem.viewIndex(scene, edge.exitView);
		if (target >= 0 && target != session.roomSystem.view) {
			int32 best = 0x7fffffff, direction = -1;
			for (int32 turn = 0; turn < 2; ++turn) {
				Common::Array<int32> ring;
				for (const auto &frame : spot.turns[turn]) if (frame.motion > 0 && frame.view >= 0 && (ring.empty() || ring.back() != frame.view)) ring.push_back(frame.view);
				if (ring.size() > 1 && ring[0] == ring.back()) ring.pop_back();
				for (uint32 i = 0; i < ring.size(); ++i) if (ring[i] == session.roomSystem.view)
					for (uint32 distance = 1; distance < ring.size(); ++distance) if (ring[(i + distance) % ring.size()] == target && int32(distance) < best) { best = distance; direction = turn; }
			}
			if (direction >= 0) return direction == 0 ? _("Turn right. Follow the recorded passage from this position.") : _("Turn left. Follow the recorded passage from this position.");
		}
	}
	return _("Walk forward through the passage you used before. A previously opened door may need to be opened again; this does not guarantee it is still accessible.");
}
bool auditTravelHistory() {
	TravelHistory travel; MapPosition a; a.room = "halla"; a.scene = "Scene52"; a.view = "View63"; a.page = 2; a.x = 100; a.y = 120;
	MapPosition b = a; b.scene = "Scene51"; b.view = "View60"; MapPosition c = b; c.room = "gstair2";
	travel.visit(a, 10); travel.passage(a, b, 20); travel.passage(b, c, 30);
	if (!travel.valid() || travel.visits.size() != 3 || travel.route(1, 3).size() != 2 || !travel.route(3, 1).empty()) return false;
	travel.passage(a, b, 40); if (travel.passages.size() != 2 || travel.visits.size() != 3) return false;
	travel.meet(a, "CD1:PENNY1.PUP", "penny1", 50); travel.meet(c, "CD1:PENNY2.PUP", "penny2", 60);
	if (!travel.valid() || travel.encounters.size() != 1 || travel.encounters[0].visit != 3) return false;
	TravelHistory earlier; earlier.visit(a, 10); if (earlier.find(c) || !earlier.route(1, 3).empty()) return false;
	travel.passages[0].to = 99; if (travel.valid()) return false;
	travel.passages[0].to = 2; travel.visits[1].position.x = 512; return !travel.valid();
}
}
