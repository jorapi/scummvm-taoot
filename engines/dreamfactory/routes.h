/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ROUTES_H
#define DREAMFACTORY_ROUTES_H
#include "dreamfactory/journal.h"
#include "common/savefile.h"
namespace DreamFactory {
struct RouteNode {
	enum Kind { kScene, kDecision, kEnding } kind = kScene;
	uint32 parent = 0;
	int32 parentChoice = -1;
	JournalEntry entry;
	uint32 seenChoices = 0;
	uint32 checkpoint = 0, checkpointBytes = 0;
};
// Discovery belongs to the profile; the current cursor belongs to a playthrough.
class RouteGraph {
	friend class RouteCodec;
	Common::Array<RouteNode> _nodes;
	uint32 _cursor = 0;
	int32 _choice = -1;
	uint32 append(RouteNode node);
public:
	static const uint32 kMaxNodes = 4096;
	static const uint32 kMaxCheckpointBytes = 128 * 1024 * 1024 + 28;
	const Common::Array<RouteNode> &nodes() const { return _nodes; }
	const RouteNode *node(uint32 id) const { return id && id <= _nodes.size() ? &_nodes[id - 1] : nullptr; }
	uint32 cursor() const { return _cursor; }
	int32 choice() const { return _choice; }
	uint32 sceneFor(uint32 id) const;
	Common::String cursorToken(uint32 id) const;
	uint32 recordScene(const JournalEntry &entry);
	uint32 recordDecision(const JournalEntry &entry);
	uint32 recordEnding(const JournalEntry &entry);
	bool restoreCursor(uint32 id, int32 choice);
	bool attachCheckpoint(uint32 scene, uint32 bytes);
	void detachCheckpoint(uint32 scene);
	bool valid() const;
};
class RouteCodec {
public:
	// Metadata only. Scene snapshots stay in separate, bounded native save files.
	static bool encode(const RouteGraph &graph, const Common::String &signature, Bytes &bytes, Common::String &error);
	static bool decode(const Bytes &bytes, const Common::String &signature, RouteGraph &graph, Common::String &error);
};
struct RouteMapItem {
	uint32 node = 0;
	int32 parent = -1, choice = -1;
	int32 column = 0, row = 0;
	bool locked = false;
};
class RouteMap {
	Common::Array<RouteMapItem> _items;
public:
	void build(const RouteGraph &graph);
	const Common::Array<RouteMapItem> &items() const { return _items; }
	int32 itemFor(uint32 node) const;
	int32 navigate(int32 selected, int32 dx, int32 dy) const;
};
class RouteStore {
	Common::SaveFileManager *_files = nullptr;
	Common::String _stem, _signature;
	uint32 _generation = 0;
	uint64 _budget = 1024ull * 1024 * 1024;
	bool read(const Common::String &name, uint32 limit, Bytes &bytes) const;
	bool write(const Common::String &name, const Bytes &bytes) const;
	Common::String checkpointName(uint32 scene) const;
public:
	RouteGraph graph;
	bool active() const { return _files != nullptr; }
	bool open(Common::SaveFileManager *files, const Common::String &target, const Common::String &signature, Common::String &error);
	void setBudget(uint64 bytes) { _budget = bytes; }
	bool save(Common::String &error);
	bool writeCheckpoint(uint32 scene, const Bytes &bytes, Common::String &error);
	bool readCheckpoint(uint32 node, Bytes &bytes, Common::String &error) const;
};
}
#endif
