#include "robot_labirinth.h"
#include "robot_vm.h"
#include "robot_idrawable.h"
#include "robot_robot.h"
#include <SDL.h>

static void robot_idrawable_init(RobotIDrawableInterface *iface);
enum sprite_type {
	sprite_way, sprite_wall, sprite_exit
};

struct sprite {
	enum sprite_type type;
	RobotSprite *sprite;
	gint64 next_update;
	char id; /* For load only */
};

static void sprite_free(gpointer p)
{
	struct sprite *s = p;

	if (!s)
		return;
	g_clear_object(&s->sprite);
	g_free(s);
}

static struct sprite* sprite_new(char c, enum sprite_type type)
{
	struct sprite *res = g_malloc0(sizeof(struct sprite));

	res->type = type;
	res->id = c;
	res->sprite = robot_sprite_new();
	res->next_update = 0;

	return res;
}

struct _RobotLabirinthPrivate {
	GArray *cells;
	guint width, height;
	GPtrArray *sprites;
	RobotRobot *robot;
	gint64 robot_update;
	guint robot_x, robot_y;
	guint next_x, next_y;
	SDL_Rect rect;
	gboolean check_result;
};

G_DEFINE_TYPE_WITH_CODE(RobotLabirinth, robot_labirinth, G_TYPE_OBJECT,
		G_ADD_PRIVATE(RobotLabirinth)
		G_IMPLEMENT_INTERFACE(ROBOT_TYPE_IDRAWABLE, robot_idrawable_init)
)

static void dispose(GObject *obj)
{
	RobotLabirinth *self = ROBOT_LABIRINTH(obj);
	g_ptr_array_unref(self->priv->sprites);
	self->priv->sprites = NULL;
	g_clear_object(&self->priv->robot);
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
	self->priv->cells = g_array_new(FALSE, TRUE, sizeof(RobotLabirinthCell));
	self->priv->sprites = g_ptr_array_new_with_free_func(sprite_free);
	self->priv->robot = NULL;
	self->priv->rect.x = 0;
	self->priv->rect.y = 0;
	self->priv->rect.w = 640;
	self->priv->rect.h = 480;
	self->priv->robot_update = 0;
}

RobotLabirinth* robot_labirinth_new(void)
{
	RobotLabirinth* self = g_object_new(ROBOT_TYPE_LABIRINTH, NULL);

	return self;
}

void robot_labirinth_set_size(RobotLabirinth *self, guint width, guint height)
{
	guint x, y;

	GArray *n = g_array_new(FALSE, TRUE, sizeof(RobotLabirinthCell));
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

RobotLabirinthCell robot_labirinth_get_cell(RobotLabirinth *self, guint x, guint y)
{
	if (x >= self->priv->width || y >= self->priv->height)
		return 0;

	return g_array_index(self->priv->cells, RobotLabirinthCell, y * self->priv->width + x);
}

gboolean robot_labirinth_is_wall(RobotLabirinth *self, RobotLabirinthCell cell)
{
	struct sprite *s;

	if (cell >= self->priv->sprites->len)
		return TRUE;

	s = g_ptr_array_index(self->priv->sprites, cell);
	return s->type == sprite_wall;
}

gboolean robot_labirinth_is_exit(RobotLabirinth *self, RobotLabirinthCell cell)
{
	struct sprite *s;

	if (cell >= self->priv->sprites->len)
		return FALSE;

	s = g_ptr_array_index(self->priv->sprites, cell);
	return s->type == sprite_exit;
}

gboolean robot_labirinth_can_walk(RobotLabirinth *self, guint x, guint y)
{
	return !robot_labirinth_is_wall(self, robot_labirinth_get_cell(self, x, y));
}

void robot_labirinth_set_cell(RobotLabirinth *self, guint x, guint y, RobotLabirinthCell cell)
{
	guint idx = y * self->priv->width + x;
	RobotLabirinthCell *p;

	g_return_if_fail(idx < self->priv->cells->len);

	p = &g_array_index(self->priv->cells, RobotLabirinthCell, idx);
	*p = cell;
}

static void set_cell(RobotLabirinth *self, guint x, guint y, char c)
{
	guint i;
	struct sprite *s;
	gint first = -1;

	for (i = 0; i < self->priv->sprites->len; i++) {
		s = g_ptr_array_index(self->priv->sprites, i);
		if (s->id == c) {
			robot_labirinth_set_cell(self, x, y, i);
			return;
		}

		if (first < 0) {
			if (s->type == sprite_wall)
				first = i;
		}
	}

	if (first >= 0)
		robot_labirinth_set_cell(self, x, y, first);
}

/* Load from XML format.
 * <labirinth x="1", y="1">
 * 	<sprites>
 * 		<sprite name="road" char=" " type="way">
 * 			<mode name="default">
 * 				<frame img="road.png"/>
 * 			</mode>
 * 		</sprite>
 * 		<sprite name="wall" char="#" type="wall">
 * 			<mode name="default">
 * 				<frame img="wall.png"/>
 * 			</mode>
 * 		</sprite>
 * 		<sprite name="exit" char="@" type="exit">
 * 			<mode name="default">
 * 				<frame img="exit.png" />
 * 			</mode>
 * 		</sprite>
 * 	</sprites>
 * 	<robot>
 *		<sprite>
 *			...
 *		</sprite>
 * 	</robot>
 * 	<table>
 * 		<row value="##########"/>
 * 		<row value="#        #"/>
 * 		<row value="# ### # ##"/>
 * 		<row value="### ### ##"/>
 * 		<row value="#       ##"/>
 * 		<row value="# ########"/>
 * 		<row value="# #      #"/>
 * 		<row value="# ## # # #"/>
 * 		<row value="#    # # #"/>
 * 		<row value="########@#"/>
 * 	</table>
 * </labirinth>
 */
gboolean robot_labirinth_parse_xml(RobotLabirinth *self, RobotXml *labirinth, const gchar* filename, SDL_Renderer *renderer, GError **error)
{
	guint width, height, pos_x, pos_y;
	RobotXml *sprites = NULL;
	RobotXml *table = NULL;
	RobotXml *robot = NULL;
	RobotXml *node = NULL;
	guint i, j, cnt;
	guint l;
	struct sprite *nsprite;
	enum sprite_type type;
	guint has_types = 0;
	const gchar* c;
	GPtrArray* rows;
	const char* row;

	if (!robot_xml_is_name(labirinth, "labirinth")) {
		g_set_error(error, ROBOT_ERROR, -1, "Invalid XML: root tag is not <labirinth>");
		return FALSE;
	}

	pos_x = robot_xml_get_attribute_long(labirinth, "x", 1);
	pos_y = robot_xml_get_attribute_long(labirinth, "y", 1);

	sprites = robot_xml_get_child_by_name(labirinth, "sprites");
	robot = robot_xml_get_child_by_name(labirinth, "robot");
	table = robot_xml_get_child_by_name(labirinth, "table");
	if (!sprites || !robot || !table) {
		g_set_error(error, ROBOT_ERROR, -1, "Invalid XML: [sprites|robot|table] = [%d, %d, %d]", sprites != NULL, robot != NULL, table != NULL);
		return FALSE;
	}

	/* Loading sprites: */
	g_ptr_array_set_size(self->priv->sprites, 0);
	cnt = robot_xml_get_children_count(sprites);
	for (i = 0; i < cnt; i++) {
		node = robot_xml_get_child(sprites, i);

		c = robot_xml_get_attribute(node, "type");
		if (!c)
			c = "wall";

		if (!strcmp(c, "way")) {
			has_types |= 1;
			type = sprite_way;
		} else if (!strcmp(c, "exit")) {
			type = sprite_exit;
			has_types |= 2;
		} else {
			type = sprite_wall;
			has_types |= 4;
		}

		c = robot_xml_get_attribute(node, "char");
		if (!c || strlen(c) > 1) {
			g_set_error(error, ROBOT_ERROR, -1, "Invalid labirinth file format");
			return FALSE;
		}

		nsprite = sprite_new(c[0], type);
		if (!robot_sprite_from_xml_node(nsprite->sprite, renderer, filename, node, error)) {
			sprite_free(nsprite);
			return FALSE;
		}

		g_ptr_array_add(self->priv->sprites, nsprite);
	}

	if (has_types != 7) {
		g_set_error(error, ROBOT_ERROR, -1, "Invalid labirinth file format: not all sprite types present");
		return FALSE;
	}

	/* Loading robot: */
	node = robot_xml_get_child_by_name(robot, "sprite");
	if (!node) {
		g_set_error(error, ROBOT_ERROR, -1, "Invalid labirinth file format");
		return FALSE;
	}

	g_clear_object(&self->priv->robot);
	self->priv->robot = robot_robot_new();
	if (!robot_robot_from_xml_node(self->priv->robot, renderer, filename, node, error)) {
		return FALSE;
	}

	cnt = robot_xml_get_children_count(table);
	if (cnt == 0) {
		g_set_error(error, ROBOT_ERROR, -1, "XML error: table has no rows");
		return FALSE;
	}

	height = cnt;
	width = 0;
	rows = g_ptr_array_new_full(cnt, g_free);
	for (i = 0; i < cnt; i++) {
		node = robot_xml_get_child(table, i);
		if (!node || !robot_xml_is_name(node, "row") || ((row = robot_xml_get_attribute(node, "value")) == NULL)) {
			g_ptr_array_free(rows, TRUE);
			g_set_error(error, ROBOT_ERROR, -1, "XML error: invalid table row");
			return FALSE;
		}

		l = strlen(row);
		if (l == 0) {
			g_ptr_array_free(rows, TRUE);
			g_set_error(error, ROBOT_ERROR, -1, "Invalid value attribute in row");
			return FALSE;
		}

		if (width == 0 || width > l)
			width = l;

		g_ptr_array_add(rows, g_strdup(row));
	}

	robot_labirinth_set_size(self, width, height);

	for (j = 0; j < height; j++) {
		row = g_ptr_array_index(rows, j);
		for (i = 0; i < width; i++) {
			set_cell(self, i, j, row[i]);
		}
	}
	g_ptr_array_free(rows, TRUE);

	self->priv->robot_x = pos_x;
	self->priv->robot_y = pos_y;
	self->priv->next_x = pos_x;
	self->priv->next_y = pos_y;

	/* Ok. We've got it! */
	return TRUE;
}

gboolean robot_labirinth_load(RobotLabirinth *self, const char *filename, SDL_Renderer *renderer, GError **error)
{
	RobotXml *xml;
	gboolean res;

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		g_set_error(error, ROBOT_ERROR, -1, "File not found: %s", filename);
		return FALSE;
	}

	xml = robot_xml_load_from_file(filename, error);
	if (!xml) {
		return FALSE;
	}

	res = robot_labirinth_parse_xml(self, xml, filename, renderer, error);
	g_object_unref(xml);

	return res;
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
	guint x, y;
	struct sprite *s;
	RobotLabirinthCell cell;
	gint cell_w, cell_h;
	gint old_x, old_y;

	g_return_if_fail(self->priv->sprites->len > 0);
	g_return_if_fail(self->priv->robot != NULL);
	g_return_if_fail(self->priv->width > 0 && self->priv->height > 0);

	cell_w = self->priv->rect.w / self->priv->width;
	cell_h = self->priv->rect.h / self->priv->height;

	for (y = 0; y < self->priv->height; y++) {
		for (x = 0; x < self->priv->width; x++) {
			cell = robot_labirinth_get_cell(self, x, y);
			if (cell >= self->priv->sprites->len)
				continue;

			s = g_ptr_array_index(self->priv->sprites, cell);

			robot_sprite_scale_to(s->sprite, cell_w, cell_h);
			robot_sprite_set_position(s->sprite, x * cell_w, y * cell_h);

			robot_sprite_render(s->sprite, renderer);
		}
	}

	robot_sprite_scale_to(ROBOT_SPRITE(self->priv->robot), cell_w * 2, cell_h * 2);
	robot_sprite_get_position(ROBOT_SPRITE(self->priv->robot), &old_x, &old_y);
	robot_sprite_set_position(ROBOT_SPRITE(self->priv->robot),
			(self->priv->robot_x - 1) * cell_w + (old_x * cell_w) / 1000 + cell_w / 2,
			(self->priv->robot_y - 1) * cell_h + (old_y * cell_h) / 1000 );
	robot_sprite_render(ROBOT_SPRITE(self->priv->robot), renderer);
	robot_sprite_set_position(ROBOT_SPRITE(self->priv->robot), old_x, old_y);
}

static gboolean x_get_rect(RobotIDrawable *self, SDL_Rect *rect)
{
	g_return_val_if_fail(ROBOT_IS_LABIRINTH(self), FALSE);

	memcpy(rect, &ROBOT_LABIRINTH(self)->priv->rect, sizeof(SDL_Rect));

	return TRUE;
}

static void x_render(RobotIDrawable *self, SDL_Renderer *renderer)
{
	g_return_if_fail(ROBOT_IS_LABIRINTH(self));

	robot_labirinth_render(ROBOT_LABIRINTH(self), renderer);
}

static gint64 x_action(RobotIDrawable *self_, gint64 now, void *userdata)
{
	guint i;
	struct sprite *s;
	RobotLabirinth *self = ROBOT_LABIRINTH(self_);

	g_return_val_if_fail(ROBOT_IS_LABIRINTH(self), -1);

	for (i = 0; i < self->priv->sprites->len; i++) {
		s = g_ptr_array_index(self->priv->sprites, i);
		if (s->next_update >= 0 && s->next_update <= now) {
			s->next_update = robot_idrawable_action(ROBOT_IDRAWABLE(s->sprite), now, userdata);
		}
	}

	if (self->priv->robot_update >= 0 && self->priv->robot_update <= now) {
		self->priv->robot_update = robot_idrawable_action(ROBOT_IDRAWABLE(self->priv->robot), now, userdata);
	}

	return now + 10000;
}

static void robot_idrawable_init(RobotIDrawableInterface *iface)
{
	iface->get_rect = x_get_rect;
	iface->render = x_render;
	iface->action = x_action;
}

void robot_labirinth_set_rect(RobotLabirinth *self, const SDL_Rect *rect)
{
	g_return_if_fail(rect != NULL && rect->w > 0 && rect->h > 0);

	memcpy(&self->priv->rect, rect, sizeof(SDL_Rect));
}

gboolean robot_labirinth_is_finish(RobotLabirinth *self)
{
	return robot_labirinth_is_exit(self, robot_labirinth_get_cell(self, self->priv->robot_x, self->priv->robot_y));
}

G_LOCK_DEFINE_STATIC(labirinth);

static void next_cell(RobotLabirinth *self, guint *x, guint *y)
{
	RobotDirection d = robot_robot_get_direction(self->priv->robot);
	guint X = self->priv->robot_x;
	guint Y = self->priv->robot_y;

	switch (d) {
		case ROBOT_UP:
			if (Y > 0) --Y;
			break;
		case ROBOT_DOWN:
			++Y;
			break;
		case ROBOT_RIGHT:
			++X;
			break;
		case ROBOT_LEFT:
			if (X > 0) --X;
			break;
	}

	if (x)
		*x = X;
	if (y)
		*y = Y;
}

gboolean robot_labirinth_check(RobotLabirinth *self)
{
	gboolean res = TRUE;
	guint x, y;

	G_LOCK(labirinth);
	if (robot_robot_get_state(self->priv->robot) != ROBOT_IDLE) {
		res = FALSE;
	} else {
		robot_robot_set_state(self->priv->robot, ROBOT_CHECK);
		next_cell(self, &x, &y);
		self->priv->check_result = robot_labirinth_can_walk(self, x, y);
		self->priv->next_x = self->priv->robot_x;
		self->priv->next_y = self->priv->robot_y;
	}
	G_UNLOCK(labirinth);

	return res;
}

gboolean robot_labirinth_walk(RobotLabirinth *self)
{
	gboolean res = TRUE;
	guint x, y;

	G_LOCK(labirinth);
	if (robot_robot_get_state(self->priv->robot) != ROBOT_IDLE) {
		res = FALSE;
	} else {
		next_cell(self, &x, &y);
		if (robot_labirinth_can_walk(self, x, y)) {
			robot_robot_set_state(self->priv->robot, ROBOT_WALK);
			self->priv->next_x = x;
			self->priv->next_y = y;
		} else {
			robot_robot_set_state(self->priv->robot, ROBOT_WALK_ON_PLACE);
			self->priv->next_x = self->priv->robot_x;
			self->priv->next_y = self->priv->robot_y;
		}
	}
	G_UNLOCK(labirinth);

	return res;
}

gboolean robot_labirinth_rotate_left(RobotLabirinth *self)
{
	gboolean res = TRUE;

	G_LOCK(labirinth);
	if (robot_robot_get_state(self->priv->robot) != ROBOT_IDLE) {
		res = FALSE;
	} else {
		robot_robot_set_state(self->priv->robot, ROBOT_ROTATE_LEFT);
		self->priv->next_x = self->priv->robot_x;
		self->priv->next_y = self->priv->robot_y;
	}
	G_UNLOCK(labirinth);

	return res;
}

gboolean robot_labirinth_rotate_right(RobotLabirinth *self)
{
	gboolean res = TRUE;

	G_LOCK(labirinth);
	if (robot_robot_get_state(self->priv->robot) != ROBOT_IDLE) {
		res = FALSE;
	} else {
		robot_robot_set_state(self->priv->robot, ROBOT_ROTATE_LEFT);
		self->priv->next_x = self->priv->robot_x;
		self->priv->next_y = self->priv->robot_y;
	}
	G_UNLOCK(labirinth);

	return res;
}


gboolean robot_labirinth_is_done(RobotLabirinth *self)
{
	gboolean res = FALSE;

	G_LOCK(labirinth);
	if (robot_robot_get_state(self->priv->robot) == ROBOT_IDLE) {
		robot_sprite_set_position(ROBOT_SPRITE(self->priv->robot), 0, 0);
		if (self->priv->next_x != self->priv->robot_x || self->priv->next_y != self->priv->robot_y) {
			/* When robot moved we need to change position: */
			self->priv->robot_x = self->priv->next_x;
			self->priv->robot_y = self->priv->next_y;
		}

		res = TRUE;
	}
	G_UNLOCK(labirinth);

	return res;
}

gboolean robot_labirinth_get_result(RobotLabirinth *self)
{
	gboolean res;

	G_LOCK(labirinth);
	res = self->priv->check_result;
	G_UNLOCK(labirinth);

	return res;
}


