/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_STAGE_VIEW_H
#define DREAMFACTORY_STAGE_VIEW_H
#include "dreamfactory/stages.h"
#include "dreamfactory/image.h"
namespace DreamFactory {
// STG/FLT art owns the full logical screen; a visible SET covers its top.
class StageView {
public:
	void reset() { _owner.reset(); _images.clear(); }
	const Frame *frame(const Common::SharedPtr<LoadedStage> &stage, int32 flat);
	static bool matte(const Frame &flat);
	// False means retain the previous screen (nothing, or an uncovered matte).
	static bool compose(const Frame *flat, const byte *flatPalette, const Frame *room, const byte *roomPalette, Bytes &rgba, bool holdMatte = true);
private:
	Common::SharedPtr<LoadedStage> _owner;
	Common::Array<Frame> _images;
};
}
#endif
