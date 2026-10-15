/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_RECOVERY_H
#define DREAMFACTORY_RECOVERY_H
#include "dreamfactory/routes.h"
namespace DreamFactory {
class Session;
struct RecoveryEntry {
	uint32 sequence = 0, slot = 0;
	RoutePreview preview;
};
class RecoveryStore {
	Common::SaveFileManager *_files = nullptr;
	Common::String _stem, _signature;
	Common::String name(uint32 slot) const;
	bool header(Common::SeekableReadStream &input, uint32 slot, RecoveryEntry &entry, uint32 &size) const;
	Common::Array<RecoveryEntry> scan(bool verify = false) const;
public:
	bool open(Common::SaveFileManager *files, const Common::String &target, const Common::String &signature);
	bool active() const { return _files != nullptr; }
	Common::Array<RecoveryEntry> list() const;
	bool write(const Bytes &snapshot, const RoutePreview &preview, Common::String &error);
	bool read(const RecoveryEntry &entry, Bytes &snapshot, Common::String &error) const;
};
RecoveryEntry openRecovery(Session &session);
}
#endif
