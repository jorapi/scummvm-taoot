/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/inventory.h"
#include "dreamfactory/session.h"
namespace DreamFactory {
bool inventoryItems(Session &session, Common::Array<InventoryItem> &items) {
	items.clear();
	const auto shop = session.propSystem.findShop("inven");
	if (!shop || !shop->main || session.runtime.busy()) return false;
	auto catalog = shop->main;
	const char *countHandler = "countitems", *indexHandler = "indextoitem";
	if (session.version == 1) {
		const int32 inventoryFlat = session.stageSystem.resolve("avatar");
		if (inventoryFlat < 0 || !session.stageSystem.current) return false;
		catalog = session.stageSystem.current->flats[inventoryFlat]; countHandler = "countday"; indexHandler = "indextoday";
		if (!catalog->script.handlers.contains(countHandler) || !catalog->script.handlers.contains(indexHandler)) return false;
	}
	Common::Array<Common::String> names;
	if (catalog->script.handlers.contains(countHandler) && catalog->script.handlers.contains(indexHandler)) {
		const int32 count = session.runtime.run(*catalog, countHandler).value.number();
		if (count < 0 || count > 256 || !session.runtime.error.empty()) return false;
		for (int32 index = 1; index <= count; ++index) {
			Values args; args.push_back(Value(index)); names.push_back(session.runtime.run(*catalog, indexHandler, args).value.text());
		}
	} else {
		for (const auto &prop : session.propSystem.props) if (prop->shop == shop) names.push_back(prop->name);
	}
	for (const auto &name : names) {
		const auto prop = session.propSystem.get(name);
		if (!prop || !prop->owner.text().equalsIgnoreCase(session.version == 1 ? "stranger" : "frank")) continue;
		bool duplicate = false; for (const auto &item : items) if (item.name.equalsIgnoreCase(name)) duplicate = true;
		if (duplicate) continue;
		InventoryItem item; item.name = name; item.title = name;
		if (name == "photo1" || name == "photo2" || name == "photo3") item.title = "Photograph";
		else if (name == "trunkkey") item.title = "Trunk key";
		if (shop->main->script.handlers.contains("getname")) {
			Values args; args.push_back(Value(name)); const Value label = session.runtime.run(*shop->main, "getname", args).value;
			if (label.isString && !label.text().empty()) item.title = label.text();
		}
		if (item.title.hasPrefix("this ")) item.title.erase(0, 5);
		else if (item.title.hasPrefix("these ")) item.title.erase(0, 6);
		if (!item.title.empty() && item.title[0] >= 'a' && item.title[0] <= 'z') item.title.setChar(item.title[0] - 'a' + 'A', 0);
		for (auto script = prop->script; script; ) {
			if (script->script.handlers.contains("infoyoself")) item.examinable = true;
			if (script == shop->main) break;
			script = shop->main;
		}
		items.push_back(item);
	}
	return session.runtime.error.empty();
}
bool inventoryPicture(Session &session, const Common::String &name, Bytes &rgba, uint32 &width, uint32 &height) {
	rgba.clear(); width = height = 0;
	const auto prop = session.propSystem.get(name);
	if (!prop || !prop->shop) return false;
	Prop preview = *prop;
	preview.selectView("large", 0);
	const PropState *state = preview.state();
	if (!state || state->pictures.empty()) return false;
	const Sprite *sprite = prop->shop->frame(state->pictures[preview.frameIndex()].location);
	if (!sprite || !sprite->width || !sprite->height || sprite->width > 1024 || sprite->height > 1024) return false;
	width = sprite->width; height = sprite->height; rgba.resize(width * height * 4);
	for (uint32 pixel = 0; pixel < width * height; ++pixel) {
		memcpy(rgba.data() + pixel * 4, prop->shop->data.palette + sprite->pixels[pixel] * 3, 3);
		rgba[pixel * 4 + 3] = sprite->opaque[pixel] ? 255 : 0;
	}
	return true;
}
}
