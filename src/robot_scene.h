#ifndef _ROBOT_SCENE_H_
#define _ROBOT_SCENE_H_ 1

#include <glib-object.h>
#include "robot_idrawable.h"

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_SCENE                   (robot_scene_get_type())
#define ROBOT_SCENE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_SCENE, RobotScene))
#define ROBOT_IS_SCENE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_SCENE))
#define ROBOT_SCENE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_SCENE, RobotSceneClass))
#define ROBOT_IS_SCENE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_SCENE))
#define ROBOT_SCENE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_SCENE, RobotSceneClass))

/* get_type prototype: */
GType robot_scene_get_type(void);

/* Structures definitions: */
typedef struct _RobotScene RobotScene;
typedef struct _RobotSceneClass RobotSceneClass;
typedef struct _RobotScenePrivate RobotScenePrivate;

struct _RobotScene {
	GObject parent_instance;

	RobotScenePrivate *priv;
};

struct _RobotSceneClass {
	GObjectClass parent_class;
};

RobotScene* robot_scene_new(void);
RobotScene* robot_scene_new_full(GMainLoop *loop);
void robot_scene_add_drawable(RobotScene *self, RobotIDrawable *unit, guint64 action, gpointer userdata, GDestroyNotify free_userdata);
void robot_scene_del_drawable(RobotScene *self, RobotIDrawable *unit);
void robot_scene_render(RobotScene *self, SDL_Renderer *renderer);
void robot_scene_register(RobotScene *self, GMainLoop *loop);

G_END_DECLS

#endif /* ROBOT_SCENE_H */
