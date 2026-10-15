/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/enhancements.h"
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/file.h"
#include "common/debug.h"
#include "common/textconsole.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "graphics/surface.h"
#include "image/bmp.h"
namespace DreamFactory {
bool EnhancementPack::file(const Common::String &relative, Common::FSNode &node) const {
	if (relative.empty() || relative.size() > 512) return false;
	node = _root; Common::String part;
	for (uint32 i = 0; i <= relative.size(); ++i) {
		const char c = i == relative.size() ? '/' : relative[i];
		if (c == '/') {
			if (part.empty() || part == "." || part == "..") return false;
			node = node.getChild(part); part.clear();
		} else {
			if (byte(c) < 32 || c == ':' || c == '\\') return false;
			part += c;
		}
	}
	return node.exists() && !node.isDirectory();
}
bool EnhancementPack::init(const Common::FSNode &directory, const Common::String &bootMD5) {
	clear(); _root = directory;
	Common::File stream; const Common::FSNode manifest = directory.getChild("pack.ini");
	if (!manifest.exists()) return false;
	_manifest.requireKeyValueDelimiter();
	Common::String format, identity;
	if (!stream.open(manifest) || stream.size() > 256 * 1024 || !_manifest.loadFromStream(stream) ||
		!_manifest.getKey("format", "pack", format) || format != "1" ||
		!_manifest.getKey("boot_md5", "pack", identity) || !identity.equalsIgnoreCase(bootMD5)) {
		warning("DreamFactory: ignoring incompatible enhancement pack"); return false;
	}
	_active = true; return true;
}
static bool frameNumber(const Common::String &text, uint32 &number) {
	if (text.empty()) return false;
	number = 0;
	for (const char *p = text.c_str(); *p; ++p) {
		if (*p < '0' || *p > '9' || number > 32000000u / 10) return false;
		number = number * 10 + *p - '0';
	}
	return number <= 32000000;
}
bool EnhancementPack::artwork(const Common::String &source, const Common::String &flat, const Bytes &originalPixels, Bytes &screen) {
	if (!_active || screen.size() != 640 * 480 * 4 || originalPixels.empty()) return false;
	const Common::String cacheKey = source + "\n" + flat;
	if (_artKey != cacheKey) {
		_artKey = cacheKey; _artPixels.clear();
		for (const auto &section : _manifest.getSections()) {
			if (!section.name.hasPrefix("art-")) continue;
			Common::String key, name, relative, identity, xText, yText;
			if (!_manifest.getKey("source", section.name, key) || !key.equalsIgnoreCase(source) ||
				!_manifest.getKey("flat", section.name, name) || !name.equalsIgnoreCase(flat)) continue;
			Common::MemoryReadStream original(originalPixels.data(), originalPixels.size());
			if (!_manifest.getKey("original_md5", section.name, identity) || !identity.equalsIgnoreCase(Common::computeStreamMD5AsString(original))) break;
			_artX = _artY = 0;
			if ((_manifest.getKey("x", section.name, xText) && !frameNumber(xText, _artX)) ||
				(_manifest.getKey("y", section.name, yText) && !frameNumber(yText, _artY))) break;
			Common::FSNode node; Common::File input;
			if (!_manifest.getKey("file", section.name, relative) || !file(relative, node) || !input.open(node) || input.size() < 54 || input.size() > 2 * 1024 * 1024) break;
			// Bound allocation before invoking the shared decoder. Pack artwork
			// uses uncompressed 24-bit BMP at final game-canvas resolution.
			if (input.readUint16LE() != 0x4d42) break;
			input.seek(14); if (input.readUint32LE() != 40) break;
			_artWidth = input.readUint32LE(); _artHeight = input.readUint32LE();
			if (!_artWidth || !_artHeight || _artWidth > 640 || _artHeight > 480 || _artX > 640 - _artWidth || _artY > 480 - _artHeight ||
				input.readUint16LE() != 1 || input.readUint16LE() != 24 || input.readUint32LE() != 0) break;
			input.seek(0); Image::BitmapDecoder decoder;
			if (!decoder.loadStream(input)) break;
			const Graphics::Surface *image = decoder.getSurface();
			if (!image || image->w != _artWidth || image->h != _artHeight || image->format.bytesPerPixel != 3) break;
			_artPixels.resize(_artWidth * _artHeight * 4);
			for (uint32 y = 0; y < _artHeight; ++y) for (uint32 x = 0; x < _artWidth; ++x) {
				byte *pixel = _artPixels.data() + (y * _artWidth + x) * 4;
				image->format.colorToRGB(image->getPixel(x, y), pixel[0], pixel[1], pixel[2]); pixel[3] = 255;
			}
			break;
		}
	}
	if (_artPixels.empty()) return false;
	for (uint32 y = 0; y < _artHeight; ++y)
		memcpy(screen.data() + ((_artY + y) * 640 + _artX) * 4, _artPixels.data() + y * _artWidth * 4, _artWidth * 4);
	return true;
}
bool EnhancementPack::music(const Common::String &source, Common::Array<Common::SharedPtr<AudioClip> > &parts, int32 &repeat, bool *preserveIntro) const {
	parts.clear(); repeat = -1;
	if (preserveIntro) *preserveIntro = false;
	if (!_active) return false;
	for (const auto &section : _manifest.getSections()) {
		if (!section.name.hasPrefix("music-")) continue;
		Common::String key, relative, startText, endText;
		if (!_manifest.getKey("source", section.name, key) || !key.equalsIgnoreCase(source)) continue;
		// Older apartment packs contain only the piano recording. Preserve its
		// broadcast by default, without imposing original musical lead-ins on
		// other replacements (e.g. DECKC's thirty-second musical introduction).
		Common::String lower = source; lower.toLowercase();
		bool keepIntro = lower == "bedrad1.trk" || lower.hasSuffix("/bedrad1.trk");
		Common::String introText;
		if (_manifest.getKey("preserve_original_intro", section.name, introText)) {
			if (introText != "true" && introText != "false") break;
			keepIntro = introText == "true";
		}
		Common::FSNode node; Common::File input;
		if (!_manifest.getKey("file", section.name, relative) || !file(relative, node) || !input.open(node) || input.size() > 64 * 1024 * 1024) break;
		Common::ScopedPtr<Audio::SeekableAudioStream> stream(Audio::makeWAVStream(&input, DisposeAfterUse::NO));
		if (!stream || stream->getRate() < 1000 || stream->getRate() > 192000) break;
		const uint32 channels = stream->isStereo() ? 2 : 1;
		const int64 frames = stream->getLength().totalNumberOfFrames();
		if (frames <= 0 || frames * channels > 32 * 1024 * 1024) break;
		uint32 start = 0, end = uint32(frames);
		if (_manifest.getKey("loop_start", section.name, startText) && !frameNumber(startText, start)) break;
		if (_manifest.getKey("loop_end", section.name, endText) && !frameNumber(endText, end)) break;
		if (start >= end || end > uint32(frames)) break;
		Common::SharedPtr<AudioClip> decoded(new AudioClip());
		decoded->rate = stream->getRate(); decoded->channels = channels; decoded->samples.resize(uint32(frames) * channels);
		if (stream->readBuffer(decoded->samples.data(), decoded->samples.size()) != int(decoded->samples.size())) break;
		auto slice = [&](uint32 from, uint32 to) {
			Common::SharedPtr<AudioClip> clip(new AudioClip()); clip->rate = decoded->rate; clip->channels = channels;
			clip->samples.resize((to - from) * channels);
			memcpy(clip->samples.data(), decoded->samples.data() + from * channels, clip->samples.size() * sizeof(int16));
			parts.push_back(clip);
		};
		if (start) slice(0, start);
		slice(start, end); repeat = parts.size() - 1;
		if (preserveIntro) *preserveIntro = keepIntro;
		debug(1, "DREAMFACTORY ENHANCED MUSIC source=%s file=%s rate=%u channels=%u loop=%u..%u", source.c_str(), relative.c_str(), decoded->rate, channels, start, end);
		return true;
	}
	return false;
}
}
