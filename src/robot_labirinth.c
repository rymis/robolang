#include "robot_labirinth.h"

struct _RobotLabirinthPrivate {
	GArray *cells;
	guint wall_start;
	guint width, height;
	GPtrArray *sprites; /* TODO: it is not always optimal */
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotLabirinth, robot_labirinth, G_TYPE_OBJECT)

static void dispose(GObject *obj)
{
	RobotLabirinth *self = ROBOT_LABIRINTH(obj);
	g_ptr_array_unref(self->priv->sprites);
	self->priv->sprites = NULL;
}

static void finalize(GObject *obj)
{
	RobotLabirinth *self = ROBOT_LABIRINTH(obj);
	g_array_free(self->priv->cells, TRUE);
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
	self->priv->cells = g_array_new(sizeof(RobotLabirinthCell), FALSE, TRUE);
	self->priv->wall_start = 2;
	self->priv->sprites = g_ptr_array_new_with_free_func(g_object_unref);
}

RobotLabirinth* robot_labirinth_new(void)
{
	RobotLabirinth* self = g_object_new(ROBOT_TYPE_LABIRINTH, NULL);

	return self;
}

RobotLabirinth* robot_labirinth_new_full(guint width, guint height, guint wall_start)
{
	RobotLabirinth *self = robot_labirinth_new();
	robot_labirinth_set_size(self, width, height);
	robot_labirinth_set_wall_start(self, wall_start);

	return self;
}

void robot_labirinth_set_size(RobotLabirinth *self, guint width, guint height)
{
	guint x, y;

	GArray *n = g_array_new(sizeof(RobotLabirinthCell), FALSE, TRUE);
	g_array_set_size(n, width * height);

	for (y = 0; y < self->priv->height && y < height; y++) {
		for (x = 0; x < self->priv->width && x < width; x++) {
			g_array_index(n, RobotLabirinthCell, y * width + x) = robot_labirinth_get_cell(self, x, y);
		}
	}

	g_array_free(self->priv->cells, TRUE);
	self->priv->cells = n;
	self->priv->width = width;
	self->priv->height = height;

	return;
}

void robot_labirinth_set_wall_start(RobotLabirinth *self, guint wall_start)
{
	self->priv->wall_start = wall_start;
}

RobotLabirinthCell robot_labirinth_get_cell(RobotLabirinth *self, guint x, guint y)
{
	if (x >= self->priv->width || y >= self->priv->height)
		return 0;

	return g_array_index(self->priv->cells, RobotLabirinthCell, y * self->priv->width + x);
}

gboolean robot_labirinth_is_wall(RobotLabirinth *self, RobotLabirinthCell cell)
{
	return cell == 0 || cell >= self->priv->wall_start;
}

gboolean robot_labirinth_can_walk(RobotLabirinth *self, guint x, guint y)
{
	return !robot_labirinth_is_wall(self, robot_labirinth_get_cell(self, x, y));
}

void robot_labirinth_set_cell(RobotLabirinth *self, guint x, guint y, RobotLabirinthCell cell)
{
	if (x >= self->priv->width || y >= self->priv->height)
		return;

	g_array_index(self->priv->cells, RobotLabirinthCell, y * self->priv->width + x) = cell;
}

/* Super simple file format:
 * WALL: "abcd"
 * FLOOR: "xyz"
 * 'a': image.png
 * 'c': sprite.ini
 *
 * aaaabbabbbabab
 * ....
 */
gboolean robot_labirinth_load(RobotLabirinth *self, const char *filename, GError **error)
{

	return FALSE;
}

void robot_labirinth_set_sprite_for_cell(RobotLabirinth *self, guint cell, RobotSprite *sprite)
{
	if (cell >= self->priv->sprites->len) {
		g_ptr_array_set_size(self->priv->sprites, cell + 1);
	}

	if (g_ptr_array_index(self->priv->sprites, cell) != NULL)
		g_object_unref(g_ptr_array_index(self->priv->sprites, cell));
	g_ptr_array_index(self->priv->sprites, cell) = g_object_ref(sprite);
}

void robot_labirinth_render(RobotLabirinth *self, SDL_Renderer *renderer)
{
	/* TODO: render */
}

