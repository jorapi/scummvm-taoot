/* SPDX-License-Identifier: GPL-3.0-or-later
 * Art composition and geometry follow dreamREfactory/web/puppet-view.ts.
 */
#include "dreamfactory/puppet_view.h"
#include "graphics/surface.h"
#include "graphics/fonts/ttf.h"
#include "common/memstream.h"
#include "common/ustr.h"
namespace DreamFactory {
static Common::U32String text(const Common::String &value) { return Common::U32String(value, Common::kWindows1252); }
#ifdef USE_FREETYPE2
static Graphics::Font *loadFont(const Bytes &bytes, int32 size, uint32 dpi = 72) {
	byte *owned = static_cast<byte *>(malloc(bytes.size())); if (!owned) return nullptr;
	memcpy(owned, bytes.data(), bytes.size());
	Common::MemoryReadStream *stream = new Common::MemoryReadStream(owned, bytes.size(), DisposeAfterUse::YES);
	// Keep the original grid-fitted metrics, but blend glyph edges on our RGB
	// canvas instead of magnifying one-bit stair steps on modern displays.
	Graphics::Font *font = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, ABS(size), size < 0 ? Graphics::kTTFSizeModeCell : Graphics::kTTFSizeModeCharacter, dpi, dpi, Graphics::kTTFRenderModeNormalWithGridFitting);
	if (!font) delete stream; // A failed load does not adopt its stream.
	return font;
}
#endif
bool PuppetFonts::load(const Bytes &bytes) {
#ifdef USE_FREETYPE2
	if (bytes.empty() || bytes.size() > 16 * 1024 * 1024) return false;
	Common::SharedPtr<Graphics::Font> probe(loadFont(bytes, 12));
	if (!probe) return false;
	_fonts.clear(); _bytes = bytes; _fonts[12] = probe; return true;
#else
	return false;
#endif
}
const Graphics::Font *PuppetFonts::font(int32 size, bool screenResolution) {
	if (!size) size = 12;
	if (size < -256 || size > 256 || _bytes.empty()) return nullptr;
	const int32 key = size + (screenResolution ? 1024 : 0);
	if (_fonts.contains(key)) return _fonts[key].get();
#ifdef USE_FREETYPE2
	// 90/72 equals the 640/512 screen scale. Rasterize at that size rather
	// than stretching already rasterized 12-pixel glyphs by a fractional factor.
	Common::SharedPtr<Graphics::Font> result(loadFont(_bytes, size, screenResolution ? 90 : 72));
	if (result) { _fonts[key] = result; return result.get(); }
#endif
	return nullptr;
}
Common::Array<Common::String> PuppetView::subtitleLines(const Common::String &value, const Graphics::Font &font) {
	Common::Array<Common::String> lines;
	if (font.getStringWidth(text(value)) < 496) { lines.push_back(value); return lines; }
	for (int32 i = int32(value.size()) - 1; i >= 0; --i) if (value[i] == ' ') {
		const Common::String head = value.substr(0, i);
		if (font.getStringWidth(text(head)) < 496) { lines.push_back(head); lines.push_back(value.substr(i + 1)); return lines; }
	}
	// The original Windows routine draws nothing if an overlong line has no
	// fitting space. Do not silently substitute the browser's CJK wrapper.
	return lines;
}
bool PuppetView::overlay(const LoadedPuppet &puppet, Bytes &rgba, const Graphics::Font &font, const Common::HashMap<int32, int32> &params, bool subtitles, bool pressing, const byte *palette, const Graphics::Font *screenFont, bool contrast, const Graphics::Font *choiceFallback) {
	if (!puppet.visible) return true;
	const int32 width = screenFont ? 640 : 512, height = screenFont ? 480 : 384;
	if (rgba.size() != uint32(width * height * 4)) return false;
	if (!palette) palette = puppet.data.palette;
	Graphics::Surface surface; surface.init(width, height, width * 4, rgba.data(), Graphics::PixelFormat::createFormatRGBA32());
	const Graphics::Font &drawFont = screenFont ? *screenFont : font;
	auto scale = [&](int32 value) { return screenFont ? int32(floor(value * 1.25 + 0.5)) : value; };
	auto param = [&](int32 slot, int32 fallback) { return params.contains(slot) ? params[slot] : fallback; };
	auto ink = [&](int32 index) { return index < 0 || index > 255 ? surface.format.RGBToColor(0, 0, 0) : surface.format.RGBToColor(palette[index * 3], palette[index * 3 + 1], palette[index * 3 + 2]); };
	const int32 ascent = drawFont.getFontAscent() >= 0 ? drawFont.getFontAscent() : drawFont.getFontHeight();
	if (subtitles && !puppet.subtitle.empty()) {
		surface.fillRect(Common::Rect(0, scale(224), width, scale(264)), surface.format.RGBToColor(0, 0, 0));
		const Common::Array<Common::String> lines = subtitleLines(puppet.subtitle, font);
		for (uint32 i = 0; i < lines.size(); ++i) drawFont.drawString(&surface, text(lines[i]), scale(8), scale(240 + i * 16) - ascent, scale(504), contrast ? surface.format.RGBToColor(255, 255, 255) : ink(0), Graphics::kTextAlignLeft, 0, false, true);
	}
	const int32 margin = CLIP<int32>(param(10, 8), -32768, 32767);
	if (contrast && !puppet.choices.empty()) surface.fillRect(Common::Rect(0, scale(264), width, height), surface.format.RGBToColor(0, 0, 0));
	for (uint32 i = 0; i < MIN<uint32>(5, puppet.choices.size()); ++i) {
		const int32 available = MAX<int32>(0, width - scale(margin));
		const Graphics::Font &choiceFont = choiceFallback && drawFont.getStringWidth(text(puppet.choices[i].text)) > available ? *choiceFallback : drawFont;
		const int32 choiceAscent = choiceFont.getFontAscent() >= 0 ? choiceFont.getFontAscent() : choiceFont.getFontHeight();
		choiceFont.drawString(&surface, text(puppet.choices[i].text), scale(margin), scale(280 + i * 24) - choiceAscent, available, contrast ? surface.format.RGBToColor(255, 255, 255) : ink(param(3, 250)), Graphics::kTextAlignLeft, 0, false, true);
	}
	const int32 framed = pressing ? puppet.pressed : puppet.chosen;
	if (framed >= 0 && uint32(framed) < MIN<uint32>(5, puppet.choices.size())) {
		const uint32 color = contrast ? surface.format.RGBToColor(255, 230, 0) : ink(param(4, 251));
		for (int32 y = scale(264 + framed * 24); y < scale(288 + framed * 24); ++y) for (int32 x = 0; x < width; ++x) {
			if (x >= scale(3) && x < scale(509) && y >= scale(267 + framed * 24) && y < scale(285 + framed * 24)) continue;
			if (pressing) { const uint32 d = (y * width + x) * 4; for (uint32 c = 0; c < 3; ++c) rgba[d + c] ^= 255; }
			else surface.setPixel(x, y, color);
		}
	}
	return true;
}
int32 PuppetView::choiceAt(int32 x, int32 y, uint32 count) {
	if (x < 0 || x >= 512 || y < 264 || y >= 384) return -1;
	const int32 row = (y - 264) / 24;
	return uint32(row) < MIN<uint32>(5, count) ? row : -1;
}
bool PuppetView::sprite(uint32 location, const Sprite *&out) {
	out = nullptr;
	if (!location || !_owner->data.archive.has(location)) return true;
	if (!_sprites.contains(location)) {
		Sprite decoded;
		if (!decodeSprite(_owner->data.archive.get(location), decoded)) return false;
		_sprites[location] = decoded;
	}
	out = &_sprites[location]; return true;
}
void PuppetView::blit(const Sprite &image, int32 x, int32 y, int32 clip, Bytes &rgba) const {
	x -= image.x; y -= image.y;
	for (int32 sy = MAX<int32>(0, -y); sy < MIN<int32>(image.height, clip - y); ++sy)
		for (int32 sx = MAX<int32>(0, -x); sx < MIN<int32>(image.width, 512 - x); ++sx) {
			const uint32 s = sy * image.width + sx; if (!image.opaque[s]) continue;
			const uint32 color = image.pixels[s], d = ((y + sy) * 512 + x + sx) * 4;
			// Same reserved endpoints as SET/MOV palettes. Text uses the raw
			// puppet CLUT separately (its index zero is commonly white).
			for (uint32 c = 0; c < 3; ++c) rgba[d + c] = _artPalette ? _artPalette[color * 3 + c] : color == 0 ? 0 : color == 255 ? 255 : _owner->data.palette[color * 3 + c];
			rgba[d + 3] = 255;
		}
}
bool PuppetView::composite(const Common::SharedPtr<LoadedPuppet> &puppet, Bytes &rgba, const Frame *backdrop, const byte *palette, bool subtitles, const byte *artPalette) {
	if (!puppet || !puppet->visible) return true;
	if (_owner != puppet) { reset(); _owner = puppet; }
	_artPalette = artPalette;
	if (rgba.size() != 512 * 384 * 4) { rgba.resize(512 * 384 * 4); memset(rgba.data(), 0, rgba.size()); }
	// The answer band's transparent pixels preserve the caller's world layer.
	memset(rgba.data(), 0, 512 * 264 * 4);
	if (backdrop && palette) {
		if (backdrop->pixels.size() != uint32(backdrop->width) * backdrop->height) return false;
		for (uint32 y = 0; y < MIN<uint32>(264, backdrop->height); ++y)
			for (uint32 x = 0; x < MIN<uint32>(512, backdrop->width); ++x) {
				const uint32 color = backdrop->pixels[y * backdrop->width + x], d = (y * 512 + x) * 4;
				memcpy(rgba.data() + d, palette + color * 3, 3); rgba[d + 3] = 255;
			}
	}
	const int32 clip = subtitles && !puppet->subtitle.empty() ? 224 : 264;
	if (clip == 224) for (uint32 d = 224 * 512 * 4; d < 264 * 512 * 4; d += 4) { memset(rgba.data() + d, 0, 3); rgba[d + 3] = 255; }
	if (puppet->hasPose && !puppet->data.stances.empty()) {
		const uint32 stance = puppet->stance >= 0 && uint32(puppet->stance) < puppet->data.stances.size() ? puppet->stance : 0;
		for (uint32 l = 0; l < 11; ++l) {
			const PuppetPose::Layer &pose = puppet->pose.layers[l]; const PuppetLayer &layer = puppet->data.stances[stance].layers[l];
			if (pose.frame < 0 || layer.frames.empty()) continue;
			const Sprite *image; if (!sprite(layer.frames[MIN<uint32>(pose.frame, layer.frames.size() - 1)], image)) return false;
			if (!image) continue;
			if (!l) {
				bool matte = true;
				for (uint32 i = 1; i < image->pixels.size(); ++i) if (image->opaque[i] && image->pixels[i] != image->pixels[0]) { matte = false; break; }
				if (matte) continue;
			}
			blit(*image, pose.x, pose.y, clip, rgba);
		}
	}
	const Sprite *band; if (!sprite(puppet->data.band, band)) return false;
	if (band) blit(*band, 256, 324, 384, rgba);
	return true;
}
}
