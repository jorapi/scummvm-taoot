/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/routes.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/serializer.h"
namespace DreamFactory {
uint32 RouteGraph::append(RouteNode node) {
	if (_nodes.size() >= kMaxNodes) return 0;
	node.parent = _cursor; node.parentChoice = _choice;
	_nodes.push_back(node); _cursor = _nodes.size(); _choice = -1;
	return _cursor;
}
uint32 RouteGraph::sceneFor(uint32 id) const {
	while (const RouteNode *current = node(id)) {
		if (current->kind == RouteNode::kScene) return id;
		if (current->parent >= id) return 0;
		id = current->parent;
	}
	return 0;
}
uint32 RouteGraph::recordScene(const JournalEntry &entry) {
	if (!entry.choices.empty()) return 0;
	RouteNode node; node.entry = entry; node.entry.selected = -1;
	return append(node);
}
uint32 RouteGraph::recordDecision(const JournalEntry &entry) {
	if (entry.choices.empty() || entry.choices.size() > 5 || entry.selected < -1 || entry.selected >= int32(entry.choices.size()) || !sceneFor(_cursor)) return 0;
	uint32 id = 0;
	for (uint32 i = 0; i < _nodes.size(); ++i) {
		const RouteNode &node = _nodes[i];
		if (node.kind == RouteNode::kDecision && node.parent == _cursor && node.parentChoice == _choice &&
			node.entry.file.equalsIgnoreCase(entry.file) && node.entry.room == entry.room && node.entry.speaker == entry.speaker && node.entry.choices == entry.choices) { id = i + 1; break; }
	}
	if (!id) { RouteNode node; node.kind = RouteNode::kDecision; node.entry = entry; id = append(node); }
	if (!id) return 0;
	RouteNode &node = _nodes[id - 1];
	if (entry.selected >= 0) node.seenChoices |= 1u << entry.selected;
	node.entry.selected = entry.selected; node.entry.text = entry.text; node.entry.ident = entry.ident;
	_cursor = id; _choice = entry.selected;
	return id;
}
uint32 RouteGraph::recordEnding(const JournalEntry &entry) {
	if (!entry.choices.empty()) return 0;
	RouteNode node; node.kind = RouteNode::kEnding; node.entry = entry; node.entry.selected = -1;
	return append(node);
}
bool RouteGraph::restoreCursor(uint32 id, int32 choice) {
	const RouteNode *current = node(id);
	if (!id && choice == -1) { _cursor = 0; _choice = -1; return true; }
	if (!current || choice < -1 || (current->kind != RouteNode::kDecision && choice != -1) ||
		(choice >= 0 && (uint32(choice) >= current->entry.choices.size() || !(current->seenChoices & (1u << choice))))) return false;
	_cursor = id; _choice = choice; return true;
}
bool RouteGraph::attachCheckpoint(uint32 scene, uint32 bytes) {
	if (!node(scene) || node(scene)->kind != RouteNode::kScene || !bytes || bytes > 32 * 1024 * 1024) return false;
	_nodes[scene - 1].checkpoint = scene; _nodes[scene - 1].checkpointBytes = bytes; return true;
}
bool RouteGraph::valid() const {
	if (_nodes.size() > kMaxNodes) return false;
	for (uint32 i = 0; i < _nodes.size(); ++i) {
		const RouteNode &current = _nodes[i];
		if (current.kind < RouteNode::kScene || current.kind > RouteNode::kEnding || current.parent > i) return false;
		const RouteNode *parent = node(current.parent);
		if (current.parentChoice < -1 || (!parent && current.parentChoice != -1) ||
			(parent && parent->kind != RouteNode::kDecision && current.parentChoice != -1) ||
			(current.parentChoice >= 0 && (uint32(current.parentChoice) >= parent->entry.choices.size() || !(parent->seenChoices & (1u << current.parentChoice))))) return false;
		if (current.entry.choices.size() > 5 || current.entry.selected < -1 || current.entry.selected >= int32(current.entry.choices.size())) return false;
		if (current.kind == RouteNode::kDecision) {
			if (current.entry.choices.empty() || !sceneFor(i + 1) || (current.seenChoices >> current.entry.choices.size())) return false;
			if (current.entry.selected >= 0 && !(current.seenChoices & (1u << current.entry.selected))) return false;
		} else if (!current.entry.choices.empty() || current.seenChoices) return false;
		if (current.checkpoint) {
			if (current.kind != RouteNode::kScene || current.checkpoint != i + 1 || !current.checkpointBytes || current.checkpointBytes > 32 * 1024 * 1024) return false;
		} else if (current.checkpointBytes) return false;
	}
	if (!_cursor) return _choice == -1;
	const RouteNode *current = node(_cursor);
	return current && _choice >= -1 && (current->kind == RouteNode::kDecision || _choice == -1) &&
		(_choice < 0 || (uint32(_choice) < current->entry.choices.size() && (current->seenChoices & (1u << _choice))));
}
namespace {
const uint32 kMaxProfileBytes = 8 * 1024 * 1024;
class RouteIO {
	Common::Serializer _sync;
	Common::SeekableReadStream *_input;
public:
	bool good = true;
	RouteIO(Common::SeekableReadStream *input, Common::WriteStream *output) : _sync(input, output), _input(input) {}
	bool loading() const { return _input != nullptr; }
	void u32(uint32 &value) { _sync.syncAsUint32LE(value); good = good && !_sync.err() && (!_input || !_input->eos()); }
	void i32(int32 &value) { _sync.syncAsSint32LE(value); good = good && !_sync.err() && (!_input || !_input->eos()); }
	void text(Common::String &value) {
		uint32 size = value.size(); u32(size); if (size > 4096) { good = false; return; }
		if (loading()) value.clear();
		for (uint32 i = 0; good && i < size; ++i) {
			byte c = loading() ? 0 : byte(value[i]); _sync.syncAsByte(c);
			good = !_sync.err() && c && (!_input || !_input->eos());
			if (loading() && good) value += char(c);
		}
	}
	void entry(JournalEntry &entry) {
		text(entry.speaker); text(entry.room); text(entry.file); text(entry.ident); text(entry.text);
		uint32 low = uint32(entry.time), high = uint32(entry.time >> 32); u32(low); u32(high);
		if (loading()) entry.time = uint64(low) | (uint64(high) << 32);
		i32(entry.selected);
		uint32 size = entry.choices.size(); u32(size); if (size > 5) { good = false; return; }
		if (loading()) entry.choices.resize(size);
		for (uint32 i = 0; good && i < size; ++i) text(entry.choices[i]);
	}
	void node(RouteNode &node) {
		uint32 kind = node.kind; u32(kind); if (kind > RouteNode::kEnding) good = false;
		if (loading()) node.kind = RouteNode::Kind(kind);
		u32(node.parent); i32(node.parentChoice); u32(node.seenChoices); u32(node.checkpoint); u32(node.checkpointBytes); entry(node.entry);
	}
};
}
bool RouteCodec::encode(const RouteGraph &graph, const Common::String &signature, Bytes &bytes, Common::String &error) {
	bytes.clear(); error.clear();
	if (!graph.valid() || signature.empty() || signature.size() > 4096) { error = "Invalid route graph"; return false; }
	Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES); RouteIO io(nullptr, &payload);
	Common::String identity = signature; io.text(identity);
	uint32 cursor = graph._cursor, count = graph._nodes.size(); int32 choice = graph._choice;
	io.u32(cursor); io.i32(choice); io.u32(count);
	for (const auto &node : graph._nodes) { RouteNode copy = node; io.node(copy); if (!io.good || payload.size() > kMaxProfileBytes) break; }
	if (!io.good || payload.size() > kMaxProfileBytes) { error = "Route profile exceeds its limits"; return false; }
	Common::MemoryReadStream content(payload.getData(), uint32(payload.size())); byte digest[16]; Common::computeStreamMD5(content, digest);
	Common::MemoryWriteStreamDynamic output(DisposeAfterUse::YES);
	output.writeUint32BE(MKTAG('D','F','R','G')); output.writeUint32LE(1); output.writeUint32LE(uint32(payload.size())); output.write(digest, 16); output.write(payload.getData(), uint32(payload.size()));
	bytes.resize(uint32(output.size())); memcpy(bytes.data(), output.getData(), bytes.size()); return true;
}
bool RouteCodec::decode(const Bytes &bytes, const Common::String &signature, RouteGraph &graph, Common::String &error) {
	error.clear();
	if (bytes.size() < 28 || bytes.size() > kMaxProfileBytes + 28) { error = "Invalid route profile size"; return false; }
	Common::MemoryReadStream input(bytes.data(), bytes.size());
	const uint32 magic = input.readUint32BE(), version = input.readUint32LE(), size = input.readUint32LE();
	byte expected[16], digest[16]; input.read(expected, 16);
	if (magic != MKTAG('D','F','R','G') || version != 1 || size != bytes.size() - 28) { error = "Unsupported route profile"; return false; }
	Common::computeStreamMD5(input, digest); input.seek(28);
	if (memcmp(expected, digest, 16)) { error = "Route profile checksum mismatch"; return false; }
	RouteGraph prepared; RouteIO io(&input, nullptr); Common::String identity; io.text(identity);
	io.u32(prepared._cursor); io.i32(prepared._choice); uint32 count = 0; io.u32(count);
	if (count > RouteGraph::kMaxNodes || identity != signature || !io.good) { error = "Route profile belongs to different game data or exceeds its limits"; return false; }
	prepared._nodes.resize(count);
	for (auto &node : prepared._nodes) { io.node(node); if (!io.good) break; }
	if (!io.good || input.pos() != input.size() || !prepared.valid()) { error = "Invalid route profile data"; return false; }
	graph = prepared; return true;
}
}
