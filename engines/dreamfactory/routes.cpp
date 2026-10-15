/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/routes.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/serializer.h"
#include "common/algorithm.h"
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
uint32 RouteGraph::checkpointFor(uint32 id) const {
	while (const RouteNode *current = node(id)) {
		if (current->kind == RouteNode::kScene && current->checkpoint) return id;
		if (current->parent >= id) return 0;
		id = current->parent;
	}
	return 0;
}
uint32 RouteGraph::recordScene(const JournalEntry &entry, bool revisit) {
	if (!entry.choices.empty()) return 0;
	if (revisit) for (uint32 i = 0; i < _nodes.size(); ++i) {
		const RouteNode &node = _nodes[i];
		if (node.kind == RouteNode::kScene && node.parent == _cursor && node.parentChoice == _choice &&
			node.entry.file.equalsIgnoreCase(entry.file) && node.entry.ident == entry.ident && node.entry.speaker == entry.speaker && node.entry.room == entry.room && node.entry.text == entry.text) {
			_cursor = i + 1; _choice = -1; return _cursor;
		}
	}
	RouteNode node; node.entry = entry; node.entry.selected = -1;
	return append(node);
}
Common::String RouteGraph::cursorToken(uint32 id) const {
	if (!node(id)) return Common::String();
	Common::MemoryWriteStreamDynamic output(DisposeAfterUse::YES);
	while (const RouteNode *current = node(id)) {
		if (current->parent >= id) return Common::String();
		output.writeUint32LE(id); output.writeUint32LE(current->kind); output.writeSint32LE(current->parentChoice);
		output.writeUint32LE(uint32(current->entry.time)); output.writeUint32LE(uint32(current->entry.time >> 32));
		for (const auto &text : {current->entry.file, current->entry.room, current->entry.speaker}) {
			output.writeUint32LE(text.size()); output.write(text.c_str(), text.size());
		}
		output.writeUint32LE(current->entry.choices.size());
		for (const auto &text : current->entry.choices) { output.writeUint32LE(text.size()); output.write(text.c_str(), text.size()); }
		id = current->parent;
	}
	Common::MemoryReadStream input(output.getData(), uint32(output.size()));
	return Common::computeStreamMD5AsString(input);
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
	for (uint32 i = 0; i < _nodes.size(); ++i) {
		const RouteNode &node = _nodes[i];
		if (node.kind == RouteNode::kEnding && node.parent == _cursor && node.parentChoice == _choice && node.entry.file.equalsIgnoreCase(entry.file) && node.entry.text == entry.text) {
			_cursor = i + 1; _choice = -1; return _cursor;
		}
	}
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
	if (!node(scene) || node(scene)->kind != RouteNode::kScene || !bytes || bytes > kMaxCheckpointBytes) return false;
	_nodes[scene - 1].checkpoint = scene; _nodes[scene - 1].checkpointBytes = bytes; return true;
}
void RouteGraph::detachCheckpoint(uint32 scene) {
	if (node(scene)) { _nodes[scene - 1].checkpoint = 0; _nodes[scene - 1].checkpointBytes = 0; }
}
void RouteGraph::setChapter(uint32 scene, uint32 chapter) {
	if (node(scene) && node(scene)->kind == RouteNode::kScene && chapter <= 22) _nodes[scene - 1].chapter = chapter;
}
bool RouteGraph::chapterCheckpoint(uint32 parent, uint32 chapter) const {
	for (const RouteNode *current = node(parent); current; current = node(current->parent)) {
		if (current->chapter && current->chapter != chapter) return false;
		if (current->chapter == chapter && current->checkpoint) return true;
	}
	return false;
}
bool RouteGraph::chapterPoint(uint32 parent, uint32 chapter) const {
	for (const RouteNode *current = node(parent); current; current = node(current->parent)) {
		if (current->chapter && current->chapter != chapter) return false;
		if (current->chapter == chapter && (current->checkpoint || current->entry.file.empty())) return true;
	}
	return false;
}
uint32 identifyRouteChapter(uint32 version, bool retail, bool tour, int32 mission, int32 phase, int32 day) {
	if (tour) return 0;
	if (version == 1) return day >= 1 && day <= 5 ? 10 + day : 0;
	if (version != 4) return 0;
	if (!retail) return phase == 0 ? 21 : phase == 1 ? 22 : 0;
	if (mission >= 0 && mission <= 3) return mission + 1;
	return mission == 4 ? phase < 3 ? 5 : 6 : 0;
}
bool RouteGraph::valid() const {
	if (_nodes.size() > kMaxNodes) return false;
	for (uint32 i = 0; i < _nodes.size(); ++i) {
		const RouteNode &current = _nodes[i];
		if (current.kind < RouteNode::kScene || current.kind > RouteNode::kEnding || current.parent > i) return false;
		if (current.chapter > 22 || (current.chapter && current.kind != RouteNode::kScene)) return false;
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
			if (current.kind != RouteNode::kScene || current.checkpoint != i + 1 || !current.checkpointBytes || current.checkpointBytes > kMaxCheckpointBytes) return false;
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
	uint32 version = 2;
	RouteIO(Common::SeekableReadStream *input, Common::WriteStream *output) : _sync(input, output), _input(input) {}
	bool loading() const { return _input != nullptr; }
	void bytes(Bytes &value, uint32 limit) {
		uint32 size = value.size(); u32(size);
		if (!good || size > limit) { good = false; return; }
		if (loading()) value.resize(size);
		if (size) _sync.syncBytes(value.data(), size);
		good = good && !_sync.err() && (!_input || !_input->eos());
	}
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
		if (version >= 2) u32(node.chapter);
	}
};
}
static const uint32 kMaxPreviewBytes = 128 * 1024;
static bool syncPreview(RouteIO &io, RoutePreview &preview, Common::String &identity) {
	io.text(identity); io.text(preview.location); io.text(preview.clock); io.text(preview.heldItem);
	io.bytes(preview.snapshotDigest, 16); io.bytes(preview.picture, RoutePreview::kWidth * RoutePreview::kHeight * 3);
	uint32 count = preview.inventory.size(); io.u32(count);
	if (count > 256) return false;
	if (io.loading()) preview.inventory.resize(count);
	for (auto &item : preview.inventory) { io.text(item.name); io.text(item.owner); }
	return io.good && preview.snapshotDigest.size() == 16 && (preview.picture.empty() || preview.picture.size() == RoutePreview::kWidth * RoutePreview::kHeight * 3);
}
bool RoutePreviewCodec::encode(const RoutePreview &preview, const Common::String &identity, Bytes &bytes) {
	bytes.clear(); RoutePreview prepared = preview; Common::String key = identity;
	Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES); RouteIO io(nullptr, &payload);
	if (!syncPreview(io, prepared, key) || payload.size() > kMaxPreviewBytes - 28) return false;
	Common::MemoryReadStream content(payload.getData(), uint32(payload.size())); byte digest[16]; Common::computeStreamMD5(content, digest);
	Common::MemoryWriteStreamDynamic output(DisposeAfterUse::YES);
	output.writeUint32BE(MKTAG('D','F','R','P')); output.writeUint32LE(1); output.writeUint32LE(uint32(payload.size())); output.write(digest, 16); output.write(payload.getData(), uint32(payload.size()));
	bytes.resize(uint32(output.size())); memcpy(bytes.data(), output.getData(), bytes.size()); return true;
}
bool RoutePreviewCodec::decode(const Bytes &bytes, const Common::String &identity, RoutePreview &preview) {
	if (bytes.size() < 28 || bytes.size() > kMaxPreviewBytes) return false;
	Common::MemoryReadStream input(bytes.data(), bytes.size());
	if (input.readUint32BE() != MKTAG('D','F','R','P') || input.readUint32LE() != 1 || input.readUint32LE() != bytes.size() - 28) return false;
	byte expected[16], digest[16]; input.read(expected, 16); Common::computeStreamMD5(input, digest); input.seek(28);
	if (memcmp(expected, digest, 16)) return false;
	RoutePreview prepared; Common::String key; RouteIO io(&input, nullptr);
	if (!syncPreview(io, prepared, key) || key != identity || input.pos() != input.size()) return false;
	preview = prepared; return true;
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
	output.writeUint32BE(MKTAG('D','F','R','G')); output.writeUint32LE(2); output.writeUint32LE(uint32(payload.size())); output.write(digest, 16); output.write(payload.getData(), uint32(payload.size()));
	bytes.resize(uint32(output.size())); memcpy(bytes.data(), output.getData(), bytes.size()); return true;
}
bool RouteCodec::decode(const Bytes &bytes, const Common::String &signature, RouteGraph &graph, Common::String &error) {
	error.clear();
	if (bytes.size() < 28 || bytes.size() > kMaxProfileBytes + 28) { error = "Invalid route profile size"; return false; }
	Common::MemoryReadStream input(bytes.data(), bytes.size());
	const uint32 magic = input.readUint32BE(), version = input.readUint32LE(), size = input.readUint32LE();
	byte expected[16], digest[16]; input.read(expected, 16);
	if (magic != MKTAG('D','F','R','G') || (version != 1 && version != 2) || size != bytes.size() - 28) { error = "Unsupported route profile"; return false; }
	Common::computeStreamMD5(input, digest); input.seek(28);
	if (memcmp(expected, digest, 16)) { error = "Route profile checksum mismatch"; return false; }
	RouteGraph prepared; RouteIO io(&input, nullptr); io.version = version; Common::String identity; io.text(identity);
	io.u32(prepared._cursor); io.i32(prepared._choice); uint32 count = 0; io.u32(count);
	if (count > RouteGraph::kMaxNodes || identity != signature || !io.good) { error = "Route profile belongs to different game data or exceeds its limits"; return false; }
	prepared._nodes.resize(count);
	for (auto &node : prepared._nodes) { io.node(node); if (!io.good) break; }
	if (!io.good || input.pos() != input.size() || !prepared.valid()) { error = "Invalid route profile data"; return false; }
	graph = prepared; return true;
}
void RouteMap::build(const RouteGraph &graph, uint32 version, bool retail, bool tour) {
	_items.clear(); _nodeItems.clear();
	if (!graph.valid()) return;
	const uint32 count = graph.nodes().size();
	Common::Array<bool> keep; keep.resize(count + 1);
	Common::Array<uint32> endings, paths, descendants, aliases, branchChildren;
	descendants.resize(count + 1); aliases.resize(count + 1); branchChildren.resize(count + 1);
	Common::HashMap<Common::String, uint32> outcomes;
	uint32 firstChapter[23] = {};
	for (uint32 id = 1; id <= count; ++id) {
		const RouteNode &node = *graph.node(id);
		if (node.chapter && node.checkpoint && !firstChapter[node.chapter]) { firstChapter[node.chapter] = id; keep[id] = true; }
		if (node.kind != RouteNode::kEnding) continue;
		Common::String key = node.entry.file; key.toLowercase(); key += '\n'; key += node.entry.speaker; key += '\n'; key += node.entry.text;
		if (outcomes.contains(key)) { aliases[id] = outcomes[key]; continue; }
		outcomes[key] = id; endings.push_back(id); paths.push_back(id); keep[id] = true;
		for (uint32 ancestor = id; ancestor; ancestor = graph.node(ancestor)->parent) ++descendants[ancestor];
	}
	// Only forks leading to different reached outcomes need their own return point.
	for (uint32 id = 1; id <= count; ++id) if (descendants[id] && graph.node(id)->parent) ++branchChildren[graph.node(id)->parent];
	for (uint32 id = 1; id <= count; ++id) if (branchChildren[id] > 1) {
		const uint32 checkpoint = graph.checkpointFor(id);
		if (checkpoint) keep[checkpoint] = true;
	}
	if (graph.cursor()) paths.push_back(graph.cursor());
	else if (endings.empty() && count) paths.push_back(count);
	for (uint32 path : paths) {
		Common::Array<uint32> chain;
		for (uint32 id = path; id; id = graph.node(id)->parent) chain.push_back(id);
		bool seenChapter[23] = {}; uint32 firstScene = 0, firstCheckpoint = 0;
		for (int32 i = int32(chain.size()) - 1; i >= 0; --i) {
			const uint32 id = chain[i]; const RouteNode &node = *graph.node(id);
			if (node.kind != RouteNode::kScene) continue;
			if (!firstScene) firstScene = id;
			if (!firstCheckpoint && node.checkpoint) firstCheckpoint = id;
			if (node.chapter && node.checkpoint && !seenChapter[node.chapter]) { keep[id] = true; seenChapter[node.chapter] = true; }
		}
		if (firstCheckpoint) keep[firstCheckpoint] = true;
		else if (firstScene) keep[firstScene] = true;
	}
	if (graph.cursor()) {
		const uint32 current = graph.checkpointFor(graph.cursor());
		if (current) keep[current] = true;
	}
	_nodeItems.resize(count + 1); for (auto &index : _nodeItems) index = -1;
	for (uint32 id = 1; id <= count; ++id) if (keep[id]) {
		RouteMapItem item; item.node = id;
		uint32 parent = graph.node(id)->parent; item.choice = graph.node(id)->parentChoice;
		while (parent && !keep[parent]) { if (graph.node(parent)->parentChoice >= 0) item.choice = graph.node(parent)->parentChoice; parent = graph.node(parent)->parent; }
		item.parent = parent ? _nodeItems[parent] : -1;
		_nodeItems[id] = _items.size(); _items.push_back(item);
	}
	for (uint32 id = 1; id <= count; ++id) {
		if (aliases[id]) _nodeItems[id] = _nodeItems[aliases[id]];
		else if (_nodeItems[id] < 0 && graph.node(id)->parent) _nodeItems[id] = _nodeItems[graph.node(id)->parent];
	}
	Common::Array<int32> spans, starts;
	Common::Array<Common::Array<uint32>> children; children.resize(_items.size() + 1);
	spans.resize(_items.size()); starts.resize(_items.size());
	for (uint32 i = 0; i < _items.size(); ++i) children[_items[i].parent + 1].push_back(i);
	for (auto &siblings : children) Common::sort(siblings.begin(), siblings.end(), [&](uint32 a, uint32 b) {
		return _items[a].choice != _items[b].choice ? _items[a].choice < _items[b].choice : a < b;
	});
	for (int32 i = int32(_items.size()) - 1; i >= 0; --i) {
		spans[i] = MAX(1, spans[i]);
		if (_items[i].parent >= 0) spans[_items[i].parent] += spans[i];
	}
	int32 root = 0;
	for (uint32 i : children[0]) { starts[i] = root; root += spans[i]; }
	for (uint32 i = 0; i < _items.size(); ++i) {
		RouteMapItem &item = _items[i];
		if (item.parent >= 0) item.row = _items[item.parent].row + 1;
		item.column = starts[i] * 2 + spans[i] - 1;
		int32 next = starts[i];
		for (uint32 child : children[i + 1]) { starts[child] = next; next += spans[child]; }
	}
	const auto catalog = routeEndingCatalog(version, retail, tour);
	Common::Array<bool> reached; reached.resize(catalog.size());
	int32 lastRow = -1;
	for (auto &item : _items) {
		lastRow = MAX(lastRow, item.row);
		const RouteNode *node = graph.node(item.node);
		if (!node || node->kind != RouteNode::kEnding) continue;
		for (uint32 i = 0; i < catalog.size(); ++i) if (node->entry.speaker == catalog[i].speaker && node->entry.text == catalog[i].text) {
			item.ending = i + 1; reached[i] = true; break;
		}
	}
	// Unreached outcomes have no known route or restorable state yet.
	int32 column = 0;
	for (uint32 i = 0; i < catalog.size(); ++i) if (!reached[i]) {
		RouteMapItem item; item.ending = i + 1; item.locked = true;
		item.column = column; column += 2; item.row = lastRow + 1;
		_items.push_back(item);
	}
}
int32 RouteMap::itemFor(uint32 node) const {
	return node < _nodeItems.size() ? _nodeItems[node] : -1;
}
int32 RouteMap::navigate(int32 selected, int32 dx, int32 dy) const {
	if (_items.empty()) return -1;
	if (selected < 0 || uint32(selected) >= _items.size()) return 0;
	if (dy < 0 && _items[selected].parent >= 0) return _items[selected].parent;
	if (dy > 0) {
		int32 child = selected, column = 0x7fffffff;
		for (uint32 i = 0; i < _items.size(); ++i) if (_items[i].parent == selected && _items[i].column < column) { child = i; column = _items[i].column; }
		if (child != selected) return child;
	}
	if (dy) {
		int32 closest = selected, distance = 0x7fffffff;
		for (uint32 i = 0; i < _items.size(); ++i) {
			if (!_items[selected].locked && !_items[i].locked) continue;
			const int32 delta = _items[i].row - _items[selected].row;
			if ((dy < 0 && delta >= 0) || (dy > 0 && delta <= 0)) continue;
			const int32 candidate = ABS(delta) * 16384 + ABS(_items[i].column - _items[selected].column);
			if (candidate < distance) { closest = i; distance = candidate; }
		}
		return closest;
	}
	int32 closest = selected, distance = 0x7fffffff;
	for (uint32 i = 0; i < _items.size(); ++i) {
		if (_items[i].parent != _items[selected].parent || _items[i].row != _items[selected].row) continue;
		const int32 delta = _items[i].column - _items[selected].column;
		if (((dx < 0 && delta < 0) || (dx > 0 && delta > 0)) && ABS(delta) < distance) { closest = i; distance = ABS(delta); }
	}
	return closest;
}
bool RouteStore::read(const Common::String &name, uint32 limit, Bytes &bytes) const {
	bytes.clear();
	Common::InSaveFile *input = _files->openForLoading(name);
	if (!input) return false;
	const int64 size = input->size();
	bool good = size > 0 && size <= limit;
	if (good) { bytes.resize(uint32(size)); good = input->read(bytes.data(), bytes.size()) == bytes.size() && !input->err(); }
	delete input;
	if (!good) bytes.clear();
	return good;
}
bool RouteStore::write(const Common::String &name, const Bytes &bytes) const {
	Common::OutSaveFile *output = _files->openForSaving(name);
	if (!output) return false;
	const bool written = output->write(bytes.data(), bytes.size()) == bytes.size();
	output->finalize();
	const bool good = written && !output->err();
	delete output;
	return good;
}
Common::String RouteStore::checkpointName(uint32 scene) const {
	return Common::String::format("%s.%u.checkpoint", _stem.c_str(), scene);
}
bool RouteStore::open(Common::SaveFileManager *files, const Common::String &target, const Common::String &signature, Common::String &error) {
	_files = nullptr; _generation = 0; graph = RouteGraph(); error.clear();
	if (!files || target.empty() || target.size() > 128 || signature.empty() || signature.size() > 4000) { error = "Invalid route profile identity"; return false; }
	for (char c : target) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
		error = "Invalid route profile target"; return false;
	}
	Common::MemoryReadStream identity(reinterpret_cast<const byte *>(signature.c_str()), signature.size());
	_stem = target + ".dfroute-" + Common::computeStreamMD5AsString(identity); _signature = signature;
	_files = files;
	bool found = false;
	for (uint32 slot = 0; slot < 2; ++slot) {
		const Common::String name = _stem + (slot ? ".b" : ".a");
		if (!_files->exists(name)) continue;
		found = true; Bytes bytes;
		if (!read(name, kMaxProfileBytes + 32, bytes) || bytes.size() < 32) continue;
		Common::MemoryReadStream input(bytes.data(), bytes.size());
		const uint32 generation = input.readUint32LE();
		Bytes profile; profile.resize(bytes.size() - 4); memcpy(profile.data(), bytes.data() + 4, profile.size());
		RouteGraph prepared; Common::String ignored;
		if (generation > _generation && RouteCodec::decode(profile, Common::String::format("%s:%u", signature.c_str(), generation), prepared, ignored)) {
			graph = prepared; _generation = generation;
		}
	}
	if (found && !_generation) { _files = nullptr; error = "No intact route profile found"; return false; }
	return true;
}
bool RouteStore::save(Common::String &error) {
	error.clear();
	if (!active() || _generation == 0xffffffffu) { error = "Route profile is unavailable"; return false; }
	const uint32 next = _generation + 1;
	Bytes profile;
	if (!RouteCodec::encode(graph, Common::String::format("%s:%u", _signature.c_str(), next), profile, error)) return false;
	Bytes bytes; bytes.resize(profile.size() + 4);
	Common::MemoryWriteStream output(bytes.data(), bytes.size()); output.writeUint32LE(next); output.write(profile.data(), profile.size());
	// Keep the previous generation intact until the new slot is finalized.
	if (!write(_stem + (next & 1 ? ".a" : ".b"), bytes)) { error = "Cannot write route profile"; return false; }
	_generation = next; return true;
}
static bool validCheckpoint(const Bytes &bytes) {
	if (bytes.size() < 28 || bytes.size() > RouteGraph::kMaxCheckpointBytes) return false;
	Common::MemoryReadStream input(bytes.data(), bytes.size());
	const uint32 magic = input.readUint32BE(), version = input.readUint32LE(), size = input.readUint32LE();
	if (magic != MKTAG('D','F','S','V') || !version || size != bytes.size() - 28) return false;
	byte expected[16], digest[16]; input.read(expected, 16); Common::computeStreamMD5(input, digest);
	return !memcmp(expected, digest, 16);
}
bool RouteStore::writeCheckpoint(uint32 scene, const Bytes &bytes, Common::String &error, const RoutePreview *preview) {
	error.clear(); const RouteNode *node = graph.node(scene);
	if (!active() || !node || node->kind != RouteNode::kScene || !validCheckpoint(bytes)) { error = "Invalid scene checkpoint"; return false; }
	if (node->checkpoint) { error = "Scene already has a checkpoint"; return false; }
	uint64 used = 0;
	for (uint32 id = 1; id <= graph.nodes().size(); ++id) {
		used += graph.node(id)->checkpointBytes;
		if (graph.node(id)->checkpoint && _files->exists(checkpointName(id) + ".preview")) used += kMaxPreviewBytes;
	}
	if (bytes.size() > _budget || used > _budget - bytes.size()) { error = "Route checkpoint storage limit reached"; return false; }
	const Common::String name = checkpointName(scene);
	if (!write(name, bytes)) { _files->removeSavefile(name); error = "Cannot write scene checkpoint"; return false; }
	graph.attachCheckpoint(scene, bytes.size());
	if (save(error)) {
		if (preview && used + bytes.size() + kMaxPreviewBytes <= _budget) {
			RoutePreview prepared = *preview; prepared.snapshotDigest.resize(16); memcpy(prepared.snapshotDigest.data(), bytes.data() + 12, 16);
			Bytes encoded;
			if (RoutePreviewCodec::encode(prepared, Common::String::format("%s:%u", _signature.c_str(), scene), encoded) && !write(name + ".preview", encoded)) _files->removeSavefile(name + ".preview");
		}
		return true;
	}
	graph.detachCheckpoint(scene); _files->removeSavefile(name); return false;
}
bool RouteStore::readPreview(uint32 id, RoutePreview &preview) const {
	preview = RoutePreview();
	const uint32 scene = graph.checkpointFor(id); const RouteNode *node = graph.node(scene);
	if (!active() || !node || !node->checkpoint) return false;
	Bytes bytes; RoutePreview prepared;
	if (!read(checkpointName(scene) + ".preview", kMaxPreviewBytes, bytes) || !RoutePreviewCodec::decode(bytes, Common::String::format("%s:%u", _signature.c_str(), scene), prepared)) return false;
	Common::InSaveFile *snapshot = _files->openForLoading(checkpointName(scene));
	if (!snapshot) return false;
	byte header[28]; const bool matches = snapshot->size() == node->checkpointBytes && snapshot->read(header, 28) == 28 && READ_BE_UINT32(header) == MKTAG('D','F','S','V') && READ_LE_UINT32(header + 8) == node->checkpointBytes - 28 && !memcmp(header + 12, prepared.snapshotDigest.data(), 16);
	delete snapshot;
	if (!matches) return false;
	preview = prepared; return true;
}
bool RouteStore::readCheckpoint(uint32 id, Bytes &bytes, Common::String &error) const {
	error.clear(); bytes.clear();
	const uint32 scene = graph.checkpointFor(id); const RouteNode *node = graph.node(scene);
	if (!active() || !node || !node->checkpoint) { error = "No checkpoint is available for this scene"; return false; }
	if (!read(checkpointName(scene), RouteGraph::kMaxCheckpointBytes, bytes) || bytes.size() != node->checkpointBytes || !validCheckpoint(bytes)) {
		bytes.clear(); error = "Scene checkpoint is missing or damaged"; return false;
	}
	return true;
}
static Common::String endingBaseName(Common::String name) {
	uint32 start = 0;
	for (uint32 i = 0; i < name.size(); ++i) if (name[i] == '/' || name[i] == '\\' || name[i] == ':') start = i + 1;
	name = name.substr(start); name.toLowercase(); return name;
}
bool identifyRouteEnding(const RouteEndingContext &context, JournalEntry &entry) {
	const Common::String movie = endingBaseName(context.movie), stage = endingBaseName(context.stage);
	JournalEntry prepared; prepared.file = context.movie;
	if (context.version == 4 && context.retail && stage == "narend.stg" && context.clock == "endgame") {
		const uint32 history = uint32(context.firstWar) | (uint32(context.secondWar) << 1) | (uint32(context.revolution) << 2);
		const char *movies[] = {"prozac.mov", "rushend.mov", "germend.mov", "germend.mov", "rushend.mov", "rushend.mov", "nuke.mov", "boom.mov"};
		if (movie != movies[history]) return false;
		prepared.speaker = history == 0 ? "A changed future" : history == 7 ? "History unchanged" : "An altered future";
		prepared.text = Common::String::format("World War I: %s\nWorld War II: %s\nRussian Revolution: %s", context.firstWar ? "occurred" : "averted", context.secondWar ? "occurred" : "averted", context.revolution ? "occurred" : "averted");
	} else if (context.version == 1 && context.day == 5 && context.phase >= 2 && context.phase <= 6 && stage == "new.flt") {
		const char *movies[] = {"trottend.mov", "mayorend.mov", "marieend.mov", "yunniend.mov", "deserend.mov"};
		const char *titles[] = {"Trotter's ranch", "The mayor", "Marie", "The Yunni", "The desert"};
		if (movie != movies[context.phase - 2]) return false;
		prepared.speaker = titles[context.phase - 2]; prepared.text = "You reached this ending of Dust.";
	} else if (context.version == 4 && !context.retail && context.phase == 2 && movie == "postdemo.mov") {
		prepared.speaker = "Interactive demo conclusion"; prepared.text = "You reached the conclusion of the supplied interactive demo.";
	} else return false;
	entry = prepared; return true;
}
Common::Array<JournalEntry> routeEndingCatalog(uint32 version, bool retail, bool tour) {
	Common::Array<JournalEntry> catalog;
	if (tour) return catalog;
	RouteEndingContext context; context.version = version; context.retail = retail;
	const char *titanic[] = {"prozac.mov", "rushend.mov", "germend.mov", "germend.mov", "rushend.mov", "rushend.mov", "nuke.mov", "boom.mov"};
	const char *dust[] = {"trottend.mov", "mayorend.mov", "marieend.mov", "yunniend.mov", "deserend.mov"};
	const uint32 count = version == 1 ? 5 : version == 4 ? retail ? 8 : 1 : 0;
	for (uint32 i = 0; i < count; ++i) {
		context.stage = version == 1 ? "new.flt" : "narend.stg"; context.clock = "endgame";
		context.day = 5; context.phase = i + 2;
		context.firstWar = i & 1; context.secondWar = i & 2; context.revolution = i & 4;
		context.movie = version == 1 ? dust[i] : retail ? titanic[i] : "postdemo.mov";
		JournalEntry entry;
		if (identifyRouteEnding(context, entry)) catalog.push_back(entry);
	}
	return catalog;
}
}
