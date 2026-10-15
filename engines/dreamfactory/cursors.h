/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_CURSORS_H
#define DREAMFACTORY_CURSORS_H
#include "dreamfactory/resources.h"
#include "graphics/wincursor.h"
#include "common/ptr.h"
namespace DreamFactory {
// CURS.<script name> artwork is read from the user's engine executable.
class CursorSystem {
public:
	explicit CursorSystem(Resources &resources, bool display) : _resources(resources), _display(display) {}
	void reset();
	bool load(const Common::String &file);
	void set(Common::String name);
	void hover(const Common::String &name);
	const Common::String &displayedName() const { return _hover.empty() ? name : _hover; }
	void hide() { if (depth > -2147483647) --depth; apply(); }
	void show() { if (depth < 2147483647) ++depth; apply(); }
	bool visible() const { return depth >= 0 && name != "none"; }
	const Graphics::Cursor *art(const Common::String &name) const;
	Common::Array<Common::String> names;
	Common::String name = "arrow";
	int32 depth = 0;
private:
	Resources &_resources;
	bool _display;
	Common::String _hover;
	Common::HashMap<Common::String, Common::SharedPtr<Graphics::WinCursorGroup> > _groups;
	void apply();
};
}
#endif
