CC = gcc
CFLAGS = -Wall -std=c11 -g
LDFLAGS =

all: client

client: client.o network_functions.o
	$(CC) $(CFLAGS) client.o network_functions.o -o client

client.o: client.c client.h network_functions.o
	$(CC) $(CFLAGS) -c client.c -o client.o

network_functions.o: network_functions.c network_functions.h
	$(CC) $(CFLAGS) -c network_functions.c -o network_functions.o

clean:
	rm -rf client *.o

