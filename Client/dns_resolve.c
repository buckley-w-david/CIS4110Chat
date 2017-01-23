#include "network_functions.h"

char* ipv4_dns_resolve(char* hostname) {
    struct addrinfo hints, *res;
    int status;
    void *addr = NULL;
    char* ipstr = malloc(sizeof(char)*INET6_ADDRSTRLEN);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    addr = &(ipv4->sin_addr);

    // convert the IP to a string and print it:
    inet_ntop(res->ai_family, addr, ipstr, INET6_ADDRSTRLEN);
    freeaddrinfo(res); // free the linked list
    return ipstr;
}