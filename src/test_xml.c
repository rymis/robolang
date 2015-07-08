#include "robot_xml.h"
#include <stdio.h>

static char s_xml[] =
		"<?xml version=\"1.0\"?>\n"
		"<book>\n"
		"<tag1 a1=\"v1\" a2=\"v2\">\n"
		"<tt1/>\n"
		"<tt2 xxx=\"yyy\">test</tt2>\n"
		"</tag1>\n"
		"</book>\n";


int main(int argc, char *argv[])
{
	RobotXml *xml;
	GError *error = NULL;
	gchar *s;

	xml = robot_xml_parse(s_xml, sizeof(s_xml) - 1, &error);
	if (!xml) {
		fprintf(stderr, "Error: %s\n", error? error->message: "No error message");
		return 1;
	}

	s = robot_xml_to_string(xml, TRUE, &error);
	if (!s) {
		fprintf(stderr, "Error: %s\n", error? error->message: "No error message");
		return 1;
	}

	printf("XML:\n%s\n", s);

	g_object_unref(xml);
	g_free(s);

	return 0;
}
