#ifndef _ROBOT_SPRITE_H_
#define _ROBOT_SPRITE_H_ 1

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
#define ROBOT_TYPE_SPRITE                   (robot_sprite_get_type())
#define ROBOT_SPRITE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_SPRITE, RobotSprite))
#define ROBOT_IS_SPRITE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_SPRITE))
#define ROBOT_SPRITE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_SPRITE, RobotSpriteClass))
#define ROBOT_IS_SPRITE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_SPRITE))
#define ROBOT_SPRITE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_SPRITE, RobotSpriteClass))

/* get_type prototype: */
GType robot_sprite_get_type(void);

/* Structures definitions: */
typedef struct _RobotSprite RobotSprite;
typedef struct _RobotSpriteClass RobotSpriteClass;
typedef struct _RobotSpritePrivate RobotSpritePrivate;

typedef gint64 (*RobotSpriteAction)(RobotSprite *self, gint64 now, gpointer userptr);

struct _RobotSprite {
	GObject parent_instance;

	RobotSpritePrivate *priv;
};

struct _RobotSpriteClass {
	GObjectClass parent_class;
};

/*
 * x_sdk_sprite_new:
 *
 * Create new RobotSprite instance.
 */
RobotSprite* robot_sprite_new(void);

/**
 * robot_sprite_load_from_file:
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
gboolean robot_sprite_load_from_file(RobotSprite *self, SDL_Renderer *renderer, const char *name, GError **error);

/**
 * robot_sprite_get_modes:
 *
 * Get full list of modes in this sprite.
 * @mode [out] pointer to array of modes. This array value will be set by this function.
 * @cnt [out] pointer to length of array.
 * You must free array using g_free.
 */
void robot_sprite_get_modes(RobotSprite *self, GQuark **modes, guint *cnt);

/**
 * robot_sprite_set_mode:
 *
 * Set current mode for this sprite. Frame will be set to 0.
 *
 * @mode is mode to set. If this mode is not supported by this sprite default_mode will be set.
 *
 * @returns TRUE on success and FALSE on error.
 */
gboolean robot_sprite_set_mode(RobotSprite *self, GQuark mode);

/**
 * robot_sprite_has_mode:
 *
 * Check if this sprite supports given mode.
 *
 * @mode is mode to check.
 *
 * @returns TRUE if mode is supported and FALSE otherwise.
 */
gboolean robot_sprite_has_mode(RobotSprite *self, GQuark mode);

/**
 * robot_sprite_set_default_mode:
 *
 * Set default mode for this sprite. If some mode is not supported default mode will be used.
 */
gboolean robot_sprite_set_default_mode(RobotSprite *self, GQuark mode);

/**
 * robot_sprite_set_frame(RobotSprite *self, guint frame);
 *
 * Set frame number to frame in current mode. Actually sets frame number to frame % frames_cnt so you could simply increment frame and don't worry about
 * frames count in some cases.
 *
 * @frame - number of frame to set as current.
 */
void robot_sprite_set_frame(RobotSprite *self, guint frame);

/**
 * robot_sprite_get_frames_count:
 *
 * Get count of frames in current mode of this sprite.
 */
guint robot_sprite_get_frames_count(RobotSprite *self);

/**
 * robot_sprite_add_frame:
 *
 * Add frame and mode if not present to current sprite. Frame will be added to the end of list.
 *
 * @mode - mode of frame.
 * @frame - frame. This frame will be freed by sprite. If it is not what you want copy it yourself.
 */
void robot_sprite_add_frame(RobotSprite *self, GQuark mode, SDL_Texture *frame);

/**
 * robot_sprite_get_width:
 *
 * Get width of current frame.
 */
guint robot_sprite_get_width(RobotSprite *self);
/**
 * robot_sprite_get_height:
 *
 * Get height of current frame.
 */
guint robot_sprite_get_height(RobotSprite *self);

void robot_sprite_set_position(RobotSprite *self, gint x, gint y);
void robot_sprite_get_position(RobotSprite *self, gint *x, gint *y);
gint robot_sprite_get_x(RobotSprite *self);
gint robot_sprite_get_y(RobotSprite *self);

void robot_sprite_set_rotation(RobotSprite *self, double angle); /* Angle in degrees */
double robot_sprite_get_rotation(RobotSprite *self);             /* Angle in degrees */

void robot_sprite_set_scale(RobotSprite *self, double scale_x, double scale_y);
void robot_sprite_get_scale(RobotSprite *self, double *scale_x, double *scale_y);
double robot_sprite_get_scale_x(RobotSprite *self);
double robot_sprite_get_scale_y(RobotSprite *self);
void robot_sprite_scale_to(RobotSprite *self, guint width, guint height);

void robot_sprite_rotate(RobotSprite *self, double angle);
void robot_sprite_move(RobotSprite *self, gint x, gint y);
void robot_sprite_scale_x(RobotSprite *self, double scale);
void robot_sprite_scale_y(RobotSprite *self, double scale);

void robot_sprite_render(RobotSprite *self, SDL_Renderer *renderer);
gboolean robot_sprite_get_rect(RobotSprite *self, SDL_Rect *rect);
gint64 robot_sprite_action(RobotSprite *self, gint64 now, void *userdata);
void robot_sprite_set_action(RobotSprite *self, RobotSpriteAction action);

G_END_DECLS

#endif /* ROBOT_SPRITE_H */

