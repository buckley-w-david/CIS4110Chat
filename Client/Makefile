CC = gcc
CFLAGS = -Wall -std=c11 -g -rdynamic
GTK3FLAGS = `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`
LDFLAGS =

all: client

client: client.o
	$(CC) $(CFLAGS) client.o $(GTK3FLAGS) -o client

client.o: glade_test.c
	$(CC) $(CFLAGS) -c glade_test.c $(GTK3FLAGS) -o client.o

clean:
	rm -rf client *.o

