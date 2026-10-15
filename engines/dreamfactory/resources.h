/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_RESOURCES_H
#define DREAMFACTORY_RESOURCES_H
#include "dreamfactory/archive.h"
#include "common/fs.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
namespace DreamFactory {
// Script paths are classic colon-separated logical paths, independent of the
// platform running ScummVM. Keep CD variants separate throughout lookup.
class Resources {
public:
	bool init(const Common::FSNode &root);
	bool mount(const Common::String &volume);
	const Common::String &currentCD() const { return _currentCD; }
	Common::String path(uint32 slot) const;
	bool setPath(uint32 slot, const Common::String &path);
	bool resolve(const Common::String &name, Common::FSNode &node) const;
	bool read(const Common::String &name, Bytes &data) const;
	Common::String relativeName(const Common::FSNode &node) const;
	Common::String sourceName(const Common::String &name) const;
	bool canonical() const { return _canonical; }
private:
	Common::FSNode _root;
	Common::HashMap<Common::String, Common::FSNode> _files;
	Common::HashMap<Common::String, bool> _volumes;
	Common::String _paths[9], _currentCD;
	bool _canonical = false;
	bool index(const Common::FSNode &dir, const Common::String &prefix, uint32 depth = 0);
	bool find(const Common::String &key, Common::FSNode &node) const;
	static bool normalize(const Common::String &input, Common::String &key);
};
}
#endif
