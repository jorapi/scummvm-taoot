/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_AUDIO_H
#define DREAMFACTORY_AUDIO_H
#include "dreamfactory/archive.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/ptr.h"
namespace DreamFactory {
struct AudioClip {
	uint32 rate = 0;
	uint32 channels = 1;
	uint32 frames() const { return channels ? samples.size() / channels : 0; }
	Common::Array<int16> samples;
};
bool decodeAudio(const Bytes &data, AudioClip &clip);
bool joinAudio(const Archive &archive, const Common::Array<uint32> &order, AudioClip &out);
// Shared bank model, with separate v1 inline and v4 table readers.
class AudioBank {
public:
	bool load(const Bytes &data);
	bool clip(uint32 container, AudioClip &out) const { return decodeAudio(archive.get(container), out); }
	bool theme(AudioClip &out) const;
	Archive archive;
	uint32 version = 0;
	uint32 loopRepeat = 0;
	uint32 themeRecords = 0;
	Common::String name;
	Common::Array<uint32> loops;
	Common::Array<Common::String> soundNames;
	Common::HashMap<Common::String, uint32> singles;
	Common::HashMap<Common::String, uint32> soundIDs;
	// v1 music names exposed as effects by older engines; save migration only.
	Common::HashMap<Common::String, uint32> legacyMusic;
private:
	void addSound(Common::String name, uint32 container, uint32 id, bool firstMatch = false);
};
}
#endif
