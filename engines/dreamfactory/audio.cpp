/* SPDX-License-Identifier: GPL-3.0-or-later
 * V40/V41 codecs and bank layouts adapted from dreamREfactory/df/audio.ts,
 * banks.ts and snd.ts. Output is signed 16-bit mono PCM for ScummVM's mixer.
 */
#include "dreamfactory/audio.h"
#include <math.h>
namespace DreamFactory {
bool decodeAudio(const Bytes &data, AudioClip &clip) {
	if (data.size() < 48) return false;
	Reader r(data); if (r.u32() != 0x10000) return false;
	r.seek(26); const uint32 codec = r.u16(), rate = r.u32();
	r.seek(36); const uint32 size = r.u32(); r.seek(44); const uint32 start = r.u32();
	if ((codec != 1 && codec != 2) || rate < 1000 || rate > 192000 || size > 64 * 1024 * 1024 || start < 48 || start > data.size() || (codec == 2 && size % 2)) return false;
	AudioClip decoded; decoded.rate = rate; decoded.samples.resize(codec == 1 ? size : size / 2);
	r.seek(start);
	if (codec == 2) {
		if (decoded.samples.size() > data.size() - start) return false;
		int16 current = 0;
		for (uint32 i = 0; i < decoded.samples.size(); ++i) {
			const byte b = r.u8(); const int32 n = (b & 0x7f) < 64 ? (b & 0x7f) : (b & 0x7f) - 128;
			current = b & 128 ? int16(n * 512) : int16(current + n * 32);
			decoded.samples[i] = current;
		}
	} else if (size) {
		int8 previous = int8(r.u8()); uint32 out = 0;
		auto emit = [&](int32 sample) { if (out < size) decoded.samples[out] = int16((int32(byte(sample + 64)) - 128) * 256); ++out; };
		emit(previous);
		while (out < size && r.good()) {
			const byte code = r.u8();
			if (!(code & 128)) { previous = int8(code); emit(previous); }
			else if (!(code & 64)) {
				for (uint32 p = 0; p <= (code & 63); ++p) {
					const byte b = r.u8();
					const int32 hi = b >> 4, lo = b & 15;
					previous = int8(previous + (hi < 8 ? hi : hi - 16)); emit(previous);
					previous = int8(previous + (lo < 8 ? lo : lo - 16)); emit(previous);
				}
			} else for (uint32 n = 0; n <= (code & 63); ++n) emit(previous);
		}
	}
	if (!r.good()) return false;
	clip = decoded; return true;
}
void AudioBank::addSound(Common::String key, uint32 container, uint32 id, bool firstMatch) {
	const uint32 slash = key.findLastOf('/');
	if (slash != Common::String::npos) key = key.substr(slash + 1);
	key.toLowercase();
	if (firstMatch || !singles.contains(key)) soundNames.push_back(key);
	if (firstMatch && singles.contains(key)) return;
	singles[key] = container;
	soundIDs[key] = id;
}
static Common::String bankName(Reader &r, const Bytes &data, uint32 at, uint32 maximum) {
	if (at >= data.size()) { r.seek(data.size() + 1); return Common::String(); }
	return r.name(at, MIN<uint32>(maximum + 1, data.size() - at));
}
bool AudioBank::load(const Bytes &data) {
	loops.clear(); singles.clear(); soundIDs.clear(); soundNames.clear(); legacyMusic.clear(); name.clear(); loopRepeat = themeRecords = 0;
	if (!archive.load(data)) return false;
	const Bytes &head = archive.get(0); Reader h(head, 2); version = h.u32();
	if (version == 1) {
		if (head.size() < 186 || !archive.size()) return false;
		name = bankName(h, head, 158, 31); h.seek(24); const uint32 oneShots = h.u16(), loopCount = h.u16();
		themeRecords = loopCount;
		const uint32 count = archive.size() - 1;
		// The final v1 record omits eight bytes of trailing padding. Only its
		// bounded Pascal string must be present; the preceding stride stays 24.
		if (oneShots + loopCount != count || (count && uint64(186) + uint64(count - 1) * 24 >= head.size())) return false;
		for (uint32 i = 0; i < count; ++i) {
			Common::String id = bankName(h, head, 186 + i * 24, 23);
			if (!h.good() || id.empty() || !archive.has(i + 1)) return false;
			if (i < oneShots) addSound(id, i + 1, i + 1, true);
			else { const uint32 slash = id.findLastOf('/'); if (slash != Common::String::npos) id = id.substr(slash + 1); id.toLowercase(); legacyMusic[id] = i + 1; }
		}
		// DF.EXE 403308 follows the explicit order at 30, then links the
		// final copied descriptor back to the first. Record count is not order.
		h.seek(28); const uint32 length = h.u16(); if (length > 64) return false;
		for (uint32 i = 0; i < length; ++i) { const uint32 index = h.u16(); if (index && index <= loopCount) loops.push_back(oneShots + index); }
		return h.good();
	}
	if (version != 4 || head.size() < 37) return false;
	h.seek(28); const uint32 loopTable = h.u32(), singleTable = h.u32();
	name = bankName(h, head, 36, 31);
	if (name.hasSuffixIgnoreCase(".wav")) name = name.substr(0, name.size() - 4);
	if (!h.good()) return false;
	if (loopTable && archive.has(loopTable) && archive.get(loopTable).size() >= 270) {
		const Bytes &table = archive.get(loopTable); Reader r(table, 4); const uint32 length = r.u16();
		if (length > 130) return false;
		Common::Array<uint32> order;
		for (uint32 i = 0; i < length; ++i) order.push_back(r.u16());
		r.seek(266); const uint32 count = r.u32();
		themeRecords = count;
		if (count > (table.size() - 270) / 26) return false;
		Common::Array<uint32> records;
		for (uint32 i = 0; i < count; ++i) { r.seek(270 + i * 26 + 4); const uint32 loc = r.u32(); if (!archive.has(loc)) return false; records.push_back(loc); }
		for (uint32 i = 0; i < order.size(); ++i) if (order[i] && order[i] <= records.size()) loops.push_back(records[order[i] - 1]);
		Reader root(table); const int32 authored = int32(root.u32());
		if (!loops.empty()) loopRepeat = uint32(CLIP<int32>(authored, 0, MIN<uint32>(count - 1, loops.size() - 1)));
		if (!r.good()) return false;
	}
	if (singleTable) {
		const Bytes &table = archive.get(singleTable); if (table.size() < 8) return false;
		Reader r(table, 4); const uint32 count = r.u32();
		if (count > (table.size() - 8) / 26) return false;
		for (uint32 i = 0; i < count; ++i) {
			r.seek(8 + i * 26 + 4); const uint32 loc = r.u32(); Common::String id = r.name(8 + i * 26 + 10);
			if (!r.good() || !archive.has(loc)) return false; addSound(id, loc, i + 1);
		}
	}
	return true;
}
bool AudioBank::theme(AudioClip &out) const {
	return joinAudio(archive, loops, out);
}
bool joinAudio(const Archive &archive, const Common::Array<uint32> &loops, AudioClip &out) {
	if (loops.empty()) return false;
	Common::Array<AudioClip> parts; uint32 rate = 0; uint64 sourceCount = 0;
	for (uint32 i = 0; i < loops.size(); ++i) {
		AudioClip part; if (!decodeAudio(archive.get(loops[i]), part)) return false;
		// Resampling to the highest input rate cannot shorten any part. Bound
		// the retained source PCM before collecting the entire loop sequence.
		sourceCount += part.samples.size();
		if (sourceCount > 64 * 1024 * 1024) return false;
		rate = MAX(rate, part.rate); parts.push_back(part);
	}
	AudioClip joined; joined.rate = rate; uint64 count = 0;
	for (uint32 i = 0; i < parts.size(); ++i) count += (uint64(parts[i].samples.size()) * rate + parts[i].rate / 2) / parts[i].rate;
	if (count > 64 * 1024 * 1024) return false;
	joined.samples.resize(uint32(count)); uint32 at = 0;
	for (uint32 p = 0; p < parts.size(); ++p) {
		const AudioClip &part = parts[p];
		const uint32 length = uint32((uint64(part.samples.size()) * rate + part.rate / 2) / part.rate);
		for (uint32 n = 0; n < length; ++n) {
			if (rate == part.rate) { joined.samples[at++] = part.samples[n]; continue; }
			const double source = double(n) / (double(rate) / part.rate);
			const uint32 a = uint32(source), b = MIN<uint32>(a + 1, part.samples.size() - 1);
			const double fraction = source - a;
			// Keep the reference's float32 intermediate before quantizing to the
			// mixer's signed PCM. Mixed-rate radio/news chunks must keep their pace.
			const float sample = float((part.samples[a] / 32768.0) * (1 - fraction) + (part.samples[b] / 32768.0) * fraction);
			joined.samples[at++] = int16(floor(double(sample) * 32768.0 + 0.5));
		}
	}
	out = joined; return true;
}
}
