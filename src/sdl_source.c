#include "sdl_source.h"

typedef struct _XSDLSource {
	GSource source;
} XSDLSource;

static gboolean prepare(GSource *source, gint *timeout)
{
	if (timeout)
		*timeout = 50;

	if (SDL_PollEvent(NULL) > 0)
		return TRUE;

	return FALSE;
}

static gboolean check(GSource *source)
{
	return (SDL_PollEvent(NULL) > 0);
}

static gboolean dispatch(GSource *source, GSourceFunc callback, gpointer userdata)
{
	SDL_Event event;
	XSDLSourceFunc cb = (XSDLSourceFunc)callback;
	gboolean res = TRUE;
	int cnt = 0;

	while (SDL_PollEvent(&event)) {
		if (cb)
			res = res && cb(&event, userdata);

		if (++cnt > 16)
			break;
	}

	return res;
}

static void finalize(GSource *source)
{
	(void)source;
}

static GSourceFuncs functions = {
	prepare, check, dispatch, finalize, NULL, NULL
};

GSource* x_sdl_create_source(void)
{
	return g_source_new(&functions, sizeof(XSDLSource));
}

guint x_sdl_add_source(GMainLoop *loop, XSDLSourceFunc cb, gpointer userdata, GDestroyNotify notify)
{
	GSource *src = x_sdl_create_source();
	g_source_set_callback(src, (GSourceFunc)cb, userdata, notify);

	return g_source_attach(src, g_main_loop_get_context(loop));
}

