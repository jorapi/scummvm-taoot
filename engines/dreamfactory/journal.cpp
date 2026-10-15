/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/journal.h"
#include "dreamfactory/leads.h"
#include "dreamfactory/navigation_ui.h"
#include "dreamfactory/controls.h"
#include "dreamfactory/session.h"
#include "common/system.h"
#include "common/config-manager.h"
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
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"

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
enum { kRead = 'JRed', kReplay = 'JPly', kStar = 'JStr', kSearch = 'JSrc', kLines = 'JLin', kPaths = 'JPth', kRoutes = 'JRte', kLeads = 'JLds', kShipMap = 'JMap', kHint = 'JHnt', kRouteSelect = 'JRSl', kRevisit = 'JRev', kRouteZoom = 'JRZm', kRouteCurrent = 'JRCr' };
Common::U32String gameText(const Common::String &text) { return Common::U32String(text, Common::kWindows1252); }
Common::U32String speakerText(const JournalEntry &entry) {
	Common::String name = entry.speaker; name.trim();
	return name.empty() || name.equalsIgnoreCase("untitled") ? _("Conversation") : gameText(name);
}
Common::U32String literal(const Common::U32String &text) {
	Common::U32String result;
	for (auto c : text) {
		if (c == '\\' || c == '*' || c == '_' || c == '[' || c == ']' || c == '#' || c == '<' || c == '>' || c == '`') result += '\\';
		result += c;
	}
	return result;
}
Common::U32String literal(const Common::String &text) { return literal(gameText(text)); }
void fit(GUI::Dialog &dialog) {
	const int w = MIN<int>(760, g_system->getOverlayWidth() - 12);
	const int h = MIN<int>(560, g_system->getOverlayHeight() - 12);
	dialog.resize((g_system->getOverlayWidth() - w) / 2, (g_system->getOverlayHeight() - h) / 2, w, h, false);
}
void place(GUI::Widget *widget, int x, int y, int w, int h) { widget->setPos(x, y); widget->setSize(w, h); }
void showPanel(GUI::Widget *widget, bool visible) {
	widget->setVisible(visible);
	// Shared GUI hit testing includes hidden widget rectangles.
	if (!visible) widget->setPos(-30000, -30000);
}
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
			const char *ids[] = {"JNPLAY", "JNSTAR", "JNPREV", "JNNEXT", "JNREAD", "JNCLOSE", "JNBACK", "JNZOOM", "JNCURRENT"};
			const char *labels[] = {_s("Replay line / read details"), _s("Track / hint / revisit / directions"), _s("Previous journal view"), _s("Next journal view"), _s("Read entry / zoom map"), _s("Close journal"), _s("Close journal"), _s("Zoom route / map"), _s("Focus current route / map position")};
			const char *inputs[] = {"JOY_A", "JOY_X", "JOY_LEFT_SHOULDER", "JOY_RIGHT_SHOULDER", "JOY_Y", "JOY_B", "JOY_BACK", "JOY_RIGHT_STICK", "JOY_LEFT_STICK"};
			const uint32 commands[] = {kJournalReplay, kJournalStar, kJournalViewPrevious, kJournalViewNext, kJournalRead, kJournalClose, kJournalClose, kJournalZoom, kJournalCurrent};
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
			const char *names[] = {_s("Navigate up"), _s("Navigate down"), _s("Navigate left"), _s("Navigate right")};
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
class CheckpointPicture : public GUI::Widget {
	Bytes _picture;
protected:
	void drawWidget() override {
		Graphics::ManagedSurface surface(_w, _h, g_system->getOverlayFormat());
		for (int y = 0; y < _h; ++y) for (int x = 0; x < _w; ++x) {
			const byte *pixel = _picture.data() + ((y * RoutePreview::kHeight / _h) * RoutePreview::kWidth + x * RoutePreview::kWidth / _w) * 3;
			surface.setPixel(x, y, surface.format.RGBToColor(pixel[0], pixel[1], pixel[2]));
		}
		g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
	}
public:
	CheckpointPicture(GUI::Dialog *boss, const Bytes &picture) : GUI::Widget(boss, 8, 32, 128, 96), _picture(picture) { setFlags(GUI::WIDGET_CLEARBG); }
};
Common::U32String checkpointBody(const RoutePreview &preview) {
	Common::U32String text = _("**Saved checkpoint**\n\nLocation: "); text += preview.location.empty() ? _("Unavailable") : literal(preview.location);
	text += _("\n\nStory time: "); text += preview.clock.empty() ? _("Unavailable") : literal(preview.clock);
	text += _("\n\nHeld item: "); text += preview.heldItem.empty() ? _("None") : literal(preview.heldItem);
	text += _("\n\n**Saved inventory / key items**\n\n");
	if (preview.inventory.empty()) text += _("No carried items recorded.");
	for (const auto &item : preview.inventory) {
		text += item.name == "realneck" ? _("Real necklace") : item.name == "rubaiyat" ? _("Rubaiyat") : item.name == "notebook" ? _("Notebook") : item.name == "painting" ? _("Painting") : literal(item.name);
		text += U": "; text += item.owner == "frank" || item.owner == "stranger" ? _("Carried") : item.owner.empty() || item.owner == "none" ? _("Not carried") : literal(item.owner); text += U"\n\n";
	}
	text += _("These are the saved checkpoint's items, not your current inventory.\n\n"); return text;
}
void showEndingHints(const JournalLead &lead, const Journal &journal, Replay &replay);
class EntryDialog : public GUI::Dialog {
	GUI::StaticTextWidget *_title;
	GUI::RichTextWidget *_text;
	GUI::ButtonWidget *_play, *_close, *_revisit = nullptr;
	CheckpointPicture *_picture = nullptr;
	bool _confirmRestore = false;
	bool _information = false;
	const JournalLead *_endingHints = nullptr;
	const Journal *_hintJournal = nullptr;
	const JournalEntry &_entry;
	Replay &_replay;
public:
	EntryDialog(const JournalEntry &entry, Replay &replay, const Common::U32String &body = Common::U32String(), bool route = false, bool canRevisit = false, const RoutePreview *preview = nullptr, bool confirmRestore = false) : GUI::Dialog(0, 0, 320, 240), _confirmRestore(confirmRestore), _entry(entry), _replay(replay) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain;
		Common::U32String text = entryContext(entry);
		text += U"\n\n";
		text += entryBody(entry);
		if (!body.empty()) text = body;
		if (preview) {
			text = checkpointBody(*preview) + text;
			if (preview->picture.size() == RoutePreview::kWidth * RoutePreview::kHeight * 3) _picture = new CheckpointPicture(this, preview->picture);
		} else if ((route || _confirmRestore) && canRevisit) text = _("A preview is unavailable for this checkpoint. Its original saved state can still be restored.\n\n") + text;
		if (_confirmRestore) text = _("Unsaved progress will be replaced. Manual saves and route discoveries are kept.\n\n") + text;
		_title = new GUI::StaticTextWidget(this, 8, 8, 300, 20, speakerText(entry), Graphics::kTextAlignLeft);
		_text = new GUI::RichTextWidget(this, 8, 32, 300, 160, text);
		_play = new GUI::ButtonWidget(this, 8, 208, 140, 24, _("Replay"), Common::U32String(), kReplay);
		_play->setEnabled(!entry.ident.empty());
		_close = new GUI::ButtonWidget(this, 164, 208, 140, 24, _("Back"), Common::U32String(), GUI::kCloseCmd);
		if (route) { _revisit = new GUI::ButtonWidget(this, 100, 208, 100, 24, _("Revisit"), Common::U32String(), kRevisit); _revisit->setEnabled(canRevisit); }
		if (_confirmRestore) { _title->setLabel(_("Restore this checkpoint?")); _play->setLabel(_("Restore checkpoint")); _play->setEnabled(true); _close->setLabel(_("Cancel")); setResult(GUI::kMessageAlt); }
	}
	void open() override {
		GUI::Dialog::open();
		if (_confirmRestore) setResult(GUI::kMessageAlt);
	}
	void information(const Common::U32String &title) { _information = true; _title->setLabel(title); showPanel(_play, false); }
	void endingHints(const JournalLead &lead, const Journal &journal, const Common::U32String &title) {
		_endingHints = &lead; _hintJournal = &journal; _play->setLabel(_("Hints")); _play->setEnabled(true); _title->setLabel(title);
	}
	void reflowLayout() override {
		fit(*this);
		const int row = MAX(20, g_gui.getFont().getFontHeight() + 8);
		place(_title, 8, 8, _w - 16, row);
		place(_text, 8, row + 12, _w - 16, MAX(20, _h - row * 2 - 28));
		if (_picture) {
			const int width = _w >= 600 ? 192 : 128;
			place(_picture, 8, row + 12, width, width * 3 / 4);
			place(_text, width + 16, row + 12, _w - width - 24, MAX(20, _h - row * 2 - 28));
		}
		place(_play, 8, _h - row - 8, (_w - 24) / 2, row);
		place(_close, _w / 2 + 4, _h - row - 8, (_w - 24) / 2, row);
		if (_revisit) {
			const int width = (_w - 32) / 3;
			place(_play, 8, _h - row - 8, width, row); place(_revisit, 16 + width, _h - row - 8, width, row); place(_close, 24 + width * 2, _h - row - 8, width, row);
		}
		if (_information) { showPanel(_play, false); place(_close, 8, _h - row - 8, _w - 16, row); }
		GUI::Dialog::reflowLayout();
	}
	void handleTickle() override {
		GUI::Dialog::handleTickle();
		if (!_confirmRestore && !_endingHints) _play->setLabel(_replay.playing(_entry) ? _("Stop") : _("Replay"));
	}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (_endingHints && event.customType == kJournalReplay) { showEndingHints(*_endingHints, *_hintJournal, _replay); return; }
			if (_confirmRestore && event.customType == kJournalReplay) { setResult(GUI::kMessageOK); close(); return; }
			if (event.customType == kJournalReplay && !_entry.ident.empty()) { _replay.play(_entry); return; }
			if (event.customType == kJournalClose) { close(); return; }
			if (event.customType == kJournalStar && _revisit && _revisit->isEnabled()) { setResult(kRevisit); close(); return; }
			if (event.customType == kJournalUp || event.customType == kJournalDown) { _text->handleMouseWheel(0, 0, event.customType == kJournalUp ? -1 : 1); return; }
		}
		GUI::Dialog::handleOtherEvent(event);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kReplay) { if (_endingHints) showEndingHints(*_endingHints, *_hintJournal, _replay); else if (_confirmRestore) { setResult(GUI::kMessageOK); close(); } else _replay.play(_entry); }
		else if (cmd == kRevisit && _revisit && _revisit->isEnabled()) { setResult(kRevisit); close(); }
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
// A variable-height transcript: each entry keeps its speaker and wrapped text
// together, with ScummVM fonts, a period palette and independent scrolling.
class TranscriptWidget : public GUI::Widget, public GUI::CommandSender {
	struct Row { int top = 0, height = 0; };
	const Journal *_journal;
	Replay &_replay;
	bool _showContext = true;
	Common::Array<uint32> _indices;
	Common::Array<Row> _rows;
	GUI::ScrollBarWidget *_scroll;
	int _selected = -1, _position = 0, _total = 0;
	int _downY = 0, _lastY = 0, _pressed = -1;
	bool _dragging = false, _showChoices = false;
	const Graphics::Font &bodyFont() const {
		const Graphics::Font &normal = g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal);
		const Graphics::Font *large = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		return normal.getFontHeight() < 12 && large ? *large : normal;
	}
	int lineHeight() const { return bodyFont().getFontHeight() + 4; }
	int at(int y) const {
		for (uint32 i = 0; i < _rows.size(); ++i) if (y + _position >= _rows[i].top && y + _position < _rows[i].top + _rows[i].height) return i;
		return -1;
	}
	void scrollTo(int position) {
		_position = CLIP(position, 0, MAX(0, _total - _h));
		_scroll->_currentPos = _position; _scroll->recalc(); markAsDirty();
	}
	Common::U32StringArray wrappedText(const JournalEntry &entry) const {
		Common::U32String text = _showContext ? gameText(entry.text) : Common::U32String(entry.text, Common::kUtf8);
		if (!entry.choices.empty()) { text = _("You: "); text += entry.selected < 0 ? _("[No reply chosen]") : gameText(entry.text); }
		if (_showChoices && !entry.choices.empty()) {
			text = _("Choices:");
			for (uint32 i = 0; i < entry.choices.size(); ++i) { text += i == uint32(entry.selected) ? U"\n> " : U"\n  "; text += gameText(entry.choices[i]); }
		}
		Common::U32StringArray lines;
		bodyFont().wordWrapText(text, MAX(20, _w - 48), lines);
		return lines;
	}
	void wrap() {
		_rows.clear(); _total = 0;
		for (uint32 index : _indices) {
			Row row; row.top = _total;
			const auto lines = wrappedText(_journal->entries[index]);
			row.height = lineHeight() * (MAX<uint32>(1, lines.size()) + 1) + 20;
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
		Graphics::ManagedSurface surface(MAX<int>(1, _w - 18), _h, g_system->getOverlayFormat());
		const uint32 background = surface.format.RGBToColor(17, 26, 34), ivory = surface.format.RGBToColor(240, 232, 211), brass = surface.format.RGBToColor(191, 161, 101), muted = surface.format.RGBToColor(146, 157, 166);
		surface.fillRect(Common::Rect(surface.w, surface.h), background);
		const auto rect = [&](int l, int t, int r, int b, uint32 ink) {
			l = MAX(0, l); t = MAX(0, t); r = MIN<int>(surface.w, r); b = MIN<int>(surface.h, b);
			if (r > l && b > t) surface.fillRect(Common::Rect(l, t, r, b), ink);
		};
		const Graphics::Font &font = bodyFont(), &bold = g_gui.getFont(T::kFontStyleBold);
		if (_rows.empty()) font.drawString(&surface, _showContext ? _("No recorded dialogue in this view.") : _("No discovered or tracked leads yet."), 12, 12, surface.w - 24, ivory, Graphics::kTextAlignLeft, 0, true);
		for (uint32 i = 0; i < _rows.size(); ++i) {
			const Row &row = _rows[i]; const int top = row.top - _position;
			if (top >= _h) break;
			if (top + row.height <= 0) continue;
			const JournalEntry &entry = _journal->entries[_indices[i]];
			Common::U32String name(entry.bookmarked ? "* " : ""); name += _showContext ? speakerText(entry) : Common::U32String(entry.speaker, Common::kUtf8);
			if (int(i) == _selected) { rect(4, top + 3, surface.w - 6, top + lineHeight() + 14, surface.format.RGBToColor(42, 56, 68)); rect(4, top + 3, 7, top + row.height - 6, brass); }
			const int nameWidth = _showContext ? MIN<int>(160, MAX(40, surface.w / 3)) : surface.w - 24;
			bold.drawString(&surface, name, 12, top + 8, nameWidth, brass, Graphics::kTextAlignLeft, 0, true);
			const int contextWidth = surface.w - nameWidth - 108;
			Common::U32String context = entryContext(entry);
			if (_w < 600) { JournalEntry compact = entry; compact.room.clear(); context = entryContext(compact); }
			if (_showContext && contextWidth >= 70) font.drawString(&surface, context, nameWidth + 24, top + 8, contextWidth, muted, Graphics::kTextAlignRight, 0, true);
			if (!entry.ident.empty()) {
				const int x = _w - 88, right = _w - 24, bottom = top + lineHeight() + 12;
				rect(x, top + 4, right, bottom, brass); rect(x + 1, top + 5, right - 1, bottom - 1, background);
				font.drawString(&surface, _replay.playing(entry) ? _("Stop") : _("Play"), x + 2, top + 8, right - x - 4, ivory, Graphics::kTextAlignCenter);
			}
			const auto lines = wrappedText(entry);
			for (uint32 j = 0; j < lines.size(); ++j) {
				const int y = top + 14 + (j + 1) * lineHeight();
				font.drawString(&surface, lines[j], 12, y, surface.w - 24, ivory);
			}
			rect(12, top + row.height - 3, surface.w - 12, top + row.height - 2, surface.format.RGBToColor(55, 66, 76));
		}
		g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
	}
public:
	TranscriptWidget(GUI::Dialog *boss, Session &session, Replay &replay) : GUI::Widget(boss, 8, 80, 300, 120), GUI::CommandSender(boss), _journal(&session.journal), _replay(replay) {
		setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_CLEARBG | GUI::WIDGET_TRACK_MOUSE | GUI::WIDGET_HOOK_DRAG);
		_scroll = new GUI::ScrollBarWidget(this, 284, 0, 16, 120); _scroll->setTarget(this);
	}
	bool wantsFocus() override { return true; }
	bool containsWidget(GUI::Widget *widget) const override { return widget == _scroll; }
	int getSelected() const { return _selected; }
	int scrollPosition() const { return _position; }
	void setJournal(const Journal &journal, bool context) { _journal = &journal; _showContext = context; _indices.clear(); _rows.clear(); _selected = -1; }
	void setEntries(const Common::Array<uint32> &indices, bool choices = false) { _indices = indices; _showChoices = choices; _selected = -1; _position = 0; wrap(); }
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
			if (!_journal->entries[_indices[_pressed]].ident.empty() && x >= _w - 88 && x < _w - 24 && y + _position - _rows[_pressed].top < lineHeight() + 12) sendCommand(kReplay, 0);
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
Common::U32String chapterTitle(uint32 chapter) {
	switch (chapter) {
	case 1: return _("The apartment");
	case 2: return _("First assignment");
	case 3: return _("Second assignment");
	case 4: return _("Third assignment");
	case 5: return _("The sinking");
	case 6: return _("Final evacuation");
	case 11: case 12: case 13: case 14: case 15: return Common::U32String::format(_("Day %u"), chapter - 10);
	case 21: return _("The briefing");
	case 22: return _("The investigation");
	default: return _("Return point");
	}
}
Common::U32String routeTitle(const RouteGraph &graph, const RouteMapItem &item) {
	if (!item.node) return item.ending ? Common::U32String::format(_("Ending %u - Unreached"), item.ending) : _("Undiscovered");
	const RouteNode &node = *graph.node(item.node);
	if (node.kind == RouteNode::kScene && node.chapter) return chapterTitle(node.chapter);
	Common::U32String text = node.kind == RouteNode::kScene ? _("Return point") : _("Ending");
	text += U" - "; text += speakerText(node.entry);
	return text;
}
Common::U32String routeBody(const RouteGraph &graph, const RouteMapItem &item) {
	if (!item.node) {
		return _("This ending has not been reached. Its details stay hidden until you discover it.\n\nUse a recorded story return point to try another outcome. Inventory, item ownership, object state, story flags and the clock return to that checkpoint together. Earlier decisions are not undone by a later checkpoint: choose an earlier chapter if an item has already been lost, used or given away.\n\nAn unreached ending has no recorded path or checkpoint of its own.");
	}
	const RouteNode &node = *graph.node(item.node);
	Common::U32String text;
	if (node.chapter) { text = U"**"; text += chapterTitle(node.chapter); text += U"**\n\n"; }
	text += entryContext(node.entry); text += U"\n\n";
	if (node.kind == RouteNode::kDecision) {
		for (uint32 i = 0; i < node.entry.choices.size(); ++i) {
			text += node.seenChoices & (1u << i) ? _("**Explored reply**") : _("**Unexplored reply**");
			text += U"\n\n"; text += literal(node.entry.choices[i]); text += U"\n\n";
		}
	} else { text += literal(node.entry.text); text += U"\n\n"; }
	const RouteNode *scene = graph.node(graph.checkpointFor(item.node));
	if (scene && scene->chapter && scene->entry.file.empty()) {
		text += _("Revisit returns to the earliest recorded checkpoint in "); text += chapterTitle(scene->chapter);
		text += _(". Your inventory, item ownership, object state, story flags and clock are restored together. Play from there to explore another outcome. A later checkpoint retains earlier decisions; choose an earlier chapter to change them.");
	} else if (scene && graph.checkpointFor(item.node) != graph.sceneFor(item.node)) {
		text += _("This part of the route uses an earlier encounter checkpoint. Revisit begins with "); text += literal(speakerText(scene->entry)); text += _(". Replay the encounter to explore another branch.");
	} else text += scene && scene->checkpoint ? _("Revisit restores the state before this conversation began. Start the conversation again to explore a different reply.") : _("No safe checkpoint was captured for this scene. You can still read its discovered dialogue and choices.");
	return text;
}
class RouteMapWidget : public GUI::Widget, public GUI::CommandSender {
	const RouteGraph &_graph;
	RouteMap _map;
	Common::Array<bool> _currentPath;
	bool _overview = true;
	int _maxColumn = 0, _maxRow = 0;
	int32 _selected = -1, _scrollX = 0, _scrollY = 0;
	int32 _extentX = 0, _extentY = 0;
	int _downX = 0, _downY = 0, _lastX = 0, _lastY = 0, _pressed = -1;
	bool _dragging = false, _held = false;
	int tileSize() const { return CLIP(MIN(((_w - 40 - _maxColumn * 8) * 2) / (_maxColumn + 2), (_h - 40) / (_maxRow + 1) - 12), 12, 32); }
	int cardWidth() const { return _overview ? tileSize() : MAX(148, g_gui.getFont().getFontHeight() * 10); }
	int cardHeight() const { return _overview ? tileSize() : g_gui.getFont().getFontHeight() * (_h < 180 ? 2 : 3) + (_h < 180 ? 14 : 24); }
	int columnStep() const { return (cardWidth() + (_overview ? 16 : 36)) / 2; }
	int rowStep() const { return cardHeight() + (_overview ? 12 : _h < 180 ? 24 : 48); }
	int left(const RouteMapItem &item) const { return 20 + MAX(0, (_w - _extentX) / 2) + item.column * columnStep() - _scrollX; }
	int top(const RouteMapItem &item) const { return 20 + item.row * rowStep() - _scrollY; }
	void scroll(int x, int y) {
		_scrollX = CLIP(x, 0, MAX(0, _extentX - _w)); _scrollY = CLIP(y, 0, MAX(0, _extentY - _h)); markAsDirty();
	}
	int at(int x, int y) const {
		for (uint32 i = 0; i < _map.items().size(); ++i) {
			const auto &item = _map.items()[i];
			if (x >= left(item) && x < left(item) + cardWidth() && y >= top(item) && y < top(item) + cardHeight()) return i;
		}
		return -1;
	}
protected:
	void drawWidget() override {
		Graphics::ManagedSurface surface(_w, _h, g_system->getOverlayFormat());
		const auto color = [&](int r, int g, int b) { return surface.format.RGBToColor(r, g, b); };
		const uint32 background = color(17, 26, 34), brass = color(191, 161, 101), ivory = color(240, 232, 211), muted = color(146, 157, 166);
		surface.fillRect(Common::Rect(_w, _h), background);
		const auto rect = [&](int l, int t, int r, int b, uint32 ink) {
			l = MAX(0, l); t = MAX(0, t); r = MIN<int>(_w, r); b = MIN<int>(_h, b);
			if (r > l && b > t) surface.fillRect(Common::Rect(l, t, r, b), ink);
		};
		const Graphics::Font &font = g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal);
		if (_map.items().empty()) font.drawString(&surface, _("Conversations and choices will appear here as you play."), 12, 12, _w - 24, ivory, Graphics::kTextAlignLeft, 0, true);
		for (uint32 pass = 0; pass < 3; ++pass) for (const auto &item : _map.items()) if (item.parent >= 0) {
			const auto &parent = _map.items()[item.parent];
			const bool current = item.node ? _currentPath[item.node] : !item.locked && parent.node == _graph.cursor() && item.choice == _graph.choice();
			if (pass != (current ? 2u : item.locked ? 0u : 1u)) continue;
			const int x1 = left(parent) + cardWidth() / 2, y1 = top(parent) + cardHeight();
			const int x2 = left(item) + cardWidth() / 2, y2 = top(item), middle = y1 + (rowStep() - cardHeight()) / 2;
			const uint32 ink = current ? color(109, 193, 184) : item.locked ? color(72, 84, 95) : brass;
			rect(x1, y1, x1 + 2, middle + 2, ink); rect(MIN(x1, x2), middle, MAX(x1, x2) + 2, middle + 2, ink); rect(x2, middle, x2 + 2, y2, ink);
			if (!_overview && item.choice >= 0 && x2 >= 0 && x2 < _w && middle >= -font.getFontHeight() && middle < _h)
				font.drawString(&surface, Common::String::format("%u", uint32(item.choice + 1)), x2 + 5, middle + 3, 32, ink);
		}
		for (uint32 i = 0; i < _map.items().size(); ++i) {
			const auto &item = _map.items()[i]; const int x = left(item), y = top(item), w = cardWidth(), h = cardHeight();
			if (x + w <= 0 || y + h <= 0 || x >= _w || y >= _h) continue;
			const RouteNode *node = _graph.node(item.node);
			const bool selected = int32(i) == _selected, current = int32(i) == _map.itemFor(_graph.cursor());
			const uint32 category = !node ? color(110, 123, 135) : node->kind == RouteNode::kScene ? color(143, 167, 190) : node->kind == RouteNode::kDecision ? brass : color(198, 132, 112);
			const uint32 border = selected ? ivory : current ? color(109, 193, 184) : _overview ? category : item.locked ? color(72, 84, 95) : brass;
			rect(x, y, x + w, y + h, selected ? color(49, 62, 74) : color(29, 41, 52));
			const int thickness = selected ? (_overview ? 2 : 3) : 1;
			rect(x, y, x + w, y + thickness, border); rect(x, y + h - thickness, x + w, y + h, border);
			rect(x, y, x + thickness, y + h, border); rect(x + w - thickness, y, x + w, y + h, border);
			if (_overview) {
				const Common::U32String symbol = item.locked ? _("?") : node && node->kind == RouteNode::kEnding ? _("E") : _("C");
				const Graphics::Font &tileFont = h >= font.getFontHeight() + 4 ? font : *FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
				tileFont.drawString(&surface, symbol, x + 2, y + (h - tileFont.getFontHeight()) / 2, w - 4, border, Graphics::kTextAlignCenter);
				continue;
			}
			Common::U32String kind = item.locked ? _("UNREACHED ENDING") : node && node->kind == RouteNode::kEnding ? _("REACHED ENDING") : _("RETURN POINT");
			if (current) kind += _("  |  Current");
			font.drawString(&surface, kind, x + 9, y + (_h < 180 ? 4 : 7), w - 18, border, Graphics::kTextAlignLeft, 0, true);
			font.drawString(&surface, routeTitle(_graph, item), x + 9, y + font.getFontHeight() + (_h < 180 ? 7 : 11), w - 18, item.locked ? muted : ivory, Graphics::kTextAlignLeft, 0, true);
			Common::U32String context = node ? gameText(node->entry.room) : _("Outcome hidden");
			if (node && node->kind == RouteNode::kScene && node->checkpoint) context = _("Checkpoint available");
			if (!node && !item.locked) context = _("Continue this route");
			if (_h >= 180) font.drawString(&surface, context, x + 9, y + font.getFontHeight() * 2 + 15, w - 18, muted, Graphics::kTextAlignLeft, 0, true);
		}
		g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
	}
public:
	RouteMapWidget(GUI::Dialog *boss, const Session &session) : GUI::Widget(boss, 8, 80, 300, 120), GUI::CommandSender(boss), _graph(session.routes.graph) {
		setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_CLEARBG | GUI::WIDGET_TRACK_MOUSE | GUI::WIDGET_HOOK_DRAG);
		const RouteGraph &graph = _graph;
		const auto tour = session.runtime.globals.find("tour");
		_map.build(graph, session.version, session.sound.isTitanicRetail(), tour != session.runtime.globals.end() && tour->_value.truth());
		for (const auto &item : _map.items()) { _maxColumn = MAX<int>(_maxColumn, item.column); _maxRow = MAX<int>(_maxRow, item.row); }
		_currentPath.resize(graph.nodes().size() + 1);
		for (uint32 id = graph.cursor(); id; id = graph.node(id)->parent) { const int32 index = _map.itemFor(id); if (index >= 0) _currentPath[_map.items()[index].node] = true; }
		_selected = _map.itemFor(graph.cursor()); if (_selected < 0 && !_map.items().empty()) _selected = 0;
	}
	bool wantsFocus() override { return true; }
	const RouteMapItem *selected() const { return _selected >= 0 ? &_map.items()[_selected] : nullptr; }
	int32 selectedIndex() const { return _selected; }
	uint32 visibleCount() const { return _map.items().size(); }
	uint32 endingCount(bool reachedOnly) const {
		uint32 count = 0, seen = 0;
		for (const auto &item : _map.items()) if (item.ending && (!reachedOnly || !item.locked) && !(seen & (1u << item.ending))) { seen |= 1u << item.ending; ++count; }
		return count;
	}
	bool overview() const { return _overview; }
	void toggleOverview() { _overview = !_overview; }
	void focusCurrent() { const int32 index = _map.itemFor(_graph.cursor()); select(index >= 0 ? index : 0); }
	void select(int32 index, bool notify = true) {
		if (index < 0 || uint32(index) >= _map.items().size()) return;
		_selected = index; const auto &item = _map.items()[index];
		scroll(20 + item.column * columnStep() + cardWidth() / 2 - _w / 2, 20 + item.row * rowStep() + cardHeight() / 2 - _h / 2);
		if (notify) sendCommand(kRouteSelect, 0);
	}
	void reflowLayout() override {
		GUI::Widget::reflowLayout(); _extentX = _extentY = 0;
		for (const auto &item : _map.items()) { _extentX = MAX(_extentX, 40 + item.column * columnStep() + cardWidth()); _extentY = MAX(_extentY, 40 + item.row * rowStep() + cardHeight()); }
		select(_selected, false);
	}
	void handleMouseWheel(int x, int y, int direction) override { scroll(_scrollX, _scrollY + direction * rowStep() / 2); }
	void handleMouseDown(int x, int y, int button, int clicks) override { _held = true; _downX = _lastX = x; _downY = _lastY = y; _pressed = at(x, y); _dragging = false; }
	void handleMouseMoved(int x, int y, int button) override {
		if (!_held) return;
		if (ABS(x - _downX) + ABS(y - _downY) > 6) _dragging = true;
		if (_dragging) scroll(_scrollX + _lastX - x, _scrollY + _lastY - y);
		_lastX = x; _lastY = y;
	}
	void handleMouseUp(int x, int y, int button, int clicks) override {
		if (!_dragging && _pressed >= 0 && _pressed == at(x, y)) { select(_pressed); if (clicks == 2) sendCommand(kRead, 0); }
		_pressed = -1; _dragging = _held = false;
	}
	void cancelDrag() override { _pressed = -1; _dragging = _held = false; }
	bool handleKeyDown(Common::KeyState key) override {
		if (key.keycode == Common::KEYCODE_UP || key.keycode == Common::KEYCODE_DOWN || key.keycode == Common::KEYCODE_LEFT || key.keycode == Common::KEYCODE_RIGHT) {
			select(_map.navigate(_selected, key.keycode == Common::KEYCODE_LEFT ? -1 : key.keycode == Common::KEYCODE_RIGHT ? 1 : 0, key.keycode == Common::KEYCODE_UP ? -1 : key.keycode == Common::KEYCODE_DOWN ? 1 : 0)); return true;
		}
		if (key.keycode == Common::KEYCODE_RETURN) { sendCommand(kRead, 0); return true; }
		if (key.keycode == Common::KEYCODE_SPACE) { sendCommand(kReplay, 0); return true; }
		if (key.keycode == Common::KEYCODE_r) { sendCommand(kRevisit, 0); return true; }
		if (key.keycode == Common::KEYCODE_z) { sendCommand(kRouteZoom, 0); return true; }
		if (key.keycode == Common::KEYCODE_HOME) { focusCurrent(); return true; }
		if (key.keycode == Common::KEYCODE_PAGEUP || key.keycode == Common::KEYCODE_PAGEDOWN) { scroll(_scrollX, _scrollY + (key.keycode == Common::KEYCODE_PAGEUP ? -_h : _h)); return true; }
		return false;
	}
};
class LeadReader : public GUI::Dialog {
	const JournalLead &_lead;
	const Journal &_journal;
	Replay &_replay;
	GUI::StaticTextWidget *_title;
	GUI::RichTextWidget *_text = nullptr;
	GUI::ButtonWidget *_hint, *_play, *_back;
	uint32 _revealed = 0;
	void refresh() {
		Common::U32String body = literal(_lead.status); body += U"\n\n";
		if (_revealed) {
			body += Common::U32String::format(_("**Hint %u / %u**\n\n"), _revealed, uint32(_lead.hints.size()));
			body += literal(_lead.hints[_revealed - 1]); body += U"\n\n";
		}
		body += literal(_lead.summary); body += U"\n\n";
		if (!_revealed) body += _("Hints are optional. Each press reveals one reminder based on information you have already discovered.\n\n");
		if (_revealed > 1) {
			body += _("**Earlier reminders**\n\n");
			for (uint32 i = 0; i + 1 < _revealed; ++i) { body += Common::U32String::format("%u. ", i + 1); body += literal(_lead.hints[i]); body += U"\n\n"; }
		}
		if (!_lead.evidence.empty()) body += _("**Recorded evidence**\n\n");
		for (uint32 index : _lead.evidence) {
			const auto &entry = _journal.entries[index];
			body += literal(speakerText(entry)); body += U"\n\n"; body += entryBody(entry); body += U"\n\n";
		}
		if (_text) { removeWidget(_text); delete _text; }
		_text = new GUI::RichTextWidget(this, 8, 36, 300, 140, body);
		_hint->setEnabled(_revealed < _lead.hints.size());
		_hint->setLabel(_revealed < _lead.hints.size() ? Common::U32String::format(_("Hint %u / %u"), _revealed + 1, uint32(_lead.hints.size())) : _("Hints shown"));
		if (isVisible()) { reflowLayout(); g_gui.scheduleTopDialogRedraw(); }
	}
public:
	uint32 shownHints() const { return _revealed; }
	LeadReader(const JournalLead &lead, const Journal &journal, Replay &replay, bool showHint) : GUI::Dialog(0, 0, 320, 240), _lead(lead), _journal(journal), _replay(replay) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain;
		_title = new GUI::StaticTextWidget(this, 8, 8, 300, 24, lead.title, Graphics::kTextAlignLeft);
		_hint = new GUI::ButtonWidget(this, 8, 208, 95, 24, _("Hint"), Common::U32String(), kHint);
		_play = new GUI::ButtonWidget(this, 112, 208, 95, 24, _("Replay latest"), Common::U32String(), kReplay);
		if (lead.id == "ending-hints") _play->setLabel(_("Replay evidence"));
		_back = new GUI::ButtonWidget(this, 216, 208, 95, 24, _("Back"), Common::U32String(), GUI::kCloseCmd);
		_play->setEnabled(!lead.evidence.empty() && !journal.entries[lead.evidence.back()].ident.empty());
		if (showHint && !lead.hints.empty()) _revealed = 1;
		refresh();
	}
	void reflowLayout() override {
		fit(*this); const int row = MAX(24, g_gui.getFont().getFontHeight() + 12), width = (_w - 32) / 3;
		place(_title, 8, 8, _w - 16, row); place(_text, 8, row + 12, _w - 16, MAX(20, _h - row * 2 - 28));
		place(_hint, 8, _h - row - 8, width, row); place(_play, 16 + width, _h - row - 8, width, row); place(_back, 24 + width * 2, _h - row - 8, width, row);
		if (_lead.evidence.empty()) { showPanel(_play, false); place(_hint, 8, _h - row - 8, (_w - 24) / 2, row); place(_back, _w / 2 + 4, _h - row - 8, (_w - 24) / 2, row); }
		GUI::Dialog::reflowLayout();
	}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (event.customType == kJournalClose) { close(); return; }
			if (event.customType == kJournalReplay) { handleCommand(nullptr, kReplay, 0); return; }
			if (event.customType == kJournalStar) { handleCommand(nullptr, kHint, 0); return; }
			if (event.customType == kJournalUp || event.customType == kJournalDown) { _text->handleMouseWheel(0, 0, event.customType == kJournalUp ? -1 : 1); return; }
		}
		GUI::Dialog::handleOtherEvent(event);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kHint) { if (_revealed < _lead.hints.size()) { ++_revealed; refresh(); } }
		else if (cmd == kReplay) { if (_play->isEnabled()) _replay.play(_journal.entries[_lead.evidence.back()]); }
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
void showEndingHints(const JournalLead &lead, const Journal &journal, Replay &replay) {
	LeadReader reader(lead, journal, replay, false); reader.runModal();
	if (ConfMan.hasKey("dreamfactory_puppet_audit") && ConfMan.getBool("dreamfactory_puppet_audit")) warning("ENDING HINT REQUESTS shown=%u available=%u", reader.shownHints(), uint32(lead.hints.size()));
}
class JournalDialog : public GUI::Dialog {
	Session &_session;
	Replay _replay;
	GUI::StaticTextWidget *_searchLabel, *_speaker, *_context, *_status, *_routeLegend;
	GUI::RichTextWidget *_preview = nullptr;
	int32 _previewIndex = -2;
	bool _wide = false, _wasPlaying = false;
	GUI::EditTextWidget *_search;
	TranscriptWidget *_list;
	RouteMapWidget *_mapWidget;
	NavigationWidget *_navigationWidget = nullptr;
	GUI::ButtonWidget *_tabs[5] = {}, *_buttons[5];
	Common::Array<JournalLead> _leads;
	Journal _leadRows;
	Common::Array<uint32> _indices;
	uint32 _mode = kLines;
	uint32 _revisitNode = 0;
	const JournalLead *selectedLead() const { const int32 index = selected(); return _mode == kLeads && index >= 0 && uint32(index) < _leads.size() ? &_leads[index] : nullptr; }
	const JournalEntry *selectedEntry() const {
		if (_mode == kShipMap) return nullptr;
		if (_mode == kLeads) { const auto *lead = selectedLead(); return lead && !lead->evidence.empty() ? &_session.journal.entries[lead->evidence.back()] : nullptr; }
		if (_mode == kRoutes) {
			const RouteMapItem *item = _mapWidget->selected(); const RouteNode *node = item ? _session.routes.graph.node(item->node) : nullptr;
			return node ? &node->entry : nullptr;
		}
		const int32 index = selected(); return index >= 0 ? &_session.journal.entries[index] : nullptr;
	}
	bool canRevisit() const {
		const RouteMapItem *item = _mapWidget->selected();
		const RouteNode *scene = item && item->node ? _session.routes.graph.node(_session.routes.graph.checkpointFor(item->node)) : nullptr;
		return scene && scene->checkpoint && _session.routes.active();
	}
	void requestRevisit() {
		if (!canRevisit()) return;
		const RouteMapItem &item = *_mapWidget->selected(); const JournalEntry &entry = _session.routes.graph.node(item.node)->entry;
		RoutePreview preview; const bool hasPreview = _session.routes.readPreview(item.node, preview);
		EntryDialog confirmation(entry, _replay, routeBody(_session.routes.graph, item), false, true, hasPreview ? &preview : nullptr, true);
		if (confirmation.runModal() == GUI::kMessageOK && !g_system->getEventManager()->shouldQuit() && !g_system->getEventManager()->shouldReturnToLauncher()) {
			_revisitNode = _mapWidget->selected()->node; close();
		}
	}
	void refresh() {
		const int old = _list->getSelected();
		const int32 previous = old >= 0 && uint32(old) < _indices.size() ? int32(_indices[old]) : -1;
		_indices.clear();
		_leads = journalLeads(_session.journal, journalLeadState(_session)); _leadRows.entries.clear();
		for (const auto &lead : _leads) { JournalEntry row; row.speaker = lead.status.encode(Common::kUtf8); Common::U32String text = lead.title; text += U"\n"; text += lead.summary; row.text = text.encode(Common::kUtf8); _leadRows.add(row); }
		const Journal &displayed = _mode == kLeads ? _leadRows : _session.journal;
		_list->setJournal(displayed, _mode != kLeads);
		for (uint32 i = 0; i < displayed.entries.size(); ++i) {
			const JournalEntry &entry = displayed.entries[i];
			if ((_mode == kPaths && entry.choices.empty()) || (_mode == kLines && !entry.choices.empty() && entry.selected < 0)) continue;
			Common::U32String searchable(entry.speaker + " " + entry.room + " " + entry.text, _mode == kLeads ? Common::kUtf8 : Common::kWindows1252);
			searchable += U" "; searchable += speakerText(entry);
			for (const auto &choice : entry.choices) { searchable += ' '; searchable += gameText(choice); }
			searchable.toLowercase();
			Common::U32String query = _search->getEditString(); query.toLowercase();
			Common::U32StringTokenizer tokens(query);
			bool matches = true;
			while (!tokens.empty()) if (!searchable.contains(tokens.nextToken())) { matches = false; break; }
			if (matches) _indices.push_back(i);
		}
		_list->setEntries(_indices, _mode == kPaths);
		int selected = _indices.empty() ? -1 : int(_indices.size()) - 1;
		for (uint32 i = 0; i < _indices.size(); ++i) if (int32(_indices[i]) == previous) selected = i;
		_list->setSelected(selected);
		const uint32 modes[] = {kLines, kPaths, kRoutes, kLeads, kShipMap};
		for (uint32 i = 0; i < (_navigationWidget ? 5u : 4u); ++i) _tabs[i]->setEnabled(_mode != modes[i]);
		_previewIndex = -2;
		updateButtons();
	}
	int32 selected() const {
		const int index = _list->getSelected();
		return index >= 0 && uint32(index) < _indices.size() ? int32(_indices[index]) : -1;
	}
	void updateButtons() {
		if (_mode == kShipMap && _navigationWidget) {
			const MapPosition point = _navigationWidget->selectedPosition();
			_buttons[0]->setEnabled(point.valid()); _buttons[1]->setLabel(_("Directions")); _buttons[1]->setEnabled(_navigationWidget->selectedVisit() != 0);
			_buttons[2]->setLabel(_navigationWidget->zoomed() ? _("Fit") : _("Zoom")); showPanel(_buttons[2], true); _buttons[2]->setEnabled(_navigationWidget->page() != 0);
			showPanel(_buttons[4], true); _buttons[4]->setEnabled(true);
			_status->setLabel(point.valid() ? mapLocationName(point) + Common::U32String::format(_("  |  %u visited rooms"), _navigationWidget->count()) : _("No recorded visit on this map page"));
			return;
		}
		const bool routes = _mode == kRoutes;
		const int32 index = routes ? _mapWidget->selectedIndex() : selected();
		const int32 previewIndex = routes ? -10 - index : index;
		const JournalEntry *entry = selectedEntry();
		const RouteMapItem *item = routes ? _mapWidget->selected() : nullptr;
		const bool wantPreview = routes && !_mapWidget->overview() && _w >= 600 && _h >= 360;
		_buttons[1]->setLabel(routes ? _mapWidget->overview() ? _("Cards") : _("Overview") : _mode == kLeads ? _("Hint") : entry && _replay.playing(*entry) ? _("Stop") : _("Replay"));
		_buttons[2]->setLabel(routes ? _("Revisit") : entry && entry->bookmarked ? _("Untrack") : _("Track"));
		showPanel(_buttons[2], _mode != kLeads);
		if (previewIndex != _previewIndex || wantPreview != (_preview != nullptr)) {
			_previewIndex = previewIndex;
			_speaker->setLabel(item ? routeTitle(_session.routes.graph, *item) : entry ? speakerText(*entry) : _("Nothing to display"));
			_context->setLabel(entry ? entryContext(*entry) : Common::U32String());
			Common::U32String body = entry ? entryBody(*entry) : !_search->getEditString().empty() ? _("No matching entries. Try another search.") :
				_mode == kPaths ? _("Your encountered dialogue choices will appear here.") : _("Heard dialogue will appear here as you play.");
			if (routes) body = item ? routeBody(_session.routes.graph, *item) : _("Discovered conversations and branches will appear as you play. Future outcomes remain hidden.");
			// Replacing the reader resets its scroll position and cached Markdown surface.
			if (_preview) { removeWidget(_preview); delete _preview; _preview = nullptr; }
			if (wantPreview) {
				_preview = new GUI::RichTextWidget(this, 8, 80, 200, 100, body);
				layoutPreview(); showPanel(_preview, _wide);
				if (isVisible()) _preview->reflowLayout();
			}
		}
		Common::U32String status = Common::U32String::format(_("%u entries  |  Oldest to newest"), uint32(_indices.size()));
		if (index >= 0) status = Common::U32String::format(_("%u / %u  |  Up/Down: Select"), uint32(_list->getSelected() + 1), uint32(_indices.size()));
		if (routes) {
			status = item ? routeTitle(_session.routes.graph, *item) : _("No route discoveries yet");
			if (_mapWidget->endingCount(false)) status = Common::U32String::format(_("Endings: %u / %u  |  "), _mapWidget->endingCount(true), _mapWidget->endingCount(false)) + status;
			else status += Common::U32String::format(_("  |  %u return points / outcomes"), _mapWidget->visibleCount());
		}
		if (_mode == kLeads) status = Common::U32String::format(_("%u discovered / tracked leads"), uint32(_indices.size()));
		_status->setLabel(status);
		_buttons[0]->setEnabled(index >= 0);
		_buttons[1]->setEnabled(routes || selectedLead() || (entry && !entry->ident.empty()));
		_buttons[2]->setEnabled(routes ? canRevisit() : index >= 0);
		showPanel(_buttons[4], routes); _buttons[4]->setEnabled(_session.routes.graph.cursor() != 0);
	}
	void layoutPreview() {
		const int row = MAX(24, g_gui.getFont().getFontHeight() + 12);
		const int top = row * 2 + 16;
		const int left = 20 + (_w - 28) * (_mode == kRoutes ? 62 : 42) / 100;
		place(_speaker, left, top, _w - left - 12, row);
		place(_context, left, top + row, _w - left - 12, row);
		if (_preview) place(_preview, left, top + row * 2 + 8, MAX(20, _w - left - 12), MAX(20, _h - top - row * 4 - 28));
	}
public:
	bool auditMapInput() {
		if (!_navigationWidget || _mode != kShipMap || !_navigationWidget->auditPointerInput()) return false;
		const auto point = _navigationWidget->markerPoint(0); const int x = _navigationWidget->getRelX() + point.x, y = _navigationWidget->getRelY() + point.y;
		if (findWidget(x, y) != _navigationWidget) return false;
		handleMouseDown(x, y, 1, 1); handleMouseUp(x, y, 1, 1); return _navigationWidget->selectedIndex() == 0;
	}
	bool auditPointerInput() {
		struct RestoreTracking {
			Journal &journal; Common::Array<bool> flags;
			explicit RestoreTracking(Journal &j) : journal(j) { for (auto &entry : journal.entries) { flags.push_back(entry.bookmarked); entry.bookmarked = true; } }
			~RestoreTracking() { for (uint32 i = 0; i < flags.size(); ++i) journal.entries[i].bookmarked = flags[i]; }
		} tracking(_session.journal);
		for (uint32 mode : {uint32(kLines), uint32(kPaths), uint32(kLeads)}) {
			handleCommand(nullptr, mode, 0);
			_list->setSelected(0); _list->setSelected(-1);
			const int x = _list->getRelX() + 30, y = _list->getRelY() + 8;
			if (findWidget(x, y) != _list) { warning("JOURNAL POINTER hit mode=%u x=%d y=%d", mode, x, y); return false; }
			handleMouseDown(x, y, 1, 1); handleMouseUp(x, y, 1, 1);
			if (_list->getSelected() != 0) { warning("JOURNAL POINTER selection mode=%u selected=%d", mode, _list->getSelected()); return false; }
			const int before = _list->scrollPosition();
			handleMouseWheel(x, y, 1);
			if (_list->scrollPosition() <= before) { warning("JOURNAL POINTER scroll mode=%u before=%d after=%d", mode, before, _list->scrollPosition()); return false; }
			handleMouseWheel(x, y, -1);
			if (_list->scrollPosition() != before) { warning("JOURNAL POINTER wheel return mode=%u", mode); return false; }
			handleMouseDown(x, y + 30, 1, 1); handleMouseMoved(x, y + 10, 1); handleMouseUp(x, y + 10, 1, 1);
			if (_list->scrollPosition() <= before) { warning("JOURNAL POINTER scroll mode=%u before=%d after=%d", mode, before, _list->scrollPosition()); return false; }
			if (!_list->containsWidget(findWidget(_list->getRelX() + _list->getWidth() - 8, y))) { warning("JOURNAL POINTER scrollbar mode=%u", mode); return false; }
		}
		return true;
	}
	explicit JournalDialog(Session &session, bool shipMap = false) : GUI::Dialog(0, 0, 320, 240), _session(session), _replay(session) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain;
		_speaker = new GUI::StaticTextWidget(this, 8, 80, 200, 24, Common::U32String(), Graphics::kTextAlignLeft);
		_context = new GUI::StaticTextWidget(this, 8, 104, 200, 24, Common::U32String(), Graphics::kTextAlignLeft, Common::U32String(), GUI::ThemeEngine::kFontStyleNormal);
		_status = new GUI::StaticTextWidget(this, 8, 180, 300, 20, Common::U32String(), Graphics::kTextAlignLeft, Common::U32String(), GUI::ThemeEngine::kFontStyleNormal);
		_searchLabel = new GUI::StaticTextWidget(this, 8, 56, 56, 20, _("Search:"), Graphics::kTextAlignLeft);
		_tabs[0] = new GUI::ButtonWidget(this, 8, 32, 95, 20, _("Log"), Common::U32String(), kLines);
		_tabs[1] = new GUI::ButtonWidget(this, 108, 32, 95, 20, _("Choices"), Common::U32String(), kPaths);
		_tabs[2] = new GUI::ButtonWidget(this, 208, 32, 95, 20, _("Flow"), Common::U32String(), kRoutes);
		_tabs[3] = new GUI::ButtonWidget(this, 308, 32, 95, 20, _("Leads"), Common::U32String(), kLeads);
		_routeLegend = new GUI::StaticTextWidget(this, 12, 56, 300, 20, _("C: Return point  E: Reached ending  ?: Unreached ending"), Graphics::kTextAlignLeft, Common::U32String(), GUI::ThemeEngine::kFontStyleNormal);
		_search = new GUI::EditTextWidget(this, 8, 56, 300, 20, Common::U32String(), _("Search recorded dialogue"), kSearch);
		_list = new TranscriptWidget(this, session, _replay);
		_mapWidget = new RouteMapWidget(this, session); showPanel(_mapWidget, false); showPanel(_routeLegend, false);
		const Common::U32String labels[] = {_("Read"), _("Replay"), _("Revisit"), _("Back")};
		const uint32 commands[] = {kRead, kReplay, kStar, GUI::kCloseCmd};
		for (uint32 i = 0; i < 4; ++i) _buttons[i] = new GUI::ButtonWidget(this, 8 + i * 76, 208, 70, 24, labels[i], Common::U32String(), commands[i]);
		_buttons[4] = new GUI::ButtonWidget(this, 8, 208, 70, 24, _("Current"), Common::U32String(), kRouteCurrent); showPanel(_buttons[4], false);
		if (session.atlas.load(session)) {
			_tabs[4] = new GUI::ButtonWidget(this, 408, 32, 95, 20, _("Map"), Common::U32String(), kShipMap);
			_navigationWidget = new NavigationWidget(this, session); showPanel(_navigationWidget, false);
			if (shipMap) _mode = kShipMap;
		}
		refresh();
	}
	uint32 revisitNode() const { return _revisitNode; }
	void open() override { GUI::Dialog::open(); setFocusWidget(_mode == kShipMap ? static_cast<GUI::Widget *>(_navigationWidget) : _list); }
	void reflowLayout() override {
		fit(*this);
		const int row = MAX(24, g_gui.getFont().getFontHeight() + 12);
		_wide = _mode == kRoutes && !_mapWidget->overview() && _w >= 600 && _h >= 360;
		const bool compact = _h < 360;
		const int tabs = _navigationWidget ? 5 : 4;
		for (int i = 0; i < tabs; ++i) place(_tabs[i], 8 + i * (_w - 12) / tabs, 6, (_w - 12 - tabs * 4) / tabs, row);
		place(_searchLabel, 12, row + 10, 56, row);
		place(_search, 72, row + 10, _w - 84, row);
		place(_routeLegend, 12, row + 10, _w - 24, row);
		showPanel(_search, _mode != kRoutes && _mode != kShipMap && !compact); showPanel(_searchLabel, _mode != kRoutes && _mode != kShipMap && !compact);
		_routeLegend->setLabel(_mode == kShipMap ? _("Left/Right: page  Up/Down: room  Y: zoom  Blue: you  Gold: visited") : _("C: Return point  E: Reached ending  ?: Unreached ending"));
		showPanel(_routeLegend, (_mode == kRoutes || _mode == kShipMap) && !compact);
		const int top = compact ? row + 12 : row * 2 + 16;
		place(_list, 12, top, _wide ? (_w - 28) * 42 / 100 : _w - 24, MAX(20, _h - top - row * 2 - 20));
		place(_mapWidget, 12, top, _wide ? (_w - 28) * 62 / 100 : _w - 24, MAX(20, _h - top - row * 2 - 20));
		showPanel(_list, _mode != kRoutes && _mode != kShipMap); showPanel(_mapWidget, _mode == kRoutes);
		if (_navigationWidget) { place(_navigationWidget, 12, top, _w - 24, MAX(20, _h - top - row * 2 - 20)); showPanel(_navigationWidget, _mode == kShipMap); }
		layoutPreview();
		showPanel(_speaker, _wide); showPanel(_context, _wide);
		if (_preview) showPanel(_preview, _wide);
		place(_status, 12, _h - row * 2 - 12, _w - 24, row);
		const int count = _mode == kRoutes || _mode == kShipMap ? 5 : _mode == kLeads ? 3 : 4;
		for (int i = 0; i < count; ++i) {
			const int button = count == 5 && i >= 3 ? i == 3 ? 4 : 3 : count == 3 && i == 2 ? 3 : i;
			place(_buttons[button], 12 + i * (_w - 20) / count, _h - row - 8, (_w - 16 - count * 5) / count, row);
		}
		GUI::Dialog::reflowLayout();
		updateButtons();
	}
	void handleTickle() override {
		GUI::Dialog::handleTickle();
		const JournalEntry *entry = selectedEntry();
		const bool playing = _replay.active();
		if (playing != _wasPlaying) { _wasPlaying = playing; _list->markAsDirty(); }
		if (_mode != kRoutes && _mode != kLeads && _mode != kShipMap) _buttons[1]->setLabel(entry && _replay.playing(*entry) ? _("Stop") : _("Replay"));
	}

	void switchView(int direction) {
		const uint32 modes[] = {kLines, kPaths, kRoutes, kLeads, kShipMap}; const int count = _navigationWidget ? 5 : 4;
		int current = _mode == kLines ? 0 : _mode == kPaths ? 1 : _mode == kRoutes ? 2 : _mode == kLeads ? 3 : 4;
		handleCommand(nullptr, modes[(current + direction + count) % count], 0);
	}
	void handleKeyDown(Common::KeyState key) override {
		if (_mode == kShipMap && _navigationWidget && _navigationWidget->handleKeyDown(key)) return;
		if (_mode == kRoutes && getFocusWidget() == _mapWidget && _mapWidget->handleKeyDown(key)) return;
		if (_mode != kRoutes && getFocusWidget() != _search && (key.keycode == Common::KEYCODE_LEFT || key.keycode == Common::KEYCODE_RIGHT)) switchView(key.keycode == Common::KEYCODE_LEFT ? -1 : 1);
		else GUI::Dialog::handleKeyDown(key);
	}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (_mode == kShipMap && _navigationWidget) {
				if (event.customType >= kJournalUp && event.customType <= kJournalRight) { const Common::KeyCode keys[] = {Common::KEYCODE_UP, Common::KEYCODE_DOWN, Common::KEYCODE_LEFT, Common::KEYCODE_RIGHT}; _navigationWidget->handleKeyDown(Common::KeyState(keys[event.customType - kJournalUp])); return; }
				if (event.customType == kJournalReplay) { handleCommand(nullptr, kRead, 0); return; }
				if (event.customType == kJournalStar) { handleCommand(nullptr, kReplay, 0); return; }
				if (event.customType == kJournalRead || event.customType == kJournalZoom) { handleCommand(nullptr, kStar, 0); return; }
				if (event.customType == kJournalCurrent) { handleCommand(nullptr, kRouteCurrent, 0); return; }
			}
			if (_mode == kRoutes && (event.customType == kJournalZoom || event.customType == kJournalCurrent)) { handleCommand(nullptr, event.customType == kJournalZoom ? kRouteZoom : kRouteCurrent, 0); return; }
			if (_mode == kRoutes && event.customType >= kJournalUp && event.customType <= kJournalRight) {
				const Common::KeyCode keys[] = {Common::KEYCODE_UP, Common::KEYCODE_DOWN, Common::KEYCODE_LEFT, Common::KEYCODE_RIGHT};
				setFocusWidget(_mapWidget); _mapWidget->handleKeyDown(Common::KeyState(keys[event.customType - kJournalUp])); return;
			}
			if ((_mode == kRoutes || _mode == kLeads) && event.customType == kJournalReplay) { handleCommand(nullptr, kRead, 0); return; }
			if (event.customType == kJournalClose) { close(); return; }
			if (event.customType == kJournalRead) { handleCommand(nullptr, kRead, 0); return; }
			if (event.customType == kJournalUp || event.customType == kJournalDown) { setFocusWidget(_list); _list->handleKeyDown(Common::KeyState(event.customType == kJournalUp ? Common::KEYCODE_UP : Common::KEYCODE_DOWN)); return; }
			if (event.customType == kJournalLeft || event.customType == kJournalRight) { switchView(event.customType == kJournalLeft ? -1 : 1); return; }
			if (event.customType == kJournalReplay) { handleCommand(nullptr, kReplay, 0); return; }
			if (event.customType == kJournalStar) { handleCommand(nullptr, _mode == kRoutes ? kRevisit : _mode == kLeads ? kReplay : kStar, 0); return; }
			if (event.customType == kJournalViewPrevious || event.customType == kJournalViewNext) { switchView(event.customType == kJournalViewPrevious ? -1 : 1); return; }
		}
		GUI::Dialog::handleOtherEvent(event);
	}

	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (_mode == kShipMap && _navigationWidget) {
			if (cmd == kNavigationSelect) { updateButtons(); return; }
			if (cmd == kStar) { if (_navigationWidget->page()) _navigationWidget->toggleZoom(); return; }
			if (cmd == kRouteCurrent) { _navigationWidget->focusCurrent(); return; }
			if (cmd == kRead || cmd == kReplay) {
				if (cmd == kReplay && !_navigationWidget->selectedVisit()) return;
				const MapPosition point = _navigationWidget->selectedPosition(); if (!point.valid()) return;
				JournalEntry empty; Common::U32String body = _navigationWidget->description();
				if (cmd == kReplay) { body = navigationDirection(_session, _navigationWidget->selectedVisit()); body += _("\n\nDirections use only passages recorded in this save. Reopen Directions after moving for the next step. No movement or map jump is performed."); }
				EntryDialog detail(empty, _replay, body); detail.information(mapLocationName(point)); detail.runModal(); return;
			}
		}
		const int32 index = selected();
		if (_mode == kLeads && (cmd == kRead || cmd == kReplay || cmd == GUI::kListItemActivatedCmd || cmd == GUI::kListItemDoubleClickedCmd)) {
			const auto *lead = selectedLead(); if (lead) { LeadReader reader(*lead, _session.journal, _replay, cmd == kReplay); reader.runModal(); } return;
		}
		if (_mode == kRoutes && (cmd == kRouteZoom || (cmd == kReplay && sender == _buttons[1]))) {
			_mapWidget->toggleOverview(); _previewIndex = -2; updateButtons(); reflowLayout(); g_gui.scheduleTopDialogRedraw(); setFocusWidget(_mapWidget); return;
		}
		if (_mode == kRoutes && cmd == kRouteCurrent) { _mapWidget->focusCurrent(); setFocusWidget(_mapWidget); return; }
		if (_mode == kRoutes && (cmd == kStar || cmd == kRevisit)) { requestRevisit(); return; }
		if (_mode == kRoutes && (cmd == kRead || cmd == kReplay)) {
			const RouteMapItem *item = _mapWidget->selected(); if (!item) return;
			const JournalEntry *entry = selectedEntry();
			if (cmd == kReplay) { if (entry && !entry->ident.empty()) _replay.play(*entry); return; }
			JournalEntry empty; empty.speaker = item->locked ? "Undiscovered" : "Continuation";
			RoutePreview preview; const bool hasPreview = item->node && _session.routes.readPreview(item->node, preview);
			EntryDialog detail(entry ? *entry : empty, _replay, routeBody(_session.routes.graph, *item), true, canRevisit(), hasPreview ? &preview : nullptr);
			const JournalLead hints = journalEndingHints(_session.journal, journalLeadState(_session), _session.routes.graph, _session.sound.isTitanicRetail(), _session.runtime.globals.contains("tour") && _session.runtime.globals["tour"].truth());
			if (item->ending && !hints.hints.empty()) detail.endingHints(hints, _session.journal, routeTitle(_session.routes.graph, *item));
			if (detail.runModal() == kRevisit) requestRevisit();
			return;
		}
		if (cmd == kLines || cmd == kPaths || cmd == kRoutes || cmd == kLeads || (cmd == kShipMap && _navigationWidget)) { _mode = cmd; refresh(); reflowLayout(); g_gui.scheduleTopDialogRedraw(); setFocusWidget(_mode == kShipMap ? static_cast<GUI::Widget *>(_navigationWidget) : _mode == kRoutes ? static_cast<GUI::Widget *>(_mapWidget) : _list); }
		else if (cmd == kStar && _mode != kLeads && index >= 0) { _session.journal.entries[index].bookmarked = !_session.journal.entries[index].bookmarked; refresh(); }
		else if (cmd == kSearch) refresh();
		else if (cmd == GUI::kListSelectionChangedCmd || cmd == kRouteSelect) updateButtons();
		else if ((cmd == kRead || cmd == GUI::kListItemActivatedCmd) && index >= 0) {
			EntryDialog detail(_session.journal.entries[index], _replay); detail.runModal();
		} else if ((cmd == kReplay || cmd == GUI::kListItemDoubleClickedCmd) && index >= 0 && !_session.journal.entries[index].ident.empty()) { _replay.play(_session.journal.entries[index]); _list->markAsDirty(); }
		else GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
}
uint32 openJournal(Session &session, bool shipMap) {
	PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken();
	struct OverlaySize {
		bool changed = false; uint width = 0, height = 0; Graphics::PixelFormat format;
		explicit OverlaySize(bool compact) {
			if (!compact) return; width = g_system->getWidth(); height = g_system->getHeight(); format = g_system->getScreenFormat();
			g_system->beginGFXTransaction(); g_system->initSize(480, 272, &format); changed = g_system->endGFXTransaction() == OSystem::kTransactionSuccess;
		}
		~OverlaySize() { if (changed) { g_system->beginGFXTransaction(); g_system->initSize(width, height, &format); g_system->endGFXTransaction(); } }
	} size(shipMap && ConfMan.hasKey("dreamfactory_navigation_test_compact") && ConfMan.getBool("dreamfactory_navigation_test_compact"));
	JournalKeymapScope keys;
	JournalDialog dialog(session, shipMap);
	dialog.runModal();
	return dialog.revisitNode();
}
bool auditJournalInput(Session &session) {
	JournalKeymapScope keys;
	JournalDialog dialog(session); dialog.open();
	const bool valid = dialog.auditPointerInput(); dialog.close(); return valid;
}
bool auditNavigationInput(Session &session) {
	PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken();
	Bytes before, after; Common::String error;
	if (!session.saveState(before, error)) return false;
	const uint width = g_system->getWidth(), height = g_system->getHeight(); const auto format = g_system->getScreenFormat();
	bool valid = true;
	for (bool compact : {false, true}) {
		if (compact) { g_system->beginGFXTransaction(); g_system->initSize(480, 272, &format); if (g_system->endGFXTransaction() != OSystem::kTransactionSuccess) { valid = false; break; } }
		JournalKeymapScope keys; JournalDialog dialog(session, true); dialog.open(); valid = dialog.auditMapInput() && valid; dialog.close();
	}
	g_system->beginGFXTransaction(); g_system->initSize(width, height, &format); valid = g_system->endGFXTransaction() == OSystem::kTransactionSuccess && valid;
	valid = session.saveState(after, error) && before == after && valid;
	if (valid) warning("NAVIGATION INPUT PASS desktop 480x272 native marker hit testing wheel selection touch pan original game snapshot unchanged");
	return valid;
}
void journalNotice(const Common::String &message) {
	JournalKeymapScope keys;
	JournalNotice dialog(Common::U32String(message, Common::kUtf8)); dialog.runModal();
}
}
