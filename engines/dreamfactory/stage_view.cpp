/* SPDX-License-Identifier: GPL-3.0-or-later
 * Layer order and matte retention follow runtime/stage.ts and web/screen-director.ts.
 */
#include "dreamfactory/stage_view.h"
namespace DreamFactory {
const Frame *StageView::frame(const Common::SharedPtr<LoadedStage> &stage, int32 flat) {
	if (stage != _owner) { reset(); _owner = stage; if (stage) _images.resize(stage->data.flats.size()); }
	if (!stage || flat < 0 || uint32(flat) >= _images.size()) return nullptr;
	Frame &image = _images[flat];
	if (image.pixels.empty()) {
		const uint32 ref = stage->data.flats[flat].image;
		if (!ref || !decodeFrame(stage->data.archive.get(ref), image)) return nullptr;
	}
	return &image;
}
bool StageView::matte(const Frame &flat) {
	if (flat.width < 512 || flat.height < 264 || flat.pixels.size() != uint32(flat.width) * flat.height) return false;
	for (uint32 y = 0; y < 264; ++y) for (uint32 x = 0; x < 512; ++x)
		if (flat.pixels[y * flat.width + x] != flat.pixels[0]) return false;
	return true;
}
static void blit(const Frame &frame, const byte *palette, Bytes &rgba) {
	if (!palette || frame.pixels.size() != uint32(frame.width) * frame.height) return;
	for (uint32 y = 0; y < MIN<uint32>(384, frame.height); ++y) for (uint32 x = 0; x < MIN<uint32>(512, frame.width); ++x)
		memcpy(rgba.data() + (y * 512 + x) * 4, palette + frame.pixels[y * frame.width + x] * 3, 3);
}
bool StageView::compose(const Frame *flat, const byte *flatPalette, const Frame *room, const byte *roomPalette, Bytes &rgba, bool holdMatte) {
	if ((!flat && !room) || (holdMatte && flat && !room && matte(*flat))) return false;
	rgba.resize(512 * 384 * 4); memset(rgba.data(), 0, rgba.size());
	for (uint32 i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
	if (flat) blit(*flat, flatPalette, rgba);
	if (room) blit(*room, roomPalette, rgba);
	return true;
}
}
