
#include "client.h"
#include "network_functions.h"

#define MESSAGE_PORT 5000
#define CONNECTION_PORT 5001
#define BUFFERSIZE 1025
 
pthread_t message_thread;
int connected = 0;

pthread_mutex_t socket_lock;

int sockfd_conn;
int sockfd_msg;

struct sockaddr_in server_conn;
struct sockaddr_in server_msg;

int error_exit(char* message) {
    gtk_main_quit();
    fprintf(stderr, "%s\n", message);
    exit(1);
}

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
    h_update* history_update = (h_update*)data;
    GtkTextView* textview;
    GtkTextIter ei;
    GtkTextMark* mk;
    gchar* sendme = history_update->new_text;
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

void* listen_for_new_message(GtkTextView* history) {
    fd_set rfds; //FD to listen for new activity on
    struct sockaddr_in cli_addr;
    socklen_t clilen = 0;
    struct timeval tv;
    char* restrict ip_conn;
    char* restrict ip_server;
    h_update hist_update;
    char buffer[BUFFERSIZE];
    int retval = 0, newsockfd = 0, n = 0;

    hist_update.history = history;

    tv.tv_sec = 7;  /* 7 Secs read Timeout */
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(sockfd_msg, &rfds);
    while (1) {
        memset(&buffer, 0, sizeof(buffer));
        retval = select(sockfd_msg+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port

        switch (retval) {
            case -1: //Something went wrong
                error_exit("ERROR on select");
                break;
            case 1: //Connection was made
                printf("Connection is available\n");
                //Accept the connction
                newsockfd = accept(sockfd_msg, (struct sockaddr *)&cli_addr, &clilen);

                ip_conn = malloc(sizeof(char)*BUFFERSIZE);
                ip_server = malloc(sizeof(char)*BUFFERSIZE);
                inet_ntop(AF_INET, &cli_addr.sin_addr, ip_conn, BUFFERSIZE);
                inet_ntop(AF_INET, &server_addr.sin_addr, ip_server, BUFFERSIZE);

                //Message from someone in the pool
                if (strncmp(ip_conn, ip_server, strlen(ip_server)) == 0) {
                    //Only allowed to wait for ~7 seconds for confirmation message
                    setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
                    //there was some problem accepting this connection
                    if (newsockfd < 0) {
                        error_exit("ERROR on accept");
                    }

                    n = read(newsockfd, buffer, BUFFERSIZE);
                    if (n < 0) { 
                        printf("ERROR reading from socket\n");
                        close(newsockfd);
                        continue;
                    }
                    hist_update.new_text = buffer;

                    gdk_threads_add_idle(&add_line_to_history, (gpointer)&hist_update);
                    printf("%s\n", buffer);
                }
                free(ip_conn);
                free(ip_server);
                
                break;
            default:
                printf("Not sure what happened here");
                break;
        }
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
    if (connected) {
        write(sockfd_msg, entry_text, strlen(entry_text));
        printf("Message sent\n");
    }
    gtk_entry_set_text(point->input, "");
}

G_MODULE_EXPORT void on_server_conn_activate()
{
    if (!connected) {
        int shake = 0;
        char server[] = "cis4110chatserver.davidbuckleyprogrammer.me";
        char* restrict server_ip;
        char recvBuff[BUFFERSIZE];

        printf("Connecting to server...\n");

        server_ip = ipv4_dns_resolve(server);

        memset(recvBuff, 0,sizeof(recvBuff));
        if((sockfd_conn = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            error_exit("Error : Could not create socket");
        }
        if((sockfd_msg = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            error_exit("Error : Could not create socket");
        }  

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
        free(server_ip);

        if(connect(sockfd_msg, (struct sockaddr *)&server_msg, sizeof(server_msg)) < 0) {
           error_exit("Error : Connect Failed on message port");
        }

        if(connect(sockfd_conn, (struct sockaddr *)&server_conn, sizeof(server_conn)) < 0) {
           error_exit("Error : Connect Failed on connection port");
        }

        shake = handshake(sockfd_conn);
        if (!shake) {
            error_exit("Unable to establish link with server");
        }
        //spawn thread to listen for messages
    }
}

G_MODULE_EXPORT void on_server_diss_activate()
{
    if (connected) {
        printf("Disconnecting to server...\n");
        connected = 0;
        sockfd_conn = 0;
        sockfd_msg = 0;


        //request to be taken out of the pool, then kill thread
    }
}

G_MODULE_EXPORT void on_about_activate()
{
    printf("Showing about...\n");
}