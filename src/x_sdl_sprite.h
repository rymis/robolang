#ifndef _X_SDL_SPRITE_H_
#define _X_SDL_SPRITE_H_ 1

/* Simple sprite object for SDL:
 * Sprite has modes and in each mode could be one or many textures.
 * For example walking man could be made using modes:
 *   "walk"
 *   "rotate"
 *   "jump"
 *   "idle"
 * You can look inside examples for example usage.
 * For this library error domain is "SDL".
 */
#include <glib-object.h>
#include <SDL.h>

G_BEGIN_DECLS

/* Type conversion macroses: */
#define X_TYPE_SDL_SPRITE                   (x_sdl_sprite_get_type())
#define X_SDL_SPRITE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  X_TYPE_SDL_SPRITE, XSDLSprite))
#define X_IS_SDL_SPRITE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), X_TYPE_SDL_SPRITE))
#define X_SDL_SPRITE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  X_TYPE_SDL_SPRITE, XSDLSpriteClass))
#define X_IS_SDL_SPRITE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  X_TYPE_SDL_SPRITE))
#define X_SDL_SPRITE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  X_TYPE_SDL_SPRITE, XSDLSpriteClass))

/* get_type prototype: */
GType x_sdl_sprite_get_type(void);

/* Structures definitions: */
typedef struct _XSDLSprite XSDLSprite;
typedef struct _XSDLSpriteClass XSDLSpriteClass;
typedef struct _XSDLSpritePrivate XSDLSpritePrivate;

struct _XSDLSprite {
	GObject parent_instance;

	XSDLSpritePrivate *priv;
};

struct _XSDLSpriteClass {
	GObjectClass parent_class;
};

/*
 * x_sdk_sprite_new:
 *
 * Create new XSDLSprite instance.
 */
XSDLSprite* x_sdl_sprite_new(void);

/**
 * x_sdl_sprite_load_from_file:
 *
 * Loads sprite from file. File is in .ini format. Each mode present as group. Each value is frame.
 * Frames will be numbered as present in file, names does not matter. If there are two values in form
 * name = ...; name_mask = ...; then name_mask will be used as mask for name.
 *
 * Default section is used for sprite parameters. Them will be saved in sprite so you could check something.
 *
 * @renderer is renderer this sprite intendent for.
 * @name is file name for loading. If this file is could not be parsed as .ini file function will try to load one image in 'default' mode.
 * @error is error placement.
 *
 * @return TRUE on success and FALSE on error.
 */
gboolean x_sdl_sprite_load_from_file(XSDLSprite *self, SDL_Renderer *renderer, const char *name, GError **error);

/**
 * x_sdl_sprite_get_modes:
 *
 * Get full list of modes in this sprite.
 * @mode [out] pointer to array of modes. This array value will be set by this function.
 * @cnt [out] pointer to length of array.
 * You must free array using g_free.
 */
void x_sdl_sprite_get_modes(XSDLSprite *self, GQuark **modes, guint *cnt);

/**
 * x_sdl_sprite_set_mode:
 *
 * Set current mode for this sprite. Frame will be set to 0.
 *
 * @mode is mode to set. If this mode is not supported by this sprite default_mode will be set.
 *
 * @returns TRUE on success and FALSE on error.
 */
gboolean x_sdl_sprite_set_mode(XSDLSprite *self, GQuark mode);

/**
 * x_sdl_sprite_has_mode:
 *
 * Check if this sprite supports given mode.
 *
 * @mode is mode to check.
 *
 * @returns TRUE if mode is supported and FALSE otherwise.
 */
gboolean x_sdl_sprite_has_mode(XSDLSprite *self, GQuark mode);

/**
 * x_sdl_sprite_set_default_mode:
 *
 * Set default mode for this sprite. If some mode is not supported default mode will be used.
 */
gboolean x_sdl_sprite_set_default_mode(XSDLSprite *self, GQuark mode);

/**
 * x_sdl_sprite_set_frame(XSDLSprite *self, guint frame);
 *
 * Set frame number to frame in current mode. Actually sets frame number to frame % frames_cnt so you could simply increment frame and don't worry about
 * frames count in some cases.
 *
 * @frame - number of frame to set as current.
 */
void x_sdl_sprite_set_frame(XSDLSprite *self, guint frame);

/**
 * x_sdl_sprite_get_frames_count:
 *
 * Get count of frames in current mode of this sprite.
 */
guint x_sdl_sprite_get_frames_count(XSDLSprite *self);

/**
 * x_sdl_sprite_add_frame:
 *
 * Add frame and mode if not present to current sprite. Frame will be added to the end of list.
 *
 * @mode - mode of frame.
 * @frame - frame. This frame will be freed by sprite. If it is not what you want copy it yourself.
 */
void x_sdl_sprite_add_frame(XSDLSprite *self, GQuark mode, SDL_Texture *frame);

/**
 * x_sdl_sprite_get_width:
 *
 * Get width of current frame.
 */
guint x_sdl_sprite_get_width(XSDLSprite *self);
/**
 * x_sdl_sprite_get_height:
 *
 * Get height of current frame.
 */
guint x_sdl_sprite_get_height(XSDLSprite *self);

void x_sdl_sprite_set_position(XSDLSprite *self, gint x, gint y);
void x_sdl_sprite_get_position(XSDLSprite *self, gint *x, gint *y);
gint x_sdl_sprite_get_x(XSDLSprite *self);
gint x_sdl_sprite_get_y(XSDLSprite *self);

void x_sdl_sprite_set_rotation(XSDLSprite *self, double angle); /* Angle in degrees */
double x_sdl_sprite_get_rotation(XSDLSprite *self);             /* Angle in degrees */

void x_sdl_sprite_set_scale(XSDLSprite *self, double scale_x, double scale_y);
void x_sdl_sprite_get_scale(XSDLSprite *self, double *scale_x, double *scale_y);
double x_sdl_sprite_get_scale_x(XSDLSprite *self);
double x_sdl_sprite_get_scale_y(XSDLSprite *self);
void x_sdl_sprite_scale_to(XSDLSprite *self, guint width, guint height);

void x_sdl_sprite_rotate(XSDLSprite *self, double angle);
void x_sdl_sprite_move(XSDLSprite *self, gint x, gint y);
void x_sdl_sprite_scale_x(XSDLSprite *self, double scale);
void x_sdl_sprite_scale_y(XSDLSprite *self, double scale);

void x_sdl_sprite_render(XSDLSprite *self, SDL_Renderer *renderer);

G_END_DECLS

#endif /* X_SDL_SPRITE_H */
