CC = gcc
CFLAGS = -Wall -std=c11 -g -rdynamic
GTK3FLAGS = `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`
LDFLAGS =

all: client

client: client.o network_functions.o
	$(CC) $(CFLAGS) client.o network_functions.o $(GTK3FLAGS) -o client

client.o: client.c
	$(CC) $(CFLAGS) -c client.c $(GTK3FLAGS) -o client.o

network_functions.o: network_functions.c network_functions.h
	$(CC) $(CFLAGS) -c network_functions.c $(GTK3FLAGS) -o network_functions.o

clean:
	rm -rf client *.o

