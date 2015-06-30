#include "robot_scene.h"

struct unit {
	RobotIDrawable *drawable;
	gint64 next_action;
	gpointer userdata;
	GDestroyNotify free_ud;
};
static void unit_free(gpointer unit);

struct _RobotScenePrivate {
	GList *units;
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotScene, robot_scene, G_TYPE_OBJECT)

static void dispose(GObject *obj)
{
	RobotScene *self = ROBOT_SCENE(obj);
	g_list_free_full(self->priv->units, unit_free);
	self->priv->units = NULL;
}

static void finalize(GObject *obj)
{
	RobotScene *self = ROBOT_SCENE(obj);
	self->priv = NULL;
}

static void robot_scene_class_init(RobotSceneClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->dispose = dispose;
	objcls->finalize = finalize;
}

static void robot_scene_init(RobotScene *self)
{
	self->priv = robot_scene_get_instance_private(self);
	self->priv->units = NULL;
}

RobotScene* robot_scene_new(void)
{
	RobotScene* self = g_object_new(ROBOT_TYPE_SCENE, NULL);

	return self;
}

RobotScene* robot_scene_new_full(GMainLoop *loop)
{
	RobotScene *self = robot_scene_new();

	robot_scene_register(self, loop);

	return self;
}

void robot_scene_add_drawable(RobotScene *self, RobotIDrawable *unit, guint64 action, gpointer userdata, GDestroyNotify free_userdata)
{
	struct unit *u;

	g_return_if_fail(self != NULL && unit != NULL);

	u= g_malloc(sizeof(struct unit));
	u->drawable = unit;
	u->next_action = action;
	u->userdata = userdata;
	u->free_ud = free_userdata;

	self->priv->units = g_list_append(self->priv->units, u);
}

void robot_scene_del_drawable(RobotScene *self, RobotIDrawable *unit)
{
	GList *p;
	struct unit *u = NULL;

	for (p = self->priv->units; p != NULL; p = g_list_next(p)) {
		u = p->data;
		if (u->drawable == unit)
			break;
	}

	if (!p || !u)
		return;

	self->priv->units = g_list_remove(self->priv->units, p);
	unit_free(u);
}

static void render(gpointer data, gpointer userdata)
{
	struct unit *unit = data;
	SDL_Renderer *renderer = userdata;

	g_return_if_fail(unit != NULL && renderer != NULL);

	robot_idrawable_render(unit->drawable, renderer);
}

void robot_scene_render(RobotScene *self, SDL_Renderer *renderer)
{
	g_list_foreach(self->priv->units, render, renderer);
}

static gboolean timeout_cb(gpointer ptr)
{
	RobotScene *self = ptr;
	gint64 now = g_get_monotonic_time();
	struct unit *u;
	GList *p;

	for (p = self->priv->units; p; p = g_list_next(p)) {
		u = p->data;
		if (u->next_action >= 0 && u->next_action > now) {
			u->next_action = robot_idrawable_action(u->drawable, now, u->userdata);
		}
	}

	return TRUE; /* Continue */
}

void robot_scene_register(RobotScene *self, GMainLoop *loop)
{
	GMainContext *ctx = g_main_loop_get_context(loop);

	g_return_if_fail(ctx != NULL);

	g_timeout_add(10, timeout_cb, ctx);
}

static void unit_free(gpointer unit)
{
	struct unit *u = unit;
	if (!u)
		return;

	g_clear_object(&u->drawable);
	if (u->userdata && u->free_ud)
		u->free_ud(u->userdata);
	u->userdata = NULL;
	u->free_ud = NULL;
	u->next_action = -1;

	g_free(u);
}


