/* SPDX-License-Identifier: GPL-3.0-or-later
 * SHP/PRP layouts and play-list interpretation adapted from df/shp.ts.
 */
#include "dreamfactory/shop.h"
#include "common/hashmap.h"
namespace DreamFactory {
bool Shop::sprite(uint32 location, Sprite &out) const {
	if (location >= archive.size()) return false;
	// A typed archive gap is a non-drawing frame, not malformed compressed
	// pixels. SCORP's desk references such a gap in the supplied Dust data.
	if (archive.get(location).empty()) { out = Sprite(); return true; }
	return decodeSprite(archive.get(location), out);
}
bool Shop::load(const Bytes &bytes) {
	groups.clear(); name.clear(); version = mainScript = 0;
	if (!archive.load(bytes)) return false;
	const Bytes &head = archive.get(0); Reader h(head, 2); version = h.u32();
	if ((version != 1 && version != 4) || head.size() < 2364) return false;
	h.seek(2340); mainScript = h.u32(); name = h.name(2344, 16);
	if (mainScript && !archive.has(mainScript)) return false;
	for (uint32 i = 0; i < 256; ++i) for (uint32 c = 0; c < 3; ++c) palette[i * 3 + c] = head[36 + i * 8 + 3 + c * 2];
	h.seek(2360); const uint32 count = h.u32(); if (count > (head.size() - 2364) / 16) return false;
	for (uint32 g = 0; g < count; ++g) {
		h.seek(2364 + g * 16); PropGroup group; group.location = h.u32();
		const Bytes &logic = archive.get(group.location); Reader r(logic, 38);
		if (logic.size() < 94) return false;
		group.script = r.u32(); group.name = r.name(42, 48);
		if (group.script && !archive.has(group.script)) return false;
		r.seek(90); const uint32 states = r.u32(); if (states > (logic.size() - 94) / 32) return false;
		for (uint32 v = 0; v < states; ++v) {
			PropState state; state.record = 94 + v * 32; r.seek(state.record);
			state.location = r.u32(); state.name = r.name(state.record + 16);
			const Bytes &data = archive.get(state.location); Reader s(data, 114);
			if (data.size() < 118) return false;
			const uint32 pictures = s.u32(); if (pictures > (data.size() - 118) / 44) return false;
			Common::HashMap<int32, bool> degrees; bool duplicate = false;
			for (uint32 p = 0; p < pictures; ++p) {
				PropPicture picture; picture.record = 118 + p * 44; s.seek(picture.record); picture.location = s.u32();
				if (picture.location >= archive.size() || (!archive.get(picture.location).empty() && archive.get(picture.location).size() < 8)) return false;
				s.seek(picture.record + 8); picture.group = int16(s.u16());
				s.seek(picture.record + 40); picture.degree = int16(s.u16()); picture.refScale = int16(s.u16()); if (!picture.refScale) picture.refScale = 96;
				if (degrees.contains(picture.degree)) duplicate = true; degrees[picture.degree] = true;
				state.pictures.push_back(picture);
			}
			s.seek(112); const int32 length = CLIP<int32>(int16(s.u16()), 0, 33); bool usable = length > 1;
			s.seek(46); for (int32 p = 0; p < length; ++p) { const int32 index = int16(s.u16()) - 1; if (index < 0 || uint32(index) >= pictures) usable = false; state.play.push_back(uint32(index)); }
			if (!duplicate && uint32(length) + 1 < pictures) usable = false;
			if (!usable) state.play.clear();
			state.animated = !state.play.empty() || duplicate;
			if (!s.good()) return false; group.states.push_back(state);
		}
		if (!r.good()) return false;
		orient(group); groups.push_back(group);
	}
	return h.good();
}
static bool samePose(const Bytes &a, const Bytes &b) { return a.size() >= 8 && b.size() >= 8 && memcmp(a.data(), b.data(), 8) == 0; }
void Shop::orient(PropGroup &group) {
	// Match the reference's structural repair for a transition whose stored
	// order contradicts its corresponding idle pose (TRUNK's gramdrawer).
	for (uint32 v = 0; v < group.states.size(); ++v) {
		PropState &state = group.states[v]; Common::String name = state.name; name.toLowercase();
		if (!state.animated || state.pictures.size() < 2) continue;
		const bool opening = name.hasPrefix("open"), closing = name.hasPrefix("close");
		if (!opening && !closing) continue;
		const Common::String suffix = name.substr(opening ? 4 : 5); if (suffix.empty()) continue;
		const PropState *idle = nullptr;
		for (uint32 i = 0; i < group.states.size(); ++i) if (group.states[i].pictures.size() == 1 && group.states[i].name.equalsIgnoreCase("idle" + suffix)) idle = &group.states[i];
		if (!idle) continue;
		const uint32 first = state.play.empty() ? 0 : state.play.front(), last = state.play.empty() ? state.pictures.size() - 1 : state.play.back();
		const Bytes &a = archive.get(state.pictures[first].location), &b = archive.get(state.pictures[last].location), &pose = archive.get(idle->pictures[0].location);
		if (samePose(a, b) || !(opening ? samePose(a, pose) : samePose(b, pose))) continue;
		if (!state.play.empty()) for (uint32 i = 0; i < state.play.size() / 2; ++i) SWAP(state.play[i], state.play[state.play.size() - 1 - i]);
		else for (uint32 i = 0; i < state.pictures.size() / 2; ++i) SWAP(state.pictures[i], state.pictures[state.pictures.size() - 1 - i]);
	}
}
}
