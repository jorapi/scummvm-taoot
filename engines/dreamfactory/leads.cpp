/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/leads.h"
#include "dreamfactory/session.h"
#include "common/tokenizer.h"
#include "common/translation.h"
namespace DreamFactory {
namespace {
Common::String basename(Common::String file) {
	file.toLowercase();
	for (int32 i = int32(file.size()) - 1; i >= 0; --i)
		if (file[i] == ':' || file[i] == '/' || file[i] == '\\') return file.substr(i + 1);
	return file;
}
bool listed(const Common::String &ident, const char *idents) {
	Common::StringTokenizer tokens(idents, "|");
	while (!tokens.empty()) if (ident.equalsIgnoreCase(tokens.nextToken())) return true;
	return false;
}
typedef Common::HashMap<Common::String, bool> EvidenceIndex;
bool known(const EvidenceIndex &index, const char *file, const char *idents) {
	Common::StringTokenizer tokens(idents, "|");
	while (!tokens.empty()) if (index.contains(Common::String(file) + "|" + tokens.nextToken())) return true;
	return false;
}
Common::String owner(const LeadState &state, const char *item) {
	return state.owners.contains(item) ? state.owners[item] : Common::String();
}
struct Topic {
	const char *id, *file, *unlock, *evidence, *title, *summary, *item;
};
}
Common::Array<JournalLead> journalLeads(const Journal &journal, const LeadState &state) {
	// Only heard lines in this save unlock topics. Profile-wide route and heard
	// history deliberately cannot introduce information from another playthrough.
	static const Topic topics[] = {
		{"rubaiyat", "penny1.pup", "penny1.007|penny1.013|penny1.042|penny1.044|penny1.053|penny1.056|penny1.063|penny1.075", "penny1.007|penny1.013|penny1.042|penny1.044|penny1.053|penny1.056|penny1.063|penny1.075|penny1.082", _s("The Rubaiyat"), _s("Penny has discussed the Rubaiyat with you. Review what she has told you so far."), "rubaiyat"},
		{"painting", "penny1.pup", "penny1.014|penny1.022|penny1.080|penny1.084|penny1.115|pennyfix1", "penny1.014|penny1.022|penny1.080|penny1.082|penny1.083|penny1.084|penny1.115|penny1.130|pennyfix1", _s("The painting"), _s("Penny has asked you to investigate a painting. The notes below contain only what you have heard."), "painting"},
		{"notebook", "penny1.pup", "penny1.031|penny1.033|penny1.120|penny1.129", "penny1.031|penny1.033|penny1.120|penny1.121|penny1.129", _s("The notebook"), _s("Penny has discussed a notebook. Follow the information already in your notes."), "notebook"},
		{"photograph", "dpenny.pup", "dpen.16|dpen.17|dpen.22|dpen.33|dpen.36", "dpen.11|dpen.12|dpen.13|dpen.14|dpen.15|dpen.16|dpen.17|dpen.22|dpen.33|dpen.36|dpen.37", _s("Conkling's photograph"), _s("Penny wants evidence about Conkling's partner. Review her request and the photographs you have found."), "photo1"},
		{"ring", "jones.pup", "jones.136|jones.137", "jones.134|jones.135|jones.136|jones.137", _s("A ring for Jones"), _s("Jones has asked for help finding an engagement ring."), "ring"},
		{"objects", "sonoma.pup", "sonoma.25|sonoma.28|sonoma.33", "sonoma.25|sonoma.26|sonoma.27|sonoma.28|sonoma.29|sonoma.30|sonoma.31|sonoma.32|sonoma.33|sonoma.34", _s("Sonoma's request"), _s("Sonoma has proposed a search for five sacred objects. Only his disclosed instructions appear below."), ""},
		{"sleep", "help1.pup", "help.21|help.19k", "help.21|help.19k", _s("A place to sleep"), _s("You have been advised to find somewhere to spend the night."), ""}
	};
	Common::Array<JournalLead> leads;
	Common::StringArray files; EvidenceIndex disclosed;
	for (const auto &entry : journal.entries) {
		files.push_back(basename(entry.file));
		if (entry.choices.empty() && !entry.text.empty()) { Common::String ident = entry.ident; ident.toLowercase(); disclosed[files.back() + "|" + ident] = true; }
	}
	for (const auto &topic : topics) {
		const bool dust = Common::String(topic.file) == "jones.pup" || Common::String(topic.file) == "sonoma.pup" || Common::String(topic.file) == "help1.pup";
		if ((state.version == 1) != dust || !known(disclosed, topic.file, topic.unlock)) continue;
		JournalLead lead; lead.id = topic.id; lead.title = _(topic.title); lead.summary = _(topic.summary); lead.status = _("Open lead");
		for (uint32 i = 0; i < journal.entries.size(); ++i) {
			const auto &entry = journal.entries[i];
			if (entry.choices.empty() && !entry.text.empty() && files[i] == topic.file && listed(entry.ident, topic.evidence)) {
				for (uint32 prior = 0; prior < lead.evidence.size(); ++prior) {
					const auto &previous = journal.entries[lead.evidence[prior]];
					if (previous.ident.equalsIgnoreCase(entry.ident) && previous.text == entry.text) { lead.evidence.remove_at(prior); break; }
				}
				lead.evidence.push_back(i);
			}
		}
		lead.hints.push_back(_("Read the recorded request again, or replay its original voice line."));
		const Common::String heldBy = owner(state, topic.item);
		if (*topic.item && heldBy.equalsIgnoreCase(dust ? "stranger" : "frank")) {
			lead.status = _("Item carried");
			lead.hints.push_back(_("You are carrying the relevant item. Examine it, then consider the request you were given."));
		}
		if (lead.id == "rubaiyat") {
			if (known(disclosed, topic.file, "penny1.053")) lead.hints.push_back(_("Penny suggested checking whether Zeitel sent or received telegrams."));
			if (known(disclosed, topic.file, "penny1.056")) lead.hints.push_back(_("Penny asked you to knock on her door after finding the book."));
			if (heldBy.equalsIgnoreCase("purser")) lead.status = _("Left with the Purser");
		} else if (lead.id == "painting") {
			if (known(disclosed, topic.file, "penny1.014|penny1.084")) lead.hints.push_back(_("Penny suggested asking the Purser about the painting."));
			if (known(disclosed, topic.file, "penny1.022|pennyfix1")) lead.hints.push_back(_("Penny pointed you toward the Purser's cargo manifest."));
			if (heldBy.equalsIgnoreCase("purser")) lead.status = _("Left with the Purser");
		} else if (lead.id == "photograph") {
			if (known(disclosed, topic.file, "dpen.16|dpen.33")) lead.hints.push_back(_("Penny suggested looking for a photographer on board."));
			if (known(disclosed, topic.file, "dpen.17")) lead.hints.push_back(_("Penny asked you to bring back the photograph."));
			if (known(disclosed, topic.file, "dpen.22")) lead.status = _("Reported to Penny");
		} else if (lead.id == "ring") {
			if (state.ringPromised) lead.status = heldBy.equalsIgnoreCase("stranger") ? _("Promise / item carried") : _("Promise made");
			if (state.day == 3 && heldBy.equalsIgnoreCase("jones")) lead.status = _("Delivered to Jones");
		} else if (lead.id == "objects") {
			if (known(disclosed, topic.file, "sonoma.28")) lead.hints.push_back(_("Sonoma said all five objects must be in your possession."));
			if (known(disclosed, topic.file, "sonoma.29|sonoma.30")) lead.hints.push_back(_("Sonoma mentioned a book about the Yunni."));
			if (known(disclosed, topic.file, "sonoma.31")) lead.hints.push_back(_("Sonoma suggested helping Jones."));
			if (known(disclosed, topic.file, "sonoma.34")) lead.hints.push_back(_("Sonoma directed you to the old schoolroom."));
		} else if (lead.id == "sleep" && state.day > 1) lead.status = _("Earlier lead");
		leads.push_back(lead);
	}
	for (uint32 i = 0; i < journal.entries.size(); ++i) {
		const auto &entry = journal.entries[i];
		if (!entry.bookmarked) continue;
		JournalLead note; note.id = Common::String::format("note-%u", i); note.title = _("Tracked note"); note.summary = Common::U32String(entry.text, Common::kWindows1252); note.status = _("Player tracked"); note.evidence.push_back(i);
		note.hints.push_back(_("Review this recorded line and its original context. You can stop tracking it from the dialogue log.")); leads.push_back(note);
	}
	return leads;
}
LeadState journalLeadState(const Session &session) {
	LeadState state; state.version = session.version;
	if (session.runtime.globals.contains("day")) state.day = session.runtime.globals["day"].number();
	if (session.runtime.globals.contains("jonesringstory")) state.ringPromised = session.runtime.globals["jonesringstory"].truth();
	for (const auto &prop : session.propSystem.props) { Common::String name = prop->name; name.toLowercase(); state.owners[name] = prop->owner.text(); }
	return state;
}
bool auditJournalLeads() {
	Journal history; LeadState state; state.version = 2;
	state.owners["notebook"] = "frank";
	if (!journalLeads(history, state).empty()) return false;
	auto add = [&](const char *file, const char *ident) { JournalEntry entry; entry.file = file; entry.ident = ident; entry.text = "Recorded evidence"; history.add(entry); };
	add("CD1:DATA:PENNY1.PUP", "penny1.007");
	auto notes = journalLeads(history, state);
	if (notes.size() != 1 || notes[0].id != "rubaiyat" || notes[0].hints.size() != 1) return false;
	add("other.pup", "penny1.053");
	if (journalLeads(history, state)[0].hints.size() != 1) return false;
	add("penny1.pup", "penny1.053");
	if (journalLeads(history, state)[0].hints.size() != 2) return false;
	state.owners["rubaiyat"] = "frank";
	if (journalLeads(history, state)[0].status != _("Item carried")) return false;
	state.owners["rubaiyat"] = "purser";
	if (journalLeads(history, state)[0].status != _("Left with the Purser")) return false;
	history.entries.clear();
	if (!journalLeads(history, state).empty()) return false;
	state.version = 1; state.day = 3; state.ringPromised = true;
	add("JONES.PUP", "jones.136");
	if (journalLeads(history, state)[0].status != _("Promise made")) return false;
	state.owners["ring"] = "jones";
	if (journalLeads(history, state)[0].status != _("Delivered to Jones")) return false;
	state.owners["ring"] = "stranger";
	if (journalLeads(history, state)[0].status != _("Promise / item carried")) return false;
	history.entries.clear(); state.version = 2;
	add("DPENNY.PUP", "dpen.14");
	if (!journalLeads(history, state).empty()) return false;
	add("DPENNY.PUP", "dpen.17");
	notes = journalLeads(history, state);
	if (notes.size() != 1 || notes[0].id != "photograph" || notes[0].hints.size() != 2) return false;
	add("DPENNY.PUP", "dpen.22");
	if (journalLeads(history, state)[0].status != _("Reported to Penny")) return false;
	history.entries[0].bookmarked = true;
	if (journalLeads(history, state).size() != 2) return false;
	history.entries[0].bookmarked = false;
	return journalLeads(history, state).size() == 1;
}
}
