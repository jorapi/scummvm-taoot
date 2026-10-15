/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_NAVIGATION_UI_H
#define DREAMFACTORY_NAVIGATION_UI_H
#include "dreamfactory/navigation.h"
#include "gui/widget.h"
namespace DreamFactory {
const uint32 kNavigationSelect = 'NVSl';
class NavigationWidget : public GUI::Widget, public GUI::CommandSender {
	Session &_session;
	MapPosition _current;
	Common::Array<uint32> _items;
	int32 _page = 1, _selected = -1;
	bool _zoom = false, _held = false, _dragged = false, _ready = false;
	int32 _panX = 0, _panY = 0;
	Common::Point _down, _last;
	Common::Rect imageRect() const;
	void rebuild();
	void select(int32 index);
	void centerSelection();
protected:
	void drawWidget() override;
public:
	NavigationWidget(GUI::Dialog *boss, Session &session);
	bool wantsFocus() override { return true; }
	uint32 selectedVisit() const { return _selected >= 0 && uint32(_selected) < _items.size() ? _items[_selected] : 0; }
	MapPosition selectedPosition() const;
	Common::U32String description() const;
	uint32 page() const { return _page; }
	uint32 count() const { return _items.size(); }
	bool zoomed() const { return _zoom; }
	void focusCurrent();
	void toggleZoom();
	bool auditPointerInput();
	Common::Point markerPoint(uint32 index) const;
	int32 selectedIndex() const { return _selected; }
	void reflowLayout() override;
	bool handleKeyDown(Common::KeyState key) override;
	void handleMouseDown(int x, int y, int button, int clicks) override;
	void handleMouseMoved(int x, int y, int button) override;
	void handleMouseUp(int x, int y, int button, int clicks) override;
	void handleMouseWheel(int x, int y, int direction) override;
};
}
#endif
