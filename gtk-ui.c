
#include <gtk/gtk.h>

#include <pidgin/gtkplugin.h>

GtkWidget* gotrg_ui_create_conf_widget(PurplePlugin *plugin)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
	GtkWidget *notebook = gtk_notebook_new();
	GtkWidget *configbox = gtk_vbox_new(FALSE, 5);
	GtkWidget *fingerprintbox = gtk_vbox_new(FALSE, 5);

//	gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
	gtk_container_set_border_width(GTK_CONTAINER(configbox), 5);
	gtk_container_set_border_width(GTK_CONTAINER(fingerprintbox), 5);

	//TODO

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), configbox, gtk_label_new("Config"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), fingerprintbox, gtk_label_new("Known fingerprints"));

	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);
	return vbox;
}
