/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_IMAGE_H
#define DREAMFACTORY_IMAGE_H
#include "dreamfactory/archive.h"
namespace DreamFactory {
struct Frame {
	uint16 width = 0, height = 0;
	// Empty when the current frame has no scenery depth plane.
	Bytes pixels, depth;
};
// Keeps the previous image for delta frames. Failure leaves it untouched.
bool decodeFrame(const Bytes &data, Frame &frame);
}
#endif
