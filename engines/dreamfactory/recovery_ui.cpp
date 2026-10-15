/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/recovery.h"
#include "dreamfactory/session.h"
#include "dreamfactory/controls.h"
#include "common/config-manager.h"
#include "common/system.h"
#include "common/translation.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymapper.h"
#include "gui/dialog.h"
#include "gui/gui-manager.h"
#include "gui/widget.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
namespace DreamFactory {
namespace {
enum { kResume = 'RRes', kBack = 'RBak', kSelected = 'RSel' };
const Graphics::Font &recoveryFont() {
	const Graphics::Font &normal = g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal);
	const Graphics::Font *large = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	return normal.getFontHeight() < 12 && large ? *large : normal;
}
class RecoveryKeymap {
	Common::Keymap *_standard, *_recovery;
	bool _standardEnabled, _recoveryEnabled;
public:
	RecoveryKeymap() {
		auto *mapper = g_system->getEventManager()->getKeymapper();
		_standard = mapper->getKeymap(Common::kGuiKeymapName);
		if (!_standard) { _standard = g_gui.getKeymap(); mapper->addGlobalKeymap(_standard); }
		_recovery = mapper->getKeymap("dreamfactory-recovery-ui");
		if (!_recovery) {
			_recovery = new Common::Keymap(Common::Keymap::kKeymapTypeGui, "dreamfactory-recovery-ui", _("DreamFactory recovery"));
			const char *ids[] = {"RECRESUME", "RECCLOSE", "RECPREV", "RECNEXT"};
			const char *labels[] = {_s("Resume selected recovery"), _s("Back"), _s("Previous recovery / confirm"), _s("Next recovery / cancel")};
			const char *bindings[] = {"JOY_A", "JOY_B", "JOY_UP", "JOY_DOWN"};
			for (uint32 i = 0; i < ARRAYSIZE(ids); ++i) {
				auto *action = new Common::Action(ids[i], _(labels[i])); action->setCustomEngineActionEvent(kRecoveryResume + i); action->addDefaultInputMapping(bindings[i]);
				if (i >= 2) { action->allowKbdRepeats(); action->addDefaultInputMapping(i == 2 ? "JOY_LEFT" : "JOY_RIGHT"); action->addDefaultInputMapping(i == 2 ? "JOY_LEFT_SHOULDER" : "JOY_RIGHT_SHOULDER"); }
				_recovery->addAction(action);
			}
			for (const auto *old : _standard->getActions()) {
				auto *action = new Common::Action(old->id, old->description); action->setEvent(old->event);
				if (old->shouldTriggerOnKbdRepeats()) action->allowKbdRepeats();
				for (const auto &binding : old->getDefaultInputMapping()) if (!binding.hasPrefix("JOY_")) action->addDefaultInputMapping(binding);
				_recovery->addAction(action);
			}
			_recovery->setEnabled(false); mapper->addGlobalKeymap(_recovery);
		}
		_standardEnabled = _standard->isEnabled(); _recoveryEnabled = _recovery->isEnabled();
		_standard->setEnabled(false); _recovery->setEnabled(true);
	}
	~RecoveryKeymap() { _standard->setEnabled(_standardEnabled); _recovery->setEnabled(_recoveryEnabled); }
};
class RecoveryCards : public GUI::Widget, public GUI::CommandSender {
	const Common::Array<RecoveryEntry> &_entries;
	int _selected = 0;
protected:
	void drawWidget() override {
		Graphics::ManagedSurface surface(_w, _h, g_system->getOverlayFormat());
		const uint32 ink = surface.format.RGBToColor(240, 232, 211);
		surface.fillRect(Common::Rect(_w, _h), surface.format.RGBToColor(17, 26, 34));
		const auto &font = recoveryFont();
		if (_entries.empty()) { font.drawString(&surface, _("No recovery saves yet."), 8, 12, _w - 16, ink); return g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE); }
		const int row = _h / 3;
		for (uint32 i = 0; i < _entries.size(); ++i) {
			const int top = i * row;
			if (int(i) == _selected) surface.fillRect(Common::Rect(0, top, _w, top + row - 3), surface.format.RGBToColor(49, 62, 74));
			const auto &preview = _entries[i].preview;
			const int height = MIN(48, row - 8), width = height * 4 / 3;
			if (preview.picture.size() == RoutePreview::kWidth * RoutePreview::kHeight * 3) for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
				const byte *rgb = preview.picture.data() + ((y * RoutePreview::kHeight / height) * RoutePreview::kWidth + x * RoutePreview::kWidth / width) * 3;
				surface.setPixel(4 + x, top + 4 + y, surface.format.RGBToColor(rgb[0], rgb[1], rgb[2]));
			}
			Common::U32String title = i == 0 ? _("Latest") : _("Earlier"); title += U"  "; title += Common::U32String(preview.clock, Common::kWindows1252);
			const int left = width + 12;
			font.drawString(&surface, title, left, top + 4, _w - left - 8, ink);
			Common::Array<Common::U32String> lines; font.wordWrapText(Common::U32String(preview.location, Common::kWindows1252), _w - left - 8, lines);
			for (uint32 line = 0; line < MIN<uint32>(2, lines.size()); ++line) if (int(line + 2) * font.getFontHeight() + 6 < row) font.drawString(&surface, lines[line], left, top + 6 + (line + 1) * font.getFontHeight(), _w - left - 8, ink);
		}
		g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
	}
public:
	RecoveryCards(GUI::Dialog *boss, const Common::Array<RecoveryEntry> &entries) : GUI::Widget(boss, 8, 40, 304, 160), GUI::CommandSender(boss), _entries(entries) { setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_CLEARBG); }
	bool wantsFocus() override { return true; }
	int selected() const { return _selected; }
	void move(int delta) { if (!_entries.empty()) { _selected = (_selected + int(_entries.size()) + delta) % int(_entries.size()); markAsDirty(); sendCommand(kSelected, _selected); } }
	void handleMouseUp(int x, int y, int button, int clicks) override { if (y >= 0 && y < _h && uint32(y / (_h / 3)) < _entries.size()) { _selected = y / (_h / 3); markAsDirty(); sendCommand(kSelected, _selected); } }
	void handleMouseWheel(int x, int y, int direction) override { move(direction > 0 ? 1 : -1); }
	bool handleKeyDown(Common::KeyState key) override {
		if (key.keycode == Common::KEYCODE_UP || key.keycode == Common::KEYCODE_DOWN) { move(key.keycode == Common::KEYCODE_UP ? -1 : 1); return true; }
		if (key.keycode == Common::KEYCODE_RETURN) { sendCommand(kResume, 0); return true; } return false;
	}
};
class RecoveryDialog : public GUI::Dialog {
	Common::Array<RecoveryEntry> _entries;
	RecoveryCards *_cards;
	GUI::StaticTextWidget *_title;
	GUI::ButtonWidget *_resume, *_back;
	bool _confirm = false;
	RecoveryEntry _choice;
	void confirmation(bool enabled) {
		_confirm = enabled; _title->setLabel(enabled ? _("Replace current progress? Choose Resume or Back.") : _("Continue - recovery saves"));
		setFocusWidget(enabled ? static_cast<GUI::Widget *>(_back) : _cards); g_gui.scheduleTopDialogRedraw();
	}
public:
	RecoveryDialog(Session &session) : GUI::Dialog(0, 0, 320, 240), _entries(session.recovery.list()) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain;
		_title = new GUI::StaticTextWidget(this, 8, 8, 304, 24, _("Continue - recovery saves"), Graphics::kTextAlignLeft);
		_cards = new RecoveryCards(this, _entries);
		_resume = new GUI::ButtonWidget(this, 8, 208, 148, 24, _("Resume"), Common::U32String(), kResume);
		_back = new GUI::ButtonWidget(this, 164, 208, 148, 24, _("Back"), Common::U32String(), kBack);
		_resume->setEnabled(!_entries.empty());
	}
	void open() override { GUI::Dialog::open(); setFocusWidget(_cards); }
	RecoveryEntry choice() const { return _choice; }
	void reflowLayout() override {
		const int width = MIN<int>(760, g_system->getOverlayWidth() - 12), height = MIN<int>(560, g_system->getOverlayHeight() - 12);
		resize((g_system->getOverlayWidth() - width) / 2, (g_system->getOverlayHeight() - height) / 2, width, height, false);
		_title->setSize(width - 16, 24); _cards->setSize(width - 16, height - 80);
		_resume->setPos(8, height - 32); _resume->setSize((width - 24) / 2, 24);
		_back->setPos(16 + (width - 24) / 2, height - 32); _back->setSize((width - 24) / 2, 24);
		GUI::Dialog::reflowLayout();
	}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (event.customType == kRecoveryClose) { handleCommand(nullptr, kBack, 0); return; }
			if (event.customType == kRecoveryResume) { handleCommand(nullptr, _confirm && getFocusWidget() == _back ? kBack : kResume, 0); return; }
			if (event.customType == kRecoveryPrevious || event.customType == kRecoveryNext) {
				if (_confirm) setFocusWidget(event.customType == kRecoveryPrevious ? static_cast<GUI::Widget *>(_resume) : _back);
				else _cards->move(event.customType == kRecoveryPrevious ? -1 : 1); return;
			}
		}
		GUI::Dialog::handleOtherEvent(event);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kSelected) { if (_confirm) confirmation(false); return; }
		if (cmd == kBack) { if (_confirm) confirmation(false); else close(); return; }
		if (cmd == kResume && !_entries.empty()) {
			if (!_confirm) confirmation(true);
			else { _choice = _entries[_cards->selected()]; close(); } return;
		}
		GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
}
RecoveryEntry openRecovery(Session &session) {
	const bool compact = ConfMan.hasKey("dreamfactory_recovery_test_compact") && ConfMan.getBool("dreamfactory_recovery_test_compact");
	const uint oldWidth = g_system->getWidth(), oldHeight = g_system->getHeight(); const Graphics::PixelFormat oldFormat = g_system->getScreenFormat();
	if (compact) {
		g_system->beginGFXTransaction(); g_system->initSize(480, 272, &oldFormat);
		if (g_system->endGFXTransaction() != OSystem::kTransactionSuccess) { session.runtime.fail("Cannot initialize compact recovery test"); return RecoveryEntry(); }
	}
	RecoveryEntry result;
	{ RecoveryKeymap keys; RecoveryDialog dialog(session); dialog.runModal(); result = dialog.choice(); }
	if (compact) {
		g_system->beginGFXTransaction(); g_system->initSize(oldWidth, oldHeight, &oldFormat);
		if (g_system->endGFXTransaction() != OSystem::kTransactionSuccess) session.runtime.fail("Cannot restore recovery test display");
	}
	return result;
}
}
