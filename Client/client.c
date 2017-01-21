#include "client.h"
#include "network_functions.h"

#define MAX_IN_QUEUE 10
#define CONNECTION_PORT 5001
#define MESSAGE_PORT 5000
#define BUFFERSIZE 1025

int error_exit(char* message) {
    printf("%s\n", message);
    exit(1);
}

int handshake(int sockfd, struct sockaddr* serv_addr, struct sockaddr_in) {
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

int main(int argc, char *argv[])
{
    int sockfd_conn = 0, sockfd_msg = 0, n = 0, shake = 0;
    struct sockaddr_in serv_addr_conn, serv_addr_msg;

    if(argc != 2) {
        printf("Usage: %s <ip of server>\n", argv[0]);
        exit(1);
    }

    memset(recvBuff, 0,sizeof(recvBuff));
    if((sockfd_conn = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_exit("Error : Could not create socket");
    }
    if((sockfd_msg = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_exit("Error : Could not create socket");
    }  

    memset(&serv_addr_conn, 0, sizeof(serv_addr_conn));
    memset(&serv_addr_msg, 0, sizeof(serv_addr_msg));

    serv_addr_conn.sin_family = AF_INET;
    serv_addr_conn.sin_port = htons(CONNECTION_PORT);

    serv_addr_msg.sin_family = AF_INET;
    serv_addr_msg.sin_port = htons(MESSAGE_PORT);

    if(inet_pton(AF_INET, argv[1], &serv_addr_conn.sin_addr) <= 0) {
        error_exit("inet_pton error occured on connection port");
    }
    if(inet_pton(AF_INET, argv[1], &serv_addr_msg.sin_addr) <= 0) {
        error_exit("inet_pton error occured on message port");
    }

    if(connect(sockfd_msg, serv_addr_msg, sizeof(serv_addr_msg)) < 0) {
       error_exit("Error : Connect Failed on message port");
    }

    if(connect(sockfd_conn, serv_addr_conn, sizeof(serv_addr_conn)) < 0) {
       error_exit("Error : Connect Failed on connection port");
    }

    shake = handshake(sockfd_conn, (struct sockaddr *)&serv_addr_conn, serv_addr_conn)
    if (!shake) {
        error_exit("Unable to establish link with server");
    }

    


    return 0;
}
