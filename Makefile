CC = gcc
CFLAGS = -I./c_library_v2-master -Wall

all : server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client