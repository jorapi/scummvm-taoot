/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_CONTROLS_H
#define DREAMFACTORY_CONTROLS_H
#include "common/events.h"
#include "dreamfactory/archive.h"
namespace DreamFactory {
enum ControlAction { kOpenJournal = 2,
					 kPadUp = 100,
					 kPadDown,
					 kPadLeft,
					 kPadRight,
					 kPadPrevious,
					 kPadNext,
					 kPadInteract,
					 kPadRepeat,
					 kPadClick,
					 kJournalReplay = 300, kJournalStar, kJournalViewPrevious, kJournalViewNext, kJournalRead, kJournalClose, kJournalUp, kJournalDown, kJournalLeft, kJournalRight, kJournalZoom, kJournalCurrent };
inline Common::KeyCode controlKey(uint32 action) {
	const Common::KeyCode keys[] = {Common::KEYCODE_UP, Common::KEYCODE_DOWN, Common::KEYCODE_LEFT, Common::KEYCODE_RIGHT};
	return action >= kPadUp && action <= kPadRight ? keys[action - kPadUp] : Common::KEYCODE_RETURN;
}
// Native 512x384 coordinates. The opt-in pad occupies the lower left corner.
inline int32 touchDirection(int32 x, int32 y) {
	const int32 bx[] = {48, 48, 8, 88}, by[] = {140, 220, 180, 180};
	for (uint32 i = 0; i < 4; ++i)
		if (x >= bx[i] && x < bx[i] + 36 && y >= by[i] && y < by[i] + 36)
			return kPadUp + i;
	return -1;
}
inline void drawTouchPad(Bytes &rgba) {
	if (rgba.size() != 512 * 384 * 4)
		return;
	for (int32 y = 140; y < 256; ++y)
		for (int32 x = 8; x < 124; ++x) {
			const int32 action = touchDirection(x, y);
			if (action < 0)
				continue;
			const int32 i = action - kPadUp, bx = i == 2 ? 8 : i == 3 ? 88
																	  : 48,
						by = i == 0 ? 140 : i == 1 ? 220
												   : 180;
			const int32 dx = x - bx, dy = y - by;
			byte *p = rgba.data() + (y * 512 + x) * 4;
			const bool border = dx == 0 || dy == 0 || dx == 35 || dy == 35;
			int32 a = dx - 18, b = dy - 18;
			if (i == 1)
				b = -b;
			if (i == 2) {
				a = dy - 18;
				b = dx - 18;
			}
			if (i == 3) {
				a = dy - 18;
				b = 18 - dx;
			}
			const bool arrow = b >= -8 && b <= 7 && ABS(a) <= (b + 8) / 2;
			for (uint32 c = 0; c < 3; ++c)
				p[c] = border || arrow ? 235 : byte(p[c] / 3);
			p[3] = 255;
		}
}
} // namespace DreamFactory
#endif
