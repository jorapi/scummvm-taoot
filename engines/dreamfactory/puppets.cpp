/* SPDX-License-Identifier: GPL-3.0-or-later
 * Puppet opening, neutral poses and branch containment follow runtime/puppet.ts.
 */
#include "dreamfactory/puppets.h"
#include "common/textconsole.h"
namespace DreamFactory {
void PuppetChoices::start(const Common::SharedPtr<LoadedPuppet> &puppet) {
	++_generation;
	escape(); _answer = -1; _released = -1; _focus = -1;
	if (!puppet) return;
	puppet->interrupted = false; puppet->pressed = -1; puppet->pressInside = false;
	if (!puppet->choices.empty()) _puppet = puppet;
}
void PuppetChoices::finish(int32 answer) {
	_answer = answer;
	if (!_puppet) return;
	_puppet->lastChoices = _puppet->choices;
	_puppet->lastChosen = answer < 0 ? -1 : _puppet->chosen;
	_puppet->voiceQueue.clear(); _puppet->pressed = -1; _puppet->pressInside = false;
	_puppet.reset(); _released = -1;
}
void PuppetChoices::down(int32 row, uint64 tick) {
	if (!_puppet || row < 0 || uint32(row) >= _puppet->choices.size()) return;
	_puppet->pressed = row; _puppet->pressInside = true; _until = tick + 10; _released = -1;
}
void PuppetChoices::move(int32 row) { if (_puppet) _puppet->pressInside = row >= 0 && row == _puppet->pressed; }
void PuppetChoices::focus(int32 direction) {
	if (!_puppet || _puppet->pressed >= 0 || _puppet->choices.empty()) return;
	const int32 count = MIN<uint32>(5, _puppet->choices.size());
	_focus = _focus < 0 ? (direction < 0 ? count - 1 : 0) : (_focus + (direction < 0 ? -1 : 1) + count) % count;
	_puppet->chosen = _focus;
}
void PuppetChoices::confirm(uint64 tick, int32 row) {
	if (!_puppet || _puppet->pressed >= 0) return;
	if (row < 0) row = _focus < 0 ? 0 : _focus;
	if (row < 0 || row >= 5 || uint32(row) >= _puppet->choices.size()) return;
	down(row, tick); up(row);
}
void PuppetChoices::up(int32 row) {
	if (!_puppet) return;
	if (row < 0 || row != _puppet->pressed || uint32(row) >= _puppet->choices.size()) {
		_puppet->pressed = -1; _puppet->pressInside = false; _released = -1; return;
	}
	_puppet->chosen = row; _released = row;
}
void PuppetChoices::tick(uint64 tick) {
	if (!_puppet) return;
	if (_puppet->choices.empty()) { escape(); return; }
	if (_released >= 0 && tick >= _until) {
		if (uint32(_released) < _puppet->choices.size()) finish(_puppet->choices[_released].id);
		else escape();
	}
}
static bool subtitled(const PuppetLine &line) {
	if (line.text.empty() || line.text[0] == '*') return false;
	for (uint32 i = 1; i <= 4; ++i) if (line.ident.equalsIgnoreCase(Common::String::format("idle %u", i))) return false;
	for (uint32 i = 0; i < line.text.size(); ++i) if (line.text[i] != ' ') return true;
	return false;
}
bool PuppetSpeech::start(const Common::SharedPtr<LoadedPuppet> &puppet, const Common::String &ident, uint64 now, bool queue) {
	abandon(); error.clear(); if (!puppet) return true;
	const int32 index = puppet->data.line(ident, true); if (index < 0) return true;
	if (queue && puppet->voiceQueue.size() < 3) puppet->voiceQueue.push_back(uint32(index));
	if (queue && puppet->interrupted) return true;
	const PuppetLine &line = puppet->data.lines[index];
	if (!puppet->data.animation(line.animation, _poses)) { error = "Invalid puppet speech animation: " + ident; return false; }
	Common::SharedPtr<AudioClip> clip(new AudioClip());
	uint64 duration = MAX<uint64>(1000, (uint64(line.text.size()) * 1000 + 14) / 15);
	if (decodeAudio(puppet->data.archive.get(line.audio), *clip) && clip->rate && !clip->samples.empty()) {
		duration = (uint64(clip->samples.size()) * 1000 + clip->rate - 1) / clip->rate;
		_host.puppetVoice(line.ident, clip);
	} else _host.puppetSilence();
	_puppet = puppet; _start = now; _deadline = now + duration + 150;
	puppet->stance = line.stance; puppet->subtitle = subtitled(line) ? line.text : Common::String();
	tick(now); return true;
}
void PuppetSpeech::tick(uint64 now) {
	if (!_puppet) return;
	if (!_poses.empty()) {
		const uint64 elapsed = now > _start ? now - _start : 0;
		const uint32 frame = uint32(MIN<uint64>(elapsed * 10 / 333, _poses.size() - 1));
		_puppet->pose = _poses[frame]; _puppet->hasPose = true;
	}
	if (now >= _deadline) finish();
}
void PuppetSpeech::finish() {
	if (!_puppet) return;
	_host.puppetSilence(); _puppet->subtitle.clear();
	if (!_poses.empty()) { _puppet->pose = _poses.back(); _puppet->hasPose = true; }
	_poses.clear(); _puppet.reset();
}
void PuppetSpeech::skip() { if (_puppet) { _puppet->interrupted = true; finish(); } }
void PuppetSpeech::abandon() { finish(); }
bool PuppetSystem::open(Common::String file) {
	file.toLowercase(); if (!file.hasSuffix(".pup")) file += ".pup";
	Bytes bytes; if (!_resources.read(file, bytes)) return false;
	Common::SharedPtr<LoadedPuppet> loaded(new LoadedPuppet()); loaded->file = file;
	if (!loaded->data.load(bytes)) { _runtime.fail("Invalid puppet: " + file); return false; }
	for (uint32 i = 0; i < loaded->data.scripts.size(); ++i) {
		const PuppetScript &ref = loaded->data.scripts[i]; Common::SharedPtr<ScriptInstance> script(new ScriptInstance()); script->name = ref.name;
		ScriptTokens tokens; Common::String error; if (!decodeScript(loaded->data.archive.get(ref.location), tokens, error)) continue;
		bool handlers = false; for (uint32 t = 0; t < tokens.size(); ++t) if (tokens[t].opcode == 4001) handlers = true;
		if (!handlers) continue;
		if (!parseScript(tokens, script->script, error)) { _runtime.fail("Puppet script " + ref.name + ": " + error); return false; }
		int32 existing = -1; for (uint32 s = 0; s < loaded->scripts.size(); ++s) if (loaded->scripts[s]->name == script->name) existing = s;
		if (existing >= 0) loaded->scripts[existing] = script; else loaded->scripts.push_back(script);
		if (script->name == "boot script") loaded->main = script;
	}
	for (uint32 i = 0; i < loaded->scripts.size(); ++i) if (loaded->scripts[i] != loaded->main) loaded->scripts[i]->parent = loaded->main.get();
	if (!loaded->data.lines.empty()) {
		const PuppetLine &line = loaded->data.lines[0]; loaded->stance = loaded->defaultStance = line.stance;
		Common::Array<PuppetPose> poses; if (!loaded->data.animation(line.animation, poses)) { _runtime.fail("Invalid puppet opening pose: " + file); return false; }
		if (!poses.empty()) { loaded->pose = loaded->defaultPose = poses[0]; loaded->hasPose = loaded->hasDefaultPose = true; }
	}
	current = loaded; return true;
}
Common::String PuppetSystem::name() const { return !current ? Common::String("none") : current->data.name.empty() ? current->file : current->data.name; }
bool PuppetSystem::base(const Common::String &ident) {
	if (!current) return true;
	if (ident.empty()) { current->pose = current->defaultPose; current->stance = current->defaultStance; current->hasPose = current->hasDefaultPose; return true; }
	const int32 index = current->data.line(ident); if (index < 0) return true;
	const PuppetLine &line = current->data.lines[index]; Common::Array<PuppetPose> poses;
	if (!current->data.animation(line.animation, poses)) { _runtime.fail("Invalid puppet base pose: " + ident); return false; }
	current->stance = line.stance; if (!poses.empty()) { current->pose = poses[0]; current->hasPose = true; }
	return true;
}
void PuppetSystem::clear() { if (current) { current->choices.clear(); current->subtitle.clear(); current->chosen = current->pressed = -1; current->pressInside = false; } }
void PuppetSystem::bevel(const Common::String &text, int32 id) {
	if (!current) return;
	if (current->choices.size() >= 5) { warning("DreamFactory puppet choice table full"); return; }
	current->chosen = -1; PuppetChoice choice; choice.text = text; choice.id = id; current->choices.push_back(choice);
}
Value PuppetSystem::send(const Common::String &script, const Common::String &handler, const Values &args, ScriptFrame &parent) {
	const auto loaded = current; if (!loaded) return Value(); Common::SharedPtr<ScriptInstance> own;
	for (uint32 s = 0; s < loaded->scripts.size(); ++s) if (loaded->scripts[s]->name.equalsIgnoreCase(script)) { own = loaded->scripts[s]; break; }
	if (!own) return Value();
	for (ScriptInstance *link = own.get(); link; link = link->parent) if (link->script.handlers.contains(handler)) {
		const ScriptResult result = _runtime.run(*link, handler, args, parent.me, &parent, Runtime::kAddressed);
		if (result.signal != ScriptResult::kPass) return result.value;
	}
	return Value();
}
}
