/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DREAMFACTORY_SCRIPT_AUDIT_H
#define DREAMFACTORY_SCRIPT_AUDIT_H
#include "common/fs.h"
class OSystem;
namespace DreamFactory {
bool auditScripts(const Common::FSNode &root, const Common::FSNode &fixtures);
bool auditStageSession(const Common::FSNode &root, OSystem *system);
bool auditStars(const Common::FSNode &root);
bool auditStages(const Common::FSNode &root);
bool auditResources(const Common::FSNode &root, const Common::FSNode &fixtures);
bool auditAudio(const Common::FSNode &root);
bool auditCasts(const Common::FSNode &root);
bool auditMovies(const Common::FSNode &root);
bool auditShops(const Common::FSNode &root);
bool auditProps(const Common::FSNode &fixtures);
bool auditMoviePlayback(const Common::FSNode &fixtures, OSystem *system, bool retail = false);
bool auditActors(const Common::FSNode &root, OSystem *system);
bool auditSound(const Common::FSNode &root, const Common::FSNode &fixture, OSystem *system);
bool auditCrickets(const Common::FSNode &root, OSystem *system, bool retail = false, bool demo = false);
bool auditRooms(const Common::FSNode &root);
bool auditCursors(const Common::FSNode &root, const Common::FSNode &fixture, OSystem *backend);
bool auditScreens(const Common::FSNode &fixture, OSystem *backend);
bool auditPuppets(const Common::FSNode &root);
bool auditPuppetSession(const Common::FSNode &root, OSystem *backend);
bool auditRoomSession(const Common::FSNode &root, OSystem *system);
}
#endif
