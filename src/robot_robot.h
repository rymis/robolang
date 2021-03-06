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

enum _RobotState {
	ROBOT_IDLE,
	ROBOT_WALK,
	ROBOT_ROTATE_LEFT,
	ROBOT_ROTATE_RIGHT,
	ROBOT_CHECK,
	ROBOT_WALK_ON_PLACE /* Walk but don't move from place (when here are the wall in front of robot) */
};
typedef enum _RobotState RobotState;

enum _RobotDirection {
	ROBOT_UP,
	ROBOT_LEFT,
	ROBOT_DOWN,
	ROBOT_RIGHT
};
typedef enum _RobotDirection RobotDirection;

struct _RobotRobot {
	RobotSprite parent_instance;

	RobotRobotPrivate *priv;
};

struct _RobotRobotClass {
	RobotSpriteClass parent_class;
};

RobotRobot* robot_robot_new(void);
gboolean robot_robot_load_from_file(RobotRobot *self, SDL_Renderer *renderer, const char *name, GError **error);
gboolean robot_robot_from_xml_node(RobotRobot *self, SDL_Renderer *renderer, const gchar* name, RobotXml *xml, GError **error);

void robot_robot_set_state(RobotRobot *self, RobotState state);
RobotState robot_robot_get_state(RobotRobot *self);

void robot_robot_set_direction(RobotRobot *self, RobotDirection direction);
RobotDirection robot_robot_get_direction(RobotRobot *self);

G_END_DECLS

#endif /* ROBOT_ROBOT_H */
