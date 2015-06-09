#include "robot.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	RobotObjFile *obj;
	RobotVM *vm;
	GError *error = NULL;
	GByteArray *data;
	FILE *f = NULL;
	unsigned char buf[256];
	int l;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		return EXIT_FAILURE;
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
	robot_vm_allocate_memory(vm, 0x10000, 0x1000);

	if (!robot_vm_load(vm, obj, &error)) {
		fprintf(stderr, "Error: can't load file into VM `%s'\n", error->message);
		return EXIT_FAILURE;
	}
	g_object_unref(obj);

	gboolean stop = FALSE;
	while (!stop) {
		if (!robot_vm_step(vm, &stop, &error)) {
			fprintf(stderr, "Error: execution fault `%s'\n", error->message);
			return EXIT_FAILURE;
		}
		printf("[%08x] [%08x] [%08x] [%08x] [%08x]\n", vm->PC, vm->T, vm->A, vm->B, vm->C);
		getchar();
	}

	g_object_unref(vm);

	return 0;
}

