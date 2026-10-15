/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/journal.h"
#include "dreamfactory/session.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/tokenizer.h"
#include "engines/engine.h"
#include "gui/dialog.h"
#include "gui/gui-manager.h"
#include "gui/message.h"
#include "gui/widget.h"
#include "gui/widgets/edittext.h"
#include "gui/widgets/list.h"
#include "gui/widgets/richtext.h"
#include "graphics/font.h"

namespace DreamFactory {
bool Session::journalAudio(const JournalEntry &entry, Common::SharedPtr<AudioClip> &clip) {
	clip.reset();
	if (entry.file.empty() || entry.ident.empty()) return false;
	Bytes bytes;
	Puppet puppet;
	if (!resources.read(entry.file, bytes) || !puppet.load(bytes)) return false;
	const int32 line = puppet.line(entry.ident);
	if (line < 0) return false;
	Common::SharedPtr<AudioClip> decoded(new AudioClip());
	if (!decodeAudio(puppet.archive.get(puppet.lines[line].audio), *decoded) || !decoded->rate || decoded->samples.empty()) return false;
	clip = decoded;
	return true;
}
namespace {
enum { kRead = 'JRed', kReplay = 'JPly', kStar = 'JStr', kSearch = 'JSrc', kLines = 'JLin', kPaths = 'JPth', kStarred = 'JFvt' };
Common::U32String gameText(const Common::String &text) { return Common::U32String(text, Common::kWindows1252); }
Common::U32String literal(const Common::String &text) {
	Common::U32String result;
	for (auto c : gameText(text)) {
		if (c == '\\' || c == '*' || c == '_' || c == '[' || c == ']' || c == '#' || c == '<' || c == '>' || c == '`') result += '\\';
		result += c;
	}
	return result;
}
void fit(GUI::Dialog &dialog) {
	const int w = MIN<int>(760, g_system->getOverlayWidth() - 12);
	const int h = MIN<int>(560, g_system->getOverlayHeight() - 12);
	dialog.resize((g_system->getOverlayWidth() - w) / 2, (g_system->getOverlayHeight() - h) / 2, w, h, false);
}
void place(GUI::Widget *widget, int x, int y, int w, int h) { widget->setPos(x, y); widget->setSize(w, h); }
Common::U32String entryBody(const JournalEntry &entry) {
	if (entry.choices.empty()) return literal(entry.text);
	Common::U32String text = _("Replies offered at this decision:");
	for (uint32 i = 0; i < entry.choices.size(); ++i) {
		text += U"\n\n";
		if (int32(i) == entry.selected) text += U"**";
		text += int32(i) == entry.selected ? _("Chosen reply") : _("Other reply");
		if (int32(i) == entry.selected) text += U"**";
		text += U"\n\n";
		text += literal(entry.choices[i]);
	}
	if (entry.selected < 0) text += _("\n\nNo reply was chosen.");
	return text;
}
Common::U32String entryContext(const JournalEntry &entry) {
	const uint64 seconds = entry.time / 1000;
	Common::String time = Common::String::format("%02u:%02u:%02u", uint32(seconds / 3600), uint32(seconds / 60 % 60), uint32(seconds % 60));
	return gameText((entry.room.empty() ? Common::String() : entry.room + "  |  ") + time);
}
class Replay {
	Session &_session;
	Common::SharedPtr<AudioClip> _clip;
	Audio::SoundHandle _handle;
	const JournalEntry *_entry = nullptr;
public:
	explicit Replay(Session &session) : _session(session) {}
	~Replay() { g_system->getMixer()->stopHandle(_handle); }
	bool playing(const JournalEntry &entry) const { return _entry == &entry && g_system->getMixer()->isSoundHandleActive(_handle); }
	void play(const JournalEntry &entry) {
		const bool wasPlaying = playing(entry);
		_entry = &entry;
		g_system->getMixer()->stopHandle(_handle);
		if (wasPlaying) return;
		if (!_session.journalAudio(entry, _clip)) {
			GUI::MessageDialog message(_("This entry has no available voice recording.")); message.runModal(); return;
		}
		g_system->getMixer()->playStream(Audio::Mixer::kSpeechSoundType, &_handle, makePCMStream(_clip.get()));
	}
};
class EntryDialog : public GUI::Dialog {
	GUI::StaticTextWidget *_title;
	GUI::RichTextWidget *_text;
	GUI::ButtonWidget *_play, *_close;
	const JournalEntry &_entry;
	Replay &_replay;
public:
	EntryDialog(const JournalEntry &entry, Replay &replay) : GUI::Dialog(0, 0, 320, 240), _entry(entry), _replay(replay) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain;
		Common::U32String text = entryContext(entry);
		text += U"\n\n";
		text += entryBody(entry);
		_title = new GUI::StaticTextWidget(this, 8, 8, 300, 20, gameText(entry.speaker), Graphics::kTextAlignLeft);
		_text = new GUI::RichTextWidget(this, 8, 32, 300, 160, text);
		_play = new GUI::ButtonWidget(this, 8, 208, 140, 24, _("Replay"), Common::U32String(), kReplay);
		_play->setEnabled(!entry.ident.empty());
		_close = new GUI::ButtonWidget(this, 164, 208, 140, 24, _("Back"), Common::U32String(), GUI::kCloseCmd);
	}
	void reflowLayout() override {
		fit(*this);
		const int row = MAX(20, g_gui.getFont().getFontHeight() + 8);
		place(_title, 8, 8, _w - 16, row);
		place(_text, 8, row + 12, _w - 16, MAX(20, _h - row * 2 - 28));
		place(_play, 8, _h - row - 8, (_w - 24) / 2, row);
		place(_close, _w / 2 + 4, _h - row - 8, (_w - 24) / 2, row);
		GUI::Dialog::reflowLayout();
	}
	void handleTickle() override {
		GUI::Dialog::handleTickle();
		_play->setLabel(_replay.playing(_entry) ? _("Stop") : _("Replay"));
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kReplay) _replay.play(_entry);
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
class JournalDialog : public GUI::Dialog {
	Session &_session;
	Replay _replay;
	GUI::StaticTextWidget *_title, *_searchLabel, *_speaker, *_context, *_status;
	GUI::RichTextWidget *_preview = nullptr;
	int32 _previewIndex = -2;
	bool _wide = false;
	GUI::EditTextWidget *_search;
	GUI::ListWidget *_list;
	GUI::ButtonWidget *_tabs[3], *_buttons[4];
	Common::Array<uint32> _indices;
	uint32 _mode = kLines;
	void refresh() {
		const int old = _list->getSelected();
		const int32 previous = old >= 0 && uint32(old) < _indices.size() ? int32(_indices[old]) : -1;
		Common::U32StringArray rows;
		_indices.clear();
		for (int32 i = int32(_session.journal.entries.size()) - 1; i >= 0; --i) {
			const JournalEntry &entry = _session.journal.entries[i];
			if ((_mode == kPaths && entry.choices.empty()) || (_mode == kStarred && !entry.bookmarked) ||
				(_mode == kLines && !entry.choices.empty() && entry.selected < 0)) continue;
			Common::U32String label(entry.bookmarked ? "* " : "");
			label += gameText(entry.speaker);
			label += entry.choices.empty() ? Common::U32String(": ") : _(" -> You: ");
			label += entry.text.empty() ? (entry.selected < 0 ? _("[No reply chosen]") : _("[Empty reply]")) : gameText(entry.text);
			Common::U32String searchable = gameText(entry.speaker + " " + entry.room + " " + entry.text);
			for (const auto &choice : entry.choices) { searchable += ' '; searchable += gameText(choice); }
			searchable.toLowercase();
			Common::U32String query = _search->getEditString(); query.toLowercase();
			Common::U32StringTokenizer tokens(query);
			bool matches = true;
			while (!tokens.empty()) if (!searchable.contains(tokens.nextToken())) { matches = false; break; }
			if (matches) { rows.push_back(label); _indices.push_back(i); }
		}
		_list->setList(rows);
		int selected = rows.empty() ? -1 : 0;
		for (uint32 i = 0; i < _indices.size(); ++i) if (int32(_indices[i]) == previous) selected = i;
		_list->setSelected(selected);
		for (uint32 i = 0; i < 3; ++i) _tabs[i]->setEnabled(_mode != (i == 0 ? kLines : i == 1 ? kPaths : kStarred));
		_previewIndex = -2;
		updateButtons();
	}
	int32 selected() const {
		const int index = _list->getSelected();
		return index >= 0 && uint32(index) < _indices.size() ? int32(_indices[index]) : -1;
	}
	void updateButtons() {
		const int32 index = selected();
		const JournalEntry *entry = index >= 0 ? &_session.journal.entries[index] : nullptr;
		_buttons[1]->setLabel(entry && _replay.playing(*entry) ? _("Stop") : _("Replay"));
		_buttons[2]->setLabel(entry && entry->bookmarked ? _("Unstar") : _("Star"));
		if (index != _previewIndex) {
			_previewIndex = index;
			_speaker->setLabel(entry ? gameText(entry->speaker) : _("Nothing to display"));
			_context->setLabel(entry ? entryContext(*entry) : Common::U32String());
			Common::U32String body = entry ? entryBody(*entry) : !_search->getEditString().empty() ? _("No matching entries. Try another search.") :
				_mode == kStarred ? _("Star a line to keep it here for later.") : _mode == kPaths ? _("Your encountered dialogue choices will appear here.") : _("Heard dialogue will appear here as you play.");
			// Replacing the reader resets its scroll position and cached Markdown surface.
			if (_preview) { removeWidget(_preview); delete _preview; }
			_preview = new GUI::RichTextWidget(this, 8, 80, 200, 100, body);
			layoutPreview();
			_preview->setVisible(_wide);
			if (isVisible()) _preview->reflowLayout();
		}
		Common::U32String status = Common::U32String::format(_("%u entries  |  Newest first"), uint32(_indices.size()));
		if (index >= 0) status += _("  |  Activate to read");
		_status->setLabel(status);
		_buttons[0]->setEnabled(index >= 0);
		_buttons[1]->setEnabled(index >= 0 && !_session.journal.entries[index].ident.empty());
		_buttons[2]->setEnabled(index >= 0);
	}
	void layoutPreview() {
		const int row = MAX(24, g_gui.getFont().getFontHeight() + 12);
		const int top = row * 3 + 24;
		const int left = 20 + (_w - 28) * 42 / 100;
		place(_speaker, left, top, _w - left - 12, row);
		place(_context, left, top + row, _w - left - 12, row);
		if (_preview) place(_preview, left, top + row * 2 + 8, MAX(20, _w - left - 12), MAX(20, _h - top - row * 4 - 28));
	}
public:
	explicit JournalDialog(Session &session) : GUI::Dialog(0, 0, 320, 240), _session(session), _replay(session) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain;
		_speaker = new GUI::StaticTextWidget(this, 8, 80, 200, 24, Common::U32String(), Graphics::kTextAlignLeft);
		_context = new GUI::StaticTextWidget(this, 8, 104, 200, 24, Common::U32String(), Graphics::kTextAlignLeft, Common::U32String(), GUI::ThemeEngine::kFontStyleNormal);
		_status = new GUI::StaticTextWidget(this, 8, 180, 300, 20, Common::U32String(), Graphics::kTextAlignLeft, Common::U32String(), GUI::ThemeEngine::kFontStyleNormal);
		_searchLabel = new GUI::StaticTextWidget(this, 8, 56, 56, 20, _("Search:"), Graphics::kTextAlignLeft);
		_title = new GUI::StaticTextWidget(this, 8, 8, 300, 20, _("Dialogue journal"), Graphics::kTextAlignLeft);
		_tabs[0] = new GUI::ButtonWidget(this, 8, 32, 95, 20, _("Journal"), Common::U32String(), kLines);
		_tabs[1] = new GUI::ButtonWidget(this, 108, 32, 95, 20, _("Decisions"), Common::U32String(), kPaths);
		_tabs[2] = new GUI::ButtonWidget(this, 208, 32, 95, 20, _("Starred"), Common::U32String(), kStarred);
		_search = new GUI::EditTextWidget(this, 8, 56, 300, 20, Common::U32String(), _("Search recorded dialogue"), kSearch);
		_list = new GUI::ListWidget(this, 8, 80, 300, 120);
		_list->setNumberingMode(GUI::kListNumberingOff); _list->setEditable(false);
		const Common::U32String labels[] = {_("Read"), _("Replay"), _("Star"), _("Close")};
		const uint32 commands[] = {kRead, kReplay, kStar, GUI::kCloseCmd};
		for (uint32 i = 0; i < 4; ++i) _buttons[i] = new GUI::ButtonWidget(this, 8 + i * 76, 208, 70, 24, labels[i], Common::U32String(), commands[i]);
		refresh();
	}
	void open() override { GUI::Dialog::open(); setFocusWidget(_list); }
	void reflowLayout() override {
		fit(*this);
		const int row = MAX(24, g_gui.getFont().getFontHeight() + 12);
		_wide = _w >= 600 && _h >= 360;
		place(_title, 12, 8, _w - 24, row);
		for (uint32 i = 0; i < 3; ++i) place(_tabs[i], 12 + i * (_w - 20) / 3, row + 12, (_w - 32) / 3, row);
		place(_searchLabel, 12, row * 2 + 16, 56, row);
		place(_search, 72, row * 2 + 16, _w - 84, row);
		const int top = row * 3 + 24;
		place(_list, 12, top, _wide ? (_w - 28) * 42 / 100 : _w - 24, MAX(20, _h - top - row * 2 - 20));
		layoutPreview();
		_speaker->setVisible(_wide); _context->setVisible(_wide);
		if (_preview) _preview->setVisible(_wide);
		place(_status, 12, _h - row * 2 - 12, _w - 24, row);
		for (uint32 i = 0; i < 4; ++i) place(_buttons[i], 12 + i * (_w - 20) / 4, _h - row - 8, (_w - 36) / 4, row);
		GUI::Dialog::reflowLayout();
	}
	void handleTickle() override {
		GUI::Dialog::handleTickle();
		const int32 index = selected();
		_buttons[1]->setLabel(index >= 0 && _replay.playing(_session.journal.entries[index]) ? _("Stop") : _("Replay"));
	}

	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		const int32 index = selected();
		if (cmd == kLines || cmd == kPaths || cmd == kStarred) { _mode = cmd; refresh(); }
		else if (cmd == kSearch) refresh();
		else if (cmd == GUI::kListSelectionChangedCmd) updateButtons();
		else if ((cmd == kRead || cmd == GUI::kListItemActivatedCmd) && index >= 0) {
			EntryDialog detail(_session.journal.entries[index], _replay); detail.runModal();
		} else if ((cmd == kReplay || cmd == GUI::kListItemDoubleClickedCmd) && index >= 0) _replay.play(_session.journal.entries[index]);
		else if (cmd == kStar && index >= 0) { _session.journal.entries[index].bookmarked = !_session.journal.entries[index].bookmarked; refresh(); }
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
}
void openJournal(Session &session) {
	PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken();
	JournalDialog dialog(session);
	dialog.runModal();
}
}
