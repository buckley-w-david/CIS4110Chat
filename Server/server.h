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

#define MAX_IN_QUEUE 10
#define CONNECTION_PORT 5000
#define BUFFERSIZE 1025

#endif