#ifndef __NETWORKFUNCTIONSH
#define __NETWORKFUNCTIONSH

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>

char* ipv4_dns_resolve(char* hostname);

#endif