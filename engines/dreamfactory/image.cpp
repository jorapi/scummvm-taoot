/* SPDX-License-Identifier: GPL-3.0-or-later
 * DreamFactory codec, based on the format research in dreamREfactory/DFET.
 * See README.dreamfactory for provenance.
 */
#include "dreamfactory/image.h"

namespace DreamFactory {
bool decodeFrame(const Bytes &data, Frame &frame) {
	Reader r(data);
	uint16 h = r.u16(), w = r.u16();
	if (!w || !h || w > 640 || h > 480) return false;
	Frame next = frame;
	uint32 total = uint32(w) * h;
	if (next.width != w || next.height != h) {
		next.pixels.resize(total);
		memset(next.pixels.data(), 0, total);
	}
	next.width = w; next.height = h;
	Bytes &out = next.pixels;
	uint32 at = 0;
	int32 lookback = 0;
	auto copy = [&](int32 back, uint32 n) -> bool {
		int64 src = int64(at) - back;
		if (src < 0 || src + n > total || n > total - at) return false;
		// Short positive offsets tile like rep movsb, rather than memmove.
		if (back > 0 && uint32(back) < n) {
			for (uint32 i = 0; i < n; ++i) out[at + i] = out[uint32(src) + i];
		} else memmove(out.data() + at, out.data() + uint32(src), n);
		return true;
	};
	for (uint32 y = 0; y < h; ++y) {
		uint32 mode = r.u8() >> 2, written = 0;
		if (mode < 1 || mode > 18) return false;
		if (mode == 1) {
			if (w > total - at) return false;
			for (uint32 x = 0; x < w; ++x) out[at++] = r.u8();
			written = w;
		}
		if (mode <= 5) lookback = w * (6 - int32(mode));
		else if (mode <= 9) lookback = w * (5 - int32(mode));
		else if (mode == 10) { if (w > total - at) return false; at += w; written = w; }
		else {
			lookback = w * ((mode <= 14 ? 15 : 14) - int32(mode));
			if (!copy(lookback, w)) return false;
			at += w; written = w;
		}
		while (written < w && r.good()) {
			byte b = r.u8(); uint32 run = b & 7, n = b >> 3;
			if (!n) n = 32 + r.u8();
			if (n > total - at) return false;
			if (run == 3) { if (!copy(lookback, n)) return false; }
			else if (run == 4) { if (!at) return false; memset(out.data() + at, out[at - 1], n); }
			else if (run == 5) { for (uint32 i = 0; i < n; ++i) out[at + i] = r.u8(); }
			else if (run == 6) { byte value = r.u8(); memset(out.data() + at, value, n); }
			else if (run == 7) { uint32 back = r.u16(); if (!back || !copy(back, n)) return false; }
			else if (run < 2) {
				uint32 i = 0; int32 back = run ? lookback : 1;
				if (!run) out[at + i++] = r.u8();
				uint32 p = r.pos();
				// Sliding window speculatively peeks up to two bytes past the
				// consumed word; padding those peeks does not consume input.
				auto peek = [&](uint32 pos) -> uint32 { return pos < data.size() ? data[pos] : 0; };
				uint32 flags = (peek(p) << 24) | (peek(p + 1) << 16) | (peek(p + 2) << 8) | peek(p + 3);
				p += 2; int bits = 16;
				for (; i < n; ++i) {
					int64 src = int64(at + i) - back;
					if (src < 0 || src >= total) return false;
					int first = 0;
					for (int bit = 15; bit >= 0; --bit) if (flags & (uint32(1) << (bit + 16))) { first = bit; break; }
					int diff, consumed;
					if (first == 15) { diff = 0; consumed = 1; }
					else if (first < 8) { diff = (flags >> 16) & 255; consumed = 16; }
					else { diff = 15 - first; if (!(flags & (uint32(1) << (first + 15)))) diff = -diff; consumed = 17 - first; }
					out[at + i] = byte(out[uint32(src)] + diff);
					bits -= consumed; flags <<= consumed;
					if (bits < 0) {
						flags >>= -bits; p += 2;
						flags |= (peek(p) << 8) | peek(p + 1);
						flags <<= -bits; bits += 16;
					}
				}
				if (bits >= 8) --p;
				if (!r.seek(p)) return false;
			}
			at += n; written += n;
		}
		if (!r.good()) return false;
	}
	if (at != total) return false;
	// Depth is a self-contained row-offset table following the colour stream.
	// It belongs to this frame, even when the colour stream contains deltas.
	next.depth.clear();
	const uint32 table = r.pos();
	if (table < data.size()) {
		if (uint32(h) * 2 > data.size() - table) return false;
		next.depth.resize(total);
		for (uint32 y = 0; y < h; ++y) {
			Reader row(data, table + y * 2);
			const uint32 offset = row.u16();
			if (offset < uint32(h) * 2 || offset >= data.size() - table) return false;
			row.seek(table + offset);
			const uint32 runs = row.u8();
			uint32 x = 0;
			for (uint32 run = 0; run < runs; ++run) {
				const uint32 count = row.u8(); const byte level = row.u8();
				if (!row.good() || count > uint32(w) - x) return false;
				memset(next.depth.data() + y * w + x, level, count);
				x += count;
			}
			if (x != w) return false;
		}
	}
	frame = next;
	return true;
}
} // namespace DreamFactory
