/* SPDX-License-Identifier: GPL-3.0-or-later
 * Format offsets follow dreamREfactory/df/stg.ts (GPL-3.0).
 */
#include "dreamfactory/stage.h"
namespace DreamFactory {
bool Stage::regions(uint32 container, Common::Array<Region> &out) {
	out.clear();
	if (!container) return true;
	if (!archive.has(container)) return false;
	const Bytes &data = archive.get(container);
	const uint32 at = version == 1 ? 0 : 1028;
	if (data.size() < at + 4) return false;
	Reader r(data, at);
	const uint32 count = r.u32();
	if (count > (data.size() - at - 4) / 32) return false;
	for (uint32 i = 0; i < count; ++i) {
		const uint32 pos = at + 4 + i * 32;
		r.seek(pos + 4);
		Region region;
		region.rect.top = int16(r.u16()); region.rect.left = int16(r.u16());
		region.rect.bottom = int16(r.u16()); region.rect.right = int16(r.u16());
		region.script = r.u32(); region.name = r.name(pos + 16);
		if (region.script && !archive.has(region.script)) return false;
		out.push_back(region);
	}
	return r.good();
}
bool Stage::load(const Bytes &data) {
	flats.clear(); name.clear();
	if (!archive.load(data)) return false;
	const Bytes &header = archive.get(0);
	Reader r(header, 2); version = r.u32();
	if (version != 1 && version != 4) return false;
	const bool v1 = version == 1;
	const uint32 paletteAt = v1 ? 36 : 56;
	const uint32 flatCountAt = v1 ? 2100 : 2120;
	const uint32 stride = v1 ? 28 : 46;
	if (header.size() < flatCountAt + 4) return false;
	r.seek(v1 ? 28 : 40); width = r.u16(); height = r.u16(); mainScript = r.u32();
	if (!width || !height || width > 640 || height > 480 || !archive.has(mainScript)) return false;
	name = r.name(v1 ? 2084 : 2104);
	for (uint32 i = 0; i < 256; ++i)
		for (uint32 c = 0; c < 3; ++c) palette[i * 3 + c] = header[paletteAt + i * 8 + 3 + c * 2];
	palette[0] = palette[1] = palette[2] = 0;
	palette[765] = palette[766] = palette[767] = 255;
	r.seek(flatCountAt); const uint32 count = r.u32();
	if (count > (header.size() - flatCountAt - 4) / stride) return false;
	for (uint32 i = 0; i < count; ++i) {
		const uint32 pos = flatCountAt + 4 + i * stride;
		Flat flat;
		r.seek(pos);
		if (!v1) { flat.condition = int32(r.u32()); r.u16(); }
		flat.script = r.u32(); flat.image = r.u32(); flat.clickLogic = r.u32();
		if (v1) { flat.width = width; flat.height = height; }
		else { r.seek(pos + 22); flat.height = r.u16(); flat.width = r.u16(); }
		flat.name = r.name(pos + (v1 ? 12 : 30));
		if ((flat.script && !archive.has(flat.script)) || (flat.image && !archive.has(flat.image)) || !regions(flat.clickLogic, flat.regions)) return false;
		flats.push_back(flat);
	}
	return r.good() && !flats.empty();
}
int32 Stage::findFlat(const Common::String &name) const {
	for (uint32 i = 0; i < flats.size(); ++i) if (flats[i].name.equalsIgnoreCase(name)) return int32(i);
	return -1;
}
int32 Stage::hitTest(uint32 flat, int16 x, int16 y) const {
	if (flat >= flats.size()) return -1;
	const Common::Array<Region> &regions = flats[flat].regions;
	// Authored button rectangles include their right and bottom edges.
	// The later authored region wins when button rectangles overlap.
	for (uint32 n = regions.size(); n > 0; --n) {
		const uint32 i = n - 1;
		const Common::Rect &rect = regions[i].rect;
		if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) return int32(i);
	}
	return -1;
}
}
