/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2003 Timothy Robinson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <gtk/gtk.h>

#include <stdlib.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define APPCONFIG_FILENAME "/.wavbreaker"

static GtkWidget *window;

/* Output directory for wave files. */
static char *outputdir = NULL;
static GtkWidget *outputdir_entry = NULL;

/* Device file for audio output. */
static char *outputdev = NULL;
static GtkWidget *outputdev_entry = NULL;

/* Config Filename */
static char *configfilename = NULL;

/* function prototypes */
static int appconfig_write_file();

char *get_outputdir()
{
	return outputdir;
}

void set_outputdir(const char *val)
{
	if (outputdir != NULL) {
		g_free(outputdir);
	}
	outputdir = g_strdup(val);
}

char *get_outputdev()
{
	return outputdev;
}

void set_outputdev(const char *val)
{
	if (outputdev != NULL) {
		g_free(outputdev);
	}
	outputdev = g_strdup(val);
}

char *get_configfilename()
{
	return configfilename;
}

void set_configfilename(const char *val)
{
	if (configfilename != NULL) {
		g_free(configfilename);
	}
	configfilename = g_strdup(val);
}

static void appconfig_hide(GtkWidget *main_window)
{
	gtk_widget_destroy(main_window);
}

static void filesel_ok_clicked(GtkWidget *widget, gpointer user_data)
{
	GtkFileSelection *filesel = GTK_FILE_SELECTION(user_data);

	gtk_entry_set_text(GTK_ENTRY(outputdir_entry), gtk_file_selection_get_filename(filesel));

	gtk_widget_destroy(user_data);
}

static void filesel_cancel_clicked(GtkWidget *widget, gpointer user_data)
{
	gtk_widget_destroy(user_data);
}

static void browse_button_clicked(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *filesel;

	filesel = gtk_file_selection_new("select output directory");
	gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(filesel), GTK_WINDOW(window));
	gtk_window_set_type_hint(GTK_WINDOW(filesel), GDK_WINDOW_TYPE_HINT_DIALOG);

	gtk_signal_connect(GTK_OBJECT( GTK_FILE_SELECTION(filesel)->ok_button),
		"clicked", (GtkSignalFunc)filesel_ok_clicked, filesel);

	gtk_signal_connect(GTK_OBJECT( GTK_FILE_SELECTION(filesel)->cancel_button),
		"clicked", (GtkSignalFunc)filesel_cancel_clicked, filesel);

	gtk_widget_show(filesel);
}

static void cancel_button_clicked(GtkWidget *widget, gpointer user_data)
{
	appconfig_hide(user_data);
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
	set_outputdir(gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));
	set_outputdev(gtk_entry_get_text(GTK_ENTRY(outputdev_entry)));
	appconfig_hide(GTK_WIDGET(user_data));
	appconfig_write_file();
}

void appconfig_show(GtkWidget *main_window)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *hbbox;
	GtkWidget *outputdir_label;
	GtkWidget *outputdev_label;
	GtkWidget *hseparator;
	GtkWidget *ok_button, *cancel_button;
	GtkWidget *browse_button;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(window);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
	gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_widget_show(vbox);

	table = gtk_table_new(3, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), table);
	gtk_widget_show(table);

	outputdir_label = gtk_label_new("Wave File Output Directory:");
	gtk_misc_set_alignment(GTK_MISC(outputdir_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), outputdir_label, 0, 1, 0, 1, GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdir_label);

	outputdir_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(outputdir_entry), outputdir);
	gtk_table_attach(GTK_TABLE(table), outputdir_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdir_entry);

	browse_button = gtk_button_new_with_label("Browse");
	gtk_table_attach(GTK_TABLE(table), browse_button, 2, 3, 0, 1, GTK_FILL, 0, 5, 0);
	g_signal_connect(G_OBJECT(browse_button), "clicked", (GtkSignalFunc)browse_button_clicked, window);
	gtk_widget_show(browse_button);

	outputdev_label = gtk_label_new("Audio Device:");
	gtk_misc_set_alignment(GTK_MISC(outputdev_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), outputdev_label, 0, 1, 1, 2, GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdev_label);

	outputdev_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(outputdev_entry), outputdev);
	gtk_table_attach(GTK_TABLE(table), outputdev_entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdev_entry);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
	gtk_widget_show(hseparator);

	hbbox = gtk_hbutton_box_new();
	gtk_container_add(GTK_CONTAINER(vbox), hbbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox), 10);
	gtk_widget_show(hbbox);

	cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_end(GTK_BOX(hbbox), cancel_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(cancel_button), "clicked", (GtkSignalFunc)cancel_button_clicked, window);
	gtk_widget_show(cancel_button);

	ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(hbbox), ok_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(ok_button), "clicked", (GtkSignalFunc)ok_button_clicked, window);
	gtk_widget_show(ok_button);

	gtk_widget_show(window);
}

static int appconfig_read_file() {
	xmlDocPtr doc;
	xmlNodePtr cur;

	doc = xmlParseFile(get_configfilename());

    if (doc == NULL) {
        fprintf(stderr, "Document not parsed successfully.\n");
        return 1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL) {
        fprintf(stderr, "empty document\n");
        xmlFreeDoc(doc);
        return 2;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "wavbreaker")) {
        fprintf(stderr, "wrong document type, root node != wavbreaker\n");
        xmlFreeDoc(doc);
        return 3;
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
		xmlChar *key;

        if (!(xmlStrcmp(cur->name, (const xmlChar *) "outputdev"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			set_outputdev(key);
			xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "outputdir"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			set_outputdir(key);
			xmlFree(key);
		}
        cur = cur->next;
    }

    return 0;
}

static int appconfig_write_file() {
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr cur;

	doc = xmlNewDoc((const xmlChar *)"1.0");

	if (doc == NULL) {
		fprintf(stderr, "Document not created successfully.\n");
		return 1;
	}

	root = xmlNewDocNode(doc, NULL, (const xmlChar *)"wavbreaker", "");

	if (root == NULL) {
		fprintf(stderr, "error creating doc node\n");
		xmlFreeDoc(doc);
		return 2;
	}

	cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdir", (const xmlChar *) get_outputdir());

	if (cur == NULL) {
		fprintf(stderr, "error creating outputdir node\n");
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 3;
	}

	cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdev", (const xmlChar *) get_outputdev());

	if (cur == NULL) {
		fprintf(stderr, "error creating outputdev node\n");
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 3;
	}

	root = xmlDocSetRootElement(doc, root);
	/*
	if (root == NULL) {
		fprintf(stderr, "error setting doc root node\n");
		xmlFreeDoc(doc);
		return 4;
	}
	*/

	xmlIndentTreeOutput = 1;
	xmlKeepBlanksDefault(0);
	if (! xmlSaveFormatFile(get_configfilename(), doc, 1)) {
		fprintf(stderr, "error writing config file: %s", get_configfilename());
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 5;
	}

	xmlFreeNodeList(root);
	xmlFreeDoc(doc);

	return 0;
}

void appconfig_init()
{
	char str[1024];

	str[0] = '\0';
	strcat(str, getenv("HOME"));
	strcat(str, APPCONFIG_FILENAME);
	configfilename = g_strdup(str);

	if (access(get_configfilename(), W_OK | F_OK)) {
		outputdir = g_strdup(getenv("PWD"));
		outputdev = g_strdup("/dev/dsp");
		appconfig_write_file();
	}

	appconfig_read_file();
}

