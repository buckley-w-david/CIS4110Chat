
#include "client.h"
#include "network_functions.h"

#define MESSAGE_PORT 5000
#define CONNECTION_PORT 5001
#define BUFFERSIZE 1025
 
pthread_t message_thread;
int connected = 0;

pthread_mutex_t socket_lock;
pthread_mutex_t connection_lock;

int sockfd_conn = -1;
int sockfd_msg = -1;

int sockfd_msg_interrupt[2];

struct sockaddr_in server_conn;
struct sockaddr_in server_msg;

int error_exit(char* message) {
    gtk_main_quit();

    close(sockfd_msg);
    close(sockfd_conn);

    pthread_cancel(message_thread);

    pthread_mutex_destroy(&socket_lock);
    pthread_mutex_destroy(&connection_lock);

    fprintf(stderr, "%s\n", message);
    exit(1);
}

void sever_connection() {
    printf("Disconnecting from server...\n");

    write(sockfd_msg_interrupt[1], "close", 5); //Will cause message listener to kill itself
    write(sockfd_conn, "DISCONNECT", 10);
    //close(sockfd_conn);

    close(sockfd_conn);
    close(sockfd_msg);

    connected = 0;

    sockfd_conn = -1;
    sockfd_msg = -1;
}

int main(int argc, char *argv[])
{
    GtkBuilder* builder; 
    GtkWidget*  window;
    Widgets* widg = malloc(sizeof(Widgets));

    pipe(sockfd_msg_interrupt);

    if (pthread_mutex_init(&socket_lock, NULL) != 0) {
        printf("mutex init failed");
        return 1;
    }

    if (pthread_mutex_init(&connection_lock, NULL) != 0) {
        printf("mutex init failed");
        return 1;
    }

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

int handshake(int sockfd) {
    char recvBuff[BUFFERSIZE];
    int n;

    n = read(sockfd, recvBuff, sizeof(recvBuff)-1);

    recvBuff[n] = 0;
    if(n < 0) {
       error_exit("Read Error");
    }
    printf("\"%s\" - Recieved\n", recvBuff);

    if (strncmp(recvBuff, "CONFIRM", 7) == 0) {
        printf("Confirming\n");
        write(sockfd, "CONNECT", 7);
        return 1;
    } else {
        printf("Not confirmed\n");
        return 0;
    }
}

int add_line_to_history(gpointer data) {
    time_t timer;
    char time_buffer[26];
    char hist_buffer[BUFFERSIZE+100];
    struct tm* tm_info;

    h_update* history_update = (h_update*)data;
    printf("Adding %s to history\n", history_update->new_text);

    time(&timer);
    tm_info = localtime(&timer);

    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    strncpy(hist_buffer, "From Anonymous at ", 18);
    strncat(hist_buffer, time_buffer, strlen(time_buffer));
    strncat(hist_buffer, ":\n", 2);
    strncat(hist_buffer, history_update->new_text, BUFFERSIZE);
    strncat(hist_buffer, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", 39);

    GtkTextView* textview;
    GtkTextIter ei;
    GtkTextMark* mk;
    gchar* sendme = hist_buffer;
    GtkTextBuffer* buff;

    //get the textview
    textview = GTK_TEXT_VIEW(history_update->history);
    buff = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    gtk_text_buffer_get_end_iter(buff, &ei);

    //append the message
    gtk_text_buffer_insert(buff, &ei, sendme, -1);
    mk = gtk_text_buffer_get_mark (buff, "insert");
    gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (textview), mk, 0.0, FALSE, 0.0, 0.0);

    return 0;
}

void* listen_for_new_message(void* history) {
    fd_set rfds; //FD to listen for new activity on
    char buffer[BUFFERSIZE];
    char to_history[BUFFERSIZE];

    int retval = 0, n = 0, maxfd = -1;
    h_update hist_update;

    hist_update.history = (GtkTextView*)history;

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(sockfd_msg, &rfds);
        FD_SET(sockfd_msg_interrupt[0], &rfds);

        maxfd = (sockfd_msg > sockfd_msg_interrupt[0]) ? sockfd_msg : sockfd_msg_interrupt[0];
        memset(&buffer, 0, sizeof(buffer));
        retval = select(maxfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port

        if (retval == -1) {
            error_exit("ERROR on select");
        }

        if (FD_ISSET(sockfd_msg_interrupt[0], &rfds)) {
            printf("No longer listening for messages\n");
            //close(sockfd_msg);
            return NULL;
        }

        printf("Connection is available - Message\n");
        n = read(sockfd_msg, buffer, BUFFERSIZE);
        printf("read %d chars: %s\n", n, buffer);

        if (n <= 0) {
            printf("Network error occurued, disconnecting from server...\n");
            sever_connection();
            return NULL;
        }
        memset(&to_history, 0, sizeof(to_history));
        strncpy(to_history, buffer, BUFFERSIZE);

        hist_update.new_text = to_history;
        gdk_threads_add_idle(&add_line_to_history, (gpointer)&hist_update);
    }

    return NULL;
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
    //Send entry_text to server if we're connected.
    printf("Status is: %d\n", connected);
    if (connected) {
        write(sockfd_msg, entry_text, strlen(entry_text));
        printf("Message sent\n");
    } else {
        //Not connected message
        printf("Not sending message\n");
    }
    gtk_entry_set_text(point->input, "");
}

G_MODULE_EXPORT void on_server_conn_activate(GtkMenuButton* button, gpointer user_data)
{
    pthread_mutex_lock(&connection_lock);
        if (!connected) {
            int shake = 0, err = 0;
            char server[] = "cis4110chatserver.davidbuckleyprogrammer.me";
            char* restrict server_ip;
            char recvBuff[BUFFERSIZE];

            printf("Connecting to server...\n");

            server_ip = ipv4_dns_resolve(server);
            
            memset(recvBuff, 0,sizeof(recvBuff));

            pthread_mutex_lock(&socket_lock);
                if((sockfd_conn = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    error_exit("Error : Could not create socket");
                }
                if((sockfd_msg = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    error_exit("Error : Could not create socket");
                } 
                
                printf("Sockets created...\n");

                memset(&server_conn, 0, sizeof(server_conn));
                memset(&server_msg, 0, sizeof(server_msg));

                server_conn.sin_family = AF_INET;
                server_conn.sin_port = htons(CONNECTION_PORT);

                server_msg.sin_family = AF_INET;
                server_msg.sin_port = htons(MESSAGE_PORT);

                if(inet_pton(AF_INET, server_ip, &server_conn.sin_addr) <= 0) {
                    error_exit("inet_pton error occured on connection port");
                }
                if(inet_pton(AF_INET, server_ip, &server_msg.sin_addr) <= 0) {
                    error_exit("inet_pton error occured on message port");
                }
                printf("Connected to ip %s...\n", server_ip);
                free(server_ip);


                if(connect(sockfd_conn, (struct sockaddr *)&server_conn, sizeof(server_conn)) < 0) {
                   error_exit("Error : Connect Failed on connection port");
                }

                printf("Doing handshake - CONNECTION_PORT\n");
                shake = handshake(sockfd_conn);
                printf("Done handshake, connection to message port\n");

                if(connect(sockfd_msg, (struct sockaddr *)&server_msg, sizeof(server_msg)) < 0) {
                   error_exit("Error : Connect Failed on message port");
                }

                printf("Msg Socket FD: %d\nConn Socket FD: %d\n", sockfd_msg, sockfd_conn);
            pthread_mutex_unlock(&socket_lock);

            if (!shake) {
                error_exit("Unable to establish link with server");
            }

            connected = 1;

            err = pthread_create(&message_thread, NULL, &listen_for_new_message, ((Widgets*)user_data)->history);
            if (err != 0) {
                error_exit("Can't create thread");
            }
            printf("Connected to server, awaiting messages...\n");
        } else {
            //Already connected
        }
    pthread_mutex_unlock(&connection_lock);
}

G_MODULE_EXPORT void on_server_diss_activate(GtkMenuButton* button, gpointer user_data)
{
    if (connected) {
        sever_connection();
    } else {
        //Not currently connected
    }
}

G_MODULE_EXPORT void on_about_activate()
{
    printf("Showing about...\n");
}