/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_HEARD_H
#define DREAMFACTORY_HEARD_H
#include "dreamfactory/archive.h"
#include "common/savefile.h"
namespace DreamFactory {
struct HeardLine { byte digest[16]; };
class HeardIndex {
	friend class HeardStore;
	Common::Array<HeardLine> _lines;
	static bool key(Common::String file, Common::String ident, const Common::String &text, HeardLine &line);
	uint32 position(const HeardLine &line) const;
public:
	static const uint32 kMaxLines = 32768;
	uint32 size() const { return _lines.size(); }
	bool contains(const Common::String &file, const Common::String &ident, const Common::String &text) const;
	bool add(const Common::String &file, const Common::String &ident, const Common::String &text);
};
// Heard history belongs to the game-data profile, independently of save rewinds.
class HeardStore {
	Common::SaveFileManager *_files = nullptr;
	Common::String _stem, _signature;
	uint32 _generation = 0;
	bool _dirty = false;
	HeardIndex _index;
	bool decode(const Bytes &bytes, HeardIndex &index, uint32 &generation) const;
public:
	const HeardIndex &index() const { return _index; }
	bool active() const { return _files != nullptr; }
	bool dirty() const { return _dirty; }
	bool open(Common::SaveFileManager *files, const Common::String &target, const Common::String &signature, Common::String &error);
	bool mark(const Common::String &file, const Common::String &ident, const Common::String &text);
	bool save(Common::String &error);
};
}
#endif
