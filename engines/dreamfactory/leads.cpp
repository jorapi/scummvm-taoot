/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/leads.h"
#include "dreamfactory/session.h"
#include "dreamfactory/routes.h"
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
JournalLead journalEndingHints(const Journal &journal, const LeadState &state, const RouteGraph &graph, bool retail, bool tour) {
	JournalLead result; result.id = "ending-hints"; result.title = _("Ending hints");
	const auto catalog = routeEndingCatalog(state.version, retail, tour);
	uint32 reached = 0;
	for (const auto &ending : catalog) {
		for (const auto &node : graph.nodes()) {
			if (node.kind == RouteNode::kEnding && node.entry.speaker == ending.speaker && node.entry.text == ending.text) { ++reached; break; }
		}
	}
	result.status = Common::U32String::format(_("Outcomes reached: %u / %u"), reached, uint32(catalog.size()));
	result.summary = _("These reminders use only dialogue recorded in this save and outcomes you have reached. They are the same for every unreached ending; no hidden ending is identified. Earlier checkpoints restore earlier knowledge, inventory and decisions.");
	if (catalog.empty()) return result;
	result.hints.push_back(reached ? _("Read the conclusions you have already reached in Flow. Compare their recorded results before choosing another return point.") : _("Continue your current story first. The Flow view counts unreached outcomes without revealing their details."));
	result.hints.push_back(_("To change an earlier action, return to a checkpoint before it happened. A later checkpoint still contains earlier item exchanges and object decisions. Read its saved inventory before restoring it."));
	struct Reminder { const char *file, *idents, *text, *item; };
	static const Reminder reminders[] = {
		{"penny1.pup", "penny1.007|penny1.013", _s("Penny asked you to find the Rubaiyat. Review what she has told you so far."), "rubaiyat"},
		{"penny1.pup", "penny1.014|penny1.080", _s("Penny has discussed a painting. Review her recorded instructions."), "painting"},
		{"penny1.pup", "penny1.056", _s("Penny asked you to knock on her door after finding the Rubaiyat."), "rubaiyat"},
		{"penny1.pup", "penny1.082", _s("Penny told you to leave the Rubaiyat with the Purser for safekeeping."), "rubaiyat"},
		{"penny1.pup", "penny1.115|penny1.130", _s("Penny told you to leave the painting with the Purser."), "painting"},
		{"penny1.pup", "penny1.031|penny1.033|penny1.129", _s("Penny stressed the importance of recovering the notebook."), "notebook"},
		{"penny2.pup", "penny2.18|penny2.19|penny2.20", _s("Penny said the Rubaiyat must be recovered and kept safe before the ship sinks."), "rubaiyat"},
		{"penny2.pup", "penny2.22|penny2.23", _s("Penny said to keep the painting safe, or recover it if it is missing."), "painting"},
		{"penny2.pup", "penny2.25", _s("Penny warned you not to let the necklace fall into the wrong hands."), ""},
		{"penny2.pup", "penny2.30", _s("Penny said you must recover the notebook for your Russian allies."), "notebook"},
		{"penny2.pup", "penny2.33|penny2.36", _s("Penny said to get the evidence safely off the ship. Check what you carry before you leave."), ""},
		{"penny2.pup", "penny2.42", _s("Penny offered to take the evidence if escape became impossible. Re-read that conversation before making a handoff."), ""},
		{"dpenny.pup", "dpen.17", _s("Penny asked you to bring back the photograph. Review the request and examine what you have found."), "photo1"},
		{"dpenny.pup", "dpen.16|dpen.33", _s("Penny suggested finding a photographer on board. Review the photographs you have already discovered."), ""},
		{"sonoma.pup", "sonoma.25|sonoma.28", _s("Sonoma proposed a trade for five sacred objects. Review his recorded instructions and the objects you carry."), ""},
		{"sonoma.pup", "sonoma.27", _s("Sonoma said the entrance would open once you found the five objects."), ""},
		{"marie.pup", "marie.156|marie.157", _s("Marie invited you to leave Diamondback with her. Return before your response if you want to reconsider that invitation."), ""}
	};
	EvidenceIndex disclosed;
	for (uint32 i = 0; i < journal.entries.size(); ++i) {
		const auto &entry = journal.entries[i];
		if (!entry.choices.empty() || entry.text.empty()) continue;
		const Common::String file = basename(entry.file); Common::String ident = entry.ident; ident.toLowercase();
		disclosed[file + "|" + ident] = true;
		for (const auto &reminder : reminders) {
			const bool dust = Common::String(reminder.file) == "sonoma.pup" || Common::String(reminder.file) == "marie.pup";
			const bool demo = Common::String(reminder.file) == "dpenny.pup";
			if (dust != (state.version == 1) || (!dust && demo == retail)) continue;
			if (file == reminder.file && listed(ident, reminder.idents)) {
				for (uint32 prior = 0; prior < result.evidence.size(); ++prior) {
					const auto &previous = journal.entries[result.evidence[prior]];
					if (basename(previous.file) == file && previous.ident.equalsIgnoreCase(ident)) { result.evidence.remove_at(prior); break; }
				}
				result.evidence.push_back(i); break;
			}
		}
	}
	for (const auto &reminder : reminders) {
		const bool dust = Common::String(reminder.file) == "sonoma.pup" || Common::String(reminder.file) == "marie.pup";
		const bool demo = Common::String(reminder.file) == "dpenny.pup";
		if (dust != (state.version == 1) || (!dust && demo == retail) || !known(disclosed, reminder.file, reminder.idents)) continue;
		if (Common::String(reminder.file) == "penny1.pup" &&
			((Common::String(reminder.item) == "rubaiyat" && known(disclosed, "penny2.pup", "penny2.18|penny2.19|penny2.20")) ||
			 (Common::String(reminder.item) == "painting" && known(disclosed, "penny2.pup", "penny2.22|penny2.23")) ||
			 (Common::String(reminder.item) == "notebook" && known(disclosed, "penny2.pup", "penny2.30")))) continue;
		Common::U32String hint = _(reminder.text);
		if (*reminder.item) hint += owner(state, reminder.item).equalsIgnoreCase(dust ? "stranger" : "frank") ? _(" You currently carry this item.") : _(" You do not currently carry this item. Its location is not disclosed here.");
		result.hints.push_back(hint);
	}
	if (result.evidence.empty()) result.hints.push_back(_("There are no recorded ending-related instructions in this save yet. Speak to the characters and keep the dialogue log enabled to collect reminders. Hidden flags and unseen instructions are never used to supply a solution."));
	return result;
}
bool auditEndingHints() {
	Journal journal; LeadState state; state.version = 4; RouteGraph graph;
	const auto empty = journalEndingHints(journal, state, graph, true, false);
	state.owners["realneck"] = "vlad"; state.owners["painting"] = "hack";
	if (empty.hints != journalEndingHints(journal, state, graph, true, false).hints || !empty.evidence.empty()) return false;
	JournalEntry entry; entry.file = "other.pup"; entry.ident = "penny2.25"; entry.text = "Heard line"; journal.add(entry);
	if (journalEndingHints(journal, state, graph, true, false).hints != empty.hints) return false;
	entry.file = "CD2:PUPPETS2:PENNY2.PUP"; entry.choices.push_back("Unchosen necklace reply"); journal.add(entry);
	if (journalEndingHints(journal, state, graph, true, false).hints != empty.hints) return false;
	entry.choices.clear(); journal.add(entry);
	journal.add(entry);
	auto knownHints = journalEndingHints(journal, state, graph, true, false);
	if (knownHints.evidence.size() != 1 || knownHints.hints.size() != 3 || !knownHints.hints.back().contains(Common::U32String(U"necklace"))) return false;
	entry.ident = "penny2.22"; journal.add(entry);
	state.owners["painting"] = "frank";
	if (!journalEndingHints(journal, state, graph, true, false).hints[2].contains(Common::U32String(U"currently carry"))) return false;
	state.owners["painting"] = "hack";
	if (!journalEndingHints(journal, state, graph, true, false).hints[2].contains(Common::U32String(U"do not currently carry")) || journalEndingHints(journal, state, graph, true, false).hints[2].contains(Common::U32String(U"hack"))) return false;
	entry.file = "PENNY1.PUP"; entry.ident = "penny1.115"; journal.add(entry);
	const auto updated = journalEndingHints(journal, state, graph, true, false);
	if (updated.hints.size() != 4 || updated.evidence.size() != 3) return false;
	for (const auto &hint : updated.hints) if (hint.contains(Common::U32String(U"Purser"))) return false;
	graph.recordScene(JournalEntry()); graph.recordEnding(routeEndingCatalog(4, true, false)[0]);
	graph.recordEnding(routeEndingCatalog(4, true, false)[0]);
	if (journalEndingHints(journal, state, graph, true, false).status != _("Outcomes reached: 1 / 8")) return false;
	journal.entries.clear();
	if (journalEndingHints(journal, state, graph, true, false).hints.size() != empty.hints.size()) return false;
	entry.file = "dpenny.pup"; entry.ident = "dpen.17"; journal.add(entry);
	if (journalEndingHints(journal, state, graph, true, false).evidence.size() || journalEndingHints(journal, state, graph, false, false).evidence.size() != 1) return false;
	state.version = 1; entry.file = "MARIE.PUP"; entry.ident = "marie.156"; journal.add(entry);
	if (journalEndingHints(journal, state, graph, false, false).evidence.size() != 1 || !journalEndingHints(journal, state, graph, false, false).hints.back().contains(Common::U32String(U"Marie"))) return false;
	return journalEndingHints(journal, state, graph, false, true).hints.empty();
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
