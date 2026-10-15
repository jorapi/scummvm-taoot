/* SPDX-License-Identifier: GPL-3.0-or-later
 * Dust's grid transitions are deliberately read separately from Titanic SETs.
 */
#include "dreamfactory/room.h"
namespace DreamFactory {
bool Room::loadV1() {
	const Bytes &head = archive.get(0);
	if (head.size() < 0x1b90) return false;
	Reader h(head, 8); uint32 count = h.u32();
	h.seek(0x18); cameraSetback = int16(h.u16()); eyeHeight = int16(h.u16());
	depthLevels = 24; depthFar = 24 * 64;
	h.seek(0x1e); uint32 reg = h.u16();
	h.seek(0x26); uint16 gridW = h.u16(), gridH = h.u16();
	width = h.u16(); height = h.u16();
	if (!gridW || !gridH || gridW > 64 || gridH > 64 || !width || !height || width > 640 || height > 400) return false;
	h.seek(0x30); uint32 dx = h.u16(), dz = h.u16(), df = h.u16();
	const char *compass[] = { "north", "south", "east", "west" };
	const int heading[] = { 192, 64, 0, 128 };
	defaultScene = Common::String::format("Scene %c%u", 'A' + dx, dz + 1);
	if (df < 1 || df > 4) return false;
	defaultView = compass[df - 1]; name = h.name(0x1b7c, 20);
	h.seek(0x1b78); mainScript = h.u32(); if (!h.good() || mainScript >= archive.size()) return false;
	// Palette labels have variable lengths. The complete grid table is the
	// named run at the tail of C0, not a fixed offset past the third palette.
	const uint32 cells = uint32(gridW) * gridH; int32 first = -1;
	for (uint32 tail = 0; tail <= 64; ++tail) {
		const int64 at = int64(head.size()) - tail - int64(cells - 1) * 32 - 16;
		if (at < 16) break;
		bool valid = true;
		for (uint32 i = 0; i < cells && valid; ++i) {
			Reader n(head); const Common::String label = n.name(uint32(at) + i * 32);
			valid = n.good() && !label.empty();
			for (uint32 j = 0; j < label.size(); ++j) if (label[j] < 32 || label[j] > 126) valid = false;
		}
		if (valid) { first = int32(at); break; }
	}
	if (first < 0) return false;
	for (uint32 i = 0; i < cells; ++i) {
		const uint32 at = uint32(first) + i * 32; Reader r(head, at - 12); Scene scene;
		const uint32 row = r.u16(), col = r.u16(); scene.x = int32(col * 256 + 128); scene.y = int32(row * 256 + 128);
		r.seek(at - 4); scene.build = r.u16() != 0; scene.name = r.name(at);
		r.seek(at + 16); scene.script = r.u32();
		if (!r.good() || col >= gridW || row >= gridH || scene.script >= archive.size()) return false;
		scenes.push_back(scene);
	}
	for (uint32 i = 0; i < 256; ++i)
		for (uint32 rgb = 0; rgb < 3; ++rgb) palette[i * 3 + rgb] = head[0x50 + i * 8 + 3 + rgb * 2];
	palette[0] = palette[1] = palette[2] = 0;
	palette[765] = palette[766] = palette[767] = 255;
	const Bytes &td = archive.get(reg);
	// The complete register is authoritative; some headers undercount it.
	if (td.empty() || td.size() % 28) return false;
	count = td.size() / 28;
	Common::Array<uint32> bases;
	for (uint32 i = 0; i < count; ++i) { Reader r(td, i * 28 + 24); bases.push_back(r.u32()); }
	for (uint32 i = 0; i < count; ++i) {
		Reader r(td, i * 28);
		uint32 x = r.u16(), z = r.u16(), facing = r.u16();
		uint32 tx = r.u16(), tz = r.u16(), tf = r.u16();
		if (x >= gridW || tx >= gridW || z >= gridH || tz >= gridH || facing < 1 || facing > 4 || tf < 1 || tf > 4) return false;
		Move move;
		move.from = (z * gridW + x) * 4 + facing - 1;
		move.to = (tz * gridW + tx) * 4 + tf - 1;
		move.direction = (x != tx || z != tz) ? 2 : (((heading[tf - 1] - heading[facing - 1]) & 255) == 64 ? 0 : 1);
		uint32 base = bases[i], next = archive.size();
		for (uint32 j = 0; j < bases.size(); ++j) if (bases[j] > base && bases[j] < next) next = bases[j];
		if (base >= archive.size()) return false;
		for (uint32 f = base; f < next && f - base < 5 && archive.get(f).size() >= 8; ++f) {
			MotionFrame frame; frame.image = f; frame.view = -1; frame.motion = 0; move.frames.push_back(frame);
		}
		if (move.frames.empty()) return false;
		for (uint32 f = 0; f < move.frames.size(); ++f) {
			CameraPose &pose = move.frames[f].camera;
			// DF.EXE 0x40de12/0x40df6e: frame*256/4 for walking,
			// frame*64/4 for turns. Five pictures include both endpoints.
			const double part = double(f) / 4;
			pose.x = int32(floor((x * 256 + 128) + (int32(tx) - int32(x)) * 256 * part + 0.5));
			pose.y = int32(floor((z * 256 + 128) + (int32(tz) - int32(z)) * 256 * part + 0.5));
			pose.z = eyeHeight;
			int32 swing = ((heading[tf - 1] - heading[facing - 1]) & 255); if (swing > 128) swing -= 256;
			pose.degree = int32(floor(heading[facing - 1] + swing * part + 0.5)) & 255;
		}
		moves.push_back(move);
		uint32 sceneIndex = scenes.size();
		for (uint32 s = 0; s < scenes.size(); ++s) if (scenes[s].x == int32(x * 256 + 128) && scenes[s].y == int32(z * 256 + 128)) { sceneIndex = s; break; }
		if (sceneIndex == scenes.size()) return false;
		Scene &scene = scenes[sceneIndex];
		uint32 viewIndex = scene.views.size();
		for (uint32 v = 0; v < scene.views.size(); ++v) if (scene.views[v].id == move.from) { viewIndex = v; break; }
		if (viewIndex == scene.views.size()) { View view; view.id = move.from; view.name = compass[facing - 1]; view.facing = heading[facing - 1]; view.cameraHeight = double(eyeHeight) / 512; scene.views.push_back(view); }
		uint32 still = base + 5;
		const Bytes &sd = archive.get(still); Reader sr(sd);
		uint16 sh = sr.u16(), sw = sr.u16();
		if (still < next && move.frames.size() == 5 && sh == height && sw == width && sd.size() >= 4u + height)
			scene.views[viewIndex].still = still;
	}
	return h.good() && !scenes.empty();
}
}
