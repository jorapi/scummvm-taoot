/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/archive.h"

namespace DreamFactory {
bool Reader::seek(uint32 pos) {
	_pos = pos;
	_ok = _ok && pos <= _data.size();
	return _ok;
}
byte Reader::u8() {
	if (!_ok || _pos >= _data.size()) { _ok = false; return 0; }
	return _data[_pos++];
}
uint16 Reader::u16() { uint16 a = u8(); return a | (uint16(u8()) << 8); }
uint32 Reader::u32() { uint32 a = u16(); return a | (uint32(u16()) << 16); }
Common::String Reader::name(uint32 pos, uint32 capacity) {
	seek(pos);
	uint32 count = u8();
	if (!capacity || count >= capacity || capacity > _data.size() - MIN<uint32>(pos, _data.size())) {
		_ok = false; return Common::String();
	}
	Common::String result;
	for (uint32 i = 0; i < count; ++i) result += char(u8());
	return result;
}
const Bytes &Archive::get(uint32 index) const { return has(index) ? _containers[index] : _empty; }
bool Archive::load(const Bytes &data) {
	_containers.clear();
	if (data.size() < 1024) return false;
	Reader r(data);
	r.u32(); // Unidentified format tag, not an ASCII FourCC.
	uint32 declared = r.u32();
	r.seek(20);
	uint32 count = r.u32(), type = r.u32(), gap = r.u32();
	if (declared != data.size() || count > (data.size() - 1024) / 4) return false;
	Common::Array<Bytes> containers;
	containers.resize(count);
	for (uint32 i = 0; i < count; ++i) {
		r.seek(1024 + i * 4);
		uint32 offset = r.u32();
		// Dust also has unallocated zero entries outside the typed gap (MAYOR,
		// NED and SCORP). They contain no record; retain an empty slot so all
		// subsequent container references keep their authored indices.
		if (!offset || (type == 1 && i == gap) || (type == 2 && (i == gap || (gap && i == gap - 1))) || ((type != 1 && type != 2) && offset <= 1024)) continue;
		if (offset < 1024 + count * 4 || offset > data.size() - 8) return false;
		r.seek(offset);
		r.u32(); // Record ID is not necessarily its index.
		uint32 length = r.u32();
		if (length > data.size() - offset - 8) return false;
		containers[i].resize(length);
		if (length) memcpy(containers[i].data(), data.data() + offset + 8, length);
	}
	_containers = containers;
	return true;
}
} // namespace DreamFactory
