#include "server.h"

#define MAX_IN_QUEUE 10
#define CONNECTION_PORT 5000
#define BUFFERSIZE 1025

int error_exit(char* message) {
    printf("%s\n", message);
    exit(1);
}

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0, rbind = 0, rlisten = 0, retval = 0;
    struct sockaddr_in serv_addr;
    fd_set rfds;

    char sendBuff[BUFFERSIZE];
    time_t ticks; 


    FD_ZERO(&rfds);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(sendBuff, 0, sizeof(sendBuff));

    FD_SET(listenfd, &rfds);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(CONNECTION_PORT); 

    rbind = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 
    if (rbind == -1) {
        error_exit("Error binding socket");
    }

    rlisten = listen(listenfd, MAX_IN_QUEUE);
    if (rlisten == -1) {
        error_exit("Error listen on socket");
    }

    while(1)
    {
        printf("Loop Iter\n");
        retval = select(listenfd+1, &rfds, NULL, NULL, NULL);

        switch (retval) {
            case -1:
                printf("Error on select\n");
                break;
            case 1:
                printf("Data is available\n");
                connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
                ticks = time(NULL);
                snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));
                write(connfd, sendBuff, strlen(sendBuff)); 

                close(connfd);
                break;
            default:
                break;
        }
     }
}