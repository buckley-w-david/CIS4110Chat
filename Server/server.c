#include "server.h"

#define MAX_IN_QUEUE 10
#define MESSAGE_PORT 5000
#define CONNECTION_PORT 5001
#define BUFFERSIZE 1025

pthread_t incomming_connections;
pthread_t incomming_messages;

pthread_mutex_t connections_lock;
Connection** connections;
int count = 0;

int error_exit(char* message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static void cleanup_handler(void *arg) {
   close(*(int*)arg);
}

int verify_connection(struct in_addr conn) {
    for (int i = 0; i < count; i++) {
        if (connections[i]->s_addr == conn.s_addr) {
            return 1;
        }
    }

    return 0;
}

//Send messages to each connection in the connections array
int send_message_to_all(char* message, int n) {    
    //Write message to each connection
    for (int i = 0; i < count; i++) {
        if (connections[i]->fd_msg != -1)
            write(connections[i]->fd_msg, message, n);
    }

    return 0;
}

//Listen for new requests to be connected to the pool
void* listen_for_new_connection() {
    int sockfd_conn, newsockfd; //Listening fd, as well as one to store incomming connections fd
    struct sockaddr_in serv_addr_conn, cli_addr; //Server and client address
    int retval = 0, n = 0, maxfd = -1, index = -1; //error checking variables
    fd_set rfds; //FD to listen for new activity on
    socklen_t clilen = 0; //Length of child address
    char buffer[BUFFERSIZE];
    char ip[BUFFERSIZE];
    struct timeval tv;
    Connection* newcon = NULL;

    tv.tv_sec = 10;  /* 10 Secs read Timeout */
    tv.tv_usec = 0;

    //Zero out various structures
    memset(&serv_addr_conn, 0, sizeof(serv_addr_conn));

    //Create the listening socket
    sockfd_conn = socket(AF_INET, SOCK_STREAM, 0);

    //Only allowed to wait for ~10 seconds for messages
    setsockopt(sockfd_conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    if (sockfd_conn < 0) { //Make sure that we were actually able to start that (it's bad if we can't, so exit)
        error_exit("ERROR opening socket");
    }

    //Initialize listening information
    serv_addr_conn.sin_family = AF_INET;
    serv_addr_conn.sin_addr.s_addr = INADDR_ANY;
    serv_addr_conn.sin_port = htons(CONNECTION_PORT);

    //Bind listening port to the socket file descriptor
    if (bind(sockfd_conn, (struct sockaddr*)&serv_addr_conn, sizeof(serv_addr_conn)) == -1) {
        error_exit("ERROR on binding connection port");
    }

    //Start listening on the socket,
    if (listen(sockfd_conn, MAX_IN_QUEUE) == -1) {
        error_exit("ERROR on listening to connection port");
    }

    //Length of the child connection address
    clilen = sizeof(cli_addr);

    pthread_cleanup_push(cleanup_handler, &sockfd_conn);
    while (1) {
        //Check the generic port as well as established connections
        FD_ZERO(&rfds);
        FD_SET(sockfd_conn, &rfds);
        maxfd = sockfd_conn;
        index = -1;

        pthread_mutex_lock(&connections_lock);
            for (int i = 0; i < count; i++) {
                FD_SET(connections[i]->fd_conn, &rfds);\
                if (connections[i]->fd_msg > maxfd) {
                    maxfd = connections[i]->fd_conn;
                }
            }
        pthread_mutex_unlock(&connections_lock);

        memset(&buffer, 0, sizeof(buffer));
        memset(&ip, 0, sizeof(ip));
        printf("Connection loop iteration\n");

        retval = select(maxfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port
        switch (retval) {
            case -1: //Something went wrong
                error_exit("ERROR on select");
                break;
            default: //Connection was made
                printf("Connection is available, locking...\n");
                pthread_mutex_lock(&connections_lock);
                    //Accept the connction
                    newsockfd = accept(sockfd_conn, (struct sockaddr *) &cli_addr, &clilen);

                    printf("Conn connection from %d\n", cli_addr.sin_addr.s_addr);

                    inet_ntop(AF_INET, &cli_addr.sin_addr, &ip, BUFFERSIZE);
                    printf("recived conn request on %s\n", ip);

                    //there was some problem accepting this connection
                    if (newsockfd < 0) {
                        printf("ERROR on accepting connection request");
                        close(newsockfd);
                        continue;
                    }

                    if (FD_ISSET(sockfd_conn, &rfds)) {
                        //Send client confirmation
                        write(newsockfd, "CONFIRM", 7);

                        //Wait for client to confirm on their end (complete the handshake)
                        n = read(newsockfd, buffer, BUFFERSIZE);

                        //Read timed out, or there was some other error reading
                        if (n < 0) { 
                            printf("ERROR reading from socket - CONNECTION\n");
                            close(newsockfd);
                            continue;
                        }

                        //Client requested to be connected to the pool, so add them
                        if (strncmp(buffer, "CONNECT", 7) == 0) {
                            printf("CONNECTION MADE by %s\n", ip);
                            newcon = malloc(sizeof(Connection));
                            newcon->s_addr = cli_addr.sin_addr.s_addr;
                            newcon->fd_conn = newsockfd;
                            newcon->fd_msg = -1;

                            //Allocate space and add connection
                            count += 1;
                            connections = realloc(connections, sizeof(Connection*)*count);
                            connections[count-1] = newcon;                    
                        }
                    } else {
                        for(int i = 0; i < count; i++) {
                            if (connections[i]->s_addr == cli_addr.sin_addr.s_addr) {
                                index = i;
                                printf("old: %d, new: %d\n", connections[i]->fd_conn, newsockfd);
                                connections[i]->fd_conn = newsockfd;
                                break;
                            }
                        }

                        if (index != -1) {
                            n = read(newsockfd, buffer, BUFFERSIZE);
                            if (n < 0) { 
                                printf("ERROR reading from socket - CONNECTION\n");
                                close(newsockfd);
                                continue;
                            }

                            if (strncmp(buffer, "DISCONNECT", 10) == 0) {
                                free(connections[index]);
                                for (int i = index; i < count-1; i++) {
                                    connections[i] = connections[i+1];
                                }
                                count -= 1;
                                connections = realloc(connections, sizeof(Connection*)*count);
                            }
                        }
                    }
                pthread_mutex_unlock(&connections_lock);
                break;
        }
    }

    pthread_cleanup_pop(1);
    return NULL;
}

//Listen for new messages
void* listen_for_new_message() {
    int sockfd_msg, newsockfd; //Listening fd, as well as one to store incomming connections fd
    struct sockaddr_in serv_addr_msg, cli_addr; //Server and client address
    int retval = 0, n = 0, new = 0, index = -1, maxfd = -1; //error checking variables
    fd_set rfds; //FD to listen for new activity on
    socklen_t clilen = 0; //Length of child address
    char buffer[BUFFERSIZE];
    char ip[BUFFERSIZE];
    struct timeval tv;

    tv.tv_sec = 10;  /* 10 Secs read Timeout */
    tv.tv_usec = 0;

    //Zero out various structures
    memset(&serv_addr_msg, 0, sizeof(serv_addr_msg));

    //Create the listening socket
    sockfd_msg = socket(AF_INET, SOCK_STREAM, 0);

    //Only allowed to wait for ~10 seconds for messages
    setsockopt(sockfd_msg, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    if (sockfd_msg < 0) { //Make sure that we were actually able to start that (it's bad if we can't, so exit)
        error_exit("ERROR opening message socket");
    }

    //Initialize listening information
    serv_addr_msg.sin_family = AF_INET;
    serv_addr_msg.sin_addr.s_addr = INADDR_ANY;
    serv_addr_msg.sin_port = htons(MESSAGE_PORT);

    //Bind listening port to the socket file descriptor
    if (bind(sockfd_msg, (struct sockaddr*)&serv_addr_msg, sizeof(serv_addr_msg)) == -1) {
        error_exit("ERROR on binding message port");
    }

    //Start listening on the socket,
    if (listen(sockfd_msg, MAX_IN_QUEUE) == -1) {
        error_exit("ERROR on listening to message port");
    }

    //Length of the child connection address
    clilen = sizeof(cli_addr);

    pthread_cleanup_push(cleanup_handler, &sockfd_msg);
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(sockfd_msg, &rfds);
        maxfd = sockfd_msg;

        pthread_mutex_lock(&connections_lock);
            for (int i = 0; i < count; i++) {
                FD_SET(connections[i]->fd_msg, &rfds);
                if (connections[i]->fd_msg > maxfd) {
                    maxfd = connections[i]->fd_msg;
                }
            }
        pthread_mutex_unlock(&connections_lock);

        memset(&buffer, 0, sizeof(buffer));
        memset(&ip, 0, sizeof(ip));
        printf("Message loop iteration\n");
        index = -1;
        new = 0;

        retval = select(maxfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port
        switch (retval) {
            case -1: //Something went wrong
                error_exit("ERROR on message select");
                break;
            default: //Connection was made
                printf("Message is available, locking...\n");
                pthread_mutex_lock(&connections_lock);
                    //Accept the connction
                    newsockfd = accept(sockfd_msg, (struct sockaddr *) &cli_addr, &clilen);

                    printf("Message connection from %d\n", cli_addr.sin_addr.s_addr);

                    if (newsockfd < 0) {
                        printf("ERROR on accepting message request\n");
                        close(newsockfd);
                        continue;
                    }

                    for(int i = 0; i < count; i++) {
                        if (connections[i]->s_addr == cli_addr.sin_addr.s_addr) {
                            if (connections[i]->fd_msg == -1) {
                                new = 1;
                            }
                            printf("old: %d, new: %d\n", connections[i]->fd_conn, newsockfd);
                            connections[i]->fd_msg = newsockfd;
                            index = i;
                            break;
                        }
                    }

                    if (FD_ISSET(sockfd_msg, &rfds)) {
                        if (index != -1) {
                            //If it wasn't the first message connection from this client (establishing contact)
                            //Then read the message it's sending
                            if (!new) {
                                //Read data from client
                                n = read(newsockfd, buffer, BUFFERSIZE);

                                //Read timed out, or there was some other error reading
                                if (n < 0) { 
                                    printf("ERROR reading from socket - MESSAGE\n");
                                    close(newsockfd);
                                    continue;
                                }

                                send_message_to_all(buffer, n);
                            }
                        }
                    } else {
                        if (!new) {
                            //Read data from client
                            n = read(newsockfd, buffer, BUFFERSIZE);

                                //Read timed out, or there was some other error reading
                            if (n < 0) { 
                                printf("ERROR reading from socket - MESSAGE\n");
                                close(newsockfd);
                                continue;
                            }

                            send_message_to_all(buffer, n);
                        }
                    }
                pthread_mutex_unlock(&connections_lock);
                break;
        }
    }

    pthread_cleanup_pop(1);
    return NULL;
}


/*
Planned program flow

parent thread                                                    connection thread
  
Start program
  |
Spawn connection thread
  |                    \------------------------------------------------\         
  |                                                                      |
Listen for new messages (port 5000)                          Listen for new connections (port 5001)
  |                                                                      |
If message recieved, send to all known other connections     If connection made, add fd to list of known connections
  |                                                                      |
Back to listening                                            Back to listening
*/

int main(int argc, char *argv[]) {
    int err;
    //Initialize mutex for adding/reading connections
    if (pthread_mutex_init(&connections_lock, NULL) != 0)
    {
        printf("mutex init failed");
        return 1;
    }
    connections = malloc(sizeof(Connection*)*0);

    //Create connection polling thread
    err = pthread_create(&incomming_connections, NULL, &listen_for_new_connection, NULL);
    if (err != 0) {
        error_exit("Can't create thread");
    }

    err = pthread_create(&incomming_messages, NULL, &listen_for_new_message, NULL);
    if (err != 0) {
        error_exit("Can't create thread");
    }

    //Kill threads after entering a character
    getchar();

    pthread_cancel(incomming_connections);
    pthread_cancel(incomming_messages);

    pthread_mutex_destroy(&connections_lock);

    free(connections);

    return 0;
}