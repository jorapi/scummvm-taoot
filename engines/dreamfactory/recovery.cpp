/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/recovery.h"
#include "common/algorithm.h"
#include "common/endian.h"
#include "common/md5.h"
#include "common/memstream.h"
namespace DreamFactory {
namespace {
const uint32 kMaxPreview = 128 * 1024;
bool validSnapshot(const Bytes &bytes) {
	if (bytes.size() < 28 || bytes.size() > RouteGraph::kMaxCheckpointBytes || READ_BE_UINT32(bytes.data()) != MKTAG('D','F','S','V') || READ_LE_UINT32(bytes.data() + 8) != bytes.size() - 28) return false;
	const uint32 version = READ_LE_UINT32(bytes.data() + 4);
	if (version < 1 || version > 12) return false;
	Common::MemoryReadStream payload(bytes.data() + 28, bytes.size() - 28); byte digest[16];
	Common::computeStreamMD5(payload, digest);
	return !memcmp(digest, bytes.data() + 12, 16);
}
bool verifySnapshot(Common::SeekableReadStream &input, const RecoveryEntry &entry, uint32 size) {
	byte native[28], digest[16];
	if (input.read(native, sizeof(native)) != sizeof(native)) return false;
	return Common::computeStreamMD5(input, digest, size - 28) && !input.err() && !memcmp(digest, entry.preview.snapshotDigest.data(), 16);
}
}
Common::String RecoveryStore::name(uint32 slot) const { return Common::String::format("%s.%u", _stem.c_str(), slot); }
bool RecoveryStore::open(Common::SaveFileManager *files, const Common::String &target, const Common::String &signature) {
	_files = nullptr;
	if (!files || target.empty() || target.size() > 128 || signature.empty() || signature.size() > 4000) return false;
	for (char c : target) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
	Common::MemoryReadStream identity(reinterpret_cast<const byte *>(signature.c_str()), signature.size());
	_stem = target + ".dfrecovery-" + Common::computeStreamMD5AsString(identity); _signature = signature; _files = files;
	return true;
}
bool RecoveryStore::header(Common::SeekableReadStream &input, uint32 slot, RecoveryEntry &entry, uint32 &size) const {
	if (input.size() < 48 || input.readUint32BE() != MKTAG('D','F','R','Y') || input.readUint32LE() != 1) return false;
	const uint32 sequence = input.readUint32LE(), previewSize = input.readUint32LE(); size = input.readUint32LE();
	if (!sequence || previewSize > kMaxPreview || size < 28 || size > RouteGraph::kMaxCheckpointBytes || uint64(input.size()) != 20ull + previewSize + size) return false;
	Bytes preview; preview.resize(previewSize);
	if (input.read(preview.data(), preview.size()) != preview.size()) return false;
	RecoveryEntry prepared; prepared.sequence = sequence; prepared.slot = slot;
	if (!RoutePreviewCodec::decode(preview, Common::String::format("%s:recovery:%u", _signature.c_str(), sequence), prepared.preview)) return false;
	const int64 nativeStart = input.pos(); byte native[28];
	if (input.read(native, sizeof(native)) != sizeof(native) || input.err() || READ_BE_UINT32(native) != MKTAG('D','F','S','V') || READ_LE_UINT32(native + 8) != size - 28 || prepared.preview.snapshotDigest.size() != 16 || memcmp(native + 12, prepared.preview.snapshotDigest.data(), 16)) return false;
	if (READ_LE_UINT32(native + 4) < 1 || READ_LE_UINT32(native + 4) > 12 || !input.seek(nativeStart)) return false;
	entry = prepared; return true;
}
Common::Array<RecoveryEntry> RecoveryStore::scan(bool verify) const {
	Common::Array<RecoveryEntry> entries;
	if (!active()) return entries;
	for (uint32 slot = 0; slot < 4; ++slot) {
		Common::InSaveFile *input = _files->openForLoading(name(slot));
		if (!input) continue;
		RecoveryEntry entry; uint32 size;
		if (header(*input, slot, entry, size) && (!verify || verifySnapshot(*input, entry, size))) entries.push_back(entry);
		delete input;
	}
	Common::sort(entries.begin(), entries.end(), [](const RecoveryEntry &a, const RecoveryEntry &b) { return a.sequence > b.sequence; });
	return entries;
}
Common::Array<RecoveryEntry> RecoveryStore::list() const {
	auto entries = scan(true); if (entries.size() > 3) entries.resize(3); return entries;
}
bool RecoveryStore::read(const RecoveryEntry &entry, Bytes &snapshot, Common::String &error) const {
	snapshot.clear(); error.clear();
	if (!active() || entry.slot >= 4 || !entry.sequence) { error = "Recovery save is unavailable"; return false; }
	Common::InSaveFile *input = _files->openForLoading(name(entry.slot)); RecoveryEntry prepared; uint32 size = 0;
	bool good = input && header(*input, entry.slot, prepared, size) && prepared.sequence == entry.sequence;
	if (good) { snapshot.resize(size); good = input->read(snapshot.data(), size) == size && !input->err() && validSnapshot(snapshot); }
	delete input;
	if (!good) { snapshot.clear(); error = "Recovery save is missing, damaged or has been replaced"; }
	return good;
}
bool RecoveryStore::write(const Bytes &snapshot, const RoutePreview &preview, Common::String &error) {
	error.clear();
	if (!active() || !validSnapshot(snapshot)) { error = "Cannot record this recovery state"; return false; }
	auto headers = scan(); auto entries = scan(true); const uint32 sequence = headers.empty() ? 1 : headers[0].sequence + 1;
	if (!sequence) { error = "Recovery sequence limit reached"; return false; }
	uint32 slot = 0;
	for (; slot < 4; ++slot) {
		bool protectedSlot = false;
		for (uint32 i = 0; i < MIN<uint32>(3, entries.size()); ++i) if (entries[i].slot == slot) protectedSlot = true;
		if (!protectedSlot) break;
	}
	RoutePreview metadata = preview; metadata.snapshotDigest.resize(16); memcpy(metadata.snapshotDigest.data(), snapshot.data() + 12, 16);
	Bytes encoded;
	if (!RoutePreviewCodec::encode(metadata, Common::String::format("%s:recovery:%u", _signature.c_str(), sequence), encoded)) { error = "Cannot encode recovery preview"; return false; }
	Common::OutSaveFile *output = _files->openForSaving(name(slot));
	if (!output) { error = "Cannot open recovery file"; return false; }
	output->writeUint32BE(MKTAG('D','F','R','Y')); output->writeUint32LE(1); output->writeUint32LE(sequence); output->writeUint32LE(encoded.size()); output->writeUint32LE(snapshot.size());
	bool good = output->write(encoded.data(), encoded.size()) == encoded.size() && output->write(snapshot.data(), snapshot.size()) == snapshot.size();
	output->finalize(); good = good && !output->err(); delete output;
	Common::InSaveFile *checked = good ? _files->openForLoading(name(slot)) : nullptr;
	RecoveryEntry recorded; uint32 checkedSize = 0;
	good = checked && header(*checked, slot, recorded, checkedSize) && recorded.sequence == sequence && checkedSize == snapshot.size() && verifySnapshot(*checked, recorded, checkedSize);
	delete checked;
	if (!good) { error = "Cannot finalize recovery file"; return false; }
	// Three older saves survive until the spare is finalized and read back.
	for (uint32 i = 2; i < entries.size(); ++i) if (entries[i].slot != slot) _files->removeSavefile(name(entries[i].slot));
	return true;
}
}
