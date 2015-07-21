#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *source = NULL;

static void create_window(void)
{
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window);
}

int main(int argc, char *argv[])
{
	gtk_init(&argc, &argv);

	create_window();

	gtk_main();

	return 0;
}
