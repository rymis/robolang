#include "robot_idrawable.h"

G_DEFINE_INTERFACE(RobotIDrawable, robot_idrawable, 0)

static void robot_idrawable_default_init(RobotIDrawableInterface *iface)
{
	/* Add properties and signals to interface here */
	iface->get_rect = NULL;
	iface->render = NULL;
}

gboolean robot_idrawable_get_rect(RobotIDrawable *self, SDL_Rect *rect)
{
	RobotIDrawableInterface *iface;

	g_return_val_if_fail(self != NULL && ROBOT_IS_IDRAWABLE(self) && rect != NULL, FALSE);
	iface = ROBOT_IDRAWABLE_GET_INTERFACE(self);

	g_return_val_if_fail(iface != NULL && iface->get_rect != NULL, FALSE);

	return iface->get_rect(self, rect);
}

void robot_idrawable_render(RobotIDrawable *self, SDL_Renderer *renderer)
{
	RobotIDrawableInterface *iface;

	g_return_if_fail(self != NULL && ROBOT_IS_IDRAWABLE(self) && renderer != NULL);
	iface = ROBOT_IDRAWABLE_GET_INTERFACE(self);

	g_return_if_fail(iface != NULL && iface->render != NULL);

	iface->render(self, renderer);
}

