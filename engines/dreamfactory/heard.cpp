/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/heard.h"
#include "common/md5.h"
#include "common/memstream.h"
namespace DreamFactory {
namespace {
const uint32 kMaxProfile = 1024 * 1024;
bool readProfile(Common::SaveFileManager *files, const Common::String &name, Bytes &bytes) {
	bytes.clear(); Common::InSaveFile *input = files->openForLoading(name);
	if (!input) return false;
	bool good = input->size() >= 28 && input->size() <= kMaxProfile;
	if (good) { bytes.resize(uint32(input->size())); good = input->read(bytes.data(), bytes.size()) == bytes.size() && !input->err(); }
	delete input; if (!good) bytes.clear(); return good;
}
}
bool HeardIndex::key(Common::String file, Common::String ident, const Common::String &text, HeardLine &line) {
	if (file.empty() || ident.empty() || text.empty() || file.size() > 4096 || ident.size() > 4096 || text.size() > 16384) return false;
	file.toLowercase(); ident.toLowercase();
	Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES);
	const Common::String *parts[] = {&file, &ident, &text};
	for (const auto *part : parts) { encoded.writeUint32LE(part->size()); encoded.write(part->c_str(), part->size()); }
	Common::MemoryReadStream input(encoded.getData(), uint32(encoded.size()));
	return Common::computeStreamMD5(input, line.digest);
}
uint32 HeardIndex::position(const HeardLine &line) const {
	uint32 low = 0, high = _lines.size();
	while (low < high) { const uint32 middle = low + (high - low) / 2; if (memcmp(_lines[middle].digest, line.digest, 16) < 0) low = middle + 1; else high = middle; }
	return low;
}
bool HeardIndex::contains(const Common::String &file, const Common::String &ident, const Common::String &text) const {
	HeardLine line; if (!key(file, ident, text, line)) return false;
	const uint32 found = position(line); return found < _lines.size() && !memcmp(_lines[found].digest, line.digest, 16);
}
bool HeardIndex::add(const Common::String &file, const Common::String &ident, const Common::String &text) {
	HeardLine line; if (!key(file, ident, text, line)) return false;
	const uint32 found = position(line);
	if (found < _lines.size() && !memcmp(_lines[found].digest, line.digest, 16)) return false;
	if (_lines.size() >= kMaxLines) return false;
	_lines.insert_at(found, line); return true;
}
bool HeardStore::decode(const Bytes &bytes, HeardIndex &result, uint32 &generation) const {
	if (bytes.size() < 28 || bytes.size() > kMaxProfile) return false;
	Common::MemoryReadStream input(bytes.data(), bytes.size());
	if (input.readUint32BE() != MKTAG('D','F','H','D') || input.readUint32LE() != 1 || input.readUint32LE() != bytes.size() - 28) return false;
	byte expected[16], digest[16]; input.read(expected, 16);
	if (!Common::computeStreamMD5(input, digest) || memcmp(expected, digest, 16) || !input.seek(28)) return false;
	const uint32 next = input.readUint32LE(), identitySize = input.readUint32LE();
	if (!next || identitySize > 4000 || identitySize > uint64(input.size() - input.pos())) return false;
	Common::String identity; for (uint32 i = 0; i < identitySize; ++i) identity += char(input.readByte());
	if (identity != _signature) return false;
	const uint32 count = input.readUint32LE();
	if (count > HeardIndex::kMaxLines || input.size() - input.pos() != int64(count) * 16 || input.err()) return false;
	HeardIndex prepared; prepared._lines.resize(count);
	for (uint32 i = 0; i < count; ++i) {
		if (input.read(prepared._lines[i].digest, 16) != 16 || (i && memcmp(prepared._lines[i - 1].digest, prepared._lines[i].digest, 16) >= 0)) return false;
	}
	if (input.err()) return false;
	result = prepared; generation = next; return true;
}
bool HeardStore::open(Common::SaveFileManager *files, const Common::String &target, const Common::String &signature, Common::String &error) {
	_files = nullptr; _generation = 0; _dirty = false; _index = HeardIndex(); error.clear();
	if (!files || target.empty() || target.size() > 128 || signature.empty() || signature.size() > 4000) { error = "Invalid heard-history profile"; return false; }
	for (char c : target) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) { error = "Invalid heard-history target"; return false; }
	Common::MemoryReadStream identity(reinterpret_cast<const byte *>(signature.c_str()), signature.size());
	_stem = target + ".dfheard-" + Common::computeStreamMD5AsString(identity); _signature = signature; _files = files;
	bool found = false;
	for (uint32 slot = 0; slot < 2; ++slot) {
		const Common::String name = _stem + (slot ? ".b" : ".a");
		if (!files->exists(name)) continue;
		found = true; Bytes bytes; HeardIndex prepared; uint32 generation = 0;
		if (readProfile(files, name, bytes) && decode(bytes, prepared, generation) && generation > _generation) { _index = prepared; _generation = generation; }
	}
	if (found && !_generation) { _files = nullptr; error = "No intact heard-history profile found"; return false; }
	return true;
}
bool HeardStore::mark(const Common::String &file, const Common::String &ident, const Common::String &text) {
	const bool added = _index.add(file, ident, text); _dirty = _dirty || added; return added;
}
bool HeardStore::save(Common::String &error) {
	error.clear(); if (!_dirty) return true;
	if (!active() || _generation == 0xffffffffu) { error = "Heard-history storage unavailable"; return false; }
	Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES);
	payload.writeUint32LE(_generation + 1); payload.writeUint32LE(_signature.size()); payload.write(_signature.c_str(), _signature.size()); payload.writeUint32LE(_index.size());
	for (const auto &line : _index._lines) payload.write(line.digest, 16);
	Common::MemoryReadStream content(payload.getData(), uint32(payload.size())); byte digest[16]; Common::computeStreamMD5(content, digest);
	const Common::String name = _stem + ((_generation + 1) & 1 ? ".a" : ".b");
	Common::OutSaveFile *output = _files->openForSaving(name);
	if (!output) { error = "Cannot open heard-history file"; return false; }
	output->writeUint32BE(MKTAG('D','F','H','D')); output->writeUint32LE(1); output->writeUint32LE(uint32(payload.size())); output->write(digest, 16);
	bool good = output->write(payload.getData(), uint32(payload.size())) == uint32(payload.size());
	output->finalize(); good = good && !output->err(); delete output;
	Bytes checked; HeardIndex prepared; uint32 generation = 0;
	if (!good || !readProfile(_files, name, checked) || !decode(checked, prepared, generation) || generation != _generation + 1) { error = "Cannot finalize heard history"; return false; }
	_generation = generation; _dirty = false; return true;
}
}
