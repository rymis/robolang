#include "robot_sprite.h"
#include "robot_idrawable.h"
#include "robot_xml.h"
#include <SDL_image.h>

static void robot_idrawable_init(RobotIDrawableInterface *iface);

static GQuark sdl_error_get(void)
{
	static GQuark e = 0;

	if (!e)
		e = g_quark_from_string("SDL");

	return e;
}

#define ERROR_SDL sdl_error_get()

/* Structure for mode representation: */
struct _RobotSpritePrivate {
	/* All modes. Each mode is array of frames. */
	GTree *modes;

	/* Current mode array: */
	GPtrArray *frames;

	/* Current texture (we don't want to search for it) */
	SDL_Texture *texture;

	GQuark default_mode;
	GQuark mode;
	guint frame;

	double rotation;
	int scale_x, scale_y;
	gint x, y;
	SDL_RendererFlip flip;

	RobotSpriteAction action;
};

G_DEFINE_TYPE_WITH_CODE(RobotSprite, robot_sprite, G_TYPE_OBJECT,
		G_ADD_PRIVATE(RobotSprite)
		G_IMPLEMENT_INTERFACE(ROBOT_TYPE_IDRAWABLE, robot_idrawable_init)
)

static void finalize(GObject *obj)
{
	RobotSprite *self = ROBOT_SPRITE(obj);

	g_tree_unref(self->priv->modes);

	self->priv->texture = NULL;

	self->priv = NULL;
}

static void robot_sprite_class_init(RobotSpriteClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->finalize = finalize;
}

static void destroy_texture(gpointer p)
{
	SDL_DestroyTexture(p);
}

static void array_unref(gpointer p)
{
	g_ptr_array_unref(p);
}

static gint quark_cmp(gconstpointer p1, gconstpointer p2, gpointer userdata)
{
	gint q1 = GPOINTER_TO_UINT(p1);
	gint q2 = GPOINTER_TO_UINT(p2);

	(void)userdata;

	return q1 - q2;
}

static void robot_sprite_init(RobotSprite *self)
{
	self->priv = robot_sprite_get_instance_private(self);
	self->priv->rotation = 0.0;
	self->priv->scale_x = -1;
	self->priv->scale_y = -1;
	self->priv->x = 0;
	self->priv->y = 0;
	self->priv->flip = SDL_FLIP_NONE;
	self->priv->modes = g_tree_new_full(quark_cmp, NULL, NULL, array_unref);
	self->priv->texture = NULL;
	self->priv->frame = 0;
	self->priv->frames = NULL;
	self->priv->action = NULL;
}

RobotSprite* robot_sprite_new(void)
{
	RobotSprite* self = g_object_new(ROBOT_TYPE_SPRITE, NULL);

	return self;
}

static SDL_Texture *load_texture(const char *filename, SDL_Renderer *renderer)
{
	SDL_Surface *surface;
	SDL_Texture *texture;

	surface = IMG_Load(filename);
	if (!surface) {
		/* TODO: create empty surface? */
		return NULL;
	}

	texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	return texture;
}

gboolean robot_sprite_load_from_file(RobotSprite *self, SDL_Renderer *renderer, const char *name, GError **error)
{
	SDL_Texture *texture;
	RobotXml *xml;

	if (!g_file_test(name, G_FILE_TEST_EXISTS)) {
		g_set_error(error, ERROR_SDL, -1, "File not found: %s", name);
		return FALSE;
	}

	/* Trying to load this texture as .ini file: */
	xml = robot_xml_load_from_file(name, error);
	if (!xml) {
		texture = load_texture(name, renderer);
		if (!texture) {
			return FALSE;
		}

		g_clear_error(error);

		robot_sprite_add_frame(self, g_quark_from_string("default"), texture);

		return TRUE;
	}

	if (!robot_sprite_from_xml_node(self, renderer, name, xml, error)) {
		g_object_unref(xml);
		return FALSE;
	}
	g_object_unref(xml);
	
	return TRUE;
}

gboolean robot_sprite_from_xml_node(RobotSprite *self, SDL_Renderer *renderer, const gchar* name, RobotXml *xml, GError **error)
{
	guint modes_cnt;
	guint frames_cnt;
	guint i, j;
	guint frames = 0;
	const gchar *mname;
	const gchar *fname;
	gchar *dirname = NULL;
	RobotXml *mxml;
	RobotXml *fxml;
	GQuark mode;
	gchar *nm;
	SDL_Texture *texture;

	if (strcmp(robot_xml_get_name(xml), "sprite")) {
		g_set_error(error, ERROR_SDL, -1, "Invalid sprite: %s", name);
		g_object_unref(xml);
		return FALSE;
	}

	modes_cnt = robot_xml_get_children_count(xml);
	for (i = 0; i < modes_cnt; i++) {
		mxml = robot_xml_get_child(xml, i);
		if (!mxml)
			continue;
		if (strcmp(robot_xml_get_name(mxml), "mode"))
			continue;
		mname = robot_xml_get_attribute(mxml, "name");
		if (!mname)
			continue;

		mode = g_quark_from_string(mname);

		frames_cnt = robot_xml_get_children_count(mxml);
		for (j = 0; j < frames_cnt; j++) {
			fxml = robot_xml_get_child(mxml, j);

			if (!fxml)
				continue;
			if (strcmp(robot_xml_get_name(fxml), "frame"))
				continue;
			fname = robot_xml_get_attribute(fxml, "img");
			if (!fname)
				continue;

			nm = g_strdup(fname);

			if (!g_path_is_absolute(nm)) {
				gchar *tmp;

				if (!dirname)
					dirname = g_path_get_dirname(name);

				tmp = g_build_filename(dirname, nm, NULL);
				g_free(nm);

				nm = tmp;
			}

			if (nm) {
				texture = load_texture(nm, renderer);

				if (texture) {
					frames++;
					robot_sprite_add_frame(self, mode, texture);
				}

				g_free(nm);
			}
		}
	}
	g_free(dirname);

	return TRUE;
}

static gboolean traverse(gpointer key, gpointer value, gpointer data)
{
	GArray *array = data;
	GQuark q = GPOINTER_TO_UINT(value);

	g_array_append_val(array, q);

	return TRUE;
}

GArray* robot_sprite_get_modes(RobotSprite *self)
{
	GArray *res = g_array_new(FALSE, FALSE, sizeof(GQuark));

	g_tree_foreach(self->priv->modes, traverse, res);

	return res;
}

gboolean robot_sprite_set_mode(RobotSprite *self, GQuark mode)
{
	GPtrArray *array = g_tree_lookup(self->priv->modes, GUINT_TO_POINTER(mode));
	if (!array) {
		array = g_tree_lookup(self->priv->modes, GUINT_TO_POINTER(self->priv->default_mode));
		mode = self->priv->default_mode;
	}

	if (!array)
		return FALSE;
	self->priv->mode = mode;
	self->priv->frames = array;

	/* Now we can set current frame */
	robot_sprite_set_frame(self, 0);

	return TRUE;
}

gboolean robot_sprite_has_mode(RobotSprite *self, GQuark mode)
{
	return g_tree_lookup(self->priv->modes, GUINT_TO_POINTER(mode)) != NULL;
}

gboolean robot_sprite_set_default_mode(RobotSprite *self, GQuark mode)
{
	if (g_tree_lookup(self->priv->modes, GUINT_TO_POINTER(mode)) != NULL) {
		self->priv->default_mode = mode;
		return TRUE;
	}

	return FALSE;
}

void robot_sprite_set_frame(RobotSprite *self, guint frame)
{
	g_return_if_fail(self->priv->frames != NULL);
	g_return_if_fail(self->priv->frames->len > 0);

	frame %= self->priv->frames->len;
	self->priv->frame = frame;
	self->priv->texture = g_ptr_array_index(self->priv->frames, frame);
}

guint robot_sprite_get_frames_count(RobotSprite *self)
{
	g_return_val_if_fail(self->priv->frames != NULL, 0);

	return self->priv->frames->len;
}

void robot_sprite_add_frame(RobotSprite *self, GQuark mode, SDL_Texture *frame)
{
	GPtrArray *array = g_tree_lookup(self->priv->modes, GUINT_TO_POINTER(mode));

	g_return_if_fail(frame != NULL);
	g_return_if_fail(mode != 0);

	if (!array) {
		array = g_ptr_array_new_with_free_func(destroy_texture);
		if (!self->priv->frames) {
			self->priv->mode = mode;
			if (!self->priv->default_mode)
				self->priv->default_mode = mode;
			self->priv->frames = array;
			self->priv->frame = 0;
			self->priv->texture = frame;
		}
		g_tree_insert(self->priv->modes, GUINT_TO_POINTER(mode), array);
	}

	g_ptr_array_add(array, frame);

}

static void get_size(RobotSprite *self, int *w, int *h)
{
	if (!self->priv->texture) {
		if (w) *w = 0;
		if (h) *h = 0;
		return;
	}

	if (self->priv->scale_x > 0 && self->priv->scale_y > 0) {
		if (w) *w = self->priv->scale_x;
		if (h) *h = self->priv->scale_y;
		return;
	}

	SDL_QueryTexture(self->priv->texture, NULL, NULL, w, h);
}

guint robot_sprite_get_width(RobotSprite *self)
{
	int w;

	get_size(self, &w, NULL);

	return w;
}

guint robot_sprite_get_height(RobotSprite *self)
{
	int h;

	get_size(self, NULL, &h);

	return h;
}

void robot_sprite_set_position(RobotSprite *self, gint x, gint y)
{
	self->priv->x = x;
	self->priv->y = y;
}

void robot_sprite_get_position(RobotSprite *self, gint *x, gint *y)
{
	if (x)
		*x = self->priv->x;
	if (y)
		*y = self->priv->y;
}

gint robot_sprite_get_x(RobotSprite *self)
{
	return self->priv->x;
}

gint robot_sprite_get_y(RobotSprite *self)
{
	return self->priv->y;
}

void robot_sprite_set_rotation(RobotSprite *self, double angle)
{
	self->priv->rotation = angle;
}

double robot_sprite_get_rotation(RobotSprite *self)
{
	return self->priv->rotation;
}

void robot_sprite_scale_to(RobotSprite *self, guint width, guint height)
{
	self->priv->scale_x = width;
	self->priv->scale_y = height;
}

void robot_sprite_rotate(RobotSprite *self, double angle)
{
	self->priv->rotation += angle;
}

void robot_sprite_move(RobotSprite *self, gint x, gint y)
{
	self->priv->x += x;
	self->priv->y += y;
}

void robot_sprite_render(RobotSprite *self, SDL_Renderer *renderer)
{
	SDL_Rect dst;

	g_return_if_fail(self->priv->texture != NULL);

	dst.x = self->priv->x;
	dst.y = self->priv->y;
	get_size(self, &dst.w, &dst.h);
	if (self->priv->scale_x > 0)
		dst.w = self->priv->scale_x;
	if (self->priv->scale_y > 0)
		dst.h = self->priv->scale_y;

	SDL_RenderCopyEx(renderer, self->priv->texture, NULL, &dst, self->priv->rotation, NULL, self->priv->flip);
}

#if 0
/* File format is simple. There are following data types:
 * U32 - unsigned int 32 in bigendian form
 * STR - 0 - terminated string
 * DATA - binary data prefixed by size of type U32
 *
 * File will be saved as:
 * | TYPE | NAME    | DESCRIPTION                                            |
 * | STR  | format  | Format magic string (#!robot_run)                      |
 * | U32  | version | Format version. Now it is 1                            |
 * | U32  | mcnt    | Modes count.                                           |
 * | U32  | defmode | Default mode index                                     |
 * Each group looks like:
 * | STR  | name    | Mode name                                              |
 * | U32  | frames  | Frames count                                           |
 * After this record frames placed in form:
 * | U32  | width   | Width of current frame                                 |
 * | U32  | height  | Height of current frame                                |
 * | DATA | data    | Pixels data                                            |
 */
GByteArray* robot_sprite_to_byte_array(RobotSprite *self, GError **error)
{

}

gboolean robot_sprite_from_byte_array(RobotSprite *self, GByteArray *data, GError **error)
{

}
#endif

gboolean robot_sprite_get_rect(RobotSprite *self, SDL_Rect *rect)
{
	g_return_val_if_fail(self != NULL && self->priv->texture != NULL && rect != NULL, FALSE);

	get_size(self, &rect->w, &rect->h);
	rect->x = self->priv->x;
	rect->y = self->priv->y;

	return TRUE;
}

void robot_sprite_set_action(RobotSprite *self, RobotSpriteAction action)
{
	g_return_if_fail(self != NULL);

	self->priv->action = action;
}

gint64 robot_sprite_action(RobotSprite *self, gint64 now, void *userdata)
{
	g_return_val_if_fail(self && ROBOT_IS_SPRITE(self), -1);


	if (self->priv->action) {
		return self->priv->action(self, now, userdata);
	}

	return -1;
}

static gboolean x_get_rect(RobotIDrawable *self, SDL_Rect *rect)
{
	g_return_val_if_fail(ROBOT_IS_SPRITE(self), FALSE);

	return robot_sprite_get_rect(ROBOT_SPRITE(self), rect);
}

static void x_render(RobotIDrawable *self, SDL_Renderer *renderer)
{
	g_return_if_fail(ROBOT_IS_SPRITE(self));

	robot_sprite_render(ROBOT_SPRITE(self), renderer);
}

static gint64 x_action(RobotIDrawable *self, gint64 now, void *userdata)
{
	g_return_val_if_fail(ROBOT_IS_SPRITE(self), -1);

	return robot_sprite_action(ROBOT_SPRITE(self), now, userdata);
}

static void robot_idrawable_init(RobotIDrawableInterface *iface)
{
	iface->get_rect = x_get_rect;
	iface->render = x_render;
	iface->action = x_action;
}

