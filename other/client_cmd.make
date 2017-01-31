CC = gcc
CFLAGS = -Wall -std=c11 -g
LDFLAGS =

all: client_cmd

client_cmd: client_cmd.o network_functions.o
	$(CC) $(CFLAGS) client_cmd.o network_functions.o -lpthread -o client_cmd

client_cmd.o: client_cmd.c
	$(CC) $(CFLAGS) -c client_cmd.c -lpthread -o client_cmd.o

network_functions.o: network_functions.c network_functions.h
	$(CC) $(CFLAGS) -c network_functions.c -o network_functions.o

clean:
	rm -rf client_cmd *.o

