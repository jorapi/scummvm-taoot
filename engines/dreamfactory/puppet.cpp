/* SPDX-License-Identifier: GPL-3.0-or-later
 * PUP layouts and animation records follow dreamREfactory/df/pup.ts.
 */
#include "dreamfactory/puppet.h"
namespace DreamFactory {
bool Puppet::stance(uint32 location) {
	const Bytes &bytes = archive.get(location); if (bytes.size() < 22 + 11 * 262) return false;
	PuppetStance value; value.location = location; Reader r(bytes);
	for (uint32 l = 0; l < 11; ++l) {
		r.seek(22 + l * 262); const int16 count = int16(r.u16()); value.layers[l].y = int16(r.u16()); value.layers[l].x = int16(r.u16());
		if (count < 0 || count > 32) return false;
		for (int32 f = 0; f < count; ++f) { const uint32 ref = r.u32(); if (ref >= archive.size()) return false; value.layers[l].frames.push_back(ref); }
	}
	if (!r.good()) return false; stances.push_back(value); return true;
}
bool Puppet::load(const Bytes &bytes) {
	lines.clear(); scripts.clear(); stances.clear(); name.clear(); version = band = 0;
	if (!archive.load(bytes) || archive.size() < 4) return false;
	const Bytes &head = archive.get(0); if (head.size() < 2160) return false;
	Reader r(head, 2); version = r.u32(); if (version != 1 && version != 4) return false;
	for (uint32 i = 0; i < 256; ++i) for (uint32 c = 0; c < 3; ++c) palette[i * 3 + c] = head[58 + i * 8 + 3 + c * 2];
	for (uint32 i = 0; i < 4; ++i) { r.seek(0x83a + i * 4); idleMin[i] = int32(r.u32()); r.seek(0x84a + i * 4); idleMax[i] = int32(r.u32()); }
	r.seek(0x85a); band = r.u32(); if (band >= archive.size()) return false;
	name = r.name(0x85e, 16); r.seek(2158); const int16 count = int16(r.u16());
	if (count < 0 || uint32(count) > (head.size() - 2160) / 312) return false;
	for (int32 i = 0; i < count; ++i) {
		PuppetLine value; value.record = 2160 + i * 312; r.seek(value.record); value.stance = int16(r.u16());
		r.seek(value.record + 8); value.audio = r.u32(); value.animation = r.u32();
		if (value.audio >= archive.size() || value.animation >= archive.size()) return false;
		value.text = r.name(value.record + 24, 256); value.ident = r.name(value.record + 280, 32);
		const int32 existing = line(value.ident); if (existing >= 0) lines[existing] = value; else lines.push_back(value);
	}
	const Bytes &directory = archive.get(2); if (directory.size() < 24) return false;
	Reader scriptsReader(directory, 22); const int16 scriptCount = int16(scriptsReader.u16());
	if (scriptCount < 0 || uint32(scriptCount) > (directory.size() - 24) / 40) return false;
	for (int32 i = 0; i < scriptCount; ++i) {
		PuppetScript value; scriptsReader.seek(24 + i * 40); value.location = scriptsReader.u32(); value.name = scriptsReader.name(32 + i * 40, 32); value.name.toLowercase();
		if (value.location >= archive.size()) return false; scripts.push_back(value);
	}
	if (!r.good() || !scriptsReader.good()) return false;
	if (version == 1) { if (!stance(3)) return false; }
	else {
		const Bytes &directory = archive.get(3); if (directory.size() < 22 + 64 * 4) return false; Reader sr(directory, 22);
		// The first non-reference terminates the used directory. Unused slots
		// contain authoring scratch, including large positive words in DPENNY.
		for (uint32 i = 0; i < 64; ++i) { const int32 location = int32(sr.u32()); if (location <= 0 || uint32(location) >= archive.size()) break; if (!stance(uint32(location))) return false; }
		if (!sr.good()) return false;
	}
	for (uint32 i = 0; i < lines.size(); ++i) if (lines[i].stance < 0 || uint32(lines[i].stance) >= stances.size()) return false;
	return !stances.empty();
}
bool Puppet::animation(uint32 location, Common::Array<PuppetPose> &out) const {
	Common::Array<PuppetPose> poses;
	if (!location || !archive.has(location)) { out = poses; return location < archive.size(); }
	const Bytes &bytes = archive.get(location); if (bytes.size() % 82) return false; Reader r(bytes);
	for (uint32 i = 0; i < bytes.size() / 82; ++i) {
		PuppetPose pose; r.seek(i * 82 + 16);
		for (uint32 l = 0; l < 11; ++l) { pose.layers[l].frame = int16(r.u16()); pose.layers[l].y = int16(r.u16()); pose.layers[l].x = int16(r.u16()); }
		poses.push_back(pose);
	}
	if (!r.good()) return false; out = poses; return true;
}
int32 Puppet::line(const Common::String &name, bool textFallback) const {
	for (uint32 i = 0; i < lines.size(); ++i) if (lines[i].ident.equalsIgnoreCase(name)) return int32(i);
	return textFallback ? textLine(name) : -1;
}
int32 Puppet::textLine(const Common::String &name) const {
	Common::String asked = name; asked.trim();
	for (uint32 i = 0; i < lines.size(); ++i) {
		Common::String text = lines[i].text; text.trim();
		if (!text.empty() && text.equalsIgnoreCase(asked)) return int32(i);
	}
	return -1;
}
}
