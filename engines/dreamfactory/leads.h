/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_LEADS_H
#define DREAMFACTORY_LEADS_H
#include "dreamfactory/journal.h"
#include "common/hashmap.h"
namespace DreamFactory {
struct LeadState {
	uint32 version = 0;
	int32 day = 0;
	bool ringPromised = false;
	Common::HashMap<Common::String, Common::String> owners;
};
struct JournalLead {
	Common::String id;
	Common::U32String title, summary, status;
	Common::U32StringArray hints;
	Common::Array<uint32> evidence;
};
Common::Array<JournalLead> journalLeads(const Journal &journal, const LeadState &state);
LeadState journalLeadState(const Session &session);
bool auditJournalLeads();
}
#endif
