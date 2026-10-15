/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dreamfactory/inventory.h"
#include "dreamfactory/session.h"
#include "common/endian.h"
#include "common/translation.h"
namespace DreamFactory {
static Common::String itemDescription(const Common::String &name) {
	const struct { const char *name, *description; } descriptions[] = {
		{"painting", _s("A painting. Examine it for a closer look.")},
		{"notebook", _s("A notebook. Examine it to view its contents.")},
		{"rubaiyat", _s("A bound book. Examine it for a closer look.")},
		{"realneck", _s("A necklace. Examine it for a closer look.")}, {"fakeneck", _s("A necklace. Examine it for a closer look.")},
		{"photo1", _s("A photograph. Examine it to view the image.")}, {"photo2", _s("A photograph. Examine it to view the image.")}, {"photo3", _s("A photograph. Examine it to view the image.")},
		{"trunkkey", _s("A key. Use it on a matching lock.")}, {"carkeys", _s("A set of keys. Use them on a matching lock.")},
		{"pipe", _s("A smoking pipe.")}, {"prayercard", _s("A prayer card. Examine it to read it.")},
		{"hackclue", _s("A scrap of paper. Examine it to read it.")}, {"rubiclue", _s("A scrap of paper. Examine it to read it.")},
		{"zeitgram", _s("A telegram. Examine it to read the message.")}, {"thayergram", _s("A telegram. Examine it to read the message.")}, {"icewarning", _s("A telegram. Examine it to read the message.")},
		{"ring", _s("A ring.")}, {"gaspen", _s("A pen.")}, {"cufflink", _s("A cufflink.")},
		{"boatpass", _s("A boat pass. Examine it to read it.")}, {"conkletter", _s("A letter. Examine it to read it.")},
		{"binoculars", _s("A pair of binoculars.")}, {"shawl", _s("A shawl.")},
		{"chest", _s("A chest. Examine it for a closer look.")}, {"gun", _s("A gun. Use it with care.")},
		{"jug", _s("A jug. Examine it for a closer look.")}, {"boots", _s("A pair of boots.")},
		{"cards", _s("A pack of playing cards.")}, {"badge", _s("A badge.")}, {"pages", _s("Loose pages. Examine them to read them.")},
		{"hairpin", _s("A hairpin.")}, {"yunnibook", _s("A book. Examine it to view its contents.")},
		{"cigar", _s("A cigar.")}, {"sugarcubes", _s("Sugar cubes.")}, {"biscuits", _s("Biscuits.")}, {"bullets", _s("Ammunition.")},
		{"matchbox", _s("A matchbox.")}, {"hrkey", _s("A key. Use it on a matching lock.")}
	};
	for (const auto &entry : descriptions) if (name.equalsIgnoreCase(entry.name)) return entry.description;
	return _s("An item you are carrying. Examine it for the original close-up and details.");
}
bool inventoryItems(Session &session, Common::Array<InventoryItem> &items) {
	items.clear();
	const auto shop = session.propSystem.findShop("inven");
	if (!shop || !shop->main || (session.runtime.busy() && !session.runtime.isRunning("handleselect"))) return false;
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
		InventoryItem item; item.name = prop->name; item.title = prop->name;
		Common::String id = item.name; id.toLowercase(); item.description = itemDescription(id);
		if (id == "photo1" || id == "photo2" || id == "photo3") item.title = "Photograph";
		else if (id == "trunkkey") item.title = "Trunk key";
		else if (id == "sugarcubes") item.title = "Sugar cubes";
		else if (id == "hrkey") item.title = "Hotel room key";
		else if (id == "yunnibook") item.title = "Yunni book";
		else if (id == "tbird") item.title = "Thunderbird";
		else if (id == "tstone") item.title = "Turquoise stone";
		else if (id == "hankerchief") {
			const char *titles[] = {"Handkerchief", "Hotel chit", "Fur pelt", "Bottle of wine", "Box of cigars"};
			if (prop->deg >= 0 && prop->deg < 5) item.title = titles[prop->deg];
		}
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
bool inventoryActive(const Session &session) {
	if (!session.stageSystem.current || session.movies.active() || (session.puppetSystem.current && session.puppetSystem.current->visible)) return false;
	if (session.version == 1) return session.stageSystem.currentFlat().equalsIgnoreCase("avatar");
	return session.stageSystem.current->name.equalsIgnoreCase("inven1.stg") || session.stageSystem.current->name.equalsIgnoreCase("inven2.stg") || session.stageSystem.current->name.equalsIgnoreCase("inven.stg");
}
bool inventoryAction(Session &session, const InventoryResult &result) {
	if (!inventoryActive(session) || session.runtime.busy()) return false;
	if (result.action != kInventoryBack) {
		Common::Array<InventoryItem> items;
		if (!inventoryItems(session, items)) return false;
		bool available = false;
		for (const auto &item : items) if (item.name == result.item && (result.action != kInventoryExamine || item.examinable)) available = true;
		if (!available) return false;
		ScriptFrame event = session.runtime.eventFrame(result.item, "mousedown"); Values args; args.push_back(Value(0));
		session.propSystem.send(true, result.item, "mousedown", args, event, true);
		if (!session.runtime.error.empty() || !inventoryActive(session)) return false;
	}
	const Common::String button = result.action == kInventoryExamine ? session.version == 1 ? "info" : "examine" : "ok";
	const auto stage = session.stageSystem.current; const int32 flat = session.stageSystem.flat;
	if (flat < 0 || uint32(flat) >= stage->data.flats.size()) return false;
	for (const auto &region : stage->data.flats[flat].regions) if (region.name.equalsIgnoreCase(button)) {
		const int16 x = (region.rect.left + region.rect.right) / 2, y = (region.rect.top + region.rect.bottom) / 2;
		ScriptFrame event = session.runtime.eventFrame(button, "mousedown"); Values args; args.push_back(Value(int32(uint32(uint16(x)) << 16 | uint16(y))));
		session.stageSystem.button(session.stageSystem.currentFlat(), button, "mousedown", args, event);
		return session.runtime.error.empty();
	}
	return false;
}
bool inventoryOffer(Session &session, const Common::String &item, int16 x, int16 y) {
	const auto prop = session.propSystem.get(item); const auto held = session.runtime.globals.find("handitem");
	if (!prop || session.runtime.busy() || session.puppetSystem.current || session.movies.active() || held == session.runtime.globals.end() || held->_value.text() != item || prop->view != "large") return false;
	if (!prop->owner.text().equalsIgnoreCase(session.version == 1 ? "stranger" : "frank")) return false;
	ScriptFrame event = session.runtime.eventFrame(item, "mousedown"); Values args; args.push_back(Value(int32(uint32(uint16(x)) << 16 | uint16(y))));
	session.propSystem.send(true, item, "mousedown", args, event, true);
	return session.runtime.error.empty();
}
}
