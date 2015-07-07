#ifndef _ROBOT_ROBOT_H_
#define _ROBOT_ROBOT_H_ 1

#include <glib-object.h>
#include "robot_sprite.h"

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_ROBOT                   (robot_robot_get_type())
#define ROBOT_ROBOT(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_ROBOT, RobotRobot))
#define ROBOT_IS_ROBOT(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_ROBOT))
#define ROBOT_ROBOT_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_ROBOT, RobotRobotClass))
#define ROBOT_IS_ROBOT_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_ROBOT))
#define ROBOT_ROBOT_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_ROBOT, RobotRobotClass))

/* get_type prototype: */
GType robot_robot_get_type(void);

/* Structures definitions: */
typedef struct _RobotRobot RobotRobot;
typedef struct _RobotRobotClass RobotRobotClass;
typedef struct _RobotRobotPrivate RobotRobotPrivate;

struct _RobotRobot {
	RobotSprite parent_instance;

	RobotRobotPrivate *priv;
};

struct _RobotRobotClass {
	RobotSpriteClass parent_class;
};

RobotRobot* robot_robot_new(void);
gboolean robot_robot_load_from_file(RobotRobot *self, SDL_Renderer *renderer, const char *name, GError **error);

G_END_DECLS

#endif /* ROBOT_ROBOT_H */
