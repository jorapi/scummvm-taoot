/* SPDX-License-Identifier: GPL-3.0-or-later
 * Loop identity, capacity, countdown and ordered dispatch follow
 * dreamREfactory/runtime/scheduler.ts; pacing follows runtime/clock.ts.
 */
#include "dreamfactory/scheduler.h"
namespace DreamFactory {
void Scheduler::reset(uint64 now) { loops.clear(); frame = 0; _time = _service = now; _display = ticks(now); }
void Scheduler::stop(Common::String kind, Common::String name) {
	kind.toLowercase(); name.toLowercase();
	for (uint32 i = loops.size(); i > 0; --i) if (loops[i - 1]->kind == kind && (name == "all" || loops[i - 1]->name == name)) loops.remove_at(i - 1);
}
void Scheduler::pause(Common::String kind, Common::String name, bool paused) {
	kind.toLowercase(); name.toLowercase();
	for (uint32 i = 0; i < loops.size(); ++i) if (loops[i]->kind == kind && (name == "all" || loops[i]->name == name)) loops[i]->paused = paused;
}
bool Scheduler::has(Common::String kind, Common::String name) const {
	kind.toLowercase(); name.toLowercase();
	for (uint32 i = 0; i < loops.size(); ++i) if (loops[i]->kind == kind && loops[i]->name == name) return true;
	return false;
}
bool Scheduler::make(Common::String kind, Common::String name, Common::String handler, int32 period) {
	stop(kind, name); if (loops.size() >= 32) return false;
	Common::SharedPtr<GameLoop> loop(new GameLoop());
	kind.toLowercase(); name.toLowercase(); loop->kind = kind; loop->name = name;
	Common::String trimmed = handler; trimmed.trim();
	if (trimmed.hasSuffix(")")) {
		uint32 end = trimmed.size() - 1;
		while (end && (trimmed[end - 1] == ' ' || trimmed[end - 1] == '\t')) --end;
		if (end && trimmed[end - 1] == '(') { trimmed = trimmed.substr(0, end - 1); trimmed.trim(); handler = trimmed; }
	}
	loop->handler = handler; loop->count = loop->period = MAX<int32>(1, period); loops.push_back(loop); return true;
}
bool Scheduler::contains(const Common::SharedPtr<GameLoop> &loop) const {
	for (uint32 i = 0; i < loops.size(); ++i) if (loops[i] == loop) return true;
	return false;
}
static bool pass(uint64 &last, uint64 now, uint32 period) {
	if (now - last < period) return false;
	last = now - last >= uint64(period) * 2 ? now : last + period; return true;
}
void Scheduler::advance(uint64 now, int32 frameTicks, bool dispatch, LoopHost &host) {
	if (now < _time) return;
	_time = now;
	const bool coarse = pass(_service, now, 50), display = pass(_display, ticks(now), MAX<int32>(1, frameTicks));
	if (coarse && !host.serviceMotion()) return;
	if (coarse) host.serviceCrickets();
	if (coarse) service(false, true, dispatch, host);
	// Actor animation heads the draw after the coarse script callbacks.
	if (coarse) host.serviceAnimation(now);
	if (display) ++frame;
	if (display) service(true, true, dispatch, host);
	// A cooperative displayed frame may release coarse callbacks that already
	// reached zero during a busy script; it must not decrement their timers.
	if (display && !coarse) service(false, false, dispatch, host);
}
void Scheduler::service(bool smooth, bool countdown, bool dispatch, LoopHost &host) {
	Common::Array<Common::SharedPtr<GameLoop> > due;
	for (uint32 i = 0; i < loops.size(); ++i) {
		const auto &loop = loops[i];
		if (loop->paused || (loop->period <= 1) != smooth) continue;
		if (countdown && loop->count > 0) --loop->count;
		if (loop->count <= 0) due.push_back(loop);
	}
	if (!dispatch) return;
	for (uint32 i = 0; i < due.size(); ++i) {
		const auto &loop = due[i];
		if (!contains(loop) || loop->paused || !host.canFireLoop(*loop)) continue;
		// Remove this exact entry immediately before its call. An earlier handler
		// can replace a later entry; its replacement must wait for a new pass.
		for (uint32 at = 0; at < loops.size(); ++at) if (loops[at] == loop) { loops.remove_at(at); break; }
		if (!host.fireLoop(*loop)) break;
	}
}
}
