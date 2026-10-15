/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cast layout and transparent sprite codec adapted from dreamREfactory's
 * df/cst.ts and df/shp.ts. DreamFactory 1 and 4 share these layouts.
 */
#include "dreamfactory/cast.h"
namespace DreamFactory {
bool decodeSprite(const Bytes &data, Sprite &sprite) {
	if (data.size() < 8) return false;
	Reader r(data); Sprite decoded;
	const int16 height = int16(r.u16()), width = int16(r.u16());
	decoded.y = int16(r.u16()); decoded.x = int16(r.u16());
	if (width < 0 || height < 0 || width > 4096 || height > 4096) return false;
	decoded.width = width; decoded.height = height;
	decoded.pixels.resize(uint32(width) * height); decoded.opaque.resize(uint32(width) * height);
	if (!decoded.pixels.empty()) { memset(decoded.pixels.data(), 0, decoded.pixels.size()); memset(decoded.opaque.data(), 0, decoded.opaque.size()); }
	for (uint32 y = 0; y < uint32(height); ++y) {
		const int16 length = int16(r.u16());
		if (!r.good() || length < 0 || uint32(length) > data.size() - r.pos()) return false;
		const uint32 end = r.pos() + length; uint32 x = 0;
		while (r.pos() < end) {
			const byte flag = r.u8(); const uint32 count = flag >> 2, at = y * width + x;
			if (count > uint32(width) - x) return false;
			switch (flag & 3) {
			case 0:
				if (count && !y) return false;
				for (uint32 i = 0; i < count; ++i) { decoded.pixels[at + i] = decoded.pixels[at + i - width]; decoded.opaque[at + i] = decoded.opaque[at + i - width]; }
				break;
			case 1: break;
			case 2: {
				if (r.pos() == end) return false;
				const byte color = r.u8();
				for (uint32 i = 0; i < count; ++i) { decoded.pixels[at + i] = color; decoded.opaque[at + i] = 1; }
				break;
			}
			case 3:
				if (count > end - r.pos()) return false;
				for (uint32 i = 0; i < count; ++i) { decoded.pixels[at + i] = r.u8(); decoded.opaque[at + i] = 1; }
				break;
			}
			x += count;
		}
		if (x != uint32(width)) return false;
	}
	if (!r.good()) return false;
	sprite = decoded; return true;
}
bool Cast::load(const Bytes &data) {
	members.clear(); mainScript = 0;
	if (!archive.load(data)) return false;
	const Bytes &head = archive.get(0); Reader h(head, 2); version = h.u32();
	if ((version != 1 && version != 4) || head.size() < 2364) return false;
	h.seek(2340); mainScript = h.u32();
	if (mainScript && !archive.has(mainScript)) return false;
	for (uint32 i = 0; i < 256; ++i) for (uint32 c = 0; c < 3; ++c) palette[i * 3 + c] = head[36 + i * 8 + 3 + c * 2];
	h.seek(2360); const uint32 count = h.u32();
	if (count > (head.size() - 2364) / 16) return false;
	for (uint32 m = 0; m < count; ++m) {
		h.seek(2364 + m * 16); CastMember member; member.logic = h.u32();
		if (!archive.has(member.logic)) return false;
		const Bytes &logic = archive.get(member.logic); Reader r(logic, 38);
		if (logic.size() < 94) return false;
		member.script = r.u32(); member.name = r.name(42, 48); member.name.toLowercase();
		if (member.script && !archive.has(member.script)) return false;
		r.seek(90); const uint32 poses = r.u32();
		if (poses > (logic.size() - 94) / 32) return false;
		for (uint32 p = 0; p < poses; ++p) {
			CastPose pose; pose.record = 94 + p * 32;
			r.seek(pose.record); pose.location = r.u32(); pose.name = r.name(pose.record + 16); pose.name.toLowercase();
			const Bytes &set = archive.get(pose.location); Reader s(set, 114);
			if (set.size() < 118) return false;
			pose.frameCount = s.u32();
			if (pose.frameCount > (set.size() - 118) / 44) return false;
			for (uint32 f = 0; f < pose.frameCount; ++f) {
				CastFrame frame; frame.record = 118 + f * 44;
				s.seek(frame.record); frame.location = s.u32(); s.seek(frame.record + 8);
				const int16 step = int16(s.u16()); frame.direction = int16(s.u16());
				s.seek(frame.record + 40); frame.angle = int16(s.u16()); frame.refScale = int16(s.u16());
				if (step < 0) continue;
				if (!archive.has(frame.location)) return false;
				if (pose.steps.size() <= uint32(step)) pose.steps.resize(uint32(step) + 1);
				pose.steps[step].push_back(frame);
			}
			s.seek(112); const int32 length = CLIP<int32>(int16(s.u16()), 0, 33); bool usable = length > 0;
			s.seek(46);
			for (int32 i = 0; i < length; ++i) { const int32 step = int16(s.u16()) - 1; if (step < 0 || uint32(step) >= pose.steps.size()) usable = false; pose.play.push_back(uint32(step)); }
			if (!usable) { pose.play.clear(); for (uint32 i = 0; i < pose.steps.size(); ++i) pose.play.push_back(i); }
			if (!s.good()) return false;
			member.poses.push_back(pose);
		}
		if (!r.good()) return false;
		members.push_back(member);
	}
	return h.good();
}
}
