#include "robot_labirinth.h"

struct _RobotLabirinthPrivate {
	/* TODO: add your privates here */

};

G_DEFINE_TYPE_WITH_PRIVATE(RobotLabirinth, robot_labirinth, G_TYPE_OBJECT)

static void dispose(GObject *obj)
{
	RobotLabirinth *self = ROBOT_LABIRINTH(obj);
	/* TODO: dispose code here */
}

static void finalize(GObject *obj)
{
	RobotLabirinth *self = ROBOT_LABIRINTH(obj);
	/* TODO: finalize code here */
	self->priv = NULL;
}

static void robot_labirinth_class_init(RobotLabirinthClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->dispose = dispose;
	objcls->finalize = finalize;
}

static void robot_labirinth_init(RobotLabirinth *self)
{
	self->priv = robot_labirinth_get_instance_private(self);
}

RobotLabirinth* robot_labirinth_new(void)
{
	RobotLabirinth* self = g_object_new(ROBOT_TYPE_LABIRINTH, NULL);

	return self;
}

