/* SPDX-License-Identifier: GPL-3.0-or-later
 * Fixed-point projection follows dreamREfactory runtime/geometry.ts.
 */
#ifndef DREAMFACTORY_GEOMETRY_H
#define DREAMFACTORY_GEOMETRY_H
#include "dreamfactory/image.h"
namespace DreamFactory {
struct WorldCamera {
	int32 x = 0, y = 0, z = 0, degree = 0;
	double focal = 256;
	uint32 width = 512, height = 264;
};
struct ProjectedPoint { double x = 0, y = 0; int32 depth = 0; };
struct Occlusion {
	const Frame *frame = nullptr;
	double scale = 1;
	int32 bias = 0;
	bool hides(int32 x, int32 y, double depth) const {
		if (!frame || frame->depth.empty() || scale <= 0 || x < 0 || y < 0 || x >= frame->width || y >= frame->height) return false;
		const double level = MAX<double>(0, floor(depth / MAX<double>(1, scale)));
		return frame->depth[y * frame->width + x] < level;
	}
};
inline int32 worldBearing(double x, double y) { return int32(floor(atan2(y, x) * 128 / M_PI + 0.5)) & 255; }
// Script calcdeg uses the original 9-bit integer ratio table, rather than
// the renderer's nearest-angle bearing. TI.EXE 0x444b40 / DF.EXE 0x411d20.
inline int32 packedBearing(int32 from, int32 to) {
	const int32 x = int16(int16(uint32(to) >> 16) - int16(uint32(from) >> 16));
	const int32 y = int16(int16(to) - int16(from));
	if (!y) return x < 0 ? 128 : 0;
	if (!x) return y < 0 ? 192 : 64;
	const int32 ax = ABS(x), ay = ABS(y);
	int32 angle = 32;
	if (ax != ay) {
		const int32 ratio = MIN(ax, ay) * 512 / MAX(ax, ay);
		angle = int32(floor(atan(double(ratio) / 512) * 128 / M_PI));
		if (ay > ax) angle = 64 - angle;
	}
	if (x < 0) angle = 128 - angle;
	if (y < 0) angle = -angle;
	return angle & 255;
}
inline bool projectPoint(const WorldCamera &camera, int32 x, int32 y, int32 z, ProjectedPoint &out) {
	const double angle = (camera.degree & 255) * M_PI / 128;
	const int32 sine = int32(floor(sin(angle) * 16384 + 0.5)), cosine = int32(floor(cos(angle) * 16384 + 0.5));
	const int64 dx = int64(x) - camera.x, dy = int64(y) - camera.y, dz = int64(z) - camera.z;
	auto fixed = [](int64 value) -> int32 { return int32(uint32(value + (value < 0 ? 0x3fff : 0))) >> 14; };
	out.depth = fixed(dy * sine + dx * cosine);
	if (out.depth <= 0) return false;
	const int32 lateral = fixed(dy * cosine - dx * sine);
	out.x = double(camera.width) / 2 + trunc(lateral * camera.focal / out.depth);
	out.y = double(camera.height) / 2 - trunc(dz * camera.focal / out.depth);
	return true;
}
}
#endif
