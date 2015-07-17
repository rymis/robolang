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

const unsigned WIDTH = 800;
const unsigned HEIGHT = 600;

static RobotScene *scene = NULL;
static RobotLabirinth *labirinth = NULL;
static GMainLoop *loop = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;

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

static gboolean idle_cb(gpointer ptr)
{
	static RobotState last_cmd = ROBOT_IDLE;
	int c;

	if (robot_labirinth_is_done(labirinth)) {
		if (last_cmd == ROBOT_CHECK) {
			if (robot_labirinth_get_result(labirinth)) {
printf("{WALK}\n");
				last_cmd = ROBOT_WALK;
				robot_labirinth_walk(labirinth);
			} else {
				last_cmd = ROBOT_IDLE;
			}
		} else {
			c = rand() % 6;

			if (robot_labirinth_is_finish(labirinth)) {
printf("{OK}\n");
				return FALSE;
			}

			if (c == 0) {
printf("{LEFT}\n");
				last_cmd = ROBOT_ROTATE_LEFT;
				robot_labirinth_rotate_left(labirinth);
			} else if (c == 1) {
printf("{RIGHT}\n");
				last_cmd = ROBOT_ROTATE_RIGHT;
				robot_labirinth_rotate_right(labirinth);
			} else {
printf("{CHECK}\n");
				last_cmd = ROBOT_CHECK;
				robot_labirinth_check(labirinth);
			}
		}
	}

	return TRUE;
}

static int test_vm(void);
int main(int argc, char *argv[])
{
	GError *error = NULL;

	if (argc > 1 && !strcmp(argv[1], "vm")) {
		return test_vm();
	}

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
	g_idle_add(idle_cb, NULL);
	x_sdl_add_source(loop, event_cb, NULL, NULL);

	g_main_loop_run(loop);

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

