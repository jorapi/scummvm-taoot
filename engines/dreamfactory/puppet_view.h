/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_PUPPET_VIEW_H
#define DREAMFACTORY_PUPPET_VIEW_H
#include "dreamfactory/puppets.h"
#include "dreamfactory/cast.h"
#include "dreamfactory/image.h"
#include "graphics/font.h"
namespace DreamFactory {
class PuppetFonts {
public:
	bool load(const Bytes &bytes);
	const Graphics::Font *font(int32 size = 12, bool screenResolution = false);
private:
	Bytes _bytes;
	Common::HashMap<int32, Common::SharedPtr<Graphics::Font> > _fonts;
};
// Logical conversation screen is 512x384 in both supplied engine versions.
// Keeps room and character palettes separate until the final screen transfer.
class PuppetView {
public:
	bool composite(const Common::SharedPtr<LoadedPuppet> &puppet, Bytes &rgba, const Frame *backdrop = nullptr, const byte *palette = nullptr, bool subtitles = true, const byte *artPalette = nullptr);
	static int32 choiceAt(int32 x, int32 y, uint32 count);
	static Common::Array<Common::String> subtitleLines(const Common::String &text, const Graphics::Font &font);
	static bool overlay(const LoadedPuppet &puppet, Bytes &rgba, const Graphics::Font &font, const Common::HashMap<int32, int32> &params, bool subtitles = true, bool pressing = false, const byte *palette = nullptr, const Graphics::Font *screenFont = nullptr, bool contrast = false, const Graphics::Font *choiceFallback = nullptr);
	void reset() { _owner.reset(); _sprites.clear(); }
private:
	Common::SharedPtr<LoadedPuppet> _owner;
	Common::HashMap<uint32, Sprite> _sprites;
	const byte *_artPalette = nullptr;
	bool sprite(uint32 location, const Sprite *&out);
	void blit(const Sprite &sprite, int32 x, int32 y, int32 clip, Bytes &rgba) const;
};
}
#endif
