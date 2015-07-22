#include <SDL.h>
#include <SDL_image.h>
#include <glib.h>
#include <glib-object.h>
#include "sdl_source.h"
#include "robot_sprite.h"
#include "robot_scene.h"
#include "robot_robot.h"
#include "robot_labirinth.h"
#include "robot.h"
#include <errno.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

const unsigned WIDTH = 800;
const unsigned HEIGHT = 600;

static RobotScene *scene = NULL;
static RobotLabirinth *labirinth = NULL;
static GMainLoop *loop = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;

static lua_State *LUA = NULL;

static int load_lua(const char *filename);

static gboolean timeout_cb(gpointer ptr)
{
	SDL_RenderClear(renderer);

	/* TODO: render :) */
	/*
	SDL_SetRenderDrawColor(renderer, 0xff, 0x0, 0x0, 0x0);
	SDL_RenderDrawLine(renderer, 0, 0, WIDTH, HEIGHT);
	SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
	*/

	robot_scene_render(scene, renderer);

	SDL_RenderPresent(renderer);

	return TRUE;
}

static gboolean event_cb(SDL_Event *event, gpointer userdata)
{
	if (event->type == SDL_QUIT) {
		g_main_loop_quit(loop);
		return TRUE;
	}

	return TRUE;
}

gboolean stop_lua = FALSE;
G_LOCK_DEFINE_STATIC(stop_lua);
static void line_hook(lua_State *L, lua_Debug *dbg)
{
	printf("#line [%d]\n", dbg->currentline);

	G_LOCK(stop_lua);
	if (stop_lua) {
		luaL_error(L, "stopped");
	}
	G_UNLOCK(stop_lua);
}

gboolean running = FALSE;
G_LOCK_DEFINE_STATIC(running);

static gpointer thread_lua(gpointer ctx)
{
	lua_sethook(LUA, line_hook, LUA_MASKLINE, 0);

	if (lua_pcall(LUA, 0, LUA_MULTRET, 0) != 0) {
		printf("#error %s\n", lua_tostring(LUA, -1));
		return FALSE;
	}

	G_LOCK(running);
	running = FALSE;
	G_UNLOCK(running);

	return NULL;
}

static GThread* th_lua;
static void start_lua(void)
{
	G_LOCK(running);
	running = FALSE;
	G_UNLOCK(running);

	th_lua = g_thread_new("lua", thread_lua, NULL);
}

static void wait_lua(void)
{
	G_LOCK(stop_lua);
	stop_lua = TRUE;
	G_UNLOCK(stop_lua);

	g_thread_join(th_lua);
}

static int test_vm(void);
static int init_lua(void);
int main(int argc, char *argv[])
{
	GError *error = NULL;
	const char *progname = "robot.lua";

	if (argc > 1 && !strcmp(argv[1], "vm")) {
		return test_vm();
	}

	if (argc > 1)
		progname = argv[1];

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Error: SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
		fprintf(stderr, "Error: IMG_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	window = SDL_CreateWindow("Roboshow", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		SDL_Quit();
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		fprintf(stderr, "Error: SDL_CreateRenderer failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);

	loop = g_main_loop_new(NULL, FALSE);
	scene = robot_scene_new_full(loop);

	labirinth = robot_labirinth_new();
	if (!robot_labirinth_load(labirinth, "img/labirinth.xml", renderer, &error)) {
		fprintf(stderr, "Error: can't load labirinth :(\n");
		fprintf(stderr, "[%s]\n", error->message);
		return -1;
	}
	robot_scene_add_drawable(scene, ROBOT_IDRAWABLE(labirinth), 0, NULL, NULL);

	/* Creating main loop: */
	g_timeout_add(30, timeout_cb, NULL);
	x_sdl_add_source(loop, event_cb, NULL, NULL);
	init_lua();

	if (load_lua(progname)) {
		g_clear_object(&scene);
		g_clear_object(&labirinth);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();

		return EXIT_FAILURE;
	}

	start_lua();

	g_main_loop_run(loop);

	wait_lua();

	g_clear_object(&scene);
	g_clear_object(&labirinth);

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

static char test_prog[] =
	"; This is comment\n"
	"# And this is comment too\n"
	"nop\n"
	"load 0x123556\n"
	":addr1\n"
	"load @addr1\n"
	"load %$add$\n"
	":var { 01 02 03 04 05}\n"
	":var2 { 01 cD 03 04 ab }\n"
	"sys\n"
	"load @addr1\n"
	"jump\n\n";

static int test_vm(void)
{
	RobotVM *vm = robot_vm_new();
	GError *error = NULL;
	RobotObjFile *obj = robot_obj_file_new();
	GByteArray *code;
	guint i;

	if (!robot_obj_file_compile(obj, test_prog, &error)) {
		fprintf(stderr, "Error: %s\n", error->message);
		g_object_unref(vm);
		return 1;
	}

	code = robot_obj_file_to_byte_array(obj, &error);
	if (code) {
		for (i = 0; i < code->len; i++) {
			printf("%02x ", code->data[i]);
			if ((i + 1) % 10 == 0)
				printf("\n");
		}
		printf("\n");

		if (!robot_obj_file_from_byte_array(obj, code, &error)) {
			fprintf(stderr, "Error: %s\n", error->message);
			return 1;
		}
		g_byte_array_unref(code);
	} else {
		fprintf(stderr, "Error: %s\n", error->message);
		return 1;
	}

	g_object_unref(vm);
	g_object_unref(obj);

	return 0;
}

#define LUA_GBOOLEAN_VOID(nm) \
static int l_##nm(lua_State *lua) \
{ \
	gboolean res = robot_labirinth_##nm(labirinth); \
	lua_pushboolean(lua, res); \
	return 1; \
}

static gboolean wait_done(void)
{
	gboolean res = FALSE;

	while (!robot_labirinth_is_done(labirinth)) {
		g_usleep(1000);
		G_LOCK(stop_lua);
		res = stop_lua;
		G_UNLOCK(stop_lua);

		if (res)
			return res;
	}

	return res;
}

static int l_check(lua_State *L)
{
	if (wait_done()) {
		lua_pushboolean(L, 0);
		return 1;
	}

	robot_labirinth_check(labirinth);

	if (wait_done()) {
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, robot_labirinth_get_result(labirinth));

	return 1;
}

static int l_walk(lua_State *L)
{
	if (wait_done())
		return 0;

	robot_labirinth_walk(labirinth);

	wait_done();

	return 0;
}

static int l_rotate_left(lua_State *L)
{
	if (wait_done())
		return 0;

	robot_labirinth_rotate_left(labirinth);

	wait_done();

	return 0;
}

static int l_rotate_right(lua_State *L)
{
	if (wait_done())
		return 0;

	robot_labirinth_rotate_right(labirinth);

	wait_done();

	return 0;
}

LUA_GBOOLEAN_VOID(is_finish);

static int init_lua(void)
{
	LUA = luaL_newstate();
	if (!LUA)
		return -1;

	lua_register(LUA, "is_finish", l_is_finish);
	lua_register(LUA, "walk", l_walk);
	lua_register(LUA, "check", l_check);
	lua_register(LUA, "right", l_rotate_right);
	lua_register(LUA, "left", l_rotate_left);

	return 0;
}

static int load_lua(const char *filename)
{
	FILE *f = fopen(filename, "rt");
	GString *str;
	char buf[512];
	gssize len;
	gchar *prog;

	if (!f) {
		fprintf(stderr, "Error: can not load file!\n");
		return -1;
	}

	str = g_string_new("");
	for (;;) {
		len = fread(buf, 1, sizeof(buf), f);
		if (len < 0) {
			fprintf(stderr, "Error: %s\n", strerror(errno));
			fclose(f);
			g_string_free(str, TRUE);
			return -1;
		}

		if (len == 0) {
			fclose(f);
			break;
		}

		g_string_append_len(str, buf, len);
	}

	prog = g_string_free(str, FALSE);

	if (luaL_loadstring(LUA, prog)) {
		fprintf(stderr, "Error: can not load Lua program!\n");
		fprintf(stderr, "ERROR: %s\n", lua_tostring(LUA, -1));

		g_free(prog);

		return -1;
	}

	return 0;
}

