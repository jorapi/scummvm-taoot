/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_CHECKERS_H
#define DREAMFACTORY_CHECKERS_H
#include "common/str.h"
namespace Common { class RandomSource; }
namespace DreamFactory {
// Portable replacement for Dust's checkmove plug-in. No native DLL is loaded.
Common::String checkersMove(const Common::String &board, int32 depth, int32 side, Common::RandomSource *random = nullptr);
bool checkersSelfTest();
}
#endif
