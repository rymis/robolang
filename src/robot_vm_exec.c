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
	int i;

	GOptionContext *optctx;
	gboolean stop = FALSE;
	gboolean debug = FALSE;
	gint mem = 0x10000;
	gint stack = 0x1000;
	gboolean readline = FALSE;

	GOptionEntry options[] = {
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "enable debug mode", "yes" },
		{ "readline", 'l', 0, G_OPTION_ARG_NONE, &readline, "try to use readline", "yes" },
		{ "memory", 'm', 0, G_OPTION_ARG_INT, &mem, "memory size in kilobytes", "M" },
		{ "stack", 's', 0, G_OPTION_ARG_INT, &stack, "stack size in kilobytes", "S" },

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
	if (stack < 0)
		stack = -stack;
	if (!stack)
		stack = 100;

	if (mem <= stack + 0x1000) {
		fprintf(stderr, "WARNING: can't make stack so big!\n");
		mem = stack + 0x8000;
		fprintf(stderr, "I will use: stack = %d, mem = %d\n", stack, mem);
	}

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

	vm = robot_vm_new();
	robot_vm_allocate_memory(vm, mem, stack);

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
			if (vm->PC < vm->memory->len) {
				robot_instruction_to_string(vm->memory->data[vm->PC], instr, sizeof(instr));
			} else {
				strcpy(instr, "${OUT OF MEMORY}$");
			}
			printf("PC=[%08x] %10s T=[%08x] A=[%08x] B=[%08x] C=[%08x]\n", vm->PC, instr, vm->T, vm->A, vm->B, vm->C);
			if (vm->T + 32 < vm->memory->len) {
				RobotVMWord T = vm->T;
				printf("$ ");
				i = 0;

				while (i < 8 && T < vm->stack_end) {
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

