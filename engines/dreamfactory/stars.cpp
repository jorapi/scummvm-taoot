/* SPDX-License-Identifier: GPL-3.0-or-later
 * Star registers and route layouts follow dreamREfactory/df/set.ts and
 * df/set-v1.ts. Coordinates here use script axes X, ground Y, height Z.
 */
#include "dreamfactory/stars.h"
namespace DreamFactory {
static Common::String starName(const Bytes &bytes, uint32 at, uint32 limit, bool truncate) {
	if (at >= bytes.size()) return Common::String();
	uint32 count = bytes[at];
	if (truncate && count > limit) return Common::String();
	if (count > bytes.size() - at - 1) return Common::String();
	return Common::String(reinterpret_cast<const char *>(bytes.data() + at + 1), count);
}
static bool validName(const Common::String &name, bool v1) {
	if (name.empty() || (!v1 && name.size() > 20)) return false;
	for (uint32 i = 0; i < name.size(); ++i) {
		const char c = name[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || (!v1 && c == '-'))) return false;
	}
	return true;
}
static Star readStar(const Bytes &bytes, uint32 at, uint32 limit, bool v1) {
	Reader r(bytes, at); Star star;
	star.facing = int16(r.u16()); star.x = int16(r.u16()); star.y = int16(r.u16()); star.z = int16(r.u16());
	star.name = starName(bytes, at + 8, limit, v1); return star;
}
bool StarTable::load(const Archive &archive, uint32 version) {
	this->version = version;
	stars.clear(); paths.clear(); if (version != 1 && version != 4) return false;
	const bool v1 = version == 1;
	Reader h(archive.get(0), v1 ? 0x22 : 0x58);
	const uint32 reg = v1 ? h.u16() : h.u32(); if (!h.good()) return false;
	if (reg >= archive.size()) return false;
	if (!archive.has(reg)) return v1;
	const Bytes &bytes = archive.get(reg); const uint32 countAt = v1 ? 24 : 0, first = v1 ? 28 : 8, stride = v1 ? 50 : 54;
	Reader r(bytes, countAt); const uint32 count = r.u32();
	if (!r.good() || bytes.size() < first || count > (bytes.size() - first) / stride || (v1 && count > 256)) return false;
	for (uint32 i = 0; i < count; ++i) {
		const uint32 base = first + i * stride, primaryAt = base + (v1 ? 0 : 4), secondaryAt = base + (v1 ? 26 : 30);
		const Star primary = readStar(bytes, primaryAt, 17, v1), secondary = readStar(bytes, secondaryAt, 15, v1);
		if (primary.name.empty()) return false;
		stars.push_back(primary);
		const bool validSecondary = validName(secondary.name, v1) && (secondary.x || secondary.y || secondary.z);
		if (validSecondary) stars.push_back(secondary);
		r.seek(base + (v1 ? 24 : 28)); const int16 location = int16(r.u16());
		if (location <= 0 || (v1 ? !validSecondary : !validName(primary.name, false) || !validName(secondary.name, false))) continue;
		if (!archive.has(location)) return false;
		const Bytes &data = archive.get(location); const uint32 pointAt = v1 ? 16 : 20;
		Reader p(data, v1 ? 0 : 8); const uint32 points = p.u32();
		if (!p.good() || data.size() < pointAt || points > (data.size() - pointAt) / 8) return false;
		StarPath path; path.a = primary.name; path.b = secondary.name; path.container = location;
		for (uint32 n = 0; n < points; ++n) {
			p.seek(pointAt + n * 8); StarPoint point;
			point.x = int16(p.u16()); point.y = int16(p.u16()); point.z = int16(p.u16()); point.distance = int16(p.u16());
			if (point.distance < 0) return false;
			path.points.push_back(point);
		}
		paths.push_back(path);
	}
	return true;
}
const Star *StarTable::find(const Common::String &name) const {
	for (uint32 i = 0; i < stars.size(); ++i) if (stars[i].name.equalsIgnoreCase(name)) return &stars[i];
	return nullptr;
}
void StarRegistry::bind(const StarTable &table) {
	current = table;
	for (uint32 i = 0; i < table.stars.size(); ++i) { Common::String key = table.stars[i].name; key.toLowercase(); known[key] = table.stars[i]; }
	for (uint32 i = 0; i < table.paths.size(); ++i) {
		uint32 at = knownPaths.size();
		for (uint32 j = 0; j < knownPaths.size(); ++j) if (knownPaths[j].a.equalsIgnoreCase(table.paths[i].a) && knownPaths[j].b.equalsIgnoreCase(table.paths[i].b)) { at = j; break; }
		if (at == knownPaths.size()) knownPaths.push_back(table.paths[i]); else knownPaths[at] = table.paths[i];
	}
}
const Star *StarRegistry::find(Common::String name) const {
	if (const Star *star = current.find(name)) return star;
	name.toLowercase(); const auto found = known.find(name); return found == known.end() ? nullptr : &found->_value;
}
const StarPath *StarRegistry::path(const Common::String &from, const Common::String &to) const {
	const Common::Array<StarPath> *tables[] = { &current.paths, &knownPaths };
	for (uint32 t = 0; t < (current.version == 1 ? 2u : 1u); ++t)
		for (uint32 i = 0; i < tables[t]->size(); ++i) {
			const StarPath &p = (*tables[t])[i];
			if (p.points.size() < 2) continue;
			if ((from.equalsIgnoreCase("resume") && (p.a.equalsIgnoreCase(to) || p.b.equalsIgnoreCase(to))) ||
				(p.a.equalsIgnoreCase(from) && p.b.equalsIgnoreCase(to)) || (p.b.equalsIgnoreCase(from) && p.a.equalsIgnoreCase(to))) return &p;
		}
	return nullptr;
}
}
