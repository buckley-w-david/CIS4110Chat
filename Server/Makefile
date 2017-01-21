CC = gcc
CFLAGS = -Wall -std=c11 -g
LDFLAGS =

all: server

server: server.o
	$(CC) $(CFLAGS) server.o -lpthread -o server

server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c -lpthread -o server.o

clean:
	rm -rf server *.o

