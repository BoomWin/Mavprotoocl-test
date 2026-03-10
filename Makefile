CC = gcc
CFLAGS = -I./c_library_v2-master -Wall -Wno-address-of-packed-member
LDFLAGS = -lssl -lcrypto

all : server client

server: server.c mavlink_aes.h
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c mavlink_aes.h
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

clean:
	rm -f server client