#include "server.h"

#define MAX_IN_QUEUE 10
#define MESSAGE_PORT 5000
#define CONNECTION_PORT 5001
#define BUFFERSIZE 1025

pthread_t tid[2];
pthread_mutex_t connections_lock;
Connection** connections;
int count = 0;

int error_exit(char* message) {
    printf("%s\n", message);
    exit(1);
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
    //Lock the connections from being messed with by other threads
    pthread_mutex_lock(&connections_lock);
    
    //Write message to each connection
    for (int i = 0; i < count; i++) {
        write(connections[i]->fd, message, n);
    }
    //Unlock the connections from being messed with by other threads
    pthread_mutex_unlock(&connections_lock);

    return 0;
}

//Listen for new requests to be connected to the pool
void* listen_for_new_connection() {
    int sockfd, newsockfd; //Listening fd, as well as one to store incomming connections fd
    struct sockaddr_in serv_addr, cli_addr; //Server and client address
    int rbind = 0, rlisten = 0, retval = 0, n = 0; //error checking variables
    fd_set rfds; //FD to listen for new activity on
    socklen_t clilen = 0; //Length of child address
    char buffer[BUFFERSIZE];
    char ip[BUFFERSIZE];
    struct timeval tv;
    Connection* newcon = NULL;

    tv.tv_sec = 7;  /* 7 Secs read Timeout */
    tv.tv_usec = 0;

    //Zero out various structures
    FD_ZERO(&rfds); 
    memset(&serv_addr, 0, sizeof(serv_addr));

    //Create the listening socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    FD_SET(sockfd, &rfds); //Set the socket we are insterested in for the select to the listening one
    if (sockfd < 0) { //Make sure that we were actually able to start that (it's bad if we can't, so exit)
        error_exit("ERROR opening socket");
    }

    //Initialize listening information
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(CONNECTION_PORT);

    //Bind listening port to the socket file descriptor
    rbind = bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (rbind == -1 || sizeof(serv_addr) < 0) { //Make sure it worked
        error_exit("ERROR on binding");
    }

    //Start listening on the socket,
    rlisten = listen(sockfd, MAX_IN_QUEUE);
    if (rlisten == -1) { //MAke sure it worked
        error_exit("ERROR on listening");
    }

    //Length of the child connection address
    clilen = sizeof(cli_addr);
    while (1) {
        memset(&buffer, 0, sizeof(buffer));
        memset(&ip, 0, sizeof(ip));
        retval = select(sockfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port
        switch (retval) {
            case -1: //Something went wrong
                error_exit("ERROR on select");
                break;
            case 1: //Connection was made
                printf("Connection is available\n");

                //Accept the connction
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

                //Only allowed to wait for ~7 seconds for confirmation message
                setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

                inet_ntop(AF_INET, &cli_addr.sin_addr, &ip, BUFFERSIZE);
                printf("recived conn request on %s\n", ip);

                //there was some problem accepting this connection
                if (newsockfd < 0) {
                    error_exit("ERROR on accept");
                }

                //Send client confirmation
                write(newsockfd, "CONFIRM", 7);

                //Wait for client to confirm on their end (complete the handshake)
                n = read(newsockfd, buffer, BUFFERSIZE);

                //Read timed out, or there was some other error reading
                if (n < 0) { 
                    printf("ERROR reading from socket\n");
                    close(newsockfd);
                    continue;
                }

                //Client requested to be connected to the pool, so add them
                if (strncmp(buffer, "CONNECT", 7) == 0) {
                    printf("CONNECTION MADE by on %s\n", ip);
                    newcon = malloc(sizeof(Connection));
                    newcon->s_addr = cli_addr.sin_addr.s_addr;
                    newcon->fd = newsockfd;

                    //Lock the connections from being messed with by other threads
                    pthread_mutex_lock(&connections_lock);

                    //Allocate space and add connection
                    count += 1;
                    connections = realloc(connections, sizeof(Connection*)*count);
                    connections[count-1] = newcon;

                    //Unlock the connections from being messed with by other threads
                    pthread_mutex_unlock(&connections_lock);
                }
                break;
            default:
                printf("Not sure what happened here");
                break;
        }
    }

    return NULL;
}

//Listens for new messages, and broadcasts them to all known connections
void* listen_for_new_message() {
    int sockfd, newsockfd; //Listening fd, as well as one to store incomming connections fd
    struct sockaddr_in serv_addr, cli_addr; //Server and client address
    int rbind = 0, rlisten = 0, retval = 0, n = 0; //error checking variables
    fd_set rfds; //FD to listen for new activity on
    socklen_t clilen = 0; //Length of child address
    char buffer[BUFFERSIZE];
    char ip[BUFFERSIZE];
    struct timeval tv;

    tv.tv_sec = 7;  /* 7 Secs read Timeout */
    tv.tv_usec = 0;

    //Zero out various structures
    FD_ZERO(&rfds); 
    memset(&serv_addr, 0, sizeof(serv_addr));

    //Create the listening socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    FD_SET(sockfd, &rfds); //Set the socket we are insterested in for the select to the listening one
    if (sockfd < 0) { //Make sure that we were actually able to start that (it's bad if we can't, so exit)
        error_exit("ERROR opening socket");
    }

    //Initialize listening information
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(MESSAGE_PORT);

    //Bind listening port to the socket file descriptor
    rbind = bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (rbind == -1 || sizeof(serv_addr) < 0) { //Make sure it worked
        error_exit("ERROR on binding");
    }

    //Start listening on the socket,
    rlisten = listen(sockfd, MAX_IN_QUEUE);
    if (rlisten == -1) { //MAke sure it worked
        error_exit("ERROR on listening");
    }

    //Length of the child connection address
    clilen = sizeof(cli_addr);
    while (1) {
        memset(&buffer, 0, sizeof(buffer));
        memset(&ip, 0, sizeof(ip));
        retval = select(sockfd+1, &rfds, NULL, NULL, NULL); //Wait for activity on the listening port
        switch (retval) {
            case -1: //Something went wrong
                error_exit("ERROR on select");
                break;
            case 1: //Connection was made
                printf("Connection is available\n");
                //Accept the connction
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

                //Message from someone in the pool
                if (verify_connection(cli_addr.sin_addr)) {
                    //Only allowed to wait for ~7 seconds for confirmation message
                    setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
                    inet_ntop(AF_INET, &cli_addr.sin_addr, &ip, BUFFERSIZE);
                    printf("recived message on %s\n", ip);
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

                    send_message_to_all(buffer, n);
                }
                
                break;
            default:
                printf("Not sure what happened here");
                break;
        }
    }

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
    err = pthread_create(&(tid[0]), NULL, &listen_for_new_connection, NULL);
    if (err != 0) {
        error_exit("Can't create thread");
    }

    //Create message polling thread
    err = pthread_create(&(tid[1]), NULL, &listen_for_new_message, NULL);
    if (err != 0) {
        error_exit("Can't create thread");
    }

    //Wait for end of threads (theoreticall should never happen - Infinite loops)
    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_mutex_destroy(&connections_lock);

    free(connections);

    return 0;
}