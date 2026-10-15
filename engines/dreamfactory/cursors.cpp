/* SPDX-License-Identifier: GPL-3.0-or-later
 * Named cursor lookup follows the executable resource scheme documented by
 * dreamREfactory/tools/dumpcursors.ts. Parsing uses ScummVM's Windows readers.
 */
#include "dreamfactory/cursors.h"
#include "common/memstream.h"
#include "common/formats/winexe.h"
#include "graphics/cursorman.h"
namespace DreamFactory {
void CursorSystem::reset() { _groups.clear(); names.clear(); _hover.clear(); name = "arrow"; depth = 0; apply(); }
bool CursorSystem::load(const Common::String &file) {
	_groups.clear(); names.clear(); Bytes bytes;
	if (!_resources.read(file, bytes)) { apply(); return false; }
	Common::MemoryReadStream stream(bytes.data(), bytes.size());
	Common::ScopedPtr<Common::WinResources> exe(Common::WinResources::createFromEXE(&stream));
	if (!exe) { apply(); return false; }
	const auto ids = exe->getIDList(Common::kWinGroupCursor);
	for (uint32 i = 0; i < ids.size(); ++i) {
		Common::String key = ids[i].getString(); key.toLowercase(); if (!key.hasPrefix("curs.")) continue; key = key.substr(5);
		Common::SharedPtr<Graphics::WinCursorGroup> group(Graphics::WinCursorGroup::createCursorGroup(exe.get(), ids[i]));
		if (!group || group->cursors.empty()) continue;
		_groups[key] = group; names.push_back(key);
	}
	apply(); return !names.empty();
}
const Graphics::Cursor *CursorSystem::art(const Common::String &asked) const {
	Common::String key = asked; key.toLowercase(); const auto found = _groups.find(key); if (found == _groups.end()) return nullptr;
	const auto &cursors = found->_value->cursors;
	for (uint32 i = 0; i < cursors.size(); ++i) if (cursors[i].cursor->getWidth() == 32 && cursors[i].cursor->getHeight() == 32) return cursors[i].cursor;
	return cursors[0].cursor;
}
void CursorSystem::set(Common::String value) { value.toLowercase(); name = value; _hover.clear(); apply(); }
void CursorSystem::hover(const Common::String &value) { if (_hover != value) { _hover = value; apply(); } }
void CursorSystem::apply() {
	if (!_display) return;
	const Graphics::Cursor *cursor = art(_hover.empty() ? name : _hover);
	if (cursor) CursorMan.replaceCursor(cursor);
	else { Common::ScopedPtr<Graphics::Cursor> fallback(Graphics::makeDefaultWinCursor()); CursorMan.replaceCursor(fallback.get()); }
	CursorMan.showMouse(visible());
}
}
