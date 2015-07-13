#include "robot_robot.h"
#include "robot_vm.h"

static gint64 robot_robot_action(RobotSprite *self, gint64 now, gpointer userptr);
static void set_mode(RobotRobot *self);

struct mode_info {
	const char *name;
	GQuark mode;
	int dx, dy;
};

static struct mode_info mode_list[] = {
	{ "walk_r", 0, 5, 0 },
	{ "walk_r", 0, 5, 0 },
	{ "check_r", 0, 0, 0 },
	{ "rotate_ru", 0, 0, 0 },
	{ "walk_u", 0, 0, -5 },
	{ "walk_u", 0, 0, -5 },
	{ "check_u", 0, 0, 0 },
	{ "rotate_ul", 0, 0, 0 },
	{ "walk_l", 0, -5, 0 },
	{ "walk_l", 0, -5, 0 },
	{ "check_l", 0, 0, 0 },
	{ "rotate_ld", 0, 0, 0 },
	{ "walk_d", 0, 0, 5 },
	{ "walk_d", 0, 0, 5 },
	{ "check_d", 0, 0, 0 },
	{ "rotate_dr", 0, 0, 0 },

	{ "rotate_rd", 0, 0, 0 },
	{ "rotate_dl", 0, 0, 0 },
	{ "rotate_lu", 0, 0, 0 },
	{ "rotate_ur", 0, 0, 0 },
};
static guint mode_cnt = sizeof(mode_list) / sizeof(mode_list[0]);

static void init_quarks(void)
{
	guint i;

	if (!mode_list[0].mode)
		for (i = 0; i < mode_cnt; i++)
			mode_list[i].mode = g_quark_from_string(mode_list[i].name);
}

struct _RobotRobotPrivate {
	guint mode;
	uint frame;
	guint frames_count;
	gint64 pause;
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotRobot, robot_robot, ROBOT_TYPE_SPRITE)

static void dispose(GObject *obj)
{
	RobotRobot *self = ROBOT_ROBOT(obj);
	/* TODO: dispose code here */
}

static void finalize(GObject *obj)
{
	RobotRobot *self = ROBOT_ROBOT(obj);
	/* TODO: finalize code here */
	self->priv = NULL;
}

static void robot_robot_class_init(RobotRobotClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->dispose = dispose;
	objcls->finalize = finalize;
}

static void robot_robot_init(RobotRobot *self)
{
	self->priv = robot_robot_get_instance_private(self);
}

RobotRobot* robot_robot_new(void)
{
	RobotRobot* self = g_object_new(ROBOT_TYPE_ROBOT, NULL);

	init_quarks();

	return self;
}

gboolean robot_robot_from_xml_node(RobotRobot *self, SDL_Renderer *renderer, const gchar* name, RobotXml *xml, GError **error)
{
	guint i;

	if (!robot_sprite_from_xml_node(ROBOT_SPRITE(self), renderer, name, xml, error)) {
		return FALSE;
	}

	for (i = 0; i < mode_cnt; i++) {
		if (!robot_sprite_has_mode(ROBOT_SPRITE(self), mode_list[i].mode)) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Not all modes in sprite: %s is absent", mode_list[i].name);
			return FALSE;
		}
	}

	self->priv->mode = 0;
	set_mode(self);

	robot_sprite_set_action(ROBOT_SPRITE(self), robot_robot_action);

	return TRUE;
}

gboolean robot_robot_load_from_file(RobotRobot *self, SDL_Renderer *renderer, const char *name, GError **error)
{
	guint i;

	if (!robot_sprite_load_from_file(ROBOT_SPRITE(self), renderer, name, error)) {
		return FALSE;
	}

	for (i = 0; i < mode_cnt; i++) {
		if (!robot_sprite_has_mode(ROBOT_SPRITE(self), mode_list[i].mode)) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Not all modes in sprite: %s is absent", mode_list[i].name);
			return FALSE;
		}
	}

	self->priv->mode = 0;
	set_mode(self);

	robot_sprite_set_action(ROBOT_SPRITE(self), robot_robot_action);

	return TRUE;
}

static gint64 robot_robot_action(RobotSprite *self_, gint64 now, gpointer userptr)
{
	RobotRobot *self = ROBOT_ROBOT(self_);

	if (self->priv->frame >= self->priv->frames_count) {
		self->priv->mode = (self->priv->mode + 1) % mode_cnt;
		set_mode(self);
	}

	robot_sprite_move(self_, mode_list[self->priv->mode].dx, mode_list[self->priv->mode].dy);
	robot_sprite_set_frame(self_, self->priv->frame);
	self->priv->frame++;

	return now + self->priv->pause;
}

static void set_mode(RobotRobot *self)
{
	self->priv->frames_count = robot_sprite_get_frames_count(ROBOT_SPRITE(self));
	self->priv->frame = 0;
	self->priv->pause = 1000000 / self->priv->frames_count;
	printf("MODE: %d, cnd: %u\n", self->priv->mode, self->priv->frames_count);

	robot_sprite_set_mode(ROBOT_SPRITE(self), mode_list[self->priv->mode].mode);
}

