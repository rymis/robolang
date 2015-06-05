/* Assembler for RobotVM */

#include "robot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void usage(const char *prog);
static GByteArray* load_file(const char *filename, GError **error);
static gboolean save_file(const char *filename, GByteArray *data, GError **error);

int main(int argc, char *argv[])
{
	const char *input = NULL;
	const char *output = NULL;
	char *outbuf;
	int i;
	GByteArray *data;
	GError *error = NULL;
	RobotObjFile *obj = NULL;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
			++i;
			if (!argv[i]) {
				fprintf(stderr, "Error: -o needs argument.\n");
				return EXIT_FAILURE;
			}
			output = argv[i];
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

	if (!output) {
		int l = strlen(input);
		outbuf = g_malloc(l + 4);
		strcpy(outbuf, input);
		if (l > 2 && input[l - 1] == 's' && input[l - 2] == '.') {
			outbuf[l - 1] = 'o';
		} else {
			strcat(outbuf, ".o");
		}
		output = outbuf;
	}

	data = load_file(input, &error);
	if (!data) {
		fprintf(stderr, "Error: can't load file (%s)\n", error->message);
		return EXIT_FAILURE;
	}

	obj = robot_obj_file_new();
	if (!robot_obj_file_compile(obj, (char*)data->data, &error)) {
		fprintf(stderr, "Error: compilation failed (%s)\n", error->message);
		return EXIT_FAILURE;
	}

	g_byte_array_unref(data);
	data = robot_obj_file_to_byte_array(obj, &error);
	if (!data) {
		fprintf(stderr, "Error: compilation failed (%s)\n", error->message);
		return EXIT_FAILURE;
	}

	if (!save_file(output, data, &error)) {
		fprintf(stderr, "Error: can't save file (%s)\n", error->message);
		return EXIT_FAILURE;
	}

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

	b = 0;
	g_byte_array_append(res, &b, 1);

	fclose(f);

	return res;
}

static gboolean save_file(const char *filename, GByteArray *data, GError **error)
{
	FILE *f;

	f = fopen(filename, "wb");

	if (!f || fwrite(data->data, data->len, 1, f) != 1) {
		if (f)
			fclose(f);
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_IO, strerror(errno));
		return FALSE;
	}

	fclose(f);

	return TRUE;
}

