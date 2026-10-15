/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_DIALOGUE_H
#define DREAMFACTORY_DIALOGUE_H
#include "engines/metaengine.h"
class OSystem;
namespace Common { class FSNode; }
namespace DreamFactory {
struct DialogueStyle {
	bool modern = false;
	int32 size = 0, opacity = 100, captionTop = 224, previewChoice = -1;
	Common::String speaker;
};
DialogueStyle dialogueStyle();
Common::String dialogueSpeaker(Common::String file, const Common::String &nativeName);
GUI::OptionsContainerWidget *dialogueOptions(GUI::GuiObject *boss, const Common::String &name, const Common::String &target, const ExtraGuiOptions &options);
bool auditDialogueOptions(OSystem *system, const Common::FSNode &root, bool compact);
}
#endif
