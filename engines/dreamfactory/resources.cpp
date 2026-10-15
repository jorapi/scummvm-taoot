/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/resources.h"
#include "common/file.h"
namespace DreamFactory {
bool Resources::normalize(const Common::String &input, Common::String &key) {
	key.clear(); Common::String part;
	for (uint32 i = 0; i <= input.size(); ++i) {
		const char c = i == input.size() ? ':' : input[i];
		if (c == ':' || c == '/' || c == '\\') {
			if (part == ".." || part == ".") return false;
			if (!part.empty()) { if (!key.empty()) key += '/'; key += part; part.clear(); }
		} else {
			if (byte(c) < 32) return false;
			part += c;
		}
	}
	key.toLowercase(); return !key.empty();
}
bool Resources::index(const Common::FSNode &dir, const Common::String &prefix, uint32 depth) {
	Common::FSList children;
	if (depth > 16 || !dir.getChildren(children, Common::FSNode::kListAll)) return false;
	for (uint32 i = 0; i < children.size(); ++i) {
		Common::String key;
		if (!normalize(prefix + ":" + children[i].getName(), key)) return false;
		if (children[i].isDirectory()) { if (!index(children[i], key, depth + 1)) return false; }
		else {
			if (_files.contains(key)) return false; // Ambiguous case-folded names.
			_files[key] = children[i];
		}
	}
	return true;
}
bool Resources::init(const Common::FSNode &root) {
	_root = root; _files.clear(); _volumes.clear(); _currentCD.clear();
	for (uint32 i = 0; i < ARRAYSIZE(_paths); ++i) _paths[i].clear();
	const Common::FSNode volumes = root.getChild("volumes"), installed = root.getChild("installed");
	_canonical = volumes.isDirectory();
	_paths[0] = _paths[1] = _canonical ? "installed:" : "flat:";
	if (!_canonical) return index(root, "flat");
	if (installed.isDirectory() && !index(installed, "installed")) return false;
	Common::FSList discs;
	if (!volumes.getChildren(discs, Common::FSNode::kListDirectoriesOnly)) return false;
	for (uint32 i = 0; i < discs.size(); ++i) {
		Common::String key; if (!normalize(discs[i].getName(), key) || key.contains('/')) return false;
		if (_volumes.contains(key) || !index(discs[i], key)) return false;
		_volumes[key] = true;
	}
	return !_volumes.empty();
}
bool Resources::mount(const Common::String &volume) {
	Common::String key;
	_currentCD.clear();
	if (!normalize(volume, key) || !_volumes.contains(key)) return false;
	_currentCD = volume; return true;
}
Common::String Resources::path(uint32 slot) const { return slot < ARRAYSIZE(_paths) ? _paths[slot] : Common::String(); }
bool Resources::setPath(uint32 slot, const Common::String &path) {
	if (!slot || slot >= ARRAYSIZE(_paths) || path.size() > 255) return false;
	Common::String key;
	if (!path.empty() && !normalize(path, key)) return false;
	_paths[slot] = path; return true;
}
bool Resources::find(const Common::String &key, Common::FSNode &node) const {
	const auto found = _files.find(key);
	if (found == _files.end()) return false;
	node = found->_value; return true;
}
bool Resources::resolve(const Common::String &name, Common::FSNode &node) const {
	Common::String key;
	if (!normalize(name, key)) return false;
	if (key.contains('/') && find(key, node)) return true;
	// An explicitly volume-qualified name must not fall through to another CD.
	if (key.contains('/') && _canonical) {
		Common::String volume = key.substr(0, key.find('/'));
		if (_volumes.contains(volume) || volume == "installed") return false;
	}
	for (uint32 i = 1; i < ARRAYSIZE(_paths); ++i) {
		Common::String candidate;
		if (!_paths[i].empty() && normalize(_paths[i] + ":" + name, candidate) && find(candidate, node)) return true;
	}
	if (key == "bootfile" && find("installed/bootfile", node)) return true;
	// Legacy flat folders are usable by the development viewer. Canonical data
	// must never silently choose a flattened winner for an unresolved script path.
	return !_canonical && find("flat/" + key, node);
}
bool Resources::read(const Common::String &name, Bytes &data) const {
	data.clear(); Common::FSNode node; Common::File file;
	if (!resolve(name, node) || !file.open(node) || file.size() < 0 || file.size() > 512 * 1024 * 1024) return false;
	data.resize(uint32(file.size()));
	if (file.read(data.data(), data.size()) == data.size()) return true;
	data.clear(); return false;
}
Common::String Resources::relativeName(const Common::FSNode &node) const { return node.getPath().relativeTo(_root.getPath()).toString('/'); }
Common::String Resources::sourceName(const Common::String &name) const {
	Common::FSNode node;
	if (!resolve(name, node)) return Common::String();
	Common::String source = relativeName(node);
	if (!_canonical) return "flat/" + source;
	return source.hasPrefix("volumes/") ? source.substr(8) : source;
}
}
