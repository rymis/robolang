/* objdump for RobotVM */

#include "robot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void usage(const char *prog);
static GByteArray* load_file(const char *filename, GError **error);
static void dump_obj(RobotObjFile *obj, int disasm);

int main(int argc, char *argv[])
{
	const char *input = NULL;
	int disasm = 0;
	int i;
	GByteArray *data;
	GError *error = NULL;
	RobotObjFile *obj = NULL;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--disassembler")) {
			++disasm;
		} else if (!strcmp(argv[i], "--")) {
			++i;
			break;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage(argv[0]);
			return EXIT_SUCCESS;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Error: unknown option `%s'\n", argv[i]);
			return EXIT_FAILURE;
		} else {
			input = argv[i];
		}
	}

	if (i < argc) {
		/* TODO: process all arguments??? */
		input = argv[i];
	}

	if (!input) {
		fprintf(stderr, "Error: no input files.\n");
		return EXIT_FAILURE;
	}

	data = load_file(input, &error);
	if (!data) {
		fprintf(stderr, "Error: can't load file (%s)\n", error->message);
		return EXIT_FAILURE;
	}

	obj = robot_obj_file_new();
	if (!robot_obj_file_from_byte_array(obj, data, &error)) {
		fprintf(stderr, "Error: parse failed (%s)\n", error->message);
		return EXIT_FAILURE;
	}

	g_byte_array_unref(data);

	dump_obj(obj, disasm);

	g_object_unref(obj);

	return 0;
}

static void usage(const char *prog)
{
	printf("%s: assembler for RobotVM.\n", prog);
	printf("Usage: %s [-o output] input1 ...\n", prog);
}

static GByteArray* load_file(const char *filename, GError **error)
{
	GByteArray* res = g_byte_array_new();
	int c;
	unsigned char b;
	FILE *f;

	f = fopen(filename, "rb");
	if (!f) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_IO, strerror(errno));
		g_byte_array_unref(res);
		return NULL;
	}

	while ((c = getc(f)) != EOF) {
		b = c;
		g_byte_array_append(res, &b, 1);
	}

	fclose(f);

	return res;
}

static void dump_obj(RobotObjFile *obj, int disasm)
{
	robot_obj_file_dump(obj, stdout, disasm, NULL);
}

