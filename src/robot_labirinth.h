#ifndef _ROBOT_LABIRINTH_H_
#define _ROBOT_LABIRINTH_H_ 1

#include <glib-object.h>

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_LABIRINTH                   (robot_labirinth_get_type())
#define ROBOT_LABIRINTH(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_LABIRINTH, RobotLabirinth))
#define ROBOT_IS_LABIRINTH(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_LABIRINTH))
#define ROBOT_LABIRINTH_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_LABIRINTH, RobotLabirinthClass))
#define ROBOT_IS_LABIRINTH_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_LABIRINTH))
#define ROBOT_LABIRINTH_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_LABIRINTH, RobotLabirinthClass))

/* get_type prototype: */
GType robot_labirinth_get_type(void);

/* Structures definitions: */
typedef struct _RobotLabirinth RobotLabirinth;
typedef struct _RobotLabirinthClass RobotLabirinthClass;
typedef struct _RobotLabirinthPrivate RobotLabirinthPrivate;

struct _RobotLabirinth {
	GObject parent_instance;

	RobotLabirinthPrivate *priv;
};

struct _RobotLabirinthClass {
	GObjectClass parent_class;
};

RobotLabirinth* robot_labirinth_new(void);

G_END_DECLS

#endif /* ROBOT_LABIRINTH_H */
