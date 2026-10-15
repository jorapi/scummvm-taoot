/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ROUTES_H
#define DREAMFACTORY_ROUTES_H
#include "dreamfactory/journal.h"
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
	const Common::Array<RouteNode> &nodes() const { return _nodes; }
	const RouteNode *node(uint32 id) const { return id && id <= _nodes.size() ? &_nodes[id - 1] : nullptr; }
	uint32 cursor() const { return _cursor; }
	int32 choice() const { return _choice; }
	uint32 sceneFor(uint32 id) const;
	uint32 recordScene(const JournalEntry &entry);
	uint32 recordDecision(const JournalEntry &entry);
	uint32 recordEnding(const JournalEntry &entry);
	bool restoreCursor(uint32 id, int32 choice);
	bool attachCheckpoint(uint32 scene, uint32 bytes);
	bool valid() const;
};
class RouteCodec {
public:
	// Metadata only. Scene snapshots stay in separate, bounded native save files.
	static bool encode(const RouteGraph &graph, const Common::String &signature, Bytes &bytes, Common::String &error);
	static bool decode(const Bytes &bytes, const Common::String &signature, RouteGraph &graph, Common::String &error);
};
}
#endif
