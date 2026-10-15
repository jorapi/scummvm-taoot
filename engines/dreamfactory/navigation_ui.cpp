/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/navigation_ui.h"
#include "dreamfactory/session.h"
#include "dreamfactory/dialogue.h"
#include "common/system.h"
#include "common/translation.h"
#include "gui/gui-manager.h"
#include "gui/dialog.h"
#include "graphics/managed_surface.h"
namespace DreamFactory {
NavigationWidget::NavigationWidget(GUI::Dialog *boss, Session &session) : GUI::Widget(boss, 12, 80, 300, 150), GUI::CommandSender(boss), _session(session) {
	setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_CLEARBG | GUI::WIDGET_WANT_TICKLE | GUI::WIDGET_TRACK_MOUSE | GUI::WIDGET_HOOK_DRAG);
	_session.atlas.project(_session, _current); _page = MAX(1, _current.page); rebuild(); focusCurrent(); _ready = true;
}
MapPosition NavigationWidget::selectedPosition() const {
	if (_selected < 0 || uint32(_selected) >= _items.size()) return MapPosition();
	const uint32 id = _items[_selected]; return id ? _session.travel.visits[id - 1].position : _current;
}
void NavigationWidget::rebuild() {
	_items.clear();
	for (uint32 i = 0; i < _session.travel.visits.size(); ++i) {
		const auto &visit = _session.travel.visits[i]; if (visit.position.page != _page) continue;
		bool replaced = false;
		for (uint32 j = 0; j < _items.size(); ++j) {
			const auto &previous = _session.travel.visits[_items[j] - 1];
			if (previous.position.room.equalsIgnoreCase(visit.position.room)) { if (previous.last <= visit.last) _items[j] = i + 1; replaced = true; break; }
		}
		if (!replaced) _items.push_back(i + 1);
	}
	if (_current.valid() && _current.page == _page) {
		const uint32 id = _session.travel.find(_current); bool replaced = false;
		for (uint32 j = 0; j < _items.size(); ++j) if (_session.travel.visits[_items[j] - 1].position.room.equalsIgnoreCase(_current.room)) { _items[j] = id; replaced = true; break; }
		if (!replaced) _items.push_back(id);
	}
	select(_items.empty() ? -1 : 0);
}
void NavigationWidget::select(int32 index) {
	_selected = index >= 0 && uint32(index) < _items.size() ? index : -1; centerSelection(); markAsDirty(); if (_ready) sendCommand(kNavigationSelect, selectedVisit());
}
Common::Rect NavigationWidget::imageRect() const {
	const int height = _session.atlas.retail() ? 264 : 334;
	const int scale = MAX(1, MIN(_w * 1024 / 512, _h * 1024 / height) * (_zoom ? 2 : 1));
	const int w = 512 * scale / 1024, h = height * scale / 1024;
	return Common::Rect((_w - w) / 2 + _panX, (_h - h) / 2 + _panY, (_w + w) / 2 + _panX, (_h + h) / 2 + _panY);
}
void NavigationWidget::centerSelection() {
	_panX = _panY = 0;
	const auto point = selectedPosition(); if (!_zoom || point.page != _page || point.x < 0) return;
	const auto rect = imageRect(); const int height = _session.atlas.retail() ? 264 : 334;
	_panX = _w / 2 - (rect.left + point.x * rect.width() / 512); _panY = _h / 2 - (rect.top + point.y * rect.height() / height);
}
void NavigationWidget::focusCurrent() {
	_page = _current.valid() ? _current.page : 1; rebuild();
	for (uint32 i = 0; i < _items.size(); ++i) { const auto point = _items[i] ? _session.travel.visits[_items[i] - 1].position : _current; if (point.room.equalsIgnoreCase(_current.room)) { select(i); break; } }
}
void NavigationWidget::toggleZoom() { _zoom = !_zoom; centerSelection(); markAsDirty(); sendCommand(kNavigationSelect, selectedVisit()); }
void NavigationWidget::reflowLayout() { GUI::Widget::reflowLayout(); centerSelection(); }
void NavigationWidget::drawWidget() {
	Graphics::ManagedSurface surface(_w, _h, g_system->getOverlayFormat());
	const uint32 navy = surface.format.RGBToColor(17, 26, 34), brass = surface.format.RGBToColor(228, 183, 99), blue = surface.format.RGBToColor(71, 195, 255), ivory = surface.format.RGBToColor(255, 255, 235);
	surface.fillRect(Common::Rect(_w, _h), navy);
	const Frame *image = _session.atlas.picture(_page); const auto rect = imageRect(); const int height = _session.atlas.retail() ? 264 : 334;
	if (image && rect.width() > 0 && rect.height() > 0) for (int y = MAX<int>(0, rect.top); y < MIN<int>(_h, rect.bottom); ++y) for (int x = MAX<int>(0, rect.left); x < MIN<int>(_w, rect.right); ++x) {
		const uint32 sx = (x - rect.left) * 512 / rect.width(), sy = (y - rect.top) * height / rect.height(); if (sx >= image->width || sy >= image->height) continue;
		const byte *color = _session.atlas.palette() + image->pixels[sy * image->width + sx] * 3; surface.setPixel(x, y, surface.format.RGBToColor(color[0], color[1], color[2]));
	}
	const auto dot = [&](const MapPosition &point, int radius, uint32 color) {
		if (point.page != _page || point.x < 0 || !rect.width() || !rect.height()) return;
		const int x = rect.left + point.x * rect.width() / 512, y = rect.top + point.y * rect.height() / height;
		for (int yy = -radius; yy <= radius; ++yy) for (int xx = -radius; xx <= radius; ++xx) if (x + xx >= 0 && x + xx < _w && y + yy >= 0 && y + yy < _h && xx * xx + yy * yy <= radius * radius) surface.setPixel(x + xx, y + yy, color);
	};
	for (uint32 id : _items) if (id) dot(_session.travel.visits[id - 1].position, 4, brass);
	for (const auto &encounter : _session.travel.encounters) {
		const auto &point = _session.travel.visits[encounter.visit - 1].position; if (point.page != _page || point.x < 0) continue;
		const int x = rect.left + point.x * rect.width() / 512 + 6, y = rect.top + point.y * rect.height() / height - 9;
		const int left = MAX(0, x - 1), top = MAX(0, y - 1), right = MIN<int>(_w, x + 15), bottom = MIN<int>(_h, y + 7);
		if (right > left && bottom > top) surface.fillRect(Common::Rect(left, top, right, bottom), navy);
		for (int dx = 1; dx <= 11; dx += 5) if (x + dx >= 0 && x + dx < _w && y + 2 >= 0 && y + 2 < _h) surface.setPixel(x + dx, y + 2, brass);
	}
	const auto selected = selectedPosition(); if (selected.valid()) { dot(selected, 7, navy); dot(selected, 6, ivory); dot(selected, 4, brass); }
	dot(_current, 3, blue);
	const auto &font = g_gui.getFont(GUI::ThemeEngine::kFontStyleNormal);
	const Common::U32String title = mapPageName(_page, _session.atlas.retail());
	surface.fillRect(Common::Rect(2, 2, MIN<int>(_w, font.getStringWidth(title) + 12), font.getFontHeight() + 8), navy);
	font.drawString(&surface, title, 6, 4, _w - 12, ivory, Graphics::kTextAlignLeft, 0, true);
	if (!_page) font.drawString(&surface, _("This visited location is outside the supplied ship chart."), 6, 28, _w - 12, ivory, Graphics::kTextAlignLeft, 0, true);
	g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), surface, Graphics::ALPHA_OPAQUE);
}
Common::U32String NavigationWidget::description() const {
	const MapPosition point = selectedPosition(); if (!point.valid()) return _("No visited locations have been recorded on this page. Blue marks your current position; gold marks rooms you have visited. Unmarked areas have no recorded visit.");
	Common::U32String body = mapLocationName(point); body += U"\n\n"; body += mapPageName(point.page, _session.atlas.retail());
	body += point.samePlace(_current) ? _("\n\nYou are here.") : _("\n\nA location you have visited in this save.");
	Common::U32StringArray here;
	for (const auto &encounter : _session.travel.encounters) {
		const auto &place = _session.travel.visits[encounter.visit - 1].position;
		if (place.page == point.page && place.room.equalsIgnoreCase(point.room)) here.push_back(Common::U32String(dialogueSpeaker(encounter.file, encounter.speaker), Common::kWindows1252));
	}
	if (!here.empty()) { body += _("\n\nLast spoken to in this room:\n\n"); for (const auto &name : here) { body += name; body += U"\n\n"; } body += _("These are recorded encounters, not live character positions."); }
	body += _("\n\nBlue: current position. Gold: visited room. White ring: selected location. Speech dots: recorded encounter. Only this save's visits and conversations are shown. Original map artwork is read-only here; selecting a marker does not move the player.");
	return body;
}
bool NavigationWidget::handleKeyDown(Common::KeyState key) {
	if (key.keycode == Common::KEYCODE_LEFT || key.keycode == Common::KEYCODE_RIGHT) {
		_page = (_page + (key.keycode == Common::KEYCODE_LEFT ? -1 : 1) + _session.atlas.pages() + 1) % (_session.atlas.pages() + 1); rebuild(); return true;
	}
	if (key.keycode == Common::KEYCODE_UP || key.keycode == Common::KEYCODE_DOWN) { if (!_items.empty()) select((_selected + (key.keycode == Common::KEYCODE_UP ? -1 : 1) + _items.size()) % _items.size()); return true; }
	if (key.keycode == Common::KEYCODE_RETURN) { sendCommand('JRed', 0); return true; }
	return false;
}
void NavigationWidget::handleMouseDown(int x, int y, int button, int clicks) { _held = true; _dragged = false; _down = _last = Common::Point(x, y); }
void NavigationWidget::handleMouseMoved(int x, int y, int button) {
	if (!_held) return;
	if (ABS(x - _down.x) + ABS(y - _down.y) > 6) _dragged = true;
	if (_dragged && _zoom) { _panX = CLIP<int32>(_panX + x - _last.x, -_w * 2, _w * 2); _panY = CLIP<int32>(_panY + y - _last.y, -_h * 2, _h * 2); markAsDirty(); }
	_last = Common::Point(x, y);
}
void NavigationWidget::handleMouseUp(int x, int y, int button, int clicks) {
	if (_held && !_dragged) {
		const auto rect = imageRect(); const int height = _session.atlas.retail() ? 264 : 334; int closest = -1, distance = 14 * 14;
		for (uint32 i = 0; i < _items.size(); ++i) { const auto point = _items[i] ? _session.travel.visits[_items[i] - 1].position : _current; if (point.x < 0) continue;
			const int dx = x - rect.left - point.x * rect.width() / 512, dy = y - rect.top - point.y * rect.height() / height, gap = dx * dx + dy * dy;
			if (gap < distance) { closest = i; distance = gap; }
		}
		if (closest >= 0) { select(closest); if (clicks == 2) sendCommand('JRed', 0); }
	}
	_held = false;
}
void NavigationWidget::handleMouseWheel(int x, int y, int direction) { handleKeyDown(Common::KeyState(direction < 0 ? Common::KEYCODE_UP : Common::KEYCODE_DOWN)); }
Common::Point NavigationWidget::markerPoint(uint32 index) const {
	if (index >= _items.size()) return Common::Point(-1, -1);
	const auto point = _items[index] ? _session.travel.visits[_items[index] - 1].position : _current; const auto rect = imageRect();
	return Common::Point(rect.left + point.x * rect.width() / 512, rect.top + point.y * rect.height() / (_session.atlas.retail() ? 264 : 334));
}
bool NavigationWidget::auditPointerInput() {
	if (_items.empty() || !_page) return false;
	const int32 previous = _selected; const bool previousZoom = _zoom;
	_zoom = false; select(-1);
	const auto point = _items[0] ? _session.travel.visits[_items[0] - 1].position : _current; const auto rect = imageRect();
	const int x = rect.left + point.x * rect.width() / 512, y = rect.top + point.y * rect.height() / (_session.atlas.retail() ? 264 : 334);
	if (x < 0 || y < 0 || x >= _w || y >= _h) return false;
	handleMouseDown(x, y, 1, 1); handleMouseUp(x, y, 1, 1);
	if (_selected != 0) return false;
	handleMouseWheel(x, y, 1); if (_items.size() > 1 && _selected != 1) return false;
	_zoom = true; centerSelection(); const int top = imageRect().top; const int selected = _selected;
	handleMouseDown(_w / 2, _h / 2, 1, 1); handleMouseMoved(_w / 2, _h / 2 + 20, 1); handleMouseUp(_w / 2, _h / 2 + 20, 1, 1);
	const bool panned = imageRect().top != top && _selected == selected;
	_zoom = previousZoom; select(previous); return panned;
}
}
