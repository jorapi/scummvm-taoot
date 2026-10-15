/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ARCHIVE_H
#define DREAMFACTORY_ARCHIVE_H

#include "common/array.h"
#include "common/str.h"

namespace DreamFactory {
typedef Common::Array<byte> Bytes;

// Every read is bounded, including references between containers.
class Reader {
public:
	Reader(const Bytes &data, uint32 pos = 0) : _data(data), _pos(pos), _ok(pos <= data.size()) {}
	bool seek(uint32 pos);
	bool good() const { return _ok; }
	uint32 pos() const { return _pos; }
	byte u8();
	uint16 u16();
	uint32 u32();
	Common::String name(uint32 pos, uint32 capacity = 16);
private:
	const Bytes &_data;
	uint32 _pos;
	bool _ok;
};

class Archive {
public:
	bool load(const Bytes &data);
	const Bytes &get(uint32 index) const;
	bool has(uint32 index) const { return index < _containers.size() && !_containers[index].empty(); }
	uint32 size() const { return _containers.size(); }
private:
	Common::Array<Bytes> _containers;
	Bytes _empty;
};
} // namespace DreamFactory
#endif
