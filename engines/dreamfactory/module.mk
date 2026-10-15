MODULE := engines/dreamfactory
MODULE_OBJS = archive.o display.o image.o audio.o enhancements.o sound.o crickets.o cursors.o cast.o actors.o motion.o balls.o checkers.o stars.o scheduler.o movie.o movie_host.o puppet.o puppets.o puppet_view.o journal.o routes.o shop.o props.o room.o room_v1.o rooms.o stage.o stage_view.o stages.o resources.o script.o runtime.o session.o savestate.o script_audit.o dreamfactory.o metaengine.o
MODULE_OBJS += inventory.o inventory_ui.o recovery.o recovery_ui.o heard.o dialogue.o leads.o navigation.o navigation_ui.o

ifeq ($(ENABLE_DREAMFACTORY), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

include $(srcdir)/rules.mk
DETECT_OBJS += $(MODULE)/detection.o
