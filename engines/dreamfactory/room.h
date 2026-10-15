/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_ROOM_H
#define DREAMFACTORY_ROOM_H
#include "dreamfactory/archive.h"
#include "dreamfactory/stars.h"
#include "dreamfactory/image.h"
#include "common/rect.h"
namespace DreamFactory {
// World coordinates use x/y for the ground plane and z for height.
struct CameraPose { int32 x = 0, y = 0, z = 0, degree = 0; };
struct MotionFrame { uint32 image = 0; int32 view = -1; int32 motion = 0; CameraPose camera; };
typedef Common::Array<MotionFrame> Sequence;
struct Painting { Common::String name; uint32 script = 0; int16 facing = 0; Common::Rect rect; };
struct View { Common::String name; int32 id; uint32 still = 0; int16 facing = 0; double cameraHeight = 0; Common::Array<Painting> paintings; };
struct Scene { Common::String name; Common::Array<View> views; Sequence turns[2]; uint32 script = 0, viewTable = 0; int32 x = 0, y = 0, z = 0; bool build = false; };
struct Road { int32 from, to; Sequence walks[2]; uint32 destinations[2] = {}; };
struct Move { int32 from, to; int direction; Sequence frames; };
class Room {
public:
	bool load(const Bytes &data);
	bool stand(uint32 scene, uint32 view, Frame &frame, CameraPose *camera = nullptr) const;
	bool roadArrival(const Road &road, uint32 direction, int32 &scene, int32 &view) const;
	bool roadSequence(const Road &road, uint32 direction, Sequence &frames, int32 &scene, int32 &view) const;
	Archive archive;
	StarTable stars;
	Common::Array<Scene> scenes;
	Common::Array<Road> roads;
	Common::Array<Move> moves;
	uint32 version = 0;
	uint32 mainScript = 0;
	uint16 width = 512, height = 320;
	int16 depthLevels = 0, depthFar = 0, cameraSetback = 0, eyeHeight = 0;
	Common::String name, defaultScene, defaultView;
	byte palette[768];
private:
	bool sequence(uint32 index, Sequence &out);
	bool loadV1();
};
}
#endif
