
#include "client_cmd.h"
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

void connect_to_server();
void send_message(char* message, int n);

int error_exit(char* message) {
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
    char buffer[BUFFERSIZE];

    if (pthread_mutex_init(&socket_lock, NULL) != 0) {
        printf("mutex init failed");
        return 1;
    }

    if (pthread_mutex_init(&connection_lock, NULL) != 0) {
        printf("mutex init failed");
        return 1;
    }

    pipe(sockfd_msg_interrupt);

    connect_to_server();
    while (strcmp(buffer, "QUIT") != 0) {
        memset(&buffer, 0, sizeof(buffer));
        printf("Enter message: ");
        fgets(buffer, BUFFERSIZE, stdin);

        if (strcmp(buffer, "!QUIT") == 0) {
            break;
        } else if (strcmp(buffer, "!DISS") == 0) {
            sever_connection();
        } else if (strcmp(buffer, "!CONN") == 0) {
            connect_to_server();
        } else {
            send_message(buffer, BUFFERSIZE);
        }
    }

    pthread_cancel(message_thread);
    pthread_mutex_destroy(&socket_lock);
    pthread_mutex_destroy(&connection_lock);    
 
    return 0;
}

void send_message(char* message, int n) {
    pthread_mutex_lock(&connection_lock);
        printf("connected is %d\n", connected);
        if (connected) {
            printf("Sending message: %s\n", message);
            write(sockfd_msg, message, n);
        }
    pthread_mutex_unlock(&connection_lock);
}

int handshake(int sockfd) {
    char recvBuff[BUFFERSIZE];
    int n;

    n = read(sockfd, recvBuff, sizeof(recvBuff)-1);

    recvBuff[n] = 0;
    if(n < 0) {
       error_exit("Read Error");
    }
    //printf("\"%s\" - Recieved\n", recvBuff);

    if (strncmp(recvBuff, "CONFIRM", 7) == 0) {
        //printf("Confirming\n");
        write(sockfd, "CONNECT", 7);
        return 1;
    } else {
        //printf("Not confirmed\n");
        return 0;
    }
}

void* listen_for_new_message() {
    fd_set rfds; //FD to listen for new activity on
    char buffer[BUFFERSIZE];

    int retval = 0, n = 0, maxfd = -1;

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

        //printf("Connection is available - Message\n");
        n = read(sockfd_msg, buffer, BUFFERSIZE);
        //printf("read %d chars: %s\n", n, buffer);

        if (n <= 0) {
            printf("Network error occurued, disconnecting from server...\n");
            sever_connection();
            return NULL;
        }
        printf("Message recieved: \"%s\"", buffer);
    }

    return NULL;
}

void connect_to_server()
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

        err = pthread_create(&message_thread, NULL, &listen_for_new_message, NULL);
        if (err != 0) {
            error_exit("Can't create thread");
        }
        printf("Connected to server, awaiting messages...\n");
    } else {
        //Already connected
    }
    pthread_mutex_unlock(&connection_lock);

}