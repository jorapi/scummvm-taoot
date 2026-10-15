/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/dialogue.h"
#include "common/config-manager.h"
#include "common/translation.h"
#include "gui/widget.h"
#include "gui/ThemeEval.h"
#include "gui/widgets/popup.h"
#include "gui/editgamedialog.h"
#include "gui/gui-manager.h"
#include "gui/widgets/scrollcontainer.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "image/bmp.h"
namespace DreamFactory {
DialogueStyle dialogueStyle() {
	DialogueStyle style;
	if (ConfMan.hasKey("dreamfactory_text_size")) style.size = ConfMan.getInt("dreamfactory_text_size");
	if (style.size != 14 && style.size != 16 && style.size != 18 && style.size != 20) style.size = 0;
	if (ConfMan.hasKey("dreamfactory_caption_opacity")) style.opacity = CLIP<int32>(ConfMan.getInt("dreamfactory_caption_opacity"), 0, 100);
	style.modern = style.size || style.opacity != 100 || (ConfMan.hasKey("dreamfactory_speaker_labels") && ConfMan.getBool("dreamfactory_speaker_labels"));
	return style;
}
Common::String dialogueSpeaker(Common::String file, const Common::String &nativeName) {
	file.toLowercase();
	for (int32 i = int32(file.size()) - 1; i >= 0; --i) if (file[i] == ':' || file[i] == '/' || file[i] == '\\') { file = file.substr(i + 1); break; }
	if (file.hasSuffix(".pup")) file.erase(file.size() - 4);
	while (!file.empty() && file.lastChar() >= '0' && file.lastChar() <= '9') file.erase(file.size() - 1);
	// Names verified in the supplied characters' introductions. Keep raw IDs
	// unchanged for scripts, saved journals and route-profile identity.
	const struct { const char *id, *name; } names[] = {
		{"penny", "Penny Pringle"}, {"dpenny", "Penny Pringle"}, {"burns", "Eric Burns"}, {"dburns", "Eric Burns"},
		{"smeth", "Smethells"}, {"dsmeth", "Smethells"}, {"claris", "Claris Limehouse"}, {"conk", "Andrew Conkling"},
		{"ga", "Lady Georgia"}, {"trask", "Trask"}, {"vlad", "Vlad"}, {"zeit", "Colonel Zeitel"},
		{"buick", "Buick Riviera"}, {"dell", "Dell Belcher"}, {"marie", "Marie Antoinette Macintosh"},
		{"mwife", "Mrs. Macintosh"}, {"side", "Hiram Sidewinder"}, {"trotter", "Nate Trotter"}, {"pete", "Buckboard Pete"}
	};
	for (const auto &entry : names) if (file == entry.id) return entry.name;
	bool human = !nativeName.empty() && nativeName[0] >= 'A' && nativeName[0] <= 'Z' && !nativeName.equalsIgnoreCase("untitled");
	for (char c : nativeName) if ((c >= '0' && c <= '9') || c == '_' || c == ':' || c == '/' || c == '\\' || c == '.') human = false;
	return human ? nativeName : _("Speaker").encode(Common::kWindows1252);
}
namespace {
class DialogueOptions : public GUI::OptionsContainerWidget {
	ExtraGuiOptions _options;
	Common::HashMap<Common::String, GUI::CheckboxWidget *> _checks;
	GUI::PopUpWidget *_size, *_opacity;
	GUI::CheckboxWidget *_speaker;
public:
	DialogueOptions(GUI::GuiObject *boss, const Common::String &name, const Common::String &target, const ExtraGuiOptions &options) : GUI::OptionsContainerWidget(boss, name, "DreamFactoryOptions", target), _options(options) {
		new GUI::StaticTextWidget(widgetsBoss(), _dialogLayout + ".SizeLabel", _("Dialogue size:"), _("Larger text with full previews of long highlighted replies"));
		_size = new GUI::PopUpWidget(widgetsBoss(), _dialogLayout + ".Size");
		_size->appendEntry(_("Original"), 0); _size->appendEntry(_("Large (14)"), 14); _size->appendEntry(_("Larger (16)"), 16); _size->appendEntry(_("Handheld (18)"), 18); _size->appendEntry(_("Handheld (20)"), 20);
		new GUI::StaticTextWidget(widgetsBoss(), _dialogLayout + ".OpacityLabel", _("Caption background:"), _("Opacity from transparent to solid"));
		_opacity = new GUI::PopUpWidget(widgetsBoss(), _dialogLayout + ".Opacity");
		_speaker = new GUI::CheckboxWidget(widgetsBoss(), _dialogLayout + ".Speaker", _("Show speaker labels"), _("Use original conversation speaker names when available"));
		for (const auto &option : _options) if (option.configOption != "dreamfactory_large_text") _checks[option.configOption] = new GUI::CheckboxWidget(widgetsBoss(), _dialogLayout + "." + option.configOption, _(option.label), _(option.tooltip));
	}
	void defineLayout(GUI::ThemeEval &layouts, const Common::String &name, const Common::String &overlay) const override {
		const int labelWidth = MAX(g_gui.getStringWidth(_("Dialogue size:")), g_gui.getStringWidth(_("Caption background:"))) + 8;
		layouts.addDialog(name, overlay); layouts.addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(0, 0, 0, 0);
		layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0).addWidget("SizeLabel", "OptionsLabel", labelWidth).addWidget("Size", "PopUp").closeLayout();
		layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0).addWidget("OpacityLabel", "OptionsLabel", labelWidth).addWidget("Opacity", "PopUp").closeLayout();
		layouts.addWidget("Speaker", "Checkbox");
		for (const auto &option : _options) if (option.configOption != "dreamfactory_large_text") layouts.addWidget(option.configOption, "Checkbox");
		layouts.closeLayout().closeDialog();
	}
	void load() override {
		int size = ConfMan.hasKey("dreamfactory_text_size", _domain) ? ConfMan.getInt("dreamfactory_text_size", _domain) : ConfMan.getBool("dreamfactory_large_text", _domain) ? 14 : 0;
		if (size != 14 && size != 16 && size != 18 && size != 20) size = 0;
		_size->setSelectedTag(size);
		const int opacity = CLIP<int32>(ConfMan.getInt("dreamfactory_caption_opacity", _domain), 0, 100);
		_opacity->clearEntries();
		for (int value = 100; value >= 0; value -= 25) _opacity->appendEntry(Common::String::format("%d%%", value), value);
		if (opacity % 25) _opacity->appendEntry(Common::String::format("%d%%", opacity), opacity);
		_opacity->setSelectedTag(opacity); _speaker->setState(ConfMan.getBool("dreamfactory_speaker_labels", _domain));
		for (const auto &option : _options) if (_checks.contains(option.configOption)) _checks[option.configOption]->setState(ConfMan.getBool(option.configOption, _domain));
	}
	bool save() override {
		ConfMan.setInt("dreamfactory_text_size", _size->getSelectedTag(), _domain); ConfMan.setBool("dreamfactory_large_text", false, _domain);
		ConfMan.setInt("dreamfactory_caption_opacity", _opacity->getSelectedTag(), _domain); ConfMan.setBool("dreamfactory_speaker_labels", _speaker->getState(), _domain);
		for (const auto &option : _options) if (_checks.contains(option.configOption)) ConfMan.setBool(option.configOption, _checks[option.configOption]->getState(), _domain);
		return true;
	}
	bool hasKeys() override {
		if (ConfMan.hasKey("dreamfactory_text_size", _domain) || ConfMan.hasKey("dreamfactory_caption_opacity", _domain) || ConfMan.hasKey("dreamfactory_speaker_labels", _domain)) return true;
		for (const auto &option : _options) if (ConfMan.hasKey(option.configOption, _domain)) return true;
		return false;
	}
	void setEnabled(bool enabled) override {
		_size->setEnabled(enabled); _opacity->setEnabled(enabled); _speaker->setEnabled(enabled);
		for (const auto &check : _checks) check._value->setEnabled(enabled);
	}
	GUI::Widget *control(uint32 item) const { return item == 0 ? static_cast<GUI::Widget *>(_size) : item == 1 ? static_cast<GUI::Widget *>(_opacity) : _speaker; }
	uint32 sizeTag() const { return _size->getSelectedTag(); }
};
}
GUI::OptionsContainerWidget *dialogueOptions(GUI::GuiObject *boss, const Common::String &name, const Common::String &target, const ExtraGuiOptions &options) { return new DialogueOptions(boss, name, target, options); }
bool auditDialogueOptions(OSystem *system, const Common::FSNode &root, bool compact) {
	const Common::String target = "dialogue-accessibility-test";
	if (ConfMan.hasGameDomain(target)) return false;
	const auto *domain = ConfMan.getActiveDomain();
	if (!domain) return false;
	const Common::ConfigManager::Domain copiedDomain = *domain;
	ConfMan.addGameDomain(target);
	for (const auto &entry : copiedDomain) ConfMan.set(entry._key, entry._value, target);
	ConfMan.removeKey("dreamfactory_text_size", target); ConfMan.setBool("dreamfactory_large_text", true, target); ConfMan.setInt("dreamfactory_caption_opacity", 100, target); ConfMan.setBool("dreamfactory_speaker_labels", false, target);
	const uint oldWidth = system->getWidth(), oldHeight = system->getHeight(); const Graphics::PixelFormat format = system->getScreenFormat();
	if (compact) { system->beginGFXTransaction(); system->initSize(480, 272, &format); if (system->endGFXTransaction() != OSystem::kTransactionSuccess) { ConfMan.removeGameDomain(target); return false; } }
	class OptionsDialog : public GUI::EditGameDialog {
	public:
		explicit OptionsDialog(const Common::String &name) : GUI::EditGameDialog(name) {}
		DialogueOptions *controls() { return static_cast<DialogueOptions *>(_engineOptions); }
		GUI::ScrollContainerWidget *scroll() { return _gameContainer; }
	};
	class Input : public Common::EventSource {
		OSystem *_system; OptionsDialog &_dialog; const Common::FSNode &_root; bool _compact, _cancel, _initialChecked = false, _scrollStopped = false, _scrollStopHeld = false; uint32 _due, _started, _scrollReady = 0; Common::Point _point;
	public:
		uint32 phase = 0; bool failed = false;
		Input(OSystem *s, OptionsDialog &d, const Common::FSNode &r, bool compactSize, bool cancel) : _system(s), _dialog(d), _root(r), _compact(compactSize), _cancel(cancel), _due(s->getMillis() + 100), _started(s->getMillis()) { s->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~Input() { _system->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			if (_system->getMillis() - _started > 12000) { failed = true; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
			if (_system->getMillis() < _due) return false;
			_due = _system->getMillis() + 60;
			auto *controls = _dialog.controls(); if (!controls) { failed = true; _dialog.close(); return false; }
			if (phase == 0) {
				if (!_initialChecked) { if (controls->sizeTag() != (_cancel ? 20u : 14u)) { failed = true; _dialog.close(); return false; } _initialChecked = true; }
				const auto *first = controls->control(0), *last = controls->control(2); const auto *scroll = _dialog.scroll();
				if (last->getAbsY() + last->getHeight() >= scroll->getAbsY() + scroll->getHeight() - 4 || first->getAbsY() < scroll->getAbsY() + 4) {
					_scrollReady = 0;
					event.type = first->getAbsY() < scroll->getAbsY() + 4 ? Common::EVENT_WHEELUP : Common::EVENT_WHEELDOWN; event.mouse = Common::Point(scroll->getAbsX() + 2, scroll->getAbsY() + scroll->getHeight() / 2); return true;
				}
				if (!_scrollReady) { _scrollReady = _system->getMillis() + 800; return false; }
				if (_system->getMillis() < _scrollReady) return false;
				if (!_scrollStopped) {
					_point = Common::Point(scroll->getAbsX() + 2, scroll->getAbsY() + scroll->getHeight() / 2); event.mouse = _point;
					event.type = _scrollStopHeld ? Common::EVENT_LBUTTONUP : Common::EVENT_LBUTTONDOWN;
					if (_scrollStopHeld) _scrollStopped = true; else _scrollStopHeld = true;
					return true;
				}
				if (_compact) {
					if (controls->sizeTag() != (_cancel ? 0u : 20u)) { const auto *widget = controls->control(0); event.type = _cancel ? Common::EVENT_WHEELUP : Common::EVENT_WHEELDOWN; event.mouse = Common::Point(widget->getAbsX() + widget->getWidth() / 2, widget->getAbsY() + widget->getHeight() / 2); return true; }
					phase = 4;
				}
			}
			if (phase == 0 || phase == 4 || phase == 10) {
				auto *widget = controls->control(phase == 0 ? 0 : phase == 4 ? 1 : 2); _point = Common::Point(widget->getAbsX() + widget->getWidth() / 2, widget->getAbsY() + widget->getHeight() / 2);
				event.type = Common::EVENT_LBUTTONDOWN; event.mouse = _point;
			} else if (phase == 1 || phase == 5 || phase == 11) { event.type = Common::EVENT_LBUTTONUP; event.mouse = _point; }
			else if (phase == 2 || phase == 6 || phase == 7 || phase == 8 || phase == 3 || phase == 9) {
				event.type = Common::EVENT_KEYDOWN;
				event.kbd.keycode = phase == 2 ? (_cancel ? Common::KEYCODE_HOME : Common::KEYCODE_END) : phase == 6 || (phase == 8 && _cancel) ? Common::KEYCODE_HOME : phase == 7 || phase == 8 ? Common::KEYCODE_DOWN : Common::KEYCODE_RETURN;
			} else if (phase == 12) {
				Graphics::Surface overlay; overlay.create(_system->getOverlayWidth(), _system->getOverlayHeight(), _system->getOverlayFormat()); _system->grabOverlay(overlay); Common::DumpFile image;
				const Common::String name = Common::String::format("accessibility-options-%s-%s.bmp", _compact ? "handheld" : "desktop", _cancel ? "cancel" : "save");
				if (!image.open(_root.getChild(name)) || !Image::writeBMP(image, overlay)) failed = true; overlay.free();
				if (!_cancel) _dialog.apply(); _dialog.close(); ++phase; return false;
			} else return false;
			++phase; return true;
		}
	};
	bool good = true;
	for (bool cancel : {false, true}) {
		OptionsDialog dialog(target); Input input(system, dialog, root, compact, cancel); dialog.runModal();
		good = good && !input.failed && input.phase == 13 && ConfMan.getInt("dreamfactory_text_size", target) == 20 && ConfMan.getInt("dreamfactory_caption_opacity", target) == 50 && ConfMan.getBool("dreamfactory_speaker_labels", target);
	}
	if (compact) { system->beginGFXTransaction(); system->initSize(oldWidth, oldHeight, &format); good = system->endGFXTransaction() == OSystem::kTransactionSuccess && good; }
	ConfMan.removeGameDomain(target);
	if (good) warning("DIALOGUE OPTIONS PASS shared Edit Game dropdowns legacy larger-text migration size opacity speaker save cancel scroll %s", compact ? "480x272" : "desktop");
	return good;
}
}
