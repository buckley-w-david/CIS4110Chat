#include "network_functions.h"

//Resolves a hostname to an ipv4 address
char* ipv4_dns_resolve(char* hostname) {
    struct addrinfo hints, *res;
    int status;
    void *addr = NULL;
    char* ipstr = malloc(sizeof(char)*INET6_ADDRSTRLEN);

    //Set the information we'd like to get in 'hinsts'
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) { //Get the address
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    addr = &(ipv4->sin_addr);

    // convert the IP to a string and return it:
    inet_ntop(res->ai_family, addr, ipstr, INET6_ADDRSTRLEN);
    freeaddrinfo(res); // free the linked list
    return ipstr;
}