#include "robot_labirinth.h"
#include "robot_vm.h"
#include "robot_robot.h"

enum sprite_type {
	sprite_way, sprite_wall, sprite_exit
};

struct sprite {
	enum sprite_type type;
	RobotSprite *sprite;
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

	return res;
}

struct _RobotLabirinthPrivate {
	GArray *cells;
	guint width, height;
	GPtrArray *sprites;
	RobotRobot *robot;
	guint robot_x, robot_y;
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotLabirinth, robot_labirinth, G_TYPE_OBJECT)

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
	self->priv->cells = g_array_new(sizeof(RobotLabirinthCell), FALSE, TRUE);
	self->priv->sprites = g_ptr_array_new_with_free_func(sprite_free);
	self->priv->robot = NULL;
}

RobotLabirinth* robot_labirinth_new(void)
{
	RobotLabirinth* self = g_object_new(ROBOT_TYPE_LABIRINTH, NULL);

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

	width = 0;
	cnt = robot_xml_get_children_count(table);
	if (cnt == 0) {
		g_set_error(error, ROBOT_ERROR, -1, "XML error");
		return FALSE;
	}

	height = cnt;
	rows = g_ptr_array_new();
	for (i = 0; i < cnt; i++) {
		node = robot_xml_get_child(table, i);
		if (!node || !robot_xml_is_name(node, "row") || !(row = robot_xml_get_attribute(node, "value"))) {
			g_ptr_array_unref(rows);
			g_set_error(error, ROBOT_ERROR, -1, "XML error");
			return FALSE;
		}

		l = strlen(row);
		if (l == 0) {
			g_ptr_array_unref(rows);
			g_set_error(error, ROBOT_ERROR, -1, "Invalid value attribute in row");
			return FALSE;
		}

		if (width > 0 && width > l)
			width = l;

		g_ptr_array_add(rows, (gpointer)row);
	}

	robot_labirinth_set_size(self, width, height);

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			row = g_ptr_array_index(rows, i);
			set_cell(self, i, j, row[j]);
		}
	}
	g_ptr_array_unref(rows);

	self->priv->robot_x = pos_x;
	self->priv->robot_y = pos_y;

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
	/* TODO: render */
}

