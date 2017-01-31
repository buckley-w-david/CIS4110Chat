#ifndef __CLIENTH
#define __CLIENTH

#define _POSIX_C_SOURCE 200112L

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>

#include <pthread.h>
#include <error.h>

typedef struct con {
	uint32_t s_addr;
	int fd;
} Connection;


int handshake(int sockfd);

#endif
