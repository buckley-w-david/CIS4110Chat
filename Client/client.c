
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

char server[] = "ec2-35-165-128-169.us-west-2.compute.amazonaws.com";//"cis4110chatserver.davidbuckleyprogrammer.me";
char id[] = "Chat Client";

//Emergency exit from the client, kill the GUI, close any connections, and cancle and ongoing threads
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

/*
Disconnect from the server

Inturrupt the message listening thread
Send it a message that the client would like to disconnect
Close socket connections
*/
void sever_connection() {
    printf("Disconnecting from server...\n");

    write(sockfd_msg_interrupt[1], "close", 5); //Will cause message listener to kill itself
    write(sockfd_conn, "DISCONNECT", 10);

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

    //File Descriptor used to inturrupt the message listening thread when needed
    pipe(sockfd_msg_interrupt);

    if (pthread_mutex_init(&socket_lock, NULL) != 0) {
        printf("mutex init failed");
        return 1;
    }

    if (pthread_mutex_init(&connection_lock, NULL) != 0) {
        printf("mutex init failed");
        return 1;
    }

    /*
    Initialize the gui components, and connect the signal handlers to their functions
    */
    gtk_init(&argc, &argv);
 
    builder = gtk_builder_new();
    gtk_builder_add_from_file (builder, "chat_gui.glade", NULL);
 
    window = GTK_WIDGET(gtk_builder_get_object(builder, "chat_gui"));
    widg->history = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "history"));
    widg->input = GTK_ENTRY(gtk_builder_get_object(builder, "message_input"));

    gtk_builder_connect_signals(builder, widg);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 600);

    //Show the gui and enter into main loop
    gtk_widget_show(window);                
    gtk_main();
 
    return 0;
}

/*
Simple handshake between the client and server to verify that a 
connection is really being requested, and from where

Client -> Server: "I have a request"
Server -> Client: "Okay, what is your request"
Client -> Server: "I would like to join the pool"

Server adds client to the pool
*/
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

/*
Add recieved message to the text view history (passed from the listener function)
*/
int add_line_to_history(gpointer data) {
    time_t timer;
    char time_buffer[26];
    char hist_buffer[BUFFERSIZE+120];
    struct tm* tm_info;

    GtkTextView* textview;
    GtkTextIter ei;
    GtkTextMark* mk;
    gchar* sendme = hist_buffer;
    GtkTextBuffer* buff;

    h_update* history_update = (h_update*)data;
    memset(&hist_buffer, 0, BUFFERSIZE+120);

    printf("Adding %s to history\n", history_update->new_text);

    time(&timer);
    tm_info = localtime(&timer); //Create message timestamp

    /*
    Add timestamp to buffer, as well as some formatting text around it
    */
    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    if (history_update->id == NULL) {
        strncpy(hist_buffer, "From Anonymous at ", 18);
    } else {
        strncpy(hist_buffer, "From " , 5);
        strncat(hist_buffer, history_update->id , history_update->id_size);
        strncat(hist_buffer, " at ", 4);

    }
    
    strncat(hist_buffer, time_buffer, strlen(time_buffer));
    strncat(hist_buffer, ":\n", 2);
    strncat(hist_buffer, history_update->new_text, BUFFERSIZE);
    strncat(hist_buffer, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\0", 40);

    //get the textview
    printf("Getting previous text...\n");
    textview = GTK_TEXT_VIEW(history_update->history);
    buff = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    gtk_text_buffer_get_end_iter(buff, &ei);

    //append the message
    printf("Trying to add message...\n");
    gtk_text_buffer_insert(buff, &ei, sendme, -1);
    mk = gtk_text_buffer_get_mark (buff, "insert");
    gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (textview), mk, 0.0, FALSE, 0.0, 0.0);

    return 0;
}

/*
Function for message listening thread
*/
void* listen_for_new_message(void* history) {
    fd_set rfds; //FDs to listen for new activity on

    //Read buffers
    char buffer[BUFFERSIZE]; 
    char to_history[BUFFERSIZE];

    int retval = 0, n = 0, maxfd = -1;
    h_update hist_update;

    hist_update.history = (GtkTextView*)history;
    hist_update.id = NULL;

    while (1) {
        //Initialize the file descriptors we would like to look for updates on
        FD_ZERO(&rfds);
        FD_SET(sockfd_msg, &rfds); //Message socket
        FD_SET(sockfd_msg_interrupt[0], &rfds); //Inturrupt file descriptor

        memset(&buffer, 0, sizeof(buffer));

        //Calculate the maximum value for the file descriptors (Needed for select)
        maxfd = (sockfd_msg > sockfd_msg_interrupt[0]) ? sockfd_msg : sockfd_msg_interrupt[0];
        retval = select(maxfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port

        //Select failed, an unknown error has occured, exit the program.
        if (retval == -1) {
            error_exit("ERROR on select");
        }

        //Check if we were interrupted to get to this point, if so kill the thread
        if (FD_ISSET(sockfd_msg_interrupt[0], &rfds)) {
            printf("No longer listening for messages\n");
            n = read(sockfd_msg_interrupt[0], buffer, BUFFERSIZE);
            return NULL;
        }

        printf("Connection is available - Message\n");
        n = read(sockfd_msg, buffer, BUFFERSIZE); //Read new message from the server
        printf("read %d chars: %s\n", n, buffer);

        //Error with read, inform user and disconnect
        if (n <= 0) {
            printf("Network error occurued, disconnecting from server...\n");
            hist_update.id = id;
            hist_update.id_size = 11;

            memset(&to_history, 0, sizeof(to_history));
            strncpy(to_history, "Network error occurued, disconnecting from server...", BUFFERSIZE);
            hist_update.new_text = to_history;

            gdk_threads_add_idle(&add_line_to_history, (gpointer)&hist_update);
            return NULL;
        }
        memset(&to_history, 0, sizeof(to_history));
        strncpy(to_history, buffer, BUFFERSIZE);

        hist_update.new_text = to_history;

        //Add message to history
        gdk_threads_add_idle(&add_line_to_history, (gpointer)&hist_update);
    }

    return NULL;
}

//Called when window is closed, simply kills the gui and ends the program
G_MODULE_EXPORT void on_chat_gui_destroy()
{
    gtk_main_quit();
}

/*
Sends a message to the server

Not very complex, just writes to the FD if the client is connected
*/
G_MODULE_EXPORT void send_message(GtkButton* button, gpointer user_data)
{
    Widgets* point = (Widgets*)user_data;
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

/*
Connects the client to the server via the handshake (if the client is not already connected)
*/
G_MODULE_EXPORT void on_server_conn_activate(GtkMenuButton* button, gpointer user_data)
{
    Widgets* point = (Widgets*)user_data;
    GtkTextView* history = GTK_TEXT_VIEW(point->history);
    char message[] = "Connected to the chat server";

    h_update hist_update;
    hist_update.history = history;
    hist_update.new_text = message;
    hist_update.id = id;
    hist_update.id_size = 11;

    pthread_mutex_lock(&connection_lock);
        if (!connected) {
            int shake = 0, err = 0;
            char* restrict server_ip;
            char recvBuff[BUFFERSIZE];

            printf("Connecting to server...\n");
            //First step, resolve the DNS name of the server to an ip
            server_ip = ipv4_dns_resolve(server);
            
            memset(recvBuff, 0,sizeof(recvBuff));

            pthread_mutex_lock(&socket_lock);
                //Create the communication sockets with the server
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

                //Translate the IP to the binary network representation
                if(inet_pton(AF_INET, server_ip, &server_conn.sin_addr) <= 0) {
                    error_exit("inet_pton error occured on connection port");
                }
                if(inet_pton(AF_INET, server_ip, &server_msg.sin_addr) <= 0) {
                    error_exit("inet_pton error occured on message port");
                }
                printf("Connected to ip %s...\n", server_ip);
                free(server_ip);

                //Connect to the server on the connection port, exiting if it fails
                //This port is used for requests to and from the server, such as connection and disconnection from thr pool
                if(connect(sockfd_conn, (struct sockaddr *)&server_conn, sizeof(server_conn)) < 0) {
                   error_exit("Error : Connect Failed on connection port");
                }

                printf("Doing handshake - CONNECTION_PORT\n");
                shake = handshake(sockfd_conn); //Handshake (explained in handshake function)
                printf("Done handshake, connection to message port\n");

                //Connect to the server on the messaging port, exiting if fails
                //This port is used for message communication between client and server
                if(connect(sockfd_msg, (struct sockaddr *)&server_msg, sizeof(server_msg)) < 0) {
                   error_exit("Error : Connect Failed on message port");
                }

                printf("Msg Socket FD: %d\nConn Socket FD: %d\n", sockfd_msg, sockfd_conn);
            pthread_mutex_unlock(&socket_lock);

            //If the handshake failed, we are not correctly connected, and should exit
            if (!shake) {
                error_exit("Unable to establish link with server");
            }

            connected = 1;

            //Create message listening thread, it will listen for new messages and display them to the user
            err = pthread_create(&message_thread, NULL, &listen_for_new_message, ((Widgets*)user_data)->history);
            if (err != 0) {
                error_exit("Can't create thread");
            }
            printf("Connected to server, awaiting messages...\n");
            add_line_to_history((gpointer)&hist_update);
        } else {
            //Already connected
        }
    pthread_mutex_unlock(&connection_lock);
}

//Disconnect from the server
G_MODULE_EXPORT void on_server_diss_activate(GtkMenuButton* button, gpointer user_data)
{
    Widgets* point = (Widgets*)user_data;
    GtkTextView* history = GTK_TEXT_VIEW(point->history);
    char message[] = "Disconnected from the chat server";

    h_update hist_update;
    hist_update.history = history;
    hist_update.new_text = message;
    hist_update.id = id;
    hist_update.id_size = sizeof(id);

    if (connected) {
        sever_connection(); //Sever the connection
        add_line_to_history((gpointer)&hist_update); //Inform the user.
    } else {
        //Not currently connected
    }
}

G_MODULE_EXPORT void on_about_activate()
{
    printf("Showing about...\n");
}