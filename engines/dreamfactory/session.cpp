/* SPDX-License-Identifier: GPL-3.0-or-later
 * Scalar host semantics follow dreamREfactory/runtime/builtins.
 */
#include "dreamfactory/session.h"
#include "dreamfactory/puppet_view.h"
#include "dreamfactory/checkers.h"
#include "dreamfactory/controls.h"
#include "dreamfactory/inventory.h"
#include "common/events.h"
#include "common/config-manager.h"
#include "common/archive.h"
#include "common/compression/unzip.h"
#include "common/file.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/ustr.h"
#include "common/translation.h"
#include "gui/message.h"
#include "engines/engine.h"
#include "graphics/surface.h"
#include "graphics/paletteman.h"
#include <math.h>
namespace DreamFactory {
void Session::resetClock() { _lastMillis = _system ? _system->getMillis() : 0; }
void Session::excludeWallTime(uint32 started) {
	if (!_system) return;
	const uint32 now = _system->getMillis();
	// Preserve time spent in gameplay before the operation. If a nested GUI
	// pause already reset the clock, discard only its remaining wall time.
	if (int32(_lastMillis - started) >= 0) _lastMillis = now;
	else _lastMillis += now - started;
}
bool Session::subtitlesEnabled() const {
	// Prefer the shared speech/subtitle mode when the player has explicitly
	// configured it. Otherwise preserve the original game's caption switch.
	if (ConfMan.getBool("speech_mute")) return true;
	if (ConfMan.hasKey("subtitles", Common::ConfigManager::kTransientDomain) ||
		ConfMan.hasKey("subtitles", ConfMan.getActiveDomainName()) ||
		ConfMan.hasKey("subtitles", Common::ConfigManager::kApplicationDomain))
		return ConfMan.getBool("subtitles");
	return !puppetParams.contains(7) || puppetParams[7];
}
Session::Session(OSystem *system, SaveHost *saveHost) : display(system), runtime(this), sound(resources, system ? system->getMixer() : nullptr), crickets(sound), cursors(resources, system != nullptr), castSystem(resources, runtime), propSystem(resources, runtime), puppetSystem(resources, runtime), puppetSpeech(*this), stageSystem(resources, runtime, this), roomSystem(resources, runtime, stageSystem, *this), motion(castSystem, runtime), balls(propSystem, castSystem, roomSystem, runtime, playerHitbox), movies(resources, sound, display, pendingEvents, system), _system(system), _saveHost(saveHost), _random("dreamfactory") {}
bool Session::init(const Common::FSNode &root) {
	_root = root;
	movies.setCursors(&cursors);
    _controlFocused = _controlHeld = _touchHeld = false;
	movies.setHotspotHints(ConfMan.hasKey("dreamfactory_hotspot_hints") && ConfMan.getBool("dreamfactory_hotspot_hints"));
	_hintValid = false; _hintTime = 0; _hintPoints.clear();
	puppetChoices.escape(); puppetSpeech.abandon(); display.reset(); _stageView.reset(); _puppetView.reset(); _fontLoaded = false; _paletteKind.clear(); _roomPalette.clear(); _stagePalette.clear(); _puppetPalette.clear(); _puppetArtPalette.clear(); roomSystem.reset(); cursors.reset(); crickets.reset(); stars.reset(); motion.reset(); balls.reset(); movies.reset(); sound.reset(); castSystem.reset(); propSystem.reset(); puppetSystem.reset(); stageSystem.reset(); scheduler.reset(); pendingEvents.clear();
	_time = 0; _lastMillis = _system ? _system->getMillis() : 0; _paletteBlack = false;
	runtime.abandon(); runtime.globals.clear(); runtime.fallbacks.clear(); runtime.error.clear(); bootScripts.clear();
	puppetParams.clear(); playerHitbox = Common::Point(); cameraHi = 0; memset(displayPalette, 0, sizeof(displayPalette)); lastResult = Value();
	journal.entries.clear(); _journalRequested = false;
	_recoveryRequested = false;
	_inventoryRequested = _inventoryShown = false; _inventoryOfferItem.clear();
	_inventoryClicks.clear(); _inventoryClickDown = _inventoryReopen = false; _inventoryClickDue = 0;
	routes = RouteStore(); recovery = RecoveryStore(); _routePending.clear(); _routeOwner.reset(); _savedRouteCursor = 0; _savedRouteChoice = -1; _savedRouteToken.clear(); _routeResumeScene = 0;
	_routeRevisiting = false;
	pointer = Common::Point(); _pointerButtons = 0; pointerDown = _stopped = _scriptQuit = _dialogActive = false;
	frameRate = 3; menuVisible = keyAborts = puppetGrab = screenBlack = false;
	Bytes data; Archive boot;
	if (!resources.init(root) || !resources.read("BOOTFILE", data) || !boot.load(data)) { runtime.fail("Could not load BOOTFILE"); return false; }
	Reader header(boot.get(0), 2); version = header.u32(); stageSystem.version = version;
	Common::MemoryReadStream bootStream(data.data(), data.size());
	_bootSignature = Common::computeStreamMD5AsString(bootStream);
	sound.enhancements.clear();
	_artPack.clear();
	if (ConfMan.hasKey("dreamfactory_artwork") && ConfMan.getBool("dreamfactory_artwork"))
		_artPack.init(root.getChild("enhancements"), _bootSignature);
	if (ConfMan.hasKey("dreamfactory_enhanced_music") && ConfMan.getBool("dreamfactory_enhanced_music"))
		sound.enhancements.init(root.getChild("enhancements"), _bootSignature);
	if (!header.good() || (version != 1 && version != 4)) { runtime.fail("Unsupported BOOTFILE version"); return false; }
	// Supplied retail BOOTFILE size, also used by the detection table. The
	// September demo predates retail's dual/both sound channel semantics.
	sound.setTitanicRetail(version == 4 && data.size() == 95488);
	sound.setTitanicDemo(version == 4 && !sound.isTitanicRetail());
	cursors.load(version == 1 ? "DF.EXE" : "TI.EXE");
	for (uint32 c = 1; c < boot.size(); ++c) {
		ScriptTokens tokens; Common::String error;
		if (!decodeScript(boot.get(c), tokens, error)) continue;
		bool handlers = false;
		for (uint32 t = 0; t < tokens.size(); ++t) if (tokens[t].opcode == 4001) handlers = true;
		if (!handlers) continue;
		Common::SharedPtr<ScriptInstance> instance(new ScriptInstance());
		instance->name = "boot";
		if (!parseScript(tokens, instance->script, error)) { runtime.fail(error); return false; }
		bootScripts.push_back(instance); runtime.fallbacks.push_back(instance.get());
	}
	if (bootScripts.empty()) { runtime.fail("BOOTFILE has no script containers"); return false; }
	if (_system && _saveHost) {
		Common::String error;
		if (!recovery.open(_system->getSavefileManager(), ConfMan.getActiveDomainName(), _bootSignature)) warning("Recovery profile unavailable");
		if (!routes.open(_system->getSavefileManager(), ConfMan.getActiveDomainName(), _bootSignature, error)) warning("Route profile: %s", error.c_str());
		routes.graph.restoreCursor(0, -1);
		const int32 budget = ConfMan.hasKey("dreamfactory_route_storage_mib") ? ConfMan.getInt("dreamfactory_route_storage_mib") : 1024;
		routes.setBudget(uint64(CLIP<int32>(budget, 64, 4096)) * 1024 * 1024);
	}
	return true;
}
uint32 Session::routeChapter() const {
	if ((version == 1 && !runtime.globals.contains("day")) || (version == 4 && !runtime.globals.contains(sound.isTitanicRetail() ? "mission" : "phase"))) return 0;
	const auto value = [&](const char *name) { const auto found = runtime.globals.find(name); return found == runtime.globals.end() ? Value() : found->_value; };
	if (version == 4 && sound.isTitanicRetail()) {
		const Common::String mission = value("mission").text();
		if (mission != "0" && mission != "1" && mission != "2" && mission != "3" && mission != "4") return 0;
	}
	return identifyRouteChapter(version, sound.isTitanicRetail(), value("tour").truth(), value("mission").number(), value("phase").number(), value("day").number());
}
RoutePreview Session::captureRoutePreview() const {
	RoutePreview preview;
	if (roomSystem.current) preview.location = roomSystem.current->data.name + " / " + roomSystem.sceneName() + " / " + roomSystem.viewName();
	const auto global = [&](const char *name) { const auto found = runtime.globals.find(name); return found == runtime.globals.end() ? Value() : found->_value; };
	if (runtime.globals.contains("hrs") && runtime.globals.contains("min")) preview.clock = Common::String::format("%02d:%02d", global("hrs").number(), global("min").number());
	if (version == 1 && runtime.globals.contains("day")) preview.clock = Common::String::format("Day %d / %s", global("day").number(), global("clock").text().c_str());
	if (runtime.globals.contains("handitem") && global("handitem").isString) preview.heldItem = global("handitem").text();
	for (const auto &prop : propSystem.props) {
		const Common::String owner = prop->owner.text();
		if (owner != (version == 1 ? "stranger" : "frank") && prop->name != preview.heldItem) continue;
		if (preview.inventory.size() >= 256) break;
		RouteInventoryItem item; item.name = prop->name; item.owner = owner; preview.inventory.push_back(item);
	}
	const Bytes &picture = display.picture();
	if (_system && picture.size() == 640 * 480 * 4) {
		preview.picture.resize(RoutePreview::kWidth * RoutePreview::kHeight * 3);
		for (uint32 y = 0; y < RoutePreview::kHeight; ++y) for (uint32 x = 0; x < RoutePreview::kWidth; ++x) for (uint32 c = 0; c < 3; ++c) {
			uint32 sum = 0;
			for (uint32 dy = 0; dy < 5; ++dy) for (uint32 dx = 0; dx < 5; ++dx) sum += picture[((y * 5 + dy) * 640 + x * 5 + dx) * 4 + c];
			preview.picture[(y * RoutePreview::kWidth + x) * 3 + c] = byte(sum / 25);
		}
	}
	return preview;
}
bool Session::recordRecovery(const Bytes &snapshot, Common::String &error) {
	if (!canAutosave()) { error = "Recovery requires a safe world view"; return false; }
	return recovery.write(snapshot, captureRoutePreview(), error);
}
bool Session::continueRecovery() {
	if (!_system || _dialogActive) return false;
	_recoveryRequested = false; pendingEvents.clear();
	const uint32 started = _system->getMillis();
	PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken(); _dialogActive = true;
	const RecoveryEntry selected = openRecovery(*this);
	_dialogActive = false; pointerDown = false; _pointerButtons = 0; _controlHeld = _touchHeld = false;
	bool restored = false;
	if (selected.sequence && !_system->getEventManager()->shouldQuit() && !_system->getEventManager()->shouldReturnToLauncher()) {
		Bytes snapshot; Common::String error;
		restored = recovery.read(selected, snapshot, error) && loadState(snapshot, error);
		if (!restored) { _dialogActive = true; journalNotice(error); _dialogActive = false; }
	}
	excludeWallTime(started);
	if (_system->getEventManager()->shouldQuit() || _system->getEventManager()->shouldReturnToLauncher()) { _stopped = true; runtime.abandon(); }
	return restored;
}
void Session::prepareRouteCheckpoint() {
	_routePending.clear(); _routeOwner.reset();
	_routePendingPreview = RoutePreview();
	_routePendingChapter = routeChapter();
	if (!_routeResumeScene) _routeRevisiting = false;
	if (!routes.active() || (ConfMan.hasKey("dreamfactory_routes") && !ConfMan.getBool("dreamfactory_routes"))) return;
	// This runs before dispatch, with no authored script stack to reconstruct.
	// pointerDown already reflects the queued press, so omit that one guard.
	if (_dialogActive || runtime.busy() || _paletteBlack || screenBlack || puppetSystem.current || movies.active() || roomSystem.moving || roomSystem.turning || !stageSystem.current) return;
	if (_routePendingChapter && roomSystem.current && roomSystem.visible && !_routeResumeScene) {
		for (const RouteNode *previous = routes.graph.node(routes.graph.cursor()); previous; previous = routes.graph.node(previous->parent)) {
			if (!previous->chapter || !previous->checkpoint) continue;
			// A new game starts a new story spine while keeping discovered outcomes.
			if (_routePendingChapter < previous->chapter) routes.graph.restoreCursor(0, -1);
			break;
		}
	}
	const uint32 started = _system ? _system->getMillis() : 0;
	Common::String error;
	if (!saveState(_routePending, error)) warning("Route checkpoint: %s", error.c_str());
	if (!_routePending.empty()) _routePendingPreview = captureRoutePreview();
	if (_routePendingChapter && roomSystem.current && roomSystem.visible && !_routePending.empty() && !routes.graph.chapterPoint(routes.graph.cursor(), _routePendingChapter)) {
		JournalEntry entry; entry.speaker = "Story return point"; entry.room = roomSystem.current->data.name; entry.time = elapsedMillis();
		entry.text = "Return to this part of the story to explore different outcomes.";
		const uint32 id = routes.graph.recordScene(entry);
		if (id) {
			routes.graph.setChapter(id, _routePendingChapter);
			if (!routes.writeCheckpoint(id, _routePending, error, &_routePendingPreview)) { warning("Chapter checkpoint: %s", error.c_str()); routes.save(error); }
		}
		_routePending.clear();
	}
	excludeWallTime(started);
}
void Session::recordRoute(const JournalEntry &entry, bool decision) {
	if (!routes.active() || (ConfMan.hasKey("dreamfactory_routes") && !ConfMan.getBool("dreamfactory_routes"))) return;
	const uint32 started = _system ? _system->getMillis() : 0;
	Common::String error;
	bool changed = decision;
	if (_routeOwner != puppetSystem.current || !routes.graph.sceneFor(routes.graph.cursor())) {
		changed = true;
		JournalEntry scene = entry; scene.choices.clear(); scene.selected = -1;
		const RouteNode *resume = routes.graph.node(_routeResumeScene);
		const bool sameScene = resume && resume->kind == RouteNode::kScene && resume->entry.file.equalsIgnoreCase(entry.file) && resume->entry.room == entry.room && resume->entry.speaker == entry.speaker;
		const uint32 id = sameScene ? _routeResumeScene : routes.graph.recordScene(scene, _routeRevisiting);
		_routeResumeScene = 0;
		if (!id) { _routePending.clear(); _routeOwner.reset(); routes.graph.restoreCursor(0, -1); excludeWallTime(started); return; }
		_routeOwner = puppetSystem.current;
		const uint32 chapter = _routePending.empty() ? routeChapter() : _routePendingChapter;
		routes.graph.setChapter(id, chapter);
		const bool needsCheckpoint = !chapter || !routes.graph.chapterCheckpoint(routes.graph.node(id)->parent, chapter);
		if (id && !sameScene && needsCheckpoint && !routes.graph.node(id)->checkpoint && !_routePending.empty() && !routes.writeCheckpoint(id, _routePending, error, &_routePendingPreview)) warning("Route checkpoint: %s", error.c_str());
		_routePending.clear();
	}
	if (decision) routes.graph.recordDecision(entry);
	if (changed && !routes.save(error)) warning("Route profile: %s", error.c_str());
	excludeWallTime(started);
}
bool Session::restoreRouteScene(uint32 id, Common::String &error) {
	if (_dialogActive) { error = "Close the journal before restoring a scene"; return false; }
	const uint32 scene = routes.graph.checkpointFor(id);
	Bytes bytes;
	const uint32 started = _system ? _system->getMillis() : 0;
	const bool restored = routes.readCheckpoint(id, bytes, error) && loadState(bytes, error);
	if (restored) { routes.graph.restoreCursor(scene, -1); _routeResumeScene = scene; _routeRevisiting = true; }
	excludeWallTime(started); return restored;
}
bool Session::boot() {
	for (uint32 i = 0; i < bootScripts.size(); ++i) {
		if (!bootScripts[i]->script.handlers.contains("boot")) continue;
		const ScriptResult result = runtime.run(*bootScripts[i], "boot");
		if (result.signal == ScriptResult::kError || result.signal == ScriptResult::kAbandoned || !runtime.error.empty()) return false;
		presentPuppet();
		return runtime.error.empty();
	}
	runtime.fail("BOOTFILE has no boot handler"); return false;
}
bool Session::resolveLibrary(Runtime &vm, const Common::String &name, const Values &args, ScriptFrame &frame, Value &result) {
	const auto stage = stageSystem.current;
	if (!stage || !stage->main->script.handlers.contains(name)) return false;
	result = vm.run(*stage->main, name, args, frame.target, &frame).value; return true;
}
bool Session::invoke(Runtime &vm, const Expression &call, ScriptFrame &frame, Value &result) {
	debug(2, "SCRIPT HOST %s opcode=%u owner=%s", call.name.c_str(), call.opcode, frame.instance->name.c_str());
	if (call.opcode == 20098) {
		const Values args = vm.arguments(call, frame); result = Value();
		if (!vm.current(frame)) return true;
		if (version != 1 || args.empty() || !args[0].text().equalsIgnoreCase("checkmove")) { vm.fail("Unsupported DreamFactory plug-in"); return true; }
		result = Value(checkersMove(args.size() > 1 ? args[1].text() : Common::String(), args.size() > 2 ? args[2].number() : 0, args.size() > 3 ? args[3].number() : 0, &_random));
		return true;
	}
	if (call.opcode == 12079 || call.opcode == 20068) {
		const Values args = vm.arguments(call, frame); result = Value();
		if (!vm.current(frame)) return true;
		if (!_system) { vm.fail("Script dialogs require a display/input host"); return true; }
		const bool question = call.opcode == 20068;
		const Common::U32String message(args.empty() ? Common::String() : args[0].text(), Common::kWindows1252);
		// GUI dialogs pause the engine. Discard the opening gesture and reset
		// the clock on return so no game timers advance behind the overlay.
		pendingEvents.clear(); pointerDown = false; _pointerButtons = 0;
		_dialogActive = true; ++_dialogGeneration;
		GUI::MessageDialog dialog(message, question ? _("Yes") : _("OK"), question ? _("No") : Common::U32String());
		const int32 answer = dialog.runModal();
		_dialogActive = false; resetClock();
		if (_system->getEventManager()->shouldQuit()) { _stopped = true; vm.abandon(); return true; }
		if (question) result = Value(answer == GUI::kMessageOK ? 1 : 0);
		debug(1, "SCRIPT DIALOG kind=%s result=%d", question ? "question" : "note", result.number());
		return true;
	}
	if (call.opcode == 12071) {
		vm.arguments(call, frame); result = Value();
		if (!vm.current(frame)) return true;
		_scriptQuit = _stopped = true;
		pendingEvents.clear(); puppetSpeech.abandon();
		sound.haltSounds(); sound.haltVoice(); sound.haltTheme();
		vm.abandon(); return true;
	}
	if (call.opcode == 12077 || call.opcode == 12078) {
		result = Value();
		if (!_saveHost) { vm.fail("Save/load dialog host unavailable"); return true; }
		if (call.opcode == 12077) _saveHost->saveGame(); else _saveHost->loadGame();
		// A modal chooser freezes the game's clock, including cancellation.
		_lastMillis = _system ? _system->getMillis() : 0;
		return true;
	}
	if (call.opcode == 12081 || call.opcode == 20095) {
		result = Value();
		if (call.args.size() != 1 || call.args[0] >= frame.instance->script.expressions.size()) { vm.fail("sendtoboot requires a deferred handler"); return true; }
		const Expression &deferred = frame.instance->script.expressions[call.args[0]];
		if (deferred.kind != Expression::kCall) { vm.fail("sendtoboot requires a handler call"); return true; }
		const Values args = vm.arguments(deferred, frame); if (!vm.current(frame)) return true;
		for (uint32 i = 0; i < bootScripts.size(); ++i) if (bootScripts[i]->script.handlers.contains(deferred.name)) {
			result = vm.run(*bootScripts[i], deferred.name, args, frame.me, &frame, Runtime::kAddressed).value; break;
		}
		return true;
	}
	if (call.opcode == 12017 || call.opcode == 20083) {
		const Values args = vm.arguments(call, frame); if (!vm.current(frame)) return true;
		const Value a = args.size() ? args[0] : Value(); result = Value();
		if (call.opcode == 12017) {
			RouteEndingContext context; context.version = version; context.retail = sound.isTitanicRetail(); context.movie = a.text();
			context.stage = stageSystem.current ? stageSystem.current->name : Common::String();
			const auto global = [&](const char *name) { return runtime.globals.contains(name) ? runtime.globals[name] : Value(); };
			context.clock = global("clock").text(); context.day = global("day").number(); context.phase = global("phase").number();
			context.firstWar = global("onehappens").number(); context.secondWar = global("twohappens").number(); context.revolution = global("revhappens").number();
			_paletteBlack = false;
			const bool completed = movies.play(a.text(), vm, frame);
			JournalEntry ending;
			if (completed && vm.current(frame) && routes.active() && (!ConfMan.hasKey("dreamfactory_routes") || ConfMan.getBool("dreamfactory_routes")) && identifyRouteEnding(context, ending)) {
				const uint32 started = _system ? _system->getMillis() : 0;
				ending.file = resources.sourceName(a.text()); ending.room = roomSystem.current ? roomSystem.current->data.name : Common::String(); ending.time = _time;
				Common::String error;
				if (!routes.graph.cursor() && !_routePending.empty()) {
					const uint32 scene = routes.graph.recordScene(ending);
					if (scene && !routes.writeCheckpoint(scene, _routePending, error, &_routePendingPreview)) warning("Route checkpoint: %s", error.c_str());
					_routePending.clear();
				}
				const uint32 id = routes.graph.recordEnding(ending);
				if (!routes.save(error)) warning("Route profile: %s", error.c_str());
				debug(1, "ROUTE ENDING node=%u movie=%s outcome=%s", id, a.text().c_str(), ending.speaker.c_str());
				excludeWallTime(started);
			}
			if (_controlHeld) { _controlHeld = false; _pointerButtons &= ~1; pointerDown = _pointerButtons != 0; }
			memcpy(displayPalette, display.palette(), 768); _paletteKind = "movie"; sampleTime();
		}
		else result = Value(a.number() >= 1 && a.number() <= 2 && (movies.actions() & (1 << (a.number() - 1))) ? 1 : 0);
		return true;
	}
	// Effect opcodes are symbolic values passed to visualeffect, not draws.
	if (call.opcode >= 24001 && call.opcode <= 24021) { result = Value(call.name); return true; }
	if (textCall(call, frame, result)) return true;
	if (inputCall(call, frame, result)) return true;
	if (cricketCall(call, frame, result)) return true;
	if (screenCall(call, frame, result)) return true;
	if (roomCall(call, frame, result)) return true;
	if (motionCall(call, frame, result)) return true;
	if (timingCall(call, frame, result)) return true;
	if (actorCall(call, frame, result)) return true;
	if (propCall(call, frame, result)) return true;
	if (puppetCall(call, frame, result)) return true;
	if (stageCall(call, frame, result)) return true;
	if (audioCall(call, frame, result)) return true;
	// Decide support before evaluating arguments: an unsupported host call may
	// still resolve to a user routine in a containing script or boot library.
	switch (call.opcode) {
	case 12001: case 12002: case 12003: case 12039: case 12040: case 12053: case 12074:
	case 16009: case 16010: case 16022: case 16030: case 16037: case 16045: case 16049:
	case 20001: case 20002: case 20003: case 20004: case 20025: case 20026:
	case 20054: case 20055: case 20056: case 20057: case 20058: case 20059: case 20060: case 20061: case 20062: case 20065: case 20067: case 20071: case 20073: case 20081: case 20082: case 20108: case 20121:
		break;
	case 16034: if (version == 1) return false; break; // Dust: actorhitbox.
	case 16035: break;
	default: return false;
	}
	const Values args = vm.arguments(call, frame);
	if (!vm.current(frame)) return true;
	const Value a = args.size() > 0 ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value();
	const Value c = args.size() > 2 ? args[2] : Value(), d = args.size() > 3 ? args[3] : Value();
	result = Value();
	switch (call.opcode) {
	case 12001: { Common::String text; for (uint32 i = 0; i < args.size(); ++i) { if (i) text += " "; text += args[i].text(); } debug(1, "SCRIPT MESSAGE %s", text.c_str()); break; }
	case 12002: cursors.hide(); break;
	case 12003: cursors.show(); break;
	case 12039: cursors.set(a.text()); break;
	case 12040: warning("DreamFactory script debugger marker"); break;
	case 12053: vm.fail("Game script requested error()"); break;
	case 12074: puppetGrab = a.truth(); break;
	case 16009:
		if (args.size() < 2) result = Value(resources.path(uint32(a.number())));
		else resources.setPath(uint32(a.number()), b.text());
		break;
	case 16010: result = lastResult; break;
	case 16022: if (args.empty()) result = Value(frameRate); else frameRate = CLIP<int32>(a.number(), 0, 60); break;
	case 16030: {
		Common::String key = a.text(); key.toLowercase();
		if (args.size() < 2) result = vm.get(key, frame);
		else if (frame.locals.contains(key)) frame.locals[key] = b;
		else vm.globals[key] = b;
		break;
	}
	case 16034: if (!args.empty()) resources.mount(a.text()); result = Value(resources.currentCD()); break;
	case 16035:
		if (version == 1) result = Value(roomSystem.current ? roomSystem.current->data.eyeHeight : 0);
		else if (args.empty()) result = Value(cameraHi); else cameraHi = int16(a.number());
		break;
	case 16037: if (!args.empty()) menuVisible = a.truth(); result = Value(menuVisible ? 1 : 0); break;
	case 16045: if (!args.empty()) keyAborts = a.truth(); result = Value(keyAborts ? 1 : 0); break;
	case 16049:
		if (args.size() > 1) puppetParams[a.number()] = b.number();
		else result = Value(puppetParams.contains(a.number()) ? puppetParams[a.number()] : a.number() == 7 ? 1 : 0);
		break;
	case 20001:
		if (a.number() <= 0) vm.fail("random() requires a positive range");
		else result = Value(int32(_random.getRandomNumber(uint32(a.number() - 1))) + 1);
		break;
	case 20002: result = Value(int16(uint32(a.number()) >> 16)); break;
	case 20003: result = Value(int16(a.number())); break;
	case 20004: result = Value(int32((uint32(a.number()) << 16) | uint16(b.number()))); break;
	case 20025: result = Value(a.number()); break;
	case 20026: result = Value(Common::String::format("%d", a.number())); break;
	case 20054: case 20057: {
		const Common::String text = a.text(), separator = args.size() > 1 ? b.text() : Common::String();
		int32 index = c.number();
		if (separator.empty()) {
			if (index >= 1 && index <= int32(text.size()))
				result = Value(call.opcode == 20054 ? text.substr(index - 1, 1) : text.substr(0, index - 1) + d.text() + text.substr(index - 1));
			else result = Value(call.opcode == 20057 && index == int32(text.size()) + 1 ? text + d.text() : Common::String());
		} else {
			Common::Array<Common::String> words; uint32 at = 0;
			for (;;) { uint32 next = text.find(separator, at); words.push_back(text.substr(at, next == Common::String::npos ? next : next - at)); if (next == Common::String::npos) break; at = next + separator.size(); }
			if (call.opcode == 20054) { if (!index) index = 1; result = Value(index > 0 && uint32(index) <= words.size() ? words[index - 1] : Common::String()); }
			else {
				index = MAX<int32>(1, index);
				if (index > 65536) { vm.fail("putword() index too large"); break; }
				while (words.size() < uint32(index)) words.push_back(Common::String());
				words[index - 1] = d.text(); Common::String joined;
				for (uint32 w = 0; w < words.size(); ++w) { if (w) joined += separator; joined += words[w]; }
				result = Value(joined);
			}
		}
		break;
	}
	case 20055: {
		Common::String text = a.text(), needle = b.text(); text.toLowercase(); needle.toLowercase();
		const uint32 found = text.find(needle); result = Value(found == Common::String::npos ? -1 : int32(found + 1)); break;
	}
	case 20056: result = Value(int32(a.text().size())); break;
	case 20058: case 20059:
		result = Value(_system && (_system->getEventManager()->getModifierState() & (call.opcode == 20058 ? Common::KBD_ALT : Common::KBD_SHIFT)) ? 1 : 0); break;
	case 20060: result = Value(_system && (_system->getEventManager()->getModifierState() & Common::KBD_META) ? 1 : 0); break;
	case 20065: result = Value("win"); break; // The supplied data uses Windows help assets.
	case 20061: case 20062: {
		// Fixed-point vector components, as in runtime/builtins/helpers.ts.
		// The original masks the heading, sign-extends magnitude and result,
		// and divides the 14-bit trig product toward zero.
		const double angle = (uint32(a.number()) & 255) * M_PI / 128;
		const int32 entry = int32(floor(16384 * (call.opcode == 20061 ? cos(angle) : sin(angle)) + 0.5));
		result = Value(int16(entry * int16(b.number()) / 16384)); break;
	}
	case 20067: { Common::FSNode node; result = Value(resources.resolve(a.text(), node) ? 1 : 0); break; }
	case 20071: result = Value(packedBearing(a.number(), b.number())); break;
	case 20073: {
		const Star *star = stars.find(a.text()); const int32 axis = args.size() > 1 ? b.number() : 1;
		if (star) result = Value(axis == 1 ? star->x : axis == 2 ? star->y : axis == 3 ? star->z : axis == 4 ? int32(uint32(star->x) << 16 | uint16(star->y)) : 0);
		break;
	}
	case 20081: {
		// DF.EXE 0x405e00 and TI.EXE 0x4276a0: signed packed halves,
		// 32-bit squared sum and an unsigned, truncated integer square root.
		const int64 dx = int16(uint32(a.number()) >> 16) - int16(uint32(b.number()) >> 16), dy = int16(a.number()) - int16(b.number());
		const uint32 square = uint32(dx * dx + dy * dy);
		result = Value(int32(uint16(sqrt(double(square))))); break;
	}
	case 20082: { const int64 x = a.number(), mod = b.number(); result = Value(mod ? int32((x % mod + mod) % mod) : 0); break; }
	case 20108: result = Value(a.number() > 0 ? int32(sqrt(double(a.number()))) : 0); break;
	case 20121: result = Value(64 * 1024 * 1024); break;
	}
	return true;
}
bool Session::sourcePalette(Common::String name, byte *palette) {
	name.toLowercase();
	if (name == "black") memset(palette, 0, 768);
	else if (name == "current") memcpy(palette, displayPalette, 768);
	else if (name == "set" && roomSystem.current) memcpy(palette, roomSystem.current->data.palette, 768);
	else if (name == "stage" && stageSystem.current) memcpy(palette, stageSystem.current->data.palette, 768);
	else if (name == "puppet" && puppetSystem.current) memcpy(palette, puppetSystem.current->data.palette, 768);
	else { runtime.fail("Palette source not loaded: " + name); return false; }
	return true;
}
void Session::showPalette(const byte *palette) {
	display.palette(palette);
}
void Session::waitFade(uint64 tick, ScriptFrame &frame) {
	// Original TI.EXE 0x435b90/0x435be0 waits on the 60 Hz clock without
	// servicing scripts, walking, or loops. Only retain input and honor quit.
	while (Scheduler::ticks(_time) < tick && runtime.current(frame)) {
		if (!_system) { _time = (tick * 50 + 2) / 3; break; }
		pollInput();
		if (!runtime.current(frame)) break;
		_system->delayMillis(1); sampleTime();
	}
	runtime.yielded();
}
bool Session::screenCall(const Expression &call, ScriptFrame &frame, Value &result) {
	if (call.opcode != 12009 && call.opcode != 12038 && call.opcode != 12049 && call.opcode != 12050 && call.opcode != 12051 && call.opcode != 12082) return false;
	const Values args = runtime.arguments(call, frame); result = Value(); if (!runtime.current(frame)) return true;
	if (call.opcode == 12009) {
		Common::String effect = args.empty() ? "plain" : args[0].text(); effect.toLowercase();
		if (effect == "plain") { presentPuppet(); return true; }
		if (effect != "barndooropen" && effect != "barndoorclose" && effect != "wipeleft" && effect != "wiperight" && effect != "irisopen" && effect != "irisclose") { runtime.fail("Visual transition not implemented: " + effect); return true; }
		const uint32 steps = CLIP<int32>(args.size() > 1 ? args[1].number() : 1, 1, 1000);
		// DF.EXE 0x40ef90 / 0x40f0d0 uses six closing strips or sixteen
		// opening apertures, with the requested delay divided among them.
		const bool dustIris = version == 1 && (effect == "irisopen" || effect == "irisclose");
		const uint32 passes = dustIris ? (effect == "irisopen" ? 16 : 6) : steps;
		const uint32 delay = dustIris ? MAX<uint32>(1, steps / passes) : 1;
		Bytes from; display.capture(from);
		_paletteBlack = screenBlack = false;
		// Capture the arriving screen once. The effect is modal: its waits
		// retain input and honor quit without advancing world scripts.
		display.hold(true); presentPuppet(); display.hold(false); if (!runtime.current(frame)) return true;
		sampleTime(); const uint64 start = Scheduler::ticks(_time);
		debug(2, "SCREEN TRANSITION effect=%s steps=%u", effect.c_str(), steps);
		for (uint32 step = 0; step <= passes && runtime.current(frame); ++step) {
			if (step) waitFade(start + step * delay, frame);
			if (!runtime.current(frame)) break;
			if (_system && !display.transition(from, effect, step, passes, version == 1)) { runtime.fail("Cannot render visual transition: " + effect); break; }
		}
		return true;
	}
	if (call.opcode == 12051) { screenBlack = true; display.clear(); return true; }
	const Common::String name = args.empty() ? "current" : args[0].text(); byte source[768], shown[768];
	if (!sourcePalette(name, source)) return true;
	Common::String layer = name; layer.toLowercase(); if (layer == "current") layer = _paletteKind;
	Bytes *paletteOverride = layer == "set" ? &_roomPalette : layer == "stage" ? &_stagePalette : layer == "puppet" ? &_puppetPalette : nullptr;
	if (!name.equalsIgnoreCase("black")) memcpy(displayPalette, source, 768);
	if (call.opcode == 12038) {
		_paletteBlack = screenBlack = name.equalsIgnoreCase("black");
		if (_paletteBlack) { if (display.rgb()) display.fade(0, 1); else showPalette(source); }
		else {
			if (paletteOverride) paletteOverride->clear(); if (layer == "puppet") _puppetArtPalette.clear();
			if (!display.rgb()) showPalette(source);
			else if (display.puppet() || stageSystem.current || (_paletteKind == "set" && roomSystem.current)) presentPuppet();
			else showPalette(source);
		}
		return true;
	}
	if (call.opcode == 12082) {
		if (args.size() < 5) { runtime.fail("mixclut requires a source, color, range and amount"); return true; }
		byte color[768]; if (!sourcePalette(args[1].text(), color)) return true;
		memcpy(shown, source, 768);
		const int32 lo = CLIP<int32>(args[2].number(), 0, 255), hi = CLIP<int32>(args[3].number(), 0, 255), amount = CLIP<int32>(args[4].number(), 0, 255);
		for (int32 i = lo * 3; i < (hi + 1) * 3; ++i) shown[i] = byte((source[i] * (255 - amount) + color[i] * amount) / 255);
		if (paletteOverride) { paletteOverride->resize(768); memcpy(paletteOverride->data(), shown, 768); }
		if (layer == "puppet") {
			_puppetArtPalette.resize(768); memcpy(_puppetArtPalette.data(), source, 768);
			memset(_puppetArtPalette.data(), 0, 3); memset(_puppetArtPalette.data() + 765, 255, 3);
			if (!args[1].text().equalsIgnoreCase("black")) { memset(color, 0, 3); memset(color + 765, 255, 3); }
			for (int32 i = lo * 3; i < (hi + 1) * 3; ++i) _puppetArtPalette[i] = byte((_puppetArtPalette[i] * (255 - amount) + color[i] * amount) / 255);
		}
		_paletteBlack = false;
		if (!display.rgb()) showPalette(shown); else if (display.puppet() || stageSystem.current || (_paletteKind == "set" && roomSystem.current)) presentPuppet(); else showPalette(shown);
		return true;
	}
	const int32 requested = args.size() > 1 ? args[1].number() : 10, steps = MAX<int32>(1, requested ? requested : 10);
	const bool reveal = call.opcode == 12049; sampleTime(); const uint64 start = Scheduler::ticks(_time);
	debug(2, "SCREEN RAMP source=%s steps=%d start=%llu rgb=%u", name.c_str(), steps, (unsigned long long)start, display.rgb() ? 1 : 0);
	if (reveal) { if (display.rgb()) display.fade(0, 1); else { memset(shown, 0, sizeof(shown)); showPalette(shown); } _paletteBlack = true; }
	for (int64 step = 1; step <= steps && runtime.current(frame); ++step) {
		waitFade(start + step, frame); if (!runtime.current(frame)) break;
		const int64 numerator = reveal ? step : steps - step;
		if (display.rgb()) display.fade(numerator, steps);
		else { for (uint32 c = 0; c < 768; ++c) shown[c] = byte(uint64(source[c]) * numerator / steps); showPalette(shown); }
	}
	if (runtime.current(frame)) _paletteBlack = screenBlack = !reveal;
	debug(2, "SCREEN RAMP DONE");
	return true;
}
void Session::sampleTime() {
	if (_system) { const uint32 now = _system->getMillis(); _time += uint32(now - _lastMillis); _lastMillis = now; }
}
void Session::advanceTime(uint64 now, bool dispatch) {
	if (now < _time) return;
	_time = now; scheduler.advance(now, frameRate, dispatch, *this);
}
bool Session::canFireLoop(const GameLoop &loop) {
	if (!runtime.error.empty()) return false;
	if (loop.kind == "flat") return !stageSystem.running(loop.name, loop.handler);
	if (loop.kind == "scene") return !roomSystem.running(loop.name, loop.handler);
	if (loop.kind == "prop") return !propSystem.running(loop.name, loop.handler);
	return loop.kind != "actor" || !castSystem.running(loop.name, loop.handler);
}
bool Session::fireLoop(const GameLoop &loop) {
	Common::String handler = loop.handler; handler.toLowercase();
	if (loop.kind != "actor" && loop.kind != "prop" && loop.kind != "flat" && loop.kind != "scene") { runtime.fail("Scheduler target kind not implemented: " + loop.kind); return false; }
	ScriptFrame context = runtime.eventFrame(loop.name, handler);
	const bool consumed = runtime.eventConsumed; runtime.eventConsumed = false;
	if (loop.kind == "scene") roomSystem.send(false, loop.name, handler, Values(), context, true);
	else if (loop.kind == "flat") stageSystem.send(false, loop.name, handler, Values(), context, true);
	else if (loop.kind == "prop") propSystem.send(true, loop.name, handler, Values(), context, true);
	else castSystem.send(true, loop.name, handler, Values(), context, true);
	runtime.eventConsumed = consumed;
	return runtime.current(context);
}
void Session::yieldFrame(bool cooperative) {
	ScriptFrame context = runtime.eventFrame(Common::String(), Common::String());
	if (_system) {
		pollInput();
		if (!runtime.busy() || runtime.isRunning("idle") || puppetChoices.active()) updateHoverCursor();
		_system->updateScreen(); _system->delayMillis(10);
		sampleTime();
	} else _time += 10;
	if (runtime.current(context)) { scheduler.advance(_time, frameRate, cooperative || !runtime.busy(), *this); if (runtime.current(context)) presentPuppet(); }
	runtime.yielded();
}
bool Session::timingCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12004: case 12005: case 12011: case 12052: case 16046: case 20008: case 20010: case 20074: case 20102: case 20105: break;
	default: return false;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value(), c = args.size() > 2 ? args[2] : Value();
	result = Value();
	switch (call.opcode) {
	case 12005:
		if (!scheduler.make(a.text(), b.text(), c.text(), args.size() > 3 ? args[3].number() : 1)) warning("DreamFactory loop table full; dropping %s/%s", a.text().c_str(), b.text().c_str());
		break;
	case 12011: scheduler.stop(a.text(), b.text()); break;
	case 16046: scheduler.pause(a.text(), b.text(), args.size() < 3 || c.truth()); break;
	case 20010: result = Value(scheduler.has(a.text(), b.text()) ? 1 : 0); break;
	case 20105: result = Value(int32(scheduler.loops.size())); break;
	case 20102: result = Value(a.number() > 0 && uint32(a.number()) <= scheduler.loops.size() ? scheduler.loops[a.number() - 1]->name : Common::String()); break;
	case 20008: sampleTime(); result = Value(int32(Scheduler::ticks(_time))); break;
	case 20074: result = Value(int32(scheduler.frame)); break;
	case 12004: {
		sampleTime(); const uint64 deadline = _time + (uint64(MAX<int32>(0, a.number())) * 50 + 2) / 3;
		while (_time < deadline && runtime.current(frame)) yieldFrame(false);
		break;
	}
	case 12052: {
		if (!_system) { advanceTime(_time + 50, true); runtime.yielded(); break; }
		sampleTime(); const uint64 deadline = Scheduler::ticks(_time) + MAX<int32>(1, frameRate);
		do { yieldFrame(true); } while (Scheduler::ticks(_time) < deadline && runtime.current(frame));
		break;
	}
	}
	return true;
}
bool Session::audioCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12019: case 12020: case 12021: case 12022: case 12023: case 12024: case 12025: case 12026: case 12028: case 12029: case 12030:
	case 16033: case 16038: case 16039: case 16040: case 16041:
	case 20014: case 20015: case 20016: case 20075: case 20076: case 20077: case 20078: case 20079: case 20097:
		break;
	default: return false;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value();
	result = Value();
	switch (call.opcode) {
	case 12019: if (!sound.open(a.text())) runtime.fail("Cannot open sound bank: " + a.text()); break;
	case 12020: sound.close(a.text()); break;
	case 12021: if (!sound.playTheme(args.empty() ? Common::String() : a.text())) runtime.fail("Cannot play theme: " + a.text()); break;
	case 12022: case 12023: case 12024: case 12025: case 12026:
		if (!sound.playSound(a.text(), static_cast<SoundSystem::SoundMode>(call.opcode - 12022))) runtime.fail("Cannot play sound: " + a.text());
		break;
	case 12028: sound.haltSounds(); break;
	case 12029: sound.haltTheme(); break;
	case 12030: sound.haltVoice(); break;
	case 16033: if (!args.empty()) sound.setWaveVolume(a.number()); result = Value(sound.waveVolume()); break;
	case 16038: if (args.size() > 1) sound.setSoundPan(a.text(), b.number()); result = Value(sound.soundPan(a.text())); break;
	case 16039: sound.soundLoop(a.text(), args.size() < 2 || b.truth()); break;
	case 16040: if (args.size() > 1) sound.setSoundVolume(a.text(), b.number()); result = Value(sound.soundVolume(a.text())); break;
	case 16041: if (args.size() > 1) sound.setThemeVolume(a.text(), b.number()); else result = Value(sound.themeVolume(a.text())); break;
	case 20014: result = Value(int32(sound.soundNames(a.text()).size())); break;
	case 20015: { const auto names = sound.soundNames(a.text()); result = Value(b.number() >= 1 && uint32(b.number()) <= names.size() ? names[b.number() - 1] : Common::String()); break; }
	case 20075: result = Value(int32(sound.trackNames().size())); break;
	case 20076: { const auto names = sound.trackNames(); result = Value(a.number() >= 1 && uint32(a.number()) <= names.size() ? names[a.number() - 1] : Common::String()); break; }
	case 20016: case 20097: yieldFrame(); result = Value(sound.done(call.opcode == 20097) ? 1 : 0); break;
	case 20077: yieldFrame(); result = Value(sound.currentSound(args.empty() ? 1 : uint32(a.number()))); break;
	case 20078: yieldFrame(); result = Value(sound.currentVoice()); break;
	case 20079: result = Value(sound.currentTheme()); break;
	}
	return true;
}
bool Session::actorCall(const Expression &call, ScriptFrame &frame, Value &result) {
	// DF.EXE 0x4261a0 / 0x4262b0: v1 opcode 16034 is actorhitbox,
	// a signed-word width/height setter or an axis (1/2) getter. "player"
	// owns a separate pair; this opcode never mounts a disc in Dust.
	if (call.opcode == 16034 && version == 1) {
		const Values args = runtime.arguments(call, frame); result = Value(); if (!runtime.current(frame)) return true;
		if (args.size() < 2) { runtime.fail("actorhitbox requires a target and axis or dimensions"); return true; }
		const auto actor = castSystem.get(args[0].text()); const bool player = args[0].text().equalsIgnoreCase("player");
		if (!player && !actor) { runtime.fail("actorhitbox actor not found: " + args[0].text()); return true; }
		if (args.size() >= 3) {
			const int16 w = int16(args[1].number()), h = int16(args[2].number());
			if (player) playerHitbox = Common::Point(w, h); else { actor->hitWidth = w; actor->hitHeight = h; }
		} else {
			const int32 axis = args[1].number(); if (axis != 1 && axis != 2) runtime.fail("actorhitbox axis must be 1 or 2");
			else result = Value(player ? (axis == 1 ? playerHitbox.x : playerHitbox.y) : (axis == 1 ? actor->hitWidth : actor->hitHeight));
		}
		return true;
	}
	switch (call.opcode) {
	case 12013: case 12014: case 12016: case 12048: case 12075: case 12085: case 12088: case 12093: case 12104:
	case 16001: case 16002: case 16003: case 16004: case 16005: case 16006: case 16012: case 16014: case 16021: case 16023: case 16024: case 16036: case 16043: case 16044: case 16051:
	case 20012: case 20013: case 20023: case 20029: case 20030: case 20084: case 20087: break;
	default: return false;
	}
	result = Value();
	// Only the deferred call's arguments belong to the caller. Its handler is
	// resolved on the recipient and must never execute while evaluating args.
	if (call.opcode == 12016 || call.opcode == 12048 || call.opcode == 20084 || call.opcode == 20087) {
		if (call.args.size() < 2) { runtime.fail("sendto requires a target and deferred handler"); return true; }
		const Value name = runtime.evaluate(call.args[0], frame);
		if (!runtime.current(frame)) return true;
		if (call.args[1] >= frame.instance->script.expressions.size()) { runtime.fail("Invalid deferred call"); return true; }
		const Expression &deferred = frame.instance->script.expressions[call.args[1]];
		if (deferred.kind != Expression::kCall) { runtime.fail("sendto requires a deferred handler call"); return true; }
		const Values args = runtime.arguments(deferred, frame);
		if (runtime.current(frame)) result = castSystem.send(call.opcode == 12016 || call.opcode == 20084, name.text(), deferred.name, args, frame);
		return true;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value();
	switch (call.opcode) {
	case 12013: result = Value(castSystem.open(a.text(), &frame) ? 1 : 0); return true;
	case 12014: castSystem.close(a.text()); motion.prune(); return true;
	case 12075: castSystem.instance(a.text(), b.text()); return true;
	case 12085: case 12088: castSystem.warm(a.text(), call.opcode == 12088); return true;
	case 12093: motion.stop(a.text()); castSystem.remove(a.text()); return true;
	case 20012: result = Value(int32(castSystem.actors.size())); return true;
	case 20013: if (a.number() > 0 && uint32(a.number()) <= castSystem.actors.size()) result = Value(castSystem.actors[a.number() - 1]->name); else result = Value(Common::String()); return true;
	case 20029: result = Value(int32(castSystem.casts.size())); return true;
	case 20030: if (a.number() > 0 && uint32(a.number()) <= castSystem.casts.size()) result = Value(castSystem.casts[a.number() - 1]->name); else result = Value(Common::String()); return true;
	}
	const auto actor = castSystem.get(a.text());
	if (!actor) { if (call.opcode == 16012 && args.size() == 1) result = Value(32000); if (call.opcode == 16006 || call.opcode == 16014 || call.opcode == 16021 || call.opcode == 16043) result = Value(Common::String()); return true; }
	const bool set = args.size() > 1;
	switch (call.opcode) {
	case 20023: result = Value(1); break;
	case 12104: case 16001:
		if (set || call.opcode == 12104) {
			actor->visible = call.opcode == 16001 && b.truth();
			const auto attention = runtime.globals.find("curattention");
			if (!actor->visible && attention != runtime.globals.end() && attention->_value.text().equalsIgnoreCase(actor->name)) attention->_value = Value(Common::String());
		} else result = Value(actor->visible ? 1 : 0);
		break;
	case 16002: if (set) actor->deg = b.number() & 255; else result = Value(actor->deg); break;
	case 16003:
		if (args.size() > 2) { actor->x = b.number(); actor->y = args[2].number(); actor->z = args.size() > 3 ? args[3].number() : 0; actor->starPending = false; actor->worldSpace = true; }
		else if (set) { switch (b.number()) { case 1: result = Value(actor->x); break; case 2: result = Value(actor->y); break; case 3: result = Value(actor->z); break; case 4: result = Value(int32((uint32(actor->x) << 16) | uint16(actor->y))); break; } }
		break;
	case 16004: if (args.size() > 2) { actor->anchorX = b.number(); actor->anchorY = args[2].number(); actor->worldSpace = false; } else if (set) result = Value(b.number() == 2 ? actor->anchorY : actor->anchorX); break;
	case 16005: if (set) actor->worldSpace = b.truth(); else result = Value(actor->worldSpace ? 1 : 0); break;
	case 16006:
		if (set) {
			actor->star = b.text(); actor->star.toLowercase(); const Star *star = stars.find(actor->star);
			actor->starPending = !star && actor->star.contains('.');
			if (star) { actor->x = star->x; actor->y = star->y; actor->z = star->z; actor->deg = star->facing & 255; actor->worldSpace = true; }
		} else result = Value(actor->star);
		break;
	case 16012:
		if (set) actor->dist = b.number();
		else {
			WorldCamera camera;
			if (!roomSystem.visible || (puppetSystem.current && puppetSystem.current->visible) || !roomSystem.worldCamera(camera, cameraHi) || !castSystem.onScreen(*actor, camera, roomSystem.current->data.name)) result = Value(32000);
			else { const double dx = double(actor->x) - camera.x, dy = double(actor->y) - camera.y; result = Value(int32(uint32(int64(floor(sqrt(dx * dx + dy * dy) + 0.5))))); }
		}
		break;
	case 16014: if (set) { actor->pose = b.text(); actor->pose.toLowercase(); actor->step = 0; } else result = Value(actor->pose); break;
	case 16021: if (set) { actor->set = b.text(); actor->set.toLowercase(); } else result = Value(actor->set); break;
	case 16023: if (set) actor->speed = b.number(); else result = Value(actor->speed); break;
	case 16024: if (set) actor->scale = b.number(); else result = Value(actor->scale); break;
	case 16036: if (set) actor->turn = b.number(); else result = Value(actor->turn); break;
	case 16043: if (set) actor->owner = b; else result = actor->owner; break;
	case 16044: if (set) actor->value = b; else result = actor->value; break;
	case 16051: if (set) actor->zclip = b.number(); else result = Value(actor->zclip); break;
	}
	return true;
}
bool Session::propCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12055: case 12056: case 12057: case 12059: case 12076: case 12086: case 12087: case 12094: case 12105:
	case 16013: case 16015: case 16016: case 16017: case 16018: case 16019: case 16020: case 16025: case 16026: case 16027: case 16028: case 16032: case 16042: case 16052:
	case 20024: case 20031: case 20032: case 20033: case 20034: case 20040: case 20088: case 20089: break;
	default: return false;
	}
	result = Value();
	if (call.opcode == 12055 || call.opcode == 12059 || call.opcode == 20088 || call.opcode == 20089) {
		if (call.args.size() < 2) { runtime.fail("sendto requires a target and deferred handler"); return true; }
		const Value name = runtime.evaluate(call.args[0], frame); if (!runtime.current(frame)) return true;
		if (call.args[1] >= frame.instance->script.expressions.size()) { runtime.fail("Invalid deferred call"); return true; }
		const Expression &deferred = frame.instance->script.expressions[call.args[1]];
		if (deferred.kind != Expression::kCall) { runtime.fail("sendto requires a deferred handler call"); return true; }
		const Values args = runtime.arguments(deferred, frame);
		if (runtime.current(frame)) result = propSystem.send(call.opcode == 12055 || call.opcode == 20088, name.text(), deferred.name, args, frame);
		return true;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value();
	switch (call.opcode) {
	case 12056: result = Value(propSystem.open(a.text(), &frame) ? 1 : 0); return true;
	case 12057: propSystem.close(a.text(), &frame); return true;
	case 12076: propSystem.instance(a.text(), b.text()); return true;
	case 12086: case 12087: propSystem.warm(a.text(), call.opcode == 12087); return true;
	case 12094: propSystem.remove(a.text()); return true;
	case 20031: result = Value(int32(propSystem.props.size())); return true;
	case 20032: result = Value(a.number() > 0 && uint32(a.number()) <= propSystem.props.size() ? propSystem.props[a.number() - 1]->name : Common::String()); return true;
	case 20033: result = Value(int32(propSystem.shops.size())); return true;
	case 20034: result = Value(a.number() > 0 && uint32(a.number()) <= propSystem.shops.size() ? propSystem.shops[a.number() - 1]->name : Common::String()); return true;
	case 12105:
		for (uint32 p = 0; p < propSystem.props.size(); ++p) {
			const auto &prop = propSystem.props[p]; const bool hide = args.size() < 2 || b.truth();
			if (a.text().equalsIgnoreCase("all")) { if (!hide || prop->visible) prop->hidden = hide; }
			else if (prop->name.equalsIgnoreCase(a.text())) prop->hidden = hide;
		}
		return true;
	}
	const auto prop = propSystem.get(a.text());
	if (!prop) { if (call.opcode == 16020 || call.opcode == 16025 || call.opcode == 16027 || call.opcode == 16032) result = Value(Common::String()); return true; }
	const bool set = args.size() > 1;
	auto world = [&]() { prop->becomeWorld(); };
	switch (call.opcode) {
	case 20024: result = Value(1); break;
	case 20040: result = Value(prop->contains(int16(uint32(b.number()) >> 16), int16(b.number())) ? 1 : 0); break;
	case 16013: if (set) prop->dist = b.number(); else result = Value(prop->dist); break;
	case 16015: if (set) { const bool was = prop->visible; prop->visible = b.truth(); if (!was && prop->visible) eraseText(*prop); } else result = Value(prop->visible ? 1 : 0); break;
	case 16016: if (set) prop->selectDegree(b.number(), frame.event); else result = Value(prop->deg); break;
	case 16017:
		if (args.size() > 2) { world(); prop->x = b.number(); prop->y = args[2].number(); prop->z = args.size() > 3 ? args[3].number() : 0; prop->starPending = false; }
		else if (set) { switch (b.number()) { case 1: result = Value(prop->x); break; case 2: result = Value(prop->y); break; case 3: result = Value(prop->z); break; case 4: result = Value(int32((uint32(prop->x) << 16) | uint16(prop->y))); break; } }
		break;
	case 16018:
		if (args.size() > 2) { prop->anchorX = b.number(); prop->anchorY = args[2].number(); prop->screenPlaced = true; prop->worldSpace = false; }
		else if (set) result = Value(b.number() == 3 ? int32((uint32(prop->anchorX) << 16) | uint16(prop->anchorY)) : b.number() == 2 ? prop->anchorY : prop->anchorX);
		break;
	case 16020:
		if (set) {
			prop->star = b.text(); prop->star.toLowercase(); const Star *star = stars.find(prop->star);
			prop->starPending = !star && prop->star.contains('.');
			if (star) { world(); prop->x = star->x; prop->y = star->y; prop->z = star->z; prop->deg = star->facing & 255; }
		} else result = Value(prop->star);
		break;
	case 16019: if (set) prop->worldSpace = b.truth(); else result = Value(prop->worldSpace ? 1 : 0); break;
	case 16025:
		if (set) { prop->selectView(b.text(), frame.event); }
		else { Common::String view = prop->view.empty() && prop->state() ? prop->state()->name : prop->view; view.toLowercase(); result = Value(view); }
		break;
	case 16026: if (set) prop->speed = b.number(); else result = Value(prop->speed); break;
	case 16027: if (set) { prop->set = b.text(); prop->set.toLowercase(); if (!prop->set.empty()) world(); } else result = Value(prop->set); break;
	case 16028: if (set) prop->scale = b.number(); else result = Value(prop->scale); break;
	case 16032: if (set) prop->owner = b; else result = prop->owner; break;
	case 16042: if (set) prop->value = b; else result = prop->value; break;
	case 16052: if (set) prop->zclip = b.number(); else result = Value(prop->zclip); break;
	}
	return true;
}

}

namespace DreamFactory {
bool Session::stageCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12060: case 12061: case 12062: case 12068: case 12069: case 12070:
	case 16008: case 20042: case 20043: case 20035: case 20036: case 20037: case 20038: case 20044: case 20045: case 20048:
	case 20092: case 20093: case 20094: break;
	default: return false;
	}
	result = Value();
	const bool stageSend = call.opcode == 12070 || call.opcode == 20094;
	const bool flatSend = call.opcode == 12069 || call.opcode == 20093;
	const bool buttonSend = call.opcode == 12068 || call.opcode == 20092;
	if (stageSend || flatSend || buttonSend) {
		const uint32 deferredIndex = stageSend ? 0 : buttonSend ? 2 : 1;
		if (call.args.size() <= deferredIndex) { runtime.fail("Stage send requires a deferred handler"); return true; }
		Values targets;
		for (uint32 i = 0; i < deferredIndex; ++i) { targets.push_back(runtime.evaluate(call.args[i], frame)); if (!runtime.current(frame)) return true; }
		if (call.args[deferredIndex] >= frame.instance->script.expressions.size()) { runtime.fail("Invalid stage deferred handler"); return true; }
		const Expression &deferred = frame.instance->script.expressions[call.args[deferredIndex]];
		if (deferred.kind != Expression::kCall) { runtime.fail("Stage send requires a handler call"); return true; }
		const Values args = runtime.arguments(deferred, frame); if (!runtime.current(frame)) return true;
		if (buttonSend) result = stageSystem.button(targets[0].text(), targets[1].text(), deferred.name, args, frame);
		else result = stageSystem.send(stageSend, stageSend ? Common::String() : targets[0].text(), deferred.name, args, frame);
		return true;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value();
	const auto loaded = stageSystem.current;
	switch (call.opcode) {
	case 12060: {
		Common::String name = a.text(); name.toLowercase();
		if (!name.hasSuffix(".stg") && !name.hasSuffix(".flt")) name += version == 1 ? ".flt" : ".stg";
		if (!loaded || loaded->name != name) _stagePalette.clear();
		result = Value(stageSystem.open(a.text(), &frame) ? 1 : 0); break;
	}
	case 12061: stageSystem.close(&frame); break;
	case 12062: stageSystem.go(a.text(), &frame); break;
	case 16008: result = Value(loaded ? 1 : 0); break;
	case 20042: result = Value(loaded && stageSystem.flat >= 0 ? 1 : 0); break;
	case 20043: {
		const int32 index = stageSystem.resolve(a.text());
		if (index >= 0 && args.size() >= 3) {
			const int16 x = int16(uint32(args[2].number()) >> 16), y = int16(args[2].number());
			const auto &regions = loaded->data.flats[index].regions;
			for (uint32 i = 0; i < regions.size(); ++i) if (regions[i].name.equalsIgnoreCase(b.text())) {
				const Common::Rect &r = regions[i].rect;
				result = Value(x >= r.left && x <= r.right && y >= r.top && y <= r.bottom ? 1 : 0); break;
			}
		}
		break;
	}
	case 20035: result = Value(loaded ? int32(loaded->data.flats.size()) : 0); break;
	case 20036: result = Value(loaded && a.number() > 0 && uint32(a.number()) <= loaded->data.flats.size() ? loaded->data.flats[a.number() - 1].name : Common::String("none")); break;
	case 20037: result = Value(stageSystem.resolve(a.text()) + 1); break;
	case 20038: result = Value(stageSystem.currentFlat()); break;
	case 20048: lastResult = Value(loaded ? loaded->name : Common::String("none")); result = Value(loaded && !loaded->data.name.empty() ? loaded->data.name : lastResult.text()); break;
	case 20044: case 20045: {
		const int32 index = stageSystem.resolve(a.text());
		if (call.opcode == 20044) result = Value(index >= 0 ? int32(loaded->data.flats[index].regions.size()) : 0);
		else result = Value(index >= 0 && b.number() > 0 && uint32(b.number()) <= loaded->data.flats[index].regions.size() ? loaded->data.flats[index].regions[b.number() - 1].name : Common::String());
		break;
	}
	}
	return true;
}
}

namespace DreamFactory {
bool Session::motionCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12006: case 12010: case 12072: case 12100: case 12101: case 16048: case 20009: case 20099: case 20103: case 20106: break;
	default: return false;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value(); result = Value();
	switch (call.opcode) {
	case 12006: {
		const auto actor = castSystem.get(a.text()); if (!actor) break;
		const Star *star = stars.find(b.text());
		if (star && stars.current.version == 1 && !actor->star.empty()) {
			const StarPath *path = stars.path(actor->star, b.text());
			if (path) { Common::String destination = b.text(); destination.toLowercase(); actor->star = "walkonpath"; motion.startPath(a.text(), *path, destination, false); break; }
		}
		if (star) { actor->star = "defer"; Common::String destination = b.text(); destination.toLowercase(); motion.start(a.text(), star->x, star->y, star->z, destination); break; }
		// Dust's animal scripts spell a world-space destination as "X,Y,Z".
		const Common::String point = b.text(); const char *cursor = point.c_str(); int32 xyz[3] = {0, 0, 0}; uint32 count = 0; bool valid = true;
		while (*cursor) {
			while (*cursor == ' ' || *cursor == '\t') ++cursor;
			const char *begin = cursor; if (*cursor == '-') ++cursor;
			if (*cursor < '0' || *cursor > '9') { valid = false; break; }
			while (*cursor >= '0' && *cursor <= '9') ++cursor;
			if (count < 3) xyz[count] = int32(strtol(begin, nullptr, 10)); ++count;
			while (*cursor == ' ' || *cursor == '\t') ++cursor;
			if (!*cursor) break;
			if (*cursor++ != ',' || !*cursor) { valid = false; break; }
		}
		if (valid && count >= 2) { actor->star = "walktoxyz"; motion.start(a.text(), xyz[0], xyz[1], xyz[2]); }
		break;
	}
	case 12010: motion.stop(a.text()); break;
	case 12072: motion.turn(a.text(), b.number()); break;
	case 12100: {
		const auto actor = castSystem.get(a.text()); const Common::String destination = args.size() > 2 ? args[2].text() : Common::String();
		const Star *star = stars.find(destination); if (!actor || !star) break;
		Common::String target = destination; target.toLowercase(); const bool resume = b.text().equalsIgnoreCase("resume");
		if (const StarPath *path = stars.path(b.text(), target)) { actor->star = "walkonpath"; motion.startPath(a.text(), *path, target, resume); }
		else {
			if (!resume) if (const Star *from = stars.find(b.text())) { actor->x = from->x; actor->y = from->y; actor->z = from->z; }
			actor->star = "defer"; motion.start(a.text(), star->x, star->y, star->z, target);
		}
		break;
	}
	case 12101: {
		const auto actor = castSystem.get(a.text());
		if (actor) { actor->star = "walktoxyz"; motion.start(a.text(), b.number(), args.size() > 2 ? args[2].number() : 0, args.size() > 3 ? args[3].number() : 0); }
		break;
	}
	case 16048: motion.pause(a.text(), args.size() < 2 || b.truth()); break;
	case 20009: result = Value(motion.get(a.text()) ? 1 : 0); break;
	case 20099: { const auto walk = motion.get(a.text()); result = Value(walk ? walk->destination : Common::String("None")); break; }
	case 20103: result = Value(a.number() > 0 && uint32(a.number()) <= motion.walks.size() ? motion.walks[a.number() - 1]->actor->name : Common::String()); break;
	case 20106: result = Value(int32(motion.walks.size())); break;
	}
	return true;
}
}

namespace DreamFactory {
void Session::bindRoom(const Room *room) {
	if (room) { _roomPalette.clear(); bindStars(room->stars, room->name); }
	else { stars.current = StarTable(); crickets.setRoom("none"); crickets.clearListener(); }
}
void Session::roomListener(const Scene &scene, const View &view) { crickets.listener(scene.x, scene.y, view.facing); _hintValid = false; }
void Session::presentRoom(const Room &room, const Frame &frame) {
	memcpy(displayPalette, room.palette, sizeof(displayPalette)); _paletteKind = "set";
	if (!_system || frame.pixels.empty()) return;
	if (presentStage(&room, &frame)) return;
	Bytes rgba;
	const byte *palette = _roomPalette.empty() ? room.palette : _roomPalette.data();
	if (!StageView::compose(nullptr, nullptr, roomSystem.visible ? &frame : nullptr, palette, rgba, false)) {
		rgba.resize(512 * 384 * 4); memset(rgba.data(), 0, rgba.size());
		for (uint32 i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
	}
	compositeSprites(rgba, palette, &room, &frame);
	compositeHints(rgba);
	compositeControls(rgba);
	if (!display.rgba(rgba, 512, 384, _paletteBlack, false)) runtime.fail("Room rendering requires an RGB display");
	display.palette(palette, false);
}
bool Session::waitRoomFrame(ScriptFrame *parent) {
	ScriptFrame context = parent ? *parent : runtime.eventFrame("", "");
	sampleTime(); const uint64 deadline = Scheduler::ticks(_time) + MAX<int32>(1, frameRate);
	do { yieldFrame(false); } while (Scheduler::ticks(_time) < deadline && runtime.current(context));
	return runtime.current(context);
}
bool Session::roomCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12032: case 12033: case 12034: case 12066: case 12067: case 16007: case 16011: case 16029: case 16031:
	case 20018: case 20019: case 20020: case 20022: case 20053: case 20063: case 20064: case 20085: case 20090: case 20091: case 20096: case 20116: case 20118: case 20119: break;
	case 20017: case 20021: case 20023: case 20024: case 20100: if (version != 1) return false; break;
	default: return false;
	}
	result = Value();
	const bool sceneSend = call.opcode == 12034 || call.opcode == 20085, setSend = call.opcode == 12067 || call.opcode == 20091, paintSend = call.opcode == 12066 || call.opcode == 20090;
	if (sceneSend || setSend || paintSend) {
		const uint32 index = paintSend ? 3 : setSend && call.args.size() == 1 ? 0 : 1;
		if (call.args.size() <= index) { runtime.fail("Room send requires a deferred handler"); return true; }
		Values targets; for (uint32 i = 0; i < index; ++i) { targets.push_back(runtime.evaluate(call.args[i], frame)); if (!runtime.current(frame)) return true; }
		if (call.args[index] >= frame.instance->script.expressions.size()) { runtime.fail("Invalid room deferred handler"); return true; }
		const Expression &deferred = frame.instance->script.expressions[call.args[index]];
		if (deferred.kind != Expression::kCall) { runtime.fail("Room send requires a handler call"); return true; }
		const Values args = runtime.arguments(deferred, frame); if (!runtime.current(frame)) return true;
		if (paintSend) result = roomSystem.painting(targets[0].text(), targets[1].text(), targets[2].text(), deferred.name, args, frame);
		else result = roomSystem.send(setSend, targets.empty() ? Common::String() : targets[0].text(), deferred.name, args, frame);
		return true;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value(); const auto loaded = roomSystem.current;
	const int32 s = roomSystem.sceneIndex(a.text()), v = roomSystem.viewIndex(s, b.text());
	switch (call.opcode) {
	case 12032: roomSystem.open(a.text(), args.size() > 1 ? b.text() : Common::String(), args.size() > 2 ? args[2].text() : Common::String(), &frame); break;
	case 12033: roomSystem.close(&frame); break;
	case 16007: if (!args.empty()) { roomSystem.visible = a.truth(); presentPuppet(); } else result = Value(loaded && roomSystem.visible ? 1 : 0); break;
	case 16011: if (args.empty()) result = Value(roomSystem.viewName()); else roomSystem.jump("", a.text(), &frame); break;
	case 16029:
		if (args.empty()) result = Value(roomSystem.sceneName());
		else if (a.text().equalsIgnoreCase("right")) roomSystem.move(0, &frame);
		else if (a.text().equalsIgnoreCase("left")) roomSystem.move(1, &frame);
		else if (a.text().equalsIgnoreCase("strait")) roomSystem.move(2, &frame);
		else roomSystem.jump(a.text(), "", &frame);
		break;
	case 16031: result = Value(loaded && roomSystem.scene >= 0 && roomSystem.view >= 0 ? loaded->data.scenes[roomSystem.scene].views[roomSystem.view].facing & 255 : -1); break;
	case 20053: result = Value(roomSystem.name()); break;
	case 20017: result = Value(loaded ? loaded->data.width : 0); break;
	case 20023: case 20024: result = Value(s >= 0 ? (call.opcode == 20023 ? loaded->data.scenes[s].y : loaded->data.scenes[s].x) / 256 : call.opcode == 20023 ? (castSystem.get(a.text()) ? 1 : 0) : (propSystem.get(a.text()) ? 1 : 0)); break;
	case 20019: result = Value(loaded ? int32(loaded->data.scenes.size()) : 0); break;
	case 20020: result = Value(loaded && a.number() > 0 && uint32(a.number()) <= loaded->data.scenes.size() ? loaded->data.scenes[a.number() - 1].name : Common::String()); break;
	case 20118: result = Value(s >= 0 ? int32(loaded->data.scenes[s].views.size()) : 0); break;
	case 20119: result = Value(s >= 0 && b.number() > 0 && uint32(b.number()) <= loaded->data.scenes[s].views.size() ? loaded->data.scenes[s].views[b.number() - 1].name : Common::String()); break;
	case 20018: result = Value(version == 1 ? loaded ? loaded->data.height : 0 : v >= 0 ? int32(loaded->data.scenes[s].views[v].paintings.size()) : 0); break;
	case 20022: { const int32 i = args.size() > 2 ? args[2].number() : 0; result = Value(v >= 0 && i > 0 && uint32(i) <= loaded->data.scenes[s].views[v].paintings.size() ? loaded->data.scenes[s].views[v].paintings[i - 1].name : Common::String()); break; }
	case 20116: result = Value(roomSystem.roadAhead(s, v) ? 1 : 0); break;
	case 20063: case 20064: case 20096: {
		if (version == 1 && call.opcode == 20063) {
			// DF.EXE 0x4078c0 reads the actual camera, including setback
			// and eye height; 0x4079a0 reads the player at ground level.
			WorldCamera camera; if (!roomSystem.worldCamera(camera, 0)) break;
			const int32 axis = args.empty() ? 1 : a.number();
			result = Value(axis == 1 ? int16(camera.x) : axis == 2 ? int16(camera.y) : axis == 3 ? int16(camera.z) : axis == 4 ? int32(uint32(uint16(camera.x)) << 16 | uint16(camera.y)) : 0);
			break;
		}
		const int32 index = call.opcode == 20096 ? s : roomSystem.scene;
		if (!loaded || index < 0) break; const Scene &sc = loaded->data.scenes[index];
		const int32 axis = call.opcode == 20096 ? (args.size() > 1 ? b.number() : 1) : (args.empty() ? 1 : a.number());
		result = Value(axis == 1 ? sc.x : axis == 2 ? sc.y : axis == 3 && call.opcode == 20096 ? sc.z : axis == 4 ? int32(uint32(sc.x) << 16 | uint16(sc.y)) : 0); break;
	}
	case 20100: result = Value(s >= 0 && loaded->data.scenes[s].build ? 1 : 0); break;
	case 20021:
		result = Value(Common::String("none"));
		if (loaded) for (uint32 i = 0; i < loaded->data.scenes.size(); ++i) if (loaded->data.scenes[i].y / 256 == a.number() && loaded->data.scenes[i].x / 256 == b.number()) { result = Value(loaded->data.scenes[i].name); break; }
		break;
	}
	return true;
}
void Session::bindStars(const StarTable &table, const Common::String &set) {
	stars.bind(table); crickets.setRoom(set);
	for (uint32 i = 0; i < castSystem.actors.size(); ++i) {
		const auto actor = castSystem.actors[i];
		if (!actor->starPending || !actor->set.equalsIgnoreCase(set) || motion.get(actor->name)) continue;
		const Star *star = table.find(actor->star); if (!star) continue;
		actor->x = star->x; actor->y = star->y; actor->z = star->z; actor->worldSpace = true; actor->starPending = false;
	}
	for (uint32 i = 0; i < propSystem.props.size(); ++i) {
		const auto prop = propSystem.props[i]; if (!prop->starPending || !prop->set.equalsIgnoreCase(set)) continue;
		const Star *star = table.find(prop->star); if (!star) continue;
		prop->x = star->x; prop->y = star->y; prop->z = star->z; prop->becomeWorld(); prop->starPending = false;
		if (prop->shop->data.version == 1 && !prop->scale) prop->scale = 1000;
	}
}
}

namespace DreamFactory {
bool Session::cricketCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12007: case 12012: case 16047: case 20011: case 20101: case 20104: break;
	default: return false;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value(); result = Value();
	if (version == 1) {
		switch (call.opcode) {
		case 12007:
			if (args.size() != 7) runtime.fail("makeball requires a prop, three velocities, gravity, bounce and frames");
			else if (!balls.make(a.text(), b.number(), args[2].number(), args[3].number(), args[4].number(), args[5].number(), args[6].number()) && runtime.error.empty()) warning("DreamFactory ball table full; dropping %s", a.text().c_str());
			break;
		case 12012: balls.stop(args.empty() ? Common::String("all") : a.text()); break;
		case 16047: balls.pause(args.empty() ? Common::String("all") : a.text(), args.size() < 2 || b.truth()); break;
		case 20011: result = Value(balls.has(a.text()) ? 1 : 0); break;
		case 20101: result = Value(balls.index(a.number())); break;
		case 20104: result = Value(balls.count()); break;
		}
		return true;
	}
	switch (call.opcode) {
	case 12007:
		if (!crickets.make(a.text(), b.number(), args.size() > 2 ? args[2].number() : 0, args.size() > 3 ? args[3].number() : 1, args.size() > 4 ? args[4].number() : 0, args.size() > 5 ? args[5].number() : -1)) warning("DreamFactory cricket table full; dropping %s", a.text().c_str());
		break;
	case 12012: crickets.stop(args.empty() ? Common::String("all") : a.text()); break;
	case 16047: crickets.pause(args.empty() ? Common::String("all") : a.text(), args.size() < 2 || b.truth()); break;
	case 20011: result = Value(crickets.has(a.text()) ? 1 : 0); break;
	case 20101: result = Value(crickets.index(a.number())); break;
	case 20104: result = Value(int32(crickets.count())); break;
	}
	return true;
}
}

namespace DreamFactory {
void Session::compositeSprites(Bytes &rgba, const byte *palette, const Room *room, const Frame *frame) {
	WorldCamera camera;
	if (room && frame && roomSystem.visible && roomSystem.current && room == &roomSystem.current->data && roomSystem.worldCamera(camera, cameraHi)) {
		Occlusion occlusion; occlusion.frame = frame;
		occlusion.scale = double(room->depthFar) / (room->depthLevels ? room->depthLevels : 24);
		occlusion.bias = room->version == 1 ? 128 - room->cameraSetback : 0;
		const auto actors = castSystem.drawList(camera, room->name);
		const auto props = propSystem.worldDrawList(camera, room->name);
		uint32 a = 0, p = 0;
		while (a < actors.size() || p < props.size()) {
			// Dust has one depth-sorted sprite pass. Titanic keeps actors
			// before props, matching the current v4 reference renderer.
			if (a < actors.size() && (room->version != 1 || p == props.size() || actors[a].point.depth >= props[p].point.depth)) {
				if (!castSystem.compositeOne(actors[a++], rgba, 512, 384, palette, camera, occlusion)) runtime.fail("Invalid actor sprite");
			} else if (!propSystem.compositeWorldOne(props[p++], rgba, 512, 384, palette, camera, occlusion)) runtime.fail("Invalid world prop sprite");
		}
	}
	if (!castSystem.compositeScreen(rgba, 512, 384, palette, roomSystem.current ? roomSystem.current->data.name : Common::String())) runtime.fail("Invalid screen actor sprite");
	if (!propSystem.composite(rgba, 512, 384, palette)) runtime.fail("Invalid prop sprite");
	compositeText(rgba, palette);
}
bool Session::presentStage(const Room *room, const Frame *frame) {
	const auto stage = stageSystem.current;
	if (!stage || stageSystem.flat < 0) return false;
	const Frame *flat = _stageView.frame(stage, stageSystem.flat);
	if (!flat) {
		if (stage->data.flats[stageSystem.flat].image) runtime.fail("Invalid stage picture: " + stageSystem.currentFlat());
		return false;
	}
	Bytes rgba;
	const byte *stagePalette = _stagePalette.empty() ? stage->data.palette : _stagePalette.data();
	const byte *roomPalette = room ? (_roomPalette.empty() ? room->palette : _roomPalette.data()) : nullptr;
	if (StageView::compose(flat, stagePalette, roomSystem.visible ? frame : nullptr, roomPalette, rgba)) {
		byte palette[768]; memcpy(palette, stagePalette, 768);
		if (room && frame && roomSystem.visible) memcpy(palette, roomPalette, 128 * 3);
		compositeSprites(rgba, palette, room, frame);
		compositeHints(rgba);
		compositeControls(rgba);
		bool staticArt = (!room || !roomSystem.visible) && textOverlay.empty() && !StageView::matte(*flat);
		for (const auto &prop : propSystem.props) if (prop->visible && !prop->hidden && !prop->worldSpace) staticArt = false;
		for (const auto &actor : castSystem.actors) if (actor->visible && !actor->worldSpace) staticArt = false;
		Bytes screen;
		if (!Display::scaleRGBA(rgba, 512, 384, screen)) { runtime.fail("Invalid stage canvas"); return true; }
		if (staticArt && ConfMan.hasKey("dreamfactory_artwork") && ConfMan.getBool("dreamfactory_artwork"))
			_artPack.artwork(stage->source, stageSystem.currentFlat(), flat->pixels, screen);
		if (!display.rgba(screen, 640, 480, _paletteBlack, false)) runtime.fail("Stage rendering requires an RGB display");
		display.palette(stagePalette, false); memcpy(displayPalette, stage->data.palette, 768); _paletteKind = "stage";
	}
	return true;
}
void Session::compositeHints(Bytes &rgba) {
	if (!movies.hotspotHints() || !ConfMan.hasKey("dreamfactory_hotspot_hints") || !ConfMan.getBool("dreamfactory_hotspot_hints") ||
		puppetSystem.current || movies.active() || roomSystem.moving || roomSystem.turning || rgba.size() != 512 * 384 * 4) return;
	if (!_hintValid || _time - _hintTime >= 250) {
		_hintValid = true; _hintTime = _time; _hintPoints.clear(); Common::HashMap<Common::String, Common::Point> found;
		// Sample the actual hit tester, including depth occlusion. Markers
		// indicate targets, not whether their scripts currently perform an action.
		for (int32 y = 4; y < 384; y += 8) for (int32 x = 4; x < 512; x += 8) {
			const Hit hit = hitTest(x, y);
			if (hit.type != "button" && hit.type != "painting" && hit.type != "prop" && hit.type != "actor") continue;
			const Common::String id = hit.type + ":" + hit.name;
			if (!found.contains(id)) found[id] = Common::Point(x, y);
		}
		for (const auto &entry : found) _hintPoints.push_back(entry._value);
	}
	for (const auto &point : _hintPoints) for (int32 dy = -3; dy <= 3; ++dy) for (int32 dx = -3; dx <= 3; ++dx) {
		if (ABS(dx) != 3 && ABS(dy) != 3) continue;
		const int32 x = point.x + dx, y = point.y + dy;
		if (x < 0 || y < 0 || x >= 512 || y >= 384) continue;
		byte *pixel = rgba.data() + (y * 512 + x) * 4;
		pixel[0] = 255; pixel[1] = 230; pixel[2] = 0; pixel[3] = 255;
	}
}
void Session::presentWorld() {
	if (!_system) return;
	if (presentStage(roomSystem.current ? &roomSystem.current->data : nullptr, roomSystem.current && !roomSystem.frame.pixels.empty() ? &roomSystem.frame : nullptr)) return;
	if (roomSystem.current && !roomSystem.frame.pixels.empty()) presentRoom(roomSystem.current->data, roomSystem.frame);
	else display.restore(_paletteBlack);
}
bool Session::loadFonts() {
	if (!_fontLoaded) {
		Bytes bytes;
		if (ConfMan.hasKey("dreamfactory_font")) {
			Common::File file;
			if (file.open(Common::FSNode(ConfMan.getPath("dreamfactory_font"))) && file.size() > 0 && file.size() <= 16 * 1024 * 1024) { bytes.resize(uint32(file.size())); if (file.read(bytes.data(), bytes.size()) != bytes.size()) bytes.clear(); }
		} else if (!resources.read("arial.ttf", bytes)) {
			Common::File file;
			if (file.open("LiberationSans-Regular.ttf") && file.size() > 0 && file.size() <= 16 * 1024 * 1024) { bytes.resize(uint32(file.size())); if (file.read(bytes.data(), bytes.size()) != bytes.size()) bytes.clear(); }
			if (bytes.empty()) {
				// Stock ScummVM distributions provide this face in fonts.dat.
				// Keep owning the bytes: FreeType may load glyphs lazily.
				Common::ScopedPtr<Common::Archive> fonts(Common::makeZipArchive(Common::Path("fonts.dat")));
				Common::File bundled;
				if (fonts && bundled.open("LiberationSans-Regular.ttf", *fonts) && bundled.size() > 0 && bundled.size() <= 16 * 1024 * 1024) {
					bytes.resize(uint32(bundled.size()));
					if (bundled.read(bytes.data(), bytes.size()) != bytes.size()) bytes.clear();
					else debug(1, "GAME FONT source=fonts.dat face=LiberationSans-Regular.ttf");
				}
			}
		}
		if (!_puppetFonts.load(bytes)) { runtime.fail("Game font unavailable: supply ScummVM fonts.dat, arial.ttf, LiberationSans-Regular.ttf in extrapath, or dreamfactory_font"); return false; }
		_fontLoaded = true;
	}
	return true;
}
bool Session::textCall(const Expression &call, ScriptFrame &frame, Value &result) {
	if (call.opcode != 12080 && call.opcode != 20117) return false;
	const Values args = runtime.arguments(call, frame); result = Value(); if (!runtime.current(frame)) return true;
	const Common::String text = args.empty() ? Common::String() : args[0].text();
	const int32 size = args.size() > (call.opcode == 12080 ? 3u : 2u) ? args[call.opcode == 12080 ? 3 : 2].number() : 12;
	if (!loadFonts()) return true;
	const Graphics::Font *font = _puppetFonts.font(size);
	if (!font) { runtime.fail("Invalid drawstring font size"); return true; }
	if (call.opcode == 20117) { result = Value(font->getStringWidth(Common::U32String(text, Common::kWindows1252))); return true; }
	const uint32 point = args.size() > 1 ? uint32(args[1].number()) : 0;
	TextEntry entry; entry.text = text; entry.x = int16(point >> 16); entry.y = int16(point); entry.size = size;
	entry.color = args.size() > 2 ? args[2].number() : 0;
	for (uint32 i = 0; i < textOverlay.size(); ++i) if (textOverlay[i].x == entry.x && textOverlay[i].y == entry.y && textOverlay[i].size == size) { textOverlay[i] = entry; return true; }
	textOverlay.push_back(entry); return true;
}
void Session::compositeText(Bytes &rgba, const byte *palette) {
	if (textOverlay.empty() || !loadFonts()) return;
	Graphics::Surface surface; surface.init(512, 384, 512 * 4, rgba.data(), Graphics::PixelFormat::createFormatRGBA32());
	for (uint32 i = 0; i < textOverlay.size(); ++i) {
		const TextEntry &entry = textOverlay[i]; const Graphics::Font *font = _puppetFonts.font(entry.size);
		if (!font) { runtime.fail("Invalid drawstring font size"); return; }
		const int32 color = CLIP<int32>(entry.color, 0, 255), ascent = font->getFontAscent() >= 0 ? font->getFontAscent() : font->getFontHeight();
		const uint32 ink = surface.format.RGBToColor(palette[color * 3], palette[color * 3 + 1], palette[color * 3 + 2]);
		font->drawString(&surface, Common::U32String(entry.text, Common::kWindows1252), entry.x, entry.y - ascent, MAX<int32>(0, 512 - entry.x), ink, Graphics::kTextAlignLeft, 0, false, true);
	}
}
void Session::eraseText(const Prop &prop) {
	if (textOverlay.empty()) return;
	const PropState *state = prop.state(); const uint32 index = prop.frameIndex();
	if (!state || index >= state->pictures.size()) return;
	const Sprite *sprite = prop.shop->frame(state->pictures[index].location); if (!sprite) return;
	const int64 left = int64(prop.anchorX) - sprite->x, top = int64(prop.anchorY) - sprite->y;
	for (int32 i = int32(textOverlay.size()) - 1; i >= 0; --i) {
		const TextEntry &entry = textOverlay[i]; const Graphics::Font *font = _puppetFonts.font(entry.size); if (!font) continue;
		const int32 size = entry.size ? ABS(entry.size) : 12;
		const int32 width = font->getStringWidth(Common::U32String(entry.text, Common::kWindows1252));
		if (entry.x < left + sprite->width && left < int64(entry.x) + width && int64(entry.y) - size < top + sprite->height && top < int64(entry.y) + (size + 3) / 4) textOverlay.remove_at(i);
	}
}
void Session::presentPuppet() {
	const auto puppet = puppetSystem.current;
	if (!_system || !runtime.error.empty()) return;
	if (!puppet || !puppet->visible) { presentWorld(); return; }
	if (!loadFonts()) return;
	const int32 originalSize = puppetParams.contains(6) ? puppetParams[6] : 12;
	const bool larger = ConfMan.hasKey("dreamfactory_large_text") && ConfMan.getBool("dreamfactory_large_text");
	// Keep exceptional script font sizes intact. Enlarge the normal dialogue
	// face within the fixed subtitle/choice bands, without changing hit targets.
	const int32 fontSize = larger && originalSize > 0 && originalSize <= 14 ? 14 : originalSize;
	const Graphics::Font *font = _puppetFonts.font(fontSize);
	const Graphics::Font *screenFont = _puppetFonts.font(fontSize, true);
	if (!font || !screenFont) { runtime.fail("Invalid conversation font size"); return; }
	Bytes rgba; rgba.resize(512 * 384 * 4); memset(rgba.data(), 0, rgba.size());
	for (uint32 i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
	const auto stage = stageSystem.current;
	const Frame *flat = _stageView.frame(stage, stageSystem.flat);
	if (flat) {
		const byte *palette = _stagePalette.empty() ? stage->data.palette : _stagePalette.data();
		StageView::compose(flat, palette, nullptr, nullptr, rgba, false);
		if (!castSystem.compositeScreen(rgba, 512, 384, palette, roomSystem.current ? roomSystem.current->data.name : Common::String())) { runtime.fail("Invalid conversation interface actor"); return; }
		if (!propSystem.composite(rgba, 512, 384, palette)) { runtime.fail("Invalid conversation interface prop"); return; }
		compositeText(rgba, palette);
	}
	const bool subtitles = subtitlesEnabled();
	const Frame *backdrop = roomSystem.current ? &roomSystem.frame : nullptr;
	const byte *roomPalette = roomSystem.current ? (_roomPalette.empty() ? roomSystem.current->data.palette : _roomPalette.data()) : nullptr;
	Bytes screen;
	if (!_puppetView.composite(puppet, rgba, backdrop, roomPalette, subtitles, _puppetArtPalette.empty() ? nullptr : _puppetArtPalette.data()) ||
		!Display::scaleRGBA(rgba, 512, 384, screen) ||
		!PuppetView::overlay(*puppet, screen, *font, puppetParams, subtitles, puppetChoices.pressVisible(Scheduler::ticks(_time)), _puppetPalette.empty() ? nullptr : _puppetPalette.data(), screenFont, ConfMan.hasKey("dreamfactory_contrast") && ConfMan.getBool("dreamfactory_contrast"), larger ? _puppetFonts.font(originalSize, true) : nullptr) ||
		!display.rgba(screen, 640, 480, _paletteBlack)) { runtime.fail("Conversation rendering failed or RGB display unavailable"); return; }
	display.palette(puppet->data.palette); memcpy(displayPalette, puppet->data.palette, 768); _paletteKind = "puppet";
}
void Session::speakPuppet(const Common::String &ident, ScriptFrame &frame, bool queue) {
	if (!puppetSpeech.start(puppetSystem.current, ident, 0, queue)) { runtime.fail(puppetSpeech.error); return; }
	if (!puppetSpeech.active()) return;
	if (queue && puppetSystem.current && !puppetSystem.current->subtitle.empty()) {
		const auto puppet = puppetSystem.current;
		JournalEntry entry;
		entry.speaker = puppetSystem.name(); entry.file = resources.sourceName(puppet->file);
		const int32 line = puppet->data.line(ident, true);
		if (line >= 0) entry.ident = puppet->data.lines[line].ident;
		entry.text = puppet->subtitle; entry.time = _time; entry.room = roomSystem.current ? roomSystem.current->data.name : Common::String();
		if (!ConfMan.hasKey("dreamfactory_journal") || ConfMan.getBool("dreamfactory_journal")) journal.add(entry);
		recordRoute(entry, false);
	}
	if (!_system) { puppetSpeech.abandon(); runtime.fail("Puppet speech requires a display/audio host"); return; }
	// Loading/decoding time is outside this line's authored playback clock.
	presentPuppet(); sampleTime(); const uint64 started = _time; bool tapStarted = false;
	while (puppetSpeech.active() && runtime.current(frame)) {
		yieldFrame(false);
		for (uint32 i = 0; i < pendingEvents.size(); ++i) {
			const Common::Event &event = pendingEvents[i];
			const bool enhancedControls = event.customType == kPadInteract || (ConfMan.hasKey("dreamfactory_dialogue_controls") && ConfMan.getBool("dreamfactory_dialogue_controls"));
			if (enhancedControls && event.type == Common::EVENT_LBUTTONDOWN) tapStarted = _time - started >= 200;
			if (enhancedControls && event.type == Common::EVENT_LBUTTONUP && tapStarted) { puppetSpeech.skip(); tapStarted = false; }
			if (event.type != Common::EVENT_KEYDOWN) continue;
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE && !event.kbdRepeat) puppetSpeech.skip(true);
			else if (event.kbd.keycode == Common::KEYCODE_PERIOD && !event.kbdRepeat) puppetSpeech.skip();
			else if (enhancedControls && event.kbd.keycode == Common::KEYCODE_RETURN && !event.kbdRepeat && !(event.kbd.flags & (Common::KBD_CTRL | Common::KBD_ALT | Common::KBD_META))) puppetSpeech.skip();
			else if ((event.kbd.flags & Common::KBD_CTRL) && event.kbd.keycode >= Common::KEYCODE_0 && event.kbd.keycode <= Common::KEYCODE_9)
				sound.setWaveVolume(event.kbd.keycode - Common::KEYCODE_0);
		}
		// The original line filter discards clicks and other non-key events.
		pendingEvents.clear(); puppetSpeech.tick(_time - started);
		if (runtime.current(frame)) presentPuppet();
	}
	if (!runtime.current(frame)) puppetSpeech.abandon();
	else presentPuppet();
}
void Session::repeatPuppet(ScriptFrame &frame) {
	const auto puppet = puppetSystem.current; if (!puppet) return;
	const auto shown = puppet->choices; const int32 chosen = puppet->chosen;
	const auto queue = puppet->voiceQueue;
	puppet->choices = puppet->lastChoices; puppet->chosen = puppet->lastChosen;
	if (puppet->lastChosen >= 0 && uint32(puppet->lastChosen) < puppet->lastChoices.size()) {
		const int32 line = puppet->data.textLine(puppet->lastChoices[puppet->lastChosen].text);
		if (line >= 0) speakPuppet(puppet->data.lines[line].ident, frame, false);
	}
	for (uint32 i = 0; i < queue.size() && runtime.current(frame) && puppetSystem.current == puppet; ++i) {
		if (queue[i] >= puppet->data.lines.size()) break;
		speakPuppet(puppet->data.lines[queue[i]].ident, frame, false);
		if (puppet->interrupted) break;
	}
	if (puppetSystem.current == puppet) { puppet->interrupted = false; puppet->choices = shown; puppet->chosen = chosen; }
}
int32 Session::waitPuppet(ScriptFrame &frame) {
	const auto puppet = puppetSystem.current; puppetChoices.start(puppet);
	if (!puppetChoices.active()) return -1;
	if (!_system) { puppetChoices.escape(); runtime.fail("Puppet choices require a display/input host"); return -1; }
	// Scripts can leave the busy cursor set while walking up to a character.
	// The modal answer band accepts input independently of the world handlers.
	cursors.set("arrow");
	sampleTime(); uint64 due[4] = {}; bool enabled[4] = {};
	auto arm = [&](uint32 slot) {
		const int32 lo = puppet->data.idleMin[slot], hi = puppet->data.idleMax[slot];
		const uint32 range = uint32(int64(hi) - lo);
		// Original timers share the script RNG; retain that draw sequence.
		due[slot] = Scheduler::ticks(_time) + lo + (range ? _random.getRandomNumber(range - 1) + 1 : 0);
	};
	if (puppetParams.contains(8) && puppetParams[8]) for (uint32 i = 0; i < 4; ++i) {
		enabled[i] = puppet->data.idleMin[i] > 0 && puppet->data.idleMax[i] >= puppet->data.idleMin[i] && puppet->data.line(Common::String::format("idle %u", i + 1)) >= 0;
		if (enabled[i]) arm(i);
	}
	while (puppetChoices.active() && runtime.current(frame) && puppetSystem.current == puppet) {
		yieldFrame(false);
		const Common::Array<Common::Event> events = pendingEvents; pendingEvents.clear();
		for (uint32 i = 0; i < events.size() && puppetChoices.active() && runtime.current(frame); ++i) {
			const Common::Event &event = events[i];
			if (event.type == Common::EVENT_KEYDOWN) {
				if (event.kbd.keycode == Common::KEYCODE_ESCAPE && !event.kbdRepeat) puppetChoices.escape();
				else if ((event.kbd.flags & Common::KBD_CTRL) && event.kbd.keycode >= Common::KEYCODE_0 && event.kbd.keycode <= Common::KEYCODE_9) sound.setWaveVolume(event.kbd.keycode - Common::KEYCODE_0);
				else if (((event.customType >= kPadUp && event.customType <= kPadRepeat) || (ConfMan.hasKey("dreamfactory_dialogue_controls") && ConfMan.getBool("dreamfactory_dialogue_controls"))) && !(event.kbd.flags & (Common::KBD_CTRL | Common::KBD_ALT | Common::KBD_META))) {
					if (event.kbd.keycode == Common::KEYCODE_UP) puppetChoices.focus(-1);
					else if (event.kbd.keycode == Common::KEYCODE_DOWN) puppetChoices.focus(1);
					else if (event.kbd.keycode == Common::KEYCODE_RETURN && !event.kbdRepeat) {
						if (event.customType == kPadInteract && !puppetChoices.hasFocus()) {
							const int32 row = PuppetView::choiceAt(pointer.x, pointer.y, puppet->choices.size());
							if (row >= 0) puppetChoices.confirm(Scheduler::ticks(_time), row);
						} else puppetChoices.confirm(Scheduler::ticks(_time));
					} else if (event.kbd.keycode >= Common::KEYCODE_1 && event.kbd.keycode <= Common::KEYCODE_5 && !event.kbdRepeat) puppetChoices.confirm(Scheduler::ticks(_time), event.kbd.keycode - Common::KEYCODE_1);
					else if (event.kbd.keycode == Common::KEYCODE_F7 && !event.kbdRepeat) repeatPuppet(frame);
					if ((event.customType == kPadPrevious || event.customType == kPadNext) && puppetChoices.hasFocus()) {
						pointer = Common::Point(16, 276 + puppet->chosen * 24);
						_system->warpMouse(20, 345 + puppet->chosen * 30);
					}
				}
			} else {
				const bool inside = event.mouse.x >= 0 && event.mouse.x < 640 && event.mouse.y >= 0 && event.mouse.y < 480;
				const int32 x = inside ? event.mouse.x * 512 / 640 : -1, y = inside ? event.mouse.y * 384 / 480 : -1;
				const int32 row = PuppetView::choiceAt(x, y, puppet->choices.size());
				if (event.type == Common::EVENT_LBUTTONDOWN) {
					if (x >= 0 && x < 512 && y >= 0 && y < 264) repeatPuppet(frame);
					else puppetChoices.down(row, Scheduler::ticks(_time));
				} else if (event.type == Common::EVENT_MOUSEMOVE) puppetChoices.move(row);
				else if (event.type == Common::EVENT_LBUTTONUP) puppetChoices.up(row);
			}
		}
		puppetChoices.tick(Scheduler::ticks(_time));
		if (runtime.current(frame) && puppetSystem.current == puppet) presentPuppet();
		if (!puppetChoices.active() || !runtime.current(frame)) break;
		if (puppet->pressed < 0) for (uint32 i = 0; i < 4; ++i) if (enabled[i] && Scheduler::ticks(_time) >= due[i]) {
			arm(i); speakPuppet(Common::String::format("idle %u", i + 1), frame, false);
			if (puppet->interrupted) puppetChoices.escape();
			break;
		}
	}
	const int32 answer = puppetChoices.answer();
	if (runtime.current(frame) && puppetSystem.current == puppet) {
		JournalEntry entry;
		entry.speaker = puppetSystem.name(); entry.file = resources.sourceName(puppet->file);
		entry.room = roomSystem.current ? roomSystem.current->data.name : Common::String(); entry.time = _time;
		for (uint32 i = 0; i < puppet->choices.size(); ++i) {
			entry.choices.push_back(puppet->choices[i].text);
			if (answer >= 0 && int32(i) == puppet->lastChosen) {
				entry.selected = i; entry.text = puppet->choices[i].text;
				const int32 line = puppet->data.textLine(entry.text);
				if (line >= 0) entry.ident = puppet->data.lines[line].ident;
			}
		}
		if (!ConfMan.hasKey("dreamfactory_journal") || ConfMan.getBool("dreamfactory_journal")) journal.add(entry);
		recordRoute(entry, true);
	}
	if (puppetChoices.active()) puppetChoices.escape();
	return answer;
}
bool Session::puppetCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12018: case 12041: case 12042: case 12043: case 12044: case 12045: case 12083:
	case 16050: case 16053: case 20046: case 20047: case 20049: case 20086: case 20028: case 20107: break;
	default: return false;
	}
	result = Value();
	if (call.opcode == 12045 || call.opcode == 20086) {
		if (call.args.size() < 2) { runtime.fail("sendtopuppet requires a script and deferred handler"); return true; }
		const Value target = runtime.evaluate(call.args[0], frame); if (!runtime.current(frame)) return true;
		if (call.args[1] >= frame.instance->script.expressions.size()) { runtime.fail("Invalid puppet deferred handler"); return true; }
		const Expression &deferred = frame.instance->script.expressions[call.args[1]];
		if (deferred.kind != Expression::kCall) { runtime.fail("sendtopuppet requires a handler call"); return true; }
		const Values args = runtime.arguments(deferred, frame);
		if (runtime.current(frame)) result = puppetSystem.send(target.text(), deferred.name, args, frame);
		return true;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(), b = args.size() > 1 ? args[1] : Value();
	const auto puppet = puppetSystem.current;
	switch (call.opcode) {
	case 12018: { const bool opened = puppetSystem.open(a.text()); if (opened) { puppetChoices.escape(); puppetSpeech.abandon(); sound.haltVoice(); _puppetPalette.clear(); _puppetArtPalette.clear(); presentPuppet(); } result = Value(opened ? 1 : 0); break; }
	case 12042: puppetChoices.escape(); puppetSpeech.abandon(); sound.haltVoice(); puppetSystem.reset(); _puppetView.reset(); presentWorld(); break;
	case 12043: speakPuppet(a.text(), frame); break;
	case 20028: result = Value(waitPuppet(frame)); break;
	case 12041: puppetSystem.clear(); presentPuppet(); break;
	case 12044: puppetSystem.bevel(a.text(), b.number()); break;
	case 20107: result = Value(puppet ? int32(puppet->choices.size()) : 0); break;
	case 12083:
		if (puppet && puppet->choices.size() >= 2) {
			const uint32 count = puppet->choices.size();
			for (uint32 i = 0; i < count * 5; ++i) { const uint32 x = _random.getRandomNumber(count - 1), y = _random.getRandomNumber(count - 1); SWAP(puppet->choices[x], puppet->choices[y]); }
		}
		break;
	case 16050: if (puppet) { if (args.empty()) result = Value(puppet->visible ? 1 : 0); else { puppet->visible = a.truth(); if (puppet->visible) presentPuppet(); else presentWorld(); } } break;
	case 16053: puppetSystem.base(a.text()); presentPuppet(); break;
	case 20046: result = Value(puppet ? int32(puppet->scripts.size()) : 0); break;
	case 20047: result = Value(puppet && a.number() > 0 && uint32(a.number()) <= puppet->scripts.size() ? puppet->scripts[a.number() - 1]->name : Common::String()); break;
	case 20049: result = Value(puppetSystem.name()); break;
	}
	return true;
}
}

namespace DreamFactory {
bool Session::touchControlsVisible() const {
	return ConfMan.hasKey("dreamfactory_touch_controls") && ConfMan.getBool("dreamfactory_touch_controls") && roomSystem.current && roomSystem.visible && !roomSystem.moving && !roomSystem.turning && !puppetSystem.current && !movies.active() && !_dialogActive && !_pollingButton && !_paletteBlack && !screenBlack;
}
void Session::updateHoverCursor() {
	if (movies.active()) return;
	Common::String cursor;
	if (puppetChoices.active() && puppetSystem.current) {
		if (PuppetView::choiceAt(pointer.x, pointer.y, puppetSystem.current->choices.size()) >= 0) cursor = "hand";
		cursors.hover(cursor); return;
	}
	if (!_dialogActive && !roomSystem.moving && !roomSystem.turning && !puppetSpeech.active() && !puppetChoices.active() && !screenBlack && !_paletteBlack) {
		const Hit hit = hitTest(pointer.x, pointer.y);
		if (hit.type == "actor" || hit.type == "prop" || hit.type == "painting" || hit.type == "button") cursor = "hand";
	}
	cursors.hover(cursor);
}
void Session::compositeControls(Bytes &rgba) {
	if (touchControlsVisible())
		drawTouchPad(rgba);
	if (!_inventoryOfferItem.empty() && !puppetSystem.current && !movies.active() && rgba.size() == 512 * 384 * 4 && loadFonts()) {
		Graphics::Surface surface; surface.init(512, 384, 512 * 4, rgba.data(), Graphics::PixelFormat::createFormatRGBA32());
		const Graphics::Font *font = _puppetFonts.font(14);
		if (font) {
			surface.fillRect(Common::Rect(0, 264, 512, 286), surface.format.RGBToColor(0, 0, 0));
			Common::U32String text = _("Use/Give: "); text += Common::U32String(_inventoryOfferLabel, Common::kWindows1252); text += _("  A/Click: target  B/Esc: cancel");
			font->drawString(&surface, text, 6, 266, 428, surface.format.RGBToColor(255, 255, 255));
			surface.fillRect(Common::Rect(440, 264, 512, 286), surface.format.RGBToColor(49, 62, 74));
			font->drawString(&surface, _("Cancel"), 442, 266, 68, surface.format.RGBToColor(255, 255, 255), Graphics::kTextAlignCenter);
		}
	}
	if (_controlFocused && !roomSystem.moving) {
		const Hit hit = hitTest(_controlPoint.x, _controlPoint.y);
		if (hit.type != _controlHit.type || hit.name != _controlHit.name)
			_controlFocused = false;
	}
}
bool Session::focusControl(int32 direction) {
	if (roomSystem.moving || puppetSpeech.active() || puppetChoices.active() || _dialogActive || _pollingButton)
		return false;
	struct Target {
		Hit hit;
		Common::Point point;
		int32 x = 0, y = 0, count = 0;
	};
	Common::Array<Target> targets;
	Common::HashMap<Common::String, uint32> indices;
	for (int32 y = 1; y < 384; y += 2)
		for (int32 x = 1; x < 512; x += 2) {
			const Hit hit = hitTest(x, y);
			if (hit.type != "actor" && hit.type != "prop" && hit.type != "painting" && hit.type != "button")
				continue;
			const Common::String id = hit.type + ":" + hit.name;
			if (!indices.contains(id)) {
				Target t;
				t.hit = hit;
				t.point = Common::Point(x, y);
				indices[id] = targets.size();
				targets.push_back(t);
			}
			Target &t = targets[indices[id]];
			t.x += x;
			t.y += y;
			++t.count;
		}
	if (targets.empty()) {
		_controlFocused = false;
		return false;
	}
	int32 selected = -1;
	for (uint32 i = 0; i < targets.size(); ++i)
		if (_controlFocused && targets[i].hit.type == _controlHit.type && targets[i].hit.name == _controlHit.name)
			selected = i;
	if (selected < 0)
		selected = direction < 0 ? targets.size() - 1 : 0;
	else
		selected = (selected + direction + int32(targets.size())) % targets.size();
	Target &t = targets[selected];
	const Common::Point center(t.x / t.count, t.y / t.count);
	const Hit hit = hitTest(center.x, center.y);
	_controlPoint = hit.type == t.hit.type && hit.name == t.hit.name ? center : t.point;
	_controlHit = t.hit;
	_controlFocused = true;
	pointer = _controlPoint;
	updateHoverCursor();
	if (_system)
		_system->warpMouse((_controlPoint.x * 640 + 511) / 512, (_controlPoint.y * 480 + 383) / 384);
	presentWorld();
	return true;
}
bool Session::controlEvent(const Common::Event &event) {
	if (event.type == Common::EVENT_LBUTTONUP && _touchHeld) {
		_touchHeld = false;
		return true;
	}
	if (event.type == Common::EVENT_LBUTTONDOWN && touchControlsVisible()) {
		const int32 action = touchDirection(event.mouse.x * 512 / 640, event.mouse.y * 384 / 480);
		if (action >= 0) {
			_touchHeld = true;
			_controlFocused = false;
			Common::Event key;
			key.type = Common::EVENT_KEYDOWN;
			key.kbd.keycode = controlKey(action);
			postEvent(key);
			return true;
		}
	}
	if (event.type == Common::EVENT_MOUSEMOVE && _controlFocused && (event.mouse.x * 512 / 640 != _controlPoint.x || event.mouse.y * 384 / 480 != _controlPoint.y))
		_controlFocused = false;
	if (event.type != Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.type != Common::EVENT_CUSTOM_ENGINE_ACTION_END)
		return false;
	const uint32 action = event.customType;
	if (action < kPadUp || action > kPadClick)
		return false;
	const bool down = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
	if ((action == kPadInteract || action == kPadClick) && !down && _controlHeld) {
		_controlHeld = false;
		Common::Event mouse;
		mouse.type = Common::EVENT_LBUTTONUP;
		mouse.mouse = _controlPress;
		postEvent(mouse);
		return true;
	}
	if (action == kPadClick && puppetSpeech.active()) {
		if (down) {
			Common::Event key;
			key.type = Common::EVENT_KEYDOWN;
			key.kbd.keycode = Common::KEYCODE_PERIOD;
			postEvent(key);
		}
		return true;
	}
	if (!down)
		return true;
	if (action != kPadClick && (puppetChoices.active() || puppetSpeech.active() || action == kPadRepeat)) {
		Common::Event key;
		key.type = Common::EVENT_KEYDOWN;
		key.customType = action;
		key.kbd.keycode = action == kPadRepeat ? Common::KEYCODE_F7 : action == kPadPrevious ? Common::KEYCODE_UP
																  : action == kPadNext       ? Common::KEYCODE_DOWN
																							 : controlKey(action);
		postEvent(key);
		return true;
	}
	if (action == kPadPrevious || action == kPadNext) {
		focusControl(action == kPadPrevious ? -1 : 1);
		return true;
	}
	if (action <= kPadRight) {
		if (!roomSystem.current || !roomSystem.visible) {
			focusControl(action == kPadUp || action == kPadLeft ? -1 : 1);
			return true;
		}
		_controlFocused = false;
		Common::Event key;
		key.type = Common::EVENT_KEYDOWN;
		key.customType = action;
		key.kbd.keycode = controlKey(action);
		postEvent(key);
		return true;
	}
	if (_controlFocused && action != kPadClick) {
		const Hit hit = hitTest(_controlPoint.x, _controlPoint.y);
		if (hit.type != _controlHit.type || hit.name != _controlHit.name) {
			_controlFocused = false;
			return true;
		}
		pointer = _controlPoint;
	}
	_controlPress = Common::Point((pointer.x * 640 + 511) / 512, (pointer.y * 480 + 383) / 384);
	_controlHeld = true;
	Common::Event mouse;
	mouse.type = Common::EVENT_LBUTTONDOWN;
	mouse.mouse = _controlPress;
	mouse.customType = action;
	postEvent(mouse);
	return true;
}
void Session::postEvent(const Common::Event &event) {
    if (controlEvent(event)) return;
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == kOpenRecovery) {
		if (!movies.active() && !puppetSystem.current && !runtime.busy()) _recoveryRequested = true;
		return;
	}
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == kOpenInventory) {
		if (!movies.active() && (!puppetSystem.current || inventoryActive(*this))) _inventoryRequested = true;
		return;
	}
	if (!_inventoryOfferItem.empty() && !puppetSystem.current && event.type == Common::EVENT_KEYDOWN && (event.kbd.keycode == Common::KEYCODE_ESCAPE || event.kbd.keycode == Common::KEYCODE_PERIOD)) {
		_inventoryOfferItem.clear(); return;
	}
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == kOpenJournal) {
		if (!movies.active()) _journalRequested = true;
		return;
	}
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == 1) {
		if (ConfMan.hasKey("dreamfactory_hotspot_hints") && ConfMan.getBool("dreamfactory_hotspot_hints")) {
			movies.setHotspotHints(!movies.hotspotHints()); _hintValid = false; _hintPoints.clear(); _hintTime = 0;
		}
		return;
	}
	if (event.type == Common::EVENT_QUIT || event.type == Common::EVENT_RETURN_TO_LAUNCHER) {
		_stopped = true; sound.haltSounds(); sound.haltVoice(); sound.haltTheme(); runtime.abandon(); return;
	}
	if (event.type == Common::EVENT_MOUSEMOVE || event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_RBUTTONDOWN || event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_RBUTTONUP) {
		pointer.x = int16(floor(double(event.mouse.x) * 512 / 640)); pointer.y = int16(floor(double(event.mouse.y) * 384 / 480));
		if (event.type == Common::EVENT_LBUTTONDOWN) _pointerButtons |= 1;
		if (event.type == Common::EVENT_RBUTTONDOWN) _pointerButtons |= 2;
		if (event.type == Common::EVENT_LBUTTONUP) _pointerButtons &= ~1;
		if (event.type == Common::EVENT_RBUTTONUP) _pointerButtons &= ~2;
		pointerDown = _pointerButtons != 0;
	}
	// Window/display notifications are not game input. Keeping them in the
	// bounded gesture queue can replace a click with its release after boot.
	if (event.type != Common::EVENT_KEYDOWN && event.type != Common::EVENT_MOUSEMOVE && event.type != Common::EVENT_LBUTTONDOWN && event.type != Common::EVENT_RBUTTONDOWN && event.type != Common::EVENT_LBUTTONUP && event.type != Common::EVENT_RBUTTONUP) return;
	if (event.type == Common::EVENT_MOUSEMOVE && !pendingEvents.empty() && pendingEvents.back().type == Common::EVENT_MOUSEMOVE) pendingEvents.pop_back();
	if (event.type == Common::EVENT_KEYDOWN && event.kbdRepeat) {
		for (uint32 i = pendingEvents.size(); i > 0; --i) if (pendingEvents[i - 1].type == Common::EVENT_KEYDOWN && pendingEvents[i - 1].kbdRepeat && pendingEvents[i - 1].kbd.keycode == event.kbd.keycode) pendingEvents.remove_at(i - 1);
	}
	if (pendingEvents.size() >= 32) {
		uint32 drop = pendingEvents.size() - 1;
		for (uint32 i = 0; i < pendingEvents.size(); ++i) if (pendingEvents[i].type == Common::EVENT_KEYDOWN && pendingEvents[i].kbdRepeat) { drop = i; break; }
		pendingEvents.remove_at(drop);
	}
	pendingEvents.push_back(event);
}
void Session::pollInput() {
	if (!_system) return;
	Common::Event event; while (_system->getEventManager()->pollEvent(event)) postEvent(event);
	serviceInventoryClicks();
	if (runtime.isRunning("handleselect")) serviceInventory();
	if (_journalRequested && !_dialogActive && !_stopped) {
		_journalRequested = false;
		pendingEvents.clear();
		const uint32 started = _system->getMillis();
		PauseToken journalPause = g_engine ? g_engine->pauseEngine() : PauseToken();
		_dialogActive = true;
		const uint32 revisit = openJournal(*this);
		_dialogActive = false;
		pointerDown = false; _pointerButtons = 0; _controlHeld = _touchHeld = false;
		if (revisit && !_system->getEventManager()->shouldQuit() && !_system->getEventManager()->shouldReturnToLauncher()) {
			Common::String error;
			if (!restoreRouteScene(revisit, error)) {
				PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken();
				_dialogActive = true;
				journalNotice(error);
				_dialogActive = false;
			}
		}
		excludeWallTime(started);
		if (_system->getEventManager()->shouldQuit() || _system->getEventManager()->shouldReturnToLauncher()) {
			_stopped = true; runtime.abandon();
		}
	}
}
bool Session::pointInRoom(int32 x, int32 y) const {
	return roomSystem.current && roomSystem.visible && x >= 0 && y >= 0 && x < roomSystem.frame.width && y < roomSystem.frame.height && !roomSystem.frame.pixels.empty();
}
Session::Hit Session::hitTest(int32 x, int32 y) {
	Hit hit;
	if (x < 0 || y < 0 || x >= 512 || y >= 384) return hit;
	const auto screenProp = propSystem.hitTest(x, y);
	if (screenProp) { hit.name = screenProp->name; hit.type = "prop"; return hit; }
	const auto room = roomSystem.current; WorldCamera camera;
	if (room && roomSystem.visible && roomSystem.worldCamera(camera, cameraHi)) {
		Occlusion occ; occ.frame = &roomSystem.frame; occ.scale = double(room->data.depthFar) / (room->data.depthLevels ? room->data.depthLevels : 24); occ.bias = room->data.version == 1 ? 128 - room->data.cameraSetback : 0;
		const auto prop = propSystem.hitWorld(x, y, camera, room->data.name, occ);
		const auto actor = (room->data.version == 1 || pointInRoom(x, y)) ? castSystem.hitTest(x, y, camera, room->data.name, occ) : Common::SharedPtr<Actor>();
		bool actorFront = actor && !prop;
		if (actor && prop && room->data.version == 1) {
			ProjectedPoint ap, pp;
			actorFront = !actor->worldSpace || (projectPoint(camera, actor->x, actor->y, actor->z, ap) && projectPoint(camera, prop->x, prop->y, prop->z, pp) && ap.depth < pp.depth);
		}
		if (actorFront) { hit.name = actor->name; hit.type = "actor"; return hit; }
		if (prop) { hit.name = prop->name; hit.type = "prop"; return hit; }
	}
	if (pointInRoom(x, y) && roomSystem.scene >= 0 && uint32(roomSystem.scene) < room->data.scenes.size() && roomSystem.view >= 0 && uint32(roomSystem.view) < room->data.scenes[roomSystem.scene].views.size()) {
		const auto &paints = room->data.scenes[roomSystem.scene].views[roomSystem.view].paintings;
		int64 area = 0x7fffffffffffffffLL;
		if (!roomSystem.moving) for (uint32 i = 0; i < paints.size(); ++i) {
			const auto &r = paints[i].rect;
			const int32 left = MIN<int32>(r.left, r.right), right = MAX<int32>(r.left, r.right), top = MIN<int32>(r.top, r.bottom), bottom = MAX<int32>(r.top, r.bottom);
			const int64 size = int64(right - left) * (bottom - top);
			if (x >= left && x <= right && y >= top && y <= bottom && size < area) { area = size; hit.name = paints[i].name; hit.type = "painting"; }
		}
		if (hit.type == "none") { hit.name = roomSystem.sceneName(); hit.type = "scene"; }
		return hit;
	}
	const auto stage = stageSystem.current;
	if (stage && stageSystem.flat >= 0) {
		const int32 region = stage->data.hitTest(stageSystem.flat, int16(x), int16(y));
		hit.type = region >= 0 ? "button" : "flat";
		hit.name = region >= 0 ? stage->data.flats[stageSystem.flat].regions[region].name : stageSystem.currentFlat();
	}
	return hit;
}
bool Session::inputCall(const Expression &call, ScriptFrame &frame, Value &result) {
	switch (call.opcode) {
	case 12073: case 20005: case 20006: case 20007: case 20039: case 20041: case 20070: break;
	default: return false;
	}
	const Values args = runtime.arguments(call, frame); if (!runtime.current(frame)) return true;
	const Value a = args.size() ? args[0] : Value(); result = Value();
	switch (call.opcode) {
	case 12073: pendingEvents.clear(); break;
	case 20005: case 20007:
		_pollingButton = true; ++_buttonQuerySequence;
		yieldFrame(false); _pollingButton = false;
		result = Value(pointerDown ? 1 : 0); break;
	case 20006: result = Value(int32(uint32(uint16(pointer.x)) << 16 | uint16(pointer.y))); break;
	case 20041: result = Value(pointInRoom(int16(uint32(a.number()) >> 16), int16(a.number())) ? 1 : 0); break;
	case 20039: case 20070: {
		const int32 pt = call.opcode == 20039 && args.size() > 1 ? args[1].number() : a.number();
		const Hit hit = hitTest(int16(uint32(pt) >> 16), int16(pt));
		if (call.opcode == 20070) { lastResult = Value(hit.type); result = Value(hit.name); }
		else result = Value(hit.type == "actor" && hit.name.equalsIgnoreCase(a.text()) ? 1 : 0);
		break;
	}
	}
	return true;
}
void Session::keyInput(const Common::Event &event) {
	Common::String key;
	switch (event.kbd.keycode) {
	case Common::KEYCODE_UP: key = "uparrow"; break;
	case Common::KEYCODE_DOWN: key = "downarrow"; break;
	case Common::KEYCODE_LEFT: key = "leftarrow"; break;
	case Common::KEYCODE_RIGHT: key = "rightarrow"; break;
	case Common::KEYCODE_ESCAPE: key = "."; break;
	default: if (event.kbd.ascii > 0 && event.kbd.ascii < 256) key += char(event.kbd.ascii); break;
	}
	if (key.empty()) return;
	Values args; args.push_back(Value(key));
	const auto stage = stageSystem.current;
	if ((!roomSystem.current || !roomSystem.visible) && stage) {
		const auto flat = stageSystem.flat >= 0 ? stage->flats[stageSystem.flat] : Common::SharedPtr<ScriptInstance>();
		const auto own = flat && flat->script.handlers.contains("keydown") ? flat : stage->main;
		if (own && own->script.handlers.contains("keydown")) { runtime.run(*own, "keydown", args); return; }
	}
	const Common::String handler = event.kbdRepeat ? "keyrepeat" : "keydown";
	for (uint32 i = 0; i < bootScripts.size(); ++i) if (bootScripts[i]->script.handlers.contains(handler)) { runtime.run(*bootScripts[i], handler, args, key); return; }
	if (event.kbdRepeat) { Common::Event first = event; first.kbdRepeat = false; keyInput(first); }
}
void Session::mouseInput(const Common::Event &event) {
	const int16 x = int16(floor(double(event.mouse.x) * 512 / 640)), y = int16(floor(double(event.mouse.y) * 384 / 480));
	Values args; args.push_back(Value(int32(uint32(uint16(x)) << 16 | uint16(y))));
	for (uint32 i = 0; i < bootScripts.size(); ++i) if (bootScripts[i]->script.handlers.contains("mousedown")) { runtime.run(*bootScripts[i], "mousedown", args); return; }
}
void Session::serviceInventoryClicks() {
	if (!_system || _dialogActive) return;
	const uint32 now = _system->getMillis();
	if (_inventoryClickDown && int32(now - _inventoryClickDue) >= 0) {
		Common::Event event; event.type = Common::EVENT_LBUTTONUP; event.mouse = _inventoryClickPoint; postEvent(event);
		_inventoryClickDown = false; _inventoryClickDue = now + 100; return;
	}
	if (_inventoryClickDown) return;
	if (!runtime.isRunning("handleselect") && !runtime.isRunning("infoyoself") && !runtime.isRunning("invenmovie")) { _inventoryClicks.clear(); _inventoryReopen = false; }
	if (!_inventoryClicks.empty() && inventoryActive(*this) && runtime.isRunning("handleselect") && int32(now - _inventoryClickDue) >= 0) {
		_inventoryClickPoint = _inventoryClicks[0]; _inventoryClicks.remove_at(0);
		Common::Event event; event.type = Common::EVENT_LBUTTONDOWN; event.mouse = _inventoryClickPoint; postEvent(event);
		_inventoryClickDown = true; _inventoryClickDue = now + 80; return;
	}
	if (_inventoryReopen && _inventoryClicks.empty() && inventoryActive(*this) && runtime.isRunning("handleselect") && !runtime.isRunning("infoyoself") && !runtime.isRunning("invenmovie")) {
		const auto stage = stageSystem.current; bool examining = false;
		for (const auto &button : stage->buttons[stageSystem.flat]) if (runtime.isRunning(*button, "mousedown")) examining = true;
		if (!examining) { _inventoryReopen = false; _inventoryRequested = true; }
	}
}
void Session::serviceInventory() {
	const bool selecting = runtime.isRunning("handleselect") && inventoryActive(*this);
	if (!_system || _dialogActive || (runtime.busy() && !selecting) || movies.active() || (puppetSystem.current && !selecting) || roomSystem.moving || roomSystem.turning || pointerDown || _paletteBlack || screenBlack || !_inventoryClicks.empty() || _inventoryClickDown) return;
	if (!inventoryActive(*this)) {
		_inventoryShown = false;
		if (!_inventoryRequested) return;
		const bool main = stageSystem.current && (version == 1 ? stageSystem.currentFlat() == "mainpanel" : stageSystem.current->name == "main.stg");
		const auto bag = propSystem.get(version == 1 ? "avatar" : "bag");
		if (!main || !bag || !bag->visible || (version != 1 && bag->owner.text() != "frank")) { _inventoryRequested = false; return; }
		if (version == 1 || bag->view == "darkclosed" || bag->view == "lightclosed" || (!sound.isTitanicRetail() && (bag->view == "darkopened" || bag->view == "lightopened"))) {
			prepareRouteCheckpoint();
			const Common::String previousView = bag->view; const auto previousStage = stageSystem.current; const int32 previousFlat = stageSystem.flat;
			ScriptFrame event = runtime.eventFrame(bag->name, "mousedown"); Values args; args.push_back(Value(int32(uint32(uint16(bag->anchorX)) << 16 | uint16(bag->anchorY))));
			propSystem.send(true, bag->name, "mousedown", args, event, true);
			if (bag->view == previousView && stageSystem.current == previousStage && stageSystem.flat == previousFlat) _inventoryRequested = false;
		}
		return;
	}
	const bool enabled = ConfMan.hasKey("dreamfactory_modern_inventory") && ConfMan.getBool("dreamfactory_modern_inventory");
	if (!_inventoryRequested && (!enabled || _inventoryShown)) return;
	_inventoryRequested = false; _inventoryShown = true; _inventoryOfferItem.clear(); pendingEvents.clear();
	const uint32 started = _system->getMillis(); InventoryResult result;
	{
		PauseToken pause = g_engine ? g_engine->pauseEngine() : PauseToken(); _dialogActive = true;
		result = openInventory(*this); _dialogActive = false;
	}
	pointerDown = false; _pointerButtons = 0; _controlHeld = _touchHeld = false; excludeWallTime(started);
	_system->warpMouse((pointer.x * 640 + 511) / 512, (pointer.y * 480 + 383) / 384);
	if (_system->getEventManager()->shouldQuit() || _system->getEventManager()->shouldReturnToLauncher()) { _stopped = true; runtime.abandon(); return; }
	if (selecting) {
		const auto addClick = [&](int16 x, int16 y) { _inventoryClicks.push_back(Common::Point((x * 640 + 511) / 512, (y * 480 + 383) / 384)); };
		if (result.action != kInventoryBack) {
			const auto prop = propSystem.get(result.item); if (!prop) return;
			Common::Point point(prop->anchorX, prop->anchorY); bool found = false;
			for (int y = 0; y < 384 && !found; ++y) for (int x = 0; x < 512; ++x) if (prop->contains(x, y)) {
				const Hit hit = hitTest(x, y); if (hit.type == "prop" && hit.name.equalsIgnoreCase(result.item)) { point = Common::Point(x, y); found = true; break; }
			}
			if (!found) return;
			addClick(point.x, point.y);
		}
		const Common::String button = result.action == kInventoryExamine ? version == 1 ? "info" : "examine" : "ok";
		for (const auto &region : stageSystem.current->data.flats[stageSystem.flat].regions) if (region.name.equalsIgnoreCase(button)) addClick((region.rect.left + region.rect.right) / 2, (region.rect.top + region.rect.bottom) / 2);
		_inventoryReopen = result.action == kInventoryExamine; _inventoryClickDue = _system->getMillis();
		return;
	}
	if (!inventoryAction(*this, result)) return;
	if (result.action == kInventoryExamine && inventoryActive(*this)) _inventoryRequested = true;
	if (result.action == kInventoryUse) {
		const auto prop = propSystem.get(result.item); const auto held = runtime.globals.find("handitem");
		if (prop && prop->view == "large" && held != runtime.globals.end() && held->_value.text() == result.item) { _inventoryOfferItem = result.item; _inventoryOfferLabel = result.title; }
	}
	presentWorld();
}
bool Session::step() {
	if (_stopped || !runtime.error.empty()) return false;
	pollInput(); if (_stopped) return false;
	if (_recoveryRequested && !_dialogActive && !runtime.busy() && !movies.active() && !puppetSystem.current && !pointerDown && !roomSystem.moving && !roomSystem.turning) { continueRecovery(); return !_stopped && runtime.error.empty(); }
	if (!_inventoryOfferItem.empty()) {
		const auto held = runtime.globals.find("handitem"); const auto prop = propSystem.get(_inventoryOfferItem);
		if (!prop || prop->view != "large" || held == runtime.globals.end() || held->_value.text() != _inventoryOfferItem || puppetSystem.current || movies.active()) _inventoryOfferItem.clear();
	}
	serviceInventory(); if (_stopped || !runtime.error.empty()) return false;
	while (!pendingEvents.empty()) {
		const Common::Event event = pendingEvents[0]; pendingEvents.remove_at(0);
		if (event.type == Common::EVENT_QUIT || event.type == Common::EVENT_RETURN_TO_LAUNCHER) { postEvent(event); return false; }
		if ((event.type == Common::EVENT_KEYDOWN && !event.kbdRepeat) || event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_RBUTTONDOWN) prepareRouteCheckpoint();
		if (event.type == Common::EVENT_KEYDOWN) { keyInput(event); presentPuppet(); return !_stopped && runtime.error.empty(); }
		if (event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_RBUTTONDOWN) {
			if (!_inventoryOfferItem.empty() && event.type == Common::EVENT_LBUTTONDOWN && (event.customType == kPadInteract || !event.customType)) {
				const Common::String item = _inventoryOfferItem; _inventoryOfferItem.clear(); pointerDown = false; _pointerButtons = 0;
				const int16 x = event.mouse.x * 512 / 640, y = event.mouse.y * 384 / 480;
				if (!(x >= 440 && y >= 264 && y < 286)) inventoryOffer(*this, item, x, y);
			} else mouseInput(event);
			presentPuppet(); return !_stopped && runtime.error.empty();
		}
	}
	// idle owns the shipped timing, cursor routing and calctime call. It yields
	// through forceupdate, so queued input waits until this handler finishes.
	for (uint32 i = 0; i < bootScripts.size(); ++i) if (bootScripts[i]->script.handlers.contains("idle")) {
		runtime.run(*bootScripts[i], "idle"); updateHoverCursor(); presentPuppet(); return !_stopped && runtime.error.empty();
	}
	updateHoverCursor(); yieldFrame(true); return !_stopped && runtime.error.empty();
}
}
