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
	{ "walk_r", 0, 1, 0 },
	{ "walk_u", 0, 0, -1 },
	{ "walk_l", 0, -1, 0 },
	{ "walk_d", 0, 0, 1 },

	{ "check_d", 0, 0, 0 },
	{ "check_l", 0, 0, 0 },
	{ "check_u", 0, 0, 0 },
	{ "check_r", 0, 0, 0 },

	{ "rotate_rd", 0, 0, 0 },
	{ "rotate_dr", 0, 0, 0 },
	{ "rotate_dl", 0, 0, 0 },
	{ "rotate_ld", 0, 0, 0 },
	{ "rotate_lu", 0, 0, 0 },
	{ "rotate_ul", 0, 0, 0 },
	{ "rotate_ru", 0, 0, 0 },
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
	gint frame;
	gint frames_count;
	gint64 pause;

	RobotState state;
	RobotDirection direction;
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

	init_quarks();
}

static void robot_robot_init(RobotRobot *self)
{
	self->priv = robot_robot_get_instance_private(self);
	self->priv->direction = ROBOT_RIGHT;
	self->priv->state = ROBOT_IDLE;
}

RobotRobot* robot_robot_new(void)
{
	RobotRobot* self = g_object_new(ROBOT_TYPE_ROBOT, NULL);

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

static void set_mode(RobotRobot *self)
{
	robot_sprite_set_mode(ROBOT_SPRITE(self), mode_list[self->priv->mode].mode);

	self->priv->frames_count = robot_sprite_get_frames_count(ROBOT_SPRITE(self));
	self->priv->frame = 0;
	self->priv->pause = 1000000 / self->priv->frames_count;

	printf("MODE: %d, direction: %d, cnt: %u\n", self->priv->mode, self->priv->direction, self->priv->frames_count);
}

static void set_mode_s(RobotRobot *self, const gchar* mode)
{
	guint i;

	for (i = 0; i < mode_cnt; i++) {
		if (!strcmp(mode_list[i].name, mode)) {
			self->priv->mode = i;
			set_mode(self);

			return;
		}
	}
}

static char direction2char(RobotDirection d)
{
	switch (d) {
		case ROBOT_UP: return 'u';
		case ROBOT_LEFT: return 'l';
		case ROBOT_DOWN: return 'd';
		case ROBOT_RIGHT: return 'r';
	}

	return ' ';
}

static RobotDirection next_direction(RobotDirection d)
{
	switch (d) {
		case ROBOT_UP: return ROBOT_LEFT;
		case ROBOT_LEFT: return ROBOT_DOWN;
		case ROBOT_DOWN: return ROBOT_RIGHT;
		case ROBOT_RIGHT: return ROBOT_UP;
	}

	return ROBOT_UP;
}

static RobotDirection prev_direction(RobotDirection d)
{
	switch (d) {
		case ROBOT_UP: return ROBOT_RIGHT;
		case ROBOT_LEFT: return ROBOT_UP;
		case ROBOT_DOWN: return ROBOT_LEFT;
		case ROBOT_RIGHT: return ROBOT_DOWN;
	}

	return ROBOT_UP;
}

void robot_robot_set_state(RobotRobot *self, RobotState state)
{
	char mode[32];

	self->priv->state = state;
	switch (state) {
		case ROBOT_IDLE:
			break;
		case ROBOT_WALK:
			sprintf(mode, "walk_%c", direction2char(self->priv->direction));
			break;
		case ROBOT_ROTATE_LEFT:
			sprintf(mode, "rotate_%c%c", direction2char(self->priv->direction), direction2char(next_direction(self->priv->direction)));
			break;
		case ROBOT_ROTATE_RIGHT:
			sprintf(mode, "rotate_%c%c", direction2char(self->priv->direction), direction2char(prev_direction(self->priv->direction)));
			break;
		case ROBOT_CHECK:
			sprintf(mode, "check_%c", direction2char(self->priv->direction));
			break;
		default:
			return;
	}
printf("MODE: %s\n", mode);
	set_mode_s(self, mode);
}

RobotState robot_robot_get_state(RobotRobot *self)
{
	return self->priv->state;
}

void robot_robot_set_direction(RobotRobot *self, RobotDirection direction)
{
	self->priv->direction = direction;
}

RobotDirection robot_robot_get_direction(RobotRobot *self)
{
	return self->priv->direction;
}

static gint64 robot_robot_action(RobotSprite *self_, gint64 now, gpointer userptr)
{
	RobotRobot *self = ROBOT_ROBOT(self_);
	int dx, dy;

	if (self->priv->state == ROBOT_IDLE) {
		return now + 5000;
	}

	if (self->priv->frame >= self->priv->frames_count) {
		if (self->priv->state == ROBOT_ROTATE_LEFT) {
			self->priv->direction = next_direction(self->priv->direction);
		} else if (self->priv->state == ROBOT_ROTATE_RIGHT) {
			self->priv->direction = prev_direction(self->priv->direction);
		}

		self->priv->state = ROBOT_IDLE;
	}

	dx = dy = 0;
	if (mode_list[self->priv->mode].dx || mode_list[self->priv->mode].dy) {
		dx = (mode_list[self->priv->mode].dx * 1000) / self->priv->frames_count;
		dy = (mode_list[self->priv->mode].dy * 1000) / self->priv->frames_count;
		robot_sprite_move(self_, dx, dy);
	}
	robot_sprite_set_frame(self_, self->priv->frame);
	self->priv->frame++;

	return now + self->priv->pause;
}


