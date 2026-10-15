/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/room.h"
#include "common/endian.h"
namespace DreamFactory {
bool Room::roadSequence(const Road &road, uint32 direction, Sequence &frames, int32 &scene, int32 &view) const {
	frames.clear();
	if (!roadArrival(road, direction, scene, view)) return false;
	CameraPose departure, arrival; Frame image;
	const int32 endpoint = direction ? road.to : road.from;
	bool found = false;
	for (uint32 s = 0; s < scenes.size() && !found; ++s)
		for (uint32 v = 0; v < scenes[s].views.size(); ++v) if (scenes[s].views[v].id == endpoint) {
			if (!stand(s, v, image, &departure)) return false;
			found = true; break;
		}
	if (!found || !stand(scene, view, image, &arrival)) return false;
	frames = road.walks[direction];
	// Some v4 endpoint records retain another deck's eye height (notably
	// GSTAIR2 Road58). TI.EXE consumes these bookkeeping rows off screen;
	// presenting their stale camera projects upstairs NPCs through the scenery.
	// Match the adjacent standpoints, without changing actual stair motion.
	frames.front().camera.z = departure.z;
	frames.back().camera.z = arrival.z;
	return true;
}
bool Room::roadArrival(const Road &road, uint32 direction, int32 &scene, int32 &view) const {
	if (direction > 1 || road.walks[direction].empty()) return false;
	scene = view = -1;
	if (road.destinations[direction])
		for (uint32 s = 0; s < scenes.size(); ++s)
			if (scenes[s].viewTable == road.destinations[direction]) { scene = s; break; }
	if (scene < 0) {
		const int32 endpoint = direction ? road.from : road.to;
		for (uint32 s = 0; s < scenes.size(); ++s)
			for (uint32 v = 0; v < scenes[s].views.size(); ++v)
				if (scenes[s].views[v].id == endpoint) scene = s;
	}
	if (scene < 0 || scenes[scene].views.empty()) return false;
	// The far endpoint faces BACK along the road. Keep the direction of travel
	// from the final motion frame, using the engine's 8-bit circular angles.
	const int32 facing = road.walks[direction].back().camera.degree & 255;
	int32 best = 257;
	for (uint32 v = 0; v < scenes[scene].views.size(); ++v) {
		const int32 delta = (scenes[scene].views[v].facing - facing) & 255;
		const int32 distance = MIN<int32>(delta, 256 - delta);
		if (distance < best) { best = distance; view = v; }
	}
	return view >= 0;
}
bool Room::stand(uint32 scene, uint32 view, Frame &out, CameraPose *camera) const {
	if (scene >= scenes.size() || view >= scenes[scene].views.size()) return false;
	const View &v = scenes[scene].views[view]; Frame candidate;
	CameraPose pose; pose.x = scenes[scene].x; pose.y = scenes[scene].y;
	pose.z = version == 1 ? eyeHeight : int32(floor(v.cameraHeight * 512 + 0.5)); pose.degree = v.facing;
	if (version == 1) {
		if (v.still) { if (!decodeFrame(archive.get(v.still), candidate)) return false; out = candidate; if (camera) *camera = pose; return true; }
		for (uint32 m = 0; m < moves.size(); ++m) if (moves[m].to == v.id && !moves[m].frames.empty()) {
			for (uint32 f = 0; f < moves[m].frames.size(); ++f) if (!decodeFrame(archive.get(moves[m].frames[f].image), candidate)) return false;
			out = candidate; if (camera) *camera = pose; return true;
		}
		return false;
	}
	// Standpoints can be deltas too. Reconstruct from the start of the authored
	// register, preferring the high-resolution left-turn pictures.
	bool emptyStandpoint = false;
	for (int32 dir = 1; dir >= 0; --dir) {
		candidate = Frame(); const Sequence &ring = scenes[scene].turns[dir];
		for (uint32 i = 0; i < ring.size(); ++i) {
			if (!ring[i].image) { if (ring[i].view == int32(view) && ring[i].motion > 0) emptyStandpoint = true; continue; }
			if (!decodeFrame(archive.get(ring[i].image), candidate)) return false;
			if (ring[i].view == int32(view) && ring[i].motion > 0) { pose = ring[i].camera; pose.degree = v.facing; out = candidate; if (camera) *camera = pose; return true; }
		}
	}
	if (emptyStandpoint) { out = Frame(); if (camera) *camera = pose; return true; }
	return false;
}
bool Room::sequence(uint32 index, Sequence &out) {
	out.clear();
	const Bytes &d = archive.get(index);
	if (d.size() < 12) return false;
	Reader r(d, 4); uint32 count = r.u32();
	if (count > (d.size() - 12) / 60) return false;
	for (uint32 i = 0; i < count; ++i) {
		r.seek(12 + i * 60 + 32);
		MotionFrame f; f.camera.x = int16(r.u16()); f.camera.y = int16(r.u16());
		f.camera.z = int16(r.u16()); f.camera.degree = int16(r.u16());
		f.motion = int32(r.u32()); f.image = r.u32();
		r.seek(12 + i * 60 + 56); f.view = int32(r.u32());
		if (!archive.has(f.image)) return false;
		out.push_back(f);
	}
	return r.good();
}
bool Room::load(const Bytes &data) {
	scenes.clear(); roads.clear(); moves.clear();
	depthLevels = depthFar = cameraSetback = eyeHeight = 0;
	if (!archive.load(data)) return false;
	const Bytes &head = archive.get(0);
	Reader h(head, 2); version = h.u32();
	if (!stars.load(archive, version)) return false;
	if (version == 1) return loadV1();
	if (version != 4 || head.size() < 0xa2e) return false;
	h.seek(0x84); width = h.u16(); height = h.u16();
	if (!width || !height || width > 640 || height > 400) return false;
	h.seek(0x9fa); depthLevels = int16(h.u16());
	h.seek(0xa08); depthFar = int16(h.u16());
	h.seek(0x54); uint32 transitions = h.u32();
	h.seek(0x5c); mainScript = h.u32(); uint32 sceneTable = h.u32(), count = h.u32();
	if (mainScript >= archive.size()) return false;
	name = h.name(0x70); defaultScene = h.name(0xa0e); defaultView = h.name(0xa1e);
	for (uint32 i = 0; i < 256; ++i)
		for (uint32 rgb = 0; rgb < 3; ++rgb) palette[i * 3 + rgb] = head[0xf2 + i * 8 + 3 + rgb * 2];
	palette[0] = palette[1] = palette[2] = 0;
	palette[765] = palette[766] = palette[767] = 255;
	const Bytes &table = archive.get(sceneTable);
	if (!h.good() || count > table.size() / 42) return false;
	Reader s(table);
	for (uint32 i = 0; i < count; ++i) {
		Scene scene; s.seek(i * 42 + 4); scene.x = int16(s.u16()); scene.y = int16(s.u16()); scene.z = int16(s.u16());
		uint32 views = s.u32(), right = s.u32(), left = s.u32();
		scene.viewTable = views;
		scene.script = s.u32(); scene.name = s.name(i * 42 + 26);
		if (scene.script >= archive.size()) return false;
		const Bytes &vd = archive.get(views);
		if (vd.size() < 52) return false;
		Reader v(vd, 48); uint32 viewCount = v.u32();
		if (viewCount > (vd.size() - 52) / 46) return false;
		for (uint32 j = 0; j < viewCount; ++j) {
			View view; v.seek(52 + j * 46 + 8); view.facing = int16(v.u16());
			view.cameraHeight = READ_BE_FLOAT64(vd.data() + 52 + j * 46 + 14);
			v.seek(52 + j * 46 + 22); view.id = int32(v.u32()); uint32 objects = v.u32(); view.name = v.name(52 + j * 46 + 30);
			if (objects) {
				const Bytes &od = archive.get(objects); Reader o(od); const uint32 n = o.u32();
				if (od.size() < 8 || n > (od.size() - 8) / 36) return false;
				for (uint32 p = 0; p < n; ++p) {
					Painting paint; o.seek(8 + p * 36 + 4); paint.facing = int16(o.u16()); o.u16();
					paint.rect.top = int16(o.u16()); paint.rect.left = int16(o.u16()); paint.rect.bottom = int16(o.u16()); paint.rect.right = int16(o.u16());
					paint.script = o.u32(); paint.name = o.name(8 + p * 36 + 20);
					if (!o.good() || paint.script >= archive.size()) return false;
					view.paintings.push_back(paint);
				}
			}
			scene.views.push_back(view);
		}
		if (!v.good() || !sequence(right, scene.turns[0]) || !sequence(left, scene.turns[1])) return false;
		for (int dir = 0; dir < 2; ++dir)
			for (uint32 j = 0; j < scene.turns[dir].size(); ++j)
				if (scene.turns[dir][j].view < -1 || scene.turns[dir][j].view >= int32(viewCount)) return false;
		scenes.push_back(scene);
	}
	const Bytes &td = archive.get(transitions);
	if (td.size() < 8) return false;
	Reader t(td, 4); uint32 roadCount = t.u32();
	if (roadCount > (td.size() - 8) / 16) return false;
	for (uint32 i = 0; i < roadCount; ++i) {
		t.seek(8 + i * 16); uint32 info = t.u32(), a = t.u32(), b = t.u32();
		Reader r(archive.get(info), 6); Road road; road.from = int32(r.u32()); road.to = int32(r.u32());
		if (!r.good() || !sequence(a, road.walks[0]) || !sequence(b, road.walks[1])) return false;
		road.destinations[0] = READ_LE_UINT32(archive.get(a).data() + 8);
		road.destinations[1] = READ_LE_UINT32(archive.get(b).data() + 8);
		roads.push_back(road);
	}
	return s.good() && t.good() && !scenes.empty();
}
}
