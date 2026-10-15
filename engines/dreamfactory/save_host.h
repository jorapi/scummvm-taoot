/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SAVE_HOST_H
#define DREAMFACTORY_SAVE_HOST_H
namespace DreamFactory {
class SaveHost {
public:
	virtual ~SaveHost() {}
	virtual bool saveGame() = 0;
	virtual bool loadGame() = 0;
};
}
#endif
