#ifndef ROBOT_IACTOR_H
#define ROBOT_IACTOR_H 1

#include "robot_idrawable.h"

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_IACTOR                   (robot_iactor_get_type())
#define ROBOT_IACTOR(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_IACTOR, RobotIActor))
#define ROBOT_IS_IACTOR(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_IACTOR))
#define ROBOT_IACTOR_GET_INTERFACE(inst)    (G_TYPE_INSTANCE_GET_INTERFACE ((inst),  ROBOT_TYPE_IACTOR, RobotIActorInterface))

/* get_type prototype: */
GType robot_iactor_get_type(void);

/* Structures definitions: */
typedef struct _RobotIActor RobotIActor;
typedef struct _RobotIActorInterface RobotIActorInterface;

struct _RobotIActorInterface {
	RobotIDrawableInterface parent_iface;
	/* Do something. If you need to set new action you must return time to call. If not return -1 */
	gint64 (*action)(RobotIActor *self, gint64 curtime, void *userdata);
};

gint64 robot_iactor_action(RobotIActor *self, gint64 curtime, void *userdata);

G_END_DECLS

#endif /* ROBOT_IACTOR_H */
