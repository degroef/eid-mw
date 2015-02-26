#include <config.h>
#include <gtk/gtk.h>
#include <assert.h>

#include "oslayer.h"
#include "viewer_glade.h"
#include "gettext.h"
#include "binops.h"
#include "gtk_globals.h"

#include "gtkui.h"
#include "thread.h"
#include "photo.h"

#ifndef _
#define _(s) gettext(s)
#endif

typedef void(*bindisplayfunc)(void*, int);
typedef void(*clearfunc)(char*);

static GHashTable* binhash;

static void uilog(enum eid_vwr_loglevel l, char* line) {
	g_message("level %d: %s", (int)l, line);
}

static void stringclear(char* l) {
	GtkLabel* label = GTK_LABEL(gtk_builder_get_object(builder, l));
	// Should only appear in the hash table if we successfully found it
	// earlier...
	assert(label != NULL);
	g_object_set_threaded(G_OBJECT(label), "label", "-", FALSE);
	g_object_set_threaded(G_OBJECT(label), "sensitive", FALSE, FALSE);
}

static void newstringdata(char* l, char* data) {
	GtkLabel* label = GTK_LABEL(gtk_builder_get_object(builder, l));
	if(!label) {
		char* msg = g_strdup_printf(_("Could not display label '%s', data '%s': no GtkLabel found for data"), l, data);
		uilog(EID_VWR_LOG_DETAIL, msg);
		g_free(msg);
		return;
	}
	g_hash_table_insert(touched_labels, g_strdup(l), stringclear);
	g_object_set_threaded(G_OBJECT(label), "label", g_strdup(data), g_free);
	g_object_set_threaded(G_OBJECT(label), "sensitive", (void*)TRUE, NULL);
}

static void newbindata(char* label, void* data, int datalen) {
	bindisplayfunc func;
	gchar* msg = g_strdup_printf("found label %s with data length %d", label, datalen);

	uilog(EID_VWR_LOG_DETAIL, msg);
	free(msg);
	if(!g_hash_table_contains(binhash, label)) {
		msg = g_strdup_printf(_("Could not display binary data with label '%s': not found in hashtable"), label);
		uilog(EID_VWR_LOG_DETAIL, msg);
		free(msg);
		return;
	}
	func = (bindisplayfunc)g_hash_table_lookup(binhash, label);
	func(data, datalen);
	return;
}

static void cleardata(gpointer key, gpointer value, gpointer user_data G_GNUC_UNUSED) {
	clearfunc func = (clearfunc)value;
	char* k = key;
	func(k);
}

static void newsrc(enum eid_vwr_source src) {
	g_hash_table_foreach(touched_labels, cleardata, NULL);
	// TODO: update display so we see which source we're using
}

static gboolean poll(gpointer user_data G_GNUC_UNUSED) {
	eid_vwr_poll();
	return TRUE;
}

static void* threadmain(void* data G_GNUC_UNUSED) {
	eid_vwr_be_mainloop();
}

int main(int argc, char** argv) {
	GtkWidget *window;
	GObject* signaltmp;
	GtkAccelGroup *group;
	struct eid_vwr_ui_callbacks* cb;
	pthread_t thread;

	bindtextdomain("eid-viewer", DATAROOTDIR "/locale");
	textdomain("eid-viewer");

	gtk_init(&argc, &argv);
	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, VIEWER_GLADE_STRING, strlen(VIEWER_GLADE_STRING), NULL);

	window = GTK_WIDGET(gtk_builder_get_object(builder, "mainwin"));
	group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), group);

	touched_labels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	binhash = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_insert(binhash, "PHOTO_FILE", displayphoto);

	cb = eid_vwr_cbstruct();
	cb->newsrc = newsrc;
	cb->newstringdata = newstringdata;
	cb->newbindata = newbindata;
	cb->log = uilog;
	eid_vwr_createcallbacks(cb);

	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_open"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(file_open), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_saveas_xml"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(file_save), "xml");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_saveas_csv"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(file_save), "csv");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_close"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(file_close), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_prefs"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(file_prefs), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_print"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(file_print), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_file_quit"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(gtk_main_quit), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_lang_de"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(translate), "de");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_lang_en"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(translate), "en");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_lang_fr"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(translate), "fr");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_lang_nl"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(translate), "nl");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_help_about"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(showabout), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_help_faq"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(showurl), "faq");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_help_test"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(showurl), "test");
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "mi_help_log"));
	g_signal_connect(signaltmp, "activate", G_CALLBACK(showlog), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "pintestbut"));
	g_signal_connect(signaltmp, "clicked", G_CALLBACK(testpin), NULL);
	signaltmp = G_OBJECT(gtk_builder_get_object(builder, "pinchangebut"));
	g_signal_connect(signaltmp, "clicked", G_CALLBACK(changepin), NULL);

	pthread_create(&thread, NULL, threadmain, NULL);

	gtk_widget_show_all(window);

	gtk_main();
}