/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/journal.h"
#include "dreamfactory/controls.h"
#include "dreamfactory/session.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/tokenizer.h"
#include "engines/engine.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymapper.h"
#include "gui/dialog.h"
#include "gui/gui-manager.h"
#include "gui/message.h"
#include "gui/widget.h"
#include "gui/widgets/edittext.h"
#include "gui/widgets/list.h"
#include "gui/widgets/richtext.h"
#include "gui/widgets/scrollbar.h"
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
class JournalKeymapScope {
	Common::Keymap *_standard, *_journal;
	bool _standardEnabled, _journalEnabled;
public:
	JournalKeymapScope() {
		Common::Keymapper *mapper = g_system->getEventManager()->getKeymapper();
		_standard = mapper->getKeymap(Common::kGuiKeymapName);
		if (!_standard) { _standard = g_gui.getKeymap(); mapper->addGlobalKeymap(_standard); }
		_journal = mapper->getKeymap("dreamfactory-journal-ui");
		if (!_journal) {
			_journal = new Common::Keymap(Common::Keymap::kKeymapTypeGui, "dreamfactory-journal-ui", _("DreamFactory journal"));
			const char *ids[] = {"JNPLAY", "JNSTAR", "JNPREV", "JNNEXT", "JNREAD", "JNCLOSE", "JNBACK"};
			const char *labels[] = {_s("Replay selected line"), _s("Bookmark selected line"), _s("Previous journal view"), _s("Next journal view"), _s("Read entry details"), _s("Close journal"), _s("Close journal")};
			const char *inputs[] = {"JOY_A", "JOY_X", "JOY_LEFT_SHOULDER", "JOY_RIGHT_SHOULDER", "JOY_Y", "JOY_B", "JOY_BACK"};
			const uint32 commands[] = {kJournalReplay, kJournalStar, kJournalViewPrevious, kJournalViewNext, kJournalRead, kJournalClose, kJournalClose};
			for (uint32 i = 0; i < ARRAYSIZE(ids); ++i) {
				Common::Action *action = new Common::Action(ids[i], _(labels[i]));
				action->setCustomEngineActionEvent(commands[i]); action->addDefaultInputMapping(inputs[i]); _journal->addAction(action);
			}
			// Retain shared keyboard editing/navigation shortcuts while replacing pad bindings.
			for (const auto *old : _standard->getActions()) {
				Common::Action *action = new Common::Action(old->id, old->description);
				action->setEvent(old->event);
				if (old->shouldTriggerOnKbdRepeats()) action->allowKbdRepeats();
				for (const auto &binding : old->getDefaultInputMapping()) if (!binding.hasPrefix("JOY_")) action->addDefaultInputMapping(binding);
				_journal->addAction(action);
			}
			const char *navIds[] = {"JNUP", "JNDOWN", "JNLEFT", "JNRIGHT"};
			const char *bindings[] = {"JOY_UP", "JOY_DOWN", "JOY_LEFT", "JOY_RIGHT"};
			const char *axes[] = {"JOY_LEFT_STICK_Y-", "JOY_LEFT_STICK_Y+", "JOY_LEFT_STICK_X-", "JOY_LEFT_STICK_X+"};
			const char *names[] = {_s("Previous entry"), _s("Next entry"), _s("Previous view"), _s("Next view")};
			for (uint32 i = 0; i < 4; ++i) {
				Common::Action *action = new Common::Action(navIds[i], _(names[i]));
				action->setCustomEngineActionEvent(kJournalUp + i); action->addDefaultInputMapping(bindings[i]); action->addDefaultInputMapping(axes[i]);
				action->allowKbdRepeats(); _journal->addAction(action);
			}
			_journal->setEnabled(false); mapper->addGlobalKeymap(_journal);
		}
		_standardEnabled = _standard && _standard->isEnabled(); _journalEnabled = _journal->isEnabled();
		if (_standard) _standard->setEnabled(false);
		_journal->setEnabled(true);
	}
	~JournalKeymapScope() {
		if (_standard) _standard->setEnabled(_standardEnabled);
		_journal->setEnabled(_journalEnabled);
	}
};
class JournalNotice : public GUI::MessageDialog {
public:
	explicit JournalNotice(const Common::U32String &message) : GUI::MessageDialog(message) {}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START &&
			(event.customType == kJournalClose || event.customType == kJournalReplay || event.customType == kJournalRead)) close();
		else GUI::MessageDialog::handleOtherEvent(event);
	}
};
class Replay {
	Session &_session;
	Common::SharedPtr<AudioClip> _clip;
	Audio::SoundHandle _handle;
	const JournalEntry *_entry = nullptr;
public:
	explicit Replay(Session &session) : _session(session) {}
	~Replay() { g_system->getMixer()->stopHandle(_handle); }
	bool active() const { return g_system->getMixer()->isSoundHandleActive(_handle); }
	bool playing(const JournalEntry &entry) const { return _entry == &entry && active(); }
	void play(const JournalEntry &entry) {
		const bool wasPlaying = playing(entry);
		_entry = &entry;
		g_system->getMixer()->stopHandle(_handle);
		if (wasPlaying) return;
		if (!_session.journalAudio(entry, _clip)) {
			JournalNotice message(_("This entry has no available voice recording.")); message.runModal(); return;
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
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (event.customType == kJournalReplay && !_entry.ident.empty()) { _replay.play(_entry); return; }
			if (event.customType == kJournalClose) { close(); return; }
			if (event.customType == kJournalUp || event.customType == kJournalDown) { _text->handleMouseWheel(0, 0, event.customType == kJournalUp ? -1 : 1); return; }
		}
		GUI::Dialog::handleOtherEvent(event);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kReplay) _replay.play(_entry);
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
// A variable-height transcript: each entry keeps its speaker and wrapped text
// together, while drawing with the active ScummVM theme's fonts and colours.
class TranscriptWidget : public GUI::Widget, public GUI::CommandSender {
	struct Row { int top = 0, height = 0; };
	Session &_session;
	Replay &_replay;
	Common::Array<uint32> _indices;
	Common::Array<Row> _rows;
	GUI::ScrollBarWidget *_scroll;
	int _selected = -1, _position = 0, _total = 0;
	int _downY = 0, _lastY = 0, _pressed = -1;
	bool _dragging = false;
	int lineHeight() const { return g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal).getFontHeight() + 4; }
	int at(int y) const {
		for (uint32 i = 0; i < _rows.size(); ++i) if (y + _position >= _rows[i].top && y + _position < _rows[i].top + _rows[i].height) return i;
		return -1;
	}
	void scrollTo(int position) {
		_position = CLIP(position, 0, MAX(0, _total - _h));
		_scroll->_currentPos = _position; _scroll->recalc(); markAsDirty();
	}
	Common::U32StringArray wrappedText(const JournalEntry &entry) const {
		Common::U32String text = gameText(entry.text);
		if (!entry.choices.empty()) { text = _("You: "); text += entry.selected < 0 ? _("[No reply chosen]") : gameText(entry.text); }
		Common::U32StringArray lines;
		g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal).wordWrapText(text, MAX(20, _w - 48), lines);
		return lines;
	}
	void wrap() {
		_rows.clear(); _total = 0;
		for (uint32 index : _indices) {
			Row row; row.top = _total;
			const auto lines = wrappedText(_session.journal.entries[index]);
			row.height = lineHeight() * (MAX<uint32>(1, lines.size()) + 2) + 24;
			_total += row.height; _rows.push_back(row);
		}
		_scroll->setPos(_w - 16, 0); _scroll->setSize(16, _h);
		_scroll->_numEntries = _total; _scroll->_entriesPerPage = _h; _scroll->_singleStep = lineHeight() * 3;
		_scroll->setVisible(_total > _h); scrollTo(_position);
	}
protected:
	GUI::Widget *findWidget(int x, int y) override { if (x >= _w - 16 && _scroll->isVisible()) return _scroll; return this; }
	void drawWidget() override {
		using T = GUI::ThemeEngine;
		const Common::Rect viewport(_x, _y, _x + _w - 18, _y + _h);
		g_gui.theme()->drawWidgetBackground(Common::Rect(_x, _y, _x + _w, _y + _h), T::kWidgetBackgroundBorder);
		const Common::Rect oldClip = g_gui.theme()->swapClipRect(viewport.findIntersectingRect(g_gui.theme()->getClipRect()));
		if (_rows.empty()) g_gui.theme()->drawText(Common::Rect(_x + 12, _y + 12, _x + _w - 24, _y + 40), _("No recorded dialogue in this view."), T::kStateEnabled, Graphics::kTextAlignLeft);
		for (uint32 i = 0; i < _rows.size(); ++i) {
			const Row &row = _rows[i]; const int top = _y + row.top - _position;
			if (top >= _y + _h) break;
			if (top + row.height <= _y) continue;
			const JournalEntry &entry = _session.journal.entries[_indices[i]];
			Common::U32String name(entry.bookmarked ? "* " : ""); name += gameText(entry.speaker);
			const auto inversion = int(i) == _selected ? T::kTextInversionFocus : T::kTextInversionNone;
			g_gui.theme()->drawText(Common::Rect(_x + 8, top + 8, _x + _w - 94, top + 8 + lineHeight()), name, T::kStateEnabled, Graphics::kTextAlignLeft, inversion, 0, true, T::kFontStyleBold, T::kFontColorNormal, false, viewport);
			if (!entry.ident.empty()) g_gui.theme()->drawButton(Common::Rect(_x + _w - 88, top + 4, _x + _w - 24, top + lineHeight() + 12), _replay.playing(entry) ? _("Stop") : _("Play"));
			g_gui.theme()->drawText(Common::Rect(_x + 12, top + lineHeight() + 10, _x + _w - 24, top + lineHeight() * 2 + 10), entryContext(entry), T::kStateEnabled, Graphics::kTextAlignLeft, T::kTextInversionNone, 0, true, T::kFontStyleNormal, T::kFontColorAlternate, false, viewport);
			const auto lines = wrappedText(entry);
			for (uint32 j = 0; j < lines.size(); ++j) {
				const int y = top + 16 + (j + 2) * lineHeight();
				g_gui.theme()->drawText(Common::Rect(_x + 12, y, _x + _w - 24, y + lineHeight()), lines[j], T::kStateEnabled, Graphics::kTextAlignLeft, T::kTextInversionNone, 0, false, T::kFontStyleNormal, T::kFontColorNormal, false, viewport);
			}
			g_gui.theme()->drawLineSeparator(Common::Rect(_x + 12, top + row.height - 3, _x + _w - 24, top + row.height - 1));
		}
		g_gui.theme()->swapClipRect(oldClip);
	}
public:
	TranscriptWidget(GUI::Dialog *boss, Session &session, Replay &replay) : GUI::Widget(boss, 8, 80, 300, 120), GUI::CommandSender(boss), _session(session), _replay(replay) {
		setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_CLEARBG | GUI::WIDGET_TRACK_MOUSE | GUI::WIDGET_HOOK_DRAG);
		_scroll = new GUI::ScrollBarWidget(this, 284, 0, 16, 120); _scroll->setTarget(this);
	}
	bool wantsFocus() override { return true; }
	bool containsWidget(GUI::Widget *widget) const override { return widget == _scroll; }
	int getSelected() const { return _selected; }
	void setEntries(const Common::Array<uint32> &indices) { _indices = indices; _selected = -1; _position = 0; wrap(); }
	void setSelected(int index) {
		_selected = index >= 0 && uint32(index) < _rows.size() ? index : -1;
		if (_selected >= 0) {
			const Row &row = _rows[_selected];
			if (row.top < _position || row.top + MIN(row.height, int(_h)) > _position + _h) scrollTo(row.top);
		}
		markAsDirty(); sendCommand(GUI::kListSelectionChangedCmd, _selected);
	}
	void reflowLayout() override { GUI::Widget::reflowLayout(); wrap(); }
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override { if (cmd == GUI::kSetPositionCmd) scrollTo(data); }
	void handleMouseWheel(int x, int y, int direction) override { scrollTo(_position + direction * lineHeight() * 3); }
	void handleMouseDown(int x, int y, int button, int clicks) override { _downY = _lastY = y; _pressed = at(y); _dragging = false; }
	void handleMouseMoved(int x, int y, int button) override {
		if (_pressed < 0) return;
		if (ABS(y - _downY) > 6) _dragging = true;
		if (_dragging) scrollTo(_position + _lastY - y);
		_lastY = y;
	}
	void handleMouseUp(int x, int y, int button, int clicks) override {
		if (!_dragging && _pressed >= 0 && _pressed == at(y)) {
			setSelected(_pressed);
			if (!_session.journal.entries[_indices[_pressed]].ident.empty() && x >= _w - 88 && x < _w - 24 && y + _position - _rows[_pressed].top < lineHeight() + 12) sendCommand(kReplay, 0);
			else if (clicks == 2) sendCommand(kRead, 0);
		}
		_pressed = -1; _dragging = false;
	}
	void cancelDrag() override { _pressed = -1; _dragging = false; }
	bool handleKeyDown(Common::KeyState key) override {
		switch (key.keycode) {
		case Common::KEYCODE_UP: setSelected(MAX(0, _selected - 1)); return true;
		case Common::KEYCODE_DOWN: setSelected(MIN(int(_rows.size()) - 1, _selected + 1)); return true;
		case Common::KEYCODE_HOME: setSelected(0); return true;
		case Common::KEYCODE_END: setSelected(int(_rows.size()) - 1); return true;
		case Common::KEYCODE_PAGEUP: scrollTo(_position - _h + lineHeight()); return true;
		case Common::KEYCODE_PAGEDOWN: scrollTo(_position + _h - lineHeight()); return true;
		case Common::KEYCODE_RETURN: sendCommand(kRead, 0); return true;
		case Common::KEYCODE_SPACE: sendCommand(kReplay, 0); return true;
		default: return false;
		}
	}
};
class JournalDialog : public GUI::Dialog {
	Session &_session;
	Replay _replay;
	GUI::StaticTextWidget *_title, *_searchLabel, *_speaker, *_context, *_status;
	GUI::RichTextWidget *_preview = nullptr;
	int32 _previewIndex = -2;
	bool _wide = false, _wasPlaying = false;
	GUI::EditTextWidget *_search;
	TranscriptWidget *_list;
	GUI::ButtonWidget *_tabs[3], *_buttons[4];
	Common::Array<uint32> _indices;
	uint32 _mode = kLines;
	void refresh() {
		const int old = _list->getSelected();
		const int32 previous = old >= 0 && uint32(old) < _indices.size() ? int32(_indices[old]) : -1;
		_indices.clear();
		for (uint32 i = 0; i < _session.journal.entries.size(); ++i) {
			const JournalEntry &entry = _session.journal.entries[i];
			if ((_mode == kPaths && entry.choices.empty()) || (_mode == kStarred && !entry.bookmarked) ||
				(_mode == kLines && !entry.choices.empty() && entry.selected < 0)) continue;
			Common::U32String searchable = gameText(entry.speaker + " " + entry.room + " " + entry.text);
			for (const auto &choice : entry.choices) { searchable += ' '; searchable += gameText(choice); }
			searchable.toLowercase();
			Common::U32String query = _search->getEditString(); query.toLowercase();
			Common::U32StringTokenizer tokens(query);
			bool matches = true;
			while (!tokens.empty()) if (!searchable.contains(tokens.nextToken())) { matches = false; break; }
			if (matches) _indices.push_back(i);
		}
		_list->setEntries(_indices);
		int selected = _indices.empty() ? -1 : int(_indices.size()) - 1;
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
		Common::U32String status = Common::U32String::format(_("%u entries  |  Oldest to newest"), uint32(_indices.size()));
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
		_tabs[0] = new GUI::ButtonWidget(this, 8, 32, 95, 20, _("Log"), Common::U32String(), kLines);
		_tabs[1] = new GUI::ButtonWidget(this, 108, 32, 95, 20, _("Decisions"), Common::U32String(), kPaths);
		_tabs[2] = new GUI::ButtonWidget(this, 208, 32, 95, 20, _("Starred"), Common::U32String(), kStarred);
		_search = new GUI::EditTextWidget(this, 8, 56, 300, 20, Common::U32String(), _("Search recorded dialogue"), kSearch);
		_list = new TranscriptWidget(this, session, _replay);
		const Common::U32String labels[] = {_("Read"), _("Replay"), _("Star"), _("Close")};
		const uint32 commands[] = {kRead, kReplay, kStar, GUI::kCloseCmd};
		for (uint32 i = 0; i < 4; ++i) _buttons[i] = new GUI::ButtonWidget(this, 8 + i * 76, 208, 70, 24, labels[i], Common::U32String(), commands[i]);
		refresh();
	}
	void open() override { GUI::Dialog::open(); setFocusWidget(_list); }
	void reflowLayout() override {
		fit(*this);
		const int row = MAX(24, g_gui.getFont().getFontHeight() + 12);
		_wide = _mode == kPaths && _w >= 600 && _h >= 360;
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
		const bool playing = _replay.active();
		if (playing != _wasPlaying) { _wasPlaying = playing; _list->markAsDirty(); }
		_buttons[1]->setLabel(index >= 0 && _replay.playing(_session.journal.entries[index]) ? _("Stop") : _("Replay"));
	}

	void switchView(int direction) {
		const uint32 modes[] = {kLines, kPaths, kStarred};
		int current = _mode == kLines ? 0 : _mode == kPaths ? 1 : 2;
		handleCommand(nullptr, modes[(current + direction + 3) % 3], 0);
	}
	void handleKeyDown(Common::KeyState key) override {
		if (getFocusWidget() != _search && (key.keycode == Common::KEYCODE_LEFT || key.keycode == Common::KEYCODE_RIGHT)) switchView(key.keycode == Common::KEYCODE_LEFT ? -1 : 1);
		else GUI::Dialog::handleKeyDown(key);
	}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (event.customType == kJournalClose) { close(); return; }
			if (event.customType == kJournalRead) { handleCommand(nullptr, kRead, 0); return; }
			if (event.customType == kJournalUp || event.customType == kJournalDown) { setFocusWidget(_list); _list->handleKeyDown(Common::KeyState(event.customType == kJournalUp ? Common::KEYCODE_UP : Common::KEYCODE_DOWN)); return; }
			if (event.customType == kJournalLeft || event.customType == kJournalRight) { switchView(event.customType == kJournalLeft ? -1 : 1); return; }
			if (event.customType == kJournalReplay) { handleCommand(nullptr, kReplay, 0); return; }
			if (event.customType == kJournalStar) { handleCommand(nullptr, kStar, 0); return; }
			if (event.customType == kJournalViewPrevious || event.customType == kJournalViewNext) { switchView(event.customType == kJournalViewPrevious ? -1 : 1); return; }
		}
		GUI::Dialog::handleOtherEvent(event);
	}

	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		const int32 index = selected();
		if (cmd == kLines || cmd == kPaths || cmd == kStarred) { _mode = cmd; refresh(); reflowLayout(); g_gui.scheduleTopDialogRedraw(); setFocusWidget(_list); }
		else if (cmd == kSearch) refresh();
		else if (cmd == GUI::kListSelectionChangedCmd) updateButtons();
		else if ((cmd == kRead || cmd == GUI::kListItemActivatedCmd) && index >= 0) {
			EntryDialog detail(_session.journal.entries[index], _replay); detail.runModal();
		} else if ((cmd == kReplay || cmd == GUI::kListItemDoubleClickedCmd) && index >= 0 && !_session.journal.entries[index].ident.empty()) { _replay.play(_session.journal.entries[index]); _list->markAsDirty(); }
		else if (cmd == kStar && index >= 0) { _session.journal.entries[index].bookmarked = !_session.journal.entries[index].bookmarked; refresh(); }
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
}
void openJournal(Session &session) {
	PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken();
	JournalKeymapScope keys;
	JournalDialog dialog(session);
	dialog.runModal();
}
}
