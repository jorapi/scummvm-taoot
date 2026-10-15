/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/inventory.h"
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
#include "gui/widgets/list.h"
#include "gui/widgets/richtext.h"
#include "gui/widgets/scrollbar.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
namespace DreamFactory {
namespace {
enum { kUse = 'IUse', kExamine = 'IExm' };
class InventoryKeymap {
	Common::Keymap *_standard, *_inventory;
	bool _standardEnabled, _inventoryEnabled;
public:
	InventoryKeymap() {
		auto *mapper = g_system->getEventManager()->getKeymapper();
		_standard = mapper->getKeymap(Common::kGuiKeymapName);
		if (!_standard) { _standard = g_gui.getKeymap(); mapper->addGlobalKeymap(_standard); }
		_inventory = mapper->getKeymap("dreamfactory-inventory-ui");
		if (!_inventory) {
			_inventory = new Common::Keymap(Common::Keymap::kKeymapTypeGui, "dreamfactory-inventory-ui", _("DreamFactory inventory"));
			const char *ids[] = {"INVUSE", "INVEXAM", "INVCLOSE", "INVBACK", "INVUP", "INVDOWN", "INVPREV", "INVNEXT"};
			const char *labels[] = {_s("Use / give selected item"), _s("Examine selected item"), _s("Close inventory"), _s("Close inventory"), _s("Previous item"), _s("Next item"), _s("Previous item"), _s("Next item")};
			const char *bindings[] = {"JOY_A", "JOY_Y", "JOY_B", "JOY_BACK", "JOY_UP", "JOY_DOWN", "JOY_LEFT_SHOULDER", "JOY_RIGHT_SHOULDER"};
			const uint32 actions[] = {kInventoryUIUse, kInventoryUIExamine, kInventoryUIClose, kInventoryUIClose, kInventoryUIUp, kInventoryUIDown, kInventoryUIUp, kInventoryUIDown};
			for (uint32 i = 0; i < ARRAYSIZE(ids); ++i) {
				auto *action = new Common::Action(ids[i], _(labels[i])); action->setCustomEngineActionEvent(actions[i]); action->addDefaultInputMapping(bindings[i]);
				if (i >= 4) action->allowKbdRepeats();
				if (i == 4 || i == 5) action->addDefaultInputMapping(i == 4 ? "JOY_LEFT_STICK_Y-" : "JOY_LEFT_STICK_Y+");
				_inventory->addAction(action);
			}
			for (const auto *old : _standard->getActions()) {
				auto *action = new Common::Action(old->id, old->description); action->setEvent(old->event);
				if (old->shouldTriggerOnKbdRepeats()) action->allowKbdRepeats();
				for (const auto &binding : old->getDefaultInputMapping()) if (!binding.hasPrefix("JOY_")) action->addDefaultInputMapping(binding);
				_inventory->addAction(action);
			}
			_inventory->setEnabled(false); mapper->addGlobalKeymap(_inventory);
		}
		_standardEnabled = _standard->isEnabled(); _inventoryEnabled = _inventory->isEnabled();
		_standard->setEnabled(false); _inventory->setEnabled(true);
	}
	~InventoryKeymap() { _standard->setEnabled(_standardEnabled); _inventory->setEnabled(_inventoryEnabled); }
};
class ItemPicture : public GUI::Widget {
	Bytes _picture;
	uint32 _width = 0, _height = 0;
protected:
	void drawWidget() override {
		Graphics::ManagedSurface surface(_w, _h, g_system->getOverlayFormat());
		surface.fillRect(Common::Rect(_w, _h), surface.format.RGBToColor(17, 26, 34));
		if (!_picture.empty()) {
			const int width = MIN<int>(_w - 16, (_h - 16) * _width / _height), height = width * _height / _width;
			for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
				const int sx = CLIP<int>((2 * x + 1) * int(_width) * 128 / width - 128, 0, (_width - 1) * 256);
				const int sy = CLIP<int>((2 * y + 1) * int(_height) * 128 / height - 128, 0, (_height - 1) * 256);
				uint32 channels[3] = {}; const uint32 background[] = {17, 26, 34};
				for (uint32 dy = 0; dy < 2; ++dy) for (uint32 dx = 0; dx < 2; ++dx) {
					const byte *pixel = _picture.data() + (MIN<uint32>(_height - 1, (sy >> 8) + dy) * _width + MIN<uint32>(_width - 1, (sx >> 8) + dx)) * 4;
					const uint32 weight = (dx ? sx & 255 : 256 - (sx & 255)) * (dy ? sy & 255 : 256 - (sy & 255));
					for (uint32 c = 0; c < 3; ++c) channels[c] += ((pixel[c] * pixel[3] + background[c] * (255 - pixel[3])) / 255) * weight;
				}
				surface.setPixel((_w - width) / 2 + x, (_h - height) / 2 + y, surface.format.RGBToColor(channels[0] >> 16, channels[1] >> 16, channels[2] >> 16));
			}
		}
		g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
	}
public:
	ItemPicture(GUI::Dialog *boss) : GUI::Widget(boss, 8, 32, 128, 96) { setFlags(GUI::WIDGET_CLEARBG); }
	void setItem(Session &session, const Common::String &name) { inventoryPicture(session, name, _picture, _width, _height); markAsDirty(); }
};
class InventoryList : public GUI::Widget, public GUI::CommandSender {
	Common::U32StringArray _labels;
	GUI::ScrollBarWidget *_scroll;
	int _selected = -1, _position = 0, _downY = 0, _startPosition = 0;
	bool _held = false, _dragging = false;
	const Graphics::Font &font() const {
		const Graphics::Font &normal = g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal);
		const Graphics::Font *large = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		return normal.getFontHeight() < 12 && large ? *large : normal;
	}
	int rowHeight() const { return font().getFontHeight() + 10; }
	void scroll(int position) {
		_position = CLIP(position, 0, MAX(0, int(_labels.size()) * rowHeight() - _h));
		_scroll->_currentPos = _position; _scroll->recalc(); markAsDirty();
	}
protected:
	GUI::Widget *findWidget(int x, int y) override { if (x >= _w - 16 && _scroll->isVisible()) return _scroll; return this; }
	void drawWidget() override {
		Graphics::ManagedSurface surface(_w - 18, _h, g_system->getOverlayFormat());
		const uint32 background = surface.format.RGBToColor(17, 26, 34), ink = surface.format.RGBToColor(240, 232, 211);
		surface.fillRect(Common::Rect(surface.w, surface.h), background);
		for (uint32 i = 0; i < _labels.size(); ++i) {
			const int top = int(i) * rowHeight() - _position;
			if (top + rowHeight() <= 0 || top >= _h) continue;
			if (int(i) == _selected) surface.fillRect(Common::Rect(2, MAX(0, top), surface.w - 2, MIN<int>(_h, top + rowHeight())), surface.format.RGBToColor(49, 62, 74));
			font().drawString(&surface, _labels[i], 8, top + 5, surface.w - 16, ink, Graphics::kTextAlignLeft, 0, true);
		}
		g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
	}
public:
	InventoryList(GUI::Dialog *boss) : GUI::Widget(boss, 8, 32, 130, 160), GUI::CommandSender(boss) {
		setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_CLEARBG | GUI::WIDGET_TRACK_MOUSE | GUI::WIDGET_HOOK_DRAG);
		_scroll = new GUI::ScrollBarWidget(this, 114, 0, 16, 160); _scroll->setTarget(this);
	}
	bool wantsFocus() override { return true; }
	bool containsWidget(GUI::Widget *widget) const override { return widget == _scroll; }
	void setList(const Common::U32StringArray &labels) { _labels = labels; _selected = -1; _position = 0; }
	int getSelected() const { return _selected; }
	void setSelected(int selected) {
		_selected = selected >= 0 && uint32(selected) < _labels.size() ? selected : -1;
		if (_selected >= 0) {
			if (_selected * rowHeight() < _position) scroll(_selected * rowHeight());
			else if ((_selected + 1) * rowHeight() > _position + _h) scroll((_selected + 1) * rowHeight() - _h);
		}
		markAsDirty(); sendCommand(GUI::kListSelectionChangedCmd, _selected);
	}
	void reflowLayout() override {
		GUI::Widget::reflowLayout(); _scroll->setPos(_w - 16, 0); _scroll->setSize(16, _h);
		_scroll->_numEntries = _labels.size() * rowHeight(); _scroll->_entriesPerPage = _h; _scroll->_singleStep = rowHeight();
		_scroll->setVisible(_scroll->_numEntries > _h); scroll(_position);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override { if (cmd == GUI::kSetPositionCmd) scroll(data); }
	void handleMouseWheel(int x, int y, int direction) override { scroll(_position + direction * rowHeight() * 2); }
	void handleMouseDown(int x, int y, int button, int clicks) override { _held = true; _dragging = false; _downY = y; _startPosition = _position; }
	void handleMouseMoved(int x, int y, int button) override { if (_held && ABS(y - _downY) > 5) { _dragging = true; scroll(_startPosition + _downY - y); } }
	void handleMouseUp(int x, int y, int button, int clicks) override {
		if (_held && !_dragging && y >= 0 && y < _h) { setSelected((y + _position) / rowHeight()); if (clicks == 2 && _selected >= 0) sendCommand(kUse, 0); }
		_held = _dragging = false;
	}
	void cancelDrag() override { _held = _dragging = false; }
	bool handleKeyDown(Common::KeyState key) override {
		if (key.keycode == Common::KEYCODE_UP || key.keycode == Common::KEYCODE_DOWN) { if (!_labels.empty()) setSelected(CLIP(_selected + (key.keycode == Common::KEYCODE_UP ? -1 : 1), 0, int(_labels.size()) - 1)); return true; }
		if (key.keycode == Common::KEYCODE_RETURN && _selected >= 0) { sendCommand(kUse, 0); return true; }
		if (key.keycode == Common::KEYCODE_HOME || key.keycode == Common::KEYCODE_END) { setSelected(key.keycode == Common::KEYCODE_HOME ? 0 : int(_labels.size()) - 1); return true; }
		return false;
	}
};
class InventoryDialog : public GUI::Dialog {
	Session &_session;
	Common::Array<InventoryItem> _items;
	InventoryList *_list;
	GUI::StaticTextWidget *_title;
	GUI::RichTextWidget *_help;
	GUI::ButtonWidget *_examine, *_use, *_back;
	ItemPicture *_picture;
	InventoryResult _result;
	int _preview = -2;
	void select() {
		const int index = _list->getSelected(); const bool valid = index >= 0 && uint32(index) < _items.size();
		if (_preview == index) return;
		_preview = index;
		_title->setLabel(valid ? Common::U32String(_items[index].title, Common::kWindows1252) : _("Inventory is empty"));
		_use->setEnabled(valid); _examine->setEnabled(valid && _items[index].examinable);
		_picture->setItem(_session, valid ? _items[index].name : Common::String());
		Common::U32String text = valid ? _(_items[index].description.c_str()) : _("No carried items are available here."); text += _("\n\nA: Use/Give  Y: Examine  B: Back");
		removeWidget(_help); delete _help; _help = new GUI::RichTextWidget(this, 148, 152, 150, 40, text);
		if (isVisible()) { reflowLayout(); g_gui.scheduleTopDialogRedraw(); }
	}
public:
	InventoryDialog(Session &session) : GUI::Dialog(0, 0, 320, 240), _session(session) {
		_backgroundType = GUI::ThemeEngine::kDialogBackgroundMain; inventoryItems(session, _items);
		_title = new GUI::StaticTextWidget(this, 8, 8, 300, 20, _("Inventory"), Graphics::kTextAlignLeft);
		_list = new InventoryList(this);
		_picture = new ItemPicture(this);
		_help = new GUI::RichTextWidget(this, 148, 152, 150, 40, _("**A: Use/Give  Y: Examine**\n\nChoose a target, then A. B cancels."));
		_examine = new GUI::ButtonWidget(this, 8, 208, 96, 24, _("Examine"), Common::U32String(), kExamine);
		_use = new GUI::ButtonWidget(this, 112, 208, 96, 24, _("Use / Give"), Common::U32String(), kUse);
		_back = new GUI::ButtonWidget(this, 216, 208, 96, 24, _("Back"), Common::U32String(), GUI::kCloseCmd);
		Common::U32StringArray labels; for (const auto &item : _items) labels.push_back(Common::U32String(item.title, Common::kWindows1252));
		_list->setList(labels);
		int selected = 0; const auto held = session.runtime.globals.find("handitem");
		for (uint32 i = 0; i < _items.size(); ++i) if (held != session.runtime.globals.end() && held->_value.text() == _items[i].name) selected = i;
		if (!_items.empty()) _list->setSelected(selected); select();
	}
	InventoryResult choice() const { return _result; }
	void open() override { GUI::Dialog::open(); setFocusWidget(_list); }
	void reflowLayout() override {
		const int width = MIN<int>(760, g_system->getOverlayWidth() - 12), height = MIN<int>(560, g_system->getOverlayHeight() - 12);
		resize((g_system->getOverlayWidth() - width) / 2, (g_system->getOverlayHeight() - height) / 2, width, height, false);
		const int row = MAX(24, g_gui.getFontHeight() + 12), split = width * 42 / 100;
		const auto place = [](GUI::Widget *widget, int x, int y, int w, int h) { widget->setPos(x, y); widget->setSize(w, h); };
		place(_title, 8, 8, width - 16, row); place(_list, 8, row + 12, split - 12, height - row * 2 - 28);
		place(_picture, split + 4, row + 12, width - split - 12, MAX(32, height - row * 5 - 32));
		place(_help, split + 4, height - row * 4 - 12, width - split - 12, row * 3);
		const int buttonWidth = (width - 32) / 3;
		place(_examine, 8, height - row - 8, buttonWidth, row); place(_use, 16 + buttonWidth, height - row - 8, buttonWidth, row); place(_back, 24 + buttonWidth * 2, height - row - 8, buttonWidth, row);
		GUI::Dialog::reflowLayout();
	}
	void handleOtherEvent(const Common::Event &event) override {
		if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
			if (event.customType == kInventoryUIClose) { close(); return; }
			if (event.customType == kInventoryUIUse || event.customType == kInventoryUIExamine) { handleCommand(nullptr, event.customType == kInventoryUIUse ? kUse : kExamine, 0); return; }
			if (event.customType == kInventoryUIUp || event.customType == kInventoryUIDown) {
				if (!_items.empty()) _list->setSelected((_list->getSelected() + int(_items.size()) + (event.customType == kInventoryUIUp ? -1 : 1)) % int(_items.size())); select(); return;
			}
		}
		GUI::Dialog::handleOtherEvent(event);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == GUI::kListSelectionChangedCmd) { select(); return; }
		if (cmd == kUse || cmd == kExamine || cmd == GUI::kListItemActivatedCmd) {
			const int index = _list->getSelected();
			if (index < 0 || uint32(index) >= _items.size() || (cmd == kExamine && !_items[index].examinable)) return;
			_result.item = _items[index].name; _result.title = _items[index].title; _result.action = cmd == kExamine ? kInventoryExamine : kInventoryUse; close(); return;
		}
		GUI::Dialog::handleCommand(sender, cmd, data);
	}
};
}
InventoryResult openInventory(Session &session) {
	const bool compact = ConfMan.hasKey("dreamfactory_inventory_test_compact") && ConfMan.getBool("dreamfactory_inventory_test_compact");
	const uint oldWidth = g_system->getWidth(), oldHeight = g_system->getHeight(); const Graphics::PixelFormat oldFormat = g_system->getScreenFormat();
	if (compact) {
		g_system->beginGFXTransaction(); g_system->initSize(480, 272, &oldFormat);
		if (g_system->endGFXTransaction() != OSystem::kTransactionSuccess) { session.runtime.fail("Cannot initialize compact inventory test"); return InventoryResult(); }
	}
	InventoryResult result;
	{ InventoryKeymap keys; InventoryDialog dialog(session); dialog.runModal(); result = dialog.choice(); }
	if (compact) {
		g_system->beginGFXTransaction(); g_system->initSize(oldWidth, oldHeight, &oldFormat);
		if (g_system->endGFXTransaction() != OSystem::kTransactionSuccess) session.runtime.fail("Cannot restore inventory test display");
	}
	return result;
}
}
