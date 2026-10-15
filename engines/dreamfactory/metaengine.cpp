/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engines/metaengine.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"
#include "base/plugins.h"
#include "common/translation.h"
#include "dreamfactory/controls.h"
#include "dreamfactory/dreamfactory.h"
#include "engines/advancedDetector.h"
static const ADExtraGuiOptionsMap optionsList[] = {
	{GUIO_GAMEOPTIONS1, {_s("Use enhancement soundtrack"), _s("Play music from the optional enhancement pack, falling back to original music for missing tracks (restart required)"), "dreamfactory_enhanced_music", false, 0, 0}},
	{GUIO_GAMEOPTIONS2, {_s("Larger dialogue text"), _s("Increase dialogue text size; long choices retain their original size to fit"), "dreamfactory_large_text", false, 0, 0}},
	{GUIO_GAMEOPTIONS3, {_s("High contrast dialogue"), _s("Draw dialogue choices in white on black with a yellow selection border"), "dreamfactory_contrast", false, 0, 0}},
	{GUIO_GAMEOPTIONS4, {_s("Enhanced dialogue controls"), _s("Select replies with Up/Down and Enter or number keys; tap or confirm to skip speech"), "dreamfactory_dialogue_controls", false, 0, 0}},
	{GUIO_GAMEOPTIONS5, {_s("Show hotspot hints"), _s("Show markers for sampled clickable objects; the mapped hotspot action toggles visibility"), "dreamfactory_hotspot_hints", false, 0, 0}},
	{GUIO_GAMEOPTIONS6, {_s("Touch direction pad"), _s("Show walking and turning buttons in navigable world views"), "dreamfactory_touch_controls", false, 0, 0}},
	{GUIO_GAMEOPTIONS7, {_s("Record dialogue journal"), _s("Keep heard lines and encountered decisions in your saves; open with J or controller Back"), "dreamfactory_journal", true, 0, 0}},
	{GUIO_GAMEOPTIONS8, {_s("Record branching routes"), _s("Keep discovered conversations across saves and record scene checkpoints for revisiting branches"), "dreamfactory_routes", true, 0, 0}},
	{GUIO_GAMEOPTIONS9, {_s("Modern inventory"), _s("Show a controller-friendly inventory with larger item pictures when opening the original bag"), "dreamfactory_modern_inventory", false, 0, 0}},
	{GUIO_GAMEOPTIONS10, {_s("Offer Continue when starting"), _s("Show saved recovery points before the original opening when available"), "dreamfactory_continue_on_start", false, 0, 0}},
	{GUIO_GAMEOPTIONS11, {_s("Show dialogue replay button"), _s("Show a touch button during speech to fast-forward only previously heard lines"), "dreamfactory_replay_controls", false, 0, 0}},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR};
class DreamFactoryMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override { return "dreamfactory"; }
	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override { return optionsList; }
	Common::KeymapArray initKeymaps(const char *target) const override {
		Common::KeymapArray defaults = MetaEngine::initKeymaps(target);
		Common::Keymap *keyboard = new Common::Keymap(Common::Keymap::kKeymapTypeGame, "engine-default", _("DreamFactory controls"));
		keyboard->setPartialMatchAllowed(false);
		for (const auto *map : defaults)
			for (const auto *old : map->getActions()) {
				Common::Action *action = new Common::Action(old->id, old->description);
				action->setEvent(old->event);
				if (old->shouldTriggerOnKbdRepeats())
					action->allowKbdRepeats();
				for (const auto &input : old->getDefaultInputMapping())
					if (!input.hasPrefix("JOY_"))
						action->addDefaultInputMapping(input);
				keyboard->addAction(action);
			}
		for (auto *map : defaults)
			delete map;
		Common::KeymapArray maps = Common::Keymap::arrayOf(keyboard);
		// Preserve ScummVM's mouse, menu and skip bindings. DreamFactory's
		// scripts use arrow names, rather than the default keypad keycodes.
		const struct Direction {
			const char *id, *keyboard, *keypad;
			Common::KeyCode key;
		} directions[] = {
			{Common::kStandardActionMoveUp, "UP", "KP8", Common::KEYCODE_UP},
			{Common::kStandardActionMoveDown, "DOWN", "KP2", Common::KEYCODE_DOWN},
			{Common::kStandardActionMoveLeft, "LEFT", "KP4", Common::KEYCODE_LEFT},
			{Common::kStandardActionMoveRight, "RIGHT", "KP6", Common::KEYCODE_RIGHT}};
		for (uint32 m = 0; m < maps.size(); ++m) {
			const Common::Keymap::ActionArray &actions = maps[m]->getActions();
			for (uint32 a = 0; a < actions.size(); ++a) {
				if (!strcmp(actions[a]->id, Common::kStandardActionSkip))
					actions[a]->addDefaultInputMapping("JOY_B");
				if (!strcmp(actions[a]->id, Common::kStandardActionOpenMainMenu))
					actions[a]->addDefaultInputMapping("JOY_START");
				if (!strcmp(actions[a]->id, Common::kStandardActionOpenMainMenu))
					actions[a]->setEvent(Common::EVENT_MAINMENU);
				for (uint32 d = 0; d < ARRAYSIZE(directions); ++d)
					if (!strcmp(actions[a]->id, directions[d].id)) {
						actions[a]->setKeyEvent(Common::KeyState(directions[d].key));
						actions[a]->addDefaultInputMapping(directions[d].keyboard);
						actions[a]->addDefaultInputMapping(directions[d].keypad);
					}
			}
		}
		Common::Keymap *extras = maps[0];
		Common::Action *journal = new Common::Action("DFJOURNAL", _("Dialogue journal"));
		journal->setCustomEngineActionEvent(DreamFactory::kOpenJournal);
		journal->addDefaultInputMapping("j");
		journal->addDefaultInputMapping("JOY_BACK");
		extras->addAction(journal);
		Common::Action *inventory = new Common::Action("DFINVENTORY", _("Open inventory"));
		inventory->setCustomEngineActionEvent(DreamFactory::kOpenInventory);
		inventory->addDefaultInputMapping("F8"); inventory->addDefaultInputMapping("JOY_RIGHT_TRIGGER"); extras->addAction(inventory);
		Common::Action *recovery = new Common::Action("DFRECOVERY", _("Continue / recovery saves"));
		recovery->setCustomEngineActionEvent(DreamFactory::kOpenRecovery);
		recovery->addDefaultInputMapping("F9"); recovery->addDefaultInputMapping("JOY_LEFT_TRIGGER"); extras->addAction(recovery);
		Common::Action *heardSkip = new Common::Action("DFHEARDSKIP", _("Toggle fast-forward heard dialogue"));
		heardSkip->setCustomEngineActionEvent(DreamFactory::kToggleHeardSkip);
		heardSkip->addDefaultInputMapping("F10"); heardSkip->addDefaultInputMapping("JOY_RIGHT_STICK"); extras->addAction(heardSkip);
		Common::Action *repeat = new Common::Action("DFREPEAT", _("Repeat last dialogue"));
		repeat->setKeyEvent(Common::KEYCODE_F7);
		repeat->addDefaultInputMapping("F7");
		extras->addAction(repeat);
		Common::Action *hints = new Common::Action(Common::kStandardActionToggleHotspots, _("Toggle hotspot hints"));
		hints->setCustomEngineActionEvent(1);
		hints->addDefaultInputMapping("F6");
		extras->addAction(hints);
		const char *ids[] = {"DFUP", "DFDOWN", "DFLEFT", "DFRIGHT", "DFPREV", "DFNEXT", "DFACT", "DFPADREPEAT", "DFPADCLICK"};
		const char *labels[] = {_s("Walk forward / previous reply"), _s("Down / next reply"), _s("Turn left"), _s("Turn right"), _s("Previous hotspot"), _s("Next hotspot"), _s("Interact / confirm dialogue"), _s("Repeat last dialogue"), _s("Pointer click / skip line")};
		const char *bindings[] = {"JOY_UP", "JOY_DOWN", "JOY_LEFT", "JOY_RIGHT", "JOY_LEFT_SHOULDER", "JOY_RIGHT_SHOULDER", "JOY_A", "JOY_Y", "JOY_X"};
		for (uint32 i = 0; i < ARRAYSIZE(ids); ++i) {
			Common::Action *action = new Common::Action(ids[i], _(labels[i]));
			action->setCustomEngineActionEvent(DreamFactory::kPadUp + i);
			action->addDefaultInputMapping(bindings[i]);
			extras->addAction(action);
		}
		return maps;
	}
	bool hasFeature(MetaEngineFeature f) const override {
		// Engine::getSaveStateName writes target.###. The chooser must use
		// that same pattern when listing saves and reading their metadata.
		return f == kSupportsListSaves || f == kSupportsDeleteSave || f == kSavesSupportMetaInfo || f == kSavesSupportThumbnail || f == kSavesSupportCreationDate || f == kSavesSupportPlayTime || f == kSavesUseExtendedFormat || f == kSimpleSavesNames || f == kSupportsLoadingDuringStartup;
	}
	Common::Error createInstance(OSystem *system, Engine **engine, const ADGameDescription *) const override {
		*engine = new DreamFactory::DreamFactoryEngine(system);
		return Common::kNoError;
	}
};
#if PLUGIN_ENABLED_DYNAMIC(DREAMFACTORY)
REGISTER_PLUGIN_DYNAMIC(DREAMFACTORY, PLUGIN_TYPE_ENGINE, DreamFactoryMetaEngine);
#else
REGISTER_PLUGIN_STATIC(DREAMFACTORY, PLUGIN_TYPE_ENGINE, DreamFactoryMetaEngine);
#endif
