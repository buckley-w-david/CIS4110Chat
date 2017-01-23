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

int sockfd_msg_interrupt[2];
int sockfd_conn_interrupt[2];

int error_exit(char* message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

int open_port_listener(int* fd, int port) {
    struct sockaddr_in serv_addr_conn; //Server and client address
    struct timeval tv;

    tv.tv_sec = 10;  /* 10 Secs read Timeout */
    tv.tv_usec = 0;

    //Zero out various structures
    memset(&serv_addr_conn, 0, sizeof(serv_addr_conn));

    //Create the listening socket
    *fd = socket(AF_INET, SOCK_STREAM, 0);

    //Only allowed to wait for ~10 seconds for messages
    setsockopt(*fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    if (*fd < 0) { //Make sure that we were actually able to start that (it's bad if we can't, so exit)
        error_exit("ERROR on opening socket");
    }

    //Initialize listening information
    serv_addr_conn.sin_family = AF_INET;
    serv_addr_conn.sin_addr.s_addr = INADDR_ANY;
    serv_addr_conn.sin_port = htons(port);

    //Bind listening port to the socket file descriptor
    if (bind(*fd, (struct sockaddr*)&serv_addr_conn, sizeof(serv_addr_conn)) == -1) {
        close(*fd);
        error_exit("ERROR on binding port");
    }

    //Start listening on the socket,
    if (listen(*fd, MAX_IN_QUEUE) == -1) {
        close(*fd);
        error_exit("ERROR on listening to port");
    }

    return *fd;
}

Connection* confirm_new_connection(int fd) {
    int newsockfd, n;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    char buffer[BUFFERSIZE];
    Connection* newconn;
    struct timeval tv;

    tv.tv_sec = 10;  /* 10 Secs read Timeout */
    tv.tv_usec = 0;

    newsockfd = accept(fd, (struct sockaddr *)&cli_addr, &clilen); //Accept the new connection
    printf("New connection from %d\n", cli_addr.sin_addr.s_addr);

    if (newsockfd < 0) {
        printf("ERROR on accepting connection request");
        return NULL;
    }

    //Send client confirmation
    write(newsockfd, "CONFIRM", 7);
    
    //Wait for client to confirm on their end (complete the handshake)
    n = read(newsockfd, buffer, BUFFERSIZE);

    //Read timed out, or there was some other error reading
    if (n < 0) { 
        printf("Unable to confirm new connection\n");
        close(newsockfd);
        return NULL;
    }

    //Client requested to be connected to the pool, so add them
    if (strncmp(buffer, "CONNECT", 7) == 0) {
        printf("Completed connection with %d\n", cli_addr.sin_addr.s_addr);

        if ((newconn = malloc(sizeof(Connection))) != NULL) {
            newconn->s_addr = cli_addr.sin_addr.s_addr;
            newconn->fd_conn = newsockfd;
            newconn->fd_msg = -1;
            setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)); //read timeout of 10 seconds

            return newconn;
        } else {
            printf("Could not allocate more space for connection\n");
            return NULL;
        }      
    }

    return NULL;
}

void remove_conn_from_pool(int index) {
    close(connections[index]->fd_conn);
    close(connections[index]->fd_msg);

    free(connections[index]);
    for (int i = index; i < count-1; i++) {
        connections[i] = connections[i+1];
    }
    count -= 1;
    connections = realloc(connections, sizeof(Connection*)*count);
}

void send_message_to_pool(char* buffer, int n) {
    //Write message to each connection
    printf("Sending \"%s\" to pool\n", buffer);
    for (int i = 0; i < count; i++) {
        if (connections[i]->fd_msg != -1)
            write(connections[i]->fd_msg, buffer, n);
    }
}

void* listen_for_new_connection() {
    int conn_listener, maxfd = -1, index = -1, n = 0, retval = 0;
    char buffer[BUFFERSIZE];
    fd_set rfds; //FD to listen for new activity on
    Connection* newconn;

    conn_listener = open_port_listener(&conn_listener, CONNECTION_PORT);

    while (1) {
        printf("Connection iteration\n");

        FD_ZERO(&rfds);
        FD_SET(conn_listener, &rfds);
        FD_SET(sockfd_conn_interrupt[0], &rfds);

        memset(&buffer, 0, sizeof(buffer));

        maxfd = (conn_listener > sockfd_msg_interrupt[0]) ? (conn_listener) : sockfd_msg_interrupt[0];
        index = -1;

        pthread_mutex_lock(&connections_lock);
        for (int i = 0; i < count; i++) {
            if (connections[i]->fd_conn != -1) {
                FD_SET(connections[i]->fd_conn, &rfds);
                if (connections[i]->fd_conn > maxfd) {
                    maxfd = connections[i]->fd_conn;
                }
            }
        }
        pthread_mutex_unlock(&connections_lock);

        retval = select(maxfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port
        printf("New activity on connect thread\n");

        if (retval == -1) {
            printf("Error on selecting...\n");
        }

        pthread_mutex_lock(&connections_lock);
        if (FD_ISSET(sockfd_conn_interrupt[0], &rfds)) {
            //We were interrupted
            printf("Connect thread interrupted\n");
            n = read(sockfd_msg_interrupt[0], buffer, BUFFERSIZE);
        } else if (FD_ISSET(conn_listener, &rfds)) {
            //A new connection is being requested
            if ((newconn = confirm_new_connection(conn_listener)) != NULL) {
                printf("Adding connection %d\n", newconn->s_addr);
                
                count += 1;
                connections = realloc(connections, sizeof(Connection*)*count);
                connections[count-1] = newconn;
                
            }
        } else {
            //An existing connection is making a request
            printf("Existing connection making request\n");

            for (int i = 0; i < count; i++) {
                if (FD_ISSET(connections[i]->fd_conn, &rfds)) {
                    index = i;
                    break;
                }
            }
            if (index != -1) {
                n = read(connections[index]->fd_conn, buffer, BUFFERSIZE);

                //Read timed out, or there was some other error reading
                if (n <= 0) { 
                    printf("Unable to read from connection, removing from pool\n");
                    write(sockfd_msg_interrupt[1], "interrupt", 9);  //Now that connection has been removed from the pool, the message thread should stop listening for it

                    remove_conn_from_pool(index);
                } else if (strncmp(buffer, "DISCONNECT", 10) == 0) {
                    printf("Removing %d from pool\n", connections[index]->s_addr);
                    write(sockfd_msg_interrupt[1], "interrupt", 9); //Now that connection has been removed from the pool, the message thread should stop listening for it

                    remove_conn_from_pool(index);
                } else {
                    printf("Unknown request, ignoring... %s\n", buffer);
                    printf("read %d chars\n", n);
                }
            } else {
                printf("Not sure how this could happen...\n");
            }
        }
        pthread_mutex_unlock(&connections_lock);
    }
    return NULL;
}

void* listen_for_new_message() {
    int msg_listener, maxfd = -1, newsockfd = -1, index = -1, n = 0, retval = 0;
    char buffer[BUFFERSIZE];
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    fd_set rfds; //FD to listen for new activity on

    msg_listener = open_port_listener(&msg_listener, MESSAGE_PORT);

    while (1) {
        printf("Message iteration\n");

        FD_ZERO(&rfds);
        FD_SET(msg_listener, &rfds);
        FD_SET(sockfd_msg_interrupt[0], &rfds);

        memset(&buffer, 0, sizeof(buffer));

        maxfd = (msg_listener > sockfd_msg_interrupt[0]) ? (msg_listener) : sockfd_msg_interrupt[0];
        index = -1;

        pthread_mutex_lock(&connections_lock);
        for (int i = 0; i < count; i++) {
            if (connections[i]->fd_msg != -1) {
                FD_SET(connections[i]->fd_msg, &rfds);
                if (connections[i]->fd_msg > maxfd) {
                    maxfd = connections[i]->fd_msg;
                }
            }
        }
        pthread_mutex_unlock(&connections_lock);

        retval = select(maxfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port
        printf("New activity on message thread\n");

        if (retval == -1) {
            printf("Error on selecting...\n");
        }

        pthread_mutex_lock(&connections_lock);
        if (FD_ISSET(sockfd_msg_interrupt[0], &rfds)) {
            //We were interrupted
            printf("Message thread interrupted\n");
            n = read(sockfd_msg_interrupt[0], buffer, BUFFERSIZE);
        } else if (FD_ISSET(msg_listener, &rfds)) {
            //A new connection is being requested
            printf("New message connection is being requested\n");
            newsockfd = accept(msg_listener, (struct sockaddr *)&cli_addr, &clilen); //Accept the new connection
            printf("Setting MSG connection to %d on %d\n", cli_addr.sin_addr.s_addr, newsockfd);
            
            for (int i = 0; i < count; i++) {
                if (connections[i]->s_addr == cli_addr.sin_addr.s_addr) {
                    connections[i]->fd_msg = newsockfd;
                    break;
                }
            }
        } else {
            //An existing connection is making a request
            printf("Existing msg connection making request\n");

            for (int i = 0; i < count; i++) {
                if (FD_ISSET(connections[i]->fd_msg, &rfds)) {
                    index = i;
                    break;
                }
            }
            if (index != -1) {
                n = read(connections[index]->fd_msg, buffer, BUFFERSIZE);

                //Read timed out, or there was some other error reading
                if (n <= 0) { 
                    printf("Unable to read from connection, removing from pool\n");
                    write(sockfd_msg_interrupt[1], "interrupt", 9); //Now that connection has been removed from the pool, the connection thread should stop listening for it

                    remove_conn_from_pool(index);
                } else {
                    printf("read %d chars\n", n);
                    send_message_to_pool(buffer, n);
                }
            } else {
                printf("Not sure how this could happen...\n");
            }
        }
        pthread_mutex_unlock(&connections_lock);
    }
    return NULL;
}


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

    pipe(sockfd_msg_interrupt);
    pipe(sockfd_conn_interrupt);

    //Kill threads after entering a 'q' character
    while (getchar() != 'q') {
        ;//Busy-wait
    }

    pthread_cancel(incomming_connections);
    pthread_cancel(incomming_messages);

    pthread_mutex_destroy(&connections_lock);

    free(connections);

    return 0;
}