#include "client.h"
#include "network_functions.h"

int error_exit(char* message) {
    printf("%s\n", message);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr;

    if(argc != 2) {
        printf("Usage: %s <ip of server>\n", argv[0]);
        exit(1);
    }

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_exit("Error : Could not create socket");
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);

    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        error_exit("inet_pton error occured");
    }

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       error_exit("Error : Connect Failed");
    }

    while ((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0) {
        recvBuff[n] = 0;
        if(fputs(recvBuff, stdout) == EOF) {
            error_exit("Error : Fputs error");
        }
    }

    if(n < 0) {
	   error_exit("Read Error");
    }

    return 0;
}
