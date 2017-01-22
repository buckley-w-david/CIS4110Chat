#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
 
typedef struct text_vars {
   GtkTextView* history;
   GtkEntry* input;
} Widgets;

G_MODULE_EXPORT void send_message(GtkButton* button, gpointer user_data);
G_MODULE_EXPORT void on_chat_gui_destroy();

int main(int argc, char *argv[])
{
    GtkBuilder* builder; 
    GtkWidget*  window;
    Widgets* widg = malloc(sizeof(Widgets));

    gtk_init(&argc, &argv);
 
    builder = gtk_builder_new();
    gtk_builder_add_from_file (builder, "chat_gui.glade", NULL);
 
    window = GTK_WIDGET(gtk_builder_get_object(builder, "chat_gui"));
    widg->history = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "history"));
    widg->input = GTK_ENTRY(gtk_builder_get_object(builder, "message_input"));


    gtk_builder_connect_signals(builder, widg);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 600);

    gtk_widget_show(window);                
    gtk_main();
 
    return 0;
}
 
// called when window is closed
G_MODULE_EXPORT void on_chat_gui_destroy()
{
    gtk_main_quit();
}

G_MODULE_EXPORT void send_message(GtkButton* button, gpointer user_data)
{
    Widgets* point = user_data;
    const gchar* entry_text = gtk_entry_get_text(point->input);
    printf("%s\n", entry_text);
    printf("Message sent\n");
    gtk_entry_set_text(point->input, "");
}

G_MODULE_EXPORT void on_server_conn_activate()
{
    printf("Connecting to server...\n");
    //spawn thread
}

G_MODULE_EXPORT void on_server_diss_activate()
{
    printf("Disconnecting to server...\n");
    //kill thread
}

G_MODULE_EXPORT void on_about_activate()
{
    printf("Showing about...\n");
}