/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "base/plugins.h"
#include "engines/advancedDetector.h"

static const PlainGameDescriptor games[] = {
	{ "taoot", "Titanic: Adventure Out of Time" },
	{ "dust", "Dust: A Tale of the Wired West" }, { nullptr, nullptr }
};
static const ADGameDescription descriptions[] = {
	{ "dust", "English retail",
		AD_ENTRY1s("BOOTFILE", "6cb6462aad61c1c1a8f45c17025f2b0d", 12928),
		Common::EN_ANY, Common::kPlatformWindows, ADGF_UNSTABLE, GUIO12(GUIO_NOMIDI, GUIO_GAMEOPTIONS1, GUIO_GAMEOPTIONS2, GUIO_GAMEOPTIONS3, GUIO_GAMEOPTIONS4, GUIO_GAMEOPTIONS5, GUIO_GAMEOPTIONS6, GUIO_GAMEOPTIONS7, GUIO_GAMEOPTIONS8, GUIO_GAMEOPTIONS9, GUIO_GAMEOPTIONS10, GUIO_GAMEOPTIONS11) },
	{ "taoot", "English retail",
		AD_ENTRY1s("BOOTFILE", "9a3eb0be9eb91d5c03d8c6a793808b9e", 95488),
		Common::EN_ANY, Common::kPlatformWindows, ADGF_UNSTABLE, GUIO12(GUIO_NOMIDI GUIO_GAMEOPTIONS12, GUIO_GAMEOPTIONS1, GUIO_GAMEOPTIONS2, GUIO_GAMEOPTIONS3, GUIO_GAMEOPTIONS4, GUIO_GAMEOPTIONS5, GUIO_GAMEOPTIONS6, GUIO_GAMEOPTIONS7, GUIO_GAMEOPTIONS8, GUIO_GAMEOPTIONS9, GUIO_GAMEOPTIONS10, GUIO_GAMEOPTIONS11) },
	{ "taoot", "September 1996 interactive demo",
		AD_ENTRY1s("BOOTFILE", "5caa95340ac6d6410e62137619525f7b", 44864),
		Common::EN_ANY, Common::kPlatformWindows, ADGF_UNSTABLE | ADGF_DEMO, GUIO12(GUIO_NOMIDI GUIO_GAMEOPTIONS12, GUIO_GAMEOPTIONS1, GUIO_GAMEOPTIONS2, GUIO_GAMEOPTIONS3, GUIO_GAMEOPTIONS4, GUIO_GAMEOPTIONS5, GUIO_GAMEOPTIONS6, GUIO_GAMEOPTIONS7, GUIO_GAMEOPTIONS8, GUIO_GAMEOPTIONS9, GUIO_GAMEOPTIONS10, GUIO_GAMEOPTIONS11) },
	AD_TABLE_END_MARKER
};
class DreamFactoryMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	DreamFactoryMetaEngineDetection() : AdvancedMetaEngineDetection(descriptions, games) {}
	const char *getName() const override { return "dreamfactory"; }
	const char *getEngineName() const override { return "CyberFlix DreamFactory"; }
	const char *getOriginalCopyright() const override { return "Titanic (C) 1996 CyberFlix, Inc."; }
};
REGISTER_PLUGIN_STATIC(DREAMFACTORY_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, DreamFactoryMetaEngineDetection);
