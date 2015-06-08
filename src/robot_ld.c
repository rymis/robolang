/* objdump for RobotVM */

#include "robot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void usage(const char *prog);
static GByteArray* load_file(const char *filename, GError **error);
static void print_depends(RobotObjFile *obj);

static gboolean merge(RobotObjFile *obj, const char *filename, GError **error)
{
	GByteArray *data;
	RobotObjFile *O = NULL;

	data = load_file(filename, error);
	if (!data) {
		return FALSE;
	}

	O = robot_obj_file_new();
	if (!robot_obj_file_from_byte_array(O, data, error)) {
		return FALSE;
	}

	g_byte_array_unref(data);

	if (!robot_obj_file_merge(obj, O, error)) {
		g_object_unref(O);
		return FALSE;
	}

	g_object_unref(O);

	return TRUE;
}

int main(int argc, char *argv[])
{
	int i;
	GError *error = NULL;
	RobotObjFile *obj = NULL;
	const char *output = "a.out";
	int processed = 0;
	int incr = 0;
	GByteArray *data;
	FILE *f;

	obj = robot_obj_file_new();
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
			++i;
			if (!argv[i]) {
				fprintf(stderr, "Error: -o needs argument.\n");
				return EXIT_FAILURE;
			}
			output = argv[i];
		} else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--incremential")) {
			++incr;
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
			if (!merge(obj, argv[i], &error)) {
				fprintf(stderr, "Error: %s\n", error->message);
				return EXIT_FAILURE;
			}
			++processed;
		}
	}

	while (i < argc) {
		if (!merge(obj, argv[i], &error)) {
			fprintf(stderr, "Error: %s\n", error->message);
			return EXIT_FAILURE;
		}
		++i;
		++processed;
	}

	if (!processed) {
		fprintf(stderr, "Error: no input files.\n");
		return EXIT_FAILURE;
	}

	if (!incr) {
		if (robot_obj_file_dependencies_count(obj) > 0) {
			fprintf(stderr, "Unresolved dependencies in file.\n");
			print_depends(obj);
			return EXIT_FAILURE;
		}
	}

	data = robot_obj_file_to_byte_array(obj, &error);
	if (!data) {
		g_object_unref(obj);
		fprintf(stderr, "Error: can't save file: %s\n", error->message);
		return EXIT_FAILURE;
	}
	g_object_unref(obj);

	f = fopen(output, "wb");
	if (!f) {
		fprintf(stderr, "Error: can't open output file: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (fwrite(data->data, data->len, 1, f) != 1) {
		fprintf(stderr, "Error: can't write file: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	fclose(f);

	return 0;
}

static void usage(const char *prog)
{
	printf("%s: linker for RobotVM.\n", prog);
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

static void print_depends(RobotObjFile *obj)
{
	RobotObjFileSymbol *s;
	guint i;

	for (i = 0; i < obj->depends->len; i++) {
		s = &g_array_index(obj->depends, RobotObjFileSymbol, i);
		if (s->name[0] != '%')
			fprintf(stderr, "U [%08x] %s\n", (unsigned)s->addr, s->name);
	}
}

