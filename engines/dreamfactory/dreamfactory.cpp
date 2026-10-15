/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/dreamfactory.h"
#include "dreamfactory/controls.h"
#include "dreamfactory/inventory.h"
#include "dreamfactory/dialogue.h"
#include "dreamfactory/leads.h"
#include "dreamfactory/script_audit.h"
#include "dreamfactory/session.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "engines/util.h"
#include "engines/metaengine.h"
#include "graphics/paletteman.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "graphics/surface.h"
#include "backends/keymapper/keymapper.h"
#include "backends/keymapper/hardware-input.h"
#include "gui/gui-manager.h"
#include "gui/ThemeEval.h"
#include "image/bmp.h"
#include <math.h>

namespace DreamFactory {
void DreamFactoryEngine::pauseEngineIntern(bool pause) { Engine::pauseEngineIntern(pause); if (!pause && _session) _session->resetClock(); }
bool DreamFactoryEngine::canSaveGameStateCurrently(Common::U32String *) { return _session && (_scriptSave || _session->canSave()); }
bool DreamFactoryEngine::canSaveAutosaveCurrently() {
	const bool safe = _session && _session->canAutosave();
	_retryAutosave = _session && !safe; return safe;
}
void DreamFactoryEngine::serviceAutosave() {
	if (!_session || !_session->canAutosave() || !ConfMan.getInt("autosave_period")) return;
	// The shared event loop may first reach the deadline inside a script.
	// Retry that deferred request at the next safe world view.
	const uint32 started = _system->getMillis();
	if (_retryAutosave) { _retryAutosave = false; saveAutosaveIfEnabled(); }
	else handleAutoSave();
	_session->excludeWallTime(started);
}
bool DreamFactoryEngine::canLoadGameStateCurrently(Common::U32String *) { return _session && !_session->dialogActive() && !_session->movies.active() && !_session->puppetSpeech.active() && !_session->puppetChoices.active(); }
bool DreamFactoryEngine::saveGame() {
	_scriptSave = true;
	const bool diagnostic = ConfMan.hasKey("dreamfactory_boot_trace") && ConfMan.getBool("dreamfactory_boot_trace") && ConfMan.hasKey("dreamfactory_boot_dialog_slot");
	const bool result = diagnostic ? saveGameState(ConfMan.getInt("dreamfactory_boot_dialog_slot"), "DreamFactory control-panel test").getCode() == Common::kNoError : saveGameDialog();
	_scriptSave = false; return result;
}
bool DreamFactoryEngine::loadGame() {
	const bool diagnostic = ConfMan.hasKey("dreamfactory_boot_trace") && ConfMan.getBool("dreamfactory_boot_trace") && ConfMan.hasKey("dreamfactory_boot_dialog_slot");
	return diagnostic ? loadGameState(ConfMan.getInt("dreamfactory_boot_dialog_slot")).getCode() == Common::kNoError : loadGameDialog();
}
Common::Error DreamFactoryEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	const uint32 started = _system->getMillis();
	Bytes bytes; Common::String message;
	if (!_session) return Common::kWritingFailed;
	if (!_session->saveState(bytes, message)) { if (isAutosave) _session->excludeWallTime(started); return Common::Error(Common::kWritingFailed, message); }
	if (stream->write(bytes.data(), bytes.size()) != bytes.size() || stream->err()) { if (isAutosave) _session->excludeWallTime(started); return Common::kWritingFailed; }
	if (isAutosave && _session->canAutosave() && !_session->recordRecovery(bytes, message)) warning("Recovery save: %s", message.c_str());
	if (isAutosave) _session->excludeWallTime(started);
	return Common::kNoError;
}
Common::Error DreamFactoryEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (!_session || stream->size() - stream->pos() < 28) return Common::kReadingFailed;
	const int64 start = stream->pos(); stream->skip(8); const uint32 size = stream->readUint32LE(); stream->seek(start);
	if (size > 128 * 1024 * 1024 || uint64(stream->size() - start) < uint64(size) + 28) return Common::kReadingFailed;
	Bytes bytes; bytes.resize(size + 28);
	if (stream->read(bytes.data(), bytes.size()) != bytes.size()) return Common::kReadingFailed;
	Common::String message;
	return _session->loadState(bytes, message) ? Common::kNoError : Common::Error(Common::kReadingFailed, message);
}

static bool depthCodecSelfTest() {
	// Raw colour rows, followed by two independent RLE depth rows.
	const byte encoded[] = {2,0,3,0,4,1,2,3,4,4,5,6,4,0,9,0,2,1,7,2,9,1,3,11};
	Bytes data; data.resize(sizeof(encoded)); memcpy(data.data(), encoded, sizeof(encoded));
	Frame frame;
	if (!decodeFrame(data, frame) || frame.depth.size() != 6) return false;
	const byte expected[] = {7,9,9,11,11,11};
	if (memcmp(frame.depth.data(), expected, sizeof(expected))) return false;
	const Frame saved = frame;
	for (uint32 n = 13; n < data.size(); ++n) {
		Bytes truncated = data; truncated.resize(n);
		if (decodeFrame(truncated, frame) || frame.width != saved.width || frame.height != saved.height ||
			frame.pixels != saved.pixels || frame.depth != saved.depth) return false;
	}
	Bytes bad = data; bad[12] = 0; // Offset into the row table.
	if (decodeFrame(bad, frame)) return false;
	bad = data; bad[17] = 4; // Row overruns the image width.
	if (decodeFrame(bad, frame)) return false;
	bad = data; bad[17] = 0; // Row does not cover the image width.
	if (decodeFrame(bad, frame)) return false;
	bad = data; bad[14] = 4; // Shared row offsets are legal.
	if (!decodeFrame(bad, frame) || memcmp(frame.depth.data(), expected, 3) || memcmp(frame.depth.data() + 3, expected, 3)) return false;
	data.resize(12);
	if (!decodeFrame(data, frame) || !frame.depth.empty() || frame.pixels != saved.pixels) return false;
	warning("DEPTH CODEC PASS row offsets shared rows truncation bounds transactional failure absent plane");
	return true;
}
void DreamFactoryEngine::draw() {
	Graphics::Surface screen;
	screen.create(640, 480, Graphics::PixelFormat::createFormatCLUT8());
	screen.fillRect(Common::Rect(640, 480), 0);
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
	font->drawString(&screen, "DreamFactory development viewer - scripts are not running", 8, 8, 624, 255);
	Common::String where = Common::String::format("%s / %s / %s", _room.name.c_str(), _room.scenes[_scene].name.c_str(), _room.scenes[_scene].views[_view].name.c_str());
	font->drawString(&screen, where, 8, 24, 624, 255);
	font->drawString(&screen, "Left/Right: turn   Up: walk   PgUp/PgDn: room   Tab: scene   Esc: exit", 8, 456, 624, 255);
	_system->copyRectToScreen(screen.getPixels(), screen.pitch, 0, 0, 640, 480);
	screen.free();
	if (!_frame.pixels.empty()) {
		Bytes scaled; scaled.resize(uint32(_room.width) * _room.height);
		for (uint32 y = 0; y < _room.height; ++y)
			for (uint32 x = 0; x < _room.width; ++x)
				scaled[y * _room.width + x] = _frame.pixels[(y * _frame.height / _room.height) * _frame.width + x * _frame.width / _room.width];
		_system->copyRectToScreen(scaled.data(), _room.width, (640 - _room.width) / 2, (480 - _room.height) / 2, _room.width, _room.height);
	}
	_system->updateScreen();
}
bool DreamFactoryEngine::animate(const Sequence &frames) {
	for (uint32 i = 0; i < frames.size() && !shouldQuit(); ++i) {
		if (!frames[i].image) continue; // Unrendered authored camera entries.
		if (!decodeFrame(_room.archive.get(frames[i].image), _frame)) {
			warning("DreamFactory: could not decode container %u", frames[i].image); return false;
		}
		draw();
		if (ConfMan.hasKey("dreamfactory_smoke") && ConfMan.getBool("dreamfactory_smoke")) continue;
		uint32 until = _system->getMillis() + 33;
		while (!shouldQuit() && int32(until - _system->getMillis()) > 0) {
			Common::Event event;
			while (_eventMan->pollEvent(event))
				if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE) quitGame();
			_system->delayMillis(5);
		}
	}
	return !shouldQuit();
}
bool DreamFactoryEngine::stand(uint32 scene, uint32 view) {
	if (!_room.stand(scene, view, _frame)) return false;
	_scene = scene; _view = view; draw(); return true;
}
bool DreamFactoryEngine::openSet(uint32 index) {
	Common::File file;
	if (!file.open(Common::FSNode(_sets[index]))) return false;
	if (file.size() <= 0 || file.size() > 128 * 1024 * 1024) return false;
	Bytes data; data.resize(uint32(file.size()));
	if (file.read(data.data(), data.size()) != data.size() || !_room.load(data)) return false;
	_set = index;
	_system->getPaletteManager()->setPalette(_room.palette, 0, 256);
	for (uint32 s = 0; s < _room.scenes.size(); ++s) {
		if (!_room.scenes[s].name.equalsIgnoreCase(_room.defaultScene)) continue;
		for (uint32 v = 0; v < _room.scenes[s].views.size(); ++v)
			if (_room.scenes[s].views[v].name.equalsIgnoreCase(_room.defaultView) && stand(s, v)) return true;
	}
	for (uint32 s = 0; s < _room.scenes.size(); ++s)
		for (uint32 v = 0; v < _room.scenes[s].views.size(); ++v) if (stand(s, v)) return true;
	return false;
}
void DreamFactoryEngine::turn(int dir) {
	if (_room.version == 1) { moveV1(dir); return; }
	const Sequence &ring = _room.scenes[_scene].turns[dir];
	for (uint32 i = 0; i < ring.size(); ++i) {
		if (ring[i].view != int32(_view) || ring[i].motion <= 0) continue;
		Sequence frames;
		for (uint32 step = 1; step <= ring.size(); ++step) {
			const MotionFrame &f = ring[(i + step) % ring.size()]; frames.push_back(f);
			if (f.view >= 0 && f.motion > 0) {
				if (animate(frames)) stand(_scene, uint32(f.view));
				return;
			}
		}
	}
}
void DreamFactoryEngine::walk() {
	if (_room.version == 1) { moveV1(2); return; }
	int32 id = _room.scenes[_scene].views[_view].id;
	for (uint32 i = 0; i < _room.roads.size(); ++i) {
		const Road &r = _room.roads[i]; int dir;
		if (r.from == id) { dir = 0; }
		else if (r.to == id) { dir = 1; }
		else continue;
		if (r.walks[dir].empty()) continue;
		int32 scene, view;
		Sequence frames;
		if (_room.roadSequence(r, dir, frames, scene, view)) {
			if (animate(frames)) stand(scene, view);
			return;
		}
	}
}
void DreamFactoryEngine::moveV1(int direction) {
	int32 id = _room.scenes[_scene].views[_view].id;
	for (uint32 m = 0; m < _room.moves.size(); ++m) {
		const Move &move = _room.moves[m];
		if (move.from != id || move.direction != direction) continue;
		for (uint32 s = 0; s < _room.scenes.size(); ++s)
			for (uint32 v = 0; v < _room.scenes[s].views.size(); ++v)
				if (_room.scenes[s].views[v].id == move.to) {
					if (animate(move.frames)) stand(s, v);
					return;
				}
	}
}
Common::Error DreamFactoryEngine::run() {
	initGraphics(640, 480);
	debug(1, "DREAMFACTORY VIDEO bpp=%u overlay=%u", _system->getScreenFormat().bytesPerPixel, _system->isOverlayVisible());
	if (ConfMan.hasKey("dreamfactory_shop_audit") && ConfMan.getBool("dreamfactory_shop_audit")) {
		auditShops(Common::FSNode(ConfMan.getPath("path")));
		if (ConfMan.hasKey("dreamfactory_prop_fixtures")) auditProps(Common::FSNode(ConfMan.getPath("dreamfactory_prop_fixtures")));
		quitGame(); return Common::kNoError;
	}
	Common::FSNode root(ConfMan.getPath("path"));
	if (ConfMan.hasKey("dreamfactory_puppet_audit") && ConfMan.getBool("dreamfactory_puppet_audit")) {
		auditPuppets(root);
		if (ConfMan.hasKey("dreamfactory_puppet_fixtures")) auditPuppetSession(Common::FSNode(ConfMan.getPath("dreamfactory_puppet_fixtures")), _system);
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_screen_audit") && ConfMan.getBool("dreamfactory_screen_audit")) {
		auditScreens(Common::FSNode(ConfMan.getPath("dreamfactory_screen_fixtures")), _system);
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_cursor_audit") && ConfMan.getBool("dreamfactory_cursor_audit")) {
		auditCursors(root, Common::FSNode(ConfMan.getPath("dreamfactory_cursor_fixtures")), _system);
		quitGame(); return Common::kNoError;
	}
	Common::FSList files;
	root.getChildren(files, Common::FSNode::kListFilesOnly);
	if (ConfMan.hasKey("dreamfactory_movie_audit") && ConfMan.getBool("dreamfactory_movie_audit")) {
		auditMovies(root);
		if (ConfMan.hasKey("dreamfactory_movie_fixtures")) auditMoviePlayback(Common::FSNode(ConfMan.getPath("dreamfactory_movie_fixtures")), _system, ConfMan.hasKey("dreamfactory_movie_retail") && ConfMan.getBool("dreamfactory_movie_retail"));
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_cast_audit") && ConfMan.getBool("dreamfactory_cast_audit")) {
		auditCasts(root);
		if (ConfMan.hasKey("dreamfactory_actor_fixtures")) auditActors(Common::FSNode(ConfMan.getPath("dreamfactory_actor_fixtures")), _system);
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_room_audit") && ConfMan.getBool("dreamfactory_room_audit")) {
		auditRooms(root);
		if (ConfMan.hasKey("dreamfactory_room_fixtures")) auditRoomSession(Common::FSNode(ConfMan.getPath("dreamfactory_room_fixtures")), _system);
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_audio_audit") && ConfMan.getBool("dreamfactory_audio_audit")) {
		auditAudio(root);
		if (ConfMan.hasKey("dreamfactory_audio_fixtures")) auditSound(root, Common::FSNode(ConfMan.getPath("dreamfactory_audio_fixtures")), _system);
		if (ConfMan.hasKey("dreamfactory_cricket_fixtures")) auditCrickets(Common::FSNode(ConfMan.getPath("dreamfactory_cricket_fixtures")), _system, ConfMan.hasKey("dreamfactory_cricket_retail") && ConfMan.getBool("dreamfactory_cricket_retail"), ConfMan.hasKey("dreamfactory_cricket_demo") && ConfMan.getBool("dreamfactory_cricket_demo"));
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_boot_trace") && ConfMan.getBool("dreamfactory_boot_trace")) {
		Session session(_system, this); _session = &session;
		// Optional input for unattended startup tracing. It uses the authored
		// PLAYMODE region, and is never enabled in the ordinary engine loop.
		class BootInput : public Common::EventSource {
		public:
			Session &session; OSystem *backend; bool enabled, sent = false; Common::String clickMovies, skipMovies, lastClick, lastSkip; uint32 releaseAt = 0, moveAt = 0; Common::Point releasePoint;
			Common::Array<Common::String> choices, optionalChoices; uint32 choiceIndex = 0; uint64 lastChoice = 0;
			Common::Array<Common::String> dialogAnswers; uint32 dialogIndex = 0, dialogDue = 0; uint64 lastDialog = 0, pendingDialog = 0;
			bool dialogKeyRelease = false; Common::KeyState dialogKey;
			Common::Array<Common::String> guiKeys; uint32 guiKeyIndex = 0, guiKeyDue = 0;
			bool guiKeyRelease = false; Common::KeyState guiKey;
			uint32 guiMouseReleaseAt = 0; Common::Point guiMousePoint;
			uint32 guiMouseMoveAt = 0; Common::Point guiDragPoint;
			bool menuPending = false, menuSeen = false, menuEscapeSent = false, menuKeyRelease = false;
			uint32 menuEscapeAt = 0;
			Common::Array<Common::String> movieClicks; uint32 movieClickIndex = 0, nextMovieClick = 0;
			Common::Array<Common::String> modalClicks; uint32 modalClickIndex = 0, nextModalClick = 0;
			uint32 modalButtonIndex = 0xffffffffu; uint64 modalFirstButtonQuery = 0;
			Common::String lastModalProp; bool modalPropSeen = false;
			uint32 nextModalStatus = 0;
			bool skipSpeech = false; uint32 nextSpeechSkip = 0;
			Common::Array<Common::Point> dragPoints; uint32 dragIndex = 0;
			Common::String holdGlobal; int32 holdMinimum = 0;
			Common::String holdProp, holdOwner;
			uint32 nextClickAt = 0, nextMoveAt = 0; Common::Point nextClickPoint;
			bool testController = false;
			BootInput(Session &s, OSystem *b, bool on, const Common::String &movies, const Common::String &skips) : session(s), backend(b), enabled(on), clickMovies(movies), skipMovies(skips) {
				if (ConfMan.hasKey("dreamfactory_boot_gui_keys") && ConfMan.get("dreamfactory_boot_gui_keys").contains("pad:")) {
					auto *inputs = new Common::CompositeHardwareInputSet(); if (auto *native = backend->getHardwareInputSet()) inputs->addHardwareInputSet(native);
					inputs->addHardwareInputSet(new Common::JoystickHardwareInputSet(Common::defaultJoystickButtons, Common::defaultJoystickAxes));
					backend->getEventManager()->getKeymapper()->registerHardwareInputSet(inputs, backend->getKeymapperDefaultBindings()); testController = true;
				}
				backend->getEventManager()->getEventDispatcher()->registerSource(this, false);
			}
			~BootInput() { backend->getEventManager()->getEventDispatcher()->unregisterSource(this); if (testController) backend->getEventManager()->getKeymapper()->registerHardwareInputSet(backend->getHardwareInputSet(), backend->getKeymapperDefaultBindings()); }
			void clickAt(int32 x, int32 y) {
				Common::Event event; event.type = Common::EVENT_LBUTTONDOWN;
				event.mouse = Common::Point(int16((x * 640 + 511) / 512), int16((y * 480 + 383) / 384));
				session.postEvent(event); releasePoint = event.mouse; releaseAt = backend->getMillis() + 80;
			}
			void dragTo(int32 x, int32 y) {
				releasePoint = Common::Point(int16((x * 640 + 511) / 512), int16((y * 480 + 383) / 384));
				moveAt = backend->getMillis() + 40;
			}
			void dragArc(const Common::Point &start, int32 x, int32 y, int32 delta, int32 steps) {
				dragPoints.clear(); dragIndex = 0;
				const double initial = atan2(double(start.y - y), double(start.x - x));
				for (int32 i = 1; i <= steps; ++i) {
					const double angle = initial + i * delta * M_PI / 128;
					const int32 px = CLIP<int32>(int32(floor(x + 24 * cos(angle) + 0.5)), 0, 511);
					const int32 py = CLIP<int32>(int32(floor(y + 24 * sin(angle) + 0.5)), 0, 383);
					dragPoints.push_back(Common::Point(int16((px * 640 + 511) / 512), int16((py * 480 + 383) / 384)));
				}
				moveAt = backend->getMillis() + 100; releaseAt = backend->getMillis() + (steps + 2) * 100;
			}
			void thenClick(int32 x, int32 y, uint32 delay) {
				nextClickPoint = Common::Point(int16((x * 640 + 511) / 512), int16((y * 480 + 383) / 384));
				nextClickAt = backend->getMillis() + delay; nextMoveAt = nextClickAt - 100;
			}
			bool allowMapping() const override { return false; }
			bool pollEvent(Common::Event &event) override {
				if (guiMouseMoveAt && int32(backend->getMillis() - guiMouseMoveAt) >= 0) {
					guiMouseMoveAt = 0; guiMousePoint = guiDragPoint; event.type = Common::EVENT_MOUSEMOVE; event.mouse = guiMousePoint; return true;
				}
				if (guiMouseReleaseAt) {
					if (int32(backend->getMillis() - guiMouseReleaseAt) < 0) return false;
					guiMouseReleaseAt = 0; event.type = Common::EVENT_LBUTTONUP; event.mouse = guiMousePoint; return true;
				}
				if (guiKeyRelease) { guiKeyRelease = false; event.type = Common::EVENT_KEYUP; event.kbd = guiKey; return true; }
				if (g_gui.isActive() && guiKeyIndex < guiKeys.size()) {
					if (!guiKeyDue) guiKeyDue = backend->getMillis() + 200;
					if (int32(backend->getMillis() - guiKeyDue) < 0) return false;
					const Common::String key = guiKeys[guiKeyIndex++]; guiKeyDue = backend->getMillis() + 200;
					if (key.hasPrefix("pad:")) {
						const char *names[] = {"a", "b", "y", "up", "down", "lb", "rb", "x", "left", "right", "l3", "r3"};
						const byte buttons[] = {Common::JOYSTICK_BUTTON_A, Common::JOYSTICK_BUTTON_B, Common::JOYSTICK_BUTTON_Y, Common::JOYSTICK_BUTTON_DPAD_UP, Common::JOYSTICK_BUTTON_DPAD_DOWN, Common::JOYSTICK_BUTTON_LEFT_SHOULDER, Common::JOYSTICK_BUTTON_RIGHT_SHOULDER, Common::JOYSTICK_BUTTON_X, Common::JOYSTICK_BUTTON_DPAD_LEFT, Common::JOYSTICK_BUTTON_DPAD_RIGHT, Common::JOYSTICK_BUTTON_LEFT_STICK, Common::JOYSTICK_BUTTON_RIGHT_STICK};
						for (uint32 i = 0; i < ARRAYSIZE(names); ++i) if (key.substr(4) == names[i]) {
							Common::Event physical; physical.type = Common::EVENT_JOYBUTTON_DOWN; physical.joystick.button = buttons[i];
							Common::List<Common::Event> mapped; backend->getEventManager()->getKeymapper()->mapEvent(physical, mapped);
							if (mapped.size() != 1) { session.runtime.fail("Diagnostic pad input has no unique GUI mapping"); event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
							event = mapped.front(); warning("BOOT GUI PAD input=%s action=%u", names[i], event.customType); return true;
						}
						session.runtime.fail("Unknown diagnostic pad input"); return false;
					}
					if (key == "snapshot") {
						Graphics::Surface overlay; overlay.create(backend->getOverlayWidth(), backend->getOverlayHeight(), backend->getOverlayFormat()); backend->grabOverlay(overlay);
						const Common::String path = ConfMan.get("dreamfactory_boot_checkpoint") + Common::String::format(".gui-%u.bmp", guiKeyIndex); Common::DumpFile file;
						if (!file.open(Common::Path(path)) || !Image::writeBMP(file, overlay)) session.runtime.fail("Cannot capture diagnostic GUI");
						overlay.free(); return false;
					}
					if (key.hasPrefix("point:") || key.hasPrefix("drag:")) {
						Common::StringTokenizer coordinates(key, ":"); coordinates.nextToken(); const int x = atoi(coordinates.nextToken().c_str()), y = atoi(coordinates.nextToken().c_str());
						guiMousePoint = Common::Point(x, y); guiMouseReleaseAt = backend->getMillis() + 100;
						if (key.hasPrefix("drag:")) { guiDragPoint = Common::Point(x, atoi(coordinates.nextToken().c_str())); guiMouseMoveAt = backend->getMillis() + 40; }
						event.type = Common::EVENT_LBUTTONDOWN; event.mouse = guiMousePoint; warning("BOOT GUI POINTER %s", key.c_str()); return true;
					}
					if (key.hasPrefix("wheel:")) {
						event.type = atoi(key.substr(6).c_str()) > 0 ? Common::EVENT_WHEELDOWN : Common::EVENT_WHEELUP; event.mouse = guiMousePoint; warning("BOOT GUI POINTER %s", key.c_str()); return true;
					}
					if (key == "gridfirst" || key.hasPrefix("click:")) {
						const Common::String widget = key == "gridfirst" ? "SaveLoadChooser.List" : key.substr(6);
						const Common::String dialog = widget.substr(0, widget.find('.'));
						int16 dx, dy, dw, dh, x, y, w, h;
						if (!g_gui.xmlEval()->getWidgetData(dialog, dx, dy, dw, dh) || !g_gui.xmlEval()->getWidgetData(widget, x, y, w, h) || w <= 0 || h <= 0) { session.runtime.fail("Cannot locate GUI widget: " + widget); return false; }
						guiMousePoint = Common::Point(dx + x + (key == "gridfirst" ? 40 : w / 2), dy + y + (key == "gridfirst" ? 40 : h / 2)); guiMouseReleaseAt = backend->getMillis() + 80;
						event.type = Common::EVENT_LBUTTONDOWN; event.mouse = guiMousePoint;
						warning("BOOT GUI CLICK widget=%s x=%d y=%d", widget.c_str(), event.mouse.x, event.mouse.y); return true;
					}
					if (key == "escape") guiKey = Common::KeyState(Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE);
					else if (key == "enter") guiKey = Common::KeyState(Common::KEYCODE_RETURN, Common::ASCII_RETURN);
					else if (key == "home") guiKey = Common::KeyState(Common::KEYCODE_HOME);
					else if (key == "end") guiKey = Common::KeyState(Common::KEYCODE_END);
					else if (key == "down") guiKey = Common::KeyState(Common::KEYCODE_DOWN);
					else if (key.size() == 1 && key[0] >= 'a' && key[0] <= 'z') guiKey = Common::KeyState(Common::KeyCode(Common::KEYCODE_a + key[0] - 'a'), byte(key[0]));
					else { session.runtime.fail("Invalid diagnostic GUI key: " + key); return false; }
					event.type = Common::EVENT_KEYDOWN; event.kbd = guiKey; guiKeyRelease = true;
					warning("BOOT GUI KEY %s", key.c_str()); return true;
				}
				if (menuKeyRelease) { menuKeyRelease = false; event.type = Common::EVENT_KEYUP; event.kbd = Common::KeyState(Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE); return true; }
				if (menuPending) {
					menuPending = false;
					Common::Event raw; raw.type = Common::EVENT_KEYDOWN; raw.kbd = Common::KeyState(Common::KEYCODE_F5);
					Common::List<Common::Event> mapped;
					backend->getEventManager()->getKeymapper()->mapEvent(raw, mapped);
					bool found = false;
					for (auto it = mapped.begin(); it != mapped.end(); ++it) if (it->type == Common::EVENT_MAINMENU) { event = *it; found = true; }
					raw.type = Common::EVENT_KEYUP; mapped.clear();
					backend->getEventManager()->getKeymapper()->mapEvent(raw, mapped);
					if (!found) { session.runtime.fail("F5 did not map to the ScummVM menu"); return false; }
					return true;
				}
				if (!menuEscapeSent && menuEscapeAt && g_gui.isActive()) {
					if (!menuSeen) { menuSeen = true; menuEscapeAt = backend->getMillis() + 300; }
					if (int32(backend->getMillis() - menuEscapeAt) >= 0) {
						menuEscapeSent = true; menuKeyRelease = true;
						event.type = Common::EVENT_KEYDOWN; event.kbd = Common::KeyState(Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE); return true;
					}
				}
				if (dialogKeyRelease) { dialogKeyRelease = false; event.type = Common::EVENT_KEYUP; event.kbd = dialogKey; return true; }
				if (session.dialogActive() && lastDialog != session.dialogGeneration() && dialogIndex < dialogAnswers.size()) {
					if (pendingDialog != session.dialogGeneration()) { pendingDialog = session.dialogGeneration(); dialogDue = backend->getMillis() + 200; }
					if (int32(backend->getMillis() - dialogDue) < 0) return false;
					const Common::String answer = dialogAnswers[dialogIndex++]; lastDialog = session.dialogGeneration();
					const bool accept = answer.equalsIgnoreCase("yes") || answer.equalsIgnoreCase("ok");
					if (!accept && !answer.equalsIgnoreCase("no")) session.runtime.fail("Invalid diagnostic dialog answer: " + answer);
					dialogKey = Common::KeyState(accept ? Common::KEYCODE_RETURN : Common::KEYCODE_ESCAPE, accept ? Common::ASCII_RETURN : Common::ASCII_ESCAPE);
					event.type = Common::EVENT_KEYDOWN; event.kbd = dialogKey; dialogKeyRelease = true;
					warning("BOOT DIALOG ANSWER %s", answer.c_str()); return true;
				}
				if (!holdProp.empty()) {
					const auto prop = session.propSystem.get(holdProp);
					if (prop && prop->owner.text().equalsIgnoreCase(holdOwner)) {
						releaseAt = backend->getMillis(); moveAt = 0; dragPoints.clear();
					}
				}
				if (nextMoveAt && int32(backend->getMillis() - nextMoveAt) >= 0) { nextMoveAt = 0; event.type = Common::EVENT_MOUSEMOVE; event.mouse = nextClickPoint; return true; }
				if (nextClickAt && int32(backend->getMillis() - nextClickAt) >= 0) { nextClickAt = 0; event.type = Common::EVENT_LBUTTONDOWN; event.mouse = nextClickPoint; releasePoint = nextClickPoint; releaseAt = backend->getMillis() + 80; return true; }
				if (moveAt && int32(backend->getMillis() - moveAt) >= 0) {
					if (dragIndex < dragPoints.size()) { releasePoint = dragPoints[dragIndex++]; moveAt = dragIndex < dragPoints.size() ? backend->getMillis() + 100 : 0; }
					else moveAt = 0;
					event.type = Common::EVENT_MOUSEMOVE; event.mouse = releasePoint; return true;
				}
				if (!holdGlobal.empty() && session.runtime.globals.contains(holdGlobal) && session.runtime.globals[holdGlobal].number() >= holdMinimum) releaseAt = backend->getMillis();
				if (releaseAt && int32(backend->getMillis() - releaseAt) >= 0) { releaseAt = 0; holdGlobal.clear(); holdProp.clear(); holdOwner.clear(); event.type = Common::EVENT_LBUTTONUP; event.mouse = releasePoint; return true; }
				if (skipSpeech && session.puppetSpeech.active() && int32(backend->getMillis() - nextSpeechSkip) >= 0) {
					nextSpeechSkip = backend->getMillis() + 100;
					warning("BOOT SPEECH ESC"); event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true;
				}
				const auto puppet = session.puppetSystem.current;
				if (!lastModalProp.empty()) {
					if (session.propSystem.running(lastModalProp, "mousedown")) modalPropSeen = true;
					else if (modalPropSeen || !releaseAt) {
						lastModalProp.clear(); modalPropSeen = false;
						// A panel selection can return without yielding, so it
						// may never be observed running. Its released click is
						// also a safe boundary before the next modal press.
						// Let handleselect leave its release loop before the next press.
						nextModalClick = backend->getMillis() + 200;
					}
				}
				const auto inventory = session.propSystem.findShop("inven");
				const bool selecting = inventory && inventory->main && session.runtime.isRunning(*inventory->main, "handleselect");
				// Inventory selectors can run inside a puppet or a stage handler.
				// The outer action loop is blocked, so deliver ordered physical clicks
				// from the event source, gated by the actual nested flat and hit test.
				if ((selecting || (!puppet && session.runtime.busy() && !session.screenBlack)) && lastModalProp.empty() &&
					!releaseAt && !session.movies.active() &&
					!session.puppetSpeech.active() && modalClickIndex < modalClicks.size() &&
					int32(backend->getMillis() - nextModalClick) >= 0) {
					Common::StringTokenizer cue(modalClicks[modalClickIndex], ":");
					const Common::String flat = cue.nextToken(), type = cue.nextToken(), name = cue.nextToken(), handler = cue.empty() ? Common::String() : cue.nextToken();
					if (flat.empty() || (type != "prop" && type != "button") || name.empty() || !cue.empty()) {
						session.runtime.fail("Invalid diagnostic modal click: " + modalClicks[modalClickIndex]); return false;
					}
					// A fourth field gates a non-selector panel by its actual
					// nested handler (for example poker's viewcards wait).
					bool eligible = handler.empty() ? selecting : !puppet && session.runtime.isRunning(handler) && session.pollingButton() && !session.pointerDown;
					if (eligible && !handler.empty()) {
						// Let a release check observe the released opening click.
						// The next query belongs to the panel's press wait.
						if (modalButtonIndex != modalClickIndex) { modalButtonIndex = modalClickIndex; modalFirstButtonQuery = session.buttonQuerySequence(); }
						eligible = session.buttonQuerySequence() != modalFirstButtonQuery;
					}
					if (eligible && session.stageSystem.currentFlat().equalsIgnoreCase(flat)) {
						for (int32 y = 0; y < 384; y += 4) for (int32 x = 0; x < 512; x += 4) {
							const auto hit = session.hitTest(x, y);
							if (hit.type != type || !hit.name.equalsIgnoreCase(name)) continue;
							warning("BOOT MODAL CLICK flat=%s type=%s name=%s", flat.c_str(), type.c_str(), name.c_str());
							++modalClickIndex; nextModalClick = backend->getMillis() + 100;
							lastModalProp = type == "prop" ? name : Common::String();
							modalPropSeen = false;
							event.type = Common::EVENT_LBUTTONDOWN;
							event.mouse = Common::Point(int16((x * 640 + 511) / 512), int16((y * 480 + 383) / 384));
							releasePoint = event.mouse; releaseAt = backend->getMillis() + 80; return true;
						}
					}
				}
				if (int32(backend->getMillis() - nextModalStatus) >= 0 &&
					(session.puppetChoices.active() || session.movies.active() || (puppet && session.runtime.busy()))) {
					nextModalStatus = backend->getMillis() + 5000;
					warning("BOOT MODAL puppet=%s choices=%u movie=%s frame=%u flat=%s selecting=%u handitem=%s", puppet ? puppet->file.c_str() : "", session.puppetChoices.active() ? 1 : 0, session.movies.currentMovie().c_str(), session.movies.currentFrame(), session.stageSystem.currentFlat().c_str(), selecting ? 1 : 0, session.runtime.globals.contains("handitem") ? session.runtime.globals["handitem"].text().c_str() : "");
				}
				if (puppet && puppet->visible && session.puppetChoices.active() && !session.puppetSpeech.active() && lastChoice != session.puppetChoices.generation() && (choiceIndex < choices.size() || !optionalChoices.empty())) {
					Common::String token = choiceIndex < choices.size() ? choices[choiceIndex] : Common::String();
					uint32 colon = token.find(':'); int32 optionalIndex = -1;
					if (!token.empty() && colon == Common::String::npos) { session.runtime.fail("Invalid diagnostic choice: " + token); return false; }
					if (token.empty() || !puppet->file.equalsIgnoreCase(token.substr(0, colon))) {
						// Authored encounters can interrupt a fixed route. Consume only
						// an explicitly planned optional reply, keeping required replies.
						for (uint32 i = 0; i < optionalChoices.size(); ++i) {
							const uint32 split = optionalChoices[i].find(':');
							if (split != Common::String::npos && puppet->file.equalsIgnoreCase(optionalChoices[i].substr(0, split))) { token = optionalChoices[i]; colon = split; optionalIndex = int32(i); break; }
						}
						if (optionalIndex < 0) {
							if (choiceIndex < choices.size()) session.runtime.fail("Diagnostic puppet does not match choice: " + token);
							return false;
						}
					}
					const int32 id = atoi(token.substr(colon + 1).c_str());
					for (uint32 row = 0; row < puppet->choices.size(); ++row) if (puppet->choices[row].id == id) {
						lastChoice = session.puppetChoices.generation();
						if (optionalIndex >= 0) optionalChoices.remove_at(uint32(optionalIndex)); else ++choiceIndex;
						warning("BOOT CHOICE file=%s id=%d text=%s optional=%u", puppet->file.c_str(), id, puppet->choices[row].text.c_str(), optionalIndex >= 0);
						event.type = Common::EVENT_LBUTTONDOWN; event.mouse = Common::Point(320, int16((276 + row * 24) * 480 / 384));
						releasePoint = event.mouse; releaseAt = backend->getMillis() + 80; return true;
					}
					session.runtime.fail("Diagnostic puppet choice unavailable: " + token); return false;
				}
				if (!session.movies.displayedFrames) return false;
				const Common::String movie = session.movies.currentMovie();
				const MovieSegment *seg = session.movies.currentSegment();
				Common::StringTokenizer skips(skipMovies, ",");
				while (!skips.empty()) {
					if (movie.equalsIgnoreCase(skips.nextToken()) && seg && seg->keySkips) {
						const Common::String token = Common::String::format("%s:%u", movie.c_str(), seg->bias);
						if (lastSkip != token) { lastSkip = token; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
					}
				}
				const bool chooseGame = enabled && !sent && movie.hasSuffixIgnoreCase("playmode.mov");
				bool scripted = false; Common::StringTokenizer names(clickMovies, ",");
				while (!names.empty()) if (movie.equalsIgnoreCase(names.nextToken())) scripted = true;
				Common::String wanted;
				if (movieClickIndex < movieClicks.size()) {
					const Common::String &click = movieClicks[movieClickIndex]; const uint32 colon = click.find(':');
					if (colon != Common::String::npos && movie.equalsIgnoreCase(click.substr(0, colon))) wanted = click.substr(colon + 1);
				}
				if (!chooseGame && !scripted && wanted.empty()) return false;
				if (!seg || session.movies.currentFrame() >= seg->frames.size() || seg->frames[session.movies.currentFrame()].regions.empty()) return false;
				const auto &regions = seg->frames[session.movies.currentFrame()].regions;
				const MovieRegion *selected = &regions[0];
				if (!wanted.empty()) {
					if (int32(backend->getMillis() - nextMovieClick) < 0) return false;
					selected = nullptr;
					if (wanted[0] == '#') {
						const int32 ordinal = atoi(wanted.c_str() + 1);
						if (ordinal > 0 && uint32(ordinal) <= regions.size()) selected = &regions[ordinal - 1];
					} else for (const auto &region : regions) if (region.target.equalsIgnoreCase(wanted) || region.event.equalsIgnoreCase(wanted) || region.sound.equalsIgnoreCase(wanted)) { selected = &region; break; }
					if (!selected) return false;
				}
				const Common::String token = Common::String::format("%s:%u:%u", movie.c_str(), seg->bias, session.movies.currentFrame());
				if (wanted.empty() && !chooseGame && lastClick == token) return false;
				lastClick = token;
				const MovieRegion &region = *selected;
				const uint32 width = session.movies.canvasWidth(), height = width == 640 ? 480 : 384;
				event.type = Common::EVENT_LBUTTONDOWN; event.mouse.x = int16((seg->x + (region.left + region.right) / 2) * 640 / width);
				event.mouse.y = int16((seg->y + (region.top + region.bottom) / 2) * 480 / height);
				if (!wanted.empty()) {
					warning("BOOT MOVIE CLICK file=%s region=%s", movie.c_str(), wanted.c_str()); ++movieClickIndex;
					nextMovieClick = backend->getMillis() + 100; releasePoint = event.mouse; releaseAt = backend->getMillis() + 80;
				}
				if (chooseGame) sent = true; return true;
			}
		} input(session, _system, ConfMan.hasKey("dreamfactory_boot_choose_game") && ConfMan.getBool("dreamfactory_boot_choose_game"), ConfMan.hasKey("dreamfactory_boot_click_movies") ? ConfMan.get("dreamfactory_boot_click_movies") : Common::String(), ConfMan.hasKey("dreamfactory_boot_skip_movies") ? ConfMan.get("dreamfactory_boot_skip_movies") : Common::String());
		if (ConfMan.hasKey("dreamfactory_boot_choices")) { Common::StringTokenizer choices(ConfMan.get("dreamfactory_boot_choices"), ","); while (!choices.empty()) input.choices.push_back(choices.nextToken()); }
		if (ConfMan.hasKey("dreamfactory_boot_optional_choices")) { Common::StringTokenizer choices(ConfMan.get("dreamfactory_boot_optional_choices"), ","); while (!choices.empty()) input.optionalChoices.push_back(choices.nextToken()); }
		if (ConfMan.hasKey("dreamfactory_boot_dialog_answers")) { Common::StringTokenizer answers(ConfMan.get("dreamfactory_boot_dialog_answers"), ","); while (!answers.empty()) input.dialogAnswers.push_back(answers.nextToken()); }
		if (ConfMan.hasKey("dreamfactory_boot_gui_keys")) { Common::StringTokenizer keys(ConfMan.get("dreamfactory_boot_gui_keys"), ","); while (!keys.empty()) input.guiKeys.push_back(keys.nextToken()); }
		if (ConfMan.hasKey("dreamfactory_boot_movie_clicks")) { Common::StringTokenizer clicks(ConfMan.get("dreamfactory_boot_movie_clicks"), ";"); while (!clicks.empty()) input.movieClicks.push_back(clicks.nextToken()); }
		if (ConfMan.hasKey("dreamfactory_boot_modal_clicks")) { Common::StringTokenizer clicks(ConfMan.get("dreamfactory_boot_modal_clicks"), ";"); while (!clicks.empty()) input.modalClicks.push_back(clicks.nextToken()); }
		input.skipSpeech = ConfMan.hasKey("dreamfactory_boot_skip_speech") && ConfMan.getBool("dreamfactory_boot_skip_speech");
		bool booted = session.init(root);
		if (booted && ConfMan.hasKey("dreamfactory_boot_restore") && !ConfMan.get("dreamfactory_boot_restore").empty()) {
			Common::File saved; booted = saved.open(Common::FSNode(ConfMan.getPath("dreamfactory_boot_restore"))) && loadGameStream(&saved).getCode() == Common::kNoError;
			if (!booted) session.runtime.fail("Could not restore diagnostic checkpoint");
		} else if (booted) booted = session.boot();
		if (booted && ConfMan.hasKey("dreamfactory_boot_keys")) {
			Common::StringTokenizer keys(ConfMan.get("dreamfactory_boot_keys"), ",");
			while (!keys.empty() && session.runtime.error.empty()) {
				const Common::String key = keys.nextToken(); Common::Event event; event.type = Common::EVENT_KEYDOWN;
				if (key == "leftarrow") event.kbd.keycode = Common::KEYCODE_LEFT;
				else if (key == "rightarrow") event.kbd.keycode = Common::KEYCODE_RIGHT;
				else if (key == "uparrow") event.kbd.keycode = Common::KEYCODE_UP;
				else if (key == "downarrow") event.kbd.keycode = Common::KEYCODE_DOWN;
				else if (key == ".") event.kbd.keycode = Common::KEYCODE_ESCAPE;
				else if (key.size() == 1) event.kbd.ascii = byte(key[0]);
				else { session.runtime.fail("Invalid diagnostic key: " + key); break; }
				session.postEvent(event); const bool ok = session.step();
				warning("BOOT KEY %s key=%s room=%s scene=%s view=%s", ok ? "PASS" : "FAIL", key.c_str(), session.roomSystem.name().c_str(), session.roomSystem.sceneName().c_str(), session.roomSystem.viewName().c_str());
			}
		}
		if (booted && ConfMan.hasKey("dreamfactory_boot_click_x") && ConfMan.getInt("dreamfactory_boot_click_x") >= 0) {
			Common::Event event; event.type = Common::EVENT_LBUTTONDOWN;
			event.mouse = Common::Point(int16((ConfMan.getInt("dreamfactory_boot_click_x") * 640 + 511) / 512), int16((ConfMan.getInt("dreamfactory_boot_click_y") * 480 + 383) / 384));
			session.postEvent(event); event.type = Common::EVENT_LBUTTONUP; session.postEvent(event);
			const bool ok = session.step();
			warning("BOOT CLICK %s room=%s scene=%s view=%s flat=%s error=%s", ok ? "PASS" : "FAIL", session.roomSystem.name().c_str(), session.roomSystem.sceneName().c_str(), session.roomSystem.viewName().c_str(), session.stageSystem.currentFlat().c_str(), session.runtime.error.c_str());
		}
		if (booted && ConfMan.hasKey("dreamfactory_boot_steps")) {
			const uint32 count = CLIP<int32>(ConfMan.getInt("dreamfactory_boot_steps"), 0, 10000);
			uint32 done = 0; while (done < count && session.step()) ++done;
			warning("BOOT STEPS %s done=%u requested=%u error=%s", done == count ? "PASS" : "FAIL", done, count, session.runtime.error.c_str());
		}
		if (booted && session.runtime.error.empty() && ConfMan.hasKey("dreamfactory_boot_actions")) {
			Variables rememberedGlobals;
			Common::StringTokenizer actions(ConfMan.get("dreamfactory_boot_actions"), ";"); uint32 index = 0;
			while (!actions.empty() && session.runtime.error.empty()) {
				Common::String action = actions.nextToken(); action.trim(); Common::StringTokenizer words(action); const Common::String command = words.nextToken(); bool ok = true;
				if (command == "rememberglobal" || command == "expectremembered") {
					const Common::String name = words.nextToken();
					if (command == "rememberglobal") { ok = session.runtime.globals.contains(name); if (ok) rememberedGlobals[name] = session.runtime.globals[name]; }
					else ok = rememberedGlobals.contains(name) && session.runtime.globals.contains(name) && rememberedGlobals[name].equals(session.runtime.globals[name]);
				} else if (command == "expectsetting") {
					const Common::String setting = words.nextToken(); const int32 expected = atoi(words.nextToken().c_str()); int32 actual = 0;
					if (setting == "wavevolume") actual = session.sound.waveVolume();
					else if (setting == "subtitles") actual = session.puppetParams.contains(7) ? session.puppetParams[7] : 1;
					else if (setting == "shared_subtitles") actual = session.subtitlesEnabled() ? 1 : 0;
					else if (setting == "framerate") actual = session.frameRate;
					else if (setting == "roomvisible") actual = session.roomSystem.visible ? 1 : 0;
					else ok = false;
					ok = ok && actual == expected;
					warning("BOOT SETTING %s name=%s actual=%d expected=%d", ok ? "PASS" : "FAIL", setting.c_str(), actual, expected);
				} else if (command == "expectsave") {
					const Common::String number = words.nextToken(); const int32 slot = atoi(number.c_str());
					Common::String expected = action.substr(command.size() + number.size() + 2); expected.trim();
					const SaveStateDescriptor saved = getMetaEngine()->querySaveMetaInfos(_targetName.c_str(), slot);
					ok = saved.getSaveSlot() == slot && saved.getDescription() == expected;
					bool listed = false; const SaveStateList saves = getMetaEngine()->listSaves(_targetName.c_str());
					for (auto it = saves.begin(); it != saves.end(); ++it) if (it->getSaveSlot() == slot && it->getDescription() == expected) listed = true;
					ok = ok && listed;
					warning("BOOT SAVE META %s slot=%d description=%s listed=%d", ok ? "PASS" : "FAIL", slot, saved.getDescription().c_str(), listed);
				} else if (command == "saveslot" || command == "loadslot") {
					const int32 slot = atoi(words.nextToken().c_str()); ok = slot >= 0 && slot <= 99;
					if (ok) ok = (command == "saveslot" ? saveGameState(slot, "DreamFactory checkpoint test") : loadGameState(slot)).getCode() == Common::kNoError;
				} else if (command == "dumproutes") {
					const auto tour = session.runtime.globals.find("tour");
					RouteMap flow; flow.build(session.routes.graph, session.version, session.sound.isTitanicRetail(), tour != session.runtime.globals.end() && tour->_value.truth());
					uint32 reached = 0, total = 0;
					for (const auto &item : flow.items()) if (item.ending) { ++total; if (!item.locked) ++reached; }
					warning("BOOT FLOW visible=%u reached=%u total=%u", uint32(flow.items().size()), reached, total);
					for (const auto &item : flow.items()) warning("BOOT FLOW node=%u parent=%d chapter=%u ending=%u locked=%u", item.node, item.parent, item.node ? session.routes.graph.node(item.node)->chapter : 0, item.ending, item.locked);
					for (uint32 n = 0; n < session.routes.graph.nodes().size(); ++n) {
						const RouteNode &node = session.routes.graph.nodes()[n];
						warning("BOOT ROUTE node=%u kind=%u parent=%u choice=%d seen=%u checkpoint=%u file=%s speaker=%s", n + 1, uint32(node.kind), node.parent, node.parentChoice, node.seenChoices, node.checkpoint, node.entry.file.c_str(), node.entry.speaker.c_str());
					}
				} else if (command == "freshcontinue") {
					ok = session.init(root) && !session.stageSystem.current && session.continueRecovery();
				} else if (command == "recoverysave") {
					ok = session.canAutosave() && saveGameState(getAutosaveSlot(), "Autosave", true).getCode() == Common::kNoError;
				} else if (command == "auditdialogueoptions") {
					const Common::FSNode output(ConfMan.getPath("dreamfactory_boot_checkpoint").getParent());
					ok = auditDialogueOptions(_system, output, false) && auditDialogueOptions(_system, output, true);
				} else if (command == "dumpleads" || command == "expectlead" || command == "expectnolead") {
					const auto leads = journalLeads(session.journal, journalLeadState(session));
					const Common::String leadID = words.empty() ? Common::String() : words.nextToken(); bool foundLead = false;
					for (const auto &lead : leads) {
						warning("BOOT LEAD id=%s evidence=%u hints=%u status=%s", lead.id.c_str(), uint32(lead.evidence.size()), uint32(lead.hints.size()), lead.status.encode(Common::kUtf8).c_str());
						if (lead.id == leadID) foundLead = true;
					}
					if (command != "dumpleads") ok = command == "expectlead" ? foundLead : !foundLead;
				} else if (command == "dumpendinghints") {
					const auto hints = journalEndingHints(session.journal, journalLeadState(session), session.routes.graph, session.sound.isTitanicRetail(), session.runtime.globals.contains("tour") && session.runtime.globals["tour"].truth());
					warning("BOOT ENDING HINTS evidence=%u hints=%u status=%s", uint32(hints.evidence.size()), uint32(hints.hints.size()), hints.status.encode(Common::kUtf8).c_str());
					for (uint32 i = 0; i < hints.hints.size(); ++i) warning("BOOT ENDING HINT %u %s", i + 1, hints.hints[i].encode(Common::kUtf8).c_str());
					for (uint32 evidenceIndex : hints.evidence) warning("BOOT ENDING EVIDENCE file=%s ident=%s", session.journal.entries[evidenceIndex].file.c_str(), session.journal.entries[evidenceIndex].ident.c_str());
				} else if (command == "dumpnavigation" || command == "expectnavigation") {
					MapPosition point; ok = session.atlas.load(session) && session.atlas.project(session, point);
					warning("BOOT NAVIGATION room=%s scene=%s view=%s page=%d x=%d y=%d visits=%u passages=%u valid=%u", point.room.c_str(), point.scene.c_str(), point.view.c_str(), point.page, point.x, point.y, uint32(session.travel.visits.size()), uint32(session.travel.passages.size()), ok);
					if (command == "expectnavigation") ok = ok && point.page == atoi(words.nextToken().c_str()) && point.x == atoi(words.nextToken().c_str()) && point.y == atoi(words.nextToken().c_str());
				} else if (command == "openmap" || command == "openmapcompact") {
					ConfMan.setBool("dreamfactory_navigation_test_compact", command == "openmapcompact", Common::ConfigManager::kTransientDomain);
					Common::Event raw; raw.type = Common::EVENT_KEYDOWN; raw.kbd = Common::KeyState(Common::KEYCODE_F11); Common::List<Common::Event> mapped;
					_system->getEventManager()->getKeymapper()->mapEvent(raw, mapped);
					ok = mapped.size() == 1 && mapped.front().type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && mapped.front().customType == kOpenNavigation;
					if (ok) { session.postEvent(mapped.front()); warning("NAVIGATION KEYMAP PASS physical F11 opens annotated map"); }
					const uint32 before = input.guiKeyIndex; ok = ok && session.step() && !g_gui.isActive() && input.guiKeyIndex > before;
					ConfMan.removeKey("dreamfactory_navigation_test_compact", Common::ConfigManager::kTransientDomain);
				} else if (command == "auditmapinput") {
					ok = auditNavigationInput(session);
				} else if (command == "dumpheard") {
					warning("BOOT HEARD lines=%u active=%u dirty=%u", session.heard.index().size(), session.heard.active(), session.heard.dirty());
				} else if (command == "expectrecoveries") {
					const auto entries = session.recovery.list(); ok = int32(entries.size()) == atoi(words.nextToken().c_str());
					for (const auto &entry : entries) warning("BOOT RECOVERY sequence=%u location=%s clock=%s", entry.sequence, entry.preview.location.c_str(), entry.preview.clock.c_str());
				} else if (command == "waitrecoveries") {
					const uint32 count = CLIP<int32>(atoi(words.nextToken().c_str()), 1, 3), started = _system->getMillis();
					while (session.recovery.list().size() < count && ok && _system->getMillis() - started < 10000) { ok = session.step(); if (ok) serviceAutosave(); }
					ok = ok && session.recovery.list().size() >= count;
				} else if (command == "openrecovery") {
					Common::Event event; event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kOpenRecovery; session.postEvent(event);
					for (uint32 tries = 0; tries < 120; ++tries) { if (!session.step()) { ok = false; break; } if (!session.recoveryPending()) break; }
					ok = ok && !session.recoveryPending();
				} else if (command == "openinventory") {
					Common::Event event; event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kOpenInventory; session.postEvent(event);
					for (uint32 tries = 0; tries < 120; ++tries) { if (!session.step()) { ok = false; break; } if (!session.inventoryPending()) break; }
					ok = ok && !session.inventoryPending();
				} else if (command == "inventoryaction") {
					InventoryResult result; result.item = words.nextToken(); const Common::String inventoryCommand = words.nextToken(); result.action = inventoryCommand == "examine" ? kInventoryExamine : inventoryCommand == "use" ? kInventoryUse : kInventoryBack;
					ok = inventoryAction(session, result);
				} else if (command == "dumpinventory") {
					Common::Array<InventoryItem> items; const Variables globals = session.runtime.globals;
					ok = inventoryItems(session, items);
					warning("BOOT INVENTORY items=%u", uint32(items.size()));
					for (const auto &item : items) {
						const auto prop = session.propSystem.get(item.name); const Prop before = *prop;
						Bytes picture; uint32 width = 0, height = 0; inventoryPicture(session, item.name, picture, width, height);
						ok = ok && prop->view == before.view && prop->deg == before.deg && prop->step == before.step && prop->owner.equals(before.owner) && prop->visible == before.visible;
						warning("BOOT INVENTORY ITEM name=%s title=%s examine=%u picture=%ux%u", item.name.c_str(), item.title.c_str(), item.examinable, width, height);
					}
					ok = ok && globals.size() == session.runtime.globals.size();
					for (const auto &variable : globals) ok = ok && session.runtime.globals.contains(variable._key) && variable._value.equals(session.runtime.globals[variable._key]);
				} else if (command == "dumppreview") {
					RoutePreview preview; ok = session.routes.readPreview(session.routes.graph.cursor(), preview);
					if (ok) {
						warning("BOOT PREVIEW location=%s clock=%s picture=%u items=%u", preview.location.c_str(), preview.clock.c_str(), uint32(preview.picture.size()), uint32(preview.inventory.size()));
						for (const auto &item : preview.inventory) warning("BOOT PREVIEW ITEM name=%s owner=%s", item.name.c_str(), item.owner.c_str());
					}
				} else if (command == "revisitscene") {
					const Common::String target = words.nextToken();
					const uint32 id = target == "latest" ? session.routes.graph.cursor() : atoi(target.c_str()); Common::String error;
					ok = session.restoreRouteScene(id, error);
					warning("BOOT ROUTE RESTORE %s node=%u cursor=%u error=%s", ok ? "PASS" : "FAIL", id, session.routes.graph.cursor(), error.c_str());
				} else if (command == "savecheckpoint" || command == "loadcheckpoint" || command == "rejectcheckpoint") {
					ok = ConfMan.hasKey("dreamfactory_boot_checkpoint");
					if (ok && command == "savecheckpoint") {
						Common::DumpFile saved;
						ok = saved.open(Common::FSNode(ConfMan.getPath("dreamfactory_boot_checkpoint"))) && saveGameStream(&saved).getCode() == Common::kNoError;
					} else if (ok) {
						Common::File saved;
						const auto room = session.roomSystem.current; const auto stage = session.stageSystem.current;
						const Variables globals = session.runtime.globals; const uint64 time = session.elapsedMillis();
						const bool opened = saved.open(Common::FSNode(ConfMan.getPath("dreamfactory_boot_checkpoint")));
						const bool loaded = opened && loadGameStream(&saved).getCode() == Common::kNoError;
						ok = command == "loadcheckpoint" ? loaded : opened && !loaded && room == session.roomSystem.current && stage == session.stageSystem.current && time == session.elapsedMillis() && globals.size() == session.runtime.globals.size();
						if (ok && command == "rejectcheckpoint") for (auto it = globals.begin(); it != globals.end(); ++it) if (!session.runtime.globals.contains(it->_key) || !session.runtime.globals[it->_key].equals(it->_value)) ok = false;
					}
				} else if (command == "menugui") {
					const uint32 before = input.guiKeyIndex;
					input.menuPending = true; input.menuEscapeAt = 0;
					ok = session.step() && !g_gui.isActive() && input.guiKeyIndex > before;
				} else if (command == "menukey") {
					const uint32 started = _system->getMillis(); const uint64 clock = session.elapsedMillis();
					input.menuPending = true; input.menuSeen = input.menuEscapeSent = false; input.menuEscapeAt = started + 300;
					ok = session.step();
					const uint32 wall = _system->getMillis() - started; const uint64 elapsed = session.elapsedMillis() - clock;
					ok = ok && input.menuSeen && input.menuEscapeSent && !g_gui.isActive() && wall >= 300 && elapsed < 200;
					warning("BOOT MENU %s opened=%d cancelled=%d wall=%u clock=%llu", ok ? "PASS" : "FAIL", input.menuSeen, input.menuEscapeSent, wall, (unsigned long long)elapsed);
					input.menuEscapeAt = 0;
				} else if (command == "idle") {
					const int32 count = CLIP<int32>(atoi(words.nextToken().c_str()), 0, 10000);
					for (int32 n = 0; n < count && ok; ++n) ok = session.step();
				} else if (command == "waitpropview" || command == "waitpropviewany" || command == "waitactorstar" || command == "waitactorwalk" || command == "waitactorpose" || command == "waitset" || command == "waitglobal") {
					const Common::String name = command == "waitset" ? Common::String() : words.nextToken(), expected = words.nextToken();
					const uint32 timeout = words.empty() ? 30000 : CLIP<int32>(atoi(words.nextToken().c_str()), 1, 60000);
					const uint32 started = _system->getMillis();
					while (ok) {
						Common::String actual;
						if (command == "waitset") actual = session.roomSystem.name();
						else if (command == "waitglobal") actual = session.runtime.globals.contains(name) ? session.runtime.globals[name].text() : Common::String("0");
						else if (command == "waitpropview" || command == "waitpropviewany") {
							const auto prop = session.propSystem.get(name);
							if (!prop) { ok = false; break; }
							actual = prop->view;
						} else {
							const auto actor = session.castSystem.get(name);
							if (!actor) { ok = false; break; }
							actual = command == "waitactorwalk" ? Common::String(session.motion.get(name) ? "1" : "0") : command == "waitactorpose" ? actor->pose : actor->star;
						}
						if (actual.equalsIgnoreCase(expected)) break;
						if (command == "waitpropviewany") {
							Common::StringTokenizer alternatives(expected, "|"); bool matched = false;
							while (!alternatives.empty()) if (actual.equalsIgnoreCase(alternatives.nextToken())) matched = true;
							if (matched) break;
						}
						if (_system->getMillis() - started >= timeout) {
							warning("BOOT WAIT name=%s actual=%s expected=%s", name.c_str(), actual.c_str(), expected.c_str());
							ok = false; break;
						}
						ok = session.step();
					}
				} else if (command == "autosave") {
					ok = session.canAutosave();
					if (ok) serviceAutosave();
                } else if (command == "pad" || command == "padup") {
                    const Common::String name = words.nextToken();
                    const char *names[] = {"up", "down", "left", "right", "previous", "next", "interact", "repeat"};
                    ok = false;
                    for (uint32 i=0;i<ARRAYSIZE(names);++i) if (name == names[i]) {
                        Common::Event event; event.type = command == "padup" ? Common::EVENT_CUSTOM_ENGINE_ACTION_END : Common::EVENT_CUSTOM_ENGINE_ACTION_START;
                        event.customType=kPadUp+i; session.postEvent(event); ok=session.step(); break;
                    }
				} else if (command == "key" || command == "keyquit" || command == "mappedkey") {
					const Common::String key = words.nextToken(); Common::Event event; event.type = Common::EVENT_KEYDOWN;
					if (key == "leftarrow") event.kbd.keycode = Common::KEYCODE_LEFT;
					else if (key == "rightarrow") event.kbd.keycode = Common::KEYCODE_RIGHT;
					else if (key == "uparrow") event.kbd.keycode = Common::KEYCODE_UP;
					else if (key == "downarrow") event.kbd.keycode = Common::KEYCODE_DOWN;
					else if (key == "keypad8") event.kbd.keycode = Common::KEYCODE_KP8;
					else if (key == "keypad2") event.kbd.keycode = Common::KEYCODE_KP2;
					else if (key == "keypad4") event.kbd.keycode = Common::KEYCODE_KP4;
					else if (key == "keypad6") event.kbd.keycode = Common::KEYCODE_KP6;
					else if (key == ".") event.kbd.keycode = Common::KEYCODE_ESCAPE;
					else if (key == "space") { event.kbd.keycode = Common::KEYCODE_SPACE; event.kbd.ascii = ' '; }
					else if (key.size() == 1) event.kbd.ascii = byte(key[0]);
					else ok = false;
					if (ok) {
						if (command == "mappedkey") {
							Common::List<Common::Event> mapped;
							ok = _eventMan->getKeymapper()->mapEvent(event, mapped) && !mapped.empty();
							for (auto it = mapped.begin(); it != mapped.end(); ++it) {
								warning("BOOT MAPPED KEY raw=%d mapped=%d", event.kbd.keycode, it->kbd.keycode);
								session.postEvent(*it);
							}
							// Release the physical binding so this probe cannot leave
							// an action held in ScummVM's mapper between assertions.
							event.type = Common::EVENT_KEYUP; mapped.clear();
							_eventMan->getKeymapper()->mapEvent(event, mapped);
							for (auto it = mapped.begin(); it != mapped.end(); ++it) session.postEvent(*it);
						} else session.postEvent(event);
						if (ok) ok = session.step();
						if (command == "keyquit") {
							ok = !ok && session.scriptQuitRequested() && session.runtime.error.empty();
							if (ok) warning("BOOT SCRIPT QUIT PASS");
						}
					}
				} else if (command == "move") {
					const int32 x = atoi(words.nextToken().c_str()), y = atoi(words.nextToken().c_str());
					ok = x >= 0 && x < 512 && y >= 0 && y < 384;
					if (ok) {
						Common::Event event; event.type = Common::EVENT_MOUSEMOVE;
						event.mouse = Common::Point(int16((x * 640 + 511) / 512), int16((y * 480 + 383) / 384));
						session.postEvent(event); ok = session.step();
					}
				} else if (command == "click" || command == "clickquit") {
					const int32 x = atoi(words.nextToken().c_str()), y = atoi(words.nextToken().c_str()); input.clickAt(x, y); ok = session.step();
					if (command == "clickquit") {
						ok = !ok && session.scriptQuitRequested() && session.runtime.error.empty();
						if (ok) { input.releaseAt = 0; warning("BOOT SCRIPT QUIT PASS"); }
					}
				} else if (command == "clickon" || command == "padon" || command == "dragto" || command == "dragarc" || command == "dragarcuntil" || command == "dragpoint" || command == "holduntil" || command == "clickthen" || command == "clickifempty") {
					const bool gesture = command == "dragto" || command == "dragarc" || command == "dragarcuntil" || command == "dragpoint" || command == "holduntil" || command == "clickthen";
					const Common::String guard = command == "clickifempty" ? words.nextToken() : Common::String();
					const Common::String type = words.nextToken(), name = gesture ? words.nextToken() : action.substr(command.size() + type.size() + 2 + (command == "clickifempty" ? guard.size() + 1 : 0));
					const bool skip = command == "clickifempty" && (!session.runtime.globals.contains(guard) || !session.runtime.globals[guard].text().empty());
					const Common::String targetType = command == "dragto" ? words.nextToken() : Common::String(), targetName = command == "dragto" ? words.nextToken() : Common::String();
					Common::Point start, end; bool found = false, targetFound = command != "dragto";
					for (int32 y = 0; !skip && y < 384 && (!found || !targetFound); y += 4) for (int32 x = 0; x < 512; x += 4) {
						const auto hit = session.hitTest(x, y);
						if (!found && hit.type == type && hit.name.equalsIgnoreCase(name)) { start = Common::Point(x, y); found = true; }
						if (!targetFound && hit.type == targetType && hit.name.equalsIgnoreCase(targetName)) { end = Common::Point(x, y); targetFound = true; }
					}
					// Small world props can occupy a single opaque pixel between
					// the coarse sample points. Refine around their projection;
					// all clicks still pass through the ordinary hit tester/input.
					if (!skip && !found && type == "prop") {
						const auto prop = session.propSystem.get(name); WorldCamera camera; ProjectedPoint point;
						if (prop && prop->worldSpace && session.roomSystem.worldCamera(camera, session.cameraHi) && projectPoint(camera, prop->x, prop->y, prop->z, point)) {
							const int32 cx = int32(point.x), cy = int32(point.y);
							for (int32 y = MAX<int32>(0, cy - 64); !found && y < MIN<int32>(384, cy + 64); ++y)
								for (int32 x = MAX<int32>(0, cx - 64); x < MIN<int32>(512, cx + 64); ++x) {
									const auto hit = session.hitTest(x, y);
									if (hit.type == type && hit.name.equalsIgnoreCase(name)) { start = Common::Point(x, y); found = true; break; }
								}
						}
					}
					ok = skip || (found && targetFound);
					if (!skip && !found && type == "actor") {
						const auto actor = session.castSystem.get(name);
						if (actor) warning("BOOT ACTOR MISS name=%s set=%s star=%s visible=%u world=%u xyz=%d,%d,%d pose=%s scale=%d deg=%d zclip=%d", name.c_str(), actor->set.c_str(), actor->star.c_str(), actor->visible, actor->worldSpace, actor->x, actor->y, actor->z, actor->pose.c_str(), actor->scale, actor->deg, actor->zclip);
						else warning("BOOT ACTOR MISS name=%s absent", name.c_str());
						WorldCamera camera; ProjectedPoint point;
						if (actor && session.roomSystem.worldCamera(camera, session.cameraHi)) {
							const bool projected = projectPoint(camera, actor->x, actor->y, actor->z, point);
							warning("BOOT ACTOR CAMERA xyz=%d,%d,%d deg=%d projected=%u xy=%.1f,%.1f depth=%d onscreen=%u", camera.x, camera.y, camera.z, camera.degree, projected, point.x, point.y, point.depth, session.castSystem.onScreen(*actor, camera, session.roomSystem.name()));
						}
					}
					if (skip) warning("BOOT CLICK SKIP global=%s", guard.c_str());
					else if (ok) {
						if (command == "padon") {
							Common::Event motion; motion.type = Common::EVENT_MOUSEMOVE; motion.mouse = Common::Point((start.x * 640 + 511) / 512, (start.y * 480 + 383) / 384); session.postEvent(motion);
							Common::Event physical; physical.type = Common::EVENT_JOYBUTTON_DOWN; physical.joystick.button = Common::JOYSTICK_BUTTON_A;
							Common::List<Common::Event> mapped; _eventMan->getKeymapper()->mapEvent(physical, mapped); ok = mapped.size() == 1 && mapped.front().type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && mapped.front().customType == kPadInteract;
							for (const auto &event : mapped) session.postEvent(event);
							physical.type = Common::EVENT_JOYBUTTON_UP; mapped.clear(); _eventMan->getKeymapper()->mapEvent(physical, mapped); for (const auto &event : mapped) session.postEvent(event);
							warning("BOOT GAMEPAD TARGET mapped=%u type=%s name=%s", ok, type.c_str(), name.c_str());
						} else input.clickAt(start.x, start.y);
						if (command == "dragto") input.dragTo(end.x, end.y);
						else if (command == "dragpoint" || command == "dragarc" || command == "dragarcuntil" || command == "clickthen") {
							const int32 x = atoi(words.nextToken().c_str()), y = atoi(words.nextToken().c_str());
							ok = x >= 0 && x < 512 && y >= 0 && y < 384;
							if (command == "dragpoint" && ok) { input.dragTo(x, y); input.releaseAt = _system->getMillis() + 300; }
							else if (command == "dragarc" || command == "dragarcuntil") {
								const int32 delta = atoi(words.nextToken().c_str()), steps = atoi(words.nextToken().c_str());
								ok = ok && delta != 0 && delta >= -32 && delta <= 32 && steps >= 1 && steps <= 64;
								if (ok) input.dragArc(start, x, y, delta, steps);
								if (ok && command == "dragarcuntil") {
									input.holdProp = name; input.holdOwner = words.nextToken();
									ok = type == "prop" && !input.holdOwner.empty();
								}
							} else if (command == "clickthen") {
								const int32 delay = atoi(words.nextToken().c_str()); ok = ok && delay >= 200 && delay <= 10000;
								if (ok) input.thenClick(x, y, uint32(delay));
							}
						} else if (command == "holduntil") {
							input.holdGlobal = words.nextToken(); input.holdMinimum = atoi(words.nextToken().c_str());
							ok = !input.holdGlobal.empty(); input.releaseAt = _system->getMillis() + 60000;
						}
						if (ok) ok = session.step();
					}
				} else if (command == "expectballs") {
					const int32 expected = atoi(words.nextToken().c_str());
					ok = session.balls.count() == expected;
					if (!ok) warning("BOOT EXPECT balls=%d expected=%d", session.balls.count(), expected);
				} else if (command == "dumpcrickets" || command == "expectambient" || command == "expectcricket") {
					const Common::String name = words.nextToken();
					const int32 expectedActive = atoi(words.nextToken().c_str());
					const int32 expectedVolume = atoi(words.nextToken().c_str());
					const int32 expectedPan = atoi(words.nextToken().c_str());
					bool found = false;
					for (const auto &cricket : session.crickets.crickets) {
						if (cricket->name.empty()) continue;
						if (command != "dumpcrickets" && !cricket->name.equalsIgnoreCase(name)) continue;
						found = true;
						const bool active = session.sound.handleActive(cricket->handle);
						const int32 volume = active ? _system->getMixer()->getChannelVolume(cricket->handle) : 0;
						const int32 pan = active ? _system->getMixer()->getChannelBalance(cricket->handle) : 0;
						warning("BOOT AMBIENT name=%s xyz=%d,%d radius=%d active=%u volume=%d pan=%d count=%lld armed=%u", cricket->name.c_str(), cricket->x, cricket->y, cricket->radius, active, volume, pan, (long long)cricket->count, cricket->armed);
						if (command == "expectambient") ok = active == bool(expectedActive) && volume == expectedVolume && pan == expectedPan;
						if (command == "expectcricket") ok = cricket->armed == bool(expectedActive) && cricket->count == expectedVolume;
					}
					if (command != "dumpcrickets" && !found) ok = false;
				} else if (command == "dumpballs") {
					for (const auto &ball : session.balls.balls) if (ball) warning("BOOT BALL name=%s xyz=%d,%d,%d velocity=%d,%d,%d frames=%d paused=%d", ball->name.c_str(), ball->x, ball->y, ball->z, ball->vx, ball->vy, ball->vz, ball->frames, ball->paused);
				} else if (command == "dumpactor") {
					const Common::String name = words.nextToken(); const auto actor = session.castSystem.get(name);
					ok = bool(actor);
					if (actor) warning("BOOT ACTOR name=%s set=%s star=%s visible=%u xyz=%d,%d,%d speed=%d deg=%d pose=%s value=%s", name.c_str(), actor->set.c_str(), actor->star.c_str(), actor->visible, actor->x, actor->y, actor->z, actor->speed, actor->deg, actor->pose.c_str(), actor->value.text().c_str());
					const auto walk = session.motion.get(name);
					if (walk) warning("BOOT WALK name=%s destination=%s progress=%.1f distance=%.1f paused=%u turning=%u", name.c_str(), walk->destination.c_str(), walk->progress, walk->distance, walk->paused, walk->turning);
				} else if (command == "dumpprop") {
					const Common::String name = words.nextToken(); const auto prop = session.propSystem.get(name);
					ok = bool(prop);
					if (prop) warning("BOOT PROP name=%s deg=%d view=%s owner=%s set=%s visible=%u world=%u xyz=%d,%d,%d scale=%d", name.c_str(), prop->deg, prop->view.c_str(), prop->owner.text().c_str(), prop->set.c_str(), prop->visible, prop->worldSpace, prop->x, prop->y, prop->z, prop->scale);
				} else if (command == "expectflat" || command == "expectset" || command == "expectscene" || command == "expectview") {
					Common::String expected = action.substr(command.size() + 1); expected.trim();
					const Common::String actual = command == "expectflat" ? session.stageSystem.currentFlat() : command == "expectset" ? session.roomSystem.name() : command == "expectscene" ? session.roomSystem.sceneName() : session.roomSystem.viewName();
					ok = actual.equalsIgnoreCase(expected);
				} else if (command == "expectowner" || command == "expectactorowner" || command == "expectactorpose" || command == "expectactorvalue" || command == "expectpropview" || command == "expectpropvalue" || command == "expectpropdeg" || command == "expectglobal" || command == "expectglobalge") {
					Common::String name = words.nextToken(); name.toLowercase(); Common::String expected = action.substr(command.size() + name.size() + 2); expected.trim();
					const auto prop = session.propSystem.get(name);
					const auto actor = session.castSystem.get(name);
					const Common::String actual = command == "expectglobal" || command == "expectglobalge" ? (session.runtime.globals.contains(name) ? session.runtime.globals[name].text() : Common::String("0")) : command == "expectactorowner" ? (actor ? actor->owner.text() : Common::String()) : command == "expectactorpose" ? (actor ? actor->pose : Common::String()) : command == "expectactorvalue" ? (actor ? actor->value.text() : Common::String()) : !prop ? Common::String() : command == "expectowner" ? prop->owner.text() : command == "expectpropvalue" ? prop->value.text() : command == "expectpropdeg" ? Common::String::format("%d", prop->deg) : prop->view;
					ok = command == "expectglobalge" ? atoi(actual.c_str()) >= atoi(expected.c_str()) : actual.equalsIgnoreCase(expected);
					if (!ok) warning("BOOT EXPECT name=%s actual=%s expected=%s", name.c_str(), actual.c_str(), expected.c_str());
				} else if (command == "targets") {
					Common::Array<Common::String> seen;
					for (int32 y = 0; y < 384; y += 8) for (int32 x = 0; x < 512; x += 8) {
						const auto hit = session.hitTest(x, y); if (hit.type == "none") continue; const Common::String token = hit.type + ":" + hit.name;
						bool known = false; for (uint32 i = 0; i < seen.size(); ++i) if (seen[i] == token) known = true;
						if (!known) { seen.push_back(token); warning("BOOT TARGET %s x=%d y=%d", token.c_str(), x, y); }
					}
				} else ok = false;
				// Finish each physical press before the next diagnostic gesture.
				while (ok && (input.releaseAt || input.nextClickAt)) ok = session.step();
				warning("BOOT ACTION %s index=%u action=%s room=%s scene=%s view=%s flat=%s", ok ? "PASS" : "FAIL", ++index, action.c_str(), session.roomSystem.name().c_str(), session.roomSystem.sceneName().c_str(), session.roomSystem.viewName().c_str(), session.stageSystem.currentFlat().c_str());
				if (!ok) { if (session.runtime.error.empty()) session.runtime.fail("Diagnostic action failed: " + action); break; }
			}
		}
		if (booted && session.runtime.error.empty() && input.choiceIndex != input.choices.size()) session.runtime.fail("Diagnostic puppet choices were not all consumed");
		if (booted && session.runtime.error.empty() && input.dialogIndex != input.dialogAnswers.size()) session.runtime.fail("Diagnostic dialog answers were not all consumed");
		if (booted && session.runtime.error.empty() && input.guiKeyIndex != input.guiKeys.size()) session.runtime.fail("Diagnostic GUI keys were not all consumed");
		if (booted && session.runtime.error.empty() && input.movieClickIndex != input.movieClicks.size()) session.runtime.fail("Diagnostic movie clicks were not all consumed");
		if (booted && session.runtime.error.empty() && input.modalClickIndex != input.modalClicks.size()) session.runtime.fail("Diagnostic modal clicks were not all consumed");
		const bool ready = booted && session.runtime.error.empty();
		if (ready && ConfMan.hasKey("dreamfactory_boot_capture")) {
			_system->saveScreenshot();
			Bytes rgba; rgba.resize(640 * 480 * 4); byte palette[768];
			if (_system->getScreenFormat().bytesPerPixel == 1) _system->getPaletteManager()->grabPalette(palette, 0, 256);
			Graphics::Surface *screen = _system->lockScreen();
			if (screen) {
				for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
					byte *p = rgba.data() + (y * 640 + x) * 4; const uint32 color = screen->getPixel(x, y);
					if (screen->format.bytesPerPixel == 1) memcpy(p, palette + color * 3, 3);
					else screen->format.colorToRGB(color, p[0], p[1], p[2]);
					p[3] = 255;
				}
				_system->unlockScreen();
				Common::DumpFile capture;
				if (!capture.open(Common::FSNode(ConfMan.getPath("dreamfactory_boot_capture"))) || capture.write(rgba.data(), rgba.size()) != rgba.size()) warning("BOOT CAPTURE FAIL write");
				else warning("BOOT CAPTURE PASS native 640x480 RGBA");
			} else warning("BOOT CAPTURE FAIL screen unavailable");
		}
		warning("BOOT %s version=%u globals=%u error=%s", ready ? "READY" : "STOP", session.version, session.runtime.globals.size(), session.runtime.error.c_str());
		uint32 copies = 0; for (uint32 a = 0; a < session.castSystem.actors.size(); ++a) if (session.castSystem.actors[a]->copy) ++copies;
		warning("BOOT CASTS files=%u actors=%u copies=%u", session.castSystem.casts.size(), session.castSystem.actors.size(), copies);
		copies = 0; for (uint32 p = 0; p < session.propSystem.props.size(); ++p) if (session.propSystem.props[p]->copy) ++copies;
		warning("BOOT SHOPS files=%u props=%u copies=%u", session.propSystem.shops.size(), session.propSystem.props.size(), copies);
		warning("BOOT LOCATION room=%s scene=%s view=%s stage=%s flat=%s", session.roomSystem.name().c_str(), session.roomSystem.sceneName().c_str(), session.roomSystem.viewName().c_str(), session.stageSystem.current ? session.stageSystem.current->name.c_str() : "none", session.stageSystem.currentFlat().c_str());
		for (uint32 p = 0; p <= 8; ++p) warning("BOOT PATH %u %s", p, session.resources.path(p).c_str());
		_session = nullptr;
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_resource_audit") && ConfMan.getBool("dreamfactory_resource_audit")) {
		auditResources(root, Common::FSNode(ConfMan.getPath("dreamfactory_resource_fixtures")));
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_star_audit") && ConfMan.getBool("dreamfactory_star_audit")) {
		auditStars(root); quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_stage_audit") && ConfMan.getBool("dreamfactory_stage_audit")) {
		auditStages(root);
		if (ConfMan.hasKey("dreamfactory_stage_fixtures")) auditStageSession(Common::FSNode(ConfMan.getPath("dreamfactory_stage_fixtures")), _system);
		quitGame(); return Common::kNoError;
	}
	if (ConfMan.hasKey("dreamfactory_script_audit") && ConfMan.getBool("dreamfactory_script_audit")) {
		auditScripts(root, Common::FSNode(ConfMan.getPath("dreamfactory_script_fixtures")));
		quitGame();
		// The audit reports failures in its machine-readable log. Avoid an error
		// dialog that would block unattended verification using SDL's dummy driver.
		return Common::kNoError;
	}
	// Normal launcher targets run the authored game. Keep the room viewer and
	// its pixel/motion audits available only through explicit developer flags.
	const bool viewer = ConfMan.hasKey("dreamfactory_viewer") && ConfMan.getBool("dreamfactory_viewer");
	const bool roomAudit = (ConfMan.hasKey("dreamfactory_verify") && ConfMan.getBool("dreamfactory_verify")) ||
		(ConfMan.hasKey("dreamfactory_smoke") && ConfMan.getBool("dreamfactory_smoke"));
	if ((!viewer && !roomAudit) || (ConfMan.hasKey("save_slot") && ConfMan.getInt("save_slot") >= 0)) {
		Session session(_system, this); _session = &session;
		bool started = session.init(root);
		if (started && ConfMan.hasKey("save_slot") && ConfMan.getInt("save_slot") >= 0) started = loadGameState(ConfMan.getInt("save_slot")).getCode() == Common::kNoError;
		else if (started) {
			const bool offer = ConfMan.hasKey("dreamfactory_continue_on_start") && ConfMan.getBool("dreamfactory_continue_on_start") && !session.recovery.list().empty();
			if (!offer || !session.continueRecovery()) started = !shouldQuit() && session.boot();
		}
		if (!started) { _session = nullptr; return session.scriptQuitRequested() || shouldQuit() ? Common::kNoError : Common::Error(Common::kUnknownError, session.runtime.error); }
		debug(1, "GAMEPLAY READY room=%s scene=%s view=%s stage=%s flat=%s", session.roomSystem.name().c_str(), session.roomSystem.sceneName().c_str(), session.roomSystem.viewName().c_str(), session.stageSystem.current ? session.stageSystem.current->name.c_str() : "none", session.stageSystem.currentFlat().c_str());
		while (!shouldQuit() && session.step()) {
			// Use ScummVM's configured autosave interval and reserved slot.
			// Never enter a save from inside a modal script or held gesture.
			serviceAutosave();
		}
		_session = nullptr;
		if (!session.runtime.error.empty()) return Common::Error(Common::kUnknownError, session.runtime.error);
		return Common::kNoError;
	}
	for (uint32 i = 0; i < files.size(); ++i)
		if (files[i].getName().matchString("*.SET", true)) _sets.push_back(files[i].getPath());
	if (_sets.empty()) return Common::Error(Common::kNoGameDataFoundError, "No SET room files found. Extract the CD DATA files beside BOOTFILE.");
	// Stable ordering regardless of filesystem enumeration order.
	for (uint32 i = 0; i < _sets.size(); ++i)
		for (uint32 j = i + 1; j < _sets.size(); ++j)
			if (_sets[j].toString() < _sets[i].toString()) SWAP(_sets[i], _sets[j]);
	if (ConfMan.hasKey("dreamfactory_verify") && ConfMan.getBool("dreamfactory_verify")) {
		if (!depthCodecSelfTest()) { warning("DEPTH CODEC FAIL"); return Common::kReadingFailed; }
		uint32 count = 0;
		for (uint32 i = 0; i < _sets.size(); ++i) {
			Common::File file;
			if (!file.open(Common::FSNode(_sets[i])) || file.size() <= 0 || file.size() > 128 * 1024 * 1024) return Common::kReadingFailed;
			Bytes data; data.resize(uint32(file.size()));
			if (file.read(data.data(), data.size()) != data.size() || !_room.load(data)) {
				warning("VERIFY failed loading %s", _sets[i].toString().c_str()); return Common::kReadingFailed;
			}
			uint32 hash = 2166136261u, depthHash = 2166136261u, depthCount = 0, roomCount = 0;
			auto verify = [&](const Sequence &seq) -> bool {
				Frame frame;
				for (uint32 f = 0; f < seq.size(); ++f) {
					if (!seq[f].image) continue;
					if (!decodeFrame(_room.archive.get(seq[f].image), frame)) {
						warning("VERIFY failed %s container %u", _sets[i].toString().c_str(), seq[f].image); return false;
					}
					for (uint32 p = 0; p < frame.pixels.size(); ++p) hash = (hash ^ frame.pixels[p]) * 16777619u;
					if (!frame.depth.empty()) {
						++depthCount;
						for (uint32 p = 0; p < frame.depth.size(); ++p) depthHash = (depthHash ^ frame.depth[p]) * 16777619u;
					}
					++roomCount;
				}
				return true;
			};
			for (uint32 s = 0; s < _room.scenes.size(); ++s)
				for (int dir = 0; dir < 2; ++dir) if (!verify(_room.scenes[s].turns[dir])) return Common::kReadingFailed;
			for (uint32 r = 0; r < _room.roads.size(); ++r)
				for (int dir = 0; dir < 2; ++dir) if (!verify(_room.roads[r].walks[dir])) return Common::kReadingFailed;
			for (uint32 m = 0; m < _room.moves.size(); ++m) if (!verify(_room.moves[m].frames)) return Common::kReadingFailed;
			for (uint32 s = 0; s < _room.scenes.size(); ++s)
				for (uint32 v = 0; v < _room.scenes[s].views.size(); ++v) {
					uint32 still = _room.scenes[s].views[v].still;
					if (still) { MotionFrame f; f.image = still; Sequence seq; seq.push_back(f); if (!verify(seq)) return Common::kReadingFailed; }
				}
			warning("VERIFY %s frames=%u fnv1a=%08x", _sets[i].getLastComponent().toString().c_str(), roomCount, hash);
			warning("DEPTH %s frames=%u fnv1a=%08x", _sets[i].getLastComponent().toString().c_str(), depthCount, depthHash);
			count += roomCount;
		}
		warning("VERIFY PASS rooms=%u frames=%u", _sets.size(), count);
		quitGame();
		return Common::kNoError;
	}
	if (!openSet(0)) return Common::Error(Common::kReadingFailed, "Could not read the initial DreamFactory room.");
	if (ConfMan.hasKey("dreamfactory_smoke") && ConfMan.getBool("dreamfactory_smoke")) {
		_system->saveScreenshot();
		for (uint32 i = 0; i < _sets.size(); ++i) {
			if (!openSet(i)) return Common::kReadingFailed;
			turn(0); turn(1); walk();
		}
		warning("SMOKE PASS rooms=%u", _sets.size());
		quitGame();
		return Common::kNoError;
	}
	while (!shouldQuit()) {
		Common::Event e;
		while (_eventMan->pollEvent(e)) {
			if (e.type != Common::EVENT_KEYDOWN) continue;
			switch (e.kbd.keycode) {
			case Common::KEYCODE_ESCAPE: quitGame(); break;
			case Common::KEYCODE_LEFT: turn(1); break;
			case Common::KEYCODE_RIGHT: turn(0); break;
			case Common::KEYCODE_UP: walk(); break;
			case Common::KEYCODE_PAGEUP: if (!openSet((_set + _sets.size() - 1) % _sets.size())) return Common::kReadingFailed; break;
			case Common::KEYCODE_PAGEDOWN: if (!openSet((_set + 1) % _sets.size())) return Common::kReadingFailed; break;
			case Common::KEYCODE_TAB: stand((_scene + 1) % _room.scenes.size(), 0); break;
			default: break;
			}
		}
		_system->delayMillis(10);
	}
	return Common::kNoError;
}
}
