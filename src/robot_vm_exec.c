#include "robot.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static char* (*rl_cb)(const char* prompt) = NULL;
static char *rl(const char *prompt)
{
	static char buf[512];

	if (rl_cb) {
		return rl_cb(prompt);
	}

	printf("%s", prompt);
	fflush(stdout);
	return fgets(buf, sizeof(buf), stdin);
}

static unsigned r32(const unsigned char *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

int main(int argc, char *argv[])
{
	RobotObjFile *obj;
	RobotVM *vm;
	GError *error = NULL;
	GByteArray *data;
	FILE *f = NULL;
	unsigned char buf[256];
	char instr[256];
	int l;
	int i, j;

	GOptionContext *optctx;
	gboolean stop = FALSE;
	gboolean debug = FALSE;
	gint mem = 0x10000;
	gboolean readline = FALSE;

	GOptionEntry options[] = {
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "enable debug mode", "yes" },
		{ "readline", 'l', 0, G_OPTION_ARG_NONE, &readline, "try to use readline", "yes" },
		{ "memory", 'm', 0, G_OPTION_ARG_INT, &mem, "memory size in kilobytes", "M" },

		{ NULL }
	};

	optctx = g_option_context_new("- run robot_vm p-code file");
	g_option_context_add_main_entries(optctx, options, "robot_vm");
	g_option_context_set_help_enabled(optctx, TRUE);

	if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
		fprintf(stderr, "Error: %s\n", error->message);
		return EXIT_FAILURE;
	}
	g_option_context_free(optctx);

	if (argc < 2) {
		fprintf(stderr, "Error: no input files!\n");
		return EXIT_FAILURE;
	}

	/* Check parameters: */
	if (mem < 0)
		mem = -mem;

	f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "Error: can't open file `%s'\n", strerror(errno));
		return EXIT_FAILURE;
	}

	data = g_byte_array_new();
	for (;;) {
		l = fread(buf, 1, sizeof(buf), f);
		if (l < 0) {
			fprintf(stderr, "Error: can't read from file `%s'\n", strerror(errno));
			return EXIT_FAILURE;
		}

		if (l == 0)
			break;

		g_byte_array_append(data, buf, l);
	}
	fclose(f);

	obj = robot_obj_file_new();
	if (!robot_obj_file_from_byte_array(obj, data, &error)) {
		fprintf(stderr, "Error: can't parse file `%s'\n", error->message);
		return EXIT_FAILURE;
	}
	g_byte_array_unref(data);

	if (mem <= (int)(obj->text->len + obj->data->len + obj->SS + 0x1000)) {
		fprintf(stderr, "WARNING: memory size is too small!\n");
		mem = obj->text->len + obj->data->len + obj->SS + 0x1000;
		fprintf(stderr, "I will use mem = %d\n", mem);
	}

	vm = robot_vm_new();
	robot_vm_allocate_memory(vm, mem);

	if (!robot_vm_load(vm, obj, &error)) {
		fprintf(stderr, "Error: can't load file into VM `%s'\n", error->message);
		return EXIT_FAILURE;
	}
	g_object_unref(obj);

	if (debug) {
		char *cmd;

		while (!stop) {
			if (!robot_vm_step(vm, &stop, &error)) {
				fprintf(stderr, "Error: execution fault `%s'\n", error->message);
				return EXIT_FAILURE;
			}
			if (vm->R[0] + 4 < vm->memory->len) {
				robot_instruction_to_string(vm->memory->data + vm->R[0], instr, sizeof(instr));
			} else {
				strcpy(instr, "${OUT OF MEMORY}$");
			}

			for (j = 0; j < 31; j++) {
				printf("R%d=[%08x] ", j, vm->R[j]);
				if (j && j % 8 == 0)
					printf("\n");
			}
			if (vm->R[1] + 32 < vm->memory->len) {
				RobotVMWord T = vm->R[1];
				printf("$ ");
				i = 0;

				while (i < 8 && T < obj->SS) {
					printf("%08x ", r32(vm->memory->data + T));
					++i;
					T += 4;
				}
				printf("\n");
			}
			cmd = rl("> ");
			/* TODO: parse command, show something... */
			if (!strcmp(cmd, "quit\n") || !strcmp(cmd, "exit\n")) {
				fprintf(stderr, "Stopped by user.\n");
				stop = TRUE;
			}
		}
	} else {
		if (!robot_vm_exec(vm, &error)) {
			fprintf(stderr, "Error: execution fault `%s'\n", error->message);
			return EXIT_FAILURE;
		}
	}

	g_object_unref(vm);

	return 0;
}

