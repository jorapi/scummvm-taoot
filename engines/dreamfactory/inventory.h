/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_INVENTORY_H
#define DREAMFACTORY_INVENTORY_H
#include "dreamfactory/archive.h"
namespace DreamFactory {
class Session;
struct InventoryItem {
	Common::String name, title, description;
	bool examinable = false;
};
bool inventoryItems(Session &session, Common::Array<InventoryItem> &items);
bool inventoryPicture(Session &session, const Common::String &name, Bytes &rgba, uint32 &width, uint32 &height);
enum InventoryAction { kInventoryBack, kInventoryExamine, kInventoryUse };
struct InventoryResult { InventoryAction action = kInventoryBack; Common::String item, title; };
bool inventoryActive(const Session &session);
bool inventoryAction(Session &session, const InventoryResult &result);
bool inventoryOffer(Session &session, const Common::String &item, int16 x, int16 y);
InventoryResult openInventory(Session &session);
}
#endif
