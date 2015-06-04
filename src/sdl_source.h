#ifndef SDL_MAIN_LOOP_SOURCE_H_
#define SDL_MAIN_LOOP_SOURCE_H_

#include <glib.h>
#include <SDL.h>

G_BEGIN_DECLS

typedef gboolean (*XSDLSourceFunc)(SDL_Event *event, gpointer userdata);

GSource* x_sdl_create_source(void);
guint x_sdl_add_source(GMainLoop *loop, XSDLSourceFunc func, gpointer userdata, GDestroyNotify notify);

G_END_DECLS

#endif

