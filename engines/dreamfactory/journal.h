/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_JOURNAL_H
#define DREAMFACTORY_JOURNAL_H
#include "dreamfactory/audio.h"
#include "common/str-array.h"
namespace DreamFactory {
class Session;
struct JournalEntry {
	Common::String speaker, room, file, ident, text;
	Common::StringArray choices;
	int32 selected = -1;
	bool bookmarked = false;
	uint64 time = 0;
};
class Journal {
public:
	Common::Array<JournalEntry> entries;
	void add(const JournalEntry &entry) {
		if (entries.size() == 16384) entries.remove_at(0);
		entries.push_back(entry);
	}
};
uint32 openJournal(Session &session);
void journalNotice(const Common::String &message);
}
#endif
