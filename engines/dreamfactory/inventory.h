/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_INVENTORY_H
#define DREAMFACTORY_INVENTORY_H
#include "dreamfactory/archive.h"
namespace DreamFactory {
class Session;
struct InventoryItem {
	Common::String name, title;
	bool examinable = false;
};
bool inventoryItems(Session &session, Common::Array<InventoryItem> &items);
bool inventoryPicture(Session &session, const Common::String &name, Bytes &rgba, uint32 &width, uint32 &height);
}
#endif
