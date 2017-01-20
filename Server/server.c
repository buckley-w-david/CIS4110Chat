#include "server.h"

#define MAX_IN_QUEUE 10
#define MESSAGE_PORT 5000
#define CONNECTION_PORT 5001
#define BUFFERSIZE 1025

pthread_t tid[2];
pthread_mutex_t connections_lock;
int* connections;
int count = 0;

int error_exit(char* message) {
    printf("%s\n", message);
    exit(1);
}

int send_message_to_all(char* message) {
    return 0;
}

void* listen_for_new_connection() {
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    int rbind = 0, rlisten = 0, retval = 0, n = 0;
    fd_set rfds;
    socklen_t clilen = 0;
    char buffer[BUFFERSIZE];
    struct timeval tv;

    tv.tv_sec = 7;  /* 30 Secs Timeout */
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    memset(&buffer, 0, sizeof(buffer));
    memset(&serv_addr, 0, sizeof(serv_addr));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    FD_SET(sockfd, &rfds);
    
    if (sockfd < 0) {
        error_exit("ERROR opening socket");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(CONNECTION_PORT);

    rbind = bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (rbind == -1 || sizeof(serv_addr) < 0) {
        error_exit("ERROR on binding");
    }

    rlisten = listen(sockfd, MAX_IN_QUEUE);
    if (rlisten == -1) {
        error_exit("ERROR on listening");
    }

    clilen = sizeof(cli_addr);
    while (1) {
        retval = select(sockfd+1, &rfds, NULL, NULL, NULL);
        switch (retval) {
            case -1:
                error_exit("ERROR on select");
                break;
            case 1:
                printf("Connection is available\n");
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
                printf("recived on %d\n", cli_addr.sin_addr);
                if (newsockfd < 0) {
                    error_exit("ERROR on accept");
                }
                write(newsockfd, "CONFIRM", 7);
                printf("Past accept\n");
                n = read(newsockfd, buffer, BUFFERSIZE);
                if (n < 0) {
                    printf("ERROR reading from socket\n");
                    close(newsockfd);
                    continue;
                }

                if (strncmp(buffer, "CONNECT", 7) == 0) {
                    printf("CONNECTION MADE by on %d\n", cli_addr.sin_addr);
                    pthread_mutex_lock(&connections_lock);
                    count += 1;
                    connections = realloc(connections, sizeof(int)*count);
                    connections[count-1] = newsockfd;
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

void* listen_for_new_message() {
    return NULL;
}

/*
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
    if (pthread_mutex_init(&connections_lock, NULL) != 0)
    {
        printf("mutex init failed");
        return 1;
    }

    err = pthread_create(&(tid[0]), NULL, &listen_for_new_connection, NULL);
    if (err != 0) {
        error_exit("Can't create thread");
    }

    err = pthread_create(&(tid[1]), NULL, &listen_for_new_message, NULL);
    if (err != 0) {
        error_exit("Can't create thread");
    }

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_mutex_destroy(&connections_lock);

    free(connections);

    return 0;
}