/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/display.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "graphics/paletteman.h"
namespace DreamFactory {
void Display::reset() {
	_indices.resize(640 * 480); memset(_indices.data(), 0, _indices.size());
	_picture.resize(640 * 480 * 4); memset(_picture.data(), 0, _picture.size());
	for (uint32 i = 3; i < _picture.size(); i += 4) _picture[i] = 255;
	memset(_palette, 0, sizeof(_palette)); _saved.clear(); _savedIndices.clear(); _transfer.clear(); _puppet = false; _indexed = true; _held = false;
}
bool Display::rgb() const { return _system && _system->getScreenFormat().bytesPerPixel > 1; }
bool Display::promote() {
	if (!_system) return false;
	if (rgb()) return true;
	const auto formats = _system->getSupportedFormats();
	for (uint32 pass = 0; pass < 2; ++pass) for (auto it = formats.begin(); it != formats.end(); ++it) {
		if (pass == 0 ? it->bytesPerPixel != 4 : it->bytesPerPixel <= 1 || it->bytesPerPixel == 4) continue;
		_system->beginGFXTransaction(); _system->initSize(640, 480, &*it);
		if (_system->endGFXTransaction() == OSystem::kTransactionSuccess && rgb()) return true;
	}
	return false;
}
void Display::colorize() {
	for (uint32 i = 0; i < _indices.size(); ++i) { memcpy(_picture.data() + i * 4, _palette + _indices[i] * 3, 3); _picture[i * 4 + 3] = 255; }
}
void Display::palette(const byte *palette, bool update) {
	memcpy(_palette, palette, sizeof(_palette));
	if (!_system) return;
	if (!rgb()) { _system->getPaletteManager()->setPalette(palette, 0, 256); if (update) _system->updateScreen(); }
	else if (_indexed && update) { colorize(); paint(); }
}
void Display::paint(uint64 numerator, uint64 denominator, const Bytes *picture) {
	if (!_system || !denominator || _held) return;
	if (!rgb()) {
		byte shown[768]; for (uint32 c = 0; c < 768; ++c) shown[c] = byte(uint64(_palette[c]) * numerator / denominator);
		_system->getPaletteManager()->setPalette(shown, 0, 256);
		_system->copyRectToScreen(_indices.data(), 640, 0, 0, 640, 480);
	} else {
		const Graphics::PixelFormat format = _system->getScreenFormat();
		_transfer.resize(640 * 480 * format.bytesPerPixel);
		Graphics::Surface screen; screen.init(640, 480, 640 * format.bytesPerPixel, _transfer.data(), format);
		for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
			const byte *p = (picture ? *picture : _picture).data() + (y * 640 + x) * 4;
			screen.setPixel(x, y, format.RGBToColor(byte(uint64(p[0]) * numerator / denominator), byte(uint64(p[1]) * numerator / denominator), byte(uint64(p[2]) * numerator / denominator)));
		}
		_system->copyRectToScreen(_transfer.data(), 640 * format.bytesPerPixel, 0, 0, 640, 480);
	}
	_system->updateScreen();
}
void Display::indexed(const Bytes &pixels, uint32 width, uint32 height, int32 x, int32 y, const byte *palette, bool black) {
	if (!width || !height || width > 4096 || height > 4096 || pixels.size() != width * height) return;
	for (int32 yy = MAX<int32>(0, -y); yy < MIN<int32>(height, 480 - y); ++yy)
		for (int32 xx = MAX<int32>(0, -x); xx < MIN<int32>(width, 640 - x); ++xx) _indices[(y + yy) * 640 + x + xx] = pixels[yy * width + xx];
	memcpy(_palette, palette, sizeof(_palette)); _puppet = false; _indexed = true; colorize(); paint(black ? 0 : 1, 1);
}
bool Display::scaleRGBA(const Bytes &pixels, uint32 width, uint32 height, Bytes &screen) {
	if (!width || !height || width > 4096 || height > 4096 || pixels.size() != width * height * 4) return false;
	if (width == 640 && height == 480) { screen = pixels; return true; }
	screen.resize(640 * 480 * 4);
	for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
		const uint32 s = ((y * height / 480) * width + x * width / 640) * 4;
		memcpy(screen.data() + (y * 640 + x) * 4, pixels.data() + s, 4);
	}
	return true;
}
bool Display::rgba(const Bytes &pixels, uint32 width, uint32 height, bool black, bool puppet) {
	if (!width || !height || width > 4096 || height > 4096 || pixels.size() != width * height * 4 || !promote()) return false;
	if (puppet && !_puppet) { _saved = _picture; _savedIndices = _indices; _savedIndexed = _indexed; memcpy(_savedPalette, _palette, 768); }
	if (!scaleRGBA(pixels, width, height, _picture)) return false;
	_puppet = puppet; _indexed = false; paint(black ? 0 : 1, 1); return true;
}
void Display::fade(uint64 numerator, uint64 denominator) { paint(numerator, denominator); }
bool Display::capture(Bytes &rgba) const {
	rgba.clear(); if (!_system) return false;
	byte palette[768]; if (!rgb()) _system->getPaletteManager()->grabPalette(palette, 0, 256);
	Graphics::Surface *screen = _system->lockScreen(); if (!screen) return false;
	rgba.resize(640 * 480 * 4);
	for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
		byte *p = rgba.data() + (y * 640 + x) * 4; const uint32 color = screen->getPixel(x, y);
		if (screen->format.bytesPerPixel == 1) memcpy(p, palette + color * 3, 3);
		else screen->format.colorToRGB(color, p[0], p[1], p[2]);
		p[3] = 255;
	}
	_system->unlockScreen(); return true;
}
bool Display::transition(const Bytes &from, const Common::String &effect, uint32 step, uint32 steps, bool lowResolution) {
	if (!steps || step > steps || from.size() != 640 * 480 * 4 || !promote()) return false;
	if (effect == "irisopen" || effect == "irisclose") {
		// TI.EXE 0x43b830 / 0x43b980: rectangular strips/aperture,
		// incremented by dimension / (2 * steps) + 1. The first strip
		// is drawn before waiting; the final pass copies the whole picture.
		const int32 width = lowResolution ? 512 : 640, height = lowResolution ? 384 : 480;
		const int32 dx = MIN<uint64>(width / 2, uint64(width / (2 * steps) + 1) * (step + 1));
		const int32 dy = MIN<uint64>(height / 2, uint64(height / (2 * steps) + 1) * (step + 1));
		const bool open = effect == "irisopen";
		Bytes shown = _picture;
		if (step < steps) for (int32 y = 0; y < 480; ++y) for (int32 x = 0; x < 640; ++x) {
			const int32 xx = x * width / 640, yy = y * height / 480;
			const bool inside = open ? xx >= width / 2 - dx && xx < width / 2 + dx && yy >= height / 2 - dy && yy < height / 2 + dy
			                         : xx >= dx && xx < width - dx && yy >= dy && yy < height - dy;
			if (inside != open) {
				const uint32 p = (y * 640 + x) * 4;
				memcpy(shown.data() + p, from.data() + p, 4);
			}
		}
		paint(1, 1, &shown); return true;
	}
	const uint32 kept = uint32((uint64(640) * (steps - step) + steps / 2) / steps);
	uint32 left = 0, right = 0;
	const bool open = effect == "barndooropen";
	if (open) { left = kept / 2; right = 640 - (kept - left); }
	else if (effect == "barndoorclose") { left = (640 - kept) / 2; right = left + kept; }
	else if (effect == "wipeleft") right = kept;
	else if (effect == "wiperight") { left = 640 - kept; right = 640; }
	else return false;
	Bytes shown = _picture;
	for (uint32 y = 0; y < 480; ++y) {
		const uint32 row = y * 640 * 4;
		if (open) {
			memcpy(shown.data() + row, from.data() + row, left * 4);
			memcpy(shown.data() + row + right * 4, from.data() + row + right * 4, (640 - right) * 4);
		} else memcpy(shown.data() + row + left * 4, from.data() + row + left * 4, (right - left) * 4);
	}
	paint(1, 1, &shown); return true;
}
void Display::clear() {
	memset(_indices.data(), 0, _indices.size()); memset(_picture.data(), 0, _picture.size());
	for (uint32 i = 3; i < _picture.size(); i += 4) _picture[i] = 255;
	if (!_system) return;
	Graphics::Surface *screen = _system->lockScreen(); if (screen) { screen->fillRect(Common::Rect(640, 480), 0); _system->unlockScreen(); } _system->updateScreen();
}
void Display::restore(bool black) {
	if (!_puppet) return;
	if (!_saved.empty()) { _picture = _saved; _indices = _savedIndices; _indexed = _savedIndexed; memcpy(_palette, _savedPalette, 768); _saved.clear(); _savedIndices.clear(); }
	else colorize();
	_puppet = false; paint(black ? 0 : 1, 1);
}
}
