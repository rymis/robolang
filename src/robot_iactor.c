#include "robot_iactor.h"

G_DEFINE_INTERFACE(RobotIActor, robot_iactor, ROBOT_TYPE_IDRAWABLE)

static void robot_iactor_default_init(RobotIActorInterface *iface)
{
	iface->action = NULL;
}

gint64 robot_iactor_action(RobotIActor *self, gint64 curtime, void *userdata)
{
	RobotIActorInterface *iface = NULL;

	g_return_val_if_fail(self != NULL && ROBOT_IS_IACTOR(self), -1);

	iface = ROBOT_IACTOR_GET_INTERFACE(self);

	g_return_val_if_fail(iface != NULL && iface->action != NULL, -1);

	return iface->action(self, curtime, userdata);
}

