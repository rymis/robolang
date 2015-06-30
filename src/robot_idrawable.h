#ifndef ROBOT_IDRAWABLE_H
#define ROBOT_IDRAWABLE_H 1

#include <glib-object.h>
#include <SDL.h>

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_IDRAWABLE                   (robot_idrawable_get_type())
#define ROBOT_IDRAWABLE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_IDRAWABLE, RobotIDrawable))
#define ROBOT_IS_IDRAWABLE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_IDRAWABLE))
#define ROBOT_IDRAWABLE_GET_INTERFACE(inst)    (G_TYPE_INSTANCE_GET_INTERFACE ((inst),  ROBOT_TYPE_IDRAWABLE, RobotIDrawableInterface))

/* get_type prototype: */
GType robot_idrawable_get_type(void);

/* Structures definitions: */
typedef struct _RobotIDrawable RobotIDrawable;
typedef struct _RobotIDrawableInterface RobotIDrawableInterface;

struct _RobotIDrawableInterface {
	GTypeInterface parent_iface;

	gboolean (*get_rect)(RobotIDrawable *self, SDL_Rect *rect);
	void (*render)(RobotIDrawable *self, SDL_Renderer *renderer);
	gint64 (*action)(RobotIDrawable *self, gint64 curtime, void *userdata);
};

gboolean robot_idrawable_get_rect(RobotIDrawable *self, SDL_Rect *rect);
void robot_idrawable_render(RobotIDrawable *self, SDL_Renderer *renderer);
gint64 robot_idrawable_action(RobotIDrawable *self, gint64 curtime, void *userdata);

G_END_DECLS

#endif /* ROBOT_IDRAWABLE_H */
