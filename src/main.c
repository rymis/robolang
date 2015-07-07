#include <SDL.h>
#include <SDL_image.h>
#include <glib.h>
#include <glib-object.h>
#include "sdl_source.h"
#include "robot_sprite.h"
#include "robot_scene.h"
#include "robot_robot.h"
#include "robot.h"

const unsigned WIDTH = 800;
const unsigned HEIGHT = 600;

static RobotScene *scene = NULL;
static RobotSprite *sprite = NULL;
static RobotSprite *man = NULL;
static RobotSprite *cman = NULL;
static GMainLoop *loop = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;

static RobotRobot *robot = NULL;

static GQuark man_walk = 0;
static GQuark man_rotate = 0;

#define CMAN_UP    1
#define CMAN_DOWN  2
#define CMAN_RIGHT 4
#define CMAN_LEFT  8
static unsigned cman_key = 0;
static unsigned cman_frame = 0;

static int move = 0;
#define MAX_SPEED 10
static int speed = 2;
static int acceleration = 1;

static int man_state = 0;
static guint man_frame = 0;

static gboolean event_cb(SDL_Event *event, gpointer userdata)
{
	if (event->type == SDL_QUIT) {
		g_main_loop_quit(loop);
		return TRUE;
	}

	if (event->type == SDL_KEYDOWN) {
		if (event->key.state == SDL_PRESSED) {
			switch (event->key.keysym.scancode) {
				case SDL_SCANCODE_UP:
					cman_key |= CMAN_UP;
					break;
				case SDL_SCANCODE_RIGHT:
					cman_key |= CMAN_RIGHT;
					break;
				case SDL_SCANCODE_DOWN:
					cman_key |= CMAN_DOWN;
					break;
				case SDL_SCANCODE_LEFT:
					cman_key |= CMAN_LEFT;
					break;
				case SDL_SCANCODE_Q:
					g_main_loop_quit(loop);
					break;
				default:
					break;
			}
		}
	} else if (event->type == SDL_KEYUP) {
		switch (event->key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				cman_key &= ~CMAN_UP;
				break;
			case SDL_SCANCODE_RIGHT:
				cman_key &= ~CMAN_RIGHT;
				break;
			case SDL_SCANCODE_DOWN:
				cman_key &= ~CMAN_DOWN;
				break;
			case SDL_SCANCODE_LEFT:
				cman_key &= ~CMAN_LEFT;
				break;
			default:
				break;
		}
	}

	fprintf(stderr, "New event!\n");

	return TRUE;
}

static gint64 sprite_action_cb(RobotSprite *self, gint64 now, gpointer userptr)
{
	guint w, h;
	gint x, y;

	(void)userptr;

	w = robot_sprite_get_width(sprite);
	h = robot_sprite_get_height(sprite);
	robot_sprite_get_position(sprite, &x, &y);

	if (move == 0) {
		if (x + w >= WIDTH) {
			move = 1;
			y += speed;
		} else {
			x += speed;
		}
	} else if (move == 1) {
		if (y + h >= HEIGHT) {
			move = 2;
			x -= speed;
		} else {
			y += speed;
		}
	} else if (move == 2) {
		if (x > 1) {
			x -= speed;
		} else {
			move = 3;
			x -= speed;
		}
	} else {
		if (y > 1) {
			y -= speed;
		} else {
			move = 0;
			x += speed;

			speed += acceleration;
			if (speed > MAX_SPEED) {
				acceleration = -acceleration;
				speed = MAX_SPEED;
			}

			if (speed < 2) {
				acceleration = -acceleration;
				speed = 2;
			}
		}
	}
	robot_sprite_set_position(sprite, x, y);
	robot_sprite_rotate(sprite, 1);

	return now + 30000;
}

static gint64 man_action_cb(RobotSprite *self, gint64 now, gpointer userptr)
{
	if (man_state == 0) { /* Walk right */
		man_frame++;
		robot_sprite_move(man, 3, 0);
		if (robot_sprite_get_x(man) > (int)(WIDTH - 60)) {
			man_state = 1;
			robot_sprite_set_mode(man, man_rotate);
			man_frame = 0;
		}
	} else if (man_state == 1) { /* rotate */
		if (man_frame == robot_sprite_get_frames_count(man) - 1) {
			man_state = 2;
			robot_sprite_set_mode(man, man_walk);
			man_frame = 0;
		}
		man_frame++;
	} else if (man_state == 2) { /* walk left */
		man_frame++;
		robot_sprite_move(man, -3, 0);
		if (robot_sprite_get_x(man) < 30) {
			man_state = 3;
			robot_sprite_set_mode(man, man_rotate);
			man_frame = 0;
		}
	} else { /* rotate */
		if (man_frame == robot_sprite_get_frames_count(man) - 1) {
			man_state = 0;
			robot_sprite_set_mode(man, man_walk);
			man_frame = 0;
		}
		man_frame++;
	}
	robot_sprite_set_frame(man, man_frame);

	return now + 30000;
}


static gint64 cman_action_cb(RobotSprite *self, gint64 now, gpointer userptr)
{
	gint x, y;
	gint dx, dy;

	if (cman_key) {
		robot_sprite_get_position(cman, &x, &y);

		dx = dy = 0;
		if (cman_key & CMAN_UP)    dy -= 3;
		if (cman_key & CMAN_RIGHT) dx += 3;
		if (cman_key & CMAN_DOWN)  dy += 3;
		if (cman_key & CMAN_LEFT)  dx -= 3;
		x += dx; y += dy;

		if (dx > 0)
			cman_frame++;
		else if (dx < 0)
			cman_frame--;

		if (x < 10) x = 10;
		if (x > (int)WIDTH - 80) x = WIDTH - 80;
		if (y < 10) y = 10;
		if (y > (int)HEIGHT - 120) y = HEIGHT - 120;

		robot_sprite_set_position(cman, x, y);
		robot_sprite_set_frame(cman, cman_frame);
	}

	return now + 30000;
}

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

	man_walk = g_quark_from_string("walk");
	man_rotate = g_quark_from_string("rotate");

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
	x_sdl_add_source(loop, event_cb, NULL, NULL);
	scene = robot_scene_new_full(loop);

	sprite = robot_sprite_new();
	man = robot_sprite_new();
	cman = robot_sprite_new();
	robot = robot_robot_new();
	robot_sprite_load_from_file(sprite, renderer, "img/test.png", NULL);
	robot_sprite_load_from_file(man, renderer, "img/man.ini", NULL);
	robot_sprite_load_from_file(cman, renderer, "img/man.ini", NULL);
	if (!robot_robot_load_from_file(robot, renderer, "img/robot.ini", &error)) {
		fprintf(stderr, "Error: can't load robot :(\n");
		fprintf(stderr, "[%s]\n", error->message);
		return -1;
	}
	robot_sprite_set_position(man, 25, HEIGHT / 2);
	robot_sprite_set_position(cman, WIDTH / 2, HEIGHT / 2);
	robot_sprite_set_position(robot, WIDTH / 4, HEIGHT / 4);

	robot_sprite_set_action(sprite, sprite_action_cb);
	robot_sprite_set_action(man, man_action_cb);
	robot_sprite_set_action(cman, cman_action_cb);

	robot_scene_add_drawable(scene, ROBOT_IDRAWABLE(sprite), 0, NULL, NULL);
	robot_scene_add_drawable(scene, ROBOT_IDRAWABLE(man), 0, NULL, NULL);
	robot_scene_add_drawable(scene, ROBOT_IDRAWABLE(cman), 0, NULL, NULL);
	robot_scene_add_drawable(scene, ROBOT_IDRAWABLE(robot), 0, NULL, NULL);

	/* Creating main loop: */
	g_timeout_add(30, timeout_cb, NULL);

	g_main_loop_run(loop);

	g_clear_object(&scene);
	g_clear_object(&man);
	g_clear_object(&cman);
	g_clear_object(&sprite);

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

