#ifndef __SERVERH
#define __SERVERH

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/select.h>

#include <pthread.h>
#include <error.h>

typedef struct con {
	uint32_t s_addr;
	int fd;
} Connection;

#endif