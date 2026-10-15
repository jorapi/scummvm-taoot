/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/script_audit.h"
#include "dreamfactory/runtime.h"
#include "dreamfactory/stage.h"
#include "dreamfactory/image.h"
#include "dreamfactory/resources.h"
#include "dreamfactory/session.h"
#include "dreamfactory/routes.h"
#include "dreamfactory/controls.h"
#include "dreamfactory/checkers.h"
#include "dreamfactory/audio.h"
#include "audio/audiostream.h"
#include "dreamfactory/cast.h"
#include "dreamfactory/movie.h"
#include "dreamfactory/shop.h"
#include "dreamfactory/puppet.h"
#include "dreamfactory/puppet_view.h"
#include "common/file.h"
#include "common/config-manager.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/endian.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "graphics/font.h"
#include "gui/gui-manager.h"
#include "backends/keymapper/keymapper.h"
#include "backends/keymapper/hardware-input.h"
#include "graphics/paletteman.h"
#include "graphics/cursorman.h"
#include "graphics/fonts/ttf.h"
#include "image/bmp.h"
namespace DreamFactory {
static bool collectFiles(const Common::FSNode &node, Common::FSList &files, uint32 depth = 0) {
	Common::FSList children;
	if (depth > 16 || !node.getChildren(children, Common::FSNode::kListAll)) return false;
	for (uint32 i = 0; i < children.size(); ++i) {
		if (children[i].isDirectory()) { if (!collectFiles(children[i], files, depth + 1)) return false; }
		else files.push_back(children[i]);
	}
	return true;
}
static bool corpusFiles(const Common::FSNode &root, Common::FSList &files) {
	const Common::FSNode volumes = root.getChild("volumes"), installed = root.getChild("installed");
	if (!volumes.isDirectory()) return collectFiles(root, files);
	// Check every original variant, rather than only the flat viewer's winner.
	return collectFiles(volumes, files) && (!installed.isDirectory() || collectFiles(installed, files));
}
static Common::String label(const Common::FSNode &node, const Common::FSNode &root) {
	return node.getPath().relativeTo(root.getPath()).toString('/');
}
static uint32 statementCount(const Common::Array<Statement> &body) {
	uint32 count = 0;
	for (uint32 i = 0; i < body.size(); ++i) {
		if (body[i].kind != Statement::kCase) ++count;
		count += statementCount(body[i].body) + statementCount(body[i].alternate);
	}
	return count;
}
static bool read(const Common::FSNode &node, Bytes &data) {
	Common::File file;
	if (!file.open(node) || file.size() < 0 || file.size() > 128 * 1024 * 1024) return false;
	data.resize(uint32(file.size()));
	return file.read(data.data(), data.size()) == data.size();
}
class AuditHash {
public:
	uint32 hash = 2166136261u;
	void byteValue(byte b) { hash = (hash ^ b) * 16777619u; }
	void number(uint32 n) { for (uint32 i = 0; i < 4; ++i) byteValue(byte(n >> (8 * i))); }
	void text(const Common::String &s) { for (uint32 i = 0; i < s.size(); ++i) byteValue(byte(s[i])); byteValue(0); }
};
class RouteTestFiles : public Common::SaveFileManager {
public:
	Common::HashMap<Common::String, Bytes> files;
	Common::String failSuffix;
	class Output : public Common::OutSaveFile {
		RouteTestFiles &_owner;
		Common::String _name;
		bool _failed = false;
	public:
		Output(RouteTestFiles &owner, const Common::String &name) : OutSaveFile(new Common::MemoryWriteStreamDynamic(DisposeAfterUse::YES)), _owner(owner), _name(name) {}
		void finalize() override {
			auto *stream = static_cast<Common::MemoryWriteStreamDynamic *>(_wrapped);
			_failed = !_owner.failSuffix.empty() && _name.hasSuffix(_owner.failSuffix);
			Bytes &bytes = _owner.files[_name]; bytes.resize(_failed ? 1 : uint32(stream->size()));
			memcpy(bytes.data(), stream->getData(), bytes.size());
		}
		bool err() const override { return _failed || OutSaveFile::err(); }
	};
	Common::OutSaveFile *openForSaving(const Common::String &name, bool compress = true) override { return new Output(*this, name); }
	Common::InSaveFile *openForLoading(const Common::String &name) override {
		if (!files.contains(name)) return nullptr;
		const Bytes &bytes = files[name]; return new Common::MemoryReadStream(bytes.data(), bytes.size());
	}
	Common::InSaveFile *openRawFile(const Common::String &name) override { return openForLoading(name); }
	bool removeSavefile(const Common::String &name) override { if (!files.contains(name)) return false; files.erase(name); return true; }
	Common::StringArray listSavefiles(const Common::String &pattern) override {
		Common::StringArray names;
		for (const auto &file : files) if (file._key.matchString(pattern)) names.push_back(file._key);
		return names;
	}
	void updateSavefilesList(Common::StringArray &) override {}
	bool exists(const Common::String &name) override { return files.contains(name); }
};
bool auditPuppetSession(const Common::FSNode &root, OSystem *backend) {
	const char *bad[] = { "bad-head", "bad-version", "bad-lines", "bad-band", "bad-script", "bad-name", "bad-layer", "bad-frame", "bad-stance" };
	Bytes bytes; for (uint32 i = 0; i < ARRAYSIZE(bad); ++i) { Puppet value; if (!read(root.getChild(Common::String(bad[i]) + ".pup"), bytes) || value.load(bytes)) { warning("PUPPET READER FAIL accepted %s", bad[i]); return false; } }
	Puppet damaged; Common::Array<PuppetPose> poses; PuppetPose retained; retained.layers[0].frame = 99; poses.push_back(retained);
	if (!read(root.getChild("bad-animation.pup"), bytes) || !damaged.load(bytes) || damaged.animation(9, poses) || poses.size() != 1 || poses[0].layers[0].frame != 99) { warning("PUPPET READER FAIL animation transaction"); return false; }
	warning("PUPPET READER PASS header counts names script/band/frame references stance bounds animation truncation transaction");
	Session session; ScriptInstance test; test.name = "puppet-test"; Common::String error;
	if (!session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("PUPPET SESSION FAIL init %s", error.c_str()); return false; }
	const ScriptResult result = session.runtime.run(test, "test"); const auto loaded = session.puppetSystem.current;
	if (result.value.number() != 12345 || !session.runtime.error.empty() || !loaded) { warning("PUPPET SESSION FAIL script value=%s error=%s", result.value.text().c_str(), session.runtime.error.c_str()); return false; }
	if (loaded->visible || loaded->stance != (session.version == 1 ? 0 : 1) || !loaded->hasPose || loaded->pose.layers[0].x != 100 || loaded->pose.layers[0].y != -5 || loaded->pose.layers[1].frame != -1) { warning("PUPPET SESSION FAIL stance/base"); return false; }
	if (loaded->choices.size() != 5 || loaded->choices.back().id != 105) { warning("PUPPET SESSION FAIL choice capacity/append"); return false; }
	bool choiceSeen[5] = {}; for (uint32 i = 0; i < 5; ++i) { const int32 id = loaded->choices[i].id - 101; if (id < 0 || id >= 5 || choiceSeen[id]) { warning("PUPPET SESSION FAIL shuffle permutation"); return false; } choiceSeen[id] = true; }
	if (loaded->data.line("LINE.1") != 1 || loaded->data.line(" hello THERE ", true) != 0 || loaded->data.line("Hello there") != -1 || loaded->data.textLine("line.1") != -1 || loaded->data.textLine(" Hello THERE ") != 0) { warning("PUPPET SESSION FAIL dialogue lookup"); return false; }
	session.runtime.run(test, "unknownbase"); if (loaded->pose.layers[0].x != 100) return false;
	session.runtime.run(test, "clearbase"); if (!loaded->choices.empty() || loaded->stance || loaded->pose.layers[0].x != 15 || loaded->visible) { warning("PUPPET SESSION FAIL clear/default base"); return false; }
	const ScriptResult replaced = session.runtime.run(test, "replaceown");
	if (replaced.value.text() != "branch/puppet-test" || session.puppetSystem.name() != "alternate" || !session.puppetSystem.current->visible || session.puppetSystem.current == loaded) { warning("PUPPET SESSION FAIL replacement/caller lifetime"); return false; }
	session.runtime.run(test, "badopen"); if (session.runtime.error.empty() || session.puppetSystem.name() != "alternate") { warning("PUPPET SESSION FAIL malformed open replaced active puppet"); return false; }
	session.runtime.error.clear(); const ScriptResult closed = session.runtime.run(test, "closeown");
	if (closed.value.text() != "branch/puppet-test" || session.puppetSystem.current || session.puppetSystem.name() != "none" || !session.runtime.error.empty()) { warning("PUPPET SESSION FAIL close/caller lifetime"); return false; }
	warning("PUPPET SESSION PASS compiled open internal name counts enumeration deferred calls parent helpers pass visibility poses choices shuffle missing/invalid replacement close lifetime");
	struct SpeechHost : PuppetSpeechHost {
		uint32 played = 0, stopped = 0, samples = 0;
		void puppetVoice(const Common::String &, const Common::SharedPtr<AudioClip> &clip) override { ++played; samples = clip->samples.size(); }
		void puppetSilence() override { ++stopped; }
	} host;
	PuppetSpeech speech(host);
	if (!session.puppetSystem.open("speech")) return false;
	const auto talker = session.puppetSystem.current;
	if (!speech.start(talker, "LINE.0", 100) || !speech.active() || host.played != 1 || host.samples != 200 || talker->subtitle != "Hello there" || talker->pose.layers[0].x != 10) return false;
	speech.tick(133); if (talker->pose.layers[0].x != 10) return false;
	speech.tick(134); if (talker->pose.layers[0].x != 20) return false;
	speech.tick(167); if (talker->pose.layers[0].x != 30) return false;
	speech.tick(449); if (!speech.active()) return false;
	speech.tick(450); if (speech.active() || !talker->subtitle.empty() || talker->pose.layers[0].x != 30 || talker->interrupted || talker->voiceQueue.size() != 1) return false;
	if (!speech.start(talker, " hello THERE ", 500)) return false;
	speech.skip(true);
	if (speech.active() || !talker->interrupted || talker->pose.layers[0].x != 30 || host.played != 2) return false;
	if (!speech.start(talker, "line.1", 600) || speech.active() || talker->voiceQueue.size() != 3 || host.played != 2) return false;
	if (!speech.start(talker, "line.0", 600) || talker->voiceQueue.size() != 3) return false;
	session.puppetSystem.clear(); if (!talker->interrupted || talker->voiceQueue.size() != 3) return false;
	talker->interrupted = false;
	if (!speech.start(talker, "line.1", 1000, false) || talker->stance != (session.version == 1 ? 0 : 1)) return false;
	speech.tick(2149); if (!speech.active()) return false; // Missing audio: minimum 1 s + tail.
	speech.tick(2150); if (speech.active() || talker->voiceQueue.size() != 3) return false;
	if (!speech.start(talker, "line.0", 2200) || !talker->subtitleBand) return false;
	speech.skip();
	if (speech.active() || talker->interrupted || !talker->subtitle.empty() || !talker->subtitleBand) return false;
	const uint32 playedAfterLineSkip = host.played;
	if (!speech.start(talker, "line.1", 2300) || !speech.active() || host.played != playedAfterLineSkip) return false;
	speech.abandon();
	if (talker->subtitleBand) return false;
	warning("DIALOGUE LINE SKIP PASS next line remains playable subtitle band retained full skip separate");
	// Caption gate is a property of each record, separate from subtitle preferences.
	talker->data.lines[1].text = "*studio note";
	if (!speech.start(talker, "line.1", 3000) || !talker->subtitle.empty()) return false;
	speech.abandon(); talker->data.lines[1].text = "   ";
	if (!speech.start(talker, "line.1", 3000) || !talker->subtitle.empty()) return false;
	speech.abandon(); talker->data.lines[1].text = "Visible words"; talker->data.lines[1].ident = "Idle 4";
	if (!speech.start(talker, "Idle 4", 3000) || !talker->subtitle.empty()) return false;
	speech.abandon();
	if (!speech.start(talker, "not present", 0) || speech.active()) return false;
	talker->data.lines[1].ident = "long"; talker->data.lines[1].text = "123456789012345678901234567890";
	if (!speech.start(talker, "long", 1000, false)) return false;
	speech.tick(3149); if (!speech.active()) return false;
	speech.tick(3150); if (speech.active()) return false;
	Common::SharedPtr<LoadedPuppet> broken(new LoadedPuppet());
	if (!read(root.getChild("bad-animation.pup"), bytes) || !broken->data.load(bytes) || speech.start(broken, "line.0", 0) || speech.error.empty() || speech.active()) return false;
	// Replacing a conversation cannot invalidate the controller's retained owner.
	if (!speech.start(talker, "line.0", 0) || !session.puppetSystem.open("alternate")) return false;
	speech.tick(350); if (speech.active() || session.puppetSystem.current->hasPose == false || !session.puppetSystem.current->subtitle.empty()) return false;
	warning("PUPPET SPEECH STATE PASS PCM timing 30Hz poses stance tail Escape run queue cap missing audio text lookup captions owner lifetime");

	Session modal(backend);
	if (!modal.init(root) || !modal.puppetSystem.open("speech")) return false;
	// Direct PUP speech is the voice channel, independent of ordinary effects.
	Common::SharedPtr<AudioClip> sustained(new AudioClip()); sustained->rate = 1000; sustained->samples.resize(10000);
	for (uint32 i = 0; i < sustained->samples.size(); ++i) sustained->samples[i] = int16(i % 64 * 128);
	Audio::SoundHandle effect;
	if (!modal.sound.playClip("effect", sustained, effect) || !modal.sound.playVoiceClip("PUP-LINE", sustained) || !modal.sound.handleActive(effect) || modal.sound.currentVoice() != "pup-line") return false;
	modal.sound.haltSounds(); if (!modal.sound.done(false) || modal.sound.done(true) || modal.sound.currentVoice() != "pup-line") return false;
	modal.sound.setWaveVolume(0); if (modal.sound.done(true)) return false;
	if (!modal.sound.playVoiceClip("replacement", sustained) || modal.sound.currentVoice() != "replacement") return false;
	modal.sound.haltVoice(); if (!modal.sound.done(true)) return false;
	// Display promotion and first font loading are outside authored speech time.
	// Prepare them before timing the line, as an already-open conversation does.
	if (modal.runtime.run(test, "displayshow").value.number() != 1 || !modal.runtime.error.empty()) return false;
	Common::Event click; click.type = Common::EVENT_LBUTTONDOWN; click.mouse = Common::Point(1, 1); backend->getEventManager()->pushEvent(click);
	Common::Event digit; digit.type = Common::EVENT_KEYDOWN; digit.kbd.keycode = Common::KEYCODE_3; digit.kbd.flags = Common::KBD_CTRL; backend->getEventManager()->pushEvent(digit);
	const uint32 started = backend->getMillis();
	const ScriptResult spoken = modal.runtime.run(test, "speak"); const uint32 elapsed = backend->getMillis() - started;
	if (spoken.value.number() != 42 || !modal.runtime.error.empty() || elapsed < 350 || elapsed > 1500 || modal.sound.waveVolume() != 3 || !modal.sound.done(true) || !modal.pendingEvents.empty() || modal.puppetSpeech.active()) { warning("PUPPET SPEECH HOST FAIL elapsed=%u value=%s volume=%d voiceDone=%u pending=%u active=%u error=%s", elapsed, spoken.value.text().c_str(), modal.sound.waveVolume(), modal.sound.done(true), modal.pendingEvents.size(), modal.puppetSpeech.active(), modal.runtime.error.c_str()); return false; }
	Common::Event escape; escape.type = Common::EVENT_KEYDOWN; escape.kbd.keycode = Common::KEYCODE_ESCAPE; backend->getEventManager()->pushEvent(escape);
	const ScriptResult skipped = modal.runtime.run(test, "skiprun");
	if (skipped.value.number() != 43 || !modal.puppetSystem.current->interrupted || modal.puppetSystem.current->voiceQueue.size() != 3 || !modal.sound.done(true)) return false;
	modal.puppetSystem.current->interrupted = false;
	Common::Event lineSkip; lineSkip.type = Common::EVENT_KEYDOWN; lineSkip.kbd.keycode = Common::KEYCODE_PERIOD;
	backend->getEventManager()->pushEvent(lineSkip);
	const uint32 lineSkipStarted = backend->getMillis();
	if (modal.runtime.run(test, "skiprun").value.number() != 43 || modal.puppetSystem.current->interrupted || backend->getMillis() - lineSkipStarted < 1150) return false;
	warning("DIALOGUE LINE SKIP HOST PASS period skips first line next line plays full skip retained");
	Common::Event quit; quit.type = Common::EVENT_QUIT; backend->getEventManager()->pushEvent(quit);
	const ScriptResult abandoned = modal.runtime.run(test, "quitspeech");
	if (abandoned.signal != ScriptResult::kAbandoned || modal.runtime.globals["afterspeech"].truth() || !modal.sound.done(true) || modal.puppetSpeech.active()) return false;
	warning("PUPPET SPEECH HOST PASS compiled modal pacing click filter Ctrl volume Escape queue quit abandonment silence elapsed=%u", elapsed);
	backend->getEventManager()->resetQuit();
	Session artwork; if (!artwork.init(root) || !artwork.puppetSystem.open("render")) return false;
	PuppetView view;
	Frame backdrop; backdrop.width = backdrop.height = 3; backdrop.pixels.resize(9); memset(backdrop.pixels.data(), 7, 9);
	byte palette[768]; for (uint32 i = 0; i < 256; ++i) { palette[i * 3] = 11; palette[i * 3 + 1] = 22; palette[i * 3 + 2] = 33; }
	Bytes rgba; rgba.resize(512 * 384 * 4);
	auto clear = [&]() { for (uint32 i = 0; i < rgba.size(); i += 4) { rgba[i] = 17; rgba[i + 1] = 31; rgba[i + 2] = 47; rgba[i + 3] = 255; } };
	auto pixel = [&](uint32 x, uint32 y, byte r, byte g, byte b) { const uint32 d = (y * 512 + x) * 4; return rgba[d] == r && rgba[d + 1] == g && rgba[d + 2] == b && rgba[d + 3] == 255; };
	clear();
	const auto art = artwork.puppetSystem.current;
	if (!view.composite(art, rgba, &backdrop, palette) || !pixel(0, 0, 11, 22, 33) || !pixel(1, 1, 5, 6, 7) || !pixel(2, 1, 11, 22, 33) || !pixel(1, 2, 0, 0, 0) || !pixel(2, 2, 9, 10, 11) || !pixel(0, 264, 5, 6, 7) || !pixel(1, 264, 17, 31, 47) || !pixel(0, 265, 0, 0, 0) || !pixel(1, 265, 255, 255, 255)) { warning("PUPPET ART FIXTURE FAIL layers anchors band matte opacity palettes"); return false; }
	art->pose.layers[1].frame = 99; art->pose.layers[1].x = -1; art->pose.layers[1].y = 223; art->subtitle = "caption";
	clear(); if (!view.composite(art, rgba, &backdrop, palette) || !pixel(0, 224, 0, 0, 0)) return false;
	clear(); if (!view.composite(art, rgba, &backdrop, palette, false) || !pixel(0, 224, 5, 6, 7)) return false;
	art->subtitle.clear(); art->subtitleBand = true;
	clear(); if (!view.composite(art, rgba, &backdrop, palette) || !pixel(0, 224, 0, 0, 0)) return false;
	clear(); if (!view.composite(art, rgba, &backdrop, palette, false) || !pixel(0, 224, 5, 6, 7)) return false;
	art->subtitleBand = false;
	art->visible = false; clear(); if (!view.composite(art, rgba, &backdrop, palette) || !pixel(0, 0, 17, 31, 47)) return false;
	if (!artwork.puppetSystem.open("renderother")) return false;
	clear(); if (!view.composite(artwork.puppetSystem.current, rgba, &backdrop, palette) || !pixel(1, 1, 6, 7, 8)) return false;
	if (!artwork.puppetSystem.open("renderbad") || view.composite(artwork.puppetSystem.current, rgba, &backdrop, palette)) return false;
	if (PuppetView::choiceAt(0, 264, 5) != 0 || PuppetView::choiceAt(511, 287, 5) != 0 || PuppetView::choiceAt(0, 288, 5) != 1 || PuppetView::choiceAt(511, 383, 5) != 4 || PuppetView::choiceAt(512, 264, 5) != -1 || PuppetView::choiceAt(-1, 264, 5) != -1 || PuppetView::choiceAt(0, 263, 5) != -1 || PuppetView::choiceAt(0, 384, 5) != -1 || PuppetView::choiceAt(0, 312, 2) != -1) return false;
	warning("PUPPET ART FIXTURE PASS layers anchors clipping matte alpha palettes band cache owner visibility bounds choices");
	artwork.puppetSystem.open("speech"); const auto rows = artwork.puppetSystem.current;
	artwork.puppetSystem.bevel("first", 71); artwork.puppetSystem.bevel("second", 92);
	rows->interrupted = true; rows->voiceQueue.push_back(0);
	PuppetChoices tracker; tracker.start(rows);
	if (!tracker.active() || rows->interrupted) return false;
	tracker.down(0, 100); tracker.move(1); tracker.up(1); tracker.tick(120);
	if (!tracker.active() || rows->pressed != -1 || rows->chosen != -1) return false;
	tracker.down(1, 200); tracker.move(-1); if (rows->pressInside || !tracker.pressVisible(209) || tracker.pressVisible(210)) return false;
	tracker.move(1); tracker.up(1); tracker.tick(209); if (!tracker.active() || rows->chosen != 1) return false;
	tracker.tick(210);
	if (tracker.active() || tracker.answer() != 92 || rows->choices.size() != 2 || rows->lastChoices.size() != 2 || rows->lastChosen != 1 || !rows->voiceQueue.empty()) return false;
	tracker.start(rows); tracker.escape();
	if (tracker.answer() != -1 || rows->lastChosen != -1 || rows->interrupted) return false;
	tracker.start(rows); tracker.focus(-1);
	if (rows->chosen != 1) return false;
	tracker.focus(1); if (rows->chosen != 0) return false;
	tracker.focus(-1); tracker.confirm(300); tracker.confirm(301, 0); tracker.tick(309);
	if (!tracker.active() || rows->chosen != 1) return false;
	tracker.tick(310); if (tracker.active() || tracker.answer() != 92) return false;
	tracker.start(rows); tracker.confirm(400, 4); if (rows->pressed != -1) return false;
	tracker.confirm(400, 0); tracker.tick(410); if (tracker.active() || tracker.answer() != rows->choices[0].id) return false;
	warning("DIALOGUE CONTROLS PASS focus wrap invalid digit authored ids ten-tick confirmation duplicate suppression");
	artwork.puppetSystem.clear(); tracker.start(rows); if (tracker.active()) return false;
	warning("PUPPET CHOICE STATE PASS press drag cancel ten ticks authored ids retained plaques queue reset escape empty");

	Session choices(backend); if (!choices.init(root) || !choices.puppetSystem.open("speech")) return false;
	Common::Event down; down.type = Common::EVENT_LBUTTONDOWN; down.mouse = Common::Point(20, 360);
	Common::Event up = down; up.type = Common::EVENT_LBUTTONUP;
	backend->getEventManager()->pushEvent(down); backend->getEventManager()->pushEvent(up);
	const uint32 choiceStart = backend->getMillis();
	choices.cursors.set("watch");
	const ScriptResult selected = choices.runtime.run(test, "choices");
	const uint32 choiceElapsed = backend->getMillis() - choiceStart;
	const auto selectedPuppet = choices.puppetSystem.current;
	if (choices.cursors.name != "arrow") return false;
	if (selected.value.number() != 902 || !choices.runtime.error.empty() || choiceElapsed < 150 || choiceElapsed > 1500 || selectedPuppet->chosen != 1 || selectedPuppet->choices.size() != 2 || selectedPuppet->lastChosen != 1 || choices.puppetChoices.active()) { warning("PUPPET CHOICE HOST FAIL selection elapsed=%u value=%s error=%s", choiceElapsed, selected.value.text().c_str(), choices.runtime.error.c_str()); return false; }
	const bool hadControls = ConfMan.hasKey("dreamfactory_dialogue_controls", Common::ConfigManager::kTransientDomain);
	const Common::String oldControls = hadControls ? ConfMan.get("dreamfactory_dialogue_controls", Common::ConfigManager::kTransientDomain) : Common::String();
	ConfMan.setBool("dreamfactory_dialogue_controls", true, Common::ConfigManager::kTransientDomain);
	class DialogueTapSource : public Common::EventSource {
	public:
		OSystem *backend; uint32 start, phase = 0;
		DialogueTapSource(OSystem *system) : backend(system), start(system->getMillis()) { backend->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~DialogueTapSource() { backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			if (phase >= 2 || backend->getMillis() - start < (phase ? 260u : 240u)) return false;
			event.type = phase++ ? Common::EVENT_LBUTTONUP : Common::EVENT_LBUTTONDOWN; event.mouse = Common::Point(20, 20); return true;
		}
	};
	{
		DialogueTapSource source(backend);
		Common::Event orphan; orphan.type = Common::EVENT_LBUTTONUP; orphan.mouse = Common::Point(20, 20); backend->getEventManager()->pushEvent(orphan);
		if (choices.runtime.run(test, "speak").value.number() != 42 || source.phase != 2 || selectedPuppet->interrupted) return false;
	}
	selectedPuppet->interrupted = false;
	Common::Event confirmSpeech; confirmSpeech.type = Common::EVENT_KEYDOWN; confirmSpeech.kbd.keycode = Common::KEYCODE_RETURN; backend->getEventManager()->pushEvent(confirmSpeech);
	confirmSpeech.type = Common::EVENT_KEYUP; backend->getEventManager()->pushEvent(confirmSpeech);
	if (choices.runtime.run(test, "speak").value.number() != 42 || selectedPuppet->interrupted) return false;
	warning("DIALOGUE TOUCH PASS paired tap skips speech orphan release ignored controller confirm skips");
	Common::Event navigation; navigation.type = Common::EVENT_KEYDOWN; navigation.kbd.keycode = Common::KEYCODE_UP;
	backend->getEventManager()->pushEvent(navigation);
	navigation.kbd.keycode = Common::KEYCODE_RETURN; backend->getEventManager()->pushEvent(navigation);
	if (choices.runtime.run(test, "choices").value.number() != 902 || choices.puppetChoices.active()) return false;
	navigation.kbd.keycode = Common::KEYCODE_1; backend->getEventManager()->pushEvent(navigation);
	if (choices.runtime.run(test, "choices").value.number() != 701 || choices.puppetChoices.active()) return false;
	ConfMan.setBool("dreamfactory_dialogue_controls", false, Common::ConfigManager::kTransientDomain);
	backend->getEventManager()->pushEvent(navigation); backend->getEventManager()->pushEvent(escape);
	if (choices.runtime.run(test, "choices").value.number() != -1) return false;
    Common::Event pad; pad.type=Common::EVENT_CUSTOM_ENGINE_ACTION_START; pad.customType=kPadUp;
    choices.pointer = Common::Point(20, 270);
    backend->getEventManager()->pushEvent(pad); pad.customType=kPadInteract; backend->getEventManager()->pushEvent(pad);
    if(choices.runtime.run(test,"choices").value.number()!=902 || choices.puppetChoices.active()) { warning("GAMEPAD DIALOGUE FAIL original keyboard mode"); return false; }
    selectedPuppet->interrupted=false; backend->getEventManager()->pushEvent(pad);
    if(choices.runtime.run(test,"speak").value.number()!=42 || selectedPuppet->interrupted) return false;
    warning("GAMEPAD DIALOGUE PASS reply navigation confirm speech skip independent of enhanced keyboard option");
	selectedPuppet->chosen = -1;
	choices.pointer = Common::Point(20, 300);
	backend->getEventManager()->pushEvent(pad);
	if (choices.runtime.run(test, "choices").value.number() != 902 || choices.puppetChoices.active()) return false;
	Common::Event skipLine; skipLine.type = Common::EVENT_KEYDOWN; skipLine.kbd.keycode = Common::KEYCODE_PERIOD;
	backend->getEventManager()->pushEvent(skipLine);
	if (choices.runtime.run(test, "speak").value.number() != 42 || selectedPuppet->interrupted) return false;
	warning("GAMEPAD CURSOR REPLY PASS unfocused confirm selects cursor row standard skip-line supported");
	pad.customType = kPadPrevious; backend->getEventManager()->pushEvent(pad);
	pad.customType = kPadInteract; backend->getEventManager()->pushEvent(pad);
	if (choices.runtime.run(test, "choices").value.number() != 902 || PuppetView::choiceAt(choices.pointer.x, choices.pointer.y, 2) != 1) {
		warning("GAMEPAD DIALOGUE FAIL shoulder selection did not move cursor to reply"); return false;
	}
	pad.customType = kPadClick; backend->getEventManager()->pushEvent(pad);
	if (choices.runtime.run(test, "skiprun").value.number() != 43 || selectedPuppet->interrupted) return false;
	warning("GAMEPAD X LINE SKIP PASS next authored line plays");
	if (hadControls) ConfMan.set("dreamfactory_dialogue_controls", oldControls, Common::ConfigManager::kTransientDomain);
	else ConfMan.removeKey("dreamfactory_dialogue_controls", Common::ConfigManager::kTransientDomain);
	warning("DIALOGUE CONTROLS HOST PASS enabled navigation confirm digits disabled original behavior");
	backend->getEventManager()->pushEvent(escape);
	if (choices.runtime.run(test, "waitchoices").value.number() != -1 || selectedPuppet->interrupted || selectedPuppet->lastChosen != -1) return false;
	class ChoiceSource : public Common::EventSource {
	public:
		OSystem *backend; Session &session; uint32 start, stage = 0; bool idle, sawReply = false, sawArrow = false;
		ChoiceSource(OSystem *b, Session &s, bool i) : backend(b), session(s), start(b->getMillis()), idle(i) { b->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~ChoiceSource() { backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			if (session.puppetChoices.active() && session.cursors.name == "arrow") sawArrow = true;
			const uint32 elapsed = backend->getMillis() - start;
			if (!idle && stage == 1 && elapsed >= 100 && session.sound.currentVoice() == "line.0") sawReply = true;
			if (stage == 0 && session.puppetSpeech.active() && elapsed >= 50) { ++stage; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
			if (!idle && stage == 1 && elapsed >= 500) { ++stage; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
			if (elapsed > 3000 && stage < 3) { stage = 3; event.type = Common::EVENT_RETURN_TO_LAUNCHER; return true; }
			return false;
		}
	};
	selectedPuppet->lastChoices = selectedPuppet->choices; selectedPuppet->lastChosen = 0; selectedPuppet->voiceQueue.push_back(0);
	down.mouse = Common::Point(10, 10); backend->getEventManager()->pushEvent(down);
	{
		ChoiceSource source(backend, choices, false);
		choices.cursors.set("watch");
		if (choices.runtime.run(test, "waitchoices").value.number() != -1 || source.stage != 2 || !source.sawReply || selectedPuppet->interrupted || !choices.sound.done(true)) { warning("PUPPET CHOICE HOST FAIL repeat"); return false; }
		if (!source.sawArrow || choices.cursors.name != "arrow") return false;
	}
	if (!choices.puppetSystem.open("idle")) return false;
	choices.puppetParams[8] = 1;
	{
		ChoiceSource source(backend, choices, true);
		if (choices.runtime.run(test, "choices").value.number() != -1 || source.stage != 1 || !choices.puppetSystem.current->interrupted || !choices.sound.done(true)) { warning("PUPPET CHOICE HOST FAIL idle Escape"); return false; }
	}
	backend->getEventManager()->pushEvent(quit);
	const ScriptResult choiceQuit = choices.runtime.run(test, "quitchoices");
	if (choiceQuit.signal != ScriptResult::kAbandoned || choices.runtime.globals["afterchoice"].truth() || choices.puppetChoices.active()) return false;
	backend->getEventManager()->resetQuit();
	warning("PUPPET CHOICE HOST PASS compiled selection pacing retained rows Escape repeat idle queue quit");
	class FixedFont : public Graphics::Font {
	public:
		mutable uint32 last = 0;
		int getFontHeight() const override { return 4; }
		int getFontAscent() const override { return 3; }
		int getMaxCharWidth() const override { return 8; }
		int getCharWidth(uint32) const override { return 8; }
		void drawChar(Graphics::Surface *dst, uint32 chr, int x, int y, uint32 color) const override {
			last = chr; if (chr == ' ') return;
			for (int dy = 0; dy < 4; ++dy) for (int dx = 0; dx < 4; ++dx) if (x + dx >= 0 && x + dx < dst->w && y + dy >= 0 && y + dy < dst->h) dst->setPixel(x + dx, y + dy, color);
		}
	} fixed;
	Common::String longWord; for (uint32 i = 0; i < 62; ++i) longWord += 'A';
	if (!PuppetView::subtitleLines(longWord, fixed).empty()) return false;
	const auto wrapped = PuppetView::subtitleLines(longWord + " B", fixed);
	// 496 px does not fit; the break must precede that threshold.
	if (!wrapped.empty()) return false;
	longWord.deleteLastChar();
	const auto two = PuppetView::subtitleLines(longWord + " B", fixed);
	if (two.size() != 2 || two[0] != longWord || two[1] != "B") return false;
	art->visible = true; art->choices.clear(); art->subtitle = "Hi";
	PuppetChoice first; first.text = "answer"; first.id = 1; art->choices.push_back(first); art->choices.push_back(first);
	art->data.palette[0] = 255; art->data.palette[1] = 200; art->data.palette[2] = 100;
	art->data.palette[42 * 3] = 7; art->data.palette[42 * 3 + 1] = 8; art->data.palette[42 * 3 + 2] = 9;
	art->data.palette[251 * 3] = 40; art->data.palette[251 * 3 + 1] = 50; art->data.palette[251 * 3 + 2] = 60;
	Common::HashMap<int32, int32> params; params[10] = 25; params[3] = 42;
	art->chosen = 0; clear();
	if (!PuppetView::overlay(*art, rgba, fixed, params) || !pixel(8, 237, 255, 200, 100) || !pixel(25, 237, 0, 0, 0) || !pixel(25, 277, 7, 8, 9) || !pixel(0, 264, 40, 50, 60) || !pixel(2, 287, 40, 50, 60) || !pixel(3, 267, 17, 31, 47)) { warning("PUPPET TEXT FAIL margins baseline raw ink caption frame"); return false; }
	art->pressed = 1; clear();
	if (!PuppetView::overlay(*art, rgba, fixed, params, false, true) || !pixel(0, 288, 238, 224, 208) || !pixel(0, 264, 17, 31, 47) || !pixel(8, 237, 17, 31, 47)) return false;
	art->subtitle = longWord + " B"; art->choices.clear(); art->chosen = -1; clear();
	if (!PuppetView::overlay(*art, rgba, fixed, params) || !pixel(8, 237, 255, 200, 100) || !pixel(8, 253, 255, 200, 100)) return false;
	art->subtitle = Common::String::format("%c", 0x92); clear();
	if (!PuppetView::overlay(*art, rgba, fixed, params) || fixed.last != 0x2019) return false;
	PuppetFonts fonts;
	if (!read(root.getChild("FONT.TTF"), bytes) || !fonts.load(bytes) || !fonts.font() || fonts.font()->getFontAscent() <= 0 || !fonts.font(16) || !fonts.font(-16) || fonts.font(257)) return false;
	const Graphics::Font *font = fonts.font(); Bytes invalid; invalid.resize(20); memset(invalid.data(), 0, invalid.size());
#ifdef USE_FREETYPE2
	// Antialiasing must produce intermediate RGB values without changing the
	// monochrome metrics used by authored wrapping and text hit regions.
	Common::MemoryReadStream monoStream(bytes.data(), bytes.size());
	Common::ScopedPtr<Graphics::Font> mono(Graphics::loadTTFFont(&monoStream, DisposeAfterUse::NO, 12, Graphics::kTTFSizeModeCharacter, 72, 72, Graphics::kTTFRenderModeMonochrome));
	if (!mono || mono->getFontAscent() != font->getFontAscent() || mono->getFontHeight() != font->getFontHeight()) return false;
	const Common::U32String sample("Subtitle: curved letters and dialogue choices.");
	if (mono->getStringWidth(sample) != font->getStringWidth(sample)) return false;
	Graphics::Surface glyphs; glyphs.create(512, 32, Graphics::PixelFormat::createFormatRGBA32());
	glyphs.fillRect(Common::Rect(512, 32), glyphs.format.RGBToColor(0, 0, 0));
	font->drawString(&glyphs, sample, 0, 0, 512, glyphs.format.RGBToColor(255, 255, 255));
	bool blended = false;
	for (int y = 0; y < glyphs.h; ++y) for (int x = 0; x < glyphs.w; ++x) {
		byte r, g, b; glyphs.format.colorToRGB(glyphs.getPixel(x, y), r, g, b);
		if (r > 0 && r < 255 && r == g && g == b) blended = true;
	}
	glyphs.free();
	if (!blended) return false;
	warning("PUPPET FONT PASS antialiased RGB edges unchanged grid-fitted metrics");
#endif
	if (fonts.load(invalid) || fonts.font() != font) return false;
	bytes.clear(); // Glyphs may be loaded lazily; the font must own its stream.
	art->subtitle = "A native conversation subtitle, with punctuation: it"; art->subtitle += char(0x92); art->subtitle += "s working.";
	art->choices.push_back(first); art->chosen = 0; clear();
	if (!view.composite(art, rgba, &backdrop, palette) || !PuppetView::overlay(*art, rgba, *font, params)) return false;
	if (!artwork.puppetSystem.open("portrait")) return false;
	const auto portrait = artwork.puppetSystem.current;
	portrait->subtitle = art->subtitle; portrait->choices = art->choices; portrait->chosen = 0;
	backdrop.width = 512; backdrop.height = 264; backdrop.pixels.resize(512 * 264);
	for (uint32 y = 0; y < 264; ++y) for (uint32 x = 0; x < 512; ++x) backdrop.pixels[y * 512 + x] = byte(y * 127 / 264);
	for (uint32 i = 0; i < 256; ++i) { palette[i * 3] = i / 2; palette[i * 3 + 1] = 50 + i / 2; palette[i * 3 + 2] = 100 + i / 2; }
	params[3] = 250;
	clear(); if (!view.composite(portrait, rgba, &backdrop, palette) || !PuppetView::overlay(*portrait, rgba, *font, params)) return false;
	Common::DumpFile picture; if (!picture.open(root.getChild("text-preview.rgba")) || picture.write(rgba.data(), rgba.size()) != rgba.size()) return false;
	const Graphics::Font *screenFont = fonts.font(12, true);
	if (!screenFont || screenFont == font || screenFont != fonts.font(12, true) || screenFont->getFontHeight() <= font->getFontHeight() || !fonts.font(-16, true)) return false;
	Bytes enlargedText, screenText;
	if (!Display::scaleRGBA(rgba, 512, 384, enlargedText)) return false;
	clear(); if (!view.composite(portrait, rgba, &backdrop, palette) || !Display::scaleRGBA(rgba, 512, 384, screenText) ||
		!PuppetView::overlay(*portrait, screenText, *font, params, true, false, nullptr, screenFont)) return false;
	if (screenText == enlargedText) return false; // Must rerasterize, not stretch the small glyph bitmap.
	// Above the subtitle band the artwork retains the established pixel scaling.
	if (memcmp(screenText.data(), enlargedText.data(), 640 * 280 * 4)) return false;
	Common::DumpFile screenPicture;
	if (!screenPicture.open(root.getChild("text-screen-preview.rgba")) || screenPicture.write(screenText.data(), screenText.size()) != screenText.size()) return false;
	warning("PUPPET SCREEN TEXT PASS native 640x480 glyphs separate signed-size cache original wrapping art unchanged");
	Bytes accessible;
	if (!Display::scaleRGBA(rgba, 512, 384, accessible) ||
		!PuppetView::overlay(*portrait, accessible, *fonts.font(14), params, true, false, nullptr, fonts.font(14, true), true, screenFont)) return false;
	if (memcmp(accessible.data(), screenText.data(), 640 * 280 * 4)) return false;
	const uint32 border = 330 * 640 * 4, bottom = (479 * 640 + 639) * 4;
	if (accessible[border] != 255 || accessible[border + 1] != 230 || accessible[border + 2] != 0 ||
		accessible[bottom] || accessible[bottom + 1] || accessible[bottom + 2]) return false;
	Common::DumpFile accessiblePicture;
	if (!accessiblePicture.open(root.getChild("text-accessible-preview.rgba")) || accessiblePicture.write(accessible.data(), accessible.size()) != accessible.size()) return false;
	warning("DIALOGUE ACCESSIBILITY PASS larger font unchanged portrait high contrast band yellow focus");
	warning("PUPPET TEXT PASS wrap threshold two lines baselines margins raw ink invert frame cp1252 TrueType cache signed sizes owned stream invalid load");
	Session gfx(backend); if (!gfx.init(root)) return false;
	gfx.roomSystem.current.reset(new LoadedRoom()); Room &world = gfx.roomSystem.current->data;
	world.width = 512; world.height = 264; memset(world.palette, 0, 768); world.palette[21] = 91; world.palette[22] = 42; world.palette[23] = 17;
	Frame &worldFrame = gfx.roomSystem.frame; worldFrame.width = 512; worldFrame.height = 264; worldFrame.pixels.resize(512 * 264); memset(worldFrame.pixels.data(), 7, worldFrame.pixels.size());
	gfx.display.indexed(worldFrame.pixels, 512, 264, 64, 108, world.palette);
	auto runDisplay = [&](const char *handler) { const ScriptResult r = gfx.runtime.run(test, handler); if (r.value.number() != 1 || !gfx.runtime.error.empty()) { warning("PUPPET DISPLAY FAIL %s value=%s error=%s", handler, r.value.text().c_str(), gfx.runtime.error.c_str()); return false; } return true; };
	auto screenPixel = [&](uint32 x, uint32 y, byte r, byte g, byte b) { Graphics::Surface *screen = backend->lockScreen(); if (!screen) return false; const bool same = uint32(screen->getPixel(x, y)) == screen->format.RGBToColor(r, g, b); backend->unlockScreen(); return same; };
	auto compareScreen = [&]() {
		Graphics::Surface *screen = backend->lockScreen(); if (!screen) return false;
		bool same = true;
		for (uint32 y = 0; y < 480 && same; ++y) for (uint32 x = 0; x < 640; ++x) {
			const byte *p = rgba.data() + (y * 640 + x) * 4;
			if (uint32(screen->getPixel(x, y)) != screen->format.RGBToColor(p[0], p[1], p[2])) { same = false; break; }
		}
		backend->unlockScreen(); return same;
	};
	if (!runDisplay("displayopen") || !gfx.display.rgb() || !gfx.display.puppet()) return false;
	const auto visible = gfx.puppetSystem.current;
	PuppetView expected; rgba.resize(512 * 384 * 4); memset(rgba.data(), 0, rgba.size()); for (uint32 i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
	Bytes expectedScreen;
	if (!expected.composite(visible, rgba, &worldFrame, world.palette) || !Display::scaleRGBA(rgba, 512, 384, expectedScreen) ||
		!PuppetView::overlay(*visible, expectedScreen, *font, gfx.puppetParams, true, false, nullptr, screenFont)) return false;
	rgba = expectedScreen;
	if (!compareScreen()) { warning("PUPPET DISPLAY FAIL native text scaled art/backdrop"); return false; }
	const Bytes original = gfx.display.picture();
	if (!runDisplay("displayblack") || !screenPixel(0, 0, 0, 0, 0) || !screenPixel(1, 2, 0, 0, 0) || gfx.display.picture() != original) return false;
	if (!runDisplay("displayreveal") || !compareScreen()) return false;
	if (!runDisplay("displaymix") || !screenPixel(2, 2, 0, 0, 0) || !screenPixel(0, 0, 91, 42, 17)) { warning("PUPPET DISPLAY FAIL independent character palette"); return false; }
	if (!runDisplay("displayreset") || !compareScreen()) return false;
	if (!runDisplay("displaydimroom") || !screenPixel(0, 0, 0, 0, 0) || !screenPixel(2, 2, 5, 6, 7)) { warning("PUPPET DISPLAY FAIL independent room palette"); return false; }
	if (!runDisplay("displayresetroom") || !compareScreen()) return false;
	if (!runDisplay("displayhide") || gfx.display.puppet() || !screenPixel(64, 108, 91, 42, 17)) return false;
	if (!runDisplay("displayshow") || !compareScreen()) return false;
	if (!runDisplay("displayfade") || !screenPixel(0, 0, 0, 0, 0) || gfx.display.picture() != original) return false;
	if (!runDisplay("displaymovie") || gfx.display.puppet() || gfx.movies.displayedFrames != 5 || !screenPixel(4, 5, 25, 230, 25)) { warning("PUPPET DISPLAY FAIL RGB v4 movie"); return false; }
	const Bytes moviePicture = gfx.display.picture();
	if (!runDisplay("displayfade") || !screenPixel(4, 5, 0, 0, 0) || gfx.display.picture() != moviePicture) return false;
	if (!runDisplay("displayreveal") || !screenPixel(4, 5, 25, 230, 25)) { warning("PUPPET DISPLAY FAIL last movie frame fade/reveal"); return false; }
	if (!runDisplay("displaydustmovie") || gfx.movies.displayedFrames != 3 || !screenPixel(0, 0, 43, 212, 43)) { warning("PUPPET DISPLAY FAIL RGB v1 movie"); return false; }
	if (!runDisplay("displayforce") || !compareScreen()) return false;
	if (!runDisplay("displayclose") || gfx.display.puppet() || !screenPixel(64, 108, 91, 42, 17)) return false;
	warning("PUPPET DISPLAY PASS native RGB exact canvas scaling palettes dim restore black hold fades movie snapshot v1/v4 close");
	if (!runDisplay("displayportrait")) return false;
	const auto actual = gfx.puppetSystem.current;
	int32 spokenLine = -1;
	for (uint32 i = 0; i < actual->data.lines.size(); ++i) {
		const PuppetLine &line = actual->data.lines[i]; AudioClip clip;
		if (line.text.empty() || line.text[0] == '*' || line.ident.hasPrefixIgnoreCase("idle ")) continue;
		if (decodeAudio(actual->data.archive.get(line.audio), clip) && clip.rate && !clip.samples.empty() && clip.samples.size() <= clip.rate * 5) { spokenLine = i; break; }
	}
	if (spokenLine < 0) { warning("PUPPET DISPLAY FAIL no short authored line"); return false; }
	Bytes capture;
	class Capture : public Common::EventSource {
	public:
		OSystem *backend; Session &session; Bytes &pixels; bool heard = false;
		Capture(OSystem *b, Session &s, Bytes &p) : backend(b), session(s), pixels(p) { b->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~Capture() { backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &) override {
			if (!pixels.empty() || !session.puppetSpeech.active()) return false;
			heard = !session.sound.done(true);
			Graphics::Surface *surface = backend->lockScreen(); if (!surface) return false;
			pixels.resize(640 * 480 * 4);
			for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
				byte *p = pixels.data() + (y * 640 + x) * 4; surface->format.colorToRGB(surface->getPixel(x, y), p[0], p[1], p[2]); p[3] = 255;
			}
			backend->unlockScreen(); return false;
		}
	};
	{
		Capture observer(backend, gfx, capture);
		Values args; args.push_back(Value(actual->data.lines[spokenLine].ident));
		const ScriptResult r = gfx.runtime.run(test, "actualspeak", args);
		if (r.value.number() != 1 || !gfx.runtime.error.empty() || !observer.heard || capture.empty() || !gfx.sound.done(true)) { warning("PUPPET DISPLAY FAIL authored speech %s", gfx.runtime.error.c_str()); return false; }
	}
	Common::DumpFile snapshot; if (!snapshot.open(root.getChild("display-preview.rgba")) || snapshot.write(capture.data(), capture.size()) != capture.size()) return false;
	warning("PUPPET AUTHORED HOST PASS real portrait speech animation caption choices native RGB capture");
	const bool hadJournal = ConfMan.hasKey("dreamfactory_journal", Common::ConfigManager::kTransientDomain);
	const Common::String oldJournal = hadJournal ? ConfMan.get("dreamfactory_journal", Common::ConfigManager::kTransientDomain) : Common::String();
	ConfMan.setBool("dreamfactory_journal", true, Common::ConfigManager::kTransientDomain);
	Session journalSession(backend);
	if (!journalSession.init(root) || !journalSession.puppetSystem.open("speech")) return false;
	RouteTestFiles routeFiles;
	if (!journalSession.routes.open(&routeFiles, "conversation-test", "fixture", error)) return false;
	class JournalSource : public Common::EventSource {
	public:
		OSystem *backend; Session &session; Common::FSNode root; uint32 phase = 0, start = 0; uint64 logicalStart = 0;
		JournalSource(OSystem *b, Session &s, const Common::FSNode &r) : backend(b), session(s), root(r) { backend->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~JournalSource() { backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			if (!phase && session.puppetSpeech.active()) {
				start = backend->getMillis(); logicalStart = session.elapsedMillis(); ++phase; event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kOpenJournal; return true;
			}
			const uint32 elapsed = backend->getMillis() - start;
			if (phase && elapsed > 5000) { event.type = Common::EVENT_QUIT; return true; }
			if (phase >= 1 && phase <= 3 && session.dialogActive() && elapsed >= phase * 250) {
				if (phase < 3) {
					Graphics::Surface overlay; overlay.create(backend->getOverlayWidth(), backend->getOverlayHeight(), backend->getOverlayFormat());
					backend->grabOverlay(overlay);
					Common::DumpFile preview;
					if (preview.open(root.getChild(phase == 1 ? "journal-preview.bmp" : "journal-detail.bmp"))) Image::writeBMP(preview, overlay);
					overlay.free();
				}
				event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = phase++ == 1 ? Common::KEYCODE_RETURN : Common::KEYCODE_ESCAPE; return true;
			}
			return false;
		}
	};
	const uint32 journalStarted = backend->getMillis();
	{
		JournalSource source(backend, journalSession, root);
		if (journalSession.runtime.run(test, "speak").value.number() != 42 || source.phase != 4 || backend->getMillis() - journalStarted < 750 || journalSession.elapsedMillis() - source.logicalStart > 600) return false;
	}
	if (journalSession.journal.entries.size() != 1 || journalSession.journal.entries[0].text != "Hello there" || !journalSession.journal.entries[0].file.equalsIgnoreCase("flat/speech.pup")) return false;
	Common::SharedPtr<AudioClip> replay;
	const auto owner = journalSession.puppetSystem.current;
	const uint32 queueSize = owner->voiceQueue.size();
	if (!journalSession.journalAudio(journalSession.journal.entries[0], replay) || replay->samples.size() != 200 || journalSession.puppetSystem.current != owner || owner->voiceQueue.size() != queueSize || journalSession.journal.entries.size() != 1) return false;
	Common::Event journalChoice; journalChoice.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; journalChoice.customType = kPadUp;
	backend->getEventManager()->pushEvent(journalChoice); journalChoice.customType = kPadInteract; backend->getEventManager()->pushEvent(journalChoice);
	if (journalSession.runtime.run(test, "choices").value.number() != 902 || journalSession.journal.entries.size() != 2) return false;
	const JournalEntry &decision = journalSession.journal.entries[1];
	if (decision.selected != 1 || decision.choices.size() != 2 || decision.text != "answer") return false;
	RouteStore discovered;
	if (!discovered.open(&routeFiles, "conversation-test", "fixture", error) || discovered.graph.nodes().size() != 2 || discovered.graph.node(2)->entry.choices != decision.choices || discovered.graph.node(2)->seenChoices != 2) return false;
	warning("ROUTE CAPTURE PASS heard speech starts a scene actual chosen reply discovery persisted across restart");
	Session routeSession(backend); RouteTestFiles checkpointFiles;
	if (!routeSession.init(root) || !routeSession.stageSystem.open("route", nullptr, false) || !routeSession.routes.open(&checkpointFiles, "input-test", "fixture", error)) { warning("ROUTE CHECKPOINT FAIL init %s %s", error.c_str(), routeSession.runtime.error.c_str()); return false; }
	Common::Event routeClick; routeClick.type = Common::EVENT_LBUTTONDOWN; routeClick.mouse = Common::Point(100, 100);
	routeSession.postEvent(routeClick);
	if (!routeSession.step() || routeSession.runtime.globals["routecounter"].number() != 1 || routeSession.routes.graph.nodes().size() != 1 || !routeSession.routes.graph.node(1)->checkpoint) { warning("ROUTE CHECKPOINT FAIL capture nodes=%u counter=%d error=%s", routeSession.routes.graph.nodes().size(), routeSession.runtime.globals["routecounter"].number(), routeSession.runtime.error.c_str()); return false; }
	if (!routeSession.restoreRouteScene(1, error) || routeSession.runtime.globals["routecounter"].number() != 0 || routeSession.pointerDown) { warning("ROUTE CHECKPOINT FAIL restore %s", error.c_str()); return false; }
	routeSession.postEvent(routeClick);
	if (!routeSession.step() || routeSession.runtime.globals["routecounter"].number() != 1 || routeSession.routes.graph.nodes().size() != 1) { warning("ROUTE CHECKPOINT FAIL revisit nodes=%u error=%s", routeSession.routes.graph.nodes().size(), routeSession.runtime.error.c_str()); return false; }
	warning("ROUTE CHECKPOINT PASS pre-gesture native snapshot restores prior authored state replay reuses discovered scene without duplicating nodes");
	Session multiSession(backend); RouteTestFiles multiFiles;
	if (!multiSession.init(root) || !multiSession.stageSystem.open("route", nullptr, false) || !multiSession.routes.open(&multiFiles, "multi-test", "fixture", error)) return false;
	multiSession.runtime.globals["multiroute"] = Value(1); multiSession.postEvent(routeClick);
	if (!multiSession.step() || multiSession.routes.graph.nodes().size() != 2 || multiSession.routes.graph.node(2)->checkpoint || multiSession.routes.graph.checkpointFor(2) != 1) return false;
	if (!multiSession.restoreRouteScene(2, error) || multiSession.runtime.globals["routecounter"].number()) return false;
	multiSession.postEvent(routeClick);
	if (!multiSession.step() || multiSession.routes.graph.nodes().size() != 2 || multiSession.runtime.globals["routecounter"].number() != 1 || multiSession.routes.graph.cursor() != 2) return false;
	warning("ROUTE ENCOUNTER PASS later character inherits encounter checkpoint replay restores first speaker and reuses all discovered scene nodes");
	ConfMan.setBool("dreamfactory_journal", false, Common::ConfigManager::kTransientDomain);
	if (journalSession.runtime.run(test, "speak").value.number() != 42 || journalSession.journal.entries.size() != 2) return false;
	if (hadJournal) ConfMan.set("dreamfactory_journal", oldJournal, Common::ConfigManager::kTransientDomain);
	else ConfMan.removeKey("dreamfactory_journal", Common::ConfigManager::kTransientDomain);
	// Exercise the reader with wrapped text, a decision, bookmarks and an empty search.
	Session journalLayout(backend);
	if (!journalLayout.init(root)) return false;
	for (uint32 i = 0; i < 6; ++i) {
		JournalEntry entry;
		entry.speaker = i % 2 ? "Passenger" : "Officer";
		entry.room = "Journal layout fixture"; entry.time = 123000 + i * 60000;
		entry.text = "This is a long journal entry used to check line wrapping and reading space. The complete conversation should remain easy to read without truncation.";
		journalLayout.journal.add(entry);
	}
	JournalEntry fork; fork.speaker = "Passenger"; fork.room = "Journal layout fixture"; fork.time = 510000;
	fork.choices.push_back("Ask about the ship and the people aboard.");
	fork.choices.push_back("Ask for directions to the grand staircase.");
	fork.selected = 1; fork.text = fork.choices[1];
	fork.file = journalSession.journal.entries[0].file; fork.ident = journalSession.journal.entries[0].ident;
	journalLayout.journal.add(fork);
	class LayoutSource : public Common::EventSource {
		OSystem *_backend; Common::FSNode _root; uint32 _start; bool _timedOut = false;
	public:
		uint32 phase = 0; bool mapped = false, replayMapped = false, closeMapped = false;
		LayoutSource(OSystem *backend, const Common::FSNode &root) : _backend(backend), _root(root), _start(backend->getMillis()) { backend->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~LayoutSource() { _backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			const uint32 elapsed = _backend->getMillis() - _start;
			if (elapsed > 5000 && !_timedOut) { _timedOut = true; event.type = Common::EVENT_QUIT; return true; }
			if (phase > 12 || elapsed < (phase + 1) * 120) return false;
			const int w = MIN<int>(760, _backend->getOverlayWidth() - 12), h = MIN<int>(560, _backend->getOverlayHeight() - 12);
			const int x = (_backend->getOverlayWidth() - w) / 2, y = (_backend->getOverlayHeight() - h) / 2;
			const int row = MAX(24, g_gui.getFont().getFontHeight() + 12);
			if (phase == 0 || phase == 5 || phase == 4 || phase == 10) {
				Graphics::Surface overlay; overlay.create(_backend->getOverlayWidth(), _backend->getOverlayHeight(), _backend->getOverlayFormat()); _backend->grabOverlay(overlay);
				Common::DumpFile image;
				Common::String name = phase == 0 ? "journal-layout.bmp" : phase == 5 ? "journal-starred.bmp" : phase == 4 ? "journal-decisions.bmp" : "journal-search-empty.bmp";
				if (_backend->getOverlayWidth() < 600) name = "compact-" + name;
				if (image.open(_root.getChild(name))) Image::writeBMP(image, overlay);
				overlay.free();
			}
			if (phase == 0) {
				Common::Event physical; physical.type = Common::EVENT_JOYBUTTON_DOWN; physical.joystick.button = Common::JOYSTICK_BUTTON_X;
				Common::List<Common::Event> actions;
				_backend->getEventManager()->getKeymapper()->mapEvent(physical, actions);
				mapped = actions.size() == 1 && actions.front().type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && actions.front().customType == kJournalStar;
				event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kJournalStar;
			} else if (phase == 1) {
				Common::Event physical; physical.type = Common::EVENT_JOYBUTTON_DOWN; physical.joystick.button = Common::JOYSTICK_BUTTON_A;
				Common::List<Common::Event> actions;
				_backend->getEventManager()->getKeymapper()->mapEvent(physical, actions);
				replayMapped = actions.size() == 1 && actions.front().type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && actions.front().customType == kJournalReplay;
				event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kJournalReplay;
			} else if (phase == 2 || phase == 4) { event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kJournalViewNext; }
			else if (phase == 12) {
				Common::Event physical; physical.type = Common::EVENT_JOYBUTTON_DOWN; physical.joystick.button = Common::JOYSTICK_BUTTON_B;
				Common::List<Common::Event> actions;
				_backend->getEventManager()->getKeymapper()->mapEvent(physical, actions);
				closeMapped = actions.size() == 1 && actions.front().type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && actions.front().customType == kJournalClose;
				event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kJournalClose;
			} else if (phase == 11) { event.type = Common::EVENT_MOUSEMOVE; event.mouse = Common::Point(x + 100, y + row * 2 + 16 + row / 2); }
			else if (phase == 8 || phase == 9) { event.type = phase == 8 ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP; event.kbd.keycode = Common::KEYCODE_z; event.kbd.ascii = 'z'; }
			else {
				event.type = phase == 10 ? Common::EVENT_MOUSEMOVE : phase % 2 ? Common::EVENT_LBUTTONUP : Common::EVENT_LBUTTONDOWN;
				if (phase <= 1) event.mouse = Common::Point(x + 12 + 2 * (w - 20) / 4 + 20, y + h - 20);
				else if (phase <= 3) event.mouse = Common::Point(x + 12 + 2 * (w - 20) / 3 + 20, y + row + 12 + row / 2);
				else if (phase <= 5) event.mouse = Common::Point(x + 12 + (w - 20) / 3 + 20, y + row + 12 + row / 2);
				else event.mouse = Common::Point(x + 100, y + row * 2 + 16 + row / 2);
			}
			++phase; return true;
		}
	};
	class ControllerInputs {
		OSystem *_backend;
	public:
		explicit ControllerInputs(OSystem *backend) : _backend(backend) {
			auto *inputs = new Common::CompositeHardwareInputSet();
			if (auto *native = backend->getHardwareInputSet()) inputs->addHardwareInputSet(native);
			inputs->addHardwareInputSet(new Common::JoystickHardwareInputSet(Common::defaultJoystickButtons, Common::defaultJoystickAxes));
			backend->getEventManager()->getKeymapper()->registerHardwareInputSet(inputs, backend->getKeymapperDefaultBindings());
		}
		~ControllerInputs() { _backend->getEventManager()->getKeymapper()->registerHardwareInputSet(_backend->getHardwareInputSet(), _backend->getKeymapperDefaultBindings()); }
	};
	{
		ControllerInputs controller(backend);
		LayoutSource source(backend, root);
		openJournal(journalLayout);
		if (source.phase != 13 || !source.mapped || !source.replayMapped || !source.closeMapped || !journalLayout.journal.entries.back().bookmarked || journalLayout.journal.entries.size() != 7) {
			warning("JOURNAL LAYOUT FAIL phase=%u mapped=%d bookmarked=%d", source.phase, source.mapped, journalLayout.journal.entries.back().bookmarked); return false;
		}
	}
	// Run the same interaction path at a handheld-sized game surface.
	const uint oldWidth = backend->getWidth(), oldHeight = backend->getHeight();
	const Graphics::PixelFormat oldFormat = backend->getScreenFormat();
	backend->beginGFXTransaction(); backend->initSize(480, 272, &oldFormat);
	if (backend->endGFXTransaction() != OSystem::kTransactionSuccess) return false;
	journalLayout.journal.entries.back().bookmarked = false;
	bool compactOK = false;
	{
		ControllerInputs controller(backend);
		LayoutSource source(backend, root);
		openJournal(journalLayout);
		compactOK = source.phase == 13 && source.mapped && source.replayMapped && source.closeMapped && journalLayout.journal.entries.back().bookmarked;
	}
	backend->beginGFXTransaction(); backend->initSize(oldWidth, oldHeight, &oldFormat);
	if (backend->endGFXTransaction() != OSystem::kTransactionSuccess || !compactOK) return false;
	JournalEntry routeDecision; routeDecision.speaker = "Officer"; routeDecision.room = "Fixture corridor"; routeDecision.selected = 0;
	routeDecision.choices.push_back("Ask about the ship"); routeDecision.choices.push_back("Ask about the passenger");
	routeSession.routes.graph.recordDecision(routeDecision);
	JournalEntry continuation; continuation.speaker = "Officer"; continuation.room = "Fixture corridor"; continuation.text = "A continuation you have discovered.";
	routeSession.routes.graph.recordScene(continuation);
	if (!routeSession.routes.save(error)) return false;
	class RouteUISource : public Common::EventSource {
		OSystem *_backend; Common::FSNode _root; uint32 _start; bool _compact, _failure, _timedOut = false;
		uint32 _gesture = 0;
		Common::String _beforeDrag;
	public:
		uint32 phase = 0;
		bool dragChanged = false;
		RouteUISource(OSystem *backend, const Common::FSNode &root, bool compact, bool failure = false) : _backend(backend), _root(root), _start(backend->getMillis()), _compact(compact), _failure(failure) { backend->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~RouteUISource() { _backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			const uint32 elapsed = _backend->getMillis() - _start;
			if (elapsed > 6000 && !_timedOut) { _timedOut = true; event.type = Common::EVENT_QUIT; return true; }
			if (phase == 1 && _gesture < 3) {
				if (elapsed < (2 + _gesture) * 150) return false;
				if (!_gesture) {
					Graphics::Surface overlay; overlay.create(_backend->getOverlayWidth(), _backend->getOverlayHeight(), _backend->getOverlayFormat()); _backend->grabOverlay(overlay);
					Common::MemoryReadStream pixels(static_cast<const byte *>(overlay.getPixels()), overlay.pitch * overlay.h); _beforeDrag = Common::computeStreamMD5AsString(pixels); overlay.free();
				}
				const int w = MIN<int>(760, _backend->getOverlayWidth() - 12), h = MIN<int>(560, _backend->getOverlayHeight() - 12), row = MAX(24, g_gui.getFont().getFontHeight() + 12);
				event.type = _gesture == 0 ? Common::EVENT_LBUTTONDOWN : _gesture == 1 ? Common::EVENT_MOUSEMOVE : Common::EVENT_LBUTTONUP;
				event.mouse = Common::Point((_backend->getOverlayWidth() - w) / 2 + 18, (_backend->getOverlayHeight() - h) / 2 + (_compact ? row + 16 : row * 3 + 24) + (_gesture ? 30 : 70));
				++_gesture; return true;
			}
			const uint32 commands[] = {kJournalViewPrevious, kJournalDown, kJournalDown, kJournalRight, kJournalReplay, kJournalClose, kJournalStar, kJournalUp, kJournalStar, kJournalClose, kJournalRead, kJournalStar, kJournalClose, kJournalStar, kJournalReplay};
			if (phase >= ARRAYSIZE(commands) + (_failure ? 1 : 0) || elapsed < (phase + 1 + (phase ? 3 : 0)) * 150) return false;
			if (phase == 1 || phase == 2 || phase == 4 || phase == 5 || phase == 9 || phase == 11) {
				const char *names[] = {"", "routes-map", "routes-decision", "", "routes-locked", "routes-locked-detail", "", "", "", "routes-confirm", "", "routes-detail"};
				Graphics::Surface overlay; overlay.create(_backend->getOverlayWidth(), _backend->getOverlayHeight(), _backend->getOverlayFormat()); _backend->grabOverlay(overlay);
				if (phase == 1) { Common::MemoryReadStream pixels(static_cast<const byte *>(overlay.getPixels()), overlay.pitch * overlay.h); dragChanged = _beforeDrag != Common::computeStreamMD5AsString(pixels); }
				Common::DumpFile image;
				if (image.open(_root.getChild(Common::String(_compact ? "compact-" : "") + names[phase] + ".bmp"))) Image::writeBMP(image, overlay);
				overlay.free();
			}
			event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = phase < ARRAYSIZE(commands) ? commands[phase] : kJournalClose; ++phase; return true;
		}
	};
	for (uint32 compact = 0; compact < 2; ++compact) {
		backend->beginGFXTransaction(); backend->initSize(compact ? 480 : oldWidth, compact ? 272 : oldHeight, &oldFormat);
		if (backend->endGFXTransaction() != OSystem::kTransactionSuccess || !routeSession.routes.graph.restoreCursor(1, -1)) return false;
		ControllerInputs controller(backend); RouteUISource source(backend, root, compact);
		Common::Event journalEvent; journalEvent.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; journalEvent.customType = kOpenJournal; routeSession.postEvent(journalEvent);
		if (!routeSession.step() || source.phase != 15 || (compact && !source.dragChanged) || routeSession.runtime.globals["routecounter"].number() || routeSession.routes.graph.cursor() != 1 || routeSession.routes.graph.nodes().size() != 3) {
			warning("ROUTE UI FAIL compact=%u phase=%u counter=%d cursor=%u nodes=%u error=%s", compact, source.phase, routeSession.runtime.globals["routecounter"].number(), routeSession.routes.graph.cursor(), routeSession.routes.graph.nodes().size(), routeSession.runtime.error.c_str()); return false;
		}
	}
	backend->beginGFXTransaction(); backend->initSize(oldWidth, oldHeight, &oldFormat);
	if (backend->endGFXTransaction() != OSystem::kTransactionSuccess) return false;
	{
		if (!routeSession.puppetSystem.open("speech")) return false;
		ControllerInputs controller(backend); RouteUISource source(backend, root, false);
		Common::Event journalEvent; journalEvent.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; journalEvent.customType = kOpenJournal; routeSession.postEvent(journalEvent);
		const ScriptResult restored = routeSession.runtime.run(test, "speak");
		if (restored.signal != ScriptResult::kAbandoned || source.phase != 15 || routeSession.puppetSystem.current || routeSession.puppetSpeech.active() || !routeSession.runtime.error.empty() || routeSession.routes.graph.nodes().size() != 3 || routeSession.routes.graph.cursor() != 1) {
			warning("ROUTE UI FAIL restore during speech phase=%u signal=%d error=%s", source.phase, restored.signal, routeSession.runtime.error.c_str()); return false;
		}
	}
	{
		checkpointFiles.files[checkpointFiles.listSavefiles("*.checkpoint")[0]].back() ^= 1;
		routeSession.runtime.globals["routecounter"] = Value(99);
		ControllerInputs controller(backend); RouteUISource source(backend, root, false, true);
		Common::Event journalEvent; journalEvent.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; journalEvent.customType = kOpenJournal; routeSession.postEvent(journalEvent);
		if (!routeSession.step() || source.phase != 16 || routeSession.runtime.globals["routecounter"].number() != 99 || routeSession.routes.graph.cursor() != 1) { warning("ROUTE UI FAIL corrupt restore phase=%u", source.phase); return false; }
	}
	warning("ROUTE UI PASS connected map desktop handheld drag pan controller branches locked outcome details unavailable revisit ignored confirmation cancel confirm safe post-dialog restore active speech script abandoned cumulative discoveries retained corrupt checkpoint notice dismissed with controller running state intact");
	Session missingVoice(backend);
	JournalEntry unavailable; unavailable.speaker = "Fixture"; unavailable.text = "Unavailable voice"; unavailable.file = "missing.pup"; unavailable.ident = "line";
	missingVoice.journal.add(unavailable);
	class NoticeSource : public Common::EventSource {
		OSystem *_backend; uint32 _start; bool _timedOut = false;
	public:
		uint32 phase = 0;
		explicit NoticeSource(OSystem *backend) : _backend(backend), _start(backend->getMillis()) { backend->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~NoticeSource() { _backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			const uint32 elapsed = _backend->getMillis() - _start;
			if (elapsed > 3000 && !_timedOut) { _timedOut = true; event.type = Common::EVENT_QUIT; return true; }
			if (phase >= 3 || elapsed < (phase + 1) * 150) return false;
			event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = phase++ ? kJournalClose : kJournalReplay; return true;
		}
	};
	{
		NoticeSource source(backend); openJournal(missingVoice);
		if (source.phase != 3 || backend->getEventManager()->shouldQuit() || missingVoice.journal.entries.size() != 1) return false;
	}
	const auto *journalMap = backend->getEventManager()->getKeymapper()->getKeymap("dreamfactory-journal-ui");
	const auto *standardMap = backend->getEventManager()->getKeymapper()->getKeymap(Common::kGuiKeymapName);
	if (!journalMap || journalMap->isEnabled() || !standardMap || !standardMap->isEnabled()) return false;
	warning("JOURNAL PASS heard lines independent replay pause modal reading recording toggle wrapped transcript scoped controller keymap");
	return true;
}
bool auditPuppets(const Common::FSNode &root) {
	Common::FSList files; if (!corpusFiles(root, files)) return false; uint32 total = 0, failures = 0;
	for (uint32 f = 0; f < files.size(); ++f) {
		const Common::String name = label(files[f], root); if (!name.hasSuffixIgnoreCase(".pup")) continue;
		Bytes bytes; Puppet pup;
		if (!read(files[f], bytes)) { ++failures; continue; }
		if (bytes.size() >= 32 && READ_BE_UINT32(bytes.data() + 4) == bytes.size() && READ_LE_UINT32(bytes.data() + 4) != bytes.size()) { warning("PUPPET SKIP %s big-endian", name.c_str()); continue; }
		if (!pup.load(bytes)) { warning("PUPPET FAIL %s reader", name.c_str()); ++failures; continue; }
		AuditHash meta, pixels, audio; uint32 frames = 0, poses = 0, samples = 0;
		meta.number(pup.version); meta.text(pup.name); meta.number(pup.band);
		for (uint32 i = 0; i < 768; ++i) meta.byteValue(pup.palette[i]);
		for (uint32 i = 0; i < 4; ++i) { meta.number(pup.idleMin[i]); meta.number(pup.idleMax[i]); }
		for (uint32 s = 0; s < pup.scripts.size(); ++s) {
			meta.text(pup.scripts[s].name); meta.number(pup.scripts[s].location);
			Script script; Common::String error; if (!loadScript(pup.archive.get(pup.scripts[s].location), script, error)) { warning("PUPPET FAIL %s script %s", name.c_str(), error.c_str()); ++failures; }
		}
		auto sprite = [&](uint32 ref) {
			if (!ref || !pup.archive.has(ref)) { pixels.number(0); return; }
			Sprite image; if (!decodeSprite(pup.archive.get(ref), image)) { warning("PUPPET FAIL %s sprite=%u", name.c_str(), ref); ++failures; return; }
			++frames; pixels.number(image.width); pixels.number(image.height); pixels.number(image.y); pixels.number(image.x);
			for (uint32 p = 0; p < image.pixels.size(); ++p) pixels.byteValue(image.pixels[p]); for (uint32 p = 0; p < image.opaque.size(); ++p) pixels.byteValue(image.opaque[p]);
		};
		sprite(pup.band);
		for (uint32 s = 0; s < pup.stances.size(); ++s) {
			meta.number(pup.stances[s].location);
			for (uint32 l = 0; l < 11; ++l) { const PuppetLayer &layer = pup.stances[s].layers[l]; meta.number(layer.y); meta.number(layer.x); meta.number(layer.frames.size()); for (uint32 r = 0; r < layer.frames.size(); ++r) { meta.number(layer.frames[r]); sprite(layer.frames[r]); } }
		}
		for (uint32 l = 0; l < pup.lines.size(); ++l) {
			const PuppetLine &line = pup.lines[l]; meta.text(line.ident); meta.text(line.text); meta.number(line.stance); meta.number(line.audio); meta.number(line.animation); meta.number(line.record);
			Common::Array<PuppetPose> anim; if (!pup.animation(line.animation, anim)) { warning("PUPPET FAIL %s animation=%u", name.c_str(), line.animation); ++failures; }
			meta.number(anim.size()); poses += anim.size(); for (uint32 p = 0; p < anim.size(); ++p) for (uint32 layer = 0; layer < 11; ++layer) { meta.number(anim[p].layers[layer].frame); meta.number(anim[p].layers[layer].y); meta.number(anim[p].layers[layer].x); }
			if (!line.audio || !pup.archive.has(line.audio)) { audio.number(0); continue; }
			AudioClip clip; if (!decodeAudio(pup.archive.get(line.audio), clip)) { warning("PUPPET FAIL %s audio=%u", name.c_str(), line.audio); ++failures; continue; }
			audio.number(clip.rate); audio.number(clip.samples.size()); samples += clip.samples.size(); for (uint32 p = 0; p < clip.samples.size(); ++p) { audio.byteValue(byte(clip.samples[p])); audio.byteValue(byte(uint16(clip.samples[p]) >> 8)); }
		}
		AuditHash rendered; uint32 pictures = 0;
		Common::SharedPtr<LoadedPuppet> loaded(new LoadedPuppet()); loaded->data = pup; loaded->hasPose = true;
		PuppetView painter;
		for (uint32 stance = 0; stance < pup.stances.size(); ++stance) {
			const PuppetLine *line = nullptr;
			for (uint32 i = 0; i < pup.lines.size(); ++i) if (pup.lines[i].stance == int32(stance)) { line = &pup.lines[i]; break; }
			if (!line) continue;
			Common::Array<PuppetPose> stancePoses; if (!pup.animation(line->animation, stancePoses) || stancePoses.empty()) continue;
			for (uint32 endpoint = 0; endpoint < 2; ++endpoint) for (uint32 caption = 0; caption < 2; ++caption) {
				Frame backdrop; backdrop.width = 512; backdrop.height = 264; backdrop.pixels.resize(512 * 264);
				for (uint32 y = 0; y < 264; ++y) for (uint32 x = 0; x < 512; ++x) backdrop.pixels[y * 512 + x] = byte((x >> 5) + (y >> 4) + pictures * 17);
				byte palette[768]; for (uint32 i = 0; i < 256; ++i) for (uint32 c = 0; c < 3; ++c) palette[i * 3 + c] = byte(i * 3 + c * 61 + pictures * 13);
				Bytes rgba; rgba.resize(512 * 384 * 4); for (uint32 i = 0; i < rgba.size(); i += 4) { rgba[i] = 17; rgba[i + 1] = 31; rgba[i + 2] = 47; rgba[i + 3] = 255; }
				loaded->stance = stance; loaded->pose = endpoint ? stancePoses.back() : stancePoses[0]; loaded->subtitle = caption ? "caption" : "";
				if (!painter.composite(loaded, rgba, &backdrop, palette)) { warning("PUPPET FAIL %s composition", name.c_str()); ++failures; }
				for (uint32 i = 0; i < rgba.size(); ++i) rendered.byteValue(rgba[i]); ++pictures;
			}
		}
		warning("PUPPETART %s pictures=%u rgba=%08x", name.c_str(), pictures, rendered.hash);
		warning("PUPPET %s lines=%u scripts=%u stances=%u frames=%u poses=%u samples=%u meta=%08x pixels=%08x audio=%08x", name.c_str(), pup.lines.size(), pup.scripts.size(), pup.stances.size(), frames, poses, samples, meta.hash, pixels.hash, audio.hash); ++total;
	}
	warning("PUPPET %s files=%u failures=%u", failures ? "FAIL" : "PASS", total, failures); return !failures;
}
bool auditScreens(const Common::FSNode &root, OSystem *backend) {
	Session session(backend); Bytes bytes; ScriptInstance test; test.name = "screen-test"; Common::String error;
	if (!backend || !session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("SCREEN FAIL init %s", error.c_str()); return false; }
	session.roomSystem.current.reset(new LoadedRoom()); session.stageSystem.current.reset(new LoadedStage());
	byte set[768], stage[768], shown[768];
	for (uint32 i = 0; i < 768; ++i) { set[i] = byte(i); stage[i] = byte(255 - byte(i)); }
	memcpy(session.roomSystem.current->data.palette, set, 768); memcpy(session.stageSystem.current->data.palette, stage, 768);
	session.runtime.run(test, "source"); backend->getPaletteManager()->grabPalette(shown, 0, 256);
	if (memcmp(shown, set, 768) || memcmp(session.displayPalette, set, 768)) { warning("SCREEN PALETTE FAIL named set"); return false; }
	session.runtime.run(test, "dim"); backend->getPaletteManager()->grabPalette(shown, 0, 256);
	for (uint32 i = 0; i < 768; ++i) if (shown[i] != (i >= 3 && i < 9 ? set[i] * 127 / 255 : set[i])) { warning("SCREEN PALETTE FAIL range mix"); return false; }
	session.runtime.run(test, "dark"); backend->getPaletteManager()->grabPalette(shown, 0, 256);
	for (uint32 i = 0; i < 768; ++i) if (shown[i]) { warning("SCREEN PALETTE FAIL dark endpoint"); return false; }
	if (memcmp(session.displayPalette, set, 768)) { warning("SCREEN PALETTE FAIL black overwrote source"); return false; }
	session.crickets.make("missing", 0, 0, 1, 5, 0); session.scheduler.make("actor", "missing", "tick", 5);
	const int64 cricketCount = session.crickets.crickets[0]->count;
	const uint32 beforeFrames = session.scheduler.frame, start = backend->getMillis();
	Common::Event mouse; mouse.type = Common::EVENT_MOUSEMOVE; mouse.mouse = Common::Point(21, 34); backend->getEventManager()->pushEvent(mouse);
	const ScriptResult faded = session.runtime.run(test, "fades"); const uint32 elapsed = backend->getMillis() - start;
	backend->getPaletteManager()->grabPalette(shown, 0, 256);
	if (faded.value.number() != 12345 || !session.runtime.error.empty() || elapsed < 180 || elapsed > 2000 || session.screenBlack || memcmp(shown, set, 768)) { warning("SCREEN TIMING FAIL elapsed=%u error=%s", elapsed, session.runtime.error.c_str()); return false; }
	if (session.scheduler.frame != beforeFrames || session.scheduler.loops[0]->count != 5 || session.crickets.crickets[0]->count != cricketCount) { warning("SCREEN TIMING FAIL scheduler ran in fade"); return false; }
	bool queued = false; for (uint32 i = 0; i < session.pendingEvents.size(); ++i) if (session.pendingEvents[i].type == Common::EVENT_MOUSEMOVE && session.pendingEvents[i].mouse == mouse.mouse) queued = true;
	if (!queued) { warning("SCREEN TIMING FAIL lost input"); return false; }
	Session virtualSession; if (!virtualSession.init(root)) return false;
	virtualSession.roomSystem.current = session.roomSystem.current;
	if (virtualSession.runtime.run(test, "fades").value.number() != 12345 || virtualSession.elapsedMillis() != 200 || virtualSession.scheduler.frame) { warning("SCREEN TIMING FAIL virtual clock"); return false; }
	warning("SCREEN TIMING PASS elapsed=%u blocking 60Hz ramps no loops or crickets deferred input virtual clock", elapsed);
	session.runtime.run(test, "stage"); backend->getPaletteManager()->grabPalette(shown, 0, 256);
	if (memcmp(shown, stage, 768)) { warning("SCREEN PALETTE FAIL named stage"); return false; }
	Graphics::Surface *screen = backend->lockScreen(); if (!screen) return false;
	screen->fillRect(Common::Rect(screen->w, screen->h), 7); backend->unlockScreen();
	session.runtime.run(test, "black"); backend->getPaletteManager()->grabPalette(shown, 0, 256);
	session.runtime.run(test, "canceltransition");
	if (!session.screenBlack || !session.runtime.error.empty()) { warning("SCREEN PALETTE FAIL plain lifted black"); return false; }
	screen = backend->lockScreen(); if (!screen) return false;
	const byte retained = *static_cast<const byte *>(screen->getBasePtr(0, 0)); backend->unlockScreen();
	for (uint32 i = 0; i < 768; ++i) if (shown[i]) return false;
	if (retained != 7 || memcmp(session.displayPalette, stage, 768)) { warning("SCREEN PALETTE FAIL black clut erased pixels or source"); return false; }
	session.runtime.run(test, "clear"); screen = backend->lockScreen(); if (!screen) return false;
	const byte cleared = *static_cast<const byte *>(screen->getBasePtr(0, 0)); backend->unlockScreen();
	if (cleared) { warning("SCREEN PALETTE FAIL blackscreen"); return false; }
	session.runtime.run(test, "invalid"); if (session.runtime.error.empty()) { warning("SCREEN PALETTE FAIL invalid source accepted"); return false; }
	session.runtime.error.clear(); session.runtime.run(test, "animated");
	if (session.runtime.error.empty()) { warning("SCREEN PALETTE FAIL unsupported animation accepted"); return false; }
	session.runtime.error.clear(); warning("SCREEN PALETTE PASS named sources range mix dark/reveal endpoints retained source clut versus clear invalid source");
	// Different palettes/pixels on the leaving and arriving pictures catch
	// accidental use of the new palette for old strips or a mutated destination.
	Bytes old, target; old.resize(640 * 480 * 4); target.resize(old.size());
	for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
		const uint32 p = (y * 640 + x) * 4;
		old[p] = byte(x); old[p + 1] = byte(y); old[p + 2] = 17; old[p + 3] = 255;
		target[p] = 29; target[p + 1] = byte(x + y); target[p + 2] = 211; target[p + 3] = 255;
	}
	if (!session.display.rgba(old, 640, 480, false, false)) return false;
	session.display.hold(true); const bool staged = session.display.rgba(target, 640, 480, false, false); session.display.hold(false);
	Bytes held; if (!staged || !session.display.capture(held)) return false;
	const auto heldFormat = backend->getScreenFormat();
	for (uint32 p = 0; p < old.size(); p += 4) {
		byte expected[3]; heldFormat.colorToRGB(heldFormat.RGBToColor(old[p], old[p + 1], old[p + 2]), expected[0], expected[1], expected[2]);
		if (memcmp(held.data() + p, expected, 3)) { warning("SCREEN TRANSITION FAIL destination flashed before reveal"); return false; }
	}
	const char *effects[] = { "barndooropen", "barndoorclose", "wipeleft", "wiperight", "irisopen", "irisclose" };
	for (uint32 resolution = 0; resolution < 2; ++resolution) for (uint32 effect = 0; effect < 6; ++effect) for (uint32 step = 0; step <= 5; ++step) {
		if (!session.display.transition(old, effects[effect], step, 5, resolution != 0)) return false;
		Bytes capture; if (!session.display.capture(capture) || session.display.picture() != target) { warning("SCREEN TRANSITION FAIL capture or destination changed"); return false; }
		for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
			// Five equal 128-column strips; barn doors split each strip
			// between the edges. Check every physical pixel at every step.
			bool arriving = effect == 0 ? x >= (5 - step) * 64 && x < 640 - (5 - step) * 64 : effect == 1 ? x < step * 64 || x >= 640 - step * 64 : effect == 2 ? x >= (5 - step) * 128 : x < step * 128;
			if (effect >= 4) {
				const uint32 w = resolution ? 512 : 640, h = resolution ? 384 : 480;
				const uint32 xx = x * w / 640, yy = y * h / 480;
				const uint32 dx = MIN<uint32>(w / 2, (resolution ? 52 : 65) * (step + 1)), dy = MIN<uint32>(h / 2, (resolution ? 39 : 49) * (step + 1));
				arriving = step == 5 || (effect == 4 ? xx >= w / 2 - dx && xx < w / 2 + dx && yy >= h / 2 - dy && yy < h / 2 + dy : xx < dx || xx >= w - dx || yy < dy || yy >= h - dy);
			}
			const uint32 p = (y * 640 + x) * 4;
			const byte *ink = (arriving ? target : old).data() + p; byte expected[4] = { 0, 0, 0, 255 };
			const auto format = backend->getScreenFormat(); format.colorToRGB(format.RGBToColor(ink[0], ink[1], ink[2]), expected[0], expected[1], expected[2]);
			if (memcmp(capture.data() + p, expected, 4)) { warning("SCREEN TRANSITION FAIL effect=%s step=%u x=%u y=%u actual=%u,%u,%u expected=%u,%u,%u", effects[effect], step, x, y, capture[p], capture[p + 1], capture[p + 2], expected[0], expected[1], expected[2]); return false; }
		}
	}
	session.display.fade(0, 1); Bytes black; if (!session.display.capture(black)) return false;
	for (uint32 p = 0; p < black.size(); p += 4) if (black[p] || black[p + 1] || black[p + 2]) { warning("SCREEN TRANSITION FAIL capture ignores displayed black"); return false; }
	const uint32 transitionStart = backend->getMillis(), transitionFrame = session.scheduler.frame;
	if (session.runtime.run(test, "transitions").value.number() != 12345 || !session.runtime.error.empty() || session.screenBlack || session.scheduler.frame != transitionFrame || backend->getMillis() - transitionStart < 280) { warning("SCREEN TRANSITION FAIL modal timing/state %s", session.runtime.error.c_str()); return false; }
	Session virtualTransition; if (!virtualTransition.init(root) || virtualTransition.runtime.run(test, "transitions").value.number() != 12345 || virtualTransition.elapsedMillis() != (session.version == 1 ? 567 : 300) || virtualTransition.scheduler.frame) { warning("SCREEN TRANSITION FAIL virtual clock"); return false; }
	warning("SCREEN TRANSITION PASS full-screen strips rectangular irises 640/512 scaling endpoints held destination displayed capture modal timing virtual clock");
	Common::Event quit; quit.type = Common::EVENT_QUIT; backend->getEventManager()->pushEvent(quit);
	const ScriptResult interrupted = session.runtime.run(test, "quitfade");
	if (interrupted.signal != ScriptResult::kAbandoned || session.runtime.globals["afterquit"].number() != 0) { warning("SCREEN QUIT FAIL continued script"); return false; }
	warning("SCREEN QUIT PASS abandon ramp before following statement"); warning("SCREEN PASS"); return true;
}
bool auditCursors(const Common::FSNode &root, const Common::FSNode &fixture, OSystem *backend) {
	Session session(backend); Bytes bytes; ScriptInstance test; test.name = "cursor-test"; Common::String error;
	if (!session.init(root) || !read(fixture, bytes) || !loadScript(bytes, test.script, error)) { warning("CURSOR FAIL init %s", error.c_str()); return false; }
	CursorSystem &c = session.cursors;
	if (c.names.empty()) { warning("CURSOR FAIL no executable artwork"); return false; }
	for (uint32 n = 0; n < c.names.size(); ++n) {
		const Graphics::Cursor *art = c.art(c.names[n]); AuditHash pixels;
		if (!art || art->getWidth() != 32 || art->getHeight() != 32) { warning("CURSOR FAIL dimensions"); return false; }
		for (uint32 p = 0; p < 1024; ++p) {
			const byte index = art->getSurface()[p];
			const bool opaque = art->getMask() ? art->getMask()[p] == kCursorMaskOpaque : index != art->getKeyColor();
			for (uint32 rgb = 0; rgb < 3; ++rgb) pixels.byteValue(opaque ? art->getPalette()[index * 3 + rgb] : 0);
			pixels.byteValue(opaque ? 255 : 0);
		}
		warning("CURSOR %s width=%u height=%u x=%u y=%u rgba=%08x", c.names[n].c_str(), art->getWidth(), art->getHeight(), art->getHotspotX(), art->getHotspotY(), pixels.hash);
	}
	const ScriptResult result = session.runtime.run(test, "test");
	if (result.value.number() != 12345 || !session.runtime.error.empty() || c.depth != -1 || c.name != "touch" || c.visible() || CursorMan.isVisible() || c.art("ToUcH") != c.art("touch")) { warning("CURSOR API FAIL nested visibility"); return false; }
	session.runtime.run(test, "reveal");
	if (c.depth != 0 || c.name != "none" || c.visible() || CursorMan.isVisible()) { warning("CURSOR API FAIL none"); return false; }
	session.runtime.run(test, "fallback");
	if (c.name != "not-in-the-executable" || !c.visible() || !CursorMan.isVisible() || c.art(c.name)) { warning("CURSOR API FAIL fallback"); return false; }
	c.show(); c.hide(); if (!c.visible() || c.depth != 0) return false;
	c.reset(); if (!c.names.empty() || c.name != "arrow" || c.depth != 0 || !CursorMan.isVisible()) return false;
	if (c.load("missing.exe") || !c.names.empty() || !c.visible() || !session.runtime.error.empty()) return false;
	warning("CURSOR API PASS compiled calls nested hide/show casefold none fallback reset missing executable");
	warning("CURSOR PASS failures=0"); return true;
}
static bool shopReaderSelfTest() {
	Bytes head, group, state, sprite, script;
	head.resize(2380); group.resize(126); state.resize(250); sprite.resize(13); script.resize(8);
	memset(head.data(), 0, head.size()); memset(group.data(), 0, group.size()); memset(state.data(), 0, state.size()); memset(sprite.data(), 0, sprite.size()); memset(script.data(), 0, script.size());
	WRITE_LE_UINT32(head.data() + 2, 4); WRITE_LE_UINT32(head.data() + 2340, 4); WRITE_LE_UINT32(head.data() + 2360, 1); WRITE_LE_UINT32(head.data() + 2364, 1);
	head[2344] = 4; memcpy(head.data() + 2345, "test", 4);
	WRITE_LE_UINT32(group.data() + 38, 4); group[42] = 4; memcpy(group.data() + 43, "door", 4); WRITE_LE_UINT32(group.data() + 90, 1); WRITE_LE_UINT32(group.data() + 94, 2);
	group[110] = 4; memcpy(group.data() + 111, "base", 4);
	WRITE_LE_UINT32(state.data() + 114, 3); WRITE_LE_UINT16(state.data() + 112, 4);
	const uint16 order[] = { 1, 2, 2, 3 }; for (uint32 p = 0; p < 4; ++p) WRITE_LE_UINT16(state.data() + 46 + p * 2, order[p]);
	for (uint32 p = 0; p < 3; ++p) { WRITE_LE_UINT32(state.data() + 118 + p * 44, 3); WRITE_LE_UINT16(state.data() + 126 + p * 44, p); WRITE_LE_UINT16(state.data() + 158 + p * 44, p == 2 ? 1 : 0); }
	WRITE_LE_UINT16(sprite.data(), 1); WRITE_LE_UINT16(sprite.data() + 2, 1); WRITE_LE_UINT16(sprite.data() + 8, 2); sprite[10] = 6; sprite[11] = 7;
	auto envelope = [&]() {
		const Bytes *parts[] = { &head, &group, &state, &sprite, &script }; uint32 size = 1044;
		for (uint32 p = 0; p < 5; ++p) size += 8 + parts[p]->size();
		Bytes data; data.resize(size); memset(data.data(), 0, data.size()); WRITE_LE_UINT32(data.data() + 4, size); WRITE_LE_UINT32(data.data() + 20, 5);
		uint32 at = 1044; for (uint32 p = 0; p < 5; ++p) { WRITE_LE_UINT32(data.data() + 1024 + p * 4, at); WRITE_LE_UINT32(data.data() + at + 4, parts[p]->size()); memcpy(data.data() + at + 8, parts[p]->data(), parts[p]->size()); at += 8 + parts[p]->size(); }
		return data;
	};
	Shop shop; Sprite decoded;
	if (!shop.load(envelope()) || shop.groups[0].states[0].play.size() != 4 || shop.groups[0].states[0].play[2] != 1 || !shop.groups[0].states[0].animated || shop.groups[0].states[0].pictures[0].refScale != 96 || !shop.sprite(3, decoded) || decoded.pixels[0] != 7) return false;
	WRITE_LE_UINT32(state.data() + 114, 4); if (shop.load(envelope())) return false; WRITE_LE_UINT32(state.data() + 114, 3);
	WRITE_LE_UINT32(group.data() + 38, 99); if (shop.load(envelope())) return false; WRITE_LE_UINT32(group.data() + 38, 4);
	WRITE_LE_UINT32(state.data() + 118, 99); if (shop.load(envelope())) return false; WRITE_LE_UINT32(state.data() + 118, 3);
	WRITE_LE_UINT16(state.data() + 46, 0); if (!shop.load(envelope()) || !shop.groups[0].states[0].play.empty() || !shop.groups[0].states[0].animated) return false;
	WRITE_LE_UINT16(state.data() + 112, 1); WRITE_LE_UINT16(state.data() + 158 + 88, 2); WRITE_LE_UINT16(state.data() + 158 + 44, 1);
	if (!shop.load(envelope()) || shop.groups[0].states[0].animated) return false;
	Bytes gap = envelope(); WRITE_LE_UINT32(gap.data() + 1024 + 3 * 4, 0);
	if (!shop.load(gap) || !shop.sprite(3, decoded) || decoded.width || !decoded.pixels.empty() || shop.sprite(99, decoded)) return false;
	group[42] = 48; if (shop.load(envelope())) return false;
	debug("SHOP READER PASS repeated lists selectors defaults gaps dimensions references names"); return true;
}
bool auditShops(const Common::FSNode &root) {
	Common::FSList files; uint32 count = 0, failures = 0;
	if (!shopReaderSelfTest()) { warning("SHOP READER FAIL"); return false; }
	if (!corpusFiles(root, files)) return false;
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root); if (!name.hasSuffixIgnoreCase(".shp") && !name.hasSuffixIgnoreCase(".prp")) continue;
		Bytes bytes; Common::SharedPtr<LoadedShop> loaded(new LoadedShop()); Shop &shop = loaded->data;
		if (!read(files[i], bytes) || !shop.load(bytes)) { warning("SHOP FAIL %s directory", name.c_str()); ++failures; continue; }
		AuditHash meta, pixels, selection, art, worldArt; uint32 states = 0, frames = 0, pictures = 0, worldPictures = 0;
		Runtime runtime; Resources resources; PropSystem props(resources, runtime);
		meta.number(shop.version); meta.number(shop.mainScript); meta.text(shop.name);
		for (uint32 b = 0; b < 768; ++b) meta.byteValue(shop.palette[b]); meta.number(shop.groups.size());
		for (uint32 g = 0; g < shop.groups.size(); ++g) {
			const PropGroup &group = shop.groups[g]; meta.text(group.name); meta.number(group.location); meta.number(group.script); meta.number(group.states.size());
			for (uint32 s = 0; s < group.states.size(); ++s) {
				++states; const PropState &state = group.states[s]; meta.text(state.name); meta.number(state.location); meta.number(state.record); meta.number(state.animated); meta.number(state.play.size());
				for (uint32 p = 0; p < state.play.size(); ++p) meta.number(state.play[p]); meta.number(state.pictures.size());
				if (!state.pictures.empty()) {
					const int32 degrees[] = {-1, 0, 1, 2, 7, 64, 128, 255, 256};
					for (uint32 d = 0; d < ARRAYSIZE(degrees); ++d) {
						Prop prop; prop.shop = loaded; prop.group = g; prop.view = state.name; prop.deg = degrees[d];
						selection.number(prop.frameIndex()); prop.selectView(state.name, 1); selection.number(prop.frameCount());
						for (uint32 step = 0; step < prop.frameCount(); ++step) { prop.step = step; selection.number(prop.frameIndex()); }
					}
					if (s == 0 || s + 1 == group.states.size()) for (int32 degree = 0; degree < 2; ++degree) {
						Common::SharedPtr<Prop> prop(new Prop()); prop->name = "probe"; prop->shop = loaded; prop->group = g; prop->deg = degree; prop->selectView(state.name, 1); prop->step = prop->frameCount() - 1; prop->visible = true;
						const Sprite *sprite = loaded->frame(state.pictures[prop->frameIndex()].location); if (!sprite) { ++failures; continue; }
						prop->anchorX = sprite->x - 3; prop->anchorY = sprite->y + 2; props.props.clear(); props.props.push_back(prop);
						Bytes rgba; rgba.resize(96 * 72 * 4); for (uint32 p = 0; p < rgba.size(); p += 4) { rgba[p] = 19; rgba[p + 1] = 31; rgba[p + 2] = 47; rgba[p + 3] = 255; }
						if (!props.composite(rgba, 96, 72, shop.palette)) { ++failures; continue; }
						for (uint32 p = 0; p < rgba.size(); ++p) art.byteValue(rgba[p]); ++pictures;
					}
				}
				if (!state.pictures.empty() && (s == 0 || s + 1 == group.states.size())) {
					const int32 headings[] = {0, 37, 128, 255};
					for (uint32 h = 0; h < ARRAYSIZE(headings); ++h) for (uint32 directional = 0; directional < 2; ++directional) {
						Common::SharedPtr<Prop> prop(new Prop()); prop->shop = loaded; prop->group = g; prop->view = state.name; prop->deg = 73; prop->visible = prop->worldSpace = true; prop->directional = directional != 0; prop->scale = 1500;
						const double angle = headings[h] * M_PI / 128;
						prop->x = 13 + int32(floor(160 * cos(angle) + 0.5)) + int32(floor(20 * sin(angle) + 0.5));
						prop->y = -27 + int32(floor(160 * sin(angle) + 0.5)) - int32(floor(20 * cos(angle) + 0.5)); prop->z = 35; prop->zclip = directional ? 75 : 0;
						WorldCamera camera; camera.x = 13; camera.y = -27; camera.z = 45; camera.degree = headings[h]; camera.focal = 48; camera.width = 96; camera.height = 72;
						Frame depth; depth.width = 96; depth.height = 72; depth.depth.resize(96 * 72); for (uint32 p = 0; p < depth.depth.size(); ++p) depth.depth[p] = ((p / 96) * 7 + (p % 96) * 3) % 16;
						Occlusion occlusion; occlusion.frame = &depth; occlusion.scale = 32; occlusion.bias = shop.version == 1 ? 64 : 0;
						props.props.clear(); props.props.push_back(prop);
						Bytes rgba; rgba.resize(96 * 72 * 4); for (uint32 p = 0; p < rgba.size(); p += 4) { rgba[p] = 19; rgba[p + 1] = 31; rgba[p + 2] = 47; rgba[p + 3] = 255; }
						if (!props.compositeWorld(rgba, 96, 72, shop.palette, camera, "", occlusion)) { ++failures; continue; }
						for (uint32 p = 0; p < rgba.size(); ++p) worldArt.byteValue(rgba[p]); ++worldPictures;
					}
				}
				for (uint32 p = 0; p < state.pictures.size(); ++p) {
					++frames; const PropPicture &picture = state.pictures[p]; meta.number(picture.location); meta.number(picture.record); meta.number(picture.group); meta.number(picture.degree); meta.number(picture.refScale);
					Sprite sprite; if (!shop.sprite(picture.location, sprite)) { warning("SHOP FAIL %s sprite=%u", name.c_str(), picture.location); ++failures; continue; }
					pixels.number(sprite.width); pixels.number(sprite.height); pixels.number(sprite.x); pixels.number(sprite.y);
					for (uint32 b = 0; b < sprite.pixels.size(); ++b) pixels.byteValue(sprite.pixels[b]); for (uint32 b = 0; b < sprite.opaque.size(); ++b) pixels.byteValue(sprite.opaque[b]);
				}
			}
		}
		debug("PROPWORLD %s pictures=%u rgba=%08x", name.c_str(), worldPictures, worldArt.hash);
		debug("PROPART %s pictures=%u selection=%08x rgba=%08x", name.c_str(), pictures, selection.hash, art.hash);
		debug("SHOP %s props=%u states=%u frames=%u meta=%08x pixels=%08x", name.c_str(), shop.groups.size(), states, frames, meta.hash, pixels.hash); ++count;
	}
	debug("SHOP %s files=%u failures=%u", failures ? "FAIL" : "PASS", count, failures); return !failures;
}
static void movieActionHash(AuditHash &hash, const MovieAction &action) {
	hash.number(action.type); hash.text(action.sound); hash.text(action.event); hash.text(action.target);
}
class SoundAudit {
public:
	static bool voice(SoundSystem &sound, Audio::Mixer *mixer, byte volume, int8 pan) {
		return sound.active(sound._voice) && mixer->getChannelVolume(sound._voice.handle) == volume && mixer->getChannelBalance(sound._voice.handle) == pan;
	}
	static bool channel(SoundSystem &sound, Audio::Mixer *mixer, const Common::String &name, byte volume, int8 pan) {
		for (const auto &slot : sound._slots) if (slot.name == name && sound.active(slot))
			return mixer->getChannelVolume(slot.handle) == volume && mixer->getChannelBalance(slot.handle) == pan;
		return false;
	}
	static bool themeShape(SoundSystem &sound, uint32 parts, int32 repeat) {
		ThemeQueue snapshot; return sound.themeSnapshot(snapshot) && snapshot.parts.size() == parts && snapshot.repeat == repeat;
	}
	static Audio::SoundHandle themeHandle(SoundSystem &sound) { return sound._theme.handle; }
	static bool themes(Session &session, OSystem *backend);
	static bool banks(Session &session, OSystem *backend, const ScriptInstance &test);
	static bool directory(Session &session, OSystem *backend, const ScriptInstance &test);
	static bool enhancements(const Common::FSNode &root, OSystem *backend);
};
static bool movieReaderSelfTest() {
	// Synthetic v4 envelope: two one-frame segment headers and an invalid
	// picture. Directory tests do not depend on any copyrighted game bytes.
	const uint32 headerSize = 2214, offset0 = 1036, offset1 = offset0 + 8 + headerSize, offset2 = offset1 + 9;
	Bytes data; data.resize(offset2 + 8 + headerSize); memset(data.data(), 0, data.size());
	WRITE_LE_UINT32(data.data() + 4, data.size()); WRITE_LE_UINT32(data.data() + 20, 3);
	WRITE_LE_UINT32(data.data() + 1024, offset0); WRITE_LE_UINT32(data.data() + 1028, offset1); WRITE_LE_UINT32(data.data() + 1032, offset2);
	WRITE_LE_UINT32(data.data() + offset0 + 4, headerSize); WRITE_LE_UINT32(data.data() + offset1 + 4, 1); WRITE_LE_UINT32(data.data() + offset2 + 4, headerSize);
	byte *head = data.data() + offset0 + 8;
	WRITE_LE_UINT32(head + 2, 4); WRITE_LE_UINT16(head + 2160, 1); WRITE_LE_UINT16(head + 2162, 1);
	WRITE_LE_UINT32(head + 2168, 1); WRITE_LE_UINT32(head + 2184, 1);
	memcpy(data.data() + offset2 + 8, head, headerSize);
	WRITE_LE_UINT32(data.data() + offset2 + 8 + 2184, 0); // Bias-relative reference to container 2.
	Movie movie; Common::Array<Frame> frames;
	if (!movie.load(data) || movie.decode(0, frames)) return false;
	WRITE_LE_UINT32(head + 2184, 99); if (movie.load(data)) return false; WRITE_LE_UINT32(head + 2184, 1);
	WRITE_LE_UINT32(head + 2168, 2); if (movie.load(data)) return false; WRITE_LE_UINT32(head + 2168, 1);
	WRITE_LE_UINT16(head + 2162, 641); if (movie.load(data)) return false; WRITE_LE_UINT16(head + 2162, 1);
	WRITE_LE_UINT32(head + 44, 2); if (!movie.load(data) || movie.segments.size() != 2) return false;
	WRITE_LE_UINT32(data.data() + offset2 + 8 + 44, 2); if (movie.load(data)) return false;
	WRITE_LE_UINT32(head + 2, 7); if (movie.load(data)) return false;
	data.resize(100); if (movie.load(data)) return false;
	debug("MOVIE READER PASS malformed directory dimensions references cycles images"); return true;
}
static bool moviePlayerSelfTest() {
	class Host : public MovieHost {
	public:
		Common::SharedPtr<Movie> a, b;
		uint32 shown = 0, sounds = 0, finishes = 0;
		bool voiceDone = true, dismissed = false, interactive = false, keepInput = false;
		Host() : a(new Movie()), b(new Movie()) {}
		Common::SharedPtr<Movie> loadMovie(const Common::String &name) override {
			return name == "a" ? a : name == "b" ? b : Common::SharedPtr<Movie>();
		}
		bool movieSegment(const Movie &, uint32) override { return true; }
		void movieFrame(uint32 frame) override { shown = frame; }
		bool movieSound(const Common::String &) override { ++sounds; return true; }
		bool movieVoiceDone() const override { return voiceDone; }
		void movieFinished(bool d, bool i, bool k) override { ++finishes; dismissed = d; interactive = i; keepInput = k; }
	} host;
	MovieSegment segment; segment.minHold = 3; segment.keySkips = true;
	for (uint32 i = 0; i < 3; ++i) { MovieFrame f; f.name = Common::String::format("%u", i); segment.frames.push_back(f); }
	segment.action1 = "1"; segment.action2 = "2";
	host.a->version = host.b->version = 4; host.a->segments.reserve(2); host.a->segments.push_back(segment); host.b->segments.push_back(segment);
	MoviePlayer p(host);
	if (!p.start("a", 0)) return false;
	p.tick(49); if (p.frame() != 0) return false;
	p.tick(50); if (p.frame() != 1 || p.actions() != 1) return false;
	host.a->segments[0].frames[1].waitVoice = true; host.voiceDone = false;
	p.tick(500); if (p.frame() != 1) return false;
	host.voiceDone = true; p.tick(501); if (p.frame() != 2 || p.actions() != 3) return false;
	p.tick(551); if (p.active() || host.finishes != 1 || host.dismissed) return false;
	// A modal region ignores outside clicks; a cue releases it once, even
	// while a sound is unfinished. The next poll cannot refire that cue.
	MovieRegion region; region.left = 2; region.top = 3; region.right = 12; region.bottom = 13; region.type = 2; region.target = "1"; region.sound = "line";
	MovieSegment &s = host.a->segments[0]; s.x = 10; s.y = 20; s.frames[0].regions.push_back(region);
	MovieSound sound; sound.name = "line"; s.sounds.push_back(sound);
	s.frames[1].sound = "line"; s.frames[1].waitVoice = false;
	if (!p.start("a", 0)) return false;
	p.tick(500); if (p.frame() != 0) return false;
	p.click(1, 1, 500); if (p.frame() != 0) return false;
	const uint32 before = host.sounds; p.click(12, 23, 501);
	if (p.frame() != 1 || host.sounds != before + 1) return false; // Duplicate entry sound suppressed.
	MovieCue dangling; dangling.tick = 0; dangling.target = "missing"; s.cues.push_back(dangling);
	MovieCue cue; cue.tick = 3; cue.target = "2"; s.cues.push_back(cue);
	p.start("a", 0); host.voiceDone = false; p.tick(50);
	if (p.frame() != 2 || p.actions() != 2) return false;
	s.frames[2].type = 2; s.frames[2].target = "0";
	p.tick(100); p.tick(500); if (p.frame() != 0) return false;
	s.cues.clear(); s.frames[2].type = 1;
	// Sound-follow jumps also escape modal frames and can be canceled by
	// another sound without a follow target.
	s.sounds[0].follow = "2"; s.frames[0].sound = "line";
	p.start("a", 0); p.tick(50); if (p.frame() != 0) return false;
	host.voiceDone = true; p.tick(51); if (p.frame() != 2) return false;
	// Missing cues do not replace a voice or cancel its pending sound follow.
	s.frames[0].regions[0].sound = "absent"; s.frames[1].sound = "absent";
	host.voiceDone = false; p.start("a", 0); const uint32 beforeMissing = host.sounds;
	p.click(12, 23, 1); if (p.frame() != 1 || host.sounds != beforeMissing) return false;
	host.voiceDone = true; p.tick(2); if (p.frame() != 2) return false;
	s.frames[0].regions[0].sound = "line"; s.frames[1].sound = "line";
	p.start("a", 0); host.voiceDone = false; p.click(12, 23, 1);
	if (p.frame() != 1) return false;
	p.key(false); if (!p.active()) return false;
	p.key(true); if (p.active() || !host.dismissed || !host.interactive) return false;
	// Call/return restores the originating segment, preserving both action
	// bits across the chain; a plain chain has no return continuation.
	s.frames[0].sound.clear(); s.frames[0].regions.clear(); s.sounds.clear(); s.x = s.y = 0;
	s.frames[0].type = 1; host.a->segments.push_back(segment);
	MovieFrame &call = host.a->segments[1].frames[0]; call.type = 4; call.event = "b"; call.target = "2";
	host.b->segments[0].frames[0].type = 5;
	p.start("a", 0); p.tick(50); if (p.segment() != 1) return false;
	p.tick(100); if (p.segment() != 0) return false;
	p.click(0, 0, 101); if (p.segment() != 1 || p.frame() != 2 || !(p.actions() & 2)) return false;
	p.abandon(); host.a->segments.resize(1);
	// Silent, region-less close-ups require clicks. Holds with the deadline
	// flag retain their previous deadline, but still advance once per poll.
	s = segment; for (uint32 i = 0; i < 3; ++i) { s.frames[i].type = 2; s.frames[i].target = "0"; }
	p.start("a", 0); p.tick(10000); if (p.frame() != 0) return false;
	s.frames[0].type = 6; s.frames[1].type = 6; s.frames[1].holdDeadline = true;
	p.start("a", 0); p.tick(100); if (p.frame() != 1) return false;
	p.tick(100); if (p.frame() != 2) return false;
	p.abandon(); s.flags = 8; s.keySkips = false;
	p.start("a", 0); p.key(false); if (p.active() || !host.keepInput) return false;
	s.flags = 0; s.frames[0].type = 2; s.frames[0].target = "missing";
	p.start("a", 0); p.click(0, 0, 1); if (p.active() || p.error.empty()) return false;
	if (p.start("missing", 0) || p.error.empty()) return false;
	debug("MOVIE STATE PASS holds voice regions cues sound-follow chain return actions skip errors"); return true;
}
bool auditMovies(const Common::FSNode &root) {
	Common::FSList files; uint32 count = 0, skipped = 0, failures = 0;
	{
		MovieSegment seg; seg.frames.resize(1); seg.x = 10; seg.y = 20;
		MovieRegion region; region.left = 30; region.right = 50; region.top = 40; region.bottom = 60;
		seg.frames[0].regions.push_back(region);
		Bytes rgba; rgba.resize(640 * 480 * 4); memset(rgba.data(), 17, rgba.size());
		const Bytes original = rgba;
		auto yellow = [&](uint32 x, uint32 y) { const byte *p = rgba.data() + (y * 640 + x) * 4; return p[0] == 255 && p[1] == 230 && p[2] == 0 && p[3] == 255; };
		// Native v4 coordinates include the segment origin before scaling.
		if (!MovieSystem::compositeHotspotHints(rgba, seg, 0, 512, 384) || !yellow(66, 87) || rgba[(87 * 640 + 62) * 4] != 17) return false;
		rgba = original;
		if (!MovieSystem::compositeHotspotHints(rgba, seg, 0, 640, 480) || !yellow(54, 70)) return false;
		rgba = original; seg.frames[0].playThrough = true;
		if (MovieSystem::compositeHotspotHints(rgba, seg, 0, 512, 384) || rgba != original) return false;
		seg.frames[0].playThrough = false; seg.flags = 8;
		if (MovieSystem::compositeHotspotHints(rgba, seg, 0, 512, 384) || rgba != original) return false;
		seg.flags = 0; seg.frames[0].regions[0].left = -100; seg.frames[0].regions[0].right = -20;
		if (MovieSystem::compositeHotspotHints(rgba, seg, 0, 512, 384) || rgba != original) return false;
		seg.x = seg.y = 0; seg.frames[0].regions[0].left = seg.frames[0].regions[0].top = 0;
		seg.frames[0].regions[0].right = seg.frames[0].regions[0].bottom = 1;
		if (!MovieSystem::compositeHotspotHints(rgba, seg, 0, 512, 384) || !yellow(4, 0)) return false;
		debug("MOVIE HINTS PASS v1 v4 segment origin scale hollow markers clipped bounds cinematic exclusion");
	}
	if (!movieReaderSelfTest() || !moviePlayerSelfTest()) { warning("MOVIE STATE FAIL"); return false; }
	if (!corpusFiles(root, files)) { warning("MOVIE FAIL corpus listing"); return false; }
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root); if (!name.hasSuffixIgnoreCase(".mov")) continue;
		Bytes data; Movie movie; Archive envelope;
		if (!read(files[i], data)) { warning("MOVIE FAIL %s read", name.c_str()); ++failures; continue; }
		// Dust's ZUNUSED directory contains unused Macintosh assets. Identify
		// their byte order from the declared size, rather than treating the
		// big-endian version field as a Windows version zero.
		if (data.size() >= 32 && READ_BE_UINT32(data.data() + 4) == data.size() && READ_LE_UINT32(data.data() + 4) != data.size()) {
			debug("MOVIE SKIP %s big-endian", name.c_str()); ++skipped; continue;
		}
		if (!envelope.load(data)) { warning("MOVIE FAIL %s envelope", name.c_str()); ++failures; continue; }
		Reader version(envelope.get(0), 2); const uint32 tag = version.u32();
		if (tag == 0) { debug("MOVIE SKIP %s version=0", name.c_str()); ++skipped; continue; }
		if (!movie.load(data)) { warning("MOVIE FAIL %s directory", name.c_str()); ++failures; continue; }
		AuditHash meta, pixels, audio; uint32 frames = 0, regions = 0, chunks = 0; Common::HashMap<uint32, bool> used;
		meta.number(movie.version);
		for (uint32 s = 0; s < movie.segments.size(); ++s) {
			const MovieSegment &seg = movie.segments[s];
			meta.number(seg.bias); meta.number(seg.width); meta.number(seg.height); meta.number(seg.x); meta.number(seg.y); meta.number(seg.flags); meta.number(seg.minHold); meta.number(seg.keySkips); meta.number(seg.audioLoops); meta.number(seg.bedRecords); meta.number(seg.bedRepeat); meta.text(seg.action1); meta.text(seg.action2);
			for (uint32 b = 0; b < 768; ++b) meta.byteValue(seg.palette[b]);
			meta.number(seg.frames.size()); Common::Array<Frame> images;
			if (!movie.decode(s, images)) { warning("MOVIE FAIL %s segment=%u images", name.c_str(), s); ++failures; continue; }
			for (uint32 f = 0; f < seg.frames.size(); ++f) {
				const MovieFrame &frame = seg.frames[f]; ++frames; movieActionHash(meta, frame);
				meta.number(frame.image); meta.number(frame.logic); meta.number(frame.record); meta.number(frame.width); meta.number(frame.height); meta.text(frame.name); meta.number(frame.hold); meta.number(frame.waitVoice); meta.number(frame.holdDeadline); meta.number(frame.playThrough); meta.number(frame.regions.size());
				for (uint32 g = 0; g < frame.regions.size(); ++g) {
					const MovieRegion &region = frame.regions[g]; ++regions; movieActionHash(meta, region);
					meta.number(region.top); meta.number(region.left); meta.number(region.bottom); meta.number(region.right); meta.number(region.record);
				}
				pixels.number(images[f].width); pixels.number(images[f].height); for (uint32 b = 0; b < images[f].pixels.size(); ++b) pixels.byteValue(images[f].pixels[b]);
			}
			meta.number(seg.bed.size()); for (uint32 c = 0; c < seg.bed.size(); ++c) { meta.number(seg.bed[c]); used[seg.bed[c]] = true; }
			meta.number(seg.sounds.size()); for (uint32 c = 0; c < seg.sounds.size(); ++c) { const MovieSound &sound = seg.sounds[c]; meta.text(sound.name); meta.number(sound.container); meta.text(sound.follow); used[sound.container] = true; }
			meta.number(seg.cues.size()); for (uint32 c = 0; c < seg.cues.size(); ++c) { meta.number(seg.cues[c].tick); meta.text(seg.cues[c].target); }
		}
		for (uint32 c = 0; c < movie.archive.size(); ++c) if (used.contains(c)) {
			AudioClip clip; if (!decodeAudio(movie.archive.get(c), clip)) { warning("MOVIE FAIL %s audio=%u", name.c_str(), c); ++failures; continue; }
			++chunks; audio.number(c); audio.number(clip.rate); audio.number(clip.samples.size());
			for (uint32 p = 0; p < clip.samples.size(); ++p) { audio.byteValue(byte(clip.samples[p])); audio.byteValue(byte(clip.samples[p] >> 8)); }
		}
		debug("MOVIE %s segments=%u frames=%u regions=%u chunks=%u meta=%08x pixels=%08x audio=%08x", name.c_str(), movie.segments.size(), frames, regions, chunks, meta.hash, pixels.hash, audio.hash); ++count;
	}
	debug("MOVIE %s files=%u skipped=%u failures=%u", failures ? "FAIL" : "PASS", count, skipped, failures); return !failures;
}
bool auditMoviePlayback(const Common::FSNode &fixtures, OSystem *system, bool retail) {
	Session session(system); Bytes bytes; ScriptInstance test; test.name = "movie-test"; Common::String error;
	// This layout is authored like the native v1 loader, rather than relying
	// on the reference reader's unused-audio inference.
	Bytes dustData; Movie dustMovie; AudioClip dustBed;
	if (!read(fixtures.getChild("DUST.MOV"), dustData) || !dustMovie.load(dustData) ||
		dustMovie.segments.size() != 1 || dustMovie.segments[0].bed.size() != 2 ||
		dustMovie.segments[0].bed[0] != 2 || dustMovie.segments[0].bed[1] != 2 ||
		!joinAudio(dustMovie.archive, dustMovie.segments[0].bed, dustBed) || dustBed.samples.size() != 200) {
		warning("MOVIE ORDER FAIL v1 authored repeat or unreferenced audio"); return false;
	}
	const uint32 dustHead = READ_LE_UINT32(dustData.data() + 1024) + 8;
	const uint32 badFields[] = {2110, 2110, 52, 28};
	const uint16 badValues[] = {0, 2, 65, 32767};
	for (uint32 i = 0; i < ARRAYSIZE(badFields); ++i) {
		Bytes bad = dustData; WRITE_LE_UINT16(bad.data() + dustHead + badFields[i], badValues[i]);
		if (dustMovie.load(bad)) { warning("MOVIE ORDER FAIL accepted invalid v1 bed directory %u", i); return false; }
	}
	if (!system || !session.init(fixtures) || !read(fixtures.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) {
		warning("MOVIE HOST FAIL setup %s %s", error.c_str(), session.runtime.error.c_str()); return false;
	}
	session.sound.setTitanicRetail(retail); session.sound.setTitanicDemo(session.version == 4 && !retail);
	session.resources.setPath(1, session.resources.path(0));
	{
		Session endingSession(system); RouteTestFiles endingFiles; ScriptInstance endingScript;
		if (!endingSession.init(fixtures) || !endingSession.routes.open(&endingFiles, "ending-test", "fixture", error) || !read(fixtures.getChild("ending.dfscript"), bytes) || !loadScript(bytes, endingScript.script, error)) return false;
		endingSession.sound.setTitanicRetail(retail);
		endingSession.stageSystem.current.reset(new LoadedStage());
		endingSession.stageSystem.current->name = endingSession.version == 1 ? "new.flt" : retail ? "narend.stg" : "demo.stg";
		endingSession.runtime.globals["clock"] = Value("endgame"); endingSession.runtime.globals["day"] = Value(5); endingSession.runtime.globals["phase"] = Value(endingSession.version == 1 ? 5 : 2);
		JournalEntry entry; entry.speaker = "Encounter";
		const uint32 scene = endingSession.routes.graph.recordScene(entry);
		for (uint32 attempt = 0; attempt < 2; ++attempt) {
			endingSession.routes.graph.restoreCursor(scene, -1);
			if (endingSession.runtime.run(endingScript, "ending").value.number() != 12345 || !endingSession.runtime.error.empty() || endingSession.routes.graph.nodes().size() != 2 || endingSession.routes.graph.node(2)->kind != RouteNode::kEnding || !endingSession.routes.graph.node(2)->entry.ident.empty()) return false;
		}
		endingSession.runtime.globals["clock"] = Value(0); endingSession.runtime.globals["phase"] = Value(1); endingSession.routes.graph.restoreCursor(scene, -1);
		if (endingSession.runtime.run(endingScript, "ending").value.number() != 12345 || endingSession.routes.graph.cursor() != scene || endingSession.routes.graph.nodes().size() != 2) return false;
		RouteStore persisted;
		if (!persisted.open(&endingFiles, "ending-test", "fixture", error) || persisted.graph.node(2)->kind != RouteNode::kEnding) return false;
		debug("ROUTE ENDING HOST PASS completed authored-context movie captured persisted stable revisit unrelated context excluded voice replay unavailable");
	}
	class Source : public Common::EventSource {
	public:
		OSystem *backend; Session &session; uint32 began = 0, stage = 0; int32 mode = 0;
		bool started = false, watchdog = false, sawInherited = false, sawVoiceHold = false, sawDetached = false;
		bool checkEffects = false, lostEffects = false, sawCueVoice = false, sawLoopedCue = false, sawPriorVoice = false, sawOrderedBed = false;
		bool checkTheme = false, changedThemeHandle = false, sawQueuedBed = false, sawFullThemeGain = false;
		bool hintsShown = false, hintsHidden = false, hintsRestored = false, cursorOnly = false;
		Bytes beforeFocus;
		Audio::SoundHandle themeHandle;
		Source(OSystem *s, Session &owner) : backend(s), session(owner) { backend->getEventManager()->getEventDispatcher()->registerSource(this, false); }
		~Source() { backend->getEventManager()->getEventDispatcher()->unregisterSource(this); }
		void arm(int32 kind) { mode = kind; stage = 0; cursorOnly = false; beforeFocus.clear(); started = watchdog = sawInherited = sawVoiceHold = sawDetached = false; checkEffects = lostEffects = sawCueVoice = sawLoopedCue = sawPriorVoice = sawOrderedBed = false; checkTheme = changedThemeHandle = sawQueuedBed = sawFullThemeGain = false; }
		bool allowMapping() const override { return false; }
		bool pollEvent(Common::Event &event) override {
			if (!session.movies.displayedFrames) return false;
			if (!session.movies.currentSegment()) sawDetached = true;
			if (!started) { started = true; began = backend->getMillis(); }
			const uint32 elapsed = backend->getMillis() - began;
			if (mode == 4 && session.movies.bedActive() && SoundAudit::themeShape(session.sound, 2, 0)) sawOrderedBed = true;
			if (checkTheme && session.movies.active()) {
				if (SoundAudit::themeHandle(session.sound) != themeHandle) changedThemeHandle = true;
				if (session.movies.bedActive() && SoundAudit::themeShape(session.sound, 3, 1)) sawQueuedBed = true;
				if (session.movies.bedActive() && backend->getMixer()->getChannelVolume(themeHandle) == byte(255 * session.sound.waveVolume() / 9)) sawFullThemeGain = true;
			}
			if (!session.movies.eventDone() && session.sound.currentVoice() == "line") sawCueVoice = true;
			if (elapsed >= 1100 && !session.movies.eventDone() && session.sound.currentVoice() == "line") sawLoopedCue = true;
			if (session.sound.currentVoice() == "prior") sawPriorVoice = true;
			if (checkEffects && (session.sound.currentSound(1) != "second" || session.sound.currentSound(2) != "first")) lostEffects = true;
			if (session.movies.displayedFrames >= 4 && session.movies.bedActive()) sawInherited = true;
			if (session.movies.displayedFrames == 2 && elapsed >= 150 && !session.movies.eventDone() && session.movies.bedActive()) sawVoiceHold = true;
			if (elapsed > 5000 && !watchdog) { watchdog = true; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
			if (mode == 5 && elapsed >= 50 + stage * 50 && stage < 3) {
				const Bytes &picture = session.display.picture();
				const byte *p = picture.data() + (16 * 640 + 19) * 4;
				const bool marked = p[0] == 255 && p[1] == 230 && p[2] == 0;
				if (stage == 0) hintsShown = marked;
				else if (stage == 1) hintsHidden = !marked;
				else hintsRestored = marked;
				++stage;
				if (stage < 3) { event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = 1; }
				else { event.type = Common::EVENT_LBUTTONDOWN; event.mouse = Common::Point(15, 16); }
				return true;
			}
			if (mode >= 7 && mode <= 10 && elapsed >= 50 + stage * 50) {
				const uint32 count = mode == 7 ? 4 : mode == 10 ? 1 : 3;
				if (stage >= count) return false;
				const MovieSegment *segment = session.movies.currentSegment();
				if (!segment) return false;
				const int width = session.movies.canvasWidth(), height = width == 640 ? 480 : 384;
				const auto point = [&](int x, int y) { return Common::Point(((segment->x + x) * 640 + width - 1) / width, ((segment->y + y) * 480 + height - 1) / height); };
				event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType = kPadInteract;
				if (mode == 7 && (stage == 0 || stage == 2)) { event.type = Common::EVENT_MOUSEMOVE; event.mouse = stage == 0 ? point(200, 200) : point(60, 60); }
				if (mode == 8 && stage == 0) event.customType = kPadNext;
				if (mode == 8 && stage == 1) { event.type = Common::EVENT_MOUSEMOVE; event.mouse = point(60, 60); }
				if (mode == 9 && stage < 2) event.customType = kPadNext;
				if (mode == 10) { event.type = Common::EVENT_LBUTTONDOWN; event.mouse = point(60, 60); }
				++stage; return true;
			}
			if (mode == 6 && elapsed >= 50 + stage * 50 && stage < 2) {
                if (!stage) beforeFocus = session.display.picture();
				else cursorOnly = !beforeFocus.empty() && beforeFocus == session.display.picture();
                event.type=Common::EVENT_CUSTOM_ENGINE_ACTION_START; event.customType=stage++?kPadInteract:kPadNext; return true;
            }
            if (mode == 1 && stage == 0 && elapsed >= 50) { ++stage; event.type = Common::EVENT_LBUTTONDOWN; event.mouse = Common::Point(0, 0); return true; }
			if (mode == 1 && stage == 1 && elapsed >= 100) { ++stage; event.type = Common::EVENT_LBUTTONDOWN; event.mouse = Common::Point(10, 10); return true; }
			if (mode == 2 && stage == 0 && elapsed >= 100) { ++stage; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = Common::KEYCODE_ESCAPE; return true; }
			if (mode == 3 && stage == 0 && elapsed >= 100) { ++stage; event.type = Common::EVENT_RETURN_TO_LAUNCHER; return true; }
			if (mode == 4 && stage == 0 && elapsed >= 1200) { ++stage; event.type = Common::EVENT_LBUTTONDOWN; event.mouse = Common::Point(10, 10); return true; }
			return false;
		}
	} source(system, session);
	auto run = [&](const char *handler) {
		const ScriptResult result = session.runtime.run(test, handler);
		if (result.value.number() != 12345 || !session.runtime.error.empty() || source.watchdog) {
			warning("MOVIE HOST FAIL %s value=%d error=%s watchdog=%d", handler, result.value.number(), session.runtime.error.c_str(), source.watchdog); return false;
		}
		return true;
	};
	auto pixel = [&](uint32 x, uint32 y) {
		Graphics::Surface *screen = system->lockScreen(); const byte value = screen ? *static_cast<const byte *>(screen->getBasePtr(x, y)) : 0;
		system->unlockScreen(); return value;
	};
	source.arm(0);
	if (!session.sound.open("fx.snd") || !session.sound.playSound("first") || !session.sound.playSound("second")) return false;
	Common::SharedPtr<AudioClip> prior(new AudioClip()); prior->rate = 1000; prior->samples.resize(1000); memset(prior->samples.data(), 0, 2000);
	Audio::SoundHandle priorHandle;
	if (!session.sound.playVoiceClip("prior", prior, false, &priorHandle)) return false;
	source.checkEffects = true;
	if (!run("auto") || session.movies.displayedFrames != 5 || session.movies.bedActive() || !source.sawInherited || !source.sawVoiceHold || !source.sawDetached || pixel(4, 5) != 25 || pixel(0, 0) != 0) { warning("MOVIE HOST FAIL v4 rendering/timing/bed inheritance"); return false; }
	if (!source.sawCueVoice || source.lostEffects || session.sound.handleActive(priorHandle) || session.sound.currentVoice() != "none") { warning("MOVIE HOST FAIL shared voice or effect preservation"); return false; }
	session.sound.haltSounds();
	byte palette[768]; system->getPaletteManager()->grabPalette(palette, 0, 256);
	if (palette[25 * 3] != 25 || palette[25 * 3 + 1] != 230 || palette[765] != 255) { warning("MOVIE HOST FAIL palette"); return false; }
	source.arm(0);
	if (!run("dust") || session.movies.displayedFrames != 3 || pixel(0, 0) != 43 || session.movies.bedActive()) { warning("MOVIE HOST FAIL v1 rendering/timing"); return false; }
	source.arm(0);
	if (!run("carry") || !session.movies.eventDone() || session.movies.bedActive() || session.sound.currentVoice() != "none") { warning("MOVIE HOST FAIL natural voice cleanup"); return false; }
	source.arm(0);
	if (!run("segmenttail") || !source.sawCueVoice || !session.movies.eventDone()) { warning("MOVIE HOST FAIL segment voice cleanup"); return false; }
	source.arm(0);
	if (!run("quiet") || !session.movies.eventDone()) { warning("MOVIE HOST FAIL absent cue cleanup"); return false; }
	source.arm(0);
	if (!session.sound.playVoiceClip("prior", prior) || !run("voicewait") || !source.sawPriorVoice || session.sound.currentVoice() != "none") { warning("MOVIE HOST FAIL global voice wait or missing cue bank fallback"); return false; }
	source.arm(4);
	if (!run("loopmovie") || source.sawLoopedCue != retail || !session.movies.eventDone() || session.sound.currentVoice() != "none") { warning("MOVIE HOST FAIL edition cue loop flag or shared voice cleanup"); return false; }
	if (!source.sawOrderedBed) { warning("MOVIE ORDER FAIL interactive v4 repeats removed"); return false; }
	Common::Array<Common::SharedPtr<AudioClip> > music; music.push_back(prior);
	if (!session.sound.playMovieTheme("game-theme", music, 0)) return false;
	source.arm(4);
	if (!run("silent") || session.sound.currentTheme() != "game-theme") { warning("MOVIE THEME FAIL silent movie changed game music"); return false; }
	session.sound.setWaveVolume(4); source.arm(4); source.checkTheme = true; source.themeHandle = SoundAudit::themeHandle(session.sound);
	if (!run("loopmovie") || source.changedThemeHandle || !source.sawQueuedBed || !source.sawFullThemeGain || session.sound.themeActive()) { warning("MOVIE THEME FAIL shared queued channel first bed gain final cleanup"); return false; }
	source.checkTheme = false; session.sound.setWaveVolume(9);
	// A stale queued click must be discarded when the modal picture loads.
	Common::Event stale; stale.type = Common::EVENT_LBUTTONDOWN; stale.mouse = Common::Point(10, 10);
	session.pendingEvents.push_back(stale); system->getEventManager()->pushEvent(stale);
	source.arm(1);
	if (!run("click") || source.stage != 2 || !session.pendingEvents.empty() || !session.movies.eventDone() || session.movies.bedActive()) { warning("MOVIE HOST FAIL modal clicks/cleanup"); return false; }
	source.arm(2);
	if (!run("click") || source.stage != 1 || !session.movies.eventDone()) { warning("MOVIE HOST FAIL escape"); return false; }
	const bool hadHints = ConfMan.hasKey("dreamfactory_hotspot_hints", Common::ConfigManager::kTransientDomain);
	const Common::String oldHints = hadHints ? ConfMan.get("dreamfactory_hotspot_hints", Common::ConfigManager::kTransientDomain) : Common::String();
	ConfMan.setBool("dreamfactory_hotspot_hints", true, Common::ConfigManager::kTransientDomain);
	session.movies.setHotspotHints(true); source.arm(5);
	const bool hintsPass = run("click") && source.stage == 3 && source.hintsShown && source.hintsHidden && source.hintsRestored && session.movies.hotspotHints();
	Common::Event toggle; toggle.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; toggle.customType = 1; session.postEvent(toggle);
	const bool worldToggle = !session.movies.hotspotHints();
	if (hadHints) ConfMan.set("dreamfactory_hotspot_hints", oldHints, Common::ConfigManager::kTransientDomain);
	else ConfMan.removeKey("dreamfactory_hotspot_hints", Common::ConfigManager::kTransientDomain);
	if (!hintsPass || !worldToggle) { warning("MOVIE HINTS FAIL modal annotation/toggle/cleanup stage=%u shown=%u hidden=%u restored=%u world=%u", source.stage, source.hintsShown, source.hintsHidden, source.hintsRestored, worldToggle); return false; }
	debug("MOVIE HINT HOST PASS held frame marker toggle redraw original restoration click geometry shared world toggle");
    source.arm(6);
    if(!run("click") || source.stage!=2 || !source.cursorOnly || session.movies.active()) { warning("GAMEPAD MOVIE FAIL hotspot selection/confirm"); return false; }
    debug("GAMEPAD MOVIE PASS authored region focus confirm modal close");
	for (const char *handler : {"control", "control1"}) for (int mode = 7; mode <= 10; ++mode) {
		source.arm(mode);
		if (!run(handler) || source.stage != uint32(mode == 7 ? 4 : mode == 10 ? 1 : 3) || session.movies.active()) {
			warning("GAMEPAD MOVIE POINTER FAIL movie=%s mode=%d stage=%u", handler, mode, source.stage); return false;
		}
	}
	debug("GAMEPAD MOVIE POINTER PASS v1/v4 cursor target outside miss focus-to-pointer shoulder warp touch parity segment offsets");

	session.movies.reset(); if (session.movies.actions()) { warning("MOVIE HOST FAIL reset action flags"); return false; }
	source.arm(3); session.runtime.run(test, "quitmovie");
	if (source.stage != 1 || !session.runtime.error.empty() || session.runtime.globals["afterquit"].truth() || session.movies.bedActive() || !session.movies.eventDone() || session.pendingEvents.empty()) { warning("MOVIE HOST FAIL quit/abandoned script"); return false; }
	debug("MOVIE VOICE PASS retail=%u shared voice replaces prior voice preserves both effects missing cue no bank fallback follow preservation global voice wait edition loop flag segment and natural exit cleanup", retail ? 1 : 0);
	debug("MOVIE ORDER PASS v1 authored directory repeats unused audio exclusion malformed bounds v4 interactive repeated chunks");
	debug("MOVIE THEME PASS silent movie preserves game music first bed queues behind game theme same physical handle repeat root full relative gain device master volume final cleanup");
	debug("MOVIE HOST PASS modal scripts v1 v4 pixels palette voice bed inheritance shared voice cleanup clicks escape quit"); return true;
}
static bool spriteSelfTest() {
	// Transparent, opaque zero, literal and previous-row runs, with signed anchors.
	const byte encoded[] = { 2, 0, 5, 0, 254, 255, 253, 255, 6, 0, 5, 10, 0, 11, 7, 9, 4, 0, 8, 7, 3, 9 };
	const byte pixels[] = { 0, 0, 0, 7, 9, 0, 0, 3, 0, 0 };
	const byte opaque[] = { 0, 1, 1, 1, 1, 0, 1, 1, 0, 0 };
	Bytes data; data.resize(sizeof(encoded)); memcpy(data.data(), encoded, sizeof(encoded)); Sprite sprite;
	if (!decodeSprite(data, sprite) || sprite.width != 5 || sprite.height != 2 || sprite.x != -3 || sprite.y != -2 || memcmp(sprite.pixels.data(), pixels, sizeof(pixels)) || memcmp(sprite.opaque.data(), opaque, sizeof(opaque))) return false;
	for (uint32 test = 0; test < 7; ++test) {
		Bytes bad = data;
		if (test == 0) bad.resize(7);
		if (test == 1) bad.resize(bad.size() - 1);
		if (test == 2) bad[10] = 4; // No preceding row exists.
		if (test == 3) bad[10] = 25; // Run exceeds the row width.
		if (test == 4) bad[10] = 1; // Row does not fill its width.
		if (test == 5) bad[9] = 128; // Negative segment length.
		if (test == 6) bad[3] = 128; // Negative width.
		if (decodeSprite(bad, sprite) || sprite.width != 5 || sprite.height != 2 || sprite.x != -3 || sprite.y != -2 || memcmp(sprite.pixels.data(), pixels, sizeof(pixels)) || memcmp(sprite.opaque.data(), opaque, sizeof(opaque))) return false;
	}
	return true;
}
bool auditCasts(const Common::FSNode &root) {
	Common::FSList files; uint32 count = 0, failures = 0;
	if (!spriteSelfTest()) { warning("CAST FAIL sprite boundary self-test"); return false; }
	if (!corpusFiles(root, files)) { warning("CAST FAIL corpus listing"); return false; }
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root);
		if (!name.hasSuffixIgnoreCase(".cst")) continue;
		Bytes data; Common::SharedPtr<LoadedCast> loaded(new LoadedCast()); Cast &cast = loaded->data;
		if (!read(files[i], data) || !cast.load(data)) { warning("CAST FAIL %s directory", name.c_str()); ++failures; continue; }
		AuditHash meta, pixels, selection, art; uint32 poses = 0, frames = 0, pictures = 0;
		Runtime runtime; Resources resources; CastSystem actors(resources, runtime);
		meta.number(cast.version); meta.number(cast.mainScript);
		for (uint32 c = 0; c < 768; ++c) meta.byteValue(cast.palette[c]);
		for (uint32 m = 0; m < cast.members.size(); ++m) {
			const CastMember &member = cast.members[m]; meta.text(member.name); meta.number(member.logic); meta.number(member.script); meta.number(member.poses.size());
			for (uint32 p = 0; p < member.poses.size(); ++p) {
				const CastPose &pose = member.poses[p]; ++poses;
				meta.text(pose.name); meta.number(pose.location); meta.number(pose.record); meta.number(pose.frameCount); meta.number(pose.steps.size());
				for (uint32 s = 0; s < pose.steps.size(); ++s) {
					meta.number(pose.steps[s].size());
					for (uint32 f = 0; f < pose.steps[s].size(); ++f) {
						const CastFrame &frame = pose.steps[s][f]; ++frames;
						meta.number(frame.location); meta.number(frame.record); meta.number(frame.direction); meta.number(frame.angle); meta.number(frame.refScale);
						Sprite sprite;
						if (!decodeSprite(cast.archive.get(frame.location), sprite)) { warning("CAST FAIL %s sprite=%u", name.c_str(), frame.location); ++failures; continue; }
						pixels.number(sprite.width); pixels.number(sprite.height); pixels.number(sprite.y); pixels.number(sprite.x);
						for (uint32 b = 0; b < sprite.pixels.size(); ++b) pixels.byteValue(sprite.pixels[b]);
						for (uint32 b = 0; b < sprite.opaque.size(); ++b) pixels.byteValue(sprite.opaque[b]);
					}
				}
				meta.number(pose.play.size()); for (uint32 s = 0; s < pose.play.size(); ++s) meta.number(pose.play[s]);
				Common::SharedPtr<Actor> actor(new Actor()); actor->cast = loaded; actor->member = m; actor->pose = pose.name; actor->x = 160; actor->y = 20;
				WorldCamera probe; probe.focal = 48; probe.width = 96; probe.height = 72;
				for (uint32 step = 0; step < MAX<uint32>(1, pose.play.size()); ++step) for (int32 degree = 0; degree < 256; ++degree) {
					actor->step = step; actor->deg = degree; const auto frame = actor->frameRecord(&probe); selection.number(frame ? frame->location : 0); selection.number(actor->referenceScale());
				}
				actor->visible = true; actor->scale = 1500; actor->deg = 73;
				const uint32 steps[] = {0, pose.play.size() / 2, pose.play.empty() ? 0 : pose.play.size() - 1}; const int32 headings[] = {0, 37, 128, 255};
				for (uint32 step = 0; step < 3; ++step) for (uint32 h = 0; h < 4; ++h) {
					actor->step = steps[step]; const double angle = headings[h] * M_PI / 128;
					actor->x = 13 + int32(floor(160 * cos(angle) + 0.5)) + int32(floor(20 * sin(angle) + 0.5));
					actor->y = -27 + int32(floor(160 * sin(angle) + 0.5)) - int32(floor(20 * cos(angle) + 0.5)); actor->z = 35;
					WorldCamera camera; camera.x = 13; camera.y = -27; camera.z = 45; camera.degree = headings[h]; camera.focal = 48; camera.width = 96; camera.height = 72;
					Frame depth; depth.width = 96; depth.height = 72; depth.depth.resize(96 * 72); for (uint32 pixel = 0; pixel < depth.depth.size(); ++pixel) depth.depth[pixel] = ((pixel / 96) * 7 + (pixel % 96) * 3) % 16;
					Occlusion occlusion; occlusion.frame = &depth; occlusion.scale = 32; occlusion.bias = cast.version == 1 ? 64 : 0;
					actors.actors.clear(); actors.actors.push_back(actor);
					Bytes rgba; rgba.resize(96 * 72 * 4); for (uint32 pixel = 0; pixel < rgba.size(); pixel += 4) { rgba[pixel] = 19; rgba[pixel + 1] = 31; rgba[pixel + 2] = 47; rgba[pixel + 3] = 255; }
					for (const auto &entry : actors.drawList(camera, "")) if (!actors.compositeOne(entry, rgba, 96, 72, cast.palette, camera, occlusion)) ++failures;
					for (uint32 pixel = 0; pixel < rgba.size(); ++pixel) art.byteValue(rgba[pixel]); ++pictures;
				}
				actor->worldSpace = false; actor->scale = 0;
				const uint32 screenSteps[] = {0, pose.play.empty() ? 0 : pose.play.size() - 1}; const int32 degrees[] = {0, 73};
				for (uint32 step = 0; step < 2; ++step) for (uint32 d = 0; d < 2; ++d) {
					actor->step = screenSteps[step]; actor->deg = degrees[d]; const auto frame = actor->frameRecord(nullptr); const Sprite *sprite = frame ? loaded->frame(frame->location) : nullptr;
					actor->anchorX = (sprite ? sprite->x : 0) - 3; actor->anchorY = (sprite ? sprite->y : 0) + 2;
					actors.actors.clear(); actors.actors.push_back(actor);
					Bytes rgba; rgba.resize(96 * 72 * 4); for (uint32 pixel = 0; pixel < rgba.size(); pixel += 4) { rgba[pixel] = 19; rgba[pixel + 1] = 31; rgba[pixel + 2] = 47; rgba[pixel + 3] = 255; }
					if (!actors.compositeScreen(rgba, 96, 72, cast.palette, "")) ++failures;
					for (uint32 pixel = 0; pixel < rgba.size(); ++pixel) art.byteValue(rgba[pixel]); ++pictures;
				}

			}
		}
		debug("ACTORART %s pictures=%u selection=%08x rgba=%08x", name.c_str(), pictures, selection.hash, art.hash);
		debug("CAST %s members=%u poses=%u frames=%u meta=%08x pixels=%08x", name.c_str(), cast.members.size(), poses, frames, meta.hash, pixels.hash); ++count;
	}
	debug("CAST %s files=%u failures=%u", failures ? "FAIL" : "PASS", count, failures); return !failures;
}
static Bytes audioFixture(uint16 codec, const byte *body, uint32 length, uint32 outputBytes) {
	Bytes data; data.resize(48 + length); memset(data.data(), 0, data.size());
	WRITE_LE_UINT32(data.data(), 0x10000); WRITE_LE_UINT16(data.data() + 26, codec);
	WRITE_LE_UINT32(data.data() + 28, 22050); WRITE_LE_UINT32(data.data() + 36, outputBytes); WRITE_LE_UINT32(data.data() + 44, 48);
	if (length) memcpy(data.data() + 48, body, length); return data;
}
static bool audioSelfTest() {
	const byte v40[] = { 0x00, 0x81, 0x78, 0x8f, 0xc2, 0x7f };
	const int16 expected40[] = { -16384, -14592, -16640, -18688, -18944, -18944, -18944, -18944, 16128 };
	const byte v41[] = { 0x3f, 0x40, 0xbf, 0x3f, 0xc0, 0x7f };
	const int16 expected41[] = { 2016, -32, 32256, -31264, -32768, 32736 };
	AudioClip clip;
	const Bytes valid40 = audioFixture(1, v40, sizeof(v40), sizeof(expected40) / 2);
	const Bytes valid41 = audioFixture(2, v41, sizeof(v41), sizeof(expected41));
	if (!decodeAudio(valid40, clip) || clip.rate != 22050 || clip.samples.size() != ARRAYSIZE(expected40) || memcmp(clip.samples.data(), expected40, sizeof(expected40))) return false;
	if (!decodeAudio(valid41, clip) || clip.rate != 22050 || clip.samples.size() != ARRAYSIZE(expected41) || memcmp(clip.samples.data(), expected41, sizeof(expected41))) return false;
	// Short, invalid, odd-sized and oversized chunks must fail transactionally.
	for (uint32 test = 0; test < 7; ++test) {
		Bytes bad = valid41;
		if (test == 0) bad.resize(47);
		if (test == 1) bad[0] = 1;
		if (test == 2) WRITE_LE_UINT16(bad.data() + 26, 3);
		if (test == 3) WRITE_LE_UINT32(bad.data() + 36, 3);
		if (test == 4) WRITE_LE_UINT32(bad.data() + 36, 0x7ffffffe);
		if (test == 5) WRITE_LE_UINT32(bad.data() + 44, bad.size() + 1);
		if (test == 6) bad.resize(bad.size() - 1);
		if (decodeAudio(bad, clip) || clip.rate != 22050 || clip.samples.size() != ARRAYSIZE(expected41) || memcmp(clip.samples.data(), expected41, sizeof(expected41))) return false;
	}
	Bytes truncated40 = valid40; truncated40.resize(truncated40.size() - 1);
	if (decodeAudio(truncated40, clip)) return false;
	return decodeAudio(audioFixture(1, nullptr, 0, 0), clip) && clip.samples.empty();
}
static bool liveLoopStreamTest() {
	Common::SharedPtr<AudioClip> clip(new AudioClip()); clip->rate = 1000;
	for (int16 n = 1; n <= 4; ++n) clip->samples.push_back(n);
	Common::SharedPtr<SoundLoopControl> control(new SoundLoopControl());
	Common::ScopedPtr<Audio::AudioStream> stream(makePCMStream(clip.get(), false, 0, control.get()));
	int16 out[10] = {};
	if (stream->readBuffer(out, 2) != 2 || out[0] != 1 || out[1] != 2) return false;
	control->loop = true;
	if (stream->readBuffer(out, 5) != 5 || out[0] != 3 || out[1] != 4 || out[2] != 1 || out[4] != 3) return false;
	control->loop = false;
	if (stream->endOfData() || stream->readBuffer(out, 10) != 1 || out[0] != 4 || !stream->endOfData()) return false;
	warning("LOOP STREAM PASS enable during playback wrap disable drains exact PCM tail"); return true;
}
static bool themeStreamTest() {
	auto clip = [](uint32 rate, int16 a, int16 b, bool pair) {
		Common::SharedPtr<AudioClip> pcm(new AudioClip()); pcm->rate = rate;
		pcm->samples.push_back(a); if (pair) pcm->samples.push_back(b); return pcm;
	};
	const auto a = clip(44100, 1, 2, true), b = clip(22050, 10, 20, true), c = clip(44100, 30, 0, false), d = clip(44100, 40, 50, true);
	ThemeQueue queue;
	for (const auto &pcm : {a, b, c, b}) { ThemePart part; part.clip = pcm; queue.parts.push_back(part); }
	queue.repeat = 2;
	Common::ScopedPtr<Audio::AudioStream> stream(makeThemeStream(&queue));
	int16 out[32] = {};
	const int16 initial[] = {1, 2, 10, 15, 20, 20, 30, 10, 15, 20, 20, 30};
	if (stream->readBuffer(out, ARRAYSIZE(initial)) != ARRAYSIZE(initial) || memcmp(out, initial, sizeof(initial))) return false;
	// Append while inside the old repeated tail. The new root takes effect at
	// the end of the pending list, retaining the old tail exactly once.
	ThemePart part; part.clip = d; queue.parts.push_back(part); queue.repeat = 4;
	const int16 appended[] = {10, 15, 20, 20, 40, 50, 40, 50};
	if (stream->readBuffer(out, ARRAYSIZE(appended)) != ARRAYSIZE(appended) || memcmp(out, appended, sizeof(appended))) return false;
	queue.parts.clear(); part.clip = b; queue.parts.push_back(part); queue.current = 0; queue.phase = 22050; queue.repeat = -1;
	if (stream->readBuffer(out, 20) != 3 || out[0] != 15 || out[1] != 20 || out[2] != 20 || !stream->endOfData()) return false;
	const auto low = clip(11025, 100, 0, false);
	queue.parts.clear(); part.clip = low; queue.parts.push_back(part); part.clip = c; queue.parts.push_back(part); queue.current = 0; queue.phase = 0;
	if (stream->readBuffer(out, 20) != 5 || out[0] != 100 || out[3] != 100 || out[4] != 30 || !stream->endOfData()) return false;
	warning("THEME STREAM PASS ordered repeated chunks lead-in repeat root append pending tail mixed rates fractional resume finite drain"); return true;
}
bool auditAudio(const Common::FSNode &root) {
	Common::FSList files; uint32 banks = 0, failures = 0;
	if (!audioSelfTest()) { warning("AUDIO FAIL codec boundary self-test"); return false; }
	if (!liveLoopStreamTest()) { warning("AUDIO FAIL live loop stream"); return false; }
	if (!themeStreamTest()) { warning("AUDIO FAIL theme queue stream"); return false; }
	if (!corpusFiles(root, files)) { warning("AUDIO FAIL corpus listing"); return false; }
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root);
		if (!name.hasSuffixIgnoreCase(".trk") && !name.hasSuffixIgnoreCase(".sfx") && !name.hasSuffixIgnoreCase(".11k") && !name.hasSuffixIgnoreCase(".snd")) continue;
		Bytes data; AudioBank bank;
		if (!read(files[i], data) || !bank.load(data)) { warning("AUDIO FAIL %s bank", name.c_str()); ++failures; continue; }
		AuditHash meta, pcm, themeHash; Common::HashMap<uint32, bool> used; uint32 chunks = 0; uint64 samples = 0;
		meta.number(bank.version); meta.text(bank.name); meta.number(bank.loops.size());
		for (uint32 l = 0; l < bank.loops.size(); ++l) { meta.number(bank.loops[l]); used[bank.loops[l]] = true; }
		meta.number(bank.soundNames.size());
		for (uint32 s = 0; s < bank.soundNames.size(); ++s) { const Common::String &id = bank.soundNames[s]; meta.text(id); meta.number(bank.singles[id]); used[bank.singles[id]] = true; }
		for (uint32 c = 0; c < bank.archive.size(); ++c) {
			if (!used.contains(c)) continue;
			AudioClip clip;
			if (!bank.clip(c, clip)) { warning("AUDIO FAIL %s container=%u decode", name.c_str(), c); ++failures; continue; }
			pcm.number(c); pcm.number(clip.rate); pcm.number(clip.samples.size());
			for (uint32 n = 0; n < clip.samples.size(); ++n) { pcm.byteValue(byte(clip.samples[n])); pcm.byteValue(byte(uint16(clip.samples[n]) >> 8)); }
			++chunks; samples += clip.samples.size();
		}
		AudioClip theme;
		if (!bank.loops.empty()) {
			if (!bank.theme(theme)) { warning("AUDIO FAIL %s theme", name.c_str()); ++failures; }
			else {
				themeHash.number(theme.rate); themeHash.number(theme.samples.size());
				for (uint32 n = 0; n < theme.samples.size(); ++n) { themeHash.byteValue(byte(theme.samples[n])); themeHash.byteValue(byte(uint16(theme.samples[n]) >> 8)); }
			}
		}
		warning("AUDIO %s loops=%u sounds=%u chunks=%u samples=%llu meta=%08x pcm=%08x theme=%08x", name.c_str(), bank.loops.size(), bank.soundNames.size(), chunks, (unsigned long long)samples, meta.hash, pcm.hash, themeHash.hash);
		++banks;
	}
	warning("AUDIO %s banks=%u failures=%u", failures ? "FAIL" : "PASS", banks, failures); return !failures;
}
class SchedulerTestHost : public LoopHost {
public:
	explicit SchedulerTestHost(Scheduler &scheduler) : scheduler(scheduler) {}
	Scheduler &scheduler;
	Common::String events, held;
	bool canFireLoop(const GameLoop &loop) override { return loop.handler != held; }
	bool fireLoop(const GameLoop &loop) override {
		events += loop.handler + "/";
		if (loop.handler == "replace") scheduler.make("actor", "later", "new", 2);
		if (loop.handler == "repeat") scheduler.make(loop.kind, loop.name, loop.handler, 1);
		return true;
	}
};
static bool schedulerSelfTest() {
	Scheduler scheduler; SchedulerTestHost host(scheduler); scheduler.reset();
	scheduler.make("ACTOR", "FIRST", "replace", 2); scheduler.make("actor", "later", "old", 2);
	scheduler.advance(49, 3, true, host);
	if (scheduler.frame || scheduler.loops[0]->count != 2) return false;
	scheduler.advance(50, 3, true, host); scheduler.advance(100, 3, true, host);
	if (host.events != "replace/" || scheduler.loops.size() != 1 || scheduler.loops[0]->handler != "new" || scheduler.loops[0]->count != 2) return false;
	scheduler.advance(150, 3, true, host); scheduler.advance(200, 3, true, host);
	if (host.events != "replace/new/" || !scheduler.loops.empty() || scheduler.frame != 4) return false;
	scheduler.reset(); host.events.clear(); scheduler.make("actor", "one", "repeat", 0);
	scheduler.advance(50, 3, true, host); scheduler.advance(100, 3, true, host);
	if (host.events != "repeat/repeat/" || scheduler.loops.size() != 1 || scheduler.loops[0]->count != 1) return false;
	scheduler.pause("ACTOR", "ALL", true); scheduler.advance(150, 3, true, host);
	if (host.events != "repeat/repeat/" || scheduler.loops[0]->count != 1) return false;
	scheduler.pause("actor", "one", false); scheduler.stop("actor", "all");
	if (!scheduler.loops.empty()) return false;
	scheduler.reset(); host.events.clear(); scheduler.make("actor", "one", "held", 2);
	scheduler.advance(50, 3, false, host); scheduler.advance(100, 3, false, host); scheduler.advance(150, 3, false, host);
	if (!host.events.empty() || scheduler.loops[0]->count != 0) return false;
	host.held = "held"; scheduler.advance(200, 3, true, host);
	if (!host.events.empty() || scheduler.loops.size() != 1) return false;
	host.held.clear(); scheduler.advance(250, 3, true, host);
	if (host.events != "held/" || !scheduler.loops.empty()) return false;
	scheduler.reset(); host.events.clear(); scheduler.make("actor", "late", "once", 3);
	scheduler.advance(5000, 3, true, host);
	if (scheduler.frame != 1 || scheduler.loops[0]->count != 2 || !host.events.empty()) return false;
	scheduler.advance(4999, 3, true, host); scheduler.advance(5050, 3, true, host);
	if (scheduler.frame != 2 || scheduler.loops[0]->count != 1) return false;
	scheduler.reset(); scheduler.make("actor", "name", " SOUNDFXS( ) ", -2);
	if (!scheduler.has("ACTOR", "NAME") || scheduler.loops[0]->handler != "SOUNDFXS" || scheduler.loops[0]->count != 1) return false;
	scheduler.reset();
	for (uint32 i = 0; i < 32; ++i) if (!scheduler.make("actor", Common::String::format("a%u", i), "idle", 1)) return false;
	if (scheduler.make("actor", "overflow", "idle", 1) || !scheduler.make("ACTOR", "A0", "replacement", 7) || scheduler.loops.size() != 32 || scheduler.loops[31]->count != 7) return false;
	scheduler.stop("actor", "all");
	return scheduler.loops.empty() && Scheduler::ticks(1000) == 60;
}
static bool propWorldSelfTest(Session &session) {
	AuditHash projection, bearings;
	const int32 coords[] = {(-2147483647 - 1), -32768, 0, 32767, 2147483647}, heights[] = {-32768, 0, 32767};
	WorldCamera probe; probe.x = 13; probe.y = -27; probe.z = 45; probe.focal = 48; probe.width = 96; probe.height = 72;
	for (int32 degree = 0; degree < 256; ++degree) for (uint32 x = 0; x < ARRAYSIZE(coords); ++x) for (uint32 y = 0; y < ARRAYSIZE(coords); ++y) for (uint32 z = 0; z < ARRAYSIZE(heights); ++z) {
		probe.degree = degree; ProjectedPoint point;
		const bool visible = projectPoint(probe, coords[x], coords[y], heights[z], point); projection.number(visible ? 1 : 0);
		if (visible) { projection.number(int32(point.x)); projection.number(int32(point.y)); projection.number(point.depth); }
	}
	for (uint32 x = 0; x < ARRAYSIZE(coords); ++x) for (uint32 y = 0; y < ARRAYSIZE(coords); ++y) bearings.number(worldBearing(coords[x], coords[y]));
	warning("GEOMETRY projection=%08x bearing=%08x", projection.hash, bearings.hash);
	PropSystem &table = session.propSystem; table.reset();
	Common::SharedPtr<LoadedShop> shop(new LoadedShop()); PropGroup group; PropState state;
	PropPicture picture; picture.location = 1; state.pictures.push_back(picture); group.states.push_back(state); shop->data.groups.push_back(group);
	Sprite back; back.width = back.height = 2; back.pixels.resize(4); back.opaque.resize(4);
	memset(back.pixels.data(), 1, 4); memset(back.opaque.data(), 1, 4); back.opaque[1] = 0; shop->frameCache[1] = back;
	Common::SharedPtr<Prop> far(new Prop()); far->shop = shop; far->visible = far->worldSpace = true; far->x = 96; far->scale = 1000;
	Common::SharedPtr<LoadedShop> frontShop(new LoadedShop()); frontShop->data = shop->data;
	Sprite face = back; memset(face.pixels.data(), 2, 4); face.opaque[2] = 0; frontShop->frameCache[1] = face;
	Common::SharedPtr<Prop> near(new Prop(*far)); near->shop = frontShop; near->x = 48; near->scale = 500;
	table.props.push_back(near); table.props.push_back(far); // Authored order opposes depth.
	WorldCamera camera; camera.width = camera.height = 8; camera.focal = 4;
	Frame depth; depth.width = depth.height = 8; depth.depth.resize(64); memset(depth.depth.data(), 255, 64);
	Occlusion occlusion; occlusion.frame = &depth; occlusion.scale = 32;
	byte palette[768] = {}; palette[3] = 71; palette[6] = 83;
	Bytes rgba; rgba.resize(8 * 8 * 4); memset(rgba.data(), 19, rgba.size());
	if (!table.compositeWorld(rgba, 8, 8, palette, camera, "room", occlusion) || rgba[(4 * 8 + 4) * 4] != 83 || rgba[(5 * 8 + 4) * 4] != 71 || rgba[(4 * 8 + 5) * 4] != 19) return false;
	if (table.hitWorld(4, 4, camera, "room", occlusion) != near || table.hitWorld(4, 5, camera, "room", occlusion) != far || table.hitWorld(5, 4, camera, "room", occlusion) || table.hitWorld(4, 8, camera, "room", occlusion)) return false;
	depth.depth[4 * 8 + 4] = 0; depth.depth[5 * 8 + 4] = 2; depth.depth[5 * 8 + 5] = 1;
	if (table.hitWorld(4, 4, camera, "room", occlusion) || table.hitWorld(4, 5, camera, "room", occlusion) || table.hitWorld(5, 5, camera, "room", occlusion) != near) return false;
	occlusion.bias = 128; if (table.hitWorld(5, 5, camera, "room", occlusion)) return false;
	occlusion.bias = 0; near->zclip = 100; if (table.hitWorld(4, 4, camera, "room", occlusion) != near) return false;
	near->set = "elsewhere"; if (table.hitWorld(5, 5, camera, "room", occlusion)) return false;
	near->set = "ROOM"; near->hidden = true; if (table.hitWorld(5, 5, camera, "room", occlusion)) return false;
	near->hidden = false; near->scale = 0; if (table.hitWorld(5, 5, camera, "room", occlusion)) return false;
	near->scale = 500; near->selectDegree(73, 1); if (!near->directional) return false;
	near->x = far->x = -1; if (!table.worldDrawList(camera, "room").empty()) return false;
	table.reset(); warning("PROP WORLD PASS projection scaling transparency depth sorting scenery equality bias zclip room visibility viewport hit tests"); return true;
}
static bool propRenderSelfTest(Session &session, ScriptInstance &script) {
	PropSystem &propTable = session.propSystem; const uint32 version = session.version;
	Common::SharedPtr<LoadedShop> shop(new LoadedShop()); shop->data.version = version;
	PropGroup group; PropState selector, variant, ordinary; selector.name = "selector"; variant.name = "variant"; ordinary.name = "ordinary";
	for (uint32 i = 0; i < 2; ++i) { PropPicture picture; picture.location = i + 1; picture.degree = 1 - i; selector.pictures.push_back(picture); }
	for (uint32 i = 0; i < 12; ++i) { PropPicture picture; picture.location = 1; picture.degree = i >= 5 && i < 11 ? 1 : 0; picture.group = i < 5 ? i + 1 : i < 11 ? i - 5 : 0; variant.pictures.push_back(picture); }
	variant.animated = true; for (int32 i = 5; i >= 0; --i) variant.play.push_back(i);
	for (uint32 i = 0; i < 3; ++i) { PropPicture picture; picture.location = 1; ordinary.pictures.push_back(picture); } ordinary.animated = true;
	group.states.push_back(selector); group.states.push_back(variant); group.states.push_back(ordinary); shop->data.groups.push_back(group);
	Common::SharedPtr<Prop> prop(new Prop()); prop->name = "probe"; prop->shop = shop; propTable.props.push_back(prop);
	if (prop->frameIndex() != 1) return false;
	prop->selectDegree(1, 1); if (prop->frameIndex() != 0 || !prop->locked) return false;
	prop->selectView("unknown", 2); if (prop->state()) return false;
	prop->selectView("variant", 2); if (!prop->animating || prop->frameCount() != 6 || prop->frameIndex() != 10) return false;
	prop->step = 2; prop->selectDegree(0, 2); if (prop->locked || prop->frameIndex() != 2 || prop->step != 2) return false;
	prop->selectView("variant", 3); prop->visible = true;
	const uint32 expected[] = {4, 3, 2, 1, 0, 11};
	for (uint32 i = 0; i < 6; ++i) { propTable.animate(1 + i * 50); if (prop->frameIndex() != expected[i]) return false; }
	propTable.animate(301); if (prop->frameIndex() != (version == 4 ? 4u : 11u)) return false;
	prop->selectDegree(0, 10); prop->selectView("ordinary", 10); if (!prop->locked || prop->animating) return false;
	prop->selectView("ordinary", 11); if (prop->locked || !prop->animating) return false;
	propTable.animate(400); propTable.animate(449); if (prop->step) return false;
	propTable.animate(450); if (prop->step != 1) return false;
	prop->visible = false; propTable.animate(500); if (prop->step != 1) return false;
	prop->visible = true; propTable.animate(550); if (prop->step != 2 || prop->animating) return false;
	prop->selectDegree(0, 12); prop->becomeWorld(); if (prop->locked || prop->variants || !prop->order.empty()) return false;
	prop->worldSpace = false; prop->selectView("selector", 13); prop->deg = 1; prop->anchorX = 0; prop->anchorY = 1; prop->dist = 5;
	Sprite back; back.width = 3; back.height = 2; back.x = -2; back.y = 1; back.pixels.resize(6); back.opaque.resize(6);
	const byte inks[] = {0, 4, 8, 9, 0, 6}, mask[] = {1, 0, 1, 0, 1, 1};
	memcpy(back.pixels.data(), inks, 6); memcpy(back.opaque.data(), mask, 6); shop->frameCache[1] = back;
	Common::SharedPtr<Prop> front(new Prop(*prop)); front->name = "front"; front->dist = -2;
	Common::SharedPtr<LoadedShop> replacement(new LoadedShop()); replacement->data = shop->data;
	Sprite face = back; face.pixels[0] = 7; face.opaque[4] = 0; replacement->frameCache[1] = face; front->shop = replacement;
	propTable.props.push_back(front); byte palette[768]; for (uint32 c = 0; c < 768; ++c) palette[c] = byte(c * 13);
	Bytes rgba; rgba.resize(8 * 4 * 4); memset(rgba.data(), 101, rgba.size());
	if (!propTable.composite(rgba, 8, 4, palette) || rgba[2 * 4] != palette[7 * 3] || rgba[3 * 4] != 101 || rgba[(8 + 3) * 4] != palette[0] || propTable.hitTest(2, 0) != front || propTable.hitTest(3, 1) != prop || propTable.hitTest(5, 0)) return false;
	front->hidden = true; if (propTable.hitTest(2, 0) != prop) return false;
	shop->persistent = false; if (propTable.hitTest(2, 0, true)) return false; shop->persistent = true;
	if (propTable.hitTest(2, 0, true) != prop) return false;
	prop->worldSpace = true; if (propTable.hitTest(2, 0)) return false;
	auto point = [&](int32 x, int32 y) { Values args; args.push_back(Value(x)); args.push_back(Value(y)); return session.runtime.run(script, "pointprobe", args).value.number(); };
	if (point(2, 0) != 1 || point(3, 0) || point(5, 0) || point(-1, 0) || !session.runtime.error.empty()) return false;
	propTable.reset();
	warning("PROP RENDER PASS degree selectors variant/group play order same-handler pinning fixed timing v1/v4 loops transparency anchors depth hit masks owner cache");
	return true;
}
static bool propWarmSelfTest(const Common::FSNode &root) {
	Session s; Bytes data; ScriptInstance test; test.name = "prop-warm-test"; Common::String error;
	if (!s.init(root) || !read(root.getChild("warm.dfscript"), data) || !loadScript(data, test.script, error) || !s.propSystem.open("warm.shp", nullptr, false)) return false;
	const auto prop = s.propSystem.get("source"); if (!prop || !prop->shop->frameCache.empty()) return false;
	prop->view = "unknown"; prop->step = 47; prop->visible = false; prop->hidden = true; prop->deg = 9; prop->anchorX = 42; prop->lastTick = 12345;
	s.propSystem.instance("source", "warmcopy");
	const auto one = s.runtime.run(test, "one");
	if (!s.runtime.error.empty() || one.value.number() != 12345 || prop->shop->frameCache.size() != 2 || prop->shop->frameCache[6].pixels[0] != 71 || prop->shop->frameCache[7].pixels[0] != 72) return false;
	const auto all = s.runtime.run(test, "all");
	if (!s.runtime.error.empty() || all.value.number() != 12345 || prop->shop->frameCache.size() != 3 || prop->shop->frameCache[9].pixels[0] != 73) return false;
	if (prop->view != "unknown" || prop->step != 47 || prop->visible || !prop->hidden || prop->deg != 9 || prop->anchorX != 42 || prop->lastTick != 12345) return false;
	s.propSystem.remove("stub"); prop->shop->frameCache.erase(9);
	s.runtime.run(test, "all");
	if (!s.runtime.error.empty() || prop->shop->frameCache.contains(9) || prop->shop->frameCache.size() != 2) return false;
	Session bad;
	if (!bad.init(root) || !bad.propSystem.open("badwarm.shp", nullptr, false)) return false;
	bad.runtime.run(test, "one"); if (bad.runtime.error.empty()) return false;
	warning("PROP WARM PASS version=%u decoded all-state sprites internal shop alias active props copies stable hidden state malformed-art rejection", s.version);
	return true;
}
bool auditProps(const Common::FSNode &root) {
	if (!propWarmSelfTest(root)) { warning("PROP WARM FAIL"); return false; }
	Session session; Bytes bytes; ScriptInstance test; test.name = "prop-test"; Common::String error;
	if (!session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("PROP FAIL fixture init %s", error.c_str()); return false; }
	if (!propWorldSelfTest(session)) { warning("PROP WORLD FAIL"); return false; }
	if (!propRenderSelfTest(session, test)) { warning("PROP RENDER FAIL"); return false; }
	const ScriptResult result = session.runtime.run(test, "test");
	const bool ok = session.runtime.error.empty() && result.signal == ScriptResult::kReturn && result.value.number() == 12345;
	warning("PROP %s version=%u value=%s props=%u shops=%u error=%s", ok ? "PASS" : "FAIL", session.version, result.value.text().c_str(), session.propSystem.props.size(), session.propSystem.shops.size(), session.runtime.error.c_str());
	if (!ok) return false;
	const ScriptResult hidden = session.runtime.run(test, "visibility");
	const auto source = session.propSystem.get("source"), copy = session.propSystem.get("copy"), stub = session.propSystem.get("stub");
	if (hidden.value.number() != 12345 || !source || !copy || !stub || !source->hidden || copy->hidden || stub->hidden || !source->visible || !copy->visible || stub->visible) { warning("PROP VISIBILITY FAIL broadcast"); return false; }
	const ScriptResult released = session.runtime.run(test, "release");
	if (released.value.number() != 12345 || source->hidden || copy->hidden || !stub->hidden || !session.runtime.error.empty()) { warning("PROP VISIBILITY FAIL release"); return false; }
	const ScriptResult pending = session.runtime.run(test, "preparestars");
	if (pending.value.number() != 12345 || !copy->starPending || source->starPending || stub->starPending) { warning("PROP STAR FAIL pending/explicit/sentinel"); return false; }
	StarTable table; table.version = session.version; Star a, b; a.name = "town.a"; a.x = -10; a.y = 20; a.z = 30; a.facing = 64; b.name = "town.b"; b.x = 40; b.y = 50; b.z = 60; b.facing = 224; table.stars.push_back(a); table.stars.push_back(b);
	session.bindStars(table, "other"); if (!copy->starPending) { warning("PROP STAR FAIL foreign room settlement"); return false; }
	session.bindStars(table, "TOWN");
	if (copy->starPending || !copy->worldSpace || copy->deg != 9 || copy->scale != (session.version == 1 ? 1000 : 0)) { warning("PROP STAR FAIL settlement/facing/scale"); return false; }
	if (session.runtime.run(test, "placedstars").value.number() != 12345 || !session.runtime.error.empty() || session.propSystem.get("fresh")->starPending) { warning("PROP STAR FAIL placement/copy %s", session.runtime.error.c_str()); return false; }
	StarTable other; other.version = session.version; session.bindStars(other, "elsewhere");
	if (session.runtime.run(test, "rememberedstar").value.number() != 12345 || copy->starPending || !session.runtime.error.empty()) { warning("PROP STAR FAIL remembered placement"); return false; }
	StarTable changed = table; changed.stars[0].x = -99; session.bindStars(changed, "town");
	if (copy->x != -10 || source->x != 7) { warning("PROP STAR FAIL re-seated explicit/resolved positions"); return false; }
	warning("PROP STAR PASS named placement pending room binding facing override fresh copies explicit placement sentinels remembered rooms scale defaults");
	session.propSystem.reset(); if (!session.propSystem.props.empty() || !session.propSystem.shops.empty()) { warning("PROP VISIBILITY FAIL reset"); return false; }
	warning("PROP VISIBILITY PASS visible-only hide named hide newly shown props release reset"); return true;
}
static bool actorRenderSelfTest(Session &session) {
	CastSystem &table = session.castSystem; table.reset();
	Common::SharedPtr<LoadedCast> cast(new LoadedCast()); CastMember member; CastPose pose; pose.name = "stand";
	Common::Array<CastFrame> first, second; CastFrame frame; frame.location = 1; first.push_back(frame); frame.location = 2; frame.angle = 128; frame.refScale = 600; first.push_back(frame); frame.location = 3; frame.angle = 0; second.push_back(frame);
	pose.steps.push_back(first); pose.steps.push_back(second); pose.play.push_back(1); pose.play.push_back(0); pose.play.push_back(1); member.poses.push_back(pose); CastPose empty; empty.name = "empty"; member.poses.push_back(empty); cast->data.members.push_back(member);
	Sprite sprite; sprite.width = sprite.height = 2; sprite.x = 1; sprite.y = 2; sprite.pixels.resize(4); sprite.opaque.resize(4); memset(sprite.opaque.data(), 1, 4); sprite.opaque[1] = 0;
	for (uint32 i = 1; i <= 3; ++i) { memset(sprite.pixels.data(), i, 4); cast->frameCache[i] = sprite; }
	Common::SharedPtr<Actor> actor(new Actor()); actor->cast = cast; actor->visible = true; actor->x = 96; actor->scale = 1000; actor->deg = 128; table.actors.push_back(actor);
	WorldCamera camera; camera.width = camera.height = 8; camera.focal = 4;
	if (!actor->frameRecord(&camera) || actor->frameRecord(&camera)->location != 3 || actor->referenceScale() != 96) return false;
	table.animate(); if (actor->step != 1 || actor->frameRecord(&camera)->location != 1) return false;
	actor->visible = false; table.animate(); if (actor->step != 2 || actor->frameRecord(&camera)->location != 3) return false;
	table.animate(); if (actor->step != 0) return false;
	actor->pose = "unknown"; if (actor->poseData() != &cast->data.members[0].poses[0]) return false; actor->pose = "stand"; actor->visible = true;
	Frame depth; depth.width = depth.height = 8; depth.depth.resize(64); memset(depth.depth.data(), 3, 64);
	Occlusion occlusion; occlusion.frame = &depth; occlusion.scale = 32;
	byte palette[768] = {}; palette[9] = 71; Bytes rgba; rgba.resize(8 * 8 * 4); memset(rgba.data(), 19, rgba.size());
	const auto list = table.drawList(camera, "room");
	if (list.size() != 1 || !table.compositeOne(list[0], rgba, 8, 8, palette, camera, occlusion) || rgba[(2 * 8 + 3) * 4] != 71 || rgba[(2 * 8 + 4) * 4] != 19 || table.hitTest(3, 2, camera, "room", occlusion) != actor || table.hitTest(4, 2, camera, "room", occlusion) || !table.onScreen(*actor, camera, "room")) return false;
	depth.depth[2 * 8 + 3] = 2; if (table.hitTest(3, 2, camera, "room", occlusion)) return false;
	actor->zclip = 32; if (table.hitTest(3, 2, camera, "room", occlusion) != actor) return false;
	occlusion.bias = 128; if (table.hitTest(3, 2, camera, "room", occlusion)) return false;
	actor->set = "elsewhere"; if (table.onScreen(*actor, camera, "room")) return false; actor->set = "ROOM";
	actor->x = -96; if (table.onScreen(*actor, camera, "room") || !table.drawList(camera, "room").empty()) return false;
	actor->worldSpace = false; actor->scale = 0; actor->anchorX = actor->anchorY = 4; actor->step = 1; actor->deg = 64;
	if (actor->frameRecord(nullptr)->location != 1 || table.hitTest(3, 2, camera, "room", occlusion) != actor || !table.onScreen(*actor, camera, "room")) return false;
	actor->pose = "empty"; actor->step = 0; table.animate(); table.animate(); if (actor->step != 1 || actor->frameRecord(nullptr)) return false;
	table.reset(); warning("ACTOR RENDER PASS play holds angle ties camera bearing reference scale fallback hidden animation empty poses scaling transparency depth hit tests screen actors room filters"); return true;
}
static bool actorWarmSelfTest(const Common::FSNode &root) {
	Session s; Bytes data; ScriptInstance test; test.name = "warm-test"; Common::String error;
	if (!s.init(root) || !read(root.getChild("warm.dfscript"), data) || !loadScript(data, test.script, error) || !s.castSystem.open("warm.cst", nullptr, false)) return false;
	const auto actor = s.castSystem.get("source"); if (!actor || !actor->cast->frameCache.empty()) return false;
	actor->pose = "unknown"; actor->step = 47; actor->visible = false; actor->x = 42;
	const auto first = s.runtime.run(test, "one");
	if (!s.runtime.error.empty() || first.value.number() != 12345 || actor->cast->frameCache.size() != 2 || actor->cast->frameCache[6].pixels[0] != 61 || actor->cast->frameCache[7].pixels[0] != 62 || actor->pose != "unknown" || actor->step != 47 || actor->visible || actor->x != 42) return false;
	const auto all = s.runtime.run(test, "all");
	if (!s.runtime.error.empty() || all.value.number() != 12345 || actor->cast->frameCache.size() != 3 || actor->cast->frameCache[9].pixels[0] != 63 || actor->step != 47) return false;
	Session bad;
	if (!bad.init(root) || !bad.castSystem.open("badwarm.cst", nullptr, false)) return false;
	bad.runtime.run(test, "one");
	if (bad.runtime.error.empty()) return false;
	warning("ACTOR WARM PASS version=%u decoded all-member sprites cast-wide preload stable hidden actor state malformed-art rejection", s.version);
	return true;
}
bool auditActors(const Common::FSNode &root, OSystem *system) {
	if (!actorWarmSelfTest(root)) { warning("ACTOR WARM FAIL"); return false; }
	if (!schedulerSelfTest()) { warning("ACTOR FAIL scheduler boundary test"); return false; }
	Session session; Bytes bytes; ScriptInstance test; test.name = "actor-test"; Common::String error;
	if (!session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("ACTOR FAIL fixture init %s", error.c_str()); return false; }
	if (!actorRenderSelfTest(session)) { warning("ACTOR RENDER FAIL"); return false; }
	const ScriptResult result = session.runtime.run(test, "test");
	const bool ok = session.runtime.error.empty() && result.signal == ScriptResult::kReturn && result.value.number() == 12345;
	warning("ACTOR %s version=%u value=%s actors=%u casts=%u error=%s", ok ? "PASS" : "FAIL", session.version, result.value.text().c_str(), session.castSystem.actors.size(), session.castSystem.casts.size(), session.runtime.error.c_str());
	if (!ok || !system) return false;
	const ScriptResult moved = session.runtime.run(test, "motiontest");
	const bool motionOK = session.runtime.error.empty() && moved.signal == ScriptResult::kReturn && moved.value.number() == 12345 && session.castSystem.get("copy") && session.castSystem.get("copy")->star == "walktoxyz";
	warning("MOTION %s value=%s error=%s", motionOK ? "PASS" : "FAIL", moved.value.text().c_str(), session.runtime.error.c_str());
	if (!motionOK) return false;
	const ScriptResult prepared = session.runtime.run(test, "preparestars");
	const auto actor = session.castSystem.get("copy"); Archive starArchive; StarTable starTable;
	if (prepared.value.number() != 12345 || !actor || !actor->starPending || !read(root.getChild("stars.set"), bytes) || !starArchive.load(bytes) || !starTable.load(starArchive, session.version)) { warning("STAR SESSION FAIL deferred placement setup"); return false; }
	session.bindStars(starTable, "other");
	if (!actor->starPending || actor->x != 1) { warning("STAR SESSION FAIL wrong room placement"); return false; }
	session.motion.start("copy", 99, 98, 97);
	session.bindStars(starTable, "town");
	if (!actor->starPending || actor->x != 1) { warning("STAR SESSION FAIL moving actor settlement"); return false; }
	session.motion.stop("copy"); session.bindStars(starTable, "town");
	if (actor->starPending || !actor->worldSpace) { warning("STAR SESSION FAIL settle pending"); return false; }
	const ScriptResult placed = session.runtime.run(test, "starstest");
	if (placed.value.number() != 12345 || !session.runtime.error.empty() || actor->starPending) { warning("STAR SESSION FAIL placement/walk value=%s error=%s", placed.value.text().c_str(), session.runtime.error.c_str()); return false; }
	const ScriptResult routed = session.runtime.run(test, "pathtest");
	if (routed.value.number() != 12345 || !session.runtime.error.empty()) { warning("STAR SESSION FAIL paths value=%s error=%s", routed.value.text().c_str(), session.runtime.error.c_str()); return false; }
	const ScriptResult last = session.runtime.run(test, "resumelast");
	if (last.value.number() != 12345 || !session.runtime.error.empty()) { warning("STAR SESSION FAIL resume last point"); return false; }
	StarTable other; other.version = session.version; session.bindStars(other, "elsewhere");
	if (session.version == 1) {
		const ScriptResult cached = session.runtime.run(test, "pathtest");
		if (cached.value.number() != 12345 || !session.runtime.error.empty()) { warning("STAR SESSION FAIL remembered paths"); return false; }
	} else if (session.stars.path("town.a", "town.b")) { warning("STAR SESSION FAIL v4 foreign path"); return false; }
	StarTable changed = starTable; changed.stars[0].x = -99; session.bindStars(changed, "town");
	if (!session.stars.find("town.a") || session.stars.find("town.a")->x != -99) { warning("STAR SESSION FAIL room override"); return false; }
	session.bindStars(starTable, "town"); session.bindStars(other, "elsewhere");
	const ScriptResult remembered = session.runtime.run(test, "starhistory");
	if (remembered.value.number() != 12345 || !session.runtime.error.empty()) { warning("STAR SESSION FAIL remembered room"); return false; }
	warning("STAR SESSION PASS pending placement facing numeric/named walks forward reverse resumed/cached paths overrides explicit placement");
	if (session.version == 1) {
		session.runtime.run(test, "invalidhitaxis");
		if (session.runtime.error.empty()) { warning("HITBOX FAIL invalid axis accepted"); return false; }
		session.runtime.error.clear(); session.runtime.run(test, "missinghitactor");
		if (session.runtime.error.empty()) { warning("HITBOX FAIL missing actor accepted"); return false; }
		session.runtime.error.clear();
		warning("HITBOX PASS signed words player/actor separation fresh copies axis validation missing target");
	}
	Session order;
	if (!order.init(root) || order.runtime.run(test, "orderprobe").value.number() != 1 || !order.castSystem.get("source") || order.castSystem.get("source")->dist != -2 || order.castSystem.get("copy")->dist != 0 || !order.runtime.error.empty()) { warning("ACTOR DIST FAIL order/getter/copy"); return false; }
	warning("ACTOR DIST PASS script screen order fresh copies missing or undrawable actor sentinel");
	Session timed(system);
	if (!timed.init(root)) { warning("TIMING FAIL fixture init"); return false; }
	const uint32 start = system->getMillis();
	const ScriptResult wall = timed.runtime.run(test, "walltest");
	const uint32 elapsed = system->getMillis() - start;
	// delay(6) holds 100 ms; a six-tick frame deadline can start partway
	// through a tick and therefore holds at least another 83 ms.
	const bool paced = timed.runtime.error.empty() && wall.signal == ScriptResult::kReturn && wall.value.number() == 12345 && elapsed >= 180;
	warning("TIMING %s elapsed=%u value=%s error=%s", paced ? "PASS" : "FAIL", elapsed, wall.value.text().c_str(), timed.runtime.error.c_str());
	return paced;
}
static bool auditBalls(Session &s, const ScriptInstance &test) {
	warning("BALL CHECK API");
	auto shop = Common::SharedPtr<LoadedShop>(new LoadedShop()); shop->name = "balls"; shop->main.reset(new ScriptInstance());
	s.propSystem.shops.push_back(shop);
	for (uint32 i = 0; i < 17; ++i) {
		auto p = Common::SharedPtr<Prop>(new Prop()); p->name = i == 0 ? "first" : i == 1 ? "second" : Common::String::format("ball%u", i);
		p->shop = shop; p->script.reset(new ScriptInstance(test)); p->script->name = p->name; p->z = 100;
		s.propSystem.props.push_back(p);
	}
	ScriptInstance api(test); const auto result = s.runtime.run(api, "test");
	if (!s.runtime.error.empty() || result.value.number() != 12345 || s.balls.count() || !s.crickets.crickets.empty()) { warning("BALL FAIL API %s", s.runtime.error.c_str()); return false; }
	auto p = s.propSystem.get("first"); BallSystem &balls = s.balls;
	warning("BALL CHECK physics");
	auto why = [&]() { return s.runtime.globals["ballwhy"].text(); };
	balls.make("first", 10, -2, 3, 1, 0, 2); balls.pause("all", true); balls.pause("first", true); balls.pause("all", false); balls.service();
	if (p->x || balls.balls[0]->frames != 2 || balls.balls[0]->paused != 1) return false;
	balls.pause("first", false); balls.service();
	if (p->x != 10 || p->y != -2 || p->z != 103 || balls.balls[0]->vz != 1) return false;
	balls.service(); balls.service();
	if (p->x != 20 || p->z != 104 || balls.count() || why() != "frames" || s.runtime.globals["ballactive"].truth()) { warning("BALL FAIL lifetime callback"); return false; }
	// Completed slots are cleared before the callback can make a replacement.
	s.runtime.globals["ballrestart"] = Value(1); balls.make("first", 1, 0, 0, 0, 0, 0); balls.service();
	if (!balls.has("first") || balls.balls[0]->frames != 2 || p->x != 20) return false;
	balls.stop("all");
	for (const auto &prop : s.propSystem.props) if (!balls.make(prop->name, 0, 0, 0, 0, 0, 1)) { if (balls.count() != 16) return false; }
	if (balls.count() != 16 || balls.has("ball16") || !balls.make("FIRST", 0, 0, 0, 0, 0, 1) || balls.index(1) != "first") return false;
	balls.stop("all");
	warning("BALL CHECK collision");
	auto room = Common::SharedPtr<LoadedRoom>(new LoadedRoom()); room->data.name = "range";
	room->main.reset(new ScriptInstance()); room->main->name = "range"; s.roomSystem.current = room;
	Scene wall; wall.x = 384; wall.y = 128; wall.build = true; room->data.scenes.push_back(wall);
	p->x = 250; p->y = 128; p->z = 10; balls.make("first", 20, 0, -20, 1, 1, 5); balls.service();
	if (p->x != 256 || p->z != 0 || balls.balls[0]->vx != -10 || balls.balls[0]->vz != 11) { warning("BALL FAIL bounce"); return false; }
	balls.stop("all"); p->x = 250; p->z = 100; balls.make("first", 20, 0, 0, 0, 0, 5); balls.service();
	if (balls.count() || why() != "building" || p->x != 270) return false;
	room->data.scenes.clear();
	auto actor = Common::SharedPtr<Actor>(new Actor()); actor->name = "target"; actor->set = "range"; actor->visible = true;
	actor->x = 100; actor->y = 100; actor->z = 100; actor->hitWidth = 30; actor->hitHeight = 5; s.castSystem.actors.push_back(actor);
	p->x = 0; p->y = 100; p->z = 125; balls.make("first", 200, 0, 0, 0, 0, 5); balls.service();
	if (balls.count() || why() != "target") { warning("BALL FAIL actor swept hitbox"); return false; }
	actor->visible = false; p->x = 0; balls.make("first", 200, 0, 0, 0, 0, 5); balls.service();
	if (!balls.count()) return false;
	balls.stop("all"); actor->visible = true; actor->set = "elsewhere"; p->x = 0; balls.make("first", 200, 0, 0, 0, 0, 5); balls.service();
	if (!balls.count()) return false;
	balls.stop("all"); s.playerHitbox = Common::Point(30, 5); s.roomSystem.camera.x = 100; s.roomSystem.camera.y = 100; s.roomSystem.camera.z = 100;
	p->x = 0; p->z = 25; balls.make("first", 200, 0, 0, 0, 0, 5); balls.service();
	if (balls.count() || why() != "player") { warning("BALL FAIL player hitbox"); return false; }
	s.playerHitbox = Common::Point(); p->x = 32760; p->y = 0; p->z = 100; balls.make("first", 20, 0, 0, 0, 0, 5); balls.service();
	if (p->x != -32756) return false;
	balls.stop("all"); p->x = 0; balls.make("first", 10, 0, 0, 0, 0, 5); s.scheduler.reset();
	warning("BALL CHECK scheduler");
	s.scheduler.advance(50, 3, false, s); if (p->x != 10 || balls.balls[0]->frames != 4) return false;
	warning("BALL PASS v1 API capacity replacement nested-pause 20Hz busy-service gravity bounce walls actor/player swept-hitbox signed-word lifetime callbacks");
	return s.runtime.error.empty();
}
static bool auditEarlySound(Session &session) {
	SoundSystem &sound = session.sound;
	session.crickets.reset(); sound.haltSounds();
	if (!sound.open("ambient.snd")) return false;
	for (const char *name : { "chirp", "other", "third", "aux", "late" }) sound.soundLoop(name, true);
	sound.playSound("chirp", SoundSystem::kSingle); sound.playSound("other", SoundSystem::kDual);
	if (sound.currentSound(1) != "other" || sound.currentSound(2) != "chirp") { warning("EARLY SOUND FAIL dual alias"); return false; }
	sound.playSound("third", SoundSystem::kBoth); sound.playSound("chirp", SoundSystem::kBoth);
	if (sound.currentSound(1) != "chirp" || sound.currentSound(2) != "third") { warning("EARLY SOUND FAIL unconditional lowest both"); return false; }
	sound.playSound("other", SoundSystem::kSingle);
	if (sound.currentSound(1) != "other" || sound.currentSound(2) != "third") return false;
	sound.playSound("chirp", SoundSystem::kMultiple);
	if (sound.currentSound(1) != "other" || sound.currentSound(2) != "third") { warning("EARLY SOUND FAIL low priority"); return false; }
	sound.haltSounds();
	if (!sound.open("aux.snd") || !sound.open("late.snd")) return false;
	sound.soundLoop("late", true);
	sound.close("aux.snd"); sound.playSound("late", SoundSystem::kSingle);
	if (!session.stageSystem.open("blank.stg", nullptr, false)) return false;
	Bytes saved; Common::String saveError;
	if (!session.saveState(saved, saveError) || !session.loadState(saved, saveError)) { warning("EARLY SOUND FAIL gap save %s", saveError.c_str()); return false; }
	sound.playSound("late", SoundSystem::kSingle);
	if (sound.currentSound(1) != "none" || sound.currentSound(2) != "late") { warning("EARLY SOUND FAIL restored priority gap"); return false; }
	if (!sound.open("aux.snd")) return false;
	sound.playSound("late", SoundSystem::kSingle);
	if (sound.currentSound(1) != "late" || sound.currentSound(2) != "late") { warning("EARLY SOUND FAIL open renumber or active slot ID copy"); return false; }
	// Leave only a zero-base bank, with active slots that still carry old IDs.
	// This must not be mistaken for a legacy all-zero Dust bank table.
	sound.close("aux.snd"); sound.close("ambient.snd");
	if (!sound.open("ambient.snd")) return false;
	sound.close("ambient.snd");
	if (!session.saveState(saved, saveError) || !session.loadState(saved, saveError)) return false;
	Audio::SoundHandle blocked;
	if (sound.playAmbient("late", 1, 0, blocked, SoundSystem::kMultiple)) { warning("EARLY SOUND FAIL lone bank copied priorities"); return false; }
	// Both slots have higher copied IDs, so the now-zero-base descriptor cannot
	// preempt either. Both must force the lowest despite this priority.
	sound.playSound("late", SoundSystem::kBoth);
	sound.playSound("late", SoundSystem::kSingle);
	if (sound.currentSound(1) != "late" || sound.currentSound(2) != "late") return false;
	Audio::SoundHandle previous;
	if (!sound.playAmbient("late", 1, 0, previous, SoundSystem::kMultiple)) return false;
	for (uint32 i = 0; i < 100; ++i) if (!sound.playSound("late", SoundSystem::kMultiple)) return false;
	if (sound.handleActive(previous) || sound.currentSound(1) != "late" || sound.currentSound(2) != "late") { warning("EARLY SOUND FAIL repeated replacement"); return false; }
	sound.haltSounds(); sound.close("late.snd");
	warning("EARLY SOUND PASS version=%u dual alias both lowest priority rejection bank close gap v4 save restore open renumber active slot copies lone zero-base bank 100 replacements", session.version);
	return true;
}
bool SoundAudit::banks(Session &session, OSystem *backend, const ScriptInstance &test) {
	SoundSystem &sound = session.sound; Audio::Mixer *mixer = backend->getMixer();
	const bool retail = sound.isTitanicRetail(), demo = sound.isTitanicDemo();
	auto reset = [&]() { sound.reset(); sound.setTitanicRetail(retail); sound.setTitanicDemo(demo); };
	reset(); if (!session.stageSystem.open("blank.stg", nullptr, false)) return false;
	ScriptInstance api(test);
	if (session.runtime.run(api, "themecontrols").value.number() != 12345 || !session.runtime.error.empty()) { warning("BANK OWNERS FAIL script theme controls %s", session.runtime.error.c_str()); return false; }
	reset();
	if (sound.themeVolume("music") != 0) return false;
	sound.setThemeVolume("music", 17);
	if (!sound.open("music.snd") || !sound.open("music2.snd") || sound.themeVolume("music") != 255 || sound.themeVolume("music2") != 255) return false;
	sound.setThemeVolume("music", 81); sound.setThemeVolume("music2", 49);
	if (!sound.playTheme("music")) return false;
	sound.setThemeVolume("music2", 151); sound.setWaveVolume(4);
	if (sound.themeVolume("music") != 81 || sound.themeVolume("music2") != 151 || sound.themeVolume() != 81 || mixer->getChannelVolume(sound._theme.handle) != 36) { warning("BANK OWNERS FAIL inactive theme changed physical gain"); return false; }
	Bytes saved; Common::String error;
	if (!session.saveState(saved, error) || !session.loadState(saved, error) || sound.themeVolume("music") != 81 || sound.themeVolume("music2") != 151 || mixer->getChannelVolume(sound._theme.handle) != 36) { warning("BANK OWNERS FAIL theme control restore %s", error.c_str()); return false; }
	sound.setWaveVolume(9); sound.haltTheme();
	if (!sound.open("music.snd") || sound._banks.size() != 3) return false;
	sound.setThemeVolume("music", 92);
	if (sound._banks[0]->themeVolume != 92 || sound._banks[2]->themeVolume != 255 || !sound.playTheme("music")) return false;
	sound.close("music.snd");
	if (sound.themeActive() || sound._banks.size() != 2 || sound.themeVolume("music") != 255) { warning("BANK OWNERS FAIL first theme close or hidden defaults"); return false; }
	reset();
	if (!sound.open("ambient.snd") || !sound.open("ambient.snd") || sound._banks.size() != 2 || sound._banks[0]->id == sound._banks[1]->id || sound.themeVolume("ambient.snd") != 0) return false;
	const auto tracks = sound.trackNames(); if (tracks.size() != 2 || tracks[0] != "ambient" || tracks[1] != "ambient") { warning("BANK OWNERS FAIL repeated non-music track enumeration"); return false; }
	sound.soundLoop("chirp", true); sound.setSoundVolume("chirp", 73); sound.setSoundPan("chirp", 41);
	if (!sound.playSound("chirp", SoundSystem::kVoice) || !session.saveState(saved, error) || !session.loadState(saved, error) || !voice(sound, mixer, 73, session.version == 1 ? 0 : -87)) return false;
	sound.close("ambient.snd");
	if (sound.currentVoice() != "none" || sound._banks.size() != 1 || sound.soundLoop("chirp") || sound.soundVolume("chirp") != 255 || sound.soundPan("chirp") != 128) return false;
	sound.soundLoop("chirp", true); sound.setSoundVolume("chirp", 120); sound.setSoundPan("chirp", 200);
	if (!sound.playSound("chirp", SoundSystem::kVoice) || !sound.open("ambient.snd")) return false;
	// The surviving first bank has ID 2; the new hidden bank reuses ID 1.
	// Saving by filename or table ordinal would attach the voice incorrectly.
	if (sound._banks[0]->id != 2 || sound._banks[1]->id != 1 || !session.saveState(saved, error) || !session.loadState(saved, error) ||
		sound._voice.bankID != 2 || !voice(sound, mixer, 120, session.version == 1 ? 0 : 72)) { warning("BANK OWNERS FAIL duplicate-instance v7 restore %s", error.c_str()); return false; }
	auto stringBytes = [](const Common::String &value) { Bytes bytes; bytes.resize(4 + value.size()); WRITE_LE_UINT32(bytes.data(), value.size()); memcpy(bytes.data() + 4, value.c_str(), value.size()); return bytes; };
	auto append = [](Bytes &to, const Bytes &from) { for (byte b : from) to.push_back(b); };
	auto locate = [&](const Bytes &needle, uint32 from) -> uint32 {
		for (uint32 at = from; at + needle.size() <= saved.size(); ++at) if (!memcmp(saved.data() + at, needle.data(), needle.size())) return at;
		return UINT32_MAX;
	};
	Bytes anchor = stringBytes(sound._banks[0]->key); append(anchor, stringBytes(sound._banks[0]->source));
	const uint32 bankAt = locate(anchor, 28); if (bankAt == UINT32_MAX || READ_LE_UINT32(saved.data() + bankAt - 4) != 2 || locate(anchor, bankAt + 1) == UINT32_MAX) return false;
	Reader r(saved, bankAt);
	for (uint32 i = 0; i < 2; ++i) for (uint32 field = 0; field < 2; ++field) { const uint32 n = r.u32(); r.seek(r.pos() + n); }
	r.seek(r.pos() + 8); // Two stored priority bases.
	for (uint32 i = 0; i < 2; ++i) for (uint32 map = 0; map < 3; ++map) {
		const uint32 count = r.u32();
		for (uint32 j = 0; j < count; ++j) { const uint32 n = r.u32(); r.seek(r.pos() + n + (map == 0 ? 1 : 4)); }
	}
	const uint32 fieldsAt = r.pos(); if (!r.good() || fieldsAt + 16 > saved.size() || READ_LE_UINT32(saved.data() + fieldsAt) != 2 || READ_LE_UINT32(saved.data() + fieldsAt + 8) != 1) return false;
	Bytes owner = stringBytes("chirp"); append(owner, stringBytes("ambient.snd"));
	const uint32 playAt = locate(owner, 28); if (playAt == UINT32_MAX) return false;
	const uint32 ownerAt = playAt + owner.size();
	const uint32 offsets[] = {fieldsAt, fieldsAt + 8, fieldsAt + 4, ownerAt, ownerAt};
	const uint32 values[] = {0, 2, 256, 0, 0x7fffffff};
	const Audio::SoundHandle live = sound._voice.handle;
	for (uint32 i = 0; i < ARRAYSIZE(offsets); ++i) {
		Bytes bad = saved; WRITE_LE_UINT32(bad.data() + offsets[i], values[i]);
		Common::MemoryReadStream payload(bad.data() + 28, bad.size() - 28); byte digest[16]; Common::computeStreamMD5(payload, digest); memcpy(bad.data() + 12, digest, 16);
		if (session.loadState(bad, error) || sound._voice.handle != live || !voice(sound, mixer, 120, session.version == 1 ? 0 : 72)) { warning("BANK OWNERS FAIL invalid owner save %u", i); return false; }
	}
	sound.close("ambient.snd");
	if (sound.currentVoice() != "none" || sound._banks.size() != 1 || sound.soundLoop("chirp") || sound.soundVolume("chirp") != 255 || sound.soundPan("chirp") != 128) return false;
	reset();
	warning("BANK OWNERS PASS theme defaults per-bank getter inactive controls preserve gain save restore repeated opens all-bank internal-name enumeration distinct IDs first close hidden defaults nonordinal v7 owners five invalid owner saves preserve live voice"); return true;
}
bool SoundAudit::directory(Session &session, OSystem *backend, const ScriptInstance &test) {
	if (session.version != 1) return true;
	SoundSystem &sound = session.sound; Audio::Mixer *mixer = backend->getMixer();
	ScriptInstance api(test);
	if (session.runtime.run(api, "sounddirectory").value.number() != 12345 || !session.runtime.error.empty()) return false;
	if (!sound.open("music.snd")) return false;
	SoundSystem::Bank *bank = sound.find("music");
	const auto names = sound.soundNames("music");
	const auto cue = sound.namedClip("cue");
	if (!bank || names.size() != 2 || names[0] != "cue" || names[1] != "cue" || bank->data.singles["cue"] != 1 || bank->data.soundIDs["cue"] != 1 ||
		!cue || cue->samples.size() != 100 || sound.namedClip("a") || sound.namedClip("b") || bank->data.loops.size() != 4) return false;
	sound.soundLoop("cue", true); sound.setSoundVolume("cue", 91);
	if (!sound.playSound("cue", SoundSystem::kVoice) || !sound.playTheme("music")) return false;
	// Recreate the old combined directory in this synthetic bank only, so its
	// serializer emits a genuine v7-shaped legacy state. No assets are changed.
	bank->data.singles["a"] = 3; bank->data.soundIDs["a"] = 3; bank->data.soundNames.push_back("a");
	bank->loops["a"] = true; bank->volumes["a"] = 44; bank->pans["a"] = 77;
	SoundSystem::Play obsolete; obsolete.name = "a"; obsolete.bank = bank->key; obsolete.bankID = bank->id;
	obsolete.clip = sound.decode(*bank, 3); obsolete.loop = true; obsolete.soundID = 3; obsolete.positionalVolume = 44;
	mixer->playStream(Audio::Mixer::kSFXSoundType, &obsolete.handle, sound.stream(obsolete), -1, 44);
	sound._plays.push_back(obsolete); sound._slots[1] = obsolete;
	session.runtime.globals["directory-probe"] = Value(123456);
	Bytes legacy; Common::String error;
	if (!session.saveState(legacy, error)) return false;
	bank->data.singles.erase("a"); bank->data.soundIDs.erase("a"); bank->data.soundNames.pop_back();
	const Audio::SoundHandle voiceHandle = sound._voice.handle;
	if (session.loadState(legacy, error) || sound._voice.handle != voiceHandle || !voice(sound, mixer, 91, 0)) { warning("DUST DIRECTORY FAIL modern save accepted retired music controls"); return false; }
	WRITE_LE_UINT32(legacy.data() + 4, 7); // v7 and v8 payload layouts are identical.
	if (!session.loadState(legacy, error) || session.runtime.globals["directory-probe"].number() != 123456 ||
		!voice(sound, mixer, 91, 0) || !sound.done(false) || sound.currentSound(2) != "none" || !sound.themeActive()) { warning("DUST DIRECTORY FAIL legacy migration %s", error.c_str()); return false; }
	bank = sound.find("music");
	if (!bank || bank->loops.contains("a") || bank->volumes.contains("a") || bank->pans.contains("a") || !bank->loops["cue"] || bank->volumes["cue"] != 91) return false;
	Bytes modern;
	if (!session.saveState(modern, error) || READ_LE_UINT32(modern.data() + 4) != 8 || !session.loadState(modern, error) || !voice(sound, mixer, 91, 0) || !sound.themeActive()) return false;
	sound.close("music"); session.runtime.globals.erase("directory-probe");
	warning("DUST DIRECTORY PASS compiled one-shot count duplicate record enumeration first named cue match music excluded from named lookup authored bed retained v7 retired controls and looping music effects removed world voice and theme preserved v8 strict rejection resave"); return true;
}
bool SoundAudit::themes(Session &session, OSystem *backend) {
	SoundSystem &sound = session.sound; Audio::Mixer *mixer = backend->getMixer();
	sound.haltTheme(); sound.setWaveVolume(9);
	if (!session.stageSystem.open("blank.stg", nullptr, false)) return false;
	auto clip = [](uint32 rate, int16 value) {
		Common::SharedPtr<AudioClip> pcm(new AudioClip()); pcm->rate = rate; pcm->samples.resize(rate * 2);
		for (auto &sample : pcm->samples) sample = value; return pcm;
	};
	if (!sound.open("music.snd") || !sound.playTheme("music") || !themeShape(sound, 4, session.version == 1 ? 0 : 1)) return false;
	ThemeQueue bankTheme;
	if (!sound.themeSnapshot(bankTheme) || bankTheme.parts[0].clip != bankTheme.parts[2].clip || bankTheme.parts[0].clip != bankTheme.parts[3].clip || bankTheme.parts[0].clip == bankTheme.parts[1].clip) return false;
	Common::Array<Common::SharedPtr<AudioClip> > after; after.push_back(clip(44100, 444));
	if (!sound.playMovieTheme("after-bank", after, 0)) return false;
	backend->delayMillis(650); sound.close("music.snd");
	if (!sound.themeActive() || sound.currentTheme() != "after-bank" || !themeShape(sound, 1, 0)) { warning("THEME QUEUE FAIL closing consumed bank stopped new music"); return false; }
	sound.haltTheme();
	Common::Array<Common::SharedPtr<AudioClip> > first, next;
	first.push_back(clip(11025, 111)); const auto b = clip(22050, 222), c = clip(44100, 333);
	next.push_back(b); next.push_back(c); next.push_back(b);
	if (!sound.queueTheme("queue-audit-old", "", first, 0, 77)) return false;
	const Audio::SoundHandle handle = sound._theme.handle; mixer->pauseHandle(handle, true);
	if (!sound.queueTheme("queue-audit-new", "", next, 1, 33) || sound._theme.handle != handle || mixer->getChannelVolume(handle) != 33 || !themeShape(sound, 4, 2)) return false;
	sound.setWaveVolume(4);
	if (mixer->getChannelVolume(handle) != 14 || sound.themeVolume() != 33) { warning("THEME QUEUE FAIL device master did not scale theme gain"); return false; }
	first.clear(); next.clear();
	Bytes saved; Common::String error;
	if (!session.saveState(saved, error) || !session.loadState(saved, error)) { warning("THEME QUEUE FAIL save %s", error.c_str()); return false; }
	mixer->pauseHandle(sound._theme.handle, true);
	ThemeQueue restored;
	if (!sound.themeSnapshot(restored) || restored.parts.size() != 4 || restored.repeat != 2 || restored.current != 0 ||
		restored.parts[1].clip != restored.parts[3].clip || restored.parts[0].clip->rate != 11025 || restored.parts[1].clip->rate != 22050 ||
		restored.parts[2].clip->rate != 44100 || sound.currentTheme() != "queue-audit-new" || sound.waveVolume() != 4 || sound.themeVolume() != 33 || mixer->getChannelVolume(sound._theme.handle) != 14) return false;
	// Locate this fixture's unique theme section, then reject invalid queue
	// cursors, repeat roots, pool references and lengths with valid checksums.
	uint32 at = 28;
	const char *tag = "queue-audit-new";
	while (at + 18 <= saved.size() && (READ_LE_UINT32(saved.data() + at) != 15 || memcmp(saved.data() + at + 4, tag, 15))) ++at;
	if (at + 18 > saved.size()) return false;
	Reader r(saved, at + 4 + 15); const uint32 bankLength = r.u32(); r.seek(r.pos() + bankLength);
	const uint32 themeOwnerAt = r.pos(); r.u32();
	const uint32 poolAt = r.pos(), pool = r.u32(); if (pool != 3) return false;
	for (uint32 i = 0; i < pool; ++i) { if (r.u8() != 1) return false; r.u32(); const uint32 n = r.u32(); r.seek(r.pos() + n * 2); }
	const uint32 countAt = r.pos(), count = r.u32(), referenceAt = r.pos(); if (count != 4) return false;
	uint32 partOwnerAt = 0;
	for (uint32 i = 0; i < count; ++i) { r.u32(); const uint32 n = r.u32(); r.seek(r.pos() + n); partOwnerAt = r.pos(); r.u32(); }
	const uint32 cursorAt = r.pos(); if (!r.good() || cursorAt + 16 > saved.size()) return false;
	const uint32 offsets[] = {poolAt, countAt, referenceAt, cursorAt, cursorAt + 4, cursorAt + 12, themeOwnerAt, partOwnerAt};
	const Audio::SoundHandle restoredHandle = sound._theme.handle;
	for (uint32 i = 0; i < ARRAYSIZE(offsets); ++i) {
		Bytes bad = saved; WRITE_LE_UINT32(bad.data() + offsets[i], i == 5 ? 0x7fffffff : 0xffffffff);
		Common::MemoryReadStream payload(bad.data() + 28, bad.size() - 28); byte digest[16]; Common::computeStreamMD5(payload, digest); memcpy(bad.data() + 12, digest, 16);
		if (session.loadState(bad, error) || sound._theme.handle != restoredHandle || !themeShape(sound, 4, 2) || sound.currentTheme() != "queue-audit-new") return false;
	}
	sound.setWaveVolume(9); if (mixer->getChannelVolume(sound._theme.handle) != 33) return false;
	sound.haltTheme(); if (sound.themeActive()) return false;
	warning("THEME QUEUE PASS authored bank order and root consumed bank close same physical handle ordered append immediate channel gain device master scale and restore pending root v7 PCM pool cursor save restore eight invalid queue saves preserve live state halt");
	return true;
}
bool SoundAudit::enhancements(const Common::FSNode &root, OSystem *backend) {
	Session session(backend); if (!session.init(root) || !session.stageSystem.open("blank.stg", nullptr, false)) return false;
	EnhancementPack pack; Common::Array<Common::SharedPtr<AudioClip> > parts; int32 repeat;
	const Common::String identity("00000000000000000000000000000000");
	if (pack.init(root.getChild("enhancements"), "wrong-edition") || pack.active() ||
		!pack.init(root.getChild("enhancements"), identity) || !pack.music("FLAT/music.snd", parts, repeat) || parts.size() != 2 || repeat != 1) return false;
	if (parts[0]->channels != 2 || parts[0]->rate != 1000 || parts[0]->frames() != 400 || parts[1]->frames() != 1400 ||
		parts[0]->samples[0] != 1 || parts[0]->samples[1] != -1 || parts[1]->samples[0] != 401 || parts[1]->samples[1] != -401) return false;
	ThemeQueue queue; queue.repeat = 1;
	for (const auto &clip : parts) { ThemePart part; part.clip = clip; queue.parts.push_back(part); }
	Common::ScopedPtr<Audio::AudioStream> stream(makeThemeStream(&queue, true)); int16 output[8];
	if (!stream->isStereo() || stream->readBuffer(output, 8) != 8) return false;
	for (uint32 i = 0; i < 8; i += 2) if (output[i] != -output[i + 1]) return false;
	queue.current = 1; queue.phase = uint64(parts[1]->frames()) * 44100;
	if (stream->readBuffer(output, 2) != 2 || output[0] != 401 || output[1] != -401) return false;
	bool keepIntro = false;
	if (!pack.music("FLAT/music.snd", parts, repeat, &keepIntro) || !keepIntro ||
		!pack.music("titanic1/DATA/BEDRAD1.TRK", parts, repeat, &keepIntro) || !keepIntro ||
		!pack.music("titanic2/DATA/BEDRAD1.TRK", parts, repeat, &keepIntro) || keepIntro ||
		!pack.music("plain", parts, repeat, &keepIntro) || keepIntro) return false;
	for (const char *source : {"missing", "escape", "invalid", "range", "intro-invalid"}) if (pack.music(source, parts, repeat) || !parts.empty()) return false;
	Bytes originalArt, artScreen; originalArt.resize(1); originalArt[0] = 1;
	artScreen.resize(640 * 480 * 4); memset(artScreen.data(), 77, artScreen.size());
	if (!pack.artwork("flat/blank.stg", "blank", originalArt, artScreen)) return false;
	const uint32 at = (20 * 640 + 10) * 4;
	if (artScreen[at] != 10 || artScreen[at + 1] != 20 || artScreen[at + 2] != 30 || artScreen[at + 3] != 255 || artScreen[at + 4] != 40 || artScreen[0] != 77) return false;
	const Bytes expectedArt = artScreen;
	for (const char *flat : {"missing", "bounds", "escape"}) if (pack.artwork("flat/blank.stg", flat, originalArt, artScreen) || artScreen != expectedArt) return false;
	originalArt[0] = 2;
	if (pack.artwork("flat/blank.stg", "blank", originalArt, artScreen) || artScreen != expectedArt) return false;
	originalArt[0] = 1;
	if (!pack.init(root.getChild("enhancements"), identity) || !pack.artwork("flat/blank.stg", "blank", originalArt, artScreen)) return false;
	warning("ARTWORK PACK PASS exact source flat original identity bounds traversal RGB patch fallback");
	SoundSystem &sound = session.sound;
	if (!sound.open("music.snd") || !sound.playTheme("music") || !themeShape(sound, 4, session.version == 1 ? 0 : 1)) return false;
	sound.haltTheme(); sound.close("music.snd");
	const uint32 originalPrefix = session.version == 1 ? 0 : 1;
	if (!sound.enhancements.init(root.getChild("enhancements"), identity) || !sound.open("music.snd") || !sound.playTheme("music") || !themeShape(sound, 2 + originalPrefix, 1 + originalPrefix)) return false;
	backend->getMixer()->pauseHandle(sound._theme.handle, true);
	ThemeQueue intro;
	if (!sound.themeSnapshot(intro) || intro.parts.size() != 2 + originalPrefix) return false;
	if (originalPrefix && (intro.parts[0].clip->channels != 1 || intro.parts[0].clip->frames() != 100 || intro.parts[0].clip->samples[0] != 0)) return false;
	intro.current = 0; intro.phase = 0;
	Common::ScopedPtr<Audio::AudioStream> introStream(makeThemeStream(&intro, true));
	Common::Array<int16> prefixPCM; prefixPCM.resize(originalPrefix * 8820 + 2);
	if (introStream->readBuffer(prefixPCM.data(), prefixPCM.size()) != int(prefixPCM.size())) return false;
	for (uint32 i = 0; i < originalPrefix * 8820; ++i) if (prefixPCM[i] != 0) return false;
	if (prefixPCM[prefixPCM.size() - 2] != 1 || prefixPCM.back() != -1) return false;
	intro.current = intro.parts.size() - 1; intro.phase = uint64(intro.parts.back().clip->frames()) * 44100;
	if (introStream->readBuffer(output, 2) != 2 || output[0] != 401 || output[1] != -401) return false;
	backend->getMixer()->pauseHandle(sound._theme.handle, false);
	warning("ENHANCEMENT AUTHORED INTRO PASS original mono prefix before stereo replacement intro preserved once excluded from repeat");
	Bytes saved; Common::String error;
	if (!session.saveState(saved, error) || READ_LE_UINT32(saved.data() + 4) != 9) return false;
	sound.haltTheme(); sound.enhancements.clear();
	if (!session.loadState(saved, error)) { warning("ENHANCEMENT MUSIC FAIL save/load %s", error.c_str()); return false; }
	ThemeQueue restored;
	if (!sound.themeSnapshot(restored) || restored.parts.empty() || restored.parts.size() > 2 + originalPrefix || restored.repeat != int32(restored.parts.size() - 1) || restored.parts.back().clip->channels != 2 || restored.parts.back().clip->samples[0] != 401) return false;
	JournalEntry journalTrack; journalTrack.text = "Heard line"; journalTrack.speaker = "Speaker";
	session.journal.add(journalTrack);
	if (!session.saveState(saved, error) || READ_LE_UINT32(saved.data() + 4) != 10 || !session.loadState(saved, error)) return false;
	if (session.journal.entries.size() != 1 || session.journal.entries[0].text != "Heard line" || !sound.themeSnapshot(restored) || restored.parts.back().clip->channels != 2) return false;
	session.journal.entries.clear();
	warning("JOURNAL STEREO SAVE PASS v10 history and remastered stereo music restored together");
	sound.haltTheme();
	warning("ENHANCEMENT MUSIC PASS edition identity stereo channels authored queue intro loop boundaries invalid missing traversal fallback original v8 stereo v9 embedded save without pack");
	return true;
}
static bool auditLiveControls(Session &session, OSystem *backend, const ScriptInstance &test) {
	SoundSystem &sound = session.sound; Audio::Mixer *mixer = backend->getMixer();
	if (!SoundAudit::banks(session, backend, test)) { warning("BANK OWNERS FAIL native controls/save integration"); return false; }
	if (!SoundAudit::directory(session, backend, test)) { warning("DUST DIRECTORY FAIL native separation/legacy save integration"); return false; }
	if (!SoundAudit::themes(session, backend)) { warning("THEME QUEUE FAIL native mixer/save integration"); return false; }
	if (!sound.open("ambient.snd")) return false;
	sound.setWaveVolume(9); sound.setSoundVolume("chirp", 120); sound.setSoundPan("chirp", 64);
	sound.soundLoop("chirp", false);
	if (!sound.playSound("chirp", SoundSystem::kSingle) || !sound.playSound("chirp", SoundSystem::kVoice)) return false;
	sound.soundLoop("chirp", true); sound.setSoundPan("chirp", 200); sound.setSoundVolume("chirp", 100);
	const int8 pan = session.version == 1 ? 0 : 72;
	if (!SoundAudit::channel(sound, mixer, "chirp", 100, pan) || !SoundAudit::voice(sound, mixer, 100, pan)) { warning("LIVE CONTROLS FAIL active volume/pan policy"); return false; }
	backend->delayMillis(2100);
	if (sound.done(false) || sound.done(true)) { warning("LIVE CONTROLS FAIL enabled loop did not repeat"); return false; }
	sound.soundLoop("chirp", false);
	if (sound.done(false) || sound.done(true)) { warning("LIVE CONTROLS FAIL immediate stop"); return false; }
	Bytes saved; Common::String error;
	if (!session.saveState(saved, error) || !session.loadState(saved, error)) { warning("LIVE CONTROLS FAIL draining save %s", error.c_str()); return false; }
	if (sound.soundLoop("chirp") || !SoundAudit::channel(sound, mixer, "chirp", 100, pan) || !SoundAudit::voice(sound, mixer, 100, pan)) { warning("LIVE CONTROLS FAIL drain offset or copied pan restore"); return false; }
	backend->delayMillis(2500);
	if (!sound.done(false) || !sound.done(true)) { warning("LIVE CONTROLS FAIL disabled loop did not drain"); return false; }
	if (!sound.playSound("chirp", SoundSystem::kSingle) || !SoundAudit::channel(sound, mixer, "chirp", 100, pan)) { warning("LIVE CONTROLS FAIL subsequent pan"); return false; }
	sound.haltSounds(); sound.close("ambient.snd");
	// Exercise natural mixer-thread completion and main-thread replacement with
	// clips whose caller releases its ownership immediately after playback starts.
	for (uint32 i = 0; i < 256; ++i) {
		{
			Common::SharedPtr<AudioClip> shortClip(new AudioClip()); shortClip->rate = 1000;
			shortClip->samples.resize(10); memset(shortClip->samples.data(), 0, 20);
			if (!sound.playVoiceClip("short-owned", shortClip)) return false;
		}
		backend->delayMillis(i % 8 ? 1 : 15);
	}
	sound.haltVoice(); if (!sound.done(true)) return false;
	warning("PCM OWNERSHIP PASS 256 short clips caller release natural mixer completion replacement explicit stop main-thread ownership");
	warning("LIVE CONTROLS PASS version=%u enable mid-sample SFX and bank voice live volume edition pan disable drain wrapped offset save restore Dust pan stored but inaudible", session.version);
	return true;
}
static bool auditBankControls(Session &session, Audio::Mixer *mixer) {
	SoundSystem &sound = session.sound;
	sound.haltSounds(); sound.close("ambient.snd");
	if (!sound.open("ambient.snd") || !session.stageSystem.open("blank.stg", nullptr, false)) return false;
	sound.soundLoop("chirp", true); sound.setSoundVolume("chirp", 73); sound.setSoundPan("chirp", 41);
	sound.setWaveVolume(9); sound.playSound("chirp", SoundSystem::kSingle);
	if (!sound.open("dup.snd")) return false;
	Bytes saved; Common::String error;
	if (!session.saveState(saved, error) || !session.loadState(saved, error)) { warning("BANK CONTROLS FAIL save %s", error.c_str()); return false; }
	if (!sound.soundLoop("chirp") || sound.soundVolume("chirp") != 73 || sound.soundPan("chirp") != 41) return false;
	if (!SoundAudit::channel(sound, mixer, "chirp", 73, session.version == 1 ? 0 : -87)) { warning("BANK CONTROLS FAIL active save levels"); return false; }
	Audio::SoundHandle cue;
	if (!sound.playClip("chirp", sound.namedClip("chirp"), cue)) return false;
	sound.close("ambient.snd");
	if (sound.soundLoop("chirp") || sound.soundVolume("chirp") != 255 || sound.soundPan("chirp") != 128) { warning("BANK CONTROLS FAIL hidden duplicate defaults"); return false; }
	sound.setWaveVolume(4);
	if (mixer->getChannelVolume(cue) != 32 || mixer->getChannelBalance(cue) != (session.version == 1 ? 0 : -87)) { warning("BANK CONTROLS FAIL playback snapshot"); return false; }
	sound.setWaveVolume(9); sound.haltSounds();
	sound.soundLoop("chirp", true); sound.setSoundVolume("chirp", 120); sound.setSoundPan("chirp", 200);
	sound.playSound("chirp", SoundSystem::kSingle);
	if (!sound.open("ambient.snd")) return false;
	if (!session.saveState(saved, error) || !session.loadState(saved, error)) return false;
	if (!SoundAudit::channel(sound, mixer, "chirp", 120, session.version == 1 ? 0 : 72)) { warning("BANK CONTROLS FAIL duplicate owner active save levels"); return false; }
	sound.close("dup.snd");
	if (sound.soundLoop("chirp") || sound.soundVolume("chirp") != 255 || sound.soundPan("chirp") != 128) { warning("BANK CONTROLS FAIL saved hidden duplicate"); return false; }
	sound.soundLoop("chirp", true); sound.setSoundVolume("chirp", 11); sound.setSoundPan("chirp", 9);
	sound.close("ambient.snd"); if (!sound.open("ambient.snd")) return false;
	if (sound.soundLoop("chirp") || sound.soundVolume("chirp") != 255 || sound.soundPan("chirp") != 128) { warning("BANK CONTROLS FAIL reopen reset"); return false; }
	const bool retail = sound.isTitanicRetail(), demo = sound.isTitanicDemo();
	// Synthetic compatibility fixture: an older save records the wrong policy.
	sound.setTitanicRetail(!retail);
	if (!session.saveState(saved, error)) return false;
	sound.setTitanicRetail(retail); sound.setTitanicDemo(demo);
	if (!session.loadState(saved, error) || sound.isTitanicRetail() != retail || sound.isTitanicDemo() != demo) { warning("BANK CONTROLS FAIL saved edition overrode live engine"); return false; }
	sound.close("ambient.snd");
	warning("BANK CONTROLS PASS version=%u hidden duplicate defaults per-bank v5 saves active volume pan restore reopen reset playback snapshot", session.version);
	return true;
}
bool auditCrickets(const Common::FSNode &root, OSystem *backend, bool retail, bool demo) {
	Session session(backend); Bytes bytes; ScriptInstance test; test.name = "ambient-test"; Common::String error;
	if (!SoundAudit::enhancements(root, backend)) { warning("ENHANCEMENT MUSIC FAIL fixture"); return false; }
	if (!session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("CRICKET FAIL fixture init %s", error.c_str()); return false; }
	if (session.version == 1) {
		if (!auditBalls(session, test)) return false;
		Session audioSession(backend);
		return audioSession.init(root) && auditEarlySound(audioSession) && auditBankControls(audioSession, backend->getMixer()) && auditLiveControls(audioSession, backend, test);
	}
	const ScriptResult result = session.runtime.run(test, "test");
	if (!session.runtime.error.empty() || result.value.number() != 12345) { warning("CRICKET FAIL API value=%s error=%s", result.value.text().c_str(), session.runtime.error.c_str()); return false; }
	CricketSystem &c = session.crickets; SoundSystem &sound = session.sound;
	sound.setTitanicRetail(retail);
	sound.setTitanicDemo(demo);
	for (uint32 i = 0; i < 16; ++i) if (!c.make(Common::String::format("missing%u", i), 0, 0, 0, 2, 0)) return false;
	if (c.make("overflow", 0, 0, 1, 0, 0) || !c.make("MISSING0", 0, 0, 0, 2, 0) || c.count() != 16 || c.crickets[0]->radius != 1 || c.index(1) != "missing0") { warning("CRICKET FAIL capacity replacement radius/order"); return false; }
	c.pause("all", true); c.service(); if (c.crickets[0]->count != 0 || c.crickets[0]->armed) return false;
	c.pause("all", false); c.service(); if (c.crickets[0]->count != 2 || !c.crickets[0]->armed) return false;
	c.service(); if (c.crickets[0]->count != 1) return false;
	c.service(); if (c.crickets[0]->count != 2) return false;
	c.reset(); c.make("missing", 0, 0, 1, 0, -1); c.service();
	if (!c.crickets.empty()) { warning("CRICKET FAIL missing one-shot removal"); return false; }
	c.make("random", 0, 0, 1, 7, 5); c.service(); const int64 seedCount = c.crickets[0]->count;
	for (uint32 i = 0; i < 100; ++i) { c.crickets[0]->count = 0; c.service(); if (c.crickets[0]->count < 8 || c.crickets[0]->count > 12) { warning("CRICKET FAIL jitter range"); return false; } }
	c.reset(); c.make("random", 0, 0, 1, 7, 5); c.service(); if (c.crickets[0]->count != seedCount) return false;
	c.reset(); c.make("busy", 0, 0, 1, 2, 0); session.scheduler.reset();
	session.scheduler.advance(50, 3, false, session); if (c.crickets[0]->count != 2 || !c.crickets[0]->armed) { warning("CRICKET FAIL busy arming"); return false; }
	session.scheduler.advance(100, 3, false, session); if (c.crickets[0]->count != 1) return false;
	session.scheduler.advance(150, 3, false, session); if (c.crickets[0]->count != 2) return false;
	c.reset(); warning("CRICKET PASS version=%u API capacity replacement pause countdown missing jitter busy reset", session.version);
	if (!backend || !sound.open("ambient.snd")) { warning("CRICKET MIXER FAIL bank or backend"); return false; }
	Audio::Mixer *mixer = backend->getMixer();
	sound.setSoundVolume("chirp", 180); sound.setSoundPan("chirp", 64); sound.setWaveVolume(9); sound.soundLoop("chirp", true);
	c.setRoom("Town"); c.listener(0, 0, 0); c.make("CHIRP", 0, 50, 100, 1, 0); c.service();
	Common::SharedPtr<Cricket> chirp = c.crickets[0];
	if (!chirp->playing || !sound.handleActive(chirp->handle) || mixer->getChannelVolume(chirp->handle) != 128 || mixer->getChannelBalance(chirp->handle) != 127 || sound.currentSound(2) != "chirp") { warning("CRICKET MIXER FAIL positional start vol=%u pan=%d", mixer->getChannelVolume(chirp->handle), mixer->getChannelBalance(chirp->handle)); return false; }
	const Audio::SoundHandle firstHandle = chirp->handle; c.service();
	if (chirp->handle != firstHandle) { warning("CRICKET MIXER FAIL restarted active sound"); return false; }
	sound.setSoundVolume("chirp", 1); sound.setSoundPan("chirp", -64);
	if (mixer->getChannelVolume(chirp->handle) != 128 || mixer->getChannelBalance(chirp->handle) != 127) { warning("CRICKET MIXER FAIL named controls changed position"); return false; }
	c.listener(0, 25, 0); c.service();
	if (chirp->handle != firstHandle || mixer->getChannelVolume(chirp->handle) != 192 || mixer->getChannelBalance(chirp->handle) != 127) { warning("CRICKET MIXER FAIL moving listener integer volume"); return false; }
	c.listener(25, 25, 0); c.service();
	if (chirp->handle != firstHandle || mixer->getChannelVolume(chirp->handle) != 166 || mixer->getChannelBalance(chirp->handle) != 64) { warning("CRICKET MIXER FAIL diagonal folded pan vol=%u pan=%d", mixer->getChannelVolume(chirp->handle), mixer->getChannelBalance(chirp->handle)); return false; }
	c.listener(0, 0, 64); c.service();
	if (chirp->handle != firstHandle || mixer->getChannelVolume(chirp->handle) != 128 || mixer->getChannelBalance(chirp->handle) != 0) { warning("CRICKET MIXER FAIL turning listener"); return false; }
	c.listener(0, -50, 0); c.service();
	if (sound.handleActive(firstHandle) || chirp->playing) { warning("CRICKET MIXER FAIL radius exit"); return false; }
	c.listener(0, 0, 0); c.service();
	if (!sound.handleActive(chirp->handle) || mixer->getChannelVolume(chirp->handle) != 128) { warning("CRICKET MIXER FAIL radius reentry"); return false; }
	sound.setWaveVolume(4); if (mixer->getChannelVolume(chirp->handle) != 56) return false; sound.setWaveVolume(9);
	const int64 pausedCount = chirp->count;
	c.pause("all", true); c.service(); if (!sound.handleActive(chirp->handle) || chirp->count != pausedCount) { warning("CRICKET MIXER FAIL pause"); return false; }
	c.setRoom("Elsewhere"); if (sound.handleActive(chirp->handle) || chirp->playing || !c.has("chirp")) { warning("CRICKET MIXER FAIL room silence"); return false; }
	c.setRoom("town"); c.pause("all", false); c.service(); if (!sound.handleActive(chirp->handle)) return false;
	c.clearListener(); sound.soundLoop("other", true); c.make("other", 0, 0, 1, 0, 0); c.service();
	Common::SharedPtr<Cricket> other = c.crickets[1];
	if (!sound.handleActive(chirp->handle) || !sound.handleActive(other->handle) || mixer->getChannelVolume(other->handle) != 255 || mixer->getChannelBalance(other->handle) != 0) { warning("CRICKET MIXER FAIL overlap or default listener"); return false; }
	c.stop("CHIRP"); if (sound.handleActive(chirp->handle) || !sound.handleActive(other->handle)) return false;
	sound.stopHandle(other->handle); c.service();
	if (demo ? sound.handleActive(other->handle) : !sound.handleActive(other->handle)) { warning("CRICKET MIXER FAIL repeat after completion"); return false; }
	if (demo) { c.listener(1000, 0, 0); c.service(); c.listener(0, 0, 0); c.service(); if (!sound.handleActive(other->handle)) { warning("CRICKET MIXER FAIL demo range rearm"); return false; } }
	c.listener(0, 0, 0); c.make("chirp", 100, 0, 100, 0, -1); c.service(); if (c.has("chirp")) return false;
	c.listener(0, 0, 128); c.make("chirp", 0, 50, 100, 0, 0); c.service(); chirp = c.crickets[0];
	if (!sound.handleActive(chirp->handle) || mixer->getChannelBalance(chirp->handle) != -127) { warning("CRICKET MIXER FAIL listener facing"); return false; }
	sound.close("ambient.snd"); if (sound.handleActive(other->handle)) { warning("CRICKET MIXER FAIL bank close"); return false; }
	if (sound.handleActive(chirp->handle) || !sound.open("ambient.snd")) return false;
	if (sound.soundLoop("chirp") || sound.soundLoop("other") || sound.soundVolume("chirp") != 255 || sound.soundPan("chirp") != 128) { warning("CRICKET MIXER FAIL bank reopen defaults"); return false; }
	sound.soundLoop("chirp", true); sound.soundLoop("other", true);
	if (demo) { c.listener(1000, 0, 0); c.service(); c.listener(0, 0, 128); }
	c.service(); if (!sound.handleActive(other->handle) || !sound.handleActive(chirp->handle)) return false;
	c.reset(); if (sound.handleActive(other->handle) || sound.handleActive(chirp->handle)) { warning("CRICKET MIXER FAIL reset handles"); return false; }
	c.setRoom("town"); c.listener(0, 0, 0); sound.soundLoop("chirp", false);
	c.make("chirp", 0, 50, 100, 3, 0); chirp = c.crickets[0];
	c.service();
	if (!chirp->armed || chirp->count != 3 || chirp->playing) { warning("CRICKET TIMER FAIL initial arm"); return false; }
	c.service(); if (chirp->count != 2) return false;
	c.listener(0, -100, 0); c.service();
	for (uint32 i = 0; i < 8; ++i) c.service();
	if (chirp->armed || chirp->count != 2 || chirp->playing) { warning("CRICKET TIMER FAIL out-of-range countdown"); return false; }
	c.listener(0, 0, 0); c.service();
	if (!chirp->armed || chirp->count != 3 || chirp->playing) { warning("CRICKET TIMER FAIL reentry rearm"); return false; }
	c.service(); c.service();
	if (chirp->count != 1 || chirp->playing) return false;
	c.service();
	if (!sound.handleActive(chirp->handle) || chirp->count != 3) { warning("CRICKET TIMER FAIL delayed fire"); return false; }
	const Audio::SoundHandle timedHandle = chirp->handle;
	c.service(); c.service();
	if (chirp->count != 1 || chirp->handle != timedHandle) { warning("CRICKET TIMER FAIL running sample countdown"); return false; }
	c.service();
	if (!sound.handleActive(chirp->handle) || chirp->handle == timedHandle || chirp->count != 3) { warning("CRICKET TIMER FAIL timer expiry during sample"); return false; }
	c.listener(0, 25, 0); c.service();
	if (((retail || demo) ? sound.handleActive(timedHandle) : mixer->getChannelVolume(timedHandle) != 192) || mixer->getChannelVolume(chirp->handle) != 192) { warning("CRICKET TIMER FAIL repeated descriptor movement/replacement"); return false; }
	c.setRoom("elsewhere"); c.service();
	if (chirp->armed || chirp->count != 2 || chirp->playing || sound.handleActive(timedHandle)) { warning("CRICKET TIMER FAIL off-room countdown or overlapping handle"); return false; }
	c.reset(); sound.haltSounds();
	warning("CRICKET TIMER PASS range arming off-room freeze reentry fresh delay timed repeat during sample overlapping movement and cleanup");
	if (retail) {
		c.setRoom("town"); c.listener(0, 0, 0);
		for (const char *name : { "chirp", "other", "third", "missing" }) sound.soundLoop(name, true);
		c.make("chirp", 0, 10, 100, 0, 0); c.make("other", 0, 20, 100, 0, 0); c.make("third", 0, 30, 100, 0, 0);
		const auto a = c.crickets[0], b = c.crickets[1], d = c.crickets[2];
		c.service();
		if (!sound.handleActive(a->handle) || !sound.handleActive(b->handle) || sound.handleActive(d->handle) || mixer->getChannelVolume(a->handle) != 230 || mixer->getChannelVolume(b->handle) != 204) { warning("CRICKET PRIORITY FAIL loudest two"); return false; }
		const Audio::SoundHandle kept = b->handle; c.service();
		if (b->handle != kept) { warning("CRICKET PRIORITY FAIL selected loop restart"); return false; }
		c.listener(0, 40, 0); c.make("missing", 0, 40, 100, 0, 0); c.service();
		if (sound.handleActive(a->handle) || !sound.handleActive(b->handle) || !sound.handleActive(d->handle) || b->handle != kept || mixer->getChannelVolume(d->handle) != 230) { warning("CRICKET PRIORITY FAIL movement or unavailable candidate"); return false; }
		c.listener(0, 20, 0); c.service();
		if (!sound.handleActive(a->handle) || !sound.handleActive(b->handle) || sound.handleActive(d->handle)) { warning("CRICKET PRIORITY FAIL equal-volume entry order"); return false; }
		if (!sound.playSound("third", SoundSystem::kSingle)) return false;
		c.service();
		if (!sound.handleActive(a->handle) || !sound.handleActive(b->handle) || sound.currentSound(1) == "third" || sound.currentSound(2) == "third") { warning("CRICKET PRIORITY FAIL deselected shared descriptor"); return false; }
		if (!sound.playSound("effect", SoundSystem::kSingle)) return false;
		c.service();
		if (sound.handleActive(a->handle) || !sound.handleActive(b->handle) || (sound.currentSound(1) != "effect" && sound.currentSound(2) != "effect")) { warning("CRICKET PRIORITY FAIL ordinary effect preemption"); return false; }
		sound.haltSounds(); c.service();
		if (!sound.handleActive(a->handle) || !sound.handleActive(b->handle)) { warning("CRICKET PRIORITY FAIL resume after effect"); return false; }
		sound.setWaveVolume(0); c.listener(0, 40, 0); c.service();
		if (sound.handleActive(a->handle) || !sound.handleActive(b->handle) || !sound.handleActive(d->handle) || mixer->getChannelVolume(d->handle) != 0) { warning("CRICKET PRIORITY FAIL pre-master ranking"); return false; }
		sound.setWaveVolume(9); c.reset(); sound.haltSounds();
		c.setRoom("town"); c.clearListener();
		c.make("chirp", 0, 0, 100, 0, 0); c.make("other", 0, 0, 100, 0, 0); c.make("third", 0, 0, 100, 0, 0);
		c.stop("other"); c.service();
		if (c.count() != 2 || c.index(1) != "chirp" || c.index(2) != "third" || !c.crickets[1]->name.empty()) { warning("CRICKET SLOT FAIL interior gap enumeration"); return false; }
		if (!session.stageSystem.open("blank.stg", nullptr, false)) { warning("CRICKET SLOT FAIL save stage"); return false; }
		Bytes saved; Common::String saveError;
		if (!session.saveState(saved, saveError)) { warning("CRICKET SLOT FAIL save %s", saveError.c_str()); return false; }
		c.make("other", 0, 0, 100, 0, 0); c.service();
		if (c.index(2) != "other" || sound.handleActive(c.crickets[2]->handle)) { warning("CRICKET SLOT FAIL first free tie promotion"); return false; }
		if (!session.loadState(saved, saveError)) { warning("CRICKET SLOT FAIL load %s", saveError.c_str()); return false; }
		if (c.count() != 2 || c.crickets.size() != 3 || !c.crickets[1]->name.empty() || c.index(2) != "third") { warning("CRICKET SLOT FAIL saved gap/order"); return false; }
		c.make("other", 0, 0, 100, 0, 0); c.service();
		if (c.index(2) != "other" || !sound.handleActive(c.crickets[0]->handle) || !sound.handleActive(c.crickets[1]->handle) || sound.handleActive(c.crickets[2]->handle)) { warning("CRICKET SLOT FAIL restored tie promotion"); return false; }
		c.reset(); sound.haltSounds();
		warning("CRICKET SLOT PASS first free allocation active enumeration interior gap v3 save restore equal-volume priority");
		warning("CRICKET PRIORITY PASS retail loudest two live promotion entry ties missing resource physical slot reuse shared descriptor stop effect preemption resume pre-master ranking");
	}
	if (demo && !auditEarlySound(session)) return false;
	if (!auditBankControls(session, mixer)) return false;
	if (!auditLiveControls(session, backend, test)) return false;
	c.reset(); if (!c.crickets.empty() || c.room != "none") return false;
	warning("CRICKET MIXER PASS positional moving listener integer volume folded pan live turn radius exit reentry master independent controls overlap pause room return repeat boundary close reset"); return true;
}
bool auditSound(const Common::FSNode &root, const Common::FSNode &fixture, OSystem *system) {
	Session session(system); Bytes bytes; ScriptInstance test; test.name = "sound-test"; Common::String error;
	if (!session.init(root) || !read(fixture, bytes) || !loadScript(bytes, test.script, error)) { warning("SOUND FAIL fixture init %s", error.c_str()); return false; }
	const ScriptResult result = session.runtime.run(test, "test");
	const bool ok = session.runtime.error.empty() && result.signal == ScriptResult::kReturn && result.value.number() == 12345;
	warning("SOUND %s version=%u value=%s error=%s", ok ? "PASS" : "FAIL", session.version, result.value.text().c_str(), session.runtime.error.c_str());
	return ok;
}
static Common::String testString(Reader &r, const Bytes &data) {
	const uint32 at = r.pos(), count = r.u8();
	const Common::String value = r.name(at, count + 1);
	r.seek(at + count + 1); return value;
}
bool auditResources(const Common::FSNode &root, const Common::FSNode &fixtures) {
	Resources resources; Bytes data;
	if (!resources.init(root) || !read(fixtures, data)) { warning("RESOURCE FAIL init or fixture read"); return false; }
	if (resources.setPath(0, "invalid:") || resources.setPath(9, "invalid:") || resources.setPath(1, "../escape")) { warning("RESOURCE FAIL path slot bounds"); return false; }
	Reader r(data); const uint32 count = r.u32();
	if (count > 10000) { warning("RESOURCE FAIL case count"); return false; }
	for (uint32 i = 0; i < count; ++i) {
		const Common::String name = testString(r, data), cd = testString(r, data);
		resources.mount(cd == "-" ? "" : cd);
		for (uint32 s = 1; s <= 8; ++s) {
			const Common::String path = testString(r, data);
			if (!resources.setPath(s, path == "-" ? "" : path)) { warning("RESOURCE FAIL fixture path"); return false; }
		}
		const Common::String query = testString(r, data);
		if (!r.good()) { warning("RESOURCE FAIL truncated fixture"); return false; }
		Common::FSNode node; Common::File file; AuditHash hash; int64 size = 0; Common::String path = "-";
		if (resources.resolve(query, node)) {
			if (!file.open(node)) { warning("RESOURCE FAIL file read"); return false; }
			size = file.size(); path = resources.relativeName(node);
			byte buffer[5000]; const uint32 bytes = file.read(buffer, sizeof(buffer));
			for (uint32 b = 0; b < bytes; ++b) hash.byteValue(buffer[b]);
		}
		warning("RESOURCE %s cd=%s path=%s size=%lld fnv1a=%08x", name.c_str(), resources.currentCD().empty() ? "-" : resources.currentCD().c_str(), path.c_str(), (long long)size, hash.hash);
	}
	if (r.pos() != data.size()) { warning("RESOURCE FAIL trailing fixture bytes"); return false; }
	warning("RESOURCE PASS cases=%u", count); return true;
}
static bool starReaderSelfTest() {
	for (uint32 version = 1; version <= 4; version += 3) {
		const bool v1 = version == 1; const uint32 first = v1 ? 28 : 8, stride = v1 ? 50 : 54;
		Bytes head, reg, route; head.resize(v1 ? 36 : 96); reg.resize(first + stride); route.resize((v1 ? 16 : 20) + 16);
		memset(head.data(), 0, head.size()); memset(reg.data(), 0, reg.size()); memset(route.data(), 0, route.size());
		WRITE_LE_UINT32(head.data() + 2, version);
		if (v1) WRITE_LE_UINT16(head.data() + 0x22, 1); else WRITE_LE_UINT32(head.data() + 0x58, 1);
		WRITE_LE_UINT32(reg.data() + (v1 ? 24 : 0), 1);
		const uint32 primary = first + (v1 ? 0 : 4), secondary = first + (v1 ? 26 : 30);
		WRITE_LE_UINT16(reg.data() + primary, 64); WRITE_LE_UINT16(reg.data() + primary + 2, uint16(-10)); reg[primary + 8] = 1; reg[primary + 9] = 'a';
		WRITE_LE_UINT16(reg.data() + secondary + 2, 20); reg[secondary + 8] = 1; reg[secondary + 9] = 'b';
		WRITE_LE_UINT16(reg.data() + first + (v1 ? 24 : 28), 2); WRITE_LE_UINT32(route.data() + (v1 ? 0 : 8), 2);
		WRITE_LE_UINT16(route.data() + (v1 ? 16 : 20), uint16(-10)); WRITE_LE_UINT16(route.data() + (v1 ? 24 : 28), 20); WRITE_LE_UINT16(route.data() + (v1 ? 30 : 34), 30);
		auto envelope = [&]() {
			const Bytes *parts[] = { &head, &reg, &route }; uint32 size = 1036;
			for (uint32 i = 0; i < 3; ++i) size += 8 + parts[i]->size();
			Bytes data; data.resize(size); memset(data.data(), 0, size); WRITE_LE_UINT32(data.data() + 4, size); WRITE_LE_UINT32(data.data() + 20, 3);
			uint32 at = 1036;
			for (uint32 i = 0; i < 3; ++i) { WRITE_LE_UINT32(data.data() + 1024 + i * 4, at); WRITE_LE_UINT32(data.data() + at + 4, parts[i]->size()); memcpy(data.data() + at + 8, parts[i]->data(), parts[i]->size()); at += 8 + parts[i]->size(); }
			return data;
		};
		auto load = [&]() { Archive archive; StarTable table; return archive.load(envelope()) && table.load(archive, version); };
		Archive archive; StarTable table;
		if (!archive.load(envelope()) || !table.load(archive, version) || table.stars.size() != 2 || table.stars[0].x != -10 || table.stars[0].facing != 64 || table.paths.size() != 1 || table.paths[0].points[1].x != 20 || table.paths[0].points[1].distance != 30) return false;
		WRITE_LE_UINT32(reg.data() + (v1 ? 24 : 0), 2); if (load()) return false; WRITE_LE_UINT32(reg.data() + (v1 ? 24 : 0), 1);
		WRITE_LE_UINT32(route.data() + (v1 ? 0 : 8), 3); if (load()) return false; WRITE_LE_UINT32(route.data() + (v1 ? 0 : 8), 2);
		WRITE_LE_UINT16(reg.data() + first + (v1 ? 24 : 28), 99); if (load()) return false; WRITE_LE_UINT16(reg.data() + first + (v1 ? 24 : 28), 2);
		reg[primary + 8] = 0; if (load()) return false; reg[primary + 8] = 1;
		reg[secondary + 8] = 255; if (!load()) return false; reg[secondary + 8] = 1;
		route.resize(route.size() - 1); if (load()) return false;
	}
	warning("STAR READER PASS primary secondary signed coordinates routes malformed counts refs names truncation"); return true;
}
bool auditStars(const Common::FSNode &root) {
	if (!starReaderSelfTest()) { warning("STAR READER FAIL"); return false; }
	Common::FSList files; uint32 count = 0, failures = 0;
	if (!corpusFiles(root, files)) { warning("STAR FAIL corpus enumeration"); return false; }
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root); if (!name.hasSuffixIgnoreCase(".set")) continue;
		Bytes data; Archive archive; StarTable table;
		if (!read(files[i], data) || !archive.load(data)) { warning("STAR FAIL %s envelope", name.c_str()); ++failures; continue; }
		Reader h(archive.get(0), 2); const uint32 version = h.u32();
		if (!h.good() || !table.load(archive, version)) { warning("STAR FAIL %s table", name.c_str()); ++failures; continue; }
		AuditHash hash; uint32 points = 0;
		for (uint32 n = 0; n < table.stars.size(); ++n) { const Star &s = table.stars[n]; hash.text(s.name); hash.number(s.x); hash.number(s.y); hash.number(s.z); hash.number(s.facing); }
		for (uint32 n = 0; n < table.paths.size(); ++n) {
			const StarPath &p = table.paths[n]; hash.text(p.a); hash.text(p.b); hash.number(p.container); hash.number(p.points.size()); points += p.points.size();
			for (uint32 j = 0; j < p.points.size(); ++j) { const StarPoint &s = p.points[j]; hash.number(s.x); hash.number(s.y); hash.number(s.z); hash.number(s.distance); }
		}
		warning("STAR %s stars=%u paths=%u points=%u meta=%08x", name.c_str(), table.stars.size(), table.paths.size(), points, hash.hash); ++count;
	}
	warning("STAR %s files=%u failures=%u", failures ? "FAIL" : "PASS", count, failures); return !failures;
}
bool auditRoomSession(const Common::FSNode &root, OSystem *backend) {
	Session session; Bytes bytes; ScriptInstance test; test.name = "room-test"; Common::String error;
	if (!session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("ROOM SESSION FAIL fixture init %s", error.c_str()); return false; }
	const char *invalid[] = {"BADMAIN.SET", "BADSCENE.SET", "BADCOUNT.SET", "BADNAME.SET", "BADPAINT.SET"};
	for (uint32 i = 0; i < (session.version == 1 ? 4u : 5u); ++i) { Bytes data; Room room; if (!read(root.getChild(invalid[i]), data) || room.load(data)) { warning("ROOM READER FAIL %s", invalid[i]); return false; } }
	Bytes roomBytes; Room cameraRoom;
	if (!read(root.getChild("FIRST.SET"), roomBytes) || !cameraRoom.load(roomBytes)) { warning("ROOM CAMERA FAIL fixture"); return false; }
	Frame image; CameraPose pose;
	if (!cameraRoom.stand(0, 0, image, &pose)) return false;
	if (session.version == 1) {
		if (pose.x != 128 || pose.y != 128 || pose.z != 160 || pose.degree != 192) { warning("ROOM CAMERA FAIL standpoint"); return false; }
		bool walk = false, turn = false;
		for (const auto &move : cameraRoom.moves) {
			if (move.frames.size() != 5) return false;
			if (move.from == 2 && move.to == 6) {
				walk = true; for (uint32 f = 0; f < 5; ++f) if (move.frames[f].camera.x != 128 + int32(f) * 64 || move.frames[f].camera.y != 128 || move.frames[f].camera.z != 160 || move.frames[f].camera.degree != 0) return false;
			}
			if (move.from == 0 && move.to == 2) {
				turn = true; for (uint32 f = 0; f < 5; ++f) if (move.frames[f].camera.x != 128 || move.frames[f].camera.y != 128 || move.frames[f].camera.degree != ((192 + int32(f) * 16) & 255)) return false;
			}
		}
		if (!walk || !turn) return false;
	} else if (pose.x != -10 || pose.y != 20 || pose.z != 30 || pose.degree != 0) { warning("ROOM CAMERA FAIL stored pose"); return false; }
	RoomSystem &rooms = session.roomSystem;
	rooms.current.reset(new LoadedRoom()); rooms.current->data = cameraRoom; rooms.scene = rooms.view = 0; rooms.camera = pose;
	WorldCamera camera;
	if (!rooms.worldCamera(camera, 7) || camera.z != pose.z + 7 || camera.focal != (session.version == 1 ? 310 : 4) || camera.width != 8 || camera.height != 4 || camera.x != (session.version == 1 ? 128 : -10) || camera.y != (session.version == 1 ? 192 : 20)) { warning("ROOM CAMERA FAIL setback focal bias"); return false; }
	rooms.reset();
	warning("ROOM CAMERA PASS stored signed coordinates standpoint height bias focal setback v1 five-frame walk turn endpoints");
	warning("ROOM READER PASS script refs counts names painting refs");
	const ScriptResult result = session.runtime.run(test, "test");
	const bool ok = session.runtime.error.empty() && result.signal == ScriptResult::kReturn && result.value.number() == 12345;
	warning("ROOM SESSION %s version=%u value=%s error=%s", ok ? "PASS" : "FAIL", session.version, result.value.text().c_str(), session.runtime.error.c_str());
	if (!ok) return false;
	{
		session.roomSystem.visible = false;
		if (!session.roomSystem.open("entry") || !session.roomSystem.visible || session.runtime.globals["openedvisible"].number() != 1) { warning("ROOM VISIBILITY FAIL hidden new set"); return false; }
		session.roomSystem.visible = false; session.roomSystem.close();
		if (!session.roomSystem.open("first") || !session.roomSystem.visible || session.runtime.globals["openedvisible"].number() != 1) { warning("ROOM VISIBILITY FAIL hidden closed set"); return false; }
		session.runtime.globals["hideopened"] = Value(1);
		if (!session.roomSystem.open("entry") || session.roomSystem.visible || session.runtime.globals["openedvisible"].number() != 1) { warning("ROOM VISIBILITY FAIL callback override"); return false; }
		session.runtime.globals["hideopened"] = Value(0);
		warning("ROOM VISIBILITY PASS version=%u hidden set reopen visible before callbacks explicit hide override", session.version);
	}
	if (!session.roomSystem.open("first") || session.crickets.room != "town" || session.stars.current.version != session.version) { warning("ROOM SESSION FAIL room star/ambience binding"); return false; }
	Values keys; keys.push_back(Value(Common::String("continue"))); session.runtime.globals["keys"] = Value(Common::String());
	session.runtime.run(*session.bootScripts[0], "keydown", keys);
	if (!session.runtime.error.empty() || session.runtime.globals["keys"].text() != "RAL" || session.runtime.eventConsumed) { warning("ROOM SESSION FAIL keyboard forwarding keys=%s error=%s", session.runtime.globals["keys"].text().c_str(), session.runtime.error.c_str()); return false; }
	keys[0] = Value(Common::String("stop")); session.runtime.globals["keys"] = Value(Common::String()); session.runtime.run(*session.bootScripts[0], "keydown", keys);
	if (!session.runtime.error.empty() || session.runtime.globals["keys"].text() != "RA" || !session.runtime.eventConsumed) { warning("ROOM SESSION FAIL keyboard consumption"); return false; }
	if (!session.stageSystem.open("library")) { warning("ROOM SESSION FAIL library stage init"); return false; }
	ScriptFrame library = session.runtime.eventFrame("caller", "test");
	const Value stageValue = session.roomSystem.send(false, "Scene A1", "stagecall", Values(), library);
	// A stage library has its own me, while target remains the addressed scene.
	// Retain that context and script storage even when it closes its own stage.
	const char *stageContext = session.version == 1 ? "library.flt/Scene A1" : "library.stg/Scene A1";
	if (!session.runtime.error.empty() || stageValue.text() != stageContext || session.stageSystem.current) { warning("ROOM SESSION FAIL stage library/context/storage %s %s", stageValue.text().c_str(), session.runtime.error.c_str()); return false; }
	if (session.version == 4) {
		ScriptFrame context = session.runtime.eventFrame("caller", "test"); Values args; args.push_back(Value(Common::String("arg")));
		const Value value = session.roomSystem.painting("Scene A1", "east", "door", "own", args, context);
		if (value.text() != "door/door/arg") { warning("ROOM SESSION FAIL painting context %s", value.text().c_str()); return false; }
		if (!session.roomSystem.jump("Scene A1", "north") || session.roomSystem.frame.pixels.empty() || session.roomSystem.frame.pixels[0] != 21) { warning("ROOM SESSION FAIL delta standpoint"); return false; }
		session.runtime.globals["log"] = Value(Common::String());
		if (!session.roomSystem.open("void") || !session.roomSystem.frame.pixels.empty() || !session.runtime.globals["log"].text().hasSuffix("SBAMb")) { warning("ROOM SESSION FAIL zero-image scene lifecycle"); return false; }
	}
	session.roomSystem.reset(); if (session.roomSystem.current || session.crickets.room != "none" || session.stars.current.version) { warning("ROOM SESSION FAIL reset binding"); return false; }
	warning("ROOM SESSION BINDING PASS stars ambience painting delta empty keyboard stage library reset");
	Session display(backend);
	if (!backend || !display.init(root) || !display.roomSystem.open("first")) { warning("ROOM DISPLAY FAIL init"); return false; }
	const Room &room = display.roomSystem.current->data;
	auto pictureMatches = [&](bool black) {
		const Frame &image = display.roomSystem.frame;
		Graphics::Surface *screen = backend->lockScreen(); if (!screen) return false;
		bool same = true;
		for (uint32 y = 0; y < 480 && same; ++y) for (uint32 x = 0; x < 640; ++x) {
			const uint32 xx = x * 512 / 640, yy = y * 384 / 480;
			byte color[3] = {};
			if (!black && xx < image.width && yy < image.height) memcpy(color, room.palette + image.pixels[yy * image.width + xx] * 3, 3);
			if (uint32(screen->getPixel(x, y)) != screen->format.RGBToColor(color[0], color[1], color[2])) { same = false; break; }
		}
		backend->unlockScreen(); return same;
	};
	if (!display.display.rgb() || !pictureMatches(false)) { warning("ROOM DISPLAY FAIL RGB placement/palette"); return false; }
	const uint32 before = backend->getMillis(); if (!display.roomSystem.move(0) || backend->getMillis() - before < 30 || display.roomSystem.viewName() == "moving") { warning("ROOM DISPLAY FAIL pacing or settle"); return false; }
	display.runtime.run(test, "holdblack");
	if (!display.runtime.error.empty() || !pictureMatches(true)) { warning("ROOM DISPLAY FAIL black hold %s", display.runtime.error.c_str()); return false; }
	display.runtime.run(test, "revealroom");
	if (!pictureMatches(false) || memcmp(display.display.palette(), room.palette, 768) || memcmp(display.displayPalette, room.palette, 768)) { warning("ROOM DISPLAY FAIL named reveal"); return false; }
	Common::Event quit; quit.type = Common::EVENT_RETURN_TO_LAUNCHER; backend->getEventManager()->pushEvent(quit);
	const ScriptResult abandoned = display.runtime.run(test, "quitmove");
	if (abandoned.signal != ScriptResult::kAbandoned || display.runtime.globals["aftermove"].truth() || display.roomSystem.moving) { warning("ROOM DISPLAY FAIL quit script abandonment signal=%d after=%s moving=%u", int(abandoned.signal), display.runtime.globals["aftermove"].text().c_str(), display.roomSystem.moving ? 1 : 0); return false; }
	warning("ROOM DISPLAY PASS RGB room placement palette black hold/reveal real-clock navigation settle quit abandonment"); return true;
}
bool auditRooms(const Common::FSNode &root) {
	// Endpoint views look back down the road; the arrival follows the film's
	// final camera angle. Exercise table destinations, legacy fallback, both
	// directions, wrap-around and stable ties without relying on game scripts.
	Room arrival; arrival.scenes.resize(2);
	arrival.scenes[0].viewTable = 100; arrival.scenes[1].viewTable = 200;
	for (uint32 s = 0; s < 2; ++s) for (uint32 v = 0; v < 4; ++v) {
		View view; view.id = s * 10 + v; view.facing = v * 64;
		arrival.scenes[s].views.push_back(view);
	}
	Road road; road.from = 0; road.to = 12; road.destinations[0] = 200; road.destinations[1] = 100;
	MotionFrame motion; motion.camera.degree = 0; road.walks[0].push_back(motion);
	motion.camera.degree = 128; road.walks[1].push_back(motion);
	int32 arrivalSceneIndex, arrivalViewIndex;
	if (!arrival.roadArrival(road, 0, arrivalSceneIndex, arrivalViewIndex) || arrivalSceneIndex != 1 || arrivalViewIndex != 0 ||
		!arrival.roadArrival(road, 1, arrivalSceneIndex, arrivalViewIndex) || arrivalSceneIndex != 0 || arrivalViewIndex != 2) return false;
	road.destinations[0] = 0; road.walks[0].back().camera.degree = 255;
	if (!arrival.roadArrival(road, 0, arrivalSceneIndex, arrivalViewIndex) || arrivalSceneIndex != 1 || arrivalViewIndex != 0) return false;
	road.destinations[0] = 100; road.walks[0].back().camera.degree = 32;
	if (!arrival.roadArrival(road, 0, arrivalSceneIndex, arrivalViewIndex) || arrivalSceneIndex != 0 || arrivalViewIndex != 0 ||
		arrival.roadArrival(road, 2, arrivalSceneIndex, arrivalViewIndex)) return false;
	warning("ROOM ARRIVAL PASS travel facing destination register reverse wraparound stable ties fallback");
	Common::FSList files; uint32 count = 0, failures = 0;
	if (!corpusFiles(root, files)) { warning("ROOM FAIL corpus enumeration"); return false; }
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root); if (!name.hasSuffixIgnoreCase(".set")) continue;
		Bytes bytes; Room room; if (!read(files[i], bytes) || !room.load(bytes)) { warning("ROOM FAIL %s metadata", name.c_str()); ++failures; continue; }
		if (room.version == 4 && room.name.equalsIgnoreCase("gstair2")) {
			bool corrected = false;
			for (const auto &corpusRoad : room.roads) for (uint32 dir = 0; dir < 2; ++dir) {
				const Sequence &raw = corpusRoad.walks[dir]; if (raw.empty()) continue;
				Sequence playback; int32 arrivalScene, arrivalView;
				if (!room.roadSequence(corpusRoad, dir, playback, arrivalScene, arrivalView) || playback.size() != raw.size()) return false;
				for (uint32 f = 0; f < raw.size(); ++f) {
					if (playback[f].image != raw[f].image || playback[f].camera.x != raw[f].camera.x || playback[f].camera.y != raw[f].camera.y || playback[f].camera.degree != raw[f].camera.degree) return false;
					if (f > 0 && f + 1 < raw.size() && playback[f].camera.z != raw[f].camera.z) return false;
				}
				Frame still; CameraPose arrivalCamera;
				if (!room.stand(arrivalScene, arrivalView, still, &arrivalCamera) || playback.back().camera.z != arrivalCamera.z) return false;
				if (corpusRoad.from == 23 && corpusRoad.to == 26 && dir == 0) {
					// Road58 on A deck: the first row incorrectly retained the
					// upper landing's height, shifting a crowd NPC into view.
					if (raw.front().camera.z != 7645 || playback.front().camera.z != 5976 || playback.back().camera.z != 5976) return false;
					corrected = true;
				}
			}
			if (!corrected) return false;
			warning("ROOM ROAD HEIGHT PASS %s Road58 7645->5976 both directions arrival heights interior ramps unchanged source immutable", name.c_str());
		}
		AuditHash camera; camera.number(room.width); camera.number(room.height); camera.number(room.depthLevels); camera.number(room.depthFar); camera.number(room.cameraSetback); camera.number(room.eyeHeight);
		uint32 cameraFrames = 0, imageFrames = 0, depthFrames = 0, depthHash = 2166136261u;
		auto depthSequence = [&](const Sequence &seq) {
			Frame frame;
			for (uint32 f = 0; f < seq.size(); ++f) {
				if (!seq[f].image) continue;
				if (!decodeFrame(room.archive.get(seq[f].image), frame)) { warning("ROOM FAIL %s image=%u", name.c_str(), seq[f].image); ++failures; return; }
				++imageFrames;
				if (!frame.depth.empty()) {
					++depthFrames;
					for (uint32 p = 0; p < frame.depth.size(); ++p) depthHash = (depthHash ^ frame.depth[p]) * 16777619u;
				}
			}
		};
		auto cameraSequence = [&](const Sequence &seq) {
			for (uint32 f = 0; f < seq.size(); ++f) {
				const CameraPose &pose = seq[f].camera;
				camera.number(pose.x); camera.number(pose.y); camera.number(pose.z); camera.number(pose.degree); ++cameraFrames;
			}
		};
		AuditHash hash; uint32 paintings = 0; hash.text(room.name); hash.number(room.mainScript);
		Common::Array<uint32> scripts; scripts.push_back(room.mainScript);
		for (uint32 s = 0; s < room.scenes.size(); ++s) {
			const Scene &scene = room.scenes[s]; hash.text(scene.name); hash.number(scene.script); hash.number(scene.x); hash.number(scene.y); hash.number(scene.z); hash.number(scene.build ? 1 : 0); scripts.push_back(scene.script);
			for (uint32 v = 0; v < scene.views.size(); ++v) { Frame frame; if (!room.stand(s, v, frame)) { warning("ROOM FAIL %s standpoint=%u/%u", name.c_str(), s, v); ++failures; } }
			if (room.version == 4) for (uint32 v = 0; v < scene.views.size(); ++v) {
				const View &view = scene.views[v]; camera.number(int32(floor(view.cameraHeight * 512 + 0.5))); hash.text(view.name); hash.number(view.id); hash.number(view.facing);
				for (uint32 p = 0; p < view.paintings.size(); ++p) {
					const Painting &paint = view.paintings[p]; ++paintings; hash.text(paint.name); hash.number(paint.script); hash.number(paint.facing); hash.number(paint.rect.left); hash.number(paint.rect.top); hash.number(paint.rect.right); hash.number(paint.rect.bottom); scripts.push_back(paint.script);
				}
			}
		}
		if (room.version == 4) {
			for (uint32 s = 0; s < room.scenes.size(); ++s) for (uint32 d = 0; d < 2; ++d) cameraSequence(room.scenes[s].turns[d]);
			for (uint32 r = 0; r < room.roads.size(); ++r) for (uint32 d = 0; d < 2; ++d) cameraSequence(room.roads[r].walks[d]);
		}
		for (uint32 s = 0; s < room.scenes.size(); ++s) for (uint32 d = 0; d < 2; ++d) depthSequence(room.scenes[s].turns[d]);
		for (uint32 r = 0; r < room.roads.size(); ++r) for (uint32 d = 0; d < 2; ++d) depthSequence(room.roads[r].walks[d]);
		for (uint32 m = 0; m < room.moves.size(); ++m) depthSequence(room.moves[m].frames);
		for (uint32 s = 0; s < room.scenes.size(); ++s) for (uint32 v = 0; v < room.scenes[s].views.size(); ++v) {
			const uint32 still = room.scenes[s].views[v].still;
			if (still) { MotionFrame f; f.image = still; Sequence seq; seq.push_back(f); depthSequence(seq); }
		}
		warning("ROOMDEPTH %s images=%u planes=%u fnv1a=%08x", name.c_str(), imageFrames, depthFrames, depthHash);
		warning("ROOMCAM %s frames=%u meta=%08x", name.c_str(), cameraFrames, camera.hash);
		for (uint32 p = 0; p < scripts.size(); ++p) if (scripts[p]) {
			Script script; Common::String error; if (!loadScript(room.archive.get(scripts[p]), script, error)) { warning("ROOM FAIL %s script=%u %s", name.c_str(), scripts[p], error.c_str()); ++failures; }
		}
		warning("ROOM %s scenes=%u paintings=%u meta=%08x", name.c_str(), room.scenes.size(), paintings, hash.hash); ++count;
	}
	warning("ROOM %s files=%u failures=%u", failures ? "FAIL" : "PASS", count, failures); return !failures;
}
static bool auditRouteStore() {
	RouteTestFiles files; RouteStore store; Common::String error;
	if (store.open(&files, "../unsafe", "fixture", error) || store.active() || !store.open(&files, "test", "fixture", error)) return false;
	JournalEntry entry; entry.text = "Scene";
	const uint32 scene = store.graph.recordScene(entry);
	if (!store.save(error)) return false;
	entry.choices.push_back("Reply"); entry.selected = 0;
	const uint32 decision = store.graph.recordDecision(entry);
	if (!store.save(error)) return false;
	RouteStore loaded;
	if (!loaded.open(&files, "test", "fixture", error) || loaded.graph.cursor() != decision) return false;
	// A torn newest slot must retain the previous committed discovery.
	const Common::String latest = files.listSavefiles("*.b")[0]; Bytes intact = files.files[latest]; files.files[latest].resize(1);
	if (!loaded.open(&files, "test", "fixture", error) || loaded.graph.cursor() != scene) return false;
	files.files[latest] = intact;
	RouteStore foreign;
	if (!foreign.open(&files, "test", "another-fixture", error) || !foreign.graph.nodes().empty()) return false;
	// Storage checks the native envelope; Session::loadState validates its contents.
	Bytes checkpoint; checkpoint.resize(32); Common::MemoryWriteStream encoded(checkpoint.data(), checkpoint.size());
	encoded.writeUint32BE(MKTAG('D','F','S','V')); encoded.writeUint32LE(10); encoded.writeUint32LE(4);
	memset(checkpoint.data() + 28, 0, 4);
	Common::MemoryReadStream payload(checkpoint.data() + 28, 4); byte digest[16]; Common::computeStreamMD5(payload, digest); encoded.write(digest, 16);
	store.setBudget(31);
	if (store.writeCheckpoint(scene, checkpoint, error) || store.graph.node(scene)->checkpoint) return false;
	store.setBudget(1024);
	files.failSuffix = ".a";
	if (store.writeCheckpoint(scene, checkpoint, error) || store.graph.node(scene)->checkpoint || !files.listSavefiles("*.checkpoint").empty()) return false;
	if (!loaded.open(&files, "test", "fixture", error) || loaded.graph.cursor() != decision) return false;
	files.failSuffix.clear();
	if (!store.writeCheckpoint(scene, checkpoint, error) || store.writeCheckpoint(scene, checkpoint, error)) return false;
	Bytes replay;
	if (!loaded.open(&files, "test", "fixture", error) || !loaded.readCheckpoint(decision, replay, error) || replay != checkpoint) return false;
	const Common::String snapshot = files.listSavefiles("*.checkpoint")[0];
	files.files[snapshot].back() ^= 1;
	if (loaded.readCheckpoint(decision, replay, error) || !replay.empty()) return false;
	files.removeSavefile(snapshot);
	if (loaded.readCheckpoint(decision, replay, error) || !replay.empty()) return false;
	for (const auto &name : files.listSavefiles("*.?")) files.files[name].resize(1);
	if (loaded.open(&files, "test", "fixture", error) || loaded.active()) return false;
	warning("ROUTE STORE PASS restart identity isolation torn metadata recovery immutable snapshots failed commit rollback quota missing corrupt checkpoint rejection");
	return true;
}
static bool auditRouteGraph() {
	RouteGraph graph;
	JournalEntry scene; scene.speaker = "Speaker"; scene.room = "Room"; scene.file = "disc1/speech.pup"; scene.text = "A scene";
	JournalEntry decision = scene; decision.choices.push_back("First reply"); decision.choices.push_back("Second reply"); decision.selected = 0; decision.text = decision.choices[0];
	if (graph.recordDecision(decision)) return false;
	const uint32 start = graph.recordScene(scene), first = graph.recordDecision(decision), continuation = graph.recordScene(scene);
	decision.selected = 1; decision.text = decision.choices[1];
	const uint32 second = graph.recordDecision(decision), ending = graph.recordEnding(scene);
	if (!start || first != 2 || continuation != 3 || second != 4 || ending != 5 || graph.sceneFor(second) != continuation || !graph.attachCheckpoint(start, 1024)) return false;
	if (graph.attachCheckpoint(first, 1024) || graph.attachCheckpoint(start, RouteGraph::kMaxCheckpointBytes + 1) || graph.restoreCursor(start, 0) || graph.restoreCursor(first, 1)) return false;
	if (graph.checkpointFor(second) != start || graph.sceneFor(second) != continuation || graph.checkpointFor(0)) return false;
	if (!graph.restoreCursor(start, -1) || graph.recordDecision(decision) != first || graph.node(first)->seenChoices != 3) return false;
	const uint32 alternate = graph.recordScene(scene);
	if (alternate != 6 || graph.node(alternate)->parent != first || graph.node(alternate)->parentChoice != 1 || !graph.valid()) return false;
	if (!graph.restoreCursor(first, 0) || graph.recordScene(scene, true) != continuation || !graph.restoreCursor(second, 1) || graph.recordEnding(scene) != ending || !graph.restoreCursor(alternate, -1) || graph.nodes().size() != 6) return false;
	Bytes encoded, roundtrip; Common::String error;
	RouteGraph restored;
	if (!RouteCodec::encode(graph, "fixture-identity", encoded, error) || !RouteCodec::decode(encoded, "fixture-identity", restored, error) ||
		!RouteCodec::encode(restored, "fixture-identity", roundtrip, error) || encoded != roundtrip || restored.cursor() != alternate || restored.node(first)->seenChoices != 3 || restored.node(start)->checkpoint != start) return false;
	if (RouteCodec::decode(encoded, "another-game", restored, error) || error.empty()) return false;
	for (uint32 size : {0u, 8u, 27u, uint32(encoded.size() - 1)}) {
		Bytes truncated = encoded; truncated.resize(size);
		if (RouteCodec::decode(truncated, "fixture-identity", restored, error) || restored.nodes().size() != graph.nodes().size() || restored.cursor() != alternate) return false;
	}
	Bytes corrupt = encoded; corrupt.back() ^= 1;
	if (RouteCodec::decode(corrupt, "fixture-identity", restored, error) || restored.cursor() != alternate) return false;
	// Recompute the digest to exercise semantic validation rather than only checksum rejection.
	corrupt = encoded;
	const uint32 firstNode = 28 + 4 + uint32(Common::String("fixture-identity").size()) + 12;
	WRITE_LE_UINT32(corrupt.data() + firstNode + 4, 1); // Root cannot parent itself.
	Common::MemoryReadStream payload(corrupt.data() + 28, corrupt.size() - 28); byte digest[16]; Common::computeStreamMD5(payload, digest); memcpy(corrupt.data() + 12, digest, 16);
	if (RouteCodec::decode(corrupt, "fixture-identity", restored, error) || restored.cursor() != alternate) return false;
	RouteGraph bounded;
	for (uint32 i = 0; i < RouteGraph::kMaxNodes; ++i) if (!bounded.recordScene(scene)) return false;
	if (bounded.recordScene(scene) || bounded.nodes().size() != RouteGraph::kMaxNodes || !bounded.valid()) return false;
	RouteGraph layoutGraph; RouteMap map;
	const uint32 layoutScene = layoutGraph.recordScene(scene);
	decision.selected = 1;
	const uint32 layoutDecision = layoutGraph.recordDecision(decision), layoutNext = layoutGraph.recordScene(scene);
	map.build(layoutGraph);
	if (map.items().size() != 4 || map.itemFor(layoutScene) != 0 || map.itemFor(layoutDecision) != 1 || map.itemFor(layoutNext) != 2 || !map.items()[3].locked || map.items()[3].choice != 0) return false;
	if (map.items()[3].column >= map.items()[2].column || map.navigate(1, 0, 1) != 3 || map.navigate(2, -1, 0) != 3 || map.navigate(3, 1, 0) != 2 || map.navigate(3, 0, -1) != 1 || map.navigate(0, 0, -1) != 0) return false;
	map.build(bounded);
	if (map.items().size() != RouteGraph::kMaxNodes || map.items().back().row != int32(RouteGraph::kMaxNodes) - 1 || map.items().back().column) return false;
	RouteGraph wide;
	for (uint32 i = 0; i < RouteGraph::kMaxNodes; ++i) { wide.restoreCursor(0, -1); wide.recordScene(scene); }
	map.build(wide);
	if (map.items().back().column != int32(RouteGraph::kMaxNodes - 1) * 2 || map.navigate(RouteGraph::kMaxNodes - 2, 1, 0) != int32(RouteGraph::kMaxNodes) - 1) return false;
	warning("ROUTE MAP PASS authored reply ordering undiscovered placeholders parent child sibling navigation long route coordinates bounded layout");
	RouteEndingContext outcome; outcome.version = 4; outcome.retail = true; outcome.stage = "disc1/narend/NAREND.STG"; outcome.clock = "endgame";
	const char *movies[] = {"prozac.mov", "rushend.mov", "germend.mov", "germend.mov", "rushend.mov", "rushend.mov", "nuke.mov", "boom.mov"};
	JournalEntry identified; identified.text = "Retained";
	for (uint32 history = 0; history < 8; ++history) {
		outcome.firstWar = history & 1; outcome.secondWar = history & 2; outcome.revolution = history & 4; outcome.movie = Common::String("disc1/endings/") + movies[history];
		if (!identifyRouteEnding(outcome, identified) || identified.speaker.empty() || !identified.ident.empty() || identified.text == "Retained") return false;
		outcome.movie = "credits.mov";
		if (identifyRouteEnding(outcome, identified)) return false;
	}
	outcome.movie = "boom.mov"; outcome.stage = "bedsit.stg";
	if (identifyRouteEnding(outcome, identified)) return false;
	outcome = RouteEndingContext(); outcome.version = 1; outcome.day = 5; outcome.stage = "installed:local:NEW.FLT";
	const char *dustMovies[] = {"trottend.mov", "mayorend.mov", "marieend.mov", "yunniend.mov", "deserend.mov"};
	for (uint32 i = 0; i < 5; ++i) { outcome.phase = i + 2; outcome.movie = dustMovies[i]; if (!identifyRouteEnding(outcome, identified)) return false; outcome.movie = "intro.mov"; if (identifyRouteEnding(outcome, identified)) return false; }
	outcome.day = 4; outcome.movie = "deserend.mov"; if (identifyRouteEnding(outcome, identified)) return false;
	outcome = RouteEndingContext(); outcome.version = 4; outcome.phase = 2; outcome.movie = "postdemo.mov";
	if (!identifyRouteEnding(outcome, identified)) return false;
	outcome.movie = "close.mov"; if (identifyRouteEnding(outcome, identified)) return false;
	outcome.movie = "postdemo.mov"; outcome.phase = 1; if (identifyRouteEnding(outcome, identified)) return false;
	warning("ROUTE ENDINGS PASS eight authored Titanic histories five Dust finales demo conclusion unrelated movies credits and quit excluded inherited checkpoints stable replay scenes endings");
	warning("ROUTE GRAPH PASS branch discovery stable revisit ids separate playthrough cursor checkpoints metadata bounds transactional codec different-game corruption rejection");
	return true;
}
bool auditStageSession(const Common::FSNode &root, OSystem *system) {
	if (!auditRouteGraph() || !auditRouteStore()) return false;
	Session session; Bytes bytes; ScriptInstance test; test.name = "stage-test"; Common::String error;
	if (!session.init(root) || !read(root.getChild("test.dfscript"), bytes) || !loadScript(bytes, test.script, error)) { warning("STAGE SESSION FAIL fixture init %s", error.c_str()); return false; }
	const ScriptResult result = session.runtime.run(test, "test");
	const bool ok = session.runtime.error.empty() && result.signal == ScriptResult::kReturn && result.value.number() == 12345;
	warning("STAGE SESSION %s version=%u value=%s error=%s", ok ? "PASS" : "FAIL", session.version, result.value.text().c_str(), session.runtime.error.c_str());
	if (!ok) return false;
	if (!session.stageSystem.open("codex") || session.stageSystem.current->data.hitTest(0, 40, 30) != 1 || session.stageSystem.current->data.hitTest(0, 41, 30) != -1) { warning("STAGE SESSION FAIL inclusive overlapping regions"); return false; }
	if (session.canAutosave()) { warning("AUTOSAVE GUARD FAIL title/document stage accepted"); return false; }
	session.stageSystem.reset();
	if (session.stageSystem.current || session.stageSystem.currentFlat() != "none") { warning("STAGE SESSION FAIL reset"); return false; }
	warning("STAGE SESSION REGIONS PASS inclusive overlap reset");
	Session gfx(system); if (!gfx.init(root)) return false;
	Common::SharedPtr<LoadedRoom> room(new LoadedRoom());
	room->data.width = 512; room->data.height = 264;
	for (uint32 i = 0; i < 768; ++i) room->data.palette[i] = byte(i * 7 + 13);
	gfx.roomSystem.current = room; gfx.roomSystem.frame.width = 512; gfx.roomSystem.frame.height = 264;
	gfx.roomSystem.frame.pixels.resize(512 * 264);
	for (uint32 y = 0; y < 264; ++y) for (uint32 x = 0; x < 512; ++x) gfx.roomSystem.frame.pixels[y * 512 + x] = byte(x + y * 3);
	auto run = [&](const char *handler) { gfx.runtime.run(test, handler); if (!gfx.runtime.error.empty()) warning("STAGE DISPLAY FAIL %s: %s", handler, gfx.runtime.error.c_str()); return gfx.runtime.error.empty(); };
	auto pixels = [&](uint32 variant, bool roomVisible, uint32 stageDim, uint32 roomDim, bool black, bool matte) {
		Graphics::Surface *screen = system->lockScreen(); if (!screen) return false;
		bool same = true;
		for (uint32 y = 0; y < 480 && same; ++y) for (uint32 x = 0; x < 640; ++x) {
			const uint32 xx = x * 512 / 640, yy = y * 384 / 480;
			byte color[3] = {};
			if (!black) {
				const bool view = roomVisible && yy < 264;
				const uint32 index = view ? byte(xx + yy * 3) : matte && yy < 264 ? 253 : 2 + ((xx * 3 + yy * 5 + variant * 31) % 250);
				for (uint32 c = 0; c < 3; ++c) color[c] = byte((view ? room->data.palette[index * 3 + c] : (index * (c + 2) + c * 19) % 256) * (255 - (view ? roomDim : stageDim)) / 255);
			}
			if (uint32(screen->getPixel(x, y)) != screen->format.RGBToColor(color[0], color[1], color[2])) { same = false; break; }
		}
		system->unlockScreen(); return same;
	};
	if (!run("display") || !gfx.display.rgb() || !pixels(0, true, 0, 0, false, false) ||
		!run("second") || !pixels(1, true, 0, 0, false, false) ||
		!run("hide") || !pixels(1, false, 0, 0, false, false) ||
		!run("dimstage") || !pixels(1, false, 128, 0, false, false) ||
		!run("show") || !pixels(1, true, 128, 0, false, false) ||
		!run("dimroom") || !pixels(1, true, 128, 192, false, false) ||
		!run("resetstage") || !pixels(1, true, 0, 192, false, false) ||
		!run("resetroom") || !pixels(1, true, 0, 0, false, false) ||
		!run("dark") || !pixels(1, true, 0, 0, true, false) ||
		!run("reveal") || !pixels(1, true, 0, 0, false, false) ||
		!run("black") || !pixels(1, true, 0, 0, true, false) ||
		!run("refresh") || !pixels(1, true, 0, 0, true, false) ||
		!run("reveal") || !run("matte") || !pixels(0, true, 0, 0, false, true)) { warning("STAGE DISPLAY FAIL layering visibility palette fade"); return false; }
	const Bytes beforeText = gfx.display.picture();
	if (!run("textprobe") || gfx.textOverlay.size() != 2 || gfx.runtime.globals["measured"].number() <= 0) { warning("STAGE TEXT FAIL draw/width"); return false; }
	const Bytes textPicture = gfx.display.picture(); uint32 textPixels = 0;
	for (uint32 y = 0; y < 480; ++y) for (uint32 x = 0; x < 640; ++x) {
		const uint32 p = (y * 640 + x) * 4;
		if (memcmp(beforeText.data() + p, textPicture.data() + p, 4)) {
			const uint32 xx = x * 512 / 640, yy = y * 384 / 480;
			if (xx < 10 || xx >= 300 || yy < 300 || yy >= 325) { warning("STAGE TEXT FAIL clipping/baseline"); return false; }
			++textPixels;
		}
	}
	if (!textPixels || !run("refresh") || gfx.display.picture() != textPicture || !run("textreplace") || gfx.textOverlay.size() != 2 || gfx.textOverlay[0].text != "$9") { warning("STAGE TEXT FAIL persistence or replacement"); return false; }
	Common::SharedPtr<LoadedShop> textShop(new LoadedShop()); PropGroup textGroup; PropState textState; PropPicture textRef; textRef.location = 1; textState.pictures.push_back(textRef); textGroup.states.push_back(textState); textShop->data.groups.push_back(textGroup);
	Sprite patch; patch.width = 100; patch.height = 30; patch.pixels.resize(3000); patch.opaque.resize(3000); memset(patch.pixels.data(), 250, 3000); memset(patch.opaque.data(), 1, 3000); textShop->frameCache[1] = patch;
	Common::SharedPtr<Prop> textClear(new Prop()); textClear->name = "textclear"; textClear->shop = textShop; textClear->anchorX = 5; textClear->anchorY = 295; gfx.propSystem.props.push_back(textClear);
	if (!run("texterase") || gfx.textOverlay.size() != 1 || gfx.textOverlay[0].text != "Other") { warning("STAGE TEXT FAIL prop erasure touched another field"); return false; }
	gfx.propSystem.remove("textclear");
	if (!run("second") || !gfx.textOverlay.empty() || !run("display") || !run("matte") || !pixels(0, true, 0, 0, false, true)) { warning("STAGE TEXT FAIL flat change clear"); return false; }
	warning("STAGE TEXT PASS native drawstring width baseline bounds persistent replacement prop erasure flat reset");
	Common::SharedPtr<LoadedShop> ui(new LoadedShop()); ui->data.version = gfx.version;
	PropGroup group; PropState state; state.name = "idle"; PropPicture ref; ref.location = 1; state.pictures.push_back(ref); group.states.push_back(state); ui->data.groups.push_back(group);
	Sprite sprite; sprite.width = sprite.height = 2; sprite.pixels.resize(4); sprite.opaque.resize(4);
	sprite.pixels[0] = 7; sprite.pixels[1] = 0; sprite.pixels[2] = 200; sprite.pixels[3] = 201;
	memset(sprite.opaque.data(), 1, 4); sprite.opaque[1] = 0; ui->frameCache[1] = sprite;
	Common::SharedPtr<Prop> overlay(new Prop()); overlay->shop = ui; overlay->visible = true; overlay->anchorX = overlay->anchorY = 0; gfx.propSystem.props.push_back(overlay);
	auto pixel = [&](uint32 x, uint32 y, const byte *color, uint32 factor = 255) {
		Graphics::Surface *screen = system->lockScreen(); if (!screen) return false;
		const uint32 expected = screen->format.RGBToColor(byte(color[0] * factor / 255), byte(color[1] * factor / 255), byte(color[2] * factor / 255));
		const bool same = uint32(screen->getPixel(x, y)) == expected; system->unlockScreen(); return same;
	};
	const byte *stageInk = gfx.stageSystem.current->data.palette + 200 * 3, *roomInk = room->data.palette + 7 * 3;
	if (!run("refresh") || !pixel(0, 0, roomInk) || !pixel(2, 0, room->data.palette + 3) || !pixel(0, 2, stageInk) ||
		!run("dimstage") || !pixel(0, 0, roomInk) || !pixel(0, 2, stageInk, 127) ||
		!run("dimroom") || !pixel(0, 0, roomInk, 63) || !run("resetstage") || !run("resetroom")) { warning("STAGE DISPLAY FAIL prop shared palettes and transparency"); return false; }
	overlay->worldSpace = true; overlay->x = 96; overlay->scale = 1000;
	gfx.roomSystem.scene = gfx.roomSystem.view = 0; gfx.roomSystem.camera = CameraPose();
	room->data.depthFar = 512; room->data.depthLevels = 16;
	gfx.roomSystem.frame.depth.resize(512 * 264); memset(gfx.roomSystem.frame.depth.data(), 255, 512 * 264);
	if (!run("refresh") || !pixel(320, 165, roomInk) || !pixel(322, 165, room->data.palette + 141 * 3) || !pixel(320, 167, stageInk)) { warning("STAGE WORLD DISPLAY FAIL projected shared palettes"); return false; }
	memset(gfx.roomSystem.frame.depth.data(), 0, 512 * 264);
	if (!run("refresh") || !pixel(320, 165, room->data.palette + 140 * 3)) { warning("STAGE WORLD DISPLAY FAIL scenery"); return false; }
	overlay->zclip = 96;
	if (!run("refresh") || !pixel(320, 165, roomInk)) { warning("STAGE WORLD DISPLAY FAIL zclip"); return false; }
	warning("STAGE WORLD DISPLAY PASS projected sprites shared palettes transparency scenery depth zclip");
	Common::SharedPtr<LoadedCast> cast(new LoadedCast()); CastMember member; CastPose pose; pose.name = "stand";
	Common::Array<CastFrame> angle; CastFrame picture; picture.location = 1; picture.refScale = 96; angle.push_back(picture); pose.steps.push_back(angle); pose.play.push_back(0); member.poses.push_back(pose); cast->data.members.push_back(member);
	Sprite actorSprite = sprite; memset(actorSprite.pixels.data(), 5, actorSprite.pixels.size()); memset(actorSprite.opaque.data(), 1, actorSprite.opaque.size()); cast->frameCache[1] = actorSprite;
	Common::SharedPtr<Actor> actor(new Actor()); actor->cast = cast; actor->visible = true; actor->x = 48; actor->scale = 500; gfx.castSystem.actors.push_back(actor);
	room->data.version = gfx.version; memset(gfx.roomSystem.frame.depth.data(), 255, 512 * 264);
	if (!run("refresh") || !pixel(320, 165, room->data.palette + (gfx.version == 1 ? 5 : 7) * 3)) { warning("STAGE ACTOR DISPLAY FAIL mixed world order"); return false; }
	actor->x = 192; actor->scale = 2000;
	if (!run("refresh") || !pixel(320, 165, roomInk)) { warning("STAGE ACTOR DISPLAY FAIL behind prop"); return false; }
	actor->x = 96; actor->scale = 1000;
	if (!run("refresh") || !pixel(320, 165, roomInk)) { warning("STAGE ACTOR DISPLAY FAIL equal depth"); return false; }
	actor->worldSpace = false; actor->scale = 0; actor->anchorX = 256; actor->anchorY = 132;
	if (!run("refresh") || !pixel(320, 165, room->data.palette + 5 * 3)) { warning("STAGE ACTOR DISPLAY FAIL screen actor"); return false; }
	// Exercise the same compositor with an open stage that has no selected flat,
	// and then with no stage at all. Both use the complete room palette.
	const auto savedStage = gfx.stageSystem.current; const int32 savedFlat = gfx.stageSystem.flat;
	gfx.stageSystem.flat = -1;
	for (uint32 bare = 0; bare < 2; ++bare) {
		if (bare) gfx.stageSystem.current.reset();
		actor->worldSpace = true; actor->x = 48; actor->scale = 500;
		if (!run("refresh") || !pixel(320, 165, room->data.palette + (gfx.version == 1 ? 5 : 7) * 3) || !pixel(320, 167, room->data.palette + (gfx.version == 1 ? 5 : 200) * 3)) { warning("ROOM SPRITE DISPLAY FAIL mixed depth/full palette"); return false; }
		actor->worldSpace = false; actor->scale = 0;
		if (!run("refresh") || !pixel(320, 165, room->data.palette + 5 * 3)) { warning("ROOM SPRITE DISPLAY FAIL screen actor"); return false; }
		actor->visible = false;
		if (!run("dimroom") || !pixel(320, 165, roomInk, 63) || !run("resetroom") || !pixel(320, 165, roomInk)) { warning("ROOM SPRITE DISPLAY FAIL palette dim/reveal"); return false; }
		if (!run("black") || !pixel(320, 165, roomInk, 0) || !run("resetroom") || !pixel(320, 165, roomInk)) { warning("ROOM SPRITE DISPLAY FAIL black/reveal"); return false; }
		if (!run("hide") || !pixel(320, 165, roomInk, 0) || !run("show") || !pixel(320, 165, roomInk)) { warning("ROOM SPRITE DISPLAY FAIL visibility"); return false; }
		actor->visible = true;
	}
	gfx.stageSystem.current = savedStage; gfx.stageSystem.flat = savedFlat;
	warning("ROOM SPRITE DISPLAY PASS absent/empty stage world depth screen actors full palette dim black reveal visibility");
	actor->visible = false;
	warning("STAGE ACTOR DISPLAY PASS v1 merged world depth v4 two-pass equal depth scale-zero screen actors");
	Scene hitScene; hitScene.name = "scene"; View hitView;
	Painting large; large.name = "large"; large.rect = Common::Rect(10, 10, 50, 50); hitView.paintings.push_back(large);
	Painting small; small.name = "small"; small.rect = Common::Rect(20, 20, 30, 30); hitView.paintings.push_back(small);
	Painting tied = small; tied.name = "tied"; hitView.paintings.push_back(tied);
	hitScene.views.push_back(hitView); room->data.scenes.push_back(hitScene);
	if (gfx.hitTest(25, 25).name != "small" || gfx.hitTest(50, 50).name != "large" || gfx.hitTest(100, 100).type != "scene" || gfx.hitTest(10, 300).type != "flat") { warning("INPUT HIT FAIL painting priority/bounds room/band zones"); return false; }
	actor->name = "actor"; overlay->name = "prop"; actor->visible = true; actor->worldSpace = true; actor->x = 48; actor->scale = 500;
	if (gfx.hitTest(256, 132).type != (gfx.version == 1 ? "actor" : "prop")) { warning("INPUT HIT FAIL mixed sprite order"); return false; }
	actor->x = 96; actor->scale = 1000;
	if (gfx.hitTest(256, 132).type != "prop") { warning("INPUT HIT FAIL depth tie"); return false; }
	memset(gfx.roomSystem.frame.depth.data(), 0, gfx.roomSystem.frame.depth.size()); overlay->zclip = 0;
	if (gfx.hitTest(256, 132).type != "scene") { warning("INPUT HIT FAIL scenery"); return false; }
	gfx.roomSystem.visible = false; if (gfx.hitTest(25, 25).type != "flat") { warning("INPUT HIT FAIL hidden room"); return false; }
	gfx.roomSystem.visible = true; actor->visible = false; memset(gfx.roomSystem.frame.depth.data(), 255, gfx.roomSystem.frame.depth.size());
	warning("INPUT HIT PASS sprite depth scenery painting smallest/ties inclusive edges scene and stage zones visibility");
	overlay->hidden = true; if (!run("refresh") || !pixel(0, 0, room->data.palette)) return false;
	const Bytes retained = gfx.display.picture(); gfx.roomSystem.current.reset(); gfx.roomSystem.frame = Frame();
	if (!run("refresh") || gfx.display.picture() != retained) { warning("STAGE DISPLAY FAIL uncovered matte"); return false; }
	// A conversation still needs the UI band when there is no SET backdrop.
	StageView view; Bytes band;
	const Frame *matte = view.frame(gfx.stageSystem.current, 0);
	if (!matte || !StageView::compose(matte, gfx.stageSystem.current->data.palette, nullptr, nullptr, band, false) || band[264 * 512 * 4 + 3] != 255) return false;
	Session input;
	if (!input.init(root) || !input.step() || input.runtime.globals["idles"].number() != 1 || input.elapsedMillis() != 50) { warning("INPUT FAIL authored idle pacing"); return false; }
	Common::Event key; key.type = Common::EVENT_KEYDOWN; key.kbd.keycode = Common::KEYCODE_DOWN;
	input.postEvent(key); if (!input.step() || input.runtime.globals["seen"].text() != "0downarrow" || input.runtime.globals["idles"].number() != 1) { warning("INPUT FAIL arrow router"); return false; }
	input.runtime.globals["seen"] = Value(Common::String());
	key.kbd.keycode = Common::KEYCODE_a; key.kbd.ascii = 'a'; input.postEvent(key);
	key.kbdRepeat = true; for (uint32 i = 0; i < 40; ++i) input.postEvent(key);
	if (input.pendingEvents.size() != 2 || !input.step() || !input.step() || input.runtime.globals["seen"].text() != "arepeat:a") { warning("INPUT FAIL repeat coalescing/router"); return false; }
	key.kbdRepeat = false; for (uint32 i = 0; i < 40; ++i) input.postEvent(key);
	if (input.pendingEvents.size() != 32) { warning("INPUT FAIL capacity"); return false; }
	input.runtime.run(*input.bootScripts[0], "flush"); if (!input.pendingEvents.empty()) { warning("INPUT FAIL flush"); return false; }
	Common::Event notice; notice.type = Common::EVENT_SCREEN_CHANGED;
	for (uint32 i = 0; i < 40; ++i) input.postEvent(notice);
	if (!input.pendingEvents.empty()) { warning("INPUT FAIL display notifications entered gesture queue"); return false; }
	Common::Event down; down.type = Common::EVENT_LBUTTONDOWN; down.mouse = Common::Point(50, 37);
	if (!input.stageSystem.open("codex")) return false;
	input.postEvent(down);
	if (!input.step() || input.runtime.globals["clickpoint"].number() != int32(40u << 16 | 29) || input.runtime.globals["clickname"].text() != "upper" || input.runtime.globals["clicktype"].text() != "button") { warning("INPUT FAIL scaled click authored router/hit"); return false; }
	input.runtime.run(*input.bootScripts[0], "pointerprobe");
	if (input.runtime.globals["sampledpoint"].number() != int32(40u << 16 | 29) || !input.runtime.globals["sampledbutton"].truth()) { warning("INPUT FAIL pointer/button poll"); return false; }
	Common::Event up = down; up.type = Common::EVENT_LBUTTONUP; input.postEvent(up);
	input.runtime.run(*input.bootScripts[0], "pointerprobe"); if (input.runtime.globals["sampledbutton"].truth()) { warning("INPUT FAIL release poll"); return false; }
	if (input.hitTest(41, 30).type != "flat" || input.hitTest(-1, 30).type != "none") { warning("INPUT FAIL flat/miss"); return false; }
	input.runtime.run(*input.bootScripts[0], "flush");
	key.kbd.keycode = Common::KEYCODE_ESCAPE; input.postEvent(key);
	if (!input.step() || input.runtime.globals["seen"].text() != "flat:.") { warning("INPUT FAIL overlay flat key priority"); return false; }
	if (!input.stageSystem.go("second")) return false;
	input.postEvent(key); if (!input.step() || input.runtime.globals["seen"].text() != "stage:.") { warning("INPUT FAIL overlay stage key fallback"); return false; }
	Common::Event right = down; right.type = Common::EVENT_RBUTTONDOWN; input.postEvent(down); input.postEvent(right); input.postEvent(up);
	if (!input.pointerDown) { warning("INPUT FAIL simultaneous buttons"); return false; }
	right.type = Common::EVENT_RBUTTONUP; input.postEvent(right); if (input.pointerDown) { warning("INPUT FAIL both buttons released"); return false; }
	Bytes angles; if (!read(root.getChild("angles.bin"), angles) || angles.empty() || angles.size() % 12) return false;
	Reader angleReader(angles); uint32 angleCount = 0;
	while (angleReader.pos() < angles.size()) {
		Values points; points.push_back(Value(int32(angleReader.u32()))); points.push_back(Value(int32(angleReader.u32()))); const int32 expected = int32(angleReader.u32());
		const ScriptResult answer = input.runtime.run(*input.bootScripts[0], "angles", points);
		if (answer.signal != ScriptResult::kReturn || answer.value.number() != expected) { warning("INPUT ANGLES FAIL row=%u expected=%d got=%d", angleCount, expected, answer.value.number()); return false; }
		++angleCount;
	}
	warning("INPUT ANGLES PASS original executable table cases=%u", angleCount);
	Common::Event quit; quit.type = Common::EVENT_QUIT; input.postEvent(quit); input.runtime.run(*input.bootScripts[0], "flush");
	if (input.step()) { warning("INPUT FAIL quit preserved across flush"); return false; }
	warning("INPUT PASS boot idle key/click routers down arrow repeat coalescing 32-slot queue pointer button release flush quit");
	warning("STAGE DISPLAY PASS native RGB room/flat layering visibility independent palettes black holds fades matte retention");
	Session autoGuard(system); if (!autoGuard.init(root) || !autoGuard.stageSystem.open("display")) return false;
	autoGuard.roomSystem.current = room; autoGuard.roomSystem.visible = true;
	if (!autoGuard.canAutosave()) { warning("AUTOSAVE GUARD FAIL navigable idle world rejected"); return false; }
	autoGuard.pointerDown = true; if (autoGuard.canAutosave()) return false; autoGuard.pointerDown = false;
	autoGuard.roomSystem.moving = true; if (autoGuard.canAutosave()) return false; autoGuard.roomSystem.moving = false;
	autoGuard.roomSystem.visible = false; if (autoGuard.canAutosave()) return false; autoGuard.roomSystem.visible = true;
	warning("AUTOSAVE GUARD PASS idle world only title documents held gestures movement excluded");
	const bool hadHints = ConfMan.hasKey("dreamfactory_hotspot_hints", Common::ConfigManager::kTransientDomain);
	const Common::String oldHints = hadHints ? ConfMan.get("dreamfactory_hotspot_hints", Common::ConfigManager::kTransientDomain) : Common::String();
	ConfMan.setBool("dreamfactory_hotspot_hints", true, Common::ConfigManager::kTransientDomain);
	Session hints(system); if (!hints.init(root) || !hints.stageSystem.open("display")) return false;
	hints.runtime.run(test, "resetstage"); Bytes beforeHints, afterHints, restoredHints;
	if (!hints.display.capture(beforeHints)) return false;
	Region marker; marker.name = "hint-test"; marker.rect = Common::Rect(20, 10, 40, 30);
	hints.stageSystem.current->data.flats[0].regions.push_back(marker);
	hints.advanceTime(300, false); hints.runtime.run(test, "resetstage");
	if (!hints.display.capture(afterHints) || afterHints == beforeHints || hints.hitTest(20, 12).type != "button") return false;
	Common::Event toggle; toggle.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; toggle.customType = 1;
	hints.postEvent(toggle); hints.runtime.run(test, "resetstage");
	if (!hints.display.capture(restoredHints) || restoredHints != beforeHints || !hints.pendingEvents.empty()) return false;
	if (hadHints) ConfMan.set("dreamfactory_hotspot_hints", oldHints, Common::ConfigManager::kTransientDomain);
	else ConfMan.removeKey("dreamfactory_hotspot_hints", Common::ConfigManager::kTransientDomain);
	warning("HOTSPOT HINTS PASS enabled startup touch-only availability sampled actual targets toggle original pixels hit geometry preserved");
	Bytes beforeFocus, afterFocus;
	hints.movies.setHotspotHints(false);
	if (!hints.display.capture(beforeFocus)) return false;
	Common::Event focus; focus.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; focus.customType = kPadNext;
	hints.postEvent(focus);
	if (hints.hitTest(hints.pointer.x, hints.pointer.y).name != "hint-test" || !hints.display.capture(afterFocus) || beforeFocus != afterFocus) {
		warning("INPUT CONTROLS FAIL shoulder focus changed artwork instead of only moving cursor"); return false;
	}
    if(touchDirection(48,140)!=kPadUp || touchDirection(83,175)!=kPadUp || touchDirection(84,175)!=-1 || touchDirection(66,238)!=kPadDown || touchDirection(26,198)!=kPadLeft || touchDirection(106,198)!=kPadRight) return false;
    Bytes padPixels; padPixels.resize(512*384*4); memset(padPixels.data(),77,padPixels.size()); drawTouchPad(padPixels);
    if(padPixels[0]!=77 || padPixels[(140*512+48)*4]!=235 || padPixels[(140*512+84)*4]!=77) return false;
    Session controls; if(!controls.init(root) || !controls.stageSystem.open("codex")) return false;
    Common::Event pad; pad.type=Common::EVENT_CUSTOM_ENGINE_ACTION_START; pad.customType=kPadNext; controls.postEvent(pad);
    if(controls.hitTest(controls.pointer.x,controls.pointer.y).name!="upper") return false;
    pad.customType=kPadInteract; controls.postEvent(pad); if(!controls.pointerDown) return false;
    pad.type=Common::EVENT_CUSTOM_ENGINE_ACTION_END; controls.postEvent(pad);
    if(controls.pointerDown || !controls.step() || controls.runtime.globals["clickname"].text()!="upper") return false;
	pad.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START; pad.customType = kPadClick; controls.postEvent(pad);
	if (!controls.pointerDown || controls.pendingEvents.empty() || controls.pendingEvents.back().type != Common::EVENT_LBUTTONDOWN) return false;
	pad.type = Common::EVENT_CUSTOM_ENGINE_ACTION_END; controls.postEvent(pad);
	if (controls.pointerDown || !controls.step() || controls.runtime.globals["clickname"].text() != "upper") return false;
    controls.pendingEvents.clear(); controls.roomSystem.current=room; controls.roomSystem.visible=true;
    controls.runtime.globals["seen"]=Value(0); pad.type=Common::EVENT_CUSTOM_ENGINE_ACTION_START; pad.customType=kPadDown; controls.postEvent(pad);
    if(!controls.step() || controls.runtime.globals["seen"].text()!="0downarrow") return false;
    const bool hadTouch=ConfMan.hasKey("dreamfactory_touch_controls",Common::ConfigManager::kTransientDomain);
    const Common::String oldTouch=hadTouch?ConfMan.get("dreamfactory_touch_controls",Common::ConfigManager::kTransientDomain):Common::String();
    ConfMan.setBool("dreamfactory_touch_controls",true,Common::ConfigManager::kTransientDomain);
    controls.runtime.globals["seen"]=Value(0); down.mouse=Common::Point(83,198); controls.postEvent(down); up.mouse=down.mouse; controls.postEvent(up);
    if(controls.pointerDown || !controls.step() || controls.runtime.globals["seen"].text()!="0uparrow") { warning("TOUCH CONTROLS FAIL arrow routing"); return false; }
    controls.pendingEvents.clear(); controls.roomSystem.moving=true; controls.postEvent(down); controls.postEvent(up);
    if(controls.pendingEvents.empty() || controls.pendingEvents[0].type!=Common::EVENT_LBUTTONDOWN) return false;
    if(hadTouch) ConfMan.set("dreamfactory_touch_controls",oldTouch,Common::ConfigManager::kTransientDomain); else ConfMan.removeKey("dreamfactory_touch_controls",Common::ConfigManager::kTransientDomain);
    warning("INPUT CONTROLS PASS gamepad hotspot focus paired click authored movement touch bounds pad pixels original click fallback during movement");
	Session journalSave;
	if (!journalSave.init(root) || !journalSave.stageSystem.open("display", nullptr, false)) return false;
	Bytes withoutJournal, withJournal; Common::String journalError;
	if (!journalSave.saveState(withoutJournal, journalError)) { warning("JOURNAL SAVE FAIL baseline %s", journalError.c_str()); return false; }
	JournalEntry entry; entry.speaker = "Speaker"; entry.file = "speech.pup"; entry.ident = "line.0";
	entry.room = "Room"; entry.time = 12345; entry.bookmarked = true; entry.text = "Chosen reply";
	entry.choices.push_back("Another reply"); entry.choices.push_back(entry.text); entry.selected = 1;
	journalSave.journal.add(entry);
	if (!journalSave.saveState(withJournal, journalError) || READ_LE_UINT32(withJournal.data() + 4) != 10) { warning("JOURNAL SAVE FAIL encode %s", journalError.c_str()); return false; }
	journalSave.journal.entries.clear();
	if (!journalSave.loadState(withJournal, journalError) || journalSave.journal.entries.size() != 1) { warning("JOURNAL SAVE FAIL decode %s", journalError.c_str()); return false; }
	const JournalEntry &restoredEntry = journalSave.journal.entries[0];
	if (!restoredEntry.bookmarked || restoredEntry.selected != 1 || restoredEntry.choices != entry.choices || restoredEntry.text != entry.text || restoredEntry.time != 12345 || restoredEntry.file != entry.file) return false;
	journalSave.journal.entries[0].selected = 5;
	Bytes invalidJournal;
	if (journalSave.saveState(invalidJournal, journalError)) return false;
	if (!journalSave.loadState(withoutJournal, journalError) || !journalSave.journal.entries.empty()) { warning("JOURNAL SAVE FAIL legacy %s", journalError.c_str()); return false; }
	warning("JOURNAL SAVE PASS bounded v10 history choices bookmarks timestamps invalid row rejection old-save compatibility");
	RouteTestFiles routeFiles;
	if (!journalSave.routes.open(&routeFiles, "save-test", "fixture", journalError)) return false;
	JournalEntry sceneEntry; sceneEntry.speaker = "Speaker"; sceneEntry.file = "speech.pup";
	const uint32 sceneId = journalSave.routes.graph.recordScene(sceneEntry);
	const uint32 decisionId = journalSave.routes.graph.recordDecision(entry);
	Bytes routeSave;
	if (!journalSave.saveState(routeSave, journalError) || READ_LE_UINT32(routeSave.data() + 4) != 11) return false;
	if (!journalSave.routes.writeCheckpoint(sceneId, withoutJournal, journalError)) return false;
	const uint32 newScene = journalSave.routes.graph.recordScene(sceneEntry);
	if (!journalSave.loadState(routeSave, journalError) || journalSave.routes.graph.nodes().size() != 3 || journalSave.routes.graph.cursor() != decisionId || journalSave.routes.graph.choice() != 1) return false;
	if (!journalSave.loadState(withoutJournal, journalError) || journalSave.routes.graph.cursor() || journalSave.routes.graph.nodes().size() != 3) return false;
	if (!journalSave.restoreRouteScene(decisionId, journalError) || journalSave.routes.graph.cursor() != sceneId || journalSave.routes.graph.nodes().size() != 3 || !journalSave.stageSystem.current) return false;
	const Common::String savedSnapshot = routeFiles.listSavefiles("*.checkpoint")[0]; routeFiles.files[savedSnapshot].back() ^= 1;
	journalSave.runtime.globals["routeRetained"] = Value(72);
	if (journalSave.restoreRouteScene(decisionId, journalError) || journalSave.runtime.globals["routeRetained"].number() != 72 || journalSave.routes.graph.cursor() != sceneId) return false;
	// A different profile may reuse numeric IDs; its incompatible lineage must not be selected.
	journalSave.routes.graph = RouteGraph(); sceneEntry.speaker = "Another speaker";
	journalSave.routes.graph.recordScene(sceneEntry); journalSave.routes.graph.recordDecision(entry);
	if (!journalSave.loadState(routeSave, journalError) || journalSave.routes.graph.cursor() || journalSave.routes.graph.nodes().size() != 2 || newScene != 3) return false;
	warning("ROUTE SAVE PASS v11 playthrough cursor cumulative discovery old-save compatibility safe native checkpoint restore corrupt restore leaves game intact foreign lineage rejected");

	return true;
}
bool auditStages(const Common::FSNode &root) {
	Common::FSList files;
	if (!corpusFiles(root, files)) { warning("STAGE FAIL unable to enumerate corpus"); return false; }
	uint32 total = 0, failures = 0;
	for (uint32 i = 0; i < files.size(); ++i) {
		const Common::String name = label(files[i], root);
		if (!name.hasSuffixIgnoreCase(".stg") && !name.hasSuffixIgnoreCase(".flt")) continue;
		Bytes data; Stage stage;
		if (!read(files[i], data) || !stage.load(data)) { warning("STAGE FAIL %s load", name.c_str()); ++failures; continue; }
		AuditHash meta, pixels, art; uint32 regions = 0, frames = 0;
		Frame backdrop; backdrop.width = 512; backdrop.height = 264; backdrop.pixels.resize(512 * 264);
		byte roomPalette[768], dimRoom[768], dimStage[768];
		for (uint32 y = 0; y < 264; ++y) for (uint32 x = 0; x < 512; ++x) backdrop.pixels[y * 512 + x] = byte(x + y * 3);
		for (uint32 c = 0; c < 768; ++c) { roomPalette[c] = byte(c * 7 + 13); dimRoom[c] = byte(roomPalette[c] * 63 / 255); dimStage[c] = byte(stage.palette[c] * 127 / 255); }
		meta.number(stage.version); meta.number(stage.width); meta.number(stage.height); meta.number(stage.mainScript); meta.text(stage.name);
		for (uint32 p = 0; p < 768; ++p) meta.byteValue(stage.palette[p]);
		for (uint32 f = 0; f < stage.flats.size(); ++f) {
			const Flat &flat = stage.flats[f];
			meta.number(uint32(flat.condition)); meta.number(flat.script); meta.number(flat.image); meta.number(flat.clickLogic);
			meta.number(flat.width); meta.number(flat.height); meta.text(flat.name);
			for (uint32 r = 0; r < flat.regions.size(); ++r) {
				const Region &region = flat.regions[r]; ++regions;
				meta.number(uint32(region.rect.top)); meta.number(uint32(region.rect.left));
				meta.number(uint32(region.rect.bottom)); meta.number(uint32(region.rect.right));
				meta.number(region.script); meta.text(region.name);
			}
			if (flat.image) {
				Frame frame;
				if (!decodeFrame(stage.archive.get(flat.image), frame)) { warning("STAGE FAIL %s flat=%u image=%u", name.c_str(), f, flat.image); ++failures; continue; }
				pixels.number(frame.width); pixels.number(frame.height);
				for (uint32 p = 0; p < frame.pixels.size(); ++p) pixels.byteValue(frame.pixels[p]);
				for (uint32 variant = 0; variant < 3; ++variant) {
					Bytes picture;
					if (!StageView::compose(&frame, variant == 2 ? dimStage : stage.palette, variant ? &backdrop : nullptr, variant == 2 ? dimRoom : roomPalette, picture, false)) { ++failures; continue; }
					for (uint32 p = 0; p < picture.size(); ++p) art.byteValue(picture[p]);
				}
				++frames;
			}
		}
		warning("STAGEART %s pictures=%u rgba=%08x", name.c_str(), frames * 3, art.hash);
		warning("STAGE %s flats=%u regions=%u frames=%u meta=%08x pixels=%08x", name.c_str(), stage.flats.size(), regions, frames, meta.hash, pixels.hash);
		++total;
	}
	warning("STAGE %s files=%u failures=%u", failures ? "FAIL" : "PASS", total, failures);
	return !failures;
}
static bool auditScriptQuit(const Common::FSNode &root) {
	Session session;
	if (!session.init(root)) return false;
	ScriptInstance script; script.name = "quit-test";
	Expression quit; quit.kind = Expression::kCall; quit.name = "quit"; quit.opcode = 12071;
	Expression number; number.number = 1;
	Expression finish; finish.kind = Expression::kCall; finish.name = "finish"; finish.opcode = 5;
	script.script.expressions.push_back(quit); script.script.expressions.push_back(number); script.script.expressions.push_back(finish);
	Statement call; call.kind = Statement::kCall; call.expressions.push_back(0);
	Statement after; after.kind = Statement::kAssign; after.name = "afterquit"; after.expressions.push_back(1);
	Handler inner; inner.name = "finish"; inner.body.push_back(call); inner.body.push_back(after);
	script.script.handlers[inner.name] = inner;
	call.expressions[0] = 2; after.name = "afterreturn";
	Handler outer; outer.name = "test"; outer.body.push_back(call); outer.body.push_back(after);
	script.script.handlers[outer.name] = outer;
	session.runtime.globals["retained"] = Value(7);
	Common::Event event; event.type = Common::EVENT_KEYDOWN; session.pendingEvents.push_back(event);
	const ScriptResult result = session.runtime.run(script, "test");
	if (result.signal != ScriptResult::kAbandoned || !session.runtime.error.empty() || !session.scriptQuitRequested() || session.step() || !session.pendingEvents.empty() || session.runtime.globals.contains("afterquit") || session.runtime.globals.contains("afterreturn") || session.runtime.globals["retained"].number() != 7) {
		warning("SCRIPT QUIT FAIL unwinding/stopped state error=%s", session.runtime.error.c_str()); return false;
	}
	if (!session.init(root) || session.scriptQuitRequested()) { warning("SCRIPT QUIT FAIL reinitialization"); return false; }
	event.type = Common::EVENT_QUIT; session.postEvent(event);
	if (session.scriptQuitRequested() || session.step()) { warning("SCRIPT QUIT FAIL external stop distinction"); return false; }
	warning("SCRIPT QUIT PASS nested unwinding no later writes input cleared restart external stop distinction"); return true;
}

static bool auditRetailClock(const Common::FSNode &root) {
	Session session;
	if (!session.init(root)) return false;
	Common::SharedPtr<ScriptInstance> clock;
	for (uint32 i = 0; i < session.bootScripts.size(); ++i)
		if (session.bootScripts[i]->script.handlers.contains("calctime") && session.bootScripts[i]->script.handlers.contains("canadvance")) clock = session.bootScripts[i];
	if (!clock) return true; // This is the retail sinking clock, absent in Dust/demo.
	for (uint32 sentinel = 0; sentinel < 2; ++sentinel) {
		session.runtime.globals.clear();
		auto &g = session.runtime.globals;
		g["sinkflag"] = Value(1); g["clockcount"] = Value(19);
		g["hrs"] = Value(13); g["min"] = Value(8); g["sec"] = Value(59);
		g["mission"] = Value(4); g["phase"] = Value(0);
		g["clock"] = sentinel ? Value(Common::String("startdisk2")) : Value(1308);
		session.runtime.run(*clock, "calctime");
		if (!session.runtime.error.empty() || g["sec"].number() != 0 || g["min"].number() != 9 || g["clock"].isString || g["clock"].number() != 1309 || g["clockcount"].number() != 0) {
			warning("RETAIL CLOCK FAIL sentinel=%u sec=%s min=%s clock=%s count=%s error=%s", sentinel, g["sec"].text().c_str(), g["min"].text().c_str(), g["clock"].text().c_str(), g["clockcount"].text().c_str(), session.runtime.error.c_str());
			return false;
		}
	}
	const int32 limits[] = { 1315, 1330, 1345, 1355, 1400, 1405 };
	for (uint32 phase = 0; phase < ARRAYSIZE(limits); ++phase) {
		for (uint32 reached = 0; reached < 2; ++reached) {
			session.runtime.globals["phase"] = Value(phase);
			session.runtime.globals["clock"] = Value(limits[phase] - (reached ? 0 : 1));
			const ScriptResult result = session.runtime.run(*clock, "canadvance");
			if (!session.runtime.error.empty() || result.value.truth() != (reached != 0)) {
				warning("RETAIL CLOCK FAIL phase=%u boundary=%d reached=%u", phase, limits[phase], reached); return false;
			}
		}
	}
	warning("RETAIL CLOCK PASS authored rollover, startdisk2 normalization and all six sinking phase boundaries");
	return true;
}

static bool auditCheckersOracle(const Common::FSNode &fixtures) {
	Bytes bytes;
	if (!read(fixtures.getChild("checkers.cases"), bytes)) { warning("CHECKERS ORACLE FAIL missing fixtures"); return false; }
	Common::StringTokenizer rows(Common::String(reinterpret_cast<const char *>(bytes.data()), bytes.size()), "\n");
	uint32 count = 0;
	while (!rows.empty()) {
		Common::StringTokenizer fields(rows.nextToken(), "|");
		const int32 depth = atoi(fields.nextToken().c_str()), side = atoi(fields.nextToken().c_str());
		Common::String expected = fields.nextToken(); if (expected == "-") expected.clear();
		const Common::String actual = checkersMove(fields.nextToken(), depth, side);
		if (actual != expected || !fields.empty()) { warning("CHECKERS ORACLE FAIL case=%u actual=%s expected=%s", count, actual.c_str(), expected.c_str()); return false; }
		++count;
	}
	warning("CHECKERS ORACLE PASS cases=%u", count); return count >= 100;
}
bool auditScripts(const Common::FSNode &root, const Common::FSNode &fixtures) {
	Common::FSList files;
	if (!corpusFiles(root, files)) { warning("SCRIPT FAIL unable to enumerate corpus"); return false; }
	uint32 scripts = 0, handlers = 0, failures = 0;
	for (uint32 i = 0; i < files.size(); ++i) {
		Bytes data; Archive archive;
		if (!read(files[i], data) || !archive.load(data)) continue;
		uint32 fileScripts = 0, fileHandlers = 0, fileStatements = 0;
		for (uint32 c = 0; c < archive.size(); ++c) {
			ScriptTokens tokens; Common::String error;
			if (!decodeScript(archive.get(c), tokens, error)) continue;
			bool code = false;
			for (uint32 t = 0; t < tokens.size(); ++t) if (tokens[t].opcode == 4001) code = true;
			if (!code) continue;
			Script script;
			if (!parseScript(tokens, script, error)) {
				warning("SCRIPT FAIL %s container=%u %s", label(files[i], root).c_str(), c, error.c_str()); ++failures; continue;
			}
			++fileScripts; fileHandlers += script.handlers.size(); fileStatements += statementCount(script.topLevel);
			for (auto h = script.handlers.begin(); h != script.handlers.end(); ++h) fileStatements += statementCount(h->_value.body);
		}
		if (fileScripts) warning("SCRIPT %s scripts=%u handlers=%u statements=%u", label(files[i], root).c_str(), fileScripts, fileHandlers, fileStatements);
		scripts += fileScripts; handlers += fileHandlers;
	}
	Common::FSList tests;
	if (!fixtures.getChildren(tests, Common::FSNode::kListFilesOnly)) { warning("SCRIPT missing fixture directory"); return false; }
	for (uint32 i = 0; i < tests.size(); ++i) {
		if (!tests[i].getName().hasSuffixIgnoreCase(".dfscript")) continue;
		Bytes data; ScriptInstance instance; instance.name = "test"; Common::String error;
		if (!read(tests[i], data) || !loadScript(data, instance.script, error)) {
			warning("SCRIPT fixture parse failed %s %s", tests[i].getName().c_str(), error.c_str()); ++failures; continue;
		}
		Runtime standalone; Session session;
		const bool host = tests[i].getName().hasPrefix("host-");
		if (host && !session.init(root)) { warning("SCRIPT fixture host failed"); ++failures; continue; }
		Runtime &runtime = host ? session.runtime : standalone;
		ScriptResult result = runtime.run(instance, "test");
		Common::String value = runtime.error.empty() ? result.value.text() : "ERROR";
		warning("TEST %s value=%s string=%u consumed=%u", tests[i].getName().c_str(), value.c_str(), result.value.isString ? 1 : 0, runtime.eventConsumed ? 1 : 0);
	}
	if (!auditScriptQuit(root) || !auditRetailClock(root) || !checkersSelfTest() || !auditCheckersOracle(fixtures)) ++failures;
	warning("SCRIPT %s scripts=%u handlers=%u failures=%u", failures ? "FAIL" : "PASS", scripts, handlers, failures);
	return !failures;
}
}
