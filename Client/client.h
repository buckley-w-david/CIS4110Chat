#ifndef __CLIENTH
#define __CLIENTH

#define _POSIX_C_SOURCE 200112L

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>

#include <gtk/gtk.h>

#include <pthread.h>
#include <error.h>

typedef struct text_vars {
   GtkTextView* history;
   GtkEntry* input;
} Widgets;

typedef struct history_update {
	GtkTextView* history;
	char* new_text;
	char* id;
	int id_size;
}h_update;

typedef struct con {
	uint32_t s_addr;
	int fd;
} Connection;


int handshake(int sockfd);
G_MODULE_EXPORT void on_chat_gui_destroy();
G_MODULE_EXPORT void send_message(GtkButton* button, gpointer user_data);
G_MODULE_EXPORT void on_server_conn_activate();
G_MODULE_EXPORT void on_server_diss_activate();
G_MODULE_EXPORT void on_about_activate();

#endif
