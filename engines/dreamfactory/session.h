/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SESSION_H
#define DREAMFACTORY_SESSION_H
#include "dreamfactory/resources.h"
#include "dreamfactory/journal.h"
#include "dreamfactory/routes.h"
#include "dreamfactory/runtime.h"
#include "dreamfactory/sound.h"
#include "dreamfactory/crickets.h"
#include "dreamfactory/cursors.h"
#include "dreamfactory/actors.h"
#include "dreamfactory/props.h"
#include "dreamfactory/puppets.h"
#include "dreamfactory/puppet_view.h"
#include "dreamfactory/display.h"
#include "dreamfactory/stages.h"
#include "dreamfactory/stage_view.h"
#include "dreamfactory/rooms.h"
#include "dreamfactory/motion.h"
#include "dreamfactory/balls.h"
#include "dreamfactory/stars.h"
#include "dreamfactory/scheduler.h"
#include "dreamfactory/movie_host.h"
#include "dreamfactory/save_host.h"
#include "common/events.h"
#include "common/ptr.h"
#include "common/random.h"
class OSystem;
namespace DreamFactory {
// Session-scoped globals, boot library, resource paths and host command state.
// Script instances have stable addresses even when a nested event opens files.
class Session : public ScriptHost, public LoopHost, public RoomHost, private PuppetSpeechHost, private StageHost {
public:
	explicit Session(OSystem *system = nullptr, SaveHost *saveHost = nullptr);
	bool init(const Common::FSNode &root);
	bool boot();
	bool canSave() const;
	bool canAutosave() const;
	bool saveState(Bytes &bytes, Common::String &error);
	bool loadState(const Bytes &bytes, Common::String &error);
	void resetClock();
	void excludeWallTime(uint32 started);
	bool subtitlesEnabled() const;
	Journal journal;
	RouteStore routes;
	bool restoreRouteScene(uint32 node, Common::String &error);
	bool journalAudio(const JournalEntry &entry, Common::SharedPtr<AudioClip> &clip);
	// One main-loop pass: deliver one queued gesture, otherwise run boot idle.
	bool step();
	void postEvent(const Common::Event &event);
	struct Hit { Common::String name, type = "none"; };
	Hit hitTest(int32 x, int32 y);
	Common::Point pointer;
	bool pointerDown = false;
	bool invoke(Runtime &runtime, const Expression &call, ScriptFrame &frame, Value &result) override;
	bool resolveLibrary(Runtime &runtime, const Common::String &name, const Values &args, ScriptFrame &frame, Value &result) override;
	Resources resources;
	Display display;
	Runtime runtime;
	SoundSystem sound;
	CricketSystem crickets;
	CursorSystem cursors;
	CastSystem castSystem;
	PropSystem propSystem;
	PuppetSystem puppetSystem;
	PuppetSpeech puppetSpeech;
	PuppetChoices puppetChoices;
	StageSystem stageSystem;
	RoomSystem roomSystem;
	MotionSystem motion;
	BallSystem balls;
	StarRegistry stars;
	void bindStars(const StarTable &table, const Common::String &set);
	Scheduler scheduler;
	void advanceTime(uint64 now, bool dispatch = true);
	uint64 elapsedMillis() const { return _time; }
	bool scriptQuitRequested() const { return _scriptQuit; }
	bool dialogActive() const { return _dialogActive; }
	uint64 dialogGeneration() const { return _dialogGeneration; }
	bool pollingButton() const { return _pollingButton; }
	uint64 buttonQuerySequence() const { return _buttonQuerySequence; }
	uint32 version = 0;
	int32 frameRate = 3;
	bool menuVisible = false, keyAborts = false, puppetGrab = false, screenBlack = false;
	Common::HashMap<int32, int32> puppetParams;
	Common::Point playerHitbox;
	int32 cameraHi = 0;
	// Unmodified palette of the last drawn source, retained across black ramps.
	byte displayPalette[768] = {};
	Value lastResult;
	Common::Array<Common::SharedPtr<ScriptInstance> > bootScripts;
	Common::Array<Common::Event> pendingEvents;
	struct TextEntry { Common::String text; int32 x = 0, y = 0, color = 0, size = 12; };
	Common::Array<TextEntry> textOverlay;
	MovieSystem movies;
private:
	friend class SaveState;
	OSystem *_system;
	Common::FSNode _root;
	EnhancementPack _artPack;
	Common::String _bootSignature;
	SaveHost *_saveHost;
	Common::RandomSource _random;
	uint64 _time = 0;
	uint32 _lastMillis = 0;
	bool _paletteBlack = false;
	bool _stopped = false;
	bool _scriptQuit = false;
	bool _dialogActive = false;
	bool _journalRequested = false;
	Bytes _routePending;
	Common::SharedPtr<LoadedPuppet> _routeOwner;
	uint32 _savedRouteCursor = 0;
	int32 _savedRouteChoice = -1;
	Common::String _savedRouteToken;
	uint32 _routeResumeScene = 0;
	void prepareRouteCheckpoint();
	void recordRoute(const JournalEntry &entry, bool decision);
	uint64 _dialogGeneration = 0;
	bool _pollingButton = false;
	uint64 _buttonQuerySequence = 0;
	byte _pointerButtons = 0;
	void pollInput();
	bool pointInRoom(int32 x, int32 y) const;
	bool inputCall(const Expression &call, ScriptFrame &frame, Value &result);
	void keyInput(const Common::Event &event);
	void mouseInput(const Common::Event &event);
	void serviceCrickets() override { crickets.service(); }
	void serviceAnimation(uint64 now) override { propSystem.animate(now); castSystem.animate(); }
	void bindRoom(const Room *room) override;
	void roomListener(const Scene &scene, const View &view) override;
	void presentRoom(const Room &room, const Frame &frame) override;
	bool waitRoomFrame(ScriptFrame *parent) override;
	bool roomCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool cricketCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool audioCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool actorCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool serviceMotion() override { return motion.service() && (version != 1 || balls.service()); }
	bool motionCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool stageCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool propCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool puppetCall(const Expression &call, ScriptFrame &frame, Value &result);
	void puppetVoice(const Common::String &name, const Common::SharedPtr<AudioClip> &clip) override { sound.playVoiceClip(name, clip); }
	void puppetSilence() override { sound.haltVoice(); }
	void speakPuppet(const Common::String &ident, ScriptFrame &frame, bool queue = true);
	int32 waitPuppet(ScriptFrame &frame);
	void repeatPuppet(ScriptFrame &frame);
	void presentPuppet();
	void presentWorld();
	bool presentStage(const Room *room, const Frame *frame);
	void compositeSprites(Bytes &rgba, const byte *palette, const Room *room, const Frame *frame);
	void compositeHints(Bytes &rgba);
    bool touchControlsVisible() const;
    void compositeControls(Bytes &rgba);
    bool controlEvent(const Common::Event &event);
    bool focusControl(int32 direction);
    Hit _controlHit;
    Common::Point _controlPoint, _controlPress;
    bool _controlFocused = false, _controlHeld = false, _touchHeld = false;

	bool _hintValid = false;
	uint64 _hintTime = 0;
	Common::Array<Common::Point> _hintPoints;
	void flatChanged() override { textOverlay.clear(); _hintValid = false; _controlFocused = false; }
	bool loadFonts();
	bool textCall(const Expression &call, ScriptFrame &frame, Value &result);
	void compositeText(Bytes &rgba, const byte *palette);
	void eraseText(const Prop &prop);
	StageView _stageView;
	PuppetView _puppetView;
	PuppetFonts _puppetFonts;
	bool _fontLoaded = false;
	Common::String _paletteKind;
	Bytes _roomPalette, _stagePalette, _puppetPalette, _puppetArtPalette;
	bool timingCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool screenCall(const Expression &call, ScriptFrame &frame, Value &result);
	bool sourcePalette(Common::String name, byte *palette);
	void showPalette(const byte *palette);
	void waitFade(uint64 tick, ScriptFrame &frame);
	bool canFireLoop(const GameLoop &loop) override;
	bool fireLoop(const GameLoop &loop) override;
	void sampleTime();
	void yieldFrame(bool cooperative = false);
};
}
#endif
